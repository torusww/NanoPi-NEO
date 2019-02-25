#include "MusicController.hpp"
#include "MusicControllerVolumioSIO.hpp"
#define MUSIC_ROOT_PATH "/mnt/"
#include <iostream>
#include <thread>

#include "common/ctrl_http_curl.h"


MusicControllerVolumioSIO::MusicControllerVolumioSIO(): MusicController()
{
	m_iPort = 3000;
	m_iPortImage = 3001;

	m_isConnected = false;

	// override capability
	m_hasQueue = true;
	m_hasPlaylists = true;
}

static void ThreadProc(MusicControllerVolumioSIO * piThis)
{
	while(piThis->m_isConnected)
	{
		piThis->pollEvent();
	}
}

int MusicControllerVolumioSIO::connect()
{
	m_iState.clear();
	// register callback
	m_v2sio.socket()->on("pushState", [&](sio::event &ev) {
		std::cout << ev.get_name() << std::endl;
		sio::message::ptr msg = ev.get_message();

		std::lock_guard<std::mutex> lock(m_mtx);
		m_iPrevPollTime = std::chrono::system_clock::now();
		m_iState.clear();
		/* parse JSON */
		for (auto it : msg->get_map())
		{
			switch (it.second->get_flag())
			{
			case sio::message::flag_integer:
				m_iState[it.first] = std::to_string(it.second->get_int());
				break;
			case sio::message::flag_string:
				m_iState[it.first] = it.second->get_string();
				break;
			case sio::message::flag_boolean:
				m_iState[it.first] = it.second->get_bool() ? "true" : "false";
				break;
			case sio::message::flag_double:
				m_iState[it.first] = std::to_string(it.second->get_double());
				break;
			}
		}
		/* map to mpd status */
		m_iState["state"] = m_iState["status"].empty() ? "" : m_iState["status"];
		m_iState["Title"] = m_iState["title"].empty() ? "" : m_iState["title"];
		m_iState["Album"] = m_iState["album"].empty() ? "" : m_iState["album"];
		m_iState["Artist"] = m_iState["artist"].empty() ? "" : m_iState["artist"];
		if (!m_iState["duration"].empty()) m_iState["Time"] = std::to_string(std::stoi(m_iState["duration"]) * 1000);
		if (!m_iState["seek"].empty()) m_iState["elapsed"] = m_iState["seek"];
		if (!m_iState["uri"].empty())
		{ // normal mode
			m_iState["file"] = m_iState["uri"];
		}
		else
		{ // airplay mode
			m_iState["file"] = m_iState["Title"] + m_iState["Album"] + m_iState["Artist"];
		}
		m_iState["connected"] = "connected";
		m_iState["hostname"]  = m_nHostname;
		m_isUpdateState = true;
//		for (auto it : m_iState)	std::cout << it.first << " : " << it.second << std::endl;
	});

	m_v2sio.socket()->on("pushQueue", [&](sio::event &ev) {
		std::cout << ev.get_name() << std::endl;
		sio::message::ptr msg = ev.get_message();

		std::lock_guard<std::mutex> lock(m_mtx);
		m_iQueue.clear();
		/* parse JSON */
		for (auto itv : msg->get_vector())
		{
			std::map<std::string, std::string> iQueue;
			for (auto itm : itv->get_map())
			{
				switch (itm.second->get_flag())
				{
				case sio::message::flag_integer:
					iQueue[itm.first] = std::to_string(itm.second->get_int());
					break;
				case sio::message::flag_string:
					iQueue[itm.first] = itm.second->get_string();
					break;
				case sio::message::flag_boolean:
					iQueue[itm.first] = itm.second->get_bool() ? "true" : "false";
					break;
				case sio::message::flag_double:
					iQueue[itm.first] = std::to_string(itm.second->get_double());
					break;
				}
			}
			m_iQueue.push_back(iQueue);
		}
		m_isUpdateQueue = true;
	});

	m_v2sio.socket()->on("pushListPlaylist", [&](sio::event &ev) {
		std::cout << ev.get_name() << std::endl;
		sio::message::ptr msg = ev.get_message();

		std::lock_guard<std::mutex> lock(m_mtx);
		m_iPlaylist.clear();
		/* parse JSON */
		for (auto it : msg->get_vector())
		{
			switch (it->get_flag())
			{
			case sio::message::flag_string:
				m_iPlaylist.push_back(it->get_string());
				break;
			default:
				std::cout << ev.get_name() << ": parse error" << std::endl;
				break;
			}
		}
		m_isUpdatePlaylists = true;
//		for (auto it : m_iPlaylist)	std::cout << it << std::endl;
	});

	m_v2sio.socket()->on("pushDeviceInfo", [&](sio::event &ev) {
		std::cout << ev.get_name() << std::endl;
		sio::message::ptr msg = ev.get_message();

		/* parse JSON */
		m_nHostname = msg->get_map().count("name") ? msg->get_map()["name"]->get_string() : "";
	});

	m_v2sio.set_socket_open_listener([&](std::string const &nsp) {
		std::cout << "socket open" << std::endl;
		m_v2sio.socket()->emit("getDeviceInfo");
		m_v2sio.socket()->emit("getQueue");
		m_v2sio.socket()->emit("listPlaylist");
		m_v2sio.socket()->emit("getState");
	});

	m_v2sio.set_reconnect_listener([&](unsigned int retry, unsigned int retrywait) {
		std::lock_guard<std::mutex> lock(m_mtx);
		if (retry > 0)
		{
			m_iState.clear();
		}
		m_iState["connected"] = "connecting...";
		m_isUpdateState = true;
	});

	m_v2sio.set_socket_close_listener([&](std::string const &nsp) {
		{
			std::cout << "socket close" << std::endl;
			std::lock_guard<std::mutex> lock(m_mtx);
			m_iState.clear();
			m_iState["connected"] = "shutdown...";
			m_isUpdateState = true;
		}
	});
#if 0
    m_v2sio.set_open_listener([&]()
    {
            std::cout << "open" << std::endl;
    });

    m_v2sio.set_fail_listener([&]()
    {
            std::cout << "fail" << std::endl;
    });

    m_v2sio.set_reconnecting_listener([&]()
    {
            std::cout << "reconnecting" << std::endl;
    });

    m_v2sio.set_close_listener([&](sio::client::close_reason const& reason)
    {
            std::cout << "close: " << reason << std::endl;
    });
#endif

	// connect
	std::lock_guard<std::mutex> lock(m_mtx);
	m_iState["connected"] = "connecting...";
	m_isUpdateState = true;

	std::string uri = "http://" + m_iHost + ":" + std::to_string(m_iPort);
	m_v2sio.connect(uri);
	m_isConnected = true;
	m_iThread = std::thread(ThreadProc,this);
}

int MusicControllerVolumioSIO::disconnect()
{
	m_isConnected = false;
	m_iThread.join();
	m_v2sio.close();
	return 0;
}

int MusicControllerVolumioSIO::toggle()
{
	m_v2sio.socket()->emit("toggle");
	return 0;
}

int MusicControllerVolumioSIO::next()
{
	m_v2sio.socket()->emit("next");
	return 0;
}

int MusicControllerVolumioSIO::prev()
{
	m_v2sio.socket()->emit("prev");
	return 0;
}

int MusicControllerVolumioSIO::volume(int vol)
{
	std::lock_guard<std::mutex> lock(m_mtx);
	m_v2sio.socket()->emit("volume", sio::int_message::create(vol));
	return vol;
}

int MusicControllerVolumioSIO::volumeoffset(int offset)
{
	int value;
	{
		std::lock_guard<std::mutex> lock(m_mtx);
		if (m_iState["volume"].empty()) return -1;
		value = std::stoi(m_iState["volume"]) + offset;
		value = 0 <= value ? value <= 100 ? value : 100 : 0;
		m_v2sio.socket()->emit("volume", sio::int_message::create(value));
		m_iState["volume"] = std::to_string(value);
	}
	return value;
}

int MusicControllerVolumioSIO::getPlayPos()
{
	std::lock_guard<std::mutex> lock(m_mtx);
	if (m_iState["position"].empty()) {
		return -1;
	}
	return std::stoi(m_iState["position"]);
}

int MusicControllerVolumioSIO::setPlayQueue(int index)
{
	std::lock_guard<std::mutex> lock(m_mtx);
	// setup parameter
	sio::message::ptr send_data(sio::object_message::create());
	std::map<std::string, sio::message::ptr>& map = send_data->get_map();
	map.insert(std::make_pair("value", sio::int_message::create(index)));
	// emit
	m_v2sio.socket()->emit("play", send_data);
	// update local info
	m_iState["position"] = std::to_string(index);
	return 0;
}
int MusicControllerVolumioSIO::setPlayList (int index)
{
	std::lock_guard<std::mutex> lock(m_mtx);
	// setup parameter
	sio::message::ptr send_data(sio::object_message::create());
	std::map<std::string, sio::message::ptr>& map = send_data->get_map();
	map.insert(std::make_pair("name", sio::string_message::create(m_iPlaylist[index])));
	// emit
	m_v2sio.socket()->emit("playPlaylist", send_data);
	return 0;
}

void MusicControllerVolumioSIO::pollEvent()
{
	std::this_thread::sleep_for(std::chrono::milliseconds(m_iPollTime));
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

	// update elapsed value
	static std::chrono::system_clock::time_point el_time;
	if (std::chrono::duration_cast<std::chrono::milliseconds>(now - el_time).count() > m_iElapseUpdateTime){
		std::lock_guard<std::mutex> lock(m_mtx);
		el_time = now;
		if (!m_iState["state"].empty() && m_iState["state"] == "play") {
			int elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
													now - m_iPrevPollTime).count();

			elapsed += std::stoi(m_iState["seek"]);
			m_iState["elapsed"] = std::to_string(elapsed);
			m_isUpdateState = true;
		}
	}

	// get coverart
	if ( m_nAlbumart != m_iState["albumart"] ) {
		std::vector<unsigned char> iData;
		std::string url = m_iState["albumart"];
		if (!HttpCurl::Get(m_iHost.c_str(), m_iPortImage, url.c_str(), iData))
		{
			std::lock_guard<std::mutex> lock(m_mtx);
			std::cout << "getAlbumart" << " : " << iData.size() << std::endl;
			m_iCoverart.clear();
			m_iCoverart = iData;
			m_nAlbumart = url;
			m_isUpdateCoverart = true;
		}
		else
		{
			m_nAlbumart = "";
		}
	}
}
