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

Ubuntu + mpd:

	apt install libcv-dev libopencv-dev opencv-doc fonts-takao-pgothic libtag1-dev

	g++ -O3 -pthread -std=c++11 mpd_gui.cpp -o mpd_gui.cpp.o `pkg-config --cflags opencv` `pkg-config --libs opencv` `freetype-config --cflags` `freetype-config --libs` `taglib-config --cflags` `taglib-config --libs`


Volumio(Debian):

	apt install libcv-dev libopencv-dev opencv-doc fonts-takao-gothic libtag1-dev libcurl4-openssl-dev
	wget "https://raw.githubusercontent.com/dropbox/json11/master/json11.cpp"
	wget "https://raw.githubusercontent.com/dropbox/json11/master/json11.hpp"

	g++ -Ofast -mfpu=neon -pthread -std=c++11 -DVOLUMIO=1 -DENABLE_GETOPT mpd_gui.cpp json11.cpp -o mpd_gui.cpp.o `pkg-config --cflags opencv` `pkg-config --libs opencv` `freetype-config --cflags` `freetype-config --libs` `taglib-config --cflags` `taglib-config --libs` -lcurl -DDISPLAY_13IPS240240

Volumio(debian) with getqueue patch:
	g++ -Ofast -mfpu=neon -pthread -std=c++11 -DVOLUMIO=1 -DVOLUMIO_QUEUE -DENABLE_GETOPT mpd_gui.cpp json11.cpp -o mpd_gui.cpp.o `pkg-config --cflags opencv` `pkg-config --libs opencv` `freetype-config --cflags` `freetype-config --libs` `taglib-config --cflags` `taglib-config --libs` -lcurl -DDISPLAY_13IPS240240

	Display type
		DISPLAY_13IPS240240
		DISPLAY_NANOHATOLED
		DISPLAY_FBDEV_FB0
		DISPLAY_FBDEV_FB1
*/

///////////////////////////////
//  Compile settings
///////////////////////////////

//#define	VOLUME_CTRL_I2C_AK449x	1

#define GPIO_BUTTON_PREV 0
#define GPIO_BUTTON_NEXT 3
#define GPIO_BUTTON_PLAY 2

#include <map>
#include <string>
#include <ctime>
#include <opencv2/opencv.hpp>

#include "common/perf_log.h"
#include "common/img_font.h"
#include "common/ctrl_socket.h"
#include "common/string_util.h"
#include "common/ctrl_socket.h"
#include "CoverArtExtractor.h"
#include "usr_displays.h"

#ifndef VOLUME_CTRL_I2C_AK449x
#define VOLUME_CTRL_I2C_AK449x 0
#endif

#ifndef VOLUMIO
#define VOLUMIO 0
#endif

#if VOLUMIO
#define MUSIC_ROOT_PATH "/mnt/"
#include "common/ctrl_http_curl.h"
#include "json11.hpp"
#else
#define MUSIC_ROOT_PATH "/media/"
#endif

#define FONT_PATH "/usr/share/fonts/truetype/takao-gothic/TakaoPGothic.ttf"
#define FONT_DATE_PATH "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"

#define MPD_HOST "127.0.0.1"
#define MPD_PORT 6600

static std::string m_volumioHost = "127.0.0.1";
//#define VOLUMIO_HOST "127.0.0.1"
#define VOLUMIO_HOST (m_volumioHost.c_str())
#define VOLUMIO_PORT 3000
#define VOLUMIO_IMGPORT 3001
#define POLLING_TIME_MS 250
#define REFRESH_TIME_MS 50


#ifdef ENABLE_GETOPT
#include <getopt.h>
#endif


class MusicController
{
  public:
	static std::map<std::string, std::string> SplitMpdStatus(std::string &str)
	{
		std::map<std::string, std::string> iInfo;
		std::string line;
		std::istringstream stream(str);
		while (std::getline(stream, line))
		{
			std::string::size_type pos = line.find(":");
			if (pos != std::string::npos)
			{
				std::string field(line, 0, pos);
				std::string value(line, pos + 2);

				iInfo[field] = value;
			}
		}

		return iInfo;
	}

	static void PlayPause()
	{
#if VOLUMIO
		std::string iData;
		HttpCurl::Get(VOLUMIO_HOST, VOLUMIO_PORT, "/api/v1/commands/?cmd=toggle", iData);
#else
		Socket iSock;
		std::string str;

		iSock.connect(MPD_HOST, MPD_PORT);

		iSock >> str; // DummyRead
		iSock << "status\n";
		iSock >> str;

		std::map<std::string, std::string> iInfo = SplitMpdStatus(str);

		if (iInfo["state"] == "play")
		{
			iSock << "pause\n";
			iSock >> str; // dummy read
		}
		else
		{
			iSock << "play\n";
			iSock >> str; // dummy read
		}
#endif
	}

	static std::string VolumeCtrl(int offset)
	{
#if VOLUME_CTRL_I2C_AK449x
		unsigned char dataL[2] = {0x03, 0x00};
		unsigned char dataR[2] = {0x04, 0x00};
		int value = 0;

		{
			ctrl_i2c iic("/dev/i2c-0", 0x10);
			iic.write(dataL, 1);
			iic.read(&dataL[1], 1);

			value = dataL[1];

			if (0 < offset)
			{
				value += offset;
				value = 135 < value ? value <= 255 ? value : 255 : 135;
			}
			else if (offset < 0)
			{
				value += offset;
				value = 135 <= value ? value : 0;
			}

			dataL[1] = value;
			dataR[1] = value;

			iic.write(dataL, 2);
			iic.write(dataR, 2);
		}

		{
			ctrl_i2c iicR("/dev/i2c-0", 0x11);
			iicR.write(dataL, 2);
			iicR.write(dataR, 2);
		}

		if (0 < value)
		{
			float db_value = (value - 255) * 0.5f;
			char szBuf[64];
			sprintf(szBuf, "%.1f dB", db_value);

			return szBuf;
		}
		else
		{
			return "- ∁EdB";
		}
#else

#if VOLUMIO
		std::string iData, err;

		HttpCurl::Get(VOLUMIO_HOST, VOLUMIO_PORT, "/api/v1/getstate", iData);
		auto json = json11::Json::parse(iData, err);

		int value = 0;
		if (err.empty())
		{
			try
			{
				value = json["volume"].int_value();
				value += offset;
				value = 0 <= value ? value <= 100 ? value : 100 : 0;

				std::string cmd = "/api/v1/commands/?cmd=volume&volume=" + std::to_string(value);
				HttpCurl::Get(VOLUMIO_HOST, VOLUMIO_PORT, cmd.c_str(), iData);

				return std::to_string(value);
			}
			catch (std::exception &e)
			{
			}

			return "n/a";
		}
		else
		{
			return "n/a";
		}
#else
		Socket iSock;
		std::string str;

		iSock.connect(MPD_HOST, MPD_PORT);

		iSock >> str; // DummyRead
		iSock << "status\n";
		iSock >> str;

		std::map<std::string, std::string> iInfo = SplitMpdStatus(str);
		int value = std::stoi(iInfo["volume"]);

		if (0 <= value)
		{
			value += offset;
			value = 0 <= value ? value <= 100 ? value : 100 : 0;

			{
				char szBuf[256];
				std::string str;

				sprintf(szBuf, "setvol %d\n", value);

				iSock << szBuf;
				iSock >> str; // Dummy read

				sprintf(szBuf, "%d", value);
				return szBuf;
			}
		}
		else
		{
			return "n/a";
		}
#endif
#endif
	}

	static void PlayNextSong()
	{
#if VOLUMIO
		std::string iData;
		HttpCurl::Get(VOLUMIO_HOST, VOLUMIO_PORT, "/api/v1/commands/?cmd=next", iData);
#else
		Socket iSock;
		std::string str;

		iSock.connect(MPD_HOST, MPD_PORT);
		iSock << "next\n";
		iSock >> str;
#endif
	}

	static void PlayPrevSong()
	{
#if VOLUMIO
		std::string iData;
		HttpCurl::Get(VOLUMIO_HOST, VOLUMIO_PORT, "/api/v1/commands/?cmd=prev", iData);
#else
		Socket iSock;
		std::string str;

		iSock.connect(MPD_HOST, MPD_PORT);
		iSock << "previous\n";
		iSock >> str;
#endif
	}
};

void Draw(cv::Mat &dst, int x, int y, cv::Mat &src)
{
	const uint8_t *src_image = src.data;
	int src_stride = src.step;
	uint8_t *dst_image = dst.data;
	int dst_stride = dst.step;
	int cx = src.cols;
	int cy = src.rows;

	if (x < 0)
	{
		src_image -= x * src.elemSize();
		cx += x;
		x = 0;
	}

	if (dst.cols < (x + cx))
	{
		cx = dst.cols - x;
	}

	if (y < 0)
	{
		src_image -= y * src_stride;
		cy += y;
		y = 0;
	}

	if (dst.rows < (y + cy))
	{
		cy = dst.rows - y;
	}

	if ((0 < cx) && (0 < cy))
	{
		dst_image += y * dst_stride + x * dst.elemSize();

		for (int r = 0; r < cy; r++)
		{
			memcpy(dst_image, src_image, cx * src.elemSize());
			src_image += src_stride;
			dst_image += dst_stride;
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

		m_iAreaImage = cv::Mat::zeros(CV_8UC1, cy, cx);
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
	DrawArea_STR(const std::string &tag, uint32_t color, DisplayIF &iDisplay, int x, int y, int cx, int cy, bool isRightAlign = false) : DrawAreaIF(iDisplay, x, y, cx, cy),
																																		 m_iFont(FONT_PATH, m_nRectHeight)
	{
		m_strTag = tag;
		m_nColor = color;
		m_isRightAlign = isRightAlign;
		m_strAttr = "";
	}

	virtual void UpdateInfo(std::map<std::string, std::string> &map)
	{
		auto it = map.find(m_strTag);
		std::string str = it != map.end() ? (*it).second : "";
		auto it_attr = map.find(m_strTag + "_attr");
		std::string str_attr = it_attr != map.end() ? (*it_attr).second : "";

		if (m_nCurrent != str || m_strAttr != str_attr)
		{
			int l, t, r, b;
			m_iFont.CalcRect(l, t, r, b, str.c_str());

			m_iAreaImage = cv::Mat::zeros(m_nRectHeight, m_nRectWidth, CV_8UC4);
			m_iImage = cv::Mat::zeros(m_nRectHeight, r, CV_8UC4);
			m_nCurrent = str;
			m_nOffsetX = m_nRectWidth;
			m_strAttr = str_attr;
			uint32_t color = m_nColor;

			bool attr_highlight = m_strAttr.find("H") != std::string::npos ? true : false;
			bool attr_center = m_strAttr.find("C") != std::string::npos ? true : false;
			if (1 < m_iDisp.GetBPP())
			{
				if (attr_highlight)
				{
					color = 0xffffffff; // ad-hoc
				}
				m_iFont.DrawTextBGRA(0, 0, str.c_str(), color, m_iImage.data, m_iImage.step, m_iImage.cols, m_iImage.rows);
			}
			else
			{
				cv::Mat gray = cv::Mat::zeros(m_iImage.rows, m_iImage.cols, CV_8UC1);

				m_iFont.DrawTextGRAY(0, 0, str.c_str(), 255, gray.data, gray.step, gray.cols, gray.rows);

				cv::threshold(gray, gray, 128, 255, CV_THRESH_BINARY);
				cv::cvtColor(gray, m_iImage, CV_GRAY2BGRA);

				if (attr_highlight)
				{
					cv::bitwise_not(m_iImage, m_iImage);
				}
			}

			if (m_isRightAlign && (r < m_nRectWidth))
			{
				Draw(m_iAreaImage, m_iAreaImage.cols - r, 0, m_iImage);
			}else
			if (attr_center && (r < m_nRectWidth))
			{
				Draw(m_iAreaImage, m_iAreaImage.cols / 2 - r / 2, 0, m_iImage);
			}
			else
			{
				Draw(m_iAreaImage, 0, 0, m_iImage);
			}

			m_iDisp.WriteImageBGRA(m_nRectX, m_nRectY, m_iAreaImage.data, m_iAreaImage.step, m_iAreaImage.cols, m_iAreaImage.rows);
		}

		if (m_nRectWidth < m_iImage.cols)
		{
			int x = m_nOffsetX;

			x = 0 < x ? 0 : x;

			m_nOffsetX -= (m_iDisp.GetSize().width + 239) / 240;
			m_nOffsetX = -m_iImage.cols <= m_nOffsetX ? m_nOffsetX : m_nRectWidth;

			m_iAreaImage = cv::Mat::zeros(m_nRectHeight, m_nRectWidth, CV_8UC4);
			Draw(m_iAreaImage, x, 0, m_iImage);
			m_iDisp.WriteImageBGRA(m_nRectX, m_nRectY, m_iAreaImage.data, m_iAreaImage.step, m_iAreaImage.cols, m_iAreaImage.rows);
		}
	}

  protected:
	std::string m_strTag;
	cv::Mat m_iImage;
	int m_nOffsetX;
	bool m_isRightAlign;
	ImageFont m_iFont;
	uint32_t m_nColor;
	std::string m_strAttr;
};

class DrawArea_StaticText : public DrawAreaIF
{
  public:
	DrawArea_StaticText(const std::string &text, uint32_t color, DisplayIF &iDisplay, int x, int y, int cx, int cy, bool isRightAlign = false) : DrawAreaIF(iDisplay, x, y, cx, cy),
																																				 m_iFont(FONT_PATH, m_nRectHeight)
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
			int l, t, r, b;
			m_iFont.CalcRect(l, t, r, b, str.c_str());

			m_iAreaImage = cv::Mat::zeros(m_nRectHeight, m_nRectWidth, CV_8UC4);
			m_iImage = cv::Mat::zeros(m_nRectHeight, r, CV_8UC4);
			m_nCurrent = str;
			m_nOffsetX = m_nRectWidth;

			if (1 < m_iDisp.GetBPP())
			{
				m_iFont.DrawTextBGRA(0, 0, str.c_str(), m_nColor, m_iImage.data, m_iImage.step, m_iImage.cols, m_iImage.rows);
			}
			else
			{
				cv::Mat gray = cv::Mat::zeros(m_iImage.rows, m_iImage.cols, CV_8UC1);

				m_iFont.DrawTextGRAY(0, 0, str.c_str(), 255, gray.data, gray.step, gray.cols, gray.rows);

				cv::threshold(gray, gray, 128, 255, CV_THRESH_BINARY);
				cv::cvtColor(gray, m_iImage, CV_GRAY2BGRA);
			}

			if (m_isRightAlign && (r < m_nRectWidth))
			{
				Draw(m_iAreaImage, m_iAreaImage.cols - r, 0, m_iImage);
			}
			else
			{
				Draw(m_iAreaImage, 0, 0, m_iImage);
			}

			m_iDisp.WriteImageBGRA(m_nRectX, m_nRectY, m_iAreaImage.data, m_iAreaImage.step, m_iAreaImage.cols, m_iAreaImage.rows);
		}

		if (m_nRectWidth < m_iImage.cols)
		{
			int x = m_nOffsetX;

			x = 0 < x ? 0 : x;

			m_nOffsetX -= (m_iDisp.GetSize().width + 239) / 240;
			m_nOffsetX = -m_iImage.cols <= m_nOffsetX ? m_nOffsetX : m_nRectWidth;

			m_iAreaImage = cv::Mat::zeros(m_nRectHeight, m_nRectWidth, CV_8UC4);
			Draw(m_iAreaImage, x, 0, m_iImage);
			m_iDisp.WriteImageBGRA(m_nRectX, m_nRectY, m_iAreaImage.data, m_iAreaImage.step, m_iAreaImage.cols, m_iAreaImage.rows);
		}
	}

  protected:
	std::string m_strText;
	cv::Mat m_iImage;
	int m_nOffsetX;
	bool m_isRightAlign;
	ImageFont m_iFont;
	uint32_t m_nColor;
};

class DrawArea_MyIpAddr : public DrawAreaIF
{
  public:
	DrawArea_MyIpAddr(uint32_t color, DisplayIF &iDisplay, int x, int y, int cx, int cy, bool isRightAlign = false) : DrawAreaIF(iDisplay, x, y, cx, cy),
																													  m_iFont(FONT_PATH, m_nRectHeight)
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
				m_nColor = 0xFFFF8080;
			}
			else
			{
				m_nColor = 0xFFFFFFFF;
			}
		}

		if (m_nCurrent != m_strText)
		{
			m_nCurrent = m_strText;

			int l, t, r, b;
			m_iFont.CalcRect(l, t, r, b, m_strText.c_str());

			m_iAreaImage = cv::Mat::zeros(m_nRectHeight, m_nRectWidth, CV_8UC4);
			m_iImage = cv::Mat::zeros(m_nRectHeight, r, CV_8UC4);
			m_nCurrent = m_strText;
			m_nOffsetX = m_nRectWidth;

			if (1 < m_iDisp.GetBPP())
			{
				m_iFont.DrawTextBGRA(0, 0, m_nCurrent.c_str(), m_nColor, m_iImage.data, m_iImage.step, m_iImage.cols, m_iImage.rows);
			}
			else
			{
				cv::Mat gray = cv::Mat::zeros(m_iImage.rows, m_iImage.cols, CV_8UC1);

				m_iFont.DrawTextGRAY(0, 0, m_nCurrent.c_str(), 255, gray.data, gray.step, gray.cols, gray.rows);

				cv::threshold(gray, gray, 128, 255, CV_THRESH_BINARY);
				cv::cvtColor(gray, m_iImage, CV_GRAY2BGRA);
			}

			if (m_isRightAlign && (r < m_nRectWidth))
			{
				Draw(m_iAreaImage, m_iAreaImage.cols - r, 0, m_iImage);
			}
			else
			{
				Draw(m_iAreaImage, 0, 0, m_iImage);
			}

			m_iDisp.WriteImageBGRA(m_nRectX, m_nRectY, m_iAreaImage.data, m_iAreaImage.step, m_iAreaImage.cols, m_iAreaImage.rows);
		}

		if (m_nRectWidth < m_iImage.cols)
		{
			int x = m_nOffsetX;

			x = 0 < x ? 0 : x;

			m_nOffsetX -= (m_iDisp.GetSize().width + 239) / 240;
			m_nOffsetX = -m_iImage.cols <= m_nOffsetX ? m_nOffsetX : m_nRectWidth;

			m_iAreaImage = cv::Mat::zeros(m_nRectHeight, m_nRectWidth, CV_8UC4);
			Draw(m_iAreaImage, x, 0, m_iImage);
			m_iDisp.WriteImageBGRA(m_nRectX, m_nRectY, m_iAreaImage.data, m_iAreaImage.step, m_iAreaImage.cols, m_iAreaImage.rows);
		}
	}

  protected:
	std::chrono::high_resolution_clock::time_point m_iLastChecked;
	std::string m_strText;
	cv::Mat m_iImage;
	int m_nOffsetX;
	bool m_isRightAlign;
	ImageFont m_iFont;
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

		m_iAreaImage = cv::Mat::zeros(m_nRectHeight, m_nRectWidth, CV_8UC4);

		if (3 <= m_nRectHeight)
		{
			int w = (m_iAreaImage.cols - 2) * elapsed / duration;

			cv::rectangle(
				m_iAreaImage,
				cv::Point2i(0, 0),
				cv::Point2i(m_iAreaImage.cols - 1, m_iAreaImage.rows - 1),
				cv::Scalar(128, 128, 128, 255),
				1);

			cv::rectangle(
				m_iAreaImage,
				cv::Point2i(1, 1),
				cv::Point2i(1 + w, m_iAreaImage.rows - 2),
				cv::Scalar(255, 255, 255, 255), // B,G,R,A
				CV_FILLED);
		}
		else
		{
			int w = (m_iAreaImage.cols - 1) * elapsed / duration;

			cv::rectangle(
				m_iAreaImage,
				cv::Point2i(0, 0),
				cv::Point2i(w, m_iAreaImage.rows - 1),
				cv::Scalar(255, 255, 255, 255), // B,G,R,A
				CV_FILLED);
		}

		m_iDisp.WriteImageBGRA(m_nRectX, m_nRectY, m_iAreaImage.data, m_iAreaImage.step, m_iAreaImage.cols, m_iAreaImage.rows);
	}
};

class DrawArea_Marker : public DrawAreaIF
{
	private:
  public:
	DrawArea_Marker(const std::string &tag, DisplayIF &iDisplay, int x, int y, int cx, int cy, int direction = 0) : DrawAreaIF(iDisplay, x, y, cx, cy){
		m_strTag = tag;
		m_nDirection = direction;
	}

	virtual void UpdateInfo(std::map<std::string, std::string> &map)
	{
		auto it = map.find(m_strTag);
		int dy = m_nRectHeight -1;
		int dx = m_nRectWidth -1;
		m_iAreaImage = cv::Mat::zeros(m_nRectHeight, m_nRectWidth, CV_8UC4);

		if (it != map.end() && !(*it).second.empty()){
			cv::Point pt[3];
			if (m_nDirection == 0) {
 		  	pt[0] = cv::Point2i(0, dy);
 		  	pt[1] = cv::Point2i(dx, dy);
 		  	pt[2] = cv::Point2i(dx/2, 0);
			}else{
	 	  	pt[0] = cv::Point2i(0, 0);
 		  	pt[1] = cv::Point2i(dx, 0);
 		  	pt[2] = cv::Point2i(dx/2, dy);
			}
 	  		cv::fillConvexPoly( m_iAreaImage, pt, 3, cv::Scalar(255, 255, 255, 255) ); 
		}
		m_iDisp.WriteImageBGRA(m_nRectX, m_nRectY, m_iAreaImage.data, m_iAreaImage.step, m_iAreaImage.cols, m_iAreaImage.rows);
	}

	protected:
		std::string m_strTag;
		int m_nDirection;
};

class DrawArea_CoverImage : public DrawAreaIF
{
	private:
		bool m_nBorder;
  public:
	DrawArea_CoverImage(DisplayIF &iDisplay, int x, int y, int cx, int cy, bool border = true ) : DrawAreaIF(iDisplay, x, y, cx, cy), m_nBorder(border) {};

	virtual void UpdateInfo(std::map<std::string, std::string> &map)
	{
#if VOLUMIO
		auto it = map.find("AlbumArt");
#else
		auto it = map.find("file");
#endif
		std::string str = it != map.end() ? (*it).second : "";

		if (m_nCurrent != str)
		{
			m_nCurrent = str;

			// Create frame
			cv::Mat image = cv::Mat::zeros(m_nRectHeight, m_nRectWidth, CV_8UC4);
			cv::Mat cover;

			if (m_nBorder) {
				cv::rectangle(
					image,
					cv::Point2i(0, 0),
					cv::Point2i(image.cols - 1, image.rows - 1),
					cv::Scalar(255, 255, 255, 255),
					1);
			}
#if VOLUMIO
			// Load from Volumio
			if (map.find("AlbumArt") != map.end())
			{
				std::vector<unsigned char> iData;
				if (!HttpCurl::Get(VOLUMIO_HOST, VOLUMIO_IMGPORT, map["AlbumArt"].c_str(), iData))
				{
					cover = cv::imdecode(iData, cv::IMREAD_COLOR);
				}
				else
				{
					m_nCurrent = "";
				}
			}
			else
#endif
				// Load music file
				if (0 != str.find("http://"))
			{
				std::string path = MUSIC_ROOT_PATH + str;
				std::vector<unsigned char> coverArt = ExtractCoverArt(path.c_str());

				if (!coverArt.empty())
				{
					cover = cv::imdecode(coverArt, cv::IMREAD_COLOR);
				}

				if (cover.empty())
				{
					path = path.substr(0, path.rfind('/') + 1);
					path += "front.jpg";

					cover = cv::imread(path);
				}
			}

			if (!cover.empty())
			{
				cv::cvtColor(cover, cover, cv::COLOR_BGR2BGRA);
				if (m_nBorder) {
					cv::resize(cover, cover, cv::Size(image.cols - 2, image.rows - 2), 0, 0, cv::INTER_AREA);
					cover.copyTo(image(cv::Rect(1, 1, cover.cols, cover.rows)));
				}else{
					cv::resize(cover, cover, cv::Size(image.cols, image.rows), 0, 0, cv::INTER_AREA);
					cover.copyTo(image(cv::Rect(0, 0, cover.cols, cover.rows)));
				}
			}
			else
			{
				const char *pszText = "NoImage";
				int cx, cy, l, t, r, b;
				ImageFont iFont(FONT_PATH, image.rows / 4);

				iFont.CalcRect(l, t, r, b, pszText);
				cx = r - l;
				cy = b - t;

				iFont.DrawTextBGRA(
					(image.cols - cx) / 2 - l,
					(image.rows - cy) / 2 - t,
					pszText,
					0xFFFFFFFF,
					image.data,
					image.step,
					image.cols,
					image.rows);
#if VOLUMIO
				m_nCurrent = "";
#endif
			}

			m_iDisp.WriteImageBGRA(m_nRectX, m_nRectY, image.data, image.step, image.cols, image.rows);
		}
	}
};

class DrawArea_CpuTemp : public DrawAreaIF
{
  public:
	DrawArea_CpuTemp(DisplayIF &iDisplay, int x, int y, int cx, int cy, bool isRightAlign = false) : DrawAreaIF(iDisplay, x, y, cx, cy),
																									 m_iFont(FONT_PATH, m_nRectHeight)
	{
		m_isRightAlign = isRightAlign;
	};

	virtual void UpdateInfo(std::map<std::string, std::string> &map)
	{
		int x = 0;
		char buf[128];
		float cpuTemp = std::stof(StringUtil::GetTextFromFile("/sys/class/thermal/thermal_zone0/temp")) / 1000.0f;

		sprintf(buf, u8"cpu %.1f C", cpuTemp);

		if (m_isRightAlign)
		{
			int l, t, r, b;
			m_iFont.CalcRect(l, t, r, b, buf);

			x = m_nRectWidth - r;
		}

		m_iAreaImage = cv::Mat::zeros(m_nRectHeight, m_nRectWidth, CV_8UC1);
		m_iFont.DrawTextGRAY(x, 0, buf, 255, m_iAreaImage.data, m_iAreaImage.step, m_iAreaImage.cols, m_iAreaImage.rows);
		m_iDisp.WriteImageGRAY(m_nRectX, m_nRectY, m_iAreaImage.data, m_iAreaImage.step, m_iAreaImage.cols, m_iAreaImage.rows);
	}

  protected:
	ImageFont m_iFont;
	bool m_isRightAlign;
};

class DrawArea_Date : public DrawAreaIF
{
  public:
	DrawArea_Date(DisplayIF &iDisplay, int x, int y, int cx, int cy) : DrawAreaIF(iDisplay, x, y, cx, cy),
																	   m_iFont(FONT_DATE_PATH, m_nRectHeight, false)
	{
		int l, t, r, b;
		m_iFont.CalcRect(l, t, r, b, "0000/00/00");

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

			m_iAreaImage = cv::Mat::zeros(m_nRectHeight, m_nRectWidth, CV_8UC1);
			m_iFont.DrawTextGRAY(m_nOffsetX, m_nOffsetY, buf, 255, m_iAreaImage.data, m_iAreaImage.step, m_iAreaImage.cols, m_iAreaImage.rows);
			m_iDisp.WriteImageGRAY(m_nRectX, m_nRectY, m_iAreaImage.data, m_iAreaImage.step, m_iAreaImage.cols, m_iAreaImage.rows);
		}
	}

  protected:
	ImageFont m_iFont;
	int m_nOffsetX;
	int m_nOffsetY;
};

class DrawArea_Time : public DrawAreaIF
{
  public:
	DrawArea_Time(DisplayIF &iDisplay, int x, int y, int cx, int cy) : DrawAreaIF(iDisplay, x, y, cx, cy),
																	   m_iFont(FONT_DATE_PATH, m_nRectHeight, false)
	{
		int l, t, r, b;
		m_iFont.CalcRect(l, t, r, b, "88:88");

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

			m_iAreaImage = cv::Mat::zeros(m_nRectHeight, m_nRectWidth, CV_8UC1);
			m_iFont.DrawTextGRAY(m_nOffsetX, m_nOffsetY, buf, 255, m_iAreaImage.data, m_iAreaImage.step, m_iAreaImage.cols, m_iAreaImage.rows);
			m_iDisp.WriteImageGRAY(m_nRectX, m_nRectY, m_iAreaImage.data, m_iAreaImage.step, m_iAreaImage.cols, m_iAreaImage.rows);
		}
	}

  protected:
	ImageFont m_iFont;
	int m_nOffsetX;
	int m_nOffsetY;
};

class MenuController
{
  private:
  protected:
	std::string name;
	std::vector<class MenuController *> m_iMenulist; // submenu

	int addmenu(MenuController *iMenu)
	{
		m_iMenulist.push_back(iMenu);
		menulists.push_back(iMenu->getname());
		iMenu->prev = this;
	}

  public:
	std::vector<std::string> menulists;
	int index;
	MenuController *prev; // parent

	virtual int refresh() { return 0; }
	virtual MenuController *select() { return this; }
	virtual MenuController *enter() { return prev; }
	std::string getname() { return name; }
};

class MenuQueuelist : public MenuController
{

  public:
	MenuQueuelist()
	{
		name = "Queue";
	}
	int refresh()
	{
		menulists.clear();

		{
			// call Volumio REST API
			std::string iData;
			HttpCurl::Get(VOLUMIO_HOST, VOLUMIO_PORT, "/api/v1/getqueue", iData);

			// json parser
			std::string err;
			auto json = json11::Json::parse(iData, err);

			// create list
			for (auto it : json.array_items())
				menulists.push_back(std::to_string(menulists.size()+1) + ". " +
						it["name"].string_value() + " / " + it["album"].string_value() + " / " + it["artist"].string_value());
		}
		{
			// call Volumio REST API
			std::string iData;
			HttpCurl::Get(VOLUMIO_HOST, VOLUMIO_PORT, "/api/v1/getstate", iData);

			// json parser
			std::string err;
			auto json = json11::Json::parse(iData, err);
			index = json["position"].int_value();
		}
	}

	MenuController *enter()
	{

		if (index < 0 || index >= menulists.size())
		{
			return NULL;
		}
		std::string cmd = "/api/v1/commands/?cmd=play&N=" + std::to_string(index);

		std::string iData;
		HttpCurl::Get(VOLUMIO_HOST, VOLUMIO_PORT, cmd.c_str(), iData);
		return this; // stay menu
	}
};

class MenuPlaylist : public MenuController
{

  public:
	MenuPlaylist()
	{
		name = "Playlists";
	}
	int refresh()
	{
		menulists.clear();

		// call Volumio REST API
		std::string iData;
		HttpCurl::Get(VOLUMIO_HOST, VOLUMIO_PORT, "/api/v1/listplaylists", iData);

		// json parser
		std::string err;
		auto json = json11::Json::parse(iData, err);

		// create list
		for (auto it : json.array_items())
			menulists.push_back(it.string_value());

		index = 0;
	}

	MenuController *enter()
	{

		if (index < 0 || index >= menulists.size())
		{
			return NULL;
		}
		std::string cmd = "/api/v1/commands/?cmd=playplaylist&name=" + menulists[index];

		std::string iData;
		HttpCurl::Get(VOLUMIO_HOST, VOLUMIO_PORT, cmd.c_str(), iData);
		return NULL; // close menu
	}
};

class MenuAlsamixerDetail : public MenuController
{
  private:
	int numid;

  public:
	MenuAlsamixerDetail(std::string &mixername, int _numid, std::vector<std::string> &lists, int _index) : numid(_numid)
	{
		name = mixername;
		menulists = lists;
		index = _index;
	}

	MenuController *select()
	{
		if (index >= 0 && index < menulists.size())
		{
			std::string cmd = "amixer cset numid=" + std::to_string(numid) + " " + std::to_string(index);
			std::system(cmd.c_str());
		}
		return this; // stay menu
	}

	MenuController *enter()
	{
		return NULL; // close menu
	}
};

class MenuAlsamixer : public MenuController
{
  private:
	int parse_key(std::string &line, const char *key, std::string &data, const char *beginmark, const char *endmark)
	{
		std::string word(key);

		/* search key */
		std::string::size_type pos = line.find(word);
		if (pos == std::string::npos)
		{
			return 0; // data none
		}

		/* search begin mark */
		std::string::size_type stapos = line.find(beginmark, pos);
		if (stapos == std::string::npos)
		{
			return 0; // data none
		}
		stapos += strlen(beginmark);

		/* search end mark */
		std::string::size_type endpos = line.find(endmark, stapos);
		if (endpos == std::string::npos)
		{
			return 0; // data none
		}

		data = line.substr(stapos, endpos - stapos);
		return 1;
	}

  public:
	MenuAlsamixer()
	{
		name = "AlsaMixer";
	}
	int refresh()
	{
		/* clear list */
		for (auto it : m_iMenulist)
			delete it;
		m_iMenulist.clear();
		menulists.clear();

		/* execute amixer */
		std::string cmd = "amixer contents";
		std::array<char, 256> buff;
		std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
		if (!pipe)
		{
			throw std::runtime_error("popen failed.");
		}

		/* parse amixer result */
		std::string numid;
		std::string iface;
		std::string type;
		std::string values;
		std::string mixer_name;
		std::vector<std::string> lists;
		while (fgets(buff.data(), buff.size(), pipe.get()) != nullptr)
		{
			std::string line(buff.data());
			if (parse_key(line, "numid=", numid, "=", ","))
			{
				lists.clear();
				parse_key(line, "iface=", iface, "=", ",");
				parse_key(line, "name=", mixer_name, "'", "'");
				continue;
			}
			if (parse_key(line, "type=", type, "=", ","))
			{
				continue;
			}
			if (parse_key(line, "values=", values, "=", "\n"))
			{
				if (type == "ENUMERATED" && !mixer_name.empty() && !lists.empty())
				{
					addmenu(new MenuAlsamixerDetail(mixer_name, std::stoi(numid), lists, std::stoi(values)));
				}
				continue;
			}
			std::string data;
			if (parse_key(line, "Item #", data, "'", "'"))
			{
				lists.push_back(data);
			}
		}
		index = 0;
		return 0;
	}

	MenuController *enter()
	{
		if (index < 0 || index >= menulists.size())
		{
			return NULL;
		}
		return m_iMenulist[index];
	}
};

static bool eventCoverArt = false; // TODO event
class MenuMiscellaneous : public MenuController
{
  public:
	MenuMiscellaneous()
	{
		menulists.push_back("CoverArt mode");
		menulists.push_back("Restart Volumio");
		menulists.push_back("Restart MPD");
//		menulists.push_back("Restart AirPlay");
		index = 0;
		name = "Miscellaneous";
	}

	MenuController *enter()
	{
		if (index < 0 || index >= menulists.size())
		{
			return NULL;
		}
		switch (index) {
			case 0:
			eventCoverArt = true;
			break;

			case 1:
			std::system("service volumio restart");
			break;

			case 2:
			std::system("service mpd restart");
			break;

			case 3:
			std::string iData;
			HttpCurl::Get(VOLUMIO_HOST, VOLUMIO_PORT, "/api/v1/commands/?cmd=stopAirplay", iData);
			sleep(3);
			HttpCurl::Get(VOLUMIO_HOST, VOLUMIO_PORT, "/api/v1/commands/?cmd=startAirplay", iData);
			break;
		}
		return NULL;
	}
};

class MenuReboot : public MenuController
{
  public:
	MenuReboot()
	{
		menulists.push_back("Cancel");
		menulists.push_back("OK");
		name = "Reboot";
	}
	int refresh()
	{
		index = 0;
	}

	MenuController *enter()
	{
		if (index == 1)
		{
			std::system("reboot");
			while (1)
				sleep(1);
		}
		return prev; // leave menu
	}
};

class MenuShutdown : public MenuController
{

  public:
	MenuShutdown()
	{
		menulists.push_back("Cancel");
		menulists.push_back("OK");
		name = "Shutdown";
	}
	int refresh()
	{
		index = 0;
	}

	MenuController *enter()
	{
		if (index == 1)
		{
			std::system("shutdown -h now");
			while (1)
				sleep(1);
		}
		return prev; // leave menu
	}
};

class MenuPoweroff : public MenuController
{
  private:
  public:
	MenuPoweroff()
	{
		addmenu(new MenuReboot());
		addmenu(new MenuShutdown());
		index = 0;
		name = "PowerOff";
	}

	MenuController *enter()
	{
		if (index < 0 || index >= menulists.size())
		{
			return NULL;
		}
		return m_iMenulist[index];
	}
};

class MenuMain : public MenuController
{
  private:
  public:
	MenuMain()
	{
#ifdef VOLUMIO_QUEUE
		addmenu(new MenuQueuelist());
#endif
		addmenu(new MenuPlaylist());
		addmenu(new MenuAlsamixer());
		addmenu(new MenuMiscellaneous());
		addmenu(new MenuPoweroff());
		index = 0;
		prev = NULL;
		name = "Main Menu";
	}

	MenuController *enter()
	{
		if (index < 0 || index >= menulists.size())
		{
			return NULL;
		}
		m_iMenulist[index]->refresh();
		return m_iMenulist[index];
	}
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

			m_iMenuroot = new MenuMain();
		}

		////////////////////////////////////
		// Gpio interrupt
		////////////////////////////////////
		SetupButtons();

		return true;
	}

	static std::string getMenuKey(int index) { return "MenuList" + std::to_string(index + 1); }
	void createMenulist()
	{
		int index, startpos = 1;
		iInfo["MenuTitle"] = m_iMenucurrent->getname();
		iInfo["Time"] = std::to_string(100);
		iInfo["elapsed"] = std::to_string(100);

		// clear marker
		iInfo["MarkerTop"] = "";
		iInfo["MarkerBottom"] = "";

		// calc start index & check prev mark
		if ((m_iMenucurrent->menulists.size() >= m_iMenuMaxlists) &&
			(m_iMenucurrent->index >= ((m_iMenuMaxlists + 1) / 2) - 1))
		{
			iInfo["MarkerTop"] = "mark";
			startpos = 0;
			if ((m_iMenucurrent->menulists.size() - m_iMenucurrent->index) < (m_iMenuMaxlists + 1) / 2)
			{
				// bottom
				index = m_iMenucurrent->menulists.size() - m_iMenuMaxlists ;
			}
			else
			{
				// middle
				index = m_iMenucurrent->index - ((m_iMenuMaxlists + 1) / 2 - 1);
			}
		}
		else
		{
			// top
			iInfo[getMenuKey(0)] = "..";
			if (m_iMenucurrent->index == -1)
				iInfo[getMenuKey(0) + "_attr"] = "H";
			else
				iInfo[getMenuKey(0) + "_attr"] = "";
			index = 0;
		}

		// check next mark
		if ((index + m_iMenuMaxlists - 1) < m_iMenucurrent->menulists.size())
		{
			iInfo["MarkerBottom"] = "mark";
		}

		// fill list
		for (int i = startpos; i < m_iMenuMaxlists; i++, index++)
		{
			iInfo[getMenuKey(i)] = (index < m_iMenucurrent->menulists.size() ? m_iMenucurrent->menulists[index] : " ");
			if (m_iMenucurrent->index == index)
				iInfo[getMenuKey(i) + "_attr"] = "H";
			else
				iInfo[getMenuKey(i) + "_attr"] = "";
		}
	}

	void selectMenulist()
	{
		if (m_iMenucurrent->index >= 0)
		{
			m_iMenucurrent = m_iMenucurrent->select();
		}
		if (m_iMenucurrent)
		{
			createMenulist();
		}
		else
		{
			m_isMenuMode = false;
			iInfo.clear();
		}
	}
	void enterMenulist()
	{
		if (m_iMenucurrent->index >= 0)
		{
			m_iMenucurrent = m_iMenucurrent->enter();
		}
		else
		{
			m_iMenucurrent = m_iMenucurrent->prev;
		}
		if (m_iMenucurrent)
		{
			createMenulist();
		}
		else
		{
			m_isMenuMode = false;
			iInfo.clear();
		}
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
		m_isVolumeCtrlMode = false;

		while (1)
		{

			///////////////////////////////////////////
			// Receive current playback info.
			///////////////////////////////////////////
			{
				std::string strStatus;
#if VOLUMIO
				auto time = std::chrono::system_clock::now();
				double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(time - m_iPrevGetStatusTime).count();
				if (!m_isMenuMode && (!iInfo.size() || elapsed > POLLING_TIME_MS))
				{
					m_iPrevGetStatusTime = time;
					std::string iData;

					/* get state */
					HttpCurl::Get(VOLUMIO_HOST, VOLUMIO_PORT, "/api/v1/getstate", iData);

					/* parse json */
					{
						std::string err;
						auto json = json11::Json::parse(iData, err);
						iInfo.clear();
						if (err.empty()) {
							/* map to mpd status */
							iInfo["state"] = json["status"].string_value();
							iInfo["Title"] = json["title"].string_value();
							iInfo["Album"] = json["album"].string_value();
							iInfo["Artist"] = json["artist"].string_value();
							iInfo["AlbumArt"] = json["albumart"].string_value();
							iInfo["volume"] = std::to_string(json["volume"].int_value());
							iInfo["Time"] = std::to_string(json["duration"].int_value() * 1000);
							iInfo["elapsed"] = std::to_string(json["seek"].int_value());

							if (!json["uri"].string_value().empty())
							{ // normal mode
								iInfo["file"] = json["uri"].string_value();
							}
							else
							{ // airplay mode
								iInfo["file"] = iInfo["Title"] + iInfo["Album"] + iInfo["Artist"];
							}

							iInfo["trackType"] = json["trackType"].string_value();
							iInfo["samplerate"] = json["samplerate"].string_value();
							iInfo["bitdepth"] = json["bitdepth"].string_value();
							iInfo["position"] = json["position"].string_value();

							iInfo["connected"] = "connected";
						}
					}
				}
#else
				{
					Socket iSock;
					std::string strCurrentSong;

					if (0 == iSock.connect(MPD_HOST, MPD_PORT))
					{
						try
						{
							iSock >> strStatus; // DummyRead

							iSock << "currentsong\n";
							iSock >> strCurrentSong;
							iSock << "status\n";
							iSock >> strStatus;
						}
						catch (...)
						{
						}
					}

					strStatus += strCurrentSong;
				}

				//				printf( "%s\n", strStatus.c_str() );

				iInfo = MusicController::SplitMpdStatus(strStatus);

				if (iInfo.find("state") == iInfo.end())
				{
					iInfo["state"] = "???";
				}

				if (iInfo.find("Date") != iInfo.end())
				{
					iInfo["Date"] = iInfo["Date"].substr(0, 4);
				}

				auto itF = iInfo.find("file");
				if (itF != iInfo.end())
				{
					if (iInfo.find("Title") == iInfo.end())
					{
						std::vector<std::string> names = StringUtil::SplitReverse((*itF).second, "/", 3);

						iInfo["Title"] = names[0];
						iInfo["Album"] = names[1];
						iInfo["Artist"] = names[2];
					}
					else if (0 == (*itF).second.find("http://"))
					{
						std::vector<std::string> names = StringUtil::Split(iInfo["Title"], " - ", 3);

						iInfo["Artist"] = names[0];
						iInfo["Title"] = names[1];
						iInfo["Album"] = names[2];

						iInfo["Album"] = iInfo["Name"];
					}
				}
#endif
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

			if (eventCoverArt){ // TODO In the future implementation by event driven
				eventCoverArt =  false;
				m_isCoverArt = true;
			}
			if (m_isCoverArt) 
			{
				m_eDisplayMode = DISPLAY_MODE_COVERART;
			}
			if (m_isVolumeCtrlMode)
			{
				m_eDisplayMode = DISPLAY_MODE_VOLUME;

				iInfo["volume"] = MusicController::VolumeCtrl(
					(m_isButtonNextPressed ? 1 : 0) +
					(m_isButtonPrevPressed ? -1 : 0));
			}

			if (!m_isMenuMode)
			{
				if (m_isButtonPlayPressed)
				{
					double elapsed = std::chrono::duration_cast<std::chrono::seconds>(
										 std::chrono::system_clock::now() -
										 m_iButtonPlayPressedTime)
										 .count();

					if (1.0 <= elapsed)
					{ // Press and hold the play button to switch to the menu screen
						m_isMenuMode = true;
						m_isButtonPlayPressed = false;
						// set root menu
						m_iMenucurrent = m_iMenuroot;
						// create menu list
						createMenulist();
					}
				}
			}
			if (m_isMenuMode)
			{
				if (m_isButtonPlayPressed) // Play button long pressed in menu mode
				{
					double elapsed = std::chrono::duration_cast<std::chrono::seconds>(
										 std::chrono::system_clock::now() -
										 m_iButtonPlayPressedTime)
										 .count();

					if (1.0 <= elapsed)
					{ // Press and hold the play button to switch to the idle/songinfo screen
						m_isMenuMode = false;
						m_isButtonPlayPressed = false;
						// set root menu
						m_iMenucurrent = NULL;
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

				std::this_thread::sleep_for(std::chrono::milliseconds(REFRESH_TIME_MS));
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

				std::this_thread::sleep_for(std::chrono::milliseconds(100));
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

				std::this_thread::sleep_for(std::chrono::milliseconds(20));
				break;

			case DISPLAY_MODE_MENU:
				for (auto it : m_iDrawAreasMenu)
				{
					it->UpdateInfo(iInfo);
				}

				for (auto it : m_iDisplays)
				{
					it->Flush();
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(50));
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

				std::this_thread::sleep_for(std::chrono::milliseconds(REFRESH_TIME_MS));
				break;
			}
		}

		m_iGpioIntCtrl.ThreadStop();
		return;
	}

  protected:
	static void SetupLayout_SongInfo(std::vector<DrawAreaIF *> &iDrawAreas, DisplayIF *it)
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
				big = 48 * cy / 240;
				med = 36 * cy / 240;
				sml = 28 * cy / 240;
				ind = 4 * cy / 240;

				x = m;
				y = m;
				iDrawAreas.push_back(new DrawArea_STR("Title", blue, *it, x, y, cx - 2 * x, big));
				y += big + 2;
				iDrawAreas.push_back(new DrawArea_PlayPos(*it, x, y, cx - 2 * x, ind));
				y += ind + 2;
				iDrawAreas.push_back(new DrawArea_STR("Artist", white, *it, x, y, cx - 2 * x, med));
				y += med + 2;

				x = m * 2;
				csz = cy - y - m * 2;
				iDrawAreas.push_back(new DrawArea_CoverImage(*it, x, y, csz, csz));
				x += csz + 8;
				iDrawAreas.push_back(new DrawArea_STR("Album", white, *it, x, y, cx - x, med));
				y += med + 2;
				iDrawAreas.push_back(new DrawArea_STR("Date", white, *it, x, y, cx - x, med));
				y += med + 2;
				iDrawAreas.push_back(new DrawArea_STR("audio", white, *it, x, cy - sml - m, cx - x, sml));

				iDrawAreas.push_back(new DrawArea_CpuTemp(*it, x, y, cx - x, med));
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
				iDrawAreas.push_back(new DrawArea_CoverImage(*it, x, y, csz, csz));

				x += csz + 4;

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

			x = m * 2;
			csz = cy - y - m * 2;
			iDrawAreas.push_back(new DrawArea_CoverImage(*it, x, y, csz, csz));
			x += csz + 8;
#if VOLUMIO
			iDrawAreas.push_back(new DrawArea_STR("trackType", white, *it, x, cy - sml * 3 - med - m * 4, cx - x, med));
			y += med + 2;
			iDrawAreas.push_back(new DrawArea_STR("samplerate", white, *it, x, cy - sml * 3 - m * 3, cx - x, sml));
			y += sml + 2;
			iDrawAreas.push_back(new DrawArea_STR("bitdepth", white, *it, x, cy - sml * 2 - m * 2, cx - x, sml));
			y += sml + 2;
			iDrawAreas.push_back(new DrawArea_CpuTemp(*it, x, cy - sml - m, cx - x, sml));
			y += med + 2;
#else
			iDrawAreas.push_back(new DrawArea_STR("Date", white, *it, x, y, cx - x, med));
			y += med + 2;
			iDrawAreas.push_back(new DrawArea_CpuTemp(*it, x, cy - sml * 2 - m * 2, cx - x, sml));
			y += med + 2;
			iDrawAreas.push_back(new DrawArea_STR("audio", white, *it, x, cy - sml - m, cx - x, sml));
#endif
		}
		else
		{
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

			csz = cy - y - 4 - med - 2 - sml;
			csz = csz < cx ? csz : cx;

			iDrawAreas.push_back(new DrawArea_CoverImage(*it, (cx - csz) / 2, y, csz, csz));
			y += csz + 4;
			iDrawAreas.push_back(new DrawArea_STR("Album", white, *it, x, y, cx - 2 * x, med, false));
			y += med + 2;
			iDrawAreas.push_back(new DrawArea_STR("audio", white, *it, x, cy - sml, cx - 2 * x, sml, true));
		}
	}

	static void SetupLayout_Volume(std::vector<DrawAreaIF *> &iDrawAreas, DisplayIF *it)
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

	static void SetupLayout_Idle(std::vector<DrawAreaIF *> &iDrawAreas, DisplayIF *it)
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

		// ip addr
		iDrawAreas.push_back(new DrawArea_MyIpAddr(0xFFFFFFFF, *it, 0, y, it->GetSize().width - x, cy, true));
		y += cy;

		// cpu temp
		iDrawAreas.push_back(new DrawArea_CpuTemp(*it, 0, y, it->GetSize().width - x, cy, true));
		y += cy;
	
#if VOLUMIO
		if (it->GetSize().height >=200 ) {
			iDrawAreas.push_back(new DrawArea_STR("connected", 0xFFFFFFFF, *it, 0, it->GetSize().height-16, it->GetSize().width - x, 16, true));
		}
#endif
	}

	static int SetupLayout_Menu(std::vector<DrawAreaIF *> &iDrawAreas, DisplayIF *it)
	{
		int x, y, csz, big, med, sml, ind;
		int cx = it->GetSize().width;
		int cy = it->GetSize().height;
		int menulinenums = 0;

		uint32_t white = 0xaaaaaaaa;
		//		uint32_t white = 0xFFFFFFFF;
		uint32_t blue = 0xFF0095D8;

		if (cx == cy)
		{
			int m = 2;
			big = 36 * cy / 240;
			med = 24 * cy / 240;
			ind = 6 * cy / 240;

			int mark_dx = 13;
			int mark_dy = 6;

			x = m;
			y = m;
			iDrawAreas.push_back(new DrawArea_STR("MenuTitle", blue, *it, x, y, cx - 2 * x, big, false));
			y += big + 2;
			iDrawAreas.push_back(new DrawArea_PlayPos(*it, x, y, cx - 2 * x, ind));
			y += ind + 2;
			iDrawAreas.push_back(new DrawArea_Marker("MarkerTop",     *it, cx/2 - mark_dx/2, y, mark_dx, mark_dy, 0));
			y += mark_dy;
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
			iDrawAreas.push_back(new DrawArea_Marker("MarkerBottom",  *it, cx/2 - mark_dx/2, y, mark_dx, mark_dy, 1));
			menulinenums = 7; // TODO Automatic calculation of the number of rows
		}else{
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
			iDrawAreas.push_back(new DrawArea_Marker("MarkerTop",     *it, cx/2 - mark_dx/2, y, mark_dx, mark_dy, 0));
			y += mark_dy;
			iDrawAreas.push_back(new DrawArea_STR("MenuList1", white, *it, x, y, cx - 2 * x, med, false));
			y += med ;
			iDrawAreas.push_back(new DrawArea_STR("MenuList2", white, *it, x, y, cx - 2 * x, med, false));
			y += med ;
			iDrawAreas.push_back(new DrawArea_STR("MenuList3", white, *it, x, y, cx - 2 * x, med, false));
			y += med + 2;
			iDrawAreas.push_back(new DrawArea_Marker("MarkerBottom",  *it, cx/2 - mark_dx/2, y, mark_dx, mark_dy, 1));
			menulinenums = 3; // TODO Automatic calculation of the number of rows
		}

		return menulinenums;
	}

	static void SetupLayout_SongInfoCA(std::vector<DrawAreaIF *> &iDrawAreas, DisplayIF *it)
	{
		int x, y, csz, big, med, sml, ind;
		int cx = it->GetSize().width;
		int cy = it->GetSize().height;

		if (cx == cy)
		{
			iDrawAreas.push_back(new DrawArea_CoverImage(*it, 0, 0, cx, cy, false));
		}else if (cx > cy){
			iDrawAreas.push_back(new DrawArea_CoverImage(*it, (cx-cy) / 2, 0, cy, cy, false));
		}else{
			iDrawAreas.push_back(new DrawArea_CoverImage(*it, 0, (cy-cx) / 2, cx, cx, false));
		}
	}

	void SetupButtons()
	{
		m_iGpioIntCtrl.RegistPin(
			GPIO_BUTTON_PREV,
			[&](int value) {
				if (0 == value)
				{
					printf("OnButtonPrev_Up()\n");

					if (m_isMenuMode)
					{
						m_iMenucurrent->index--;
						if (m_iMenucurrent->index < -1)
							m_iMenucurrent->index = -1;
						selectMenulist();
					}
					else if (m_isVolumeCtrlMode == 0)
					{
						m_iPrevGetStatusTime = std::chrono::system_clock::now();
						MusicController::PlayPrevSong();
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
				if (0 == value)
				{
					printf("OnButtonNext_Up()\n");

					if (m_isMenuMode)
					{
						m_iMenucurrent->index++;
						if (m_iMenucurrent->index >= m_iMenucurrent->menulists.size())
							m_iMenucurrent->index = m_iMenucurrent->menulists.size() - 1;
						selectMenulist();
					}
					else if (!m_isVolumeCtrlMode)
					{
						m_iPrevGetStatusTime = std::chrono::system_clock::now();
						MusicController::PlayNextSong();
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
				if (0 == value)
				{
					printf("OnButtonPlay_Up()\n");
					if (m_isButtonPlayPressed)
					{
						m_isButtonPlayPressed = false;
						if (m_isCoverArt){
							m_isCoverArt = false;
						}else
						if (m_isMenuMode)
						{
							enterMenulist();
						}
						else
						{
							MusicController::PlayPause();
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

	std::map<std::string, std::string> iInfo;
	std::chrono::system_clock::time_point m_iPrevGetStatusTime;

	// menu
	MenuController *m_iMenuroot;
	MenuController *m_iMenucurrent;
	int m_iMenuMaxlists;
};

#ifdef ENABLE_GETOPT
void printusage(void)
{
	std::cout << "-s <volumio server>" << std::endl;
}
#endif

int main(int argc, char** argv)
{
	MpdGui iMPD;

#ifdef ENABLE_GETOPT
	const char* const shortopts = "s:";

	while(1) {
		const auto opt = getopt(argc, argv, shortopts);
		if ( -1 == opt )	break;

		switch (opt) {
		case 's':
			m_volumioHost = std::string(optarg);
			std::cout << "Host name override: " << m_volumioHost << std::endl;
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
