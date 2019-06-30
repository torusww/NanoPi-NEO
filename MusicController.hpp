#ifndef MUSICCONTROLLER
#define MUSICCONTROLLER
#include <mutex>
#include <string>
#include <vector>
#include <map>
#include <iostream> // cout

#include "MenuController.hpp"

class MusicController
{
public:
	/* constructer*/
	MusicController() {
		m_isUpdateState = false;
		m_isUpdateQueue = false;
		m_isUpdatePlaylists = false;
		m_isUpdateCoverart = false;
		m_hasQueue = false;
		m_hasPlaylists = false;
		m_iHost = "127.0.0.1";
		m_iPort = 6000;
	}

	/* session control */
	virtual int sethost(std::string &iHost) { m_iHost = iHost; return 0; };
	virtual int setport(int iPort )    { m_iPort = iPort; return 0; };
	virtual int connect() { return 0; };
	virtual int disconnect() { return 0; };

	/* music control */
	virtual int play() { return 0; };
	virtual int stop() { return 0; };
	virtual int toggle() { return 0; };
	virtual int next() { return 0; };
	virtual int prev() { return 0; };

	virtual int fastfoward(int msec) { return 0; };
	virtual int rewind(int msec) { return 0; };
	virtual int volume(int) { return 0; };
	virtual int volumeoffset(int) { return 0; };

	/* state i/f */
	bool isUpdateState() { return m_isUpdateState; }
	virtual void getState(std::map<std::string, std::string>& iState) {
		std::lock_guard<std::mutex> lock(m_mtx);
		iState.clear();
		iState = m_iState;
		m_isUpdateState = false;
	}

	bool isUpdateCoverart() { return m_isUpdateCoverart; }
	virtual int getCoverart(std::vector<unsigned char>& iData) {
		std::lock_guard<std::mutex> lock(m_mtx);
		iData.clear();
		iData = m_iCoverart;
		m_isUpdateCoverart = false;
		return 0;
	}

	/* queue control */
	bool isUpdateQueue() { return m_isUpdateQueue; }
	virtual void getQueue(std::vector<std::map<std::string, std::string>>& iQueue) {
		std::lock_guard<std::mutex> lock(m_mtx);
		iQueue.clear();
		iQueue = m_iQueue;
		m_isUpdateQueue = false;
	}

	bool isUpdatePlayLists() { return m_isUpdatePlaylists; }
	virtual void getPlayLists(std::vector<std::string>& iList){
		std::lock_guard<std::mutex> lock(m_mtx);
		iList.clear();
		iList = m_iPlaylist;
		m_isUpdatePlaylists = false;
	}

	virtual int getPlayPos() { return -1; };
	virtual int setPlayQueue(int index) { return 0; };
	virtual int setPlayList (int index) { return 0; };

	/* capability info */
	bool hasQueue() { return m_hasQueue; }
	bool hasPlaylists(){ return m_hasPlaylists; }

	/* Menu i/f */
	virtual MenuList *getMenu(MenuController &iCtl) {return nullptr;};

protected:
	// host/port
	std::string m_iHost;
	int         m_iPort;
	// data
	std::map<std::string, std::string> m_iState;
	std::vector<std::map<std::string, std::string>> m_iQueue;
	std::vector<std::string> m_iPlaylist;
	std::vector<unsigned char> m_iCoverart;
	std::mutex m_mtx;

	// flags
	bool m_isUpdateState;
	bool m_isUpdateQueue;
	bool m_isUpdatePlaylists;
	bool m_isUpdateCoverart;

	bool m_hasQueue;
	bool m_hasPlaylists;
};
#endif
