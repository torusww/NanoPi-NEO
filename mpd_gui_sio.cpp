/******************************************************************************
	Copyright (C) 2017-2018 blue-7 (http://qiita.com/blue-7)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

/*

Required external packages and compile command:

Volumio(Debian):
	apt install libcv-dev libopencv-dev opencv-doc fonts-takao-gothic libtag1-dev libcurl4-openssl-dev libboost1.55-dev

	git clone --recurse-submodules https://github.com/socketio/socket.io-client-cpp
	cd socket.io-client-cpp
	cmake -D CMAKE_CXX_FLAGS=-std=c++11 -DBOOST_INCLUDEDIR=/usr/include/boost/ -DBOOST_LIBRARYDIR=/usr/local/lib/ -DBOOST_VER:STRING=1.55 ./

	cd ~/NanoPi-NEO
	# for NanoHat OLED
	make DISPTYPE=-DDISPLAY_NANOHATOLED TARGET=mpd_gui.cpp.o_nanohatoled
	# for IPS 1.3" 240x240 LCD
	make DISPTYPE=-DDISPLAY_13IPS240240 TARGET=mpd_gui.cpp.o_ips240240
	# for NasPiDAC LCD
	make DISPTYPE="-DDISPLAY_IPS320240 -DFEATURE_INA219 -DGPIO_5BUTTON" TARGET=mpd_gui.cpp.o_naspidac
	# for NasPiDAC LCD without voltage detection
	make DISPTYPE="-DDISPLAY_IPS320240 -DGPIO_5BUTTON" TARGET=mpd_gui.cpp.o_naspidac_novoldet


*/

///////////////////////////////
//  Compile settings
///////////////////////////////
#include "MusicControllerVolumioSIO.hpp"
#include "MenuController.hpp"


#ifndef GPIO_BUTTON_ROTATE
 #ifndef GPIO_BUTTON_PREV
  #define GPIO_BUTTON_PREV 0
 #endif
 #ifndef GPIO_BUTTON_NEXT
  #define GPIO_BUTTON_NEXT 3
 #endif
 #ifndef GPIO_BUTTON_PLAY
  #define GPIO_BUTTON_PLAY 2
 #endif
 #ifdef GPIO_5BUTTON // NasPi DAC LCD Panel compatible
  #ifndef GPIO_BUTTON_UP
   #define GPIO_BUTTON_UP   203
  #endif
  #ifndef GPIO_BUTTON_DOWN
   #define GPIO_BUTTON_DOWN 198
  #endif
 #endif
#else
 #define GPIO_BUTTON_PREV 198
 #define GPIO_BUTTON_NEXT 203
 #define GPIO_BUTTON_PLAY 2
 #ifdef GPIO_5BUTTON // NasPi DAC LCD Panel compatible
  #define GPIO_BUTTON_UP   0
  #define GPIO_BUTTON_DOWN 3
 #endif
#endif


#include <map>
#include <string>
#include <ctime>
#include <opencv2/opencv.hpp>

#include "common/perf_log.h"
#include "common/img_font.h"
#include "common/ctrl_socket.h"
#include "common/string_util.h"
#include "common/ctrl_socket.h"
#include "usr_displays.h"

#define FONT_PATH "/usr/share/fonts/truetype/takao-gothic/TakaoPGothic.ttf"
#define FONT_DATE_PATH "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"

#define REFRESH_SONGINFO_TIME_MS	100
#define REFRESH_IDLE_TIME_MS		250
#define REFRESH_MENU_TIME_MS		250
#define REFRESH_VOLUME_TIME_MS		50

#ifdef ENABLE_GETOPT
#include <getopt.h>
#endif


void	Draw( cv::Mat& dst, int x, int y, cv::Mat& src )
{
	const uint8_t*	src_image	= src.data;
	int				src_stride	= src.step;
	uint8_t*		dst_image	= dst.data;
	int				dst_stride	= dst.step;
	
	if( (src_image != NULL) && 
		(dst_image != NULL) )
	{
		int				cx			= src.cols;
		int				cy			= src.rows;
	
		if( x < 0 )
		{
			src_image   -= x * src.elemSize();
			cx			+= x;
			x			= 0;
		}
	
		if( dst.cols < (x + cx) )
		{
			cx  = dst.cols - x;
		}
	
		if( y < 0 )
		{
			src_image	-= y * src_stride;
			cy			+= y;
			y			= 0;
		}
	
		if( dst.rows < (y + cy) )
		{
			cy  = dst.rows - y;
		}
	
		if( (0 < cx) && (0 < cy) )
		{
			dst_image	+= y * dst_stride + x * dst.elemSize();
			
			for( int r = 0; r < cy; r++ )
			{
				memcpy( dst_image, src_image, cx * src.elemSize() );
				src_image	+= src_stride;
				dst_image	+= dst_stride;
			}
		}
	}
}

class DrawAreaIF
{
public:
	DrawAreaIF(DisplayIF &iDisplay, int x, int y, int cx, int cy) : m_iDisp(iDisplay)
	{
		m_nRectX = x;
		m_nRectY = y;
		m_nRectWidth = cx;
		m_nRectHeight = cy;

		m_iAreaImage = cv::Mat::zeros(cy, cx, CV_8UC1);
	}

	virtual void UpdateInfo(std::map<std::string, std::string> &map) = 0;

	virtual void Reset()
	{
		m_nCurrent = "";
	}

	virtual void setColor(uint32_t color){};
	virtual void setAttr(uint32_t attr){};

protected:
	DisplayIF &m_iDisp;
	int m_nRectX;
	int m_nRectY;
	int m_nRectWidth;
	int m_nRectHeight;
	std::string m_nCurrent;
	cv::Mat m_iAreaImage;
};





class DrawArea_STR : public DrawAreaIF
{
public:
	DrawArea_STR(const std::string &tag, uint32_t color, DisplayIF &iDisplay, int x, int y, int cx, int cy, bool isRightAlign = false) : DrawAreaIF(iDisplay, x, y, cx, cy)																								 
	{
		m_strTag = tag;
		m_nColor = color;
		m_isRightAlign = isRightAlign;
		m_strAttr = "";
	}

	virtual void UpdateInfo(std::map<std::string, std::string> &map)
	{
		auto it = map.find(m_strTag);
		std::string str = it != map.end() ? (*it).second : " ";
		auto it_attr = map.find(m_strTag + "_attr");
		std::string str_attr = it_attr != map.end() ? (*it_attr).second : "";

		if (m_nCurrent != str || m_strAttr != str_attr)
		{
			int l, t, r, b;
			m_nCurrent = str;
			m_nOffsetX = m_nRectWidth;
			m_strAttr = str_attr;
			m_iDisp.WriteString(m_nRectX,m_nRectY,m_nColor,m_nCurrent);
			
		}


	}

protected:
	std::string m_strTag;
	int m_nOffsetX;
	bool m_isRightAlign;
	uint32_t m_nColor;
	std::string m_strAttr;
};

class DrawArea_StaticText : public DrawAreaIF
{
public:
	DrawArea_StaticText(const std::string &text, uint32_t color, DisplayIF &iDisplay, int x, int y, int cx, int cy, bool isRightAlign = false) : DrawAreaIF(iDisplay, x, y, cx, cy)
	{
		m_strText = text;
		m_nColor = color;
		m_isRightAlign = isRightAlign;
	}

	virtual void UpdateInfo(std::map<std::string, std::string> &map)
	{
		std::string str = m_strText;

		if (m_nCurrent != str)
		{
			m_nCurrent = str;
			m_iDisp.WriteString(m_nRectX,m_nRectY,m_nColor,m_nCurrent);

		}

	}

protected:
	std::string m_strText;
	int m_nOffsetX;
	bool m_isRightAlign;
	uint32_t m_nColor;
};

class DrawArea_MyIpAddr : public DrawAreaIF
{
public:
	DrawArea_MyIpAddr(uint32_t color, DisplayIF &iDisplay, int x, int y, int cx, int cy, bool isRightAlign = false) : DrawAreaIF(iDisplay, x, y, cx, cy)
	{
		m_nColor = color;
		m_isRightAlign = isRightAlign;

		m_iLastChecked = std::chrono::high_resolution_clock::now();
		m_strText = Socket::GetMyIpAddrString();
	}

	virtual void UpdateInfo(std::map<std::string, std::string> &map)
	{

		std::chrono::high_resolution_clock::time_point current = std::chrono::high_resolution_clock::now();
		double elapsed = std::chrono::duration_cast<std::chrono::seconds>(current - m_iLastChecked).count();

		if (10 <= elapsed)
		{
			m_iLastChecked = current;
			m_strText = Socket::GetMyIpAddrString();

			if (m_strText == "127.0.0.1")
			{
				m_nColor = 1;
			}
			else
			{
				m_nColor = 2;
			}
		}

		if (m_nCurrent != m_strText)
		{
			m_nCurrent = m_strText;
			m_iDisp.WriteString(m_nRectX,m_nRectY,m_nColor,m_nCurrent);
		}

	}

protected:
	std::chrono::high_resolution_clock::time_point m_iLastChecked;
	std::string m_strText;
	int m_nOffsetX;
	bool m_isRightAlign;
	uint32_t m_nColor;
};

class DrawArea_PlayPos : public DrawAreaIF
{
public:
	DrawArea_PlayPos(DisplayIF &iDisplay, int x, int y, int cx, int cy) : DrawAreaIF(iDisplay, x, y, cx, cy){};

	virtual void UpdateInfo(std::map<std::string, std::string> &map)
	{
		auto itT = map.find("Time");
		auto itE = map.find("elapsed");

		float duration = itT != map.end() ? std::stof((*itT).second) : 1;
		float elapsed = itE != map.end() ? std::stof((*itE).second) : 0;
		m_iDisp.WriteString(m_nRectX,m_nRectY,2,(int)duration);

	}
};
/*
class DrawArea_Marker : public DrawAreaIF
{
private:
public:
	DrawArea_Marker(const std::string &tag, DisplayIF &iDisplay, int x, int y, int cx, int cy, int direction = 0) : DrawAreaIF(iDisplay, x, y, cx, cy)
	{
		m_strTag = tag;
		m_nDirection = direction;
	}

	virtual void UpdateInfo(std::map<std::string, std::string> &map)
	{
		auto it = map.find(m_strTag);
		int dy = m_nRectHeight - 1;
		int dx = m_nRectWidth - 1;
		m_iAreaImage = cv::Mat::zeros(m_nRectHeight, m_nRectWidth, CV_8UC4);

		if (it != map.end() && !(*it).second.empty())
		{
			cv::Point pt[3];
			if (m_nDirection == 0)
			{
				pt[0] = cv::Point2i(0, dy);
				pt[1] = cv::Point2i(dx, dy);
				pt[2] = cv::Point2i(dx / 2, 0);
			}
			else
			{
				pt[0] = cv::Point2i(0, 0);
				pt[1] = cv::Point2i(dx, 0);
				pt[2] = cv::Point2i(dx / 2, dy);
			}
			cv::fillConvexPoly(m_iAreaImage, pt, 3, cv::Scalar(255, 255, 255, 255));
		}
		m_iDisp.WriteImageBGRA(m_nRectX, m_nRectY, m_iAreaImage.data, m_iAreaImage.step, m_iAreaImage.cols, m_iAreaImage.rows);
	}

protected:
	std::string m_strTag;
	int m_nDirection;
};
*/


class DrawArea_CpuTemp : public DrawAreaIF
{
public:
	DrawArea_CpuTemp(DisplayIF &iDisplay, int x, int y, int cx, int cy, bool isRightAlign = false) : DrawAreaIF(iDisplay, x, y, cx, cy)
	{
		m_isRightAlign = isRightAlign;
	};

	virtual void UpdateInfo(std::map<std::string, std::string> &map)
	{
		int x = 0;
		char buf[128];
		float cpuTemp = std::stof(StringUtil::GetTextFromFile("/sys/class/thermal/thermal_zone0/temp")) / 1000.0f;
		m_iDisp.WriteString(m_nRectX,m_nRectY,3,"hello");
	}

protected:
	bool m_isRightAlign;
};

class DrawArea_Date : public DrawAreaIF
{
public:
	DrawArea_Date(DisplayIF &iDisplay, int x, int y, int cx, int cy) : DrawAreaIF(iDisplay, x, y, cx, cy)
	{
		int l, t, r, b;
	
		m_nOffsetX = 0; //(cx - (r-l) + 1) / 2 - l;
		m_nOffsetY = (cy - (b - t) + 1) / 2 - t;
	};

	virtual void UpdateInfo(std::map<std::string, std::string> &map)
	{
		char buf[32];
		std::time_t t = std::time(nullptr);
		std::tm *tl = std::localtime(&t);

		sprintf(buf, "%04d/%02d/%02d", 1900 + tl->tm_year, 1 + tl->tm_mon, tl->tm_mday);

		if (m_nCurrent != buf)
		{
			m_nCurrent = buf;
			m_iDisp.WriteString(m_nRectX,m_nRectY,3,m_nCurrent);

		}
	}

protected:
	
	int m_nOffsetX;
	int m_nOffsetY;
};

class DrawArea_Time : public DrawAreaIF
{
public:
	DrawArea_Time(DisplayIF &iDisplay, int x, int y, int cx, int cy) : DrawAreaIF(iDisplay, x, y, cx, cy)
	{
		int l, t, r, b;
		m_nOffsetX = (cx - (r - l)) / 2 - l;
		m_nOffsetY = (cy - (b - t)) / 2 - t;
	};

	virtual void UpdateInfo(std::map<std::string, std::string> &map)
	{
		char buf[32];
		std::time_t t = std::time(nullptr);
		std::tm *tl = std::localtime(&t);

		if (tl->tm_sec & 1)
			sprintf(buf, "%02d %02d", tl->tm_hour, tl->tm_min);
		else
			sprintf(buf, "%02d:%02d", tl->tm_hour, tl->tm_min);

		if (m_nCurrent != buf)
		{
			m_nCurrent = buf;
 			m_iDisp.WriteString(m_nRectX,m_nRectY,3,m_nCurrent);
		}
	}

protected:
	int m_nOffsetX;
	int m_nOffsetY;
};



class MpdGui
{
public:
	MpdGui()
	{
		m_iDisplays = GetUsrDisplays();
		m_eDisplayMode = DISPLAY_MODE_NONE;

		m_isVolumeCtrlMode = false;
		m_isMenuMode = false;
		m_isCoverArt = false;
		m_isButtonNextPressed = false;
		m_isButtonPrevPressed = false;
		m_isButtonPlayPressed = false;
	}

	bool Initialize()
	{
		for (auto it : m_iDisplays)
		{
			if (0 != it->Init())
			{
				return false;
			}

			it->DispClear();
			it->DispOn();

			SetupLayout_SongInfo(m_iDrawAreasSongInfo, it);
			SetupLayout_Idle(m_iDrawAreasIdle, it);
			SetupLayout_Volume(m_iDrawAreasVolume, it);
			m_iMenuMaxlists = SetupLayout_Menu(m_iDrawAreasMenu, it);
			SetupLayout_SongInfoCA(m_iDrawAreasSongInfoCA, it);
		}

		// Setup Gpio
		SetupButtons();

		// Setup Music&Menu Resources
		m_iMusicCtl = new MusicControllerVolumioSIO();
		m_iMenuCtl  = new MenuController(*m_iMusicCtl, m_iMenuMaxlists);

		// Setup Callback
		m_iMenuCtl->on("coverart", [&](std::string &value)
		{
			m_isCoverArt = true;
		});
		m_iLcdSleepTime = 0;
		m_iMenuCtl->on("lcdsleeptime", [&](std::string &value)
		{
			m_iLcdSleepTime = std::stoi( value );
			std::cout << "Lcd Sleep Time= " << m_iLcdSleepTime << " Sec" << std::endl;
		});
		m_iMenuCtl->emit("lcdsleeptime_update", ""); // Update lcd sleep time request

		return true;
	}

	void Loop()
	{
		m_iGpioIntCtrl.ThreadStart();

		////////////////////////////////////
		// Connect to MPD
		////////////////////////////////////
		DISPLAY_MODE ePrevDisplayMode;

		ePrevDisplayMode = DISPLAY_MODE_NONE;
		m_eDisplayMode = DISPLAY_MODE_NONE;
		m_iMusicCtl->connect();

		// for LCD Sleep Function
		m_isLcdSleep = false;
		m_isPrevLcdSleep = false;
		m_iPrevKeyPressTime = std::chrono::system_clock::now();

		while (1)
		{
			// LCD Sleep Check
			{
				double elapsed = std::chrono::duration_cast<std::chrono::seconds>(
										 std::chrono::system_clock::now() -
										 m_iPrevKeyPressTime)
										 .count();
				
				if ( !m_isLcdSleep && (m_iLcdSleepTime > 0) && (elapsed > m_iLcdSleepTime) ) {
					// Sleep Detect
					m_isLcdSleep = true;
					m_isPrevLcdSleep = true;
					for (auto it : m_iDisplays)
					{
						it->DispOff();
					}
				}else if (m_isLcdSleep == false && m_isPrevLcdSleep == true) {
					// Resume Detect
					m_isPrevLcdSleep = false;
					for (auto it : m_iDisplays)
					{
						it->DispClear();
						it->DispOn();
					}
					ePrevDisplayMode = DISPLAY_MODE_NONE;
				}

				if (m_isLcdSleep) {
					// Sleep Loop
					std::this_thread::sleep_for(std::chrono::milliseconds(REFRESH_IDLE_TIME_MS));
					continue;
				}
			}

			///////////////////////////////////////////
			// Receive current playback info.
			///////////////////////////////////////////
			if (m_iMusicCtl->isUpdateState())
			{
				m_iMusicCtl->getState(iInfo);
//				std::cout << "updateInfo" << std::endl;
			}

			if (iInfo["state"] == "play")
			{
				m_eDisplayMode = DISPLAY_MODE_SONGINFO;
			}
			else
			{
				m_eDisplayMode = DISPLAY_MODE_IDLE;
			}

			if (!m_isVolumeCtrlMode)
			{
				if (m_isButtonNextPressed)
				{
					double elapsed = std::chrono::duration_cast<std::chrono::seconds>(
															 std::chrono::system_clock::now() -
															 m_iButtonNextPressedTime)
															 .count();

					if (0.3 <= elapsed)
					{
						m_isVolumeCtrlMode = true;
					}
				}

				if (m_isButtonPrevPressed)
				{
					double elapsed = std::chrono::duration_cast<std::chrono::seconds>(
															 std::chrono::system_clock::now() -
															 m_iButtonPrevPressedTime)
															 .count();

					if (0.3 <= elapsed)
					{
						m_isVolumeCtrlMode = true;
					}
				}
			}

			if (m_isCoverArt)
			{
				m_eDisplayMode = DISPLAY_MODE_COVERART;
			}

			if (m_isVolumeCtrlMode)
			{
				m_eDisplayMode = DISPLAY_MODE_VOLUME;
				int value = 0;
				value = m_iMusicCtl->volumeoffset((m_isButtonNextPressed ? 1 : 0) + (m_isButtonPrevPressed ? -1 : 0));
				iInfo["volume"] = std::to_string(value);
			}

			if (!m_iMenuCtl->isMenuMode())
			{
				if (m_isButtonPlayPressed)
				{
					double elapsed = std::chrono::duration_cast<std::chrono::seconds>(
															 std::chrono::system_clock::now() -
															 m_iButtonPlayPressedTime)
															 .count();

					if (1.0 <= elapsed)
					{ // Press and hold the play button to switch to the menu screen
						m_isButtonPlayPressed = false;
						m_iMenuCtl->open();
					}
				}
			}
			if (!m_isVolumeCtrlMode && m_iMenuCtl->isMenuMode())
			{
				if (m_isButtonPlayPressed) // Play button long pressed in menu mode
				{
					double elapsed = std::chrono::duration_cast<std::chrono::seconds>(
															 std::chrono::system_clock::now() -
															 m_iButtonPlayPressedTime)
															 .count();

					if (1.0 <= elapsed)
					{ // Press and hold the play button to switch to the idle/songinfo screen
						m_isButtonPlayPressed = false;
						m_iMenuCtl->close();
						continue;
					}
				}
				m_eDisplayMode = DISPLAY_MODE_MENU;
			}

			if (m_eDisplayMode != ePrevDisplayMode)
			{
				for (auto it : m_iDisplays)
				{
					it->DispClear();
				}

				for (auto it : m_iDrawAreasSongInfo)
				{
					it->Reset();
				}

				for (auto it : m_iDrawAreasIdle)
				{
					it->Reset();
				}

				for (auto it : m_iDrawAreasVolume)
				{
					it->Reset();
				}

				for (auto it : m_iDrawAreasMenu)
				{
					it->Reset();
				}

				for (auto it : m_iDrawAreasSongInfoCA)
				{
					it->Reset();
				}
				ePrevDisplayMode = m_eDisplayMode;
			}

			switch (m_eDisplayMode)
			{
			case DISPLAY_MODE_SONGINFO:
				for (auto it : m_iDrawAreasSongInfo)
				{
					it->UpdateInfo(iInfo);
				}

				for (auto it : m_iDisplays)
				{
					it->Flush();
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(REFRESH_SONGINFO_TIME_MS));
				break;

			case DISPLAY_MODE_IDLE:
				for (auto it : m_iDrawAreasIdle)
				{
					it->UpdateInfo(iInfo);
				}

				for (auto it : m_iDisplays)
				{
					it->Flush();
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(REFRESH_IDLE_TIME_MS));
				break;

			case DISPLAY_MODE_VOLUME:
				for (auto it : m_iDrawAreasVolume)
				{
					it->UpdateInfo(iInfo);
				}

				for (auto it : m_iDisplays)
				{
					it->Flush();
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(REFRESH_VOLUME_TIME_MS));
				break;

			case DISPLAY_MODE_MENU:
				if (m_iMenuCtl->isUpdateMenu()) {
					m_iMenuCtl->getMenulists(iMenuInfo);
				}
				for (auto it : m_iDrawAreasMenu)
				{
					it->UpdateInfo(iMenuInfo);
				}

				for (auto it : m_iDisplays)
				{
					it->Flush();
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(REFRESH_MENU_TIME_MS));
				break;

			case DISPLAY_MODE_COVERART:
				for (auto it : m_iDrawAreasSongInfoCA)
				{
					it->UpdateInfo(iInfo);
				}

				for (auto it : m_iDisplays)
				{
					it->Flush();
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(REFRESH_IDLE_TIME_MS));
				break;
			}
		}

		m_iGpioIntCtrl.ThreadStop();
		return;
	}

	bool getImageData(std::vector<unsigned char>& data, bool force)
	{
		if (force || m_iMusicCtl->isUpdateCoverart()){
			if (m_iMusicCtl->getCoverart(data) == 0) {
				return true;
			}
		}
		return force;
	}

protected:
	void SetupLayout_SongInfo(std::vector<DrawAreaIF *> &iDrawAreas, DisplayIF *it)
	{
		int x, y, csz, big, med, sml, ind;
		int cx = it->GetSize().width;
		int cy = it->GetSize().height;

		uint32_t white = 0xFFFFFFFF;
		uint32_t blue = 0xFF0095D8;

		if (cy < cx)
		{
			if (64 < cy)
			{
				int m = 2;
				big = 36 * cy / 240;
				med = 32 * cy / 240;
				sml = 20 * cy / 240;
				ind = 6 * cy / 240;

				x = m;
				y = m;
				iDrawAreas.push_back(new DrawArea_STR("Title", blue, *it, x, y, cx - 2 * x, big));
				y += big + 2;
				iDrawAreas.push_back(new DrawArea_PlayPos(*it, x, y, cx - 2 * x, ind));
				y += ind + 2;
				iDrawAreas.push_back(new DrawArea_STR("Artist", white, *it, x, y, cx - 2 * x, med));
				y += med + 2;

				
				iDrawAreas.push_back(new DrawArea_STR("Album", white, *it, x, y + m, cx - x, med));
				iDrawAreas.push_back(new DrawArea_STR("trackType", white, *it, x, cy - sml * 3 - med * 0 - m * 0, cx - x, sml));
				iDrawAreas.push_back(new DrawArea_STR("pcmrate", white, *it, x, cy - sml * 2 - m * 0, cx - x, sml));
				iDrawAreas.push_back(new DrawArea_CpuTemp(*it, x, cy - sml * 1- m * 0, cx - x, sml));

			}
			else if (32 < cy)
			{
				big = 18 * cy / 64;
				sml = 12 * cy / 64;
				med = (cy - big - sml - 2) / 2;

				x = 0;
				y = 0;
				iDrawAreas.push_back(new DrawArea_STR("Title", white, *it, x, y, cx, big));
				y += big;
				iDrawAreas.push_back(new DrawArea_PlayPos(*it, x, y, cx, 1));
				y += 1 + 1;

				csz = it->GetSize().height - y;
				

				iDrawAreas.push_back(new DrawArea_STR("Artist", white, *it, x, y, cx - x, med));
				y += med;
				iDrawAreas.push_back(new DrawArea_STR("Album", white, *it, x, y, cx - x, med));
				y += med;
				iDrawAreas.push_back(new DrawArea_STR("audio", white, *it, x, cy - sml, cx - x, sml));
			}
			else
			{
				big = 16 * cy / 32;
				med = cy - big - 2;

				x = 0;
				y = 0;
				iDrawAreas.push_back(new DrawArea_STR("Title", white, *it, x, y, cx, big));
				y += big;
				iDrawAreas.push_back(new DrawArea_PlayPos(*it, x, y, cx, 1));
				y += 1 + 1;
				iDrawAreas.push_back(new DrawArea_STR("Artist", white, *it, x, y, cx, med));
				y += med;
			}
		}
		else if (cx == cy)
		{
			int m = 2;
			big = 36 * cy / 240; // 48
			med = 32 * cy / 240; // 36
			sml = 20 * cy / 240;
			ind = 6 * cy / 240;

			x = m;
			y = m;
			iDrawAreas.push_back(new DrawArea_STR("Title", blue, *it, x, y, cx - 2 * x, big, false));
			y += big + 2;
			iDrawAreas.push_back(new DrawArea_PlayPos(*it, x, y, cx - 2 * x, ind));
			y += ind + 2;
			iDrawAreas.push_back(new DrawArea_STR("Album", white, *it, x, y, cx - 2 * x, med, false));
			y += med + 2;
			iDrawAreas.push_back(new DrawArea_STR("Artist", white, *it, x, y, cx - 2 * x, med, false));
			y += med + 2;

			

			iDrawAreas.push_back(new DrawArea_STR("trackType", white, *it, x, cy - sml * 3 - med - m * 4, cx - x, med));
			iDrawAreas.push_back(new DrawArea_STR("samplerate", white, *it, x, cy - sml * 3 - m * 3, cx - x, sml));
			iDrawAreas.push_back(new DrawArea_STR("bitdepth", white, *it, x, cy - sml * 2 - m * 2, cx - x, sml));
			iDrawAreas.push_back(new DrawArea_CpuTemp(*it, x, cy - sml - m, cx - x, sml));

		}
		else
		{
#if 1
			int m = 2;
			big = 30 * cy / 320;
			med = 22 * cy / 320;
			sml = 16 * cy / 320;
			ind = 4 * cy / 320;

			x = m;
			y = 0;
			iDrawAreas.push_back(new DrawArea_STR("Title", blue, *it, x, y, cx - 2 * x, big, false));
			y += big + 2;
			iDrawAreas.push_back(new DrawArea_PlayPos(*it, x, y, cx - 2 * x, ind));
			y += ind + 2;
			iDrawAreas.push_back(new DrawArea_STR("Artist", white, *it, x, y, cx - 2 * x, med, true));
			y += med + 2;
			iDrawAreas.push_back(new DrawArea_STR("Album", white, *it, x, y, cx - 2 * x, sml, true));
			y += sml + 2;

//			csz = cy - y - 4 - med - 2 - sml;
						
//			y += csz + 4;
//			iDrawAreas.push_back(new DrawArea_STR("Album", white, *it, x, y, cx - 2 * x, med, false));
//			y += med + 2;
//			iDrawAreas.push_back(new DrawArea_STR("audio", white, *it, x, cy - sml, cx - 2 * x, sml, true));
#else
			int m = 2;
			big = 36 * cy / 320;
			med = 28 * cy / 320;
			sml = 24 * cy / 320;
			ind = 4 * cy / 320;

			x = m;
			y = m;
			iDrawAreas.push_back(new DrawArea_STR("Title", blue, *it, x, y, cx - 2 * x, big, false));
			y += big + 2;
			iDrawAreas.push_back(new DrawArea_PlayPos(*it, x, y, cx - 2 * x, ind));
			y += ind + 2;
			iDrawAreas.push_back(new DrawArea_STR("Artist", white, *it, x, y, cx - 2 * x, med, true));
			y += med + 2;

	
			iDrawAreas.push_back(new DrawArea_STR("Album", white, *it, x, y, cx - 2 * x, med, false));
			y += med + 2;
			iDrawAreas.push_back(new DrawArea_STR("audio", white, *it, x, cy - sml, cx - 2 * x, sml, true));
#endif
		}
	}

	void SetupLayout_Volume(std::vector<DrawAreaIF *> &iDrawAreas, DisplayIF *it)
	{
		int m = 4;
		int cx = it->GetSize().width;
		int cy = it->GetSize().height;

		uint32_t white = 0xFFFFFFFF;
		int oy = 0;

		if (cx <= cy)
		{
			int h = cx * 2 / 3;
			oy = (cy - h) / 2;
			cy = h;
		}

#ifdef VOLUME_CTRL_I2C_AK449x
		iDrawAreas.push_back(new DrawArea_StaticText("volume", white, *it, m, oy, cx - 2 * m, cy * 6 / 16));
		iDrawAreas.push_back(new DrawArea_STR("volume", white, *it, 0, oy + cy * 6 / 16, cx, cy * 6 / 16, true));
#else
		iDrawAreas.push_back(new DrawArea_StaticText("volume", white, *it, m, oy, cx - 2 * m, cy * 6 / 16));
		iDrawAreas.push_back(new DrawArea_STR("volume", white, *it, 0, oy + cy * 6 / 16, cx - 2 * m, cy * 10 / 16, true));
#endif
	}

	void SetupLayout_Idle(std::vector<DrawAreaIF *> &iDrawAreas, DisplayIF *it)
	{
		int font_height = it->GetSize().height;
		char buf[256];
		int cx, cy;
		int x, y, top;

		{
			ImageFont iFont(FONT_DATE_PATH, font_height, false);
			int l, t, r, b;

			iFont.CalcRect(l, t, r, b, "88:88");
			cx = r - l;
			cy = b - t;

			if (it->GetSize().width < cx)
			{
				cy = it->GetSize().height * it->GetSize().width / cx;
				cx = it->GetSize().width;
				cy = cy * 7 / 8;
				font_height = cy;
			}

			//		printf("%d x %d, %d\n", cx, cy, font_height );
		}

		// draw time
		{
			x = (it->GetSize().width - cx) / 2;
			y = (it->GetSize().height - cy) / 2;

			iDrawAreas.push_back(new DrawArea_Time(*it, x, y, cx, cy));

			top = y;
			y += cy;
		}

		x = 4;

		// draw date
		cy = font_height * 4 / 10;
		cy = cy < top ? cy : top;
		iDrawAreas.push_back(new DrawArea_Date(*it, x, top - cy, it->GetSize().width - x, cy));

		cy = font_height * 4 / 10;
		cy = y + cy < it->GetSize().height ? cy : it->GetSize().height - y;
		cy	= cy * 2 / 4;

		// ip addr
		iDrawAreas.push_back(new DrawArea_MyIpAddr(0xFFFFFFFF, *it, 0, y, it->GetSize().width - x, cy, true));
		y += cy;

		// cpu temp
		iDrawAreas.push_back(new DrawArea_CpuTemp(*it, 0, y, it->GetSize().width - x, cy, true));
		y += cy;



		// Volumio Status
		iDrawAreas.push_back(new DrawArea_STR("hostname" , 0xFFFFFFFF, *it, 0, it->GetSize().height - 16, it->GetSize().width / 2, cy));
		x = it->GetSize().width / 2;
		iDrawAreas.push_back(new DrawArea_STR("connected", 0xFFFFFFFF, *it, x, it->GetSize().height - 16, it->GetSize().width - x, cy, true));
	}

	int SetupLayout_Menu(std::vector<DrawAreaIF *> &iDrawAreas, DisplayIF *it)
	{
		int x, y, csz, big, med, sml, ind;
		int cx = it->GetSize().width;
		int cy = it->GetSize().height;
		int menulinenums = 0;

		uint32_t white = 0xaaaaaaaa;
		//		uint32_t white = 0xFFFFFFFF;
		uint32_t blue = 0xFF0095D8;

		if (cy >= 240)
		{
			int m = 2;
			if (cy < 320) {
				big = 36 * cy / 240;
				med = 24 * cy / 240;
				ind = 6 * cy / 240;
			}else{
				big = 36 * cy / 320;
				med = 24 * cy / 320;
				ind = 6 * cy / 320;
			}

			int mark_dx = 13;
			int mark_dy = 6;

			x = m;
			y = m;
			iDrawAreas.push_back(new DrawArea_STR("MenuTitle", blue, *it, x, y, cx - 2 * x, big, false));
			y += big + 2;
			iDrawAreas.push_back(new DrawArea_PlayPos(*it, x, y, cx - 2 * x, ind));
			y += ind + 2;
			//iDrawAreas.push_back(new DrawArea_Marker("MarkerTop", *it, cx / 2 - mark_dx / 2, y, mark_dx, mark_dy, 0));
			//y += mark_dy;
			menulinenums = (cy - y - mark_dy - 2) / (med + 1); // TODO Automatic calculation of the number of rows
			for ( int i = 1; i <= menulinenums; i++) {
				std::string menutag = "MenuList" + std::to_string(i);
				iDrawAreas.push_back(new DrawArea_STR(menutag, white, *it, x, y, cx - 2 * x, med, false));
				y += med + 1;
			}
			#if 0
			iDrawAreas.push_back(new DrawArea_STR("MenuList1", white, *it, x, y, cx - 2 * x, med, false));
			y += med + 1;
			iDrawAreas.push_back(new DrawArea_STR("MenuList2", white, *it, x, y, cx - 2 * x, med, false));
			y += med + 1;
			iDrawAreas.push_back(new DrawArea_STR("MenuList3", white, *it, x, y, cx - 2 * x, med, false));
			y += med + 1;
			iDrawAreas.push_back(new DrawArea_STR("MenuList4", white, *it, x, y, cx - 2 * x, med, false));
			y += med + 1;
			iDrawAreas.push_back(new DrawArea_STR("MenuList5", white, *it, x, y, cx - 2 * x, med, false));
			y += med + 1;
			iDrawAreas.push_back(new DrawArea_STR("MenuList6", white, *it, x, y, cx - 2 * x, med, false));
			y += med + 1;
			iDrawAreas.push_back(new DrawArea_STR("MenuList7", white, *it, x, y, cx - 2 * x, med, false));
			y += med + 1;
			#endif
			iDrawAreas.push_back(new DrawArea_Marker("MarkerBottom", *it, cx / 2 - mark_dx / 2, y, mark_dx, mark_dy, 1));
		}
		else
		{
			big = 16 * cy / 64;
			med = 12 * cy / 64;

			int mark_dx = 7;
			int mark_dy = 3;

			x = 0;
			y = 0;
			iDrawAreas.push_back(new DrawArea_STR("MenuTitle", blue, *it, x, y, cx - 2 * x, big, false));
			y += big;
			iDrawAreas.push_back(new DrawArea_PlayPos(*it, x, y, cx - 2 * x, 1));
			y += 3;
			//iDrawAreas.push_back(new DrawArea_Marker("MarkerTop", *it, cx / 2 - mark_dx / 2, y, mark_dx, mark_dy, 0));
			//y += mark_dy;
			iDrawAreas.push_back(new DrawArea_STR("MenuList1", white, *it, x, y, cx - 2 * x, med, false));
			y += med;
			iDrawAreas.push_back(new DrawArea_STR("MenuList2", white, *it, x, y, cx - 2 * x, med, false));
			y += med;
			iDrawAreas.push_back(new DrawArea_STR("MenuList3", white, *it, x, y, cx - 2 * x, med, false));
			y += med + 2;
			//iDrawAreas.push_back(new DrawArea_Marker("MarkerBottom", *it, cx / 2 - mark_dx / 2, y, mark_dx, mark_dy, 1));
			menulinenums = 3; // TODO Automatic calculation of the number of rows
		}

		return menulinenums;
	}

	void SetupLayout_SongInfoCA(std::vector<DrawAreaIF *> &iDrawAreas, DisplayIF *it)
	{
		int x, y, csz, big, med, sml, ind;
		int cx = it->GetSize().width;
		int cy = it->GetSize().height;

		
	}

	void SetupButtons()
	{
		m_iGpioIntCtrl.RegistPin(
				GPIO_BUTTON_PREV,
				[&](int value) {
					m_iPrevKeyPressTime = std::chrono::system_clock::now();
					m_isLcdSleep = false;

					if (0 == value)
					{
						printf("OnButtonPrev_Up()\n");

						if (m_iMenuCtl->isMenuMode())
						{
							m_iMenuCtl->prev();
						}
						else if (m_isVolumeCtrlMode == 0)
						{
							m_iPrevGetStatusTime = std::chrono::system_clock::now();
							m_iMusicCtl->prev();
						}

						m_isButtonPrevPressed = false;
						m_isVolumeCtrlMode = false;
						printf("OnButtonPrev_Up() Leave\n");
					}
					else
					{
						printf("OnButtonPrev_Down()\n");
						m_isVolumeCtrlMode = false;
						m_iButtonPrevPressedTime = std::chrono::system_clock::now();
						m_isButtonPrevPressed = true;
						printf("OnButtonPrev_Down() Leave\n");
					}
				});

		m_iGpioIntCtrl.RegistPin(
				GPIO_BUTTON_NEXT,
				[&](int value) {
					m_iPrevKeyPressTime = std::chrono::system_clock::now();
					m_isLcdSleep = false;
					if (0 == value)
					{
						printf("OnButtonNext_Up()\n");

						if (m_iMenuCtl->isMenuMode())
						{
							m_iMenuCtl->next();
						}
						else if (!m_isVolumeCtrlMode)
						{
							m_iPrevGetStatusTime = std::chrono::system_clock::now();
							m_iMusicCtl->next();
						}

						m_isButtonNextPressed = false;
						m_isVolumeCtrlMode = false;

						printf("OnButtonNext_Up() LEAVE\n");
					}
					else
					{
						printf("OnButtonNext_Down()\n");
						m_isVolumeCtrlMode = false;
						m_iButtonNextPressedTime = std::chrono::system_clock::now();
						m_isButtonNextPressed = true;
						printf("OnButtonNext_Down() LEAVE\n");
					}
				});

		m_iGpioIntCtrl.RegistPin(
				GPIO_BUTTON_PLAY,
				[&](int value) {
					m_iPrevKeyPressTime = std::chrono::system_clock::now();
					m_isLcdSleep = false;
					if (0 == value)
					{
						printf("OnButtonPlay_Up()\n");
						if (m_isButtonPlayPressed)
						{
							m_isButtonPlayPressed = false;
							if (m_isPrevLcdSleep == true) {
								return;
							}
							if (m_isCoverArt)
							{
								m_isCoverArt = false;
							}
							else if (m_iMenuCtl->isMenuMode())
							{
								m_iMenuCtl->exec();
							}
							else
							{
								m_iMusicCtl->toggle();
							}
						}
					}
					else
					{
						printf("OnButtonPlay_Down()\n");
						m_iButtonPlayPressedTime = std::chrono::system_clock::now();
						m_isButtonPlayPressed = true;
					}
				});
#ifdef GPIO_5BUTTON
		m_iGpioIntCtrl.RegistPin(
				GPIO_BUTTON_UP,
				[&](int value) {
					m_iPrevKeyPressTime = std::chrono::system_clock::now();
					m_isLcdSleep = false;
					if (0 == value)
					{
						printf("OnButtonUp_Up()\n");
						if (m_isButtonUpPressed)
						{
							m_isButtonUpPressed = false;
							if (!m_iMenuCtl->isMenuMode())
							{
								m_isVolumeCtrlMode = false;
							}
						}
					}
					else
					{
						printf("OnButtonUp_Down()\n");
						m_isButtonUpPressed = true;
						if (m_iMenuCtl->isMenuMode())
						{
							m_iMenuCtl->prev();
						}else{
							m_iMusicCtl->volumeoffset(1);
							m_isVolumeCtrlMode = true;
						}
					}
				});
		m_iGpioIntCtrl.RegistPin(
				GPIO_BUTTON_DOWN,
				[&](int value) {
					m_iPrevKeyPressTime = std::chrono::system_clock::now();
					m_isLcdSleep = false;
					if (0 == value)
					{
						printf("OnButtonDown_Up()\n");
						if (m_isButtonDownPressed)
						{
							m_isButtonDownPressed = false;
							if (!m_iMenuCtl->isMenuMode())
							{
								m_isVolumeCtrlMode = false;
							}
						}
					}
					else
					{
						printf("OnButtonDown_Down()\n");
						m_isButtonDownPressed = true;
						if (m_iMenuCtl->isMenuMode())
						{
							m_iMenuCtl->next();
						}else{
							m_iMusicCtl->volumeoffset(-1);
							m_isVolumeCtrlMode = true;
						}
					}
				});
#endif
	}

	enum DISPLAY_MODE
	{
		DISPLAY_MODE_NONE = 0,
		DISPLAY_MODE_IDLE,
		DISPLAY_MODE_SONGINFO,
		DISPLAY_MODE_VOLUME,
		DISPLAY_MODE_MENU,
		DISPLAY_MODE_COVERART,
	};

protected:
	DISPLAY_MODE m_eDisplayMode;
	DISPLAY_MODE m_eDisplayMode_prev;

	std::vector<DisplayIF *> m_iDisplays;
	std::vector<DrawAreaIF *> m_iDrawAreasSongInfo;
	std::vector<DrawAreaIF *> m_iDrawAreasIdle;
	std::vector<DrawAreaIF *> m_iDrawAreasVolume;
	std::vector<DrawAreaIF *> m_iDrawAreasMenu;
	std::vector<DrawAreaIF *> m_iDrawAreasSongInfoCA;
	GpioInterruptCtrl m_iGpioIntCtrl;

	bool m_isVolumeCtrlMode;
	bool m_isMenuMode;
	bool m_isCoverArt;
	bool m_isButtonNextPressed;
	bool m_isButtonPrevPressed;
	volatile bool m_isButtonPlayPressed;
	std::chrono::system_clock::time_point m_iButtonNextPressedTime;
	std::chrono::system_clock::time_point m_iButtonPrevPressedTime;
	std::chrono::system_clock::time_point m_iButtonPlayPressedTime;

#ifdef GPIO_5BUTTON
	bool m_isButtonUpPressed;
	bool m_isButtonDownPressed;
#endif
	// LCD Sleep
	std::chrono::system_clock::time_point m_iPrevKeyPressTime;
	bool m_isLcdSleep;
	bool m_isPrevLcdSleep;
	int  m_iLcdSleepTime;

	// Info
	std::map<std::string, std::string> iInfo;
	std::map<std::string, std::string> iMenuInfo;
	std::chrono::system_clock::time_point m_iPrevGetStatusTime;

	// controller
	MusicController *m_iMusicCtl;
	int m_iMenuMaxlists;
	MenuController *m_iMenuCtl;
};

#ifdef ENABLE_GETOPT
void printusage(void)
{
	std::cout << "-s <volumio server>" << std::endl;
}
#endif

int main(int argc, char **argv)

{
	MpdGui iMPD;

#ifdef ENABLE_GETOPT
	const char *const shortopts = "s:";

	while (1)
	{
		const auto opt = getopt(argc, argv, shortopts);
		if (-1 == opt)
			break;

		switch (opt)
		{
		case 's':
//			m_volumioHost = std::string(optarg);
			std::cout << "Host name override: " << std::endl;
			break;

		default:
			printusage();
			break;
		}
	}
#endif

	if (!iMPD.Initialize())
	{
		return -1;
	}

	iMPD.Loop();
	return 0;
}
