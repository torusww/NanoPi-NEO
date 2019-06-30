#include "MenuController.hpp"
#include "MusicController.hpp"
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/stat.h>

class MenuQueuelist : public MenuList
{
private:
	bool isForceUpdate;
	std::vector<std::map<std::string, std::string>> m_iQueue;

public:
	MenuQueuelist(const char* iName, MenuController &iCtl ) : MenuList(iName, iCtl)
	{
		isForceUpdate = true;
	}
	int enter()
	{
		if (isForceUpdate || m_iCtl.m_iMusicCtl.isUpdateQueue())
		{
			isForceUpdate = false;
			menulists.clear();
			m_iCtl.m_iMusicCtl.getQueue(m_iQueue);
			for ( auto itq: m_iQueue)
			{
				menulists.push_back(std::to_string(menulists.size() + 1) + ". " +
														itq["name"] + " / " + itq["album"] + " / " + itq["artist"]);

			}
		}
		index = m_iCtl.m_iMusicCtl.getPlayPos();
	}

	MenuList *exec()
	{
		if (index < 0 || index >= menulists.size())
		{
			return nullptr;
		}
		m_iCtl.m_iMusicCtl.setPlayQueue(index);
		return this; // stay menu
	}
};

class MenuPlaylist : public MenuList
{

public:
	using MenuList::MenuList;
	int enter()
	{
		menulists.clear();
		m_iCtl.m_iMusicCtl.getPlayLists(menulists);
	}

	MenuList *exec()
	{
		if (index < 0 || index >= menulists.size())
		{
			return nullptr;
		}
		m_iCtl.m_iMusicCtl.setPlayList(index);
		return nullptr; // close menu
	}
};

class MenuAlsamixerDetail : public MenuList
{
protected:
	int numid;

public:
	MenuAlsamixerDetail(const char* iName, MenuController &iCtl, int _numid, std::vector<std::string> &lists, int _index) : 
		MenuList(iName, iCtl), numid(_numid)
	{
		menulists = lists;
		index = _index;
	}

	MenuList *select()
	{
		if (index >= 0 && index < menulists.size())
		{
			std::string cmd = "amixer cset numid=" + std::to_string(numid) + " " + std::to_string(index);
			std::system(cmd.c_str());
		}
		return this; // stay menu
	}

	MenuList *exec()
	{
		return nullptr; // close menu
	}
};

class MenuAlsamixer : public MenuList
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
	using MenuList::MenuList;

	int enter()
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
					addmenu(new MenuAlsamixerDetail(mixer_name.c_str(), m_iCtl, std::stoi(numid), lists, std::stoi(values)));
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

	MenuList *exec()
	{
		if (index < 0 || index >= menulists.size())
		{
			return nullptr;
		}
		return m_iMenulist[index];
	}
};

#ifdef DTBSELECTION
class MenuDTBSelection : public MenuList
{
private:
	std::vector<std::string> dtblists;
public:
	MenuDTBSelection(const char* iName, MenuController &iCtl ) : MenuList(iName, iCtl)
	{
		std::ifstream rfs;
		std::string rbuff;
		rfs.open(DTBSELECTION_LIST, std::ios::in);
		while (getline(rfs,rbuff)) {
			std::string dtbname, dtbfile;
			std::istringstream sep(rbuff);
			getline(sep, dtbname, '\t');
			if (dtbname[0] == '#') continue; // comment line
			getline(sep, dtbfile, '\t');
			if (!dtbname.empty() && !dtbfile.empty()){
				menulists.push_back(dtbname);
				dtblists.push_back(dtbfile);
			}
		}
	}

	MenuList *exec()
	{
		if (index < 0 || index >= menulists.size())
		{
			return nullptr;
		}
		std::string cmd = "cp /boot/" + dtblists[index] + " /boot/" + DTBSELECTION_COPYTO;
		std::system(cmd.c_str());
		return nullptr;
	}
};
#endif

#ifdef GUISELECTION
class MenuGUISelection : public MenuList
{
private:
	std::vector<std::string> guilists;
public:
	MenuGUISelection(const char* iName, MenuController &iCtl ) : MenuList(iName, iCtl)
	{
		std::ifstream rfs;
		std::string rbuff;
		rfs.open(GUISELECTION_LIST, std::ios::in);
		while (getline(rfs,rbuff)) {
			std::string guiname, guifile;
			std::istringstream sep(rbuff);
			getline(sep, guiname, '\t');
			if (guiname[0] == '#') continue; // comment line
			getline(sep, guifile, '\t');
			if (!guiname.empty() && !guifile.empty()){
				menulists.push_back(guiname);
				guilists.push_back(guifile);
			}
		}
	}

	MenuList *exec()
	{
		if (index < 0 || index >= menulists.size())
		{
			return nullptr;
		}
		std::string cmd = "rm /boot/gui_*; touch /boot/gui_" + guilists[index];
		std::system(cmd.c_str());
		return nullptr;
	}
};
#endif

#define LCDSLEEPTIME_FILE "/boot/lcd_sleeptime"
class MenuLCDSleepTime : public MenuList
{
private:
	int iLcdSleepTime;
	int getSleepTime()
	{
		int ret = 0;
		std::ifstream rfs;
		std::string rbuff;
		rfs.open(LCDSLEEPTIME_FILE, std::ios::in);
		getline(rfs,rbuff);
		if ( !rbuff.empty()) {
			ret = std::stoi(rbuff);
		}
		return ret;
	}
public:
	MenuLCDSleepTime(const char* iName, MenuController &iCtl ) : MenuList(iName, iCtl)
	{
		menulists.push_back("OFF");
		for ( int i = 1; i <= 6; i++) {
			std::string listname = std::to_string(i * 10) + " Sec";
			menulists.push_back(listname);
		}
		iLcdSleepTime = getSleepTime();

		// Setup Callback
		m_iCtl.on("lcdsleeptime_update", [&](std::string &value)
		{
			std::cout << "Read Lcd Sleep Time= " << iLcdSleepTime << " Sec" << std::endl;
			m_iCtl.emit("lcdsleeptime", std::to_string(iLcdSleepTime));
		});
	}

	int enter()
	{
		index = iLcdSleepTime / 10;
	}

	MenuList *exec()
	{
		if (index < 0 || index >= menulists.size())
		{
			return nullptr;
		}
		iLcdSleepTime = index * 10;
		m_iCtl.emit("lcdsleeptime", std::to_string(iLcdSleepTime));
		std::string cmd = "echo " + std::to_string(iLcdSleepTime) + " > " + LCDSLEEPTIME_FILE + "; sync";
		std::system(cmd.c_str());
		return nullptr;
	}
};

class MenuMiscellaneous : public MenuList
{
public:
	MenuMiscellaneous(const char* iName, MenuController &iCtl ) : MenuList(iName, iCtl)
	{
		menulists.push_back("CoverArt mode");
		menulists.push_back("Restart AirPlay");
		menulists.push_back("Restart Volumio");
		menulists.push_back("Restart MPD");

#if defined(DTBSELECTION) || defined(GUISELECTION)
		struct stat buffer;
#endif

#ifdef DTBSELECTION
		if (stat (DTBSELECTION_LIST, &buffer) == 0) {
	  		addmenu(new MenuDTBSelection("Select DTB", m_iCtl));
		}
#endif
#ifdef GUISELECTION
		if (stat (GUISELECTION_LIST, &buffer) == 0) {
			addmenu(new MenuGUISelection("Select GUI", m_iCtl));
		}
#endif
		addmenu(new MenuLCDSleepTime("LCD Sleep Time", m_iCtl));
	}

	MenuList *exec()
	{
		if (index < 0 || index >= menulists.size())
		{
			return nullptr;
		}
		switch (index)
		{
		case 0:
			m_iCtl.emit("coverart", "true");
			break;

		case 1:
			std::system("service airplay restart");
			break;

		case 2:
			std::system("service volumio restart");
			break;

		case 3:
			std::system("service mpd restart");
			break;
		default:
			return m_iMenulist[index];
			break;
		}
		return nullptr;
	}
};

class MenuQuit : public MenuList
{
protected:
	std::string m_iCmd;
public:
	MenuQuit(const char* iName, MenuController &iCtl, const char* iCmd) : 
		MenuList::MenuList(iName, iCtl)
	{
		m_iCmd = iCmd;
		menulists.push_back("Cancel");
		menulists.push_back("OK");
	}
	int enter()
	{
		index = 0;
	}

	MenuList *exec()
	{
		if (index == 1)
		{
			m_iCtl.m_iMusicCtl.disconnect();
			sleep(1);
			std::system(m_iCmd.c_str());
		}
		return nullptr; // leave menu
	}
};


class MenuPoweroff : public MenuList
{
private:
public:
	MenuPoweroff(const char* iName, MenuController &iCtl ) : MenuList(iName, iCtl)
	{
		addmenu(new MenuQuit("Reboot", m_iCtl, "reboot"));
		addmenu(new MenuQuit("Shutdown", m_iCtl, "shutdown -h now"));
	}
	int enter()
	{
		index = 0;
	}

	MenuList *exec()
	{
		if (index < 0 || index >= menulists.size())
		{
			return nullptr;
		}
		return m_iMenulist[index];
	}
};

class MenuMain : public MenuList
{
private:
public:
	MenuMain(const char* iName, MenuController &iCtl ) : MenuList(iName, iCtl)
	{
		if (m_iCtl.m_iMusicCtl.hasQueue())     addmenu(new MenuQueuelist("Queue", m_iCtl));
		if (m_iCtl.m_iMusicCtl.hasPlaylists()) addmenu(new MenuPlaylist("Playlists", m_iCtl));
		addmenu(new MenuAlsamixer("Alsa Mixer", m_iCtl));
		MenuList *MusicCtlMenu = m_iCtl.m_iMusicCtl.getMenu(m_iCtl);
		if (MusicCtlMenu != nullptr) addmenu(MusicCtlMenu);
		addmenu(new MenuMiscellaneous("Miscellaneous", m_iCtl));
		addmenu(new MenuPoweroff("PowerOff", m_iCtl));
	}

	MenuList *exec()
	{
		if (index < 0 || index >= menulists.size())
		{
			return nullptr;
		}
		return m_iMenulist[index];
	}
};


MenuController::MenuController(MusicController &iMusicCtl, int maxrow): m_iMusicCtl(iMusicCtl), m_iMaxRows(maxrow) {
	m_iRoot = new MenuMain("Main Menu", *this);
	m_Index = 0;

	m_isUpdateMenu = false;
	m_isMenuMode = false;
}



int MenuController::open()
{
	{
		std::lock_guard<std::mutex> lock(m_mtx);
		m_iMenulist.clear();
		m_iCurrent = m_iRoot; // set root menu
		m_Index = m_iCurrent->index = 0;
		m_isUpdateMenu = false;
		m_iCurrent->enter();
	}
	select();
	m_isMenuMode = true;
}

int MenuController::close()
{
	std::lock_guard<std::mutex> lock(m_mtx);
	m_iMenulist.clear();
	m_iCurrent = nullptr; // set root menu
	m_isUpdateMenu = false;
	m_isMenuMode = false;
}

int MenuController::select(void)
{
	if (m_iCurrent->index >= 0)
	{
		m_iCurrent = m_iCurrent->select();
	}
	if (m_iCurrent)
	{
		createMenulist();
	}
	else
	{
		std::lock_guard<std::mutex> lock(m_mtx);
		m_iMenulist.clear();
		m_isMenuMode = false;
		m_isUpdateMenu = true;
	}
}

int MenuController::next()
{
	m_iCurrent->index++;
	if (m_iCurrent->index >= m_iCurrent->menulists.size()) {
		m_iCurrent->index = m_iCurrent->menulists.size() - 1;
	}
	select();
}

int MenuController::prev()
{
	m_iCurrent->index--;
	if (m_iCurrent->index < -1) {
		m_iCurrent->index = - 1;
	}
	select();
}

int MenuController::exec()
{
	if (m_iCurrent->index >= 0)
	{
		m_iCurrent = m_iCurrent->exec();
	}
	else
	{
		m_iCurrent = m_iCurrent->prev;
	}
	if (m_iCurrent)
	{
		m_iCurrent->enter();
		createMenulist();
	}
	else
	{
		std::lock_guard<std::mutex> lock(m_mtx);
		m_iMenulist.clear();
		m_isMenuMode = false;
		m_isUpdateMenu = true;
	}
}






static std::string getMenuKey(int index) { return "MenuList" + std::to_string(index + 1); }
void MenuController::createMenulist()
{
	int index, startpos = 1;
	std::map<std::string, std::string> iInfo;
	std::lock_guard<std::mutex> lock(m_mtx);

	iInfo["MenuTitle"] = m_iCurrent->getname();
	iInfo["Time"] = std::to_string(100);
	iInfo["elapsed"] = std::to_string(100);

	// clear marker
	iInfo["MarkerTop"] = "";
	iInfo["MarkerBottom"] = "";

	// calc start index & check prev mark
	if ((m_iCurrent->menulists.size() >= m_iMaxRows) &&
			(m_iCurrent->index >= ((m_iMaxRows + 1) / 2) - 1))
	{
		iInfo["MarkerTop"] = "mark";
		startpos = 0;
		if ((m_iCurrent->menulists.size() - m_iCurrent->index) < (m_iMaxRows + 1) / 2)
		{
			// bottom
			index = m_iCurrent->menulists.size() - m_iMaxRows;
		}
		else
		{
			// middle
			index = m_iCurrent->index - ((m_iMaxRows + 1) / 2 - 1);
		}
	}
	else
	{
		// top
		iInfo[getMenuKey(0)] = "..";
		if (m_iCurrent->index == -1)
			iInfo[getMenuKey(0) + "_attr"] = "H";
		else
			iInfo[getMenuKey(0) + "_attr"] = "";
		index = 0;
	}

	// check next mark
	if ((m_iCurrent->index + m_iMaxRows - 1) < m_iCurrent->menulists.size())
	{
		iInfo["MarkerBottom"] = "mark";
	}

	// fill list
	for (int i = startpos; i < m_iMaxRows; i++, index++)
	{
		iInfo[getMenuKey(i)] = (index < m_iCurrent->menulists.size() ? m_iCurrent->menulists[index] : " ");
		if (m_iCurrent->index == index)
			iInfo[getMenuKey(i) + "_attr"] = "H";
		else
			iInfo[getMenuKey(i) + "_attr"] = "";
	}
	
	if (m_Index != m_iCurrent->index || m_iMenulist != iInfo) {
		m_iMenulist = iInfo;
		m_Index = m_iCurrent->index;
		m_isUpdateMenu = true;
//		for (auto it : m_iMenulist)	std::cout << it.first << " : " << it.second << std::endl;
	}
}
int MenuController::emit(std::map<std::string, std::string> &data)
{
	for (auto it: m_nMapCB)
		it(data);

	for (auto itk: data) 
		if (m_nStringCB.count(itk.first))
			m_nStringCB[itk.first](itk.second);

	return 0;
}
int MenuController::emit(std::string key, std::string value)
{
	if (m_nStringCB.count(key))
		m_nStringCB[key](value);

	std::map<std::string, std::string> data = {{key, value}};
	for (auto it: m_nMapCB)
		it(data);

	return 0;
}
