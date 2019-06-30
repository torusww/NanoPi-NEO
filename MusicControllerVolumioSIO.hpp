#ifndef MUSICCONTROLLERVOLUMIOSIO
#define MUSICCONTROLLERVOLUMIOSIO

#include "MusicController.hpp"
#include "sio_client.h"
#include <thread>


const int m_iPollTime = 50;
const int m_iElapseUpdateTime = 1000;

class MusicControllerVolumioSIO : public MusicController
{
private:
	sio::client m_v2sio;
	int m_iPortImage;
	std::string m_nAlbumart;
	std::string m_nHostname;

	std::thread m_iThread;
	std::chrono::system_clock::time_point m_iPrevPollTime;

public:
	MusicControllerVolumioSIO();
	int connect();
	int disconnect();
	int toggle();
	int next();
	int prev();
	int volume(int);
	int volumeoffset(int);

	int getPlayPos();
	int setPlayQueue(int index);
	int setPlayList (int index);

	MenuList *getMenu(MenuController &iCtl);

	bool m_isConnected;
	void pollEvent();

private:
	int callMethod(std::string endpoint, std::string method, sio::message::ptr data);
	int callMethod(const char* endpoint, const char* method, sio::message::ptr data);
};

#endif

