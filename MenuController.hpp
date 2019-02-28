#ifndef MENUCONTROLLER
#define MENUCONTROLLER
#include <string.h>
#include <unistd.h>
#include <mutex>
#include <memory>
#include <string>
#include <vector>
#include <map>

class MusicController;
class MenuController;

class MenuList
{
private:
protected:
	std::string name;
	std::vector<class MenuList *> m_iMenulist; // submenu
	MenuController &m_iCtl;

	int addmenu(MenuList *iMenu)
	{
		m_iMenulist.resize(menulists.size()+1);
		m_iMenulist.insert(m_iMenulist.begin() + menulists.size(), iMenu);
		menulists.push_back(iMenu->getname());
		iMenu->prev = this;
	}

public:
	MenuList(const char* iName, MenuController &iCtl): m_iCtl(iCtl) {
		name = iName;
		index = 0;
		prev = NULL;
	};

	std::vector<std::string> menulists;
	int index;
	MenuList *prev; // parent

	virtual int enter() { return 0; }
	virtual int refresh() { return 0; }
	virtual MenuList *select(void) { return this; }
	virtual MenuList *exec(void) { return prev; }
	std::string getname() { return name; }
};


class MenuController
{
private:
	MenuList *m_iCurrent;
	MenuList *m_iRoot;
	std::map<std::string, std::string> m_iMenulist;
	int m_Index;
	int m_iMaxRows;

	bool m_isUpdateMenu;
	bool m_isMenuMode;

	std::mutex m_mtx;

public:
	MenuController(MusicController &iMusicCtl, int maxrows);

	// for draw i/f
	bool isUpdateMenu() { return m_isUpdateMenu; }
	int getMenulists(std::map<std::string, std::string> &iLists) {
		std::lock_guard<std::mutex> lock(m_mtx);
		iLists.clear();
		iLists = m_iMenulist;
		m_isUpdateMenu = false;
	}
	int getIndex() { return m_Index; }

	// for control i/f
	bool isMenuMode() { return m_isMenuMode; }
	int open();
	int close();
	int next();
	int prev();
	int select();
	int exec();

	// for parameter
	void setMaxRows(int num) { m_iMaxRows = num;}
	int on(std::function<void(std::map<std::string, std::string> &)> const &func) {
		m_nMapCB.push_back(func);
	}
	int on(std::string const &key, std::function<void(std::string &)> const &func) {
		m_nStringCB[key] = func;
	}

public:
	MusicController &m_iMusicCtl;
	int emit(std::map<std::string, std::string> &);
	int emit(std::string key, std::string value);

private:
	void createMenulist();
	std::vector<std::function<void(std::map<std::string, std::string> &)>> m_nMapCB;
	std::map<std::string, std::function<void(std::string &)>> m_nStringCB;

};
#endif
