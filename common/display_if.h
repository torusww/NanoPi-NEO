#ifndef __DISPALY_BASE_INCLUDED__
#define __DISPALY_BASE_INCLUDED__

#include <stdio.h>
#include <stdint.h>


class DisplayIF
{
public:
	class DispSize
	{
	public:
		DispSize()
		{
			width	= 0;
			height	= 0;
		}

		DispSize( int w, int h )
		{
			width	= w;
			height	= h;
		};

	public:
		uint16_t    width;
		uint16_t    height;
	};

public:
	DisplayIF(){};
	virtual ~DisplayIF(){};
	
	int Init(){
		return 0；
	}
	int DispClear(){
		return 0;
	}
	int DispOn(){
		return 0;
	}
	int DispOff(){
		return 0;
	}
	int Quit(){
		return 0;
	}

	int WriteString( int x, int y, const string& str, int color ){

	}
	
	void Flush()
	{
	}

	const DispSize&    GetSize()
	{
		return  m_tDispSize;
	}
	
	virtual	int			GetBPP()=0;

protected:


protected:
	DispSize     m_tDispSize;
};

#endif