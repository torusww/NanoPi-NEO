
#include <vector>
#include <thread>
#include "common/ctrl_spi.h"
//#include "common/ctrl_gpio.h"
#include "common/ctrl_gpio_poll.h"


#include "common/display_if.h"	// Conflict to fbdev.h. Use exclusive.

std::vector<DisplayIF*>	GetUsrDisplays()
{
	std::vector<DisplayIF*>	iDisplays;

	iDisplays.push_back( new DisplayIF() );
	return	iDisplays;
}

