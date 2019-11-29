#include "MusicControllerVolumioSIO.hpp"
#include "MenuController.hpp"

#define GPIO_BUTTON_PREV 203
#define GPIO_BUTTON_NEXT 198
#define GPIO_BUTTON_PLAY 6
#define GPIO_BUTTON_UP   201
#define GPIO_BUTTON_DOWN 67


#include <map>
#include <string>
#include <ctime>


#include "common/perf_log.h"
#include "common/ctrl_socket.h"
#include "common/string_util.h"
#include "common/ctrl_socket.h"
#include "usr_displays.h"

#define REFRESH_SONGINFO_TIME_MS	100
#define REFRESH_IDLE_TIME_MS		250
#define REFRESH_MENU_TIME_MS		250
#define REFRESH_VOLUME_TIME_MS		50

#ifdef ENABLE_GETOPT
#include <getopt.h>
#endif




class DrawAreaIF
{
public:
	DrawAreaIF(DisplayIF &iDisplay, int x, int y, int cx, int cy) : m_iDisp(iDisplay)
	{
		m_nRectX = x;
		m_nRectY = y;
		m_nRectWidth = cx;
		m_nRectHeight = cy;

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

class DrawArea_CSTR : public DrawAreaIF
{
public:
	DrawArea_CSTR(const std::string &tag, uint32_t color, DisplayIF &iDisplay, int x, int y, int cx, int cy, bool isRightAlign = false) : DrawAreaIF(iDisplay, x, y, cx, cy)																								 
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

		auto it2_attr = map.find("m_Index");
		std::string str2_attr = it2_attr != map.end() ? (*it2_attr).second : "-1";
		int index = std::stoi(str2_attr)+1;
		//printf("index = %d\n",index);
		std::string menuindex = "MenuList" + std::to_string(index);
		uint32_t color;
		if(m_strTag == menuindex)
			color=3;
		else color = 2;
		//std::cout << "index = "<<index<<" menuindex="<<menuindex<<" m_str_Tag="<<m_strTag<<std::endl;

		if (1)
		//if (m_nCurrent != str || m_strAttr != str_attr || color != m_nColor)
		{
			int l, t, r, b;
			m_nCurrent = str;
			m_nOffsetX = m_nRectWidth;
			m_strAttr = str_attr;
			m_nColor = color;
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
		//m_iDisp.WriteString(m_nRectX,m_nRectY,2,std::to_string(elapsed));
		m_iDisp.WritePos(m_nRectY,elapsed/duration);

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
		m_iDisp.WriteString(m_nRectX,m_nRectY,3,std::to_string(cpuTemp));
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

/*
			if (m_isVolumeCtrlMode)
			{
				m_eDisplayMode = DISPLAY_MODE_VOLUME;
				int value = 0;
				value = m_iMusicCtl->volumeoffset((m_isButtonNextPressed ? 1 : 0) + (m_isButtonPrevPressed ? -1 : 0));
				iInfo["volume"] = std::to_string(value);
			}

*/
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
                std::cout << "DisplayMode="<<m_eDisplayMode<<std::endl;
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
					auto it2_attr = iMenuInfo.find("m_Index");
					std::string str2_attr = it2_attr != iMenuInfo.end() ? (*it2_attr).second : "0";
					std::cout << "m_index = "<< str2_attr<<std::endl;
					for (auto it : m_iDisplays)
					{
						it->DispClear();
					}
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

		int m = 2;
		big = 36 * cy / 240;
		med = 32 * cy / 240;
		sml = 20 * cy / 240;
		ind = 6 * cy / 240;

		x = m;
		y = m;
		iDrawAreas.push_back(new DrawArea_STR("Title", blue, *it, 2, 2, cx - 2 * x, big));
		y += big + 2;
		iDrawAreas.push_back(new DrawArea_PlayPos(*it, 2, 204, cx - 2 * x, ind));
		y += ind + 2;
		iDrawAreas.push_back(new DrawArea_STR("Artist", white, *it, 2, 42, cx - 2 * x, med));
		y += med + 2;

				
		iDrawAreas.push_back(new DrawArea_STR("Album", white, *it, 2, 62, cx - x, med));
		iDrawAreas.push_back(new DrawArea_STR("trackType", white, *it, 2, 82, cx - x, sml));
		iDrawAreas.push_back(new DrawArea_STR("pcmrate", white, *it, 2, 102, cx - x, sml));
		iDrawAreas.push_back(new DrawArea_CpuTemp(*it, 2, 122, cx - x, sml));

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


		// draw time
		{
			x = (it->GetSize().width - cx) / 2;
			y = (it->GetSize().height - cy) / 2;

			iDrawAreas.push_back(new DrawArea_Time(*it, 2, 2, cx, cy));

			top = y;
			y += cy;
		}

		x = 4;

		// draw date
		cy = font_height * 4 / 10;
		cy = cy < top ? cy : top;
		iDrawAreas.push_back(new DrawArea_Date(*it, 2, 22, it->GetSize().width - x, cy));

		cy = font_height * 4 / 10;
		cy = y + cy < it->GetSize().height ? cy : it->GetSize().height - y;
		cy	= cy * 2 / 4;

		// ip addr
		iDrawAreas.push_back(new DrawArea_MyIpAddr(0xFFFFFFFF, *it, 2, 42, it->GetSize().width - x, cy, true));
		y += cy;

		// cpu temp
		iDrawAreas.push_back(new DrawArea_CpuTemp(*it, 2, 62, it->GetSize().width - x, cy, true));
		y += cy;



		// Volumio Status
		iDrawAreas.push_back(new DrawArea_STR("hostname" , 0xFFFFFFFF, *it, 2, 82, it->GetSize().width / 2, cy));
		x = it->GetSize().width / 2;
		iDrawAreas.push_back(new DrawArea_STR("connected", 0xFFFFFFFF, *it, 2, 102, it->GetSize().width - x, cy, true));
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
			//iDrawAreas.push_back(new DrawArea_PlayPos(*it, x, y, cx - 2 * x, ind));
			//y += ind + 2;
			//iDrawAreas.push_back(new DrawArea_Marker("MarkerTop", *it, cx / 2 - mark_dx / 2, y, mark_dx, mark_dy, 0));
			//y += mark_dy;
			menulinenums = (cy - y - mark_dy - 2) / (med + 1); // TODO Automatic calculation of the number of rows
			for ( int i = 1; i <= menulinenums; i++) {
				std::string menutag = "MenuList" + std::to_string(i);
				iDrawAreas.push_back(new DrawArea_CSTR(menutag, white, *it, x, y, cx - 2 * x, med, false));
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
			//iDrawAreas.push_back(new DrawArea_Marker("MarkerBottom", *it, cx / 2 - mark_dx / 2, y, mark_dx, mark_dy, 1));
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
						std::cout<<"m_isButtonPlayPressed="<<m_isButtonPlayPressed<<" m_isPrevLcdSleep="<<m_isPrevLcdSleep<<" m_isCoverArt=" << m_isCoverArt<<std::endl;

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

	bool m_isButtonUpPressed;
	bool m_isButtonDownPressed;
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
	GpioOut lcden(66);
	lcden << 1;
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
