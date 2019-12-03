#ifndef __DISPALY_BASE_INCLUDED__
#define __DISPALY_BASE_INCLUDED__

#include <iconv.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>

#include <stdio.h>
#include <stdint.h>

int code_convert(const char *from_charset, const char *to_charset, char *inbuf,
                size_t inlen, char *outbuf, size_t outlen)
{
        iconv_t cd;
        char **pin = &inbuf;
        char **pout = &outbuf;

        cd = iconv_open(to_charset, from_charset);
        if (cd == 0)
        {
                std::cout<<"can't open iconv...........\n";
                return -1;
        }
        memset(outbuf, 0, outlen);
        if (iconv(cd, pin, &inlen, pout, &outlen) != 0)
        {
                std::cout<<"can't iconv...........\n";
                return -1;
        }
        iconv_close(cd);
        return 0;
}

int u2g(char *inbuf, int inlen, char *outbuf, int outlen)
{
        return code_convert("utf-8", "gb18030", inbuf, inlen, outbuf, outlen);
}

int g2u(char *inbuf, size_t inlen, char *outbuf, size_t outlen)
{
        return code_convert("gb2312", "utf-8", inbuf, inlen, outbuf, outlen);
}


class DisplayIF
{
public:
	class DispSize
	{
	public:
		DispSize()
		{
			width	= 320;
			height	= 240;
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
	
	int Init(){
                fd = open("/dev/ttyS2", O_RDWR); /* connect to port */
                if(fd==-1){
                        perror("can't open serail port\n");
                        return 1;
                }

                struct termios settings;
                tcgetattr(fd, &settings);

                cfsetospeed(&settings, B115200);
                settings.c_cflag &= ~PARENB; /* no parity */
                settings.c_cflag &= ~CSTOPB; /* 1 stop bit */
                settings.c_cflag &= ~CSIZE;
                settings.c_cflag |= CS8 | CLOCAL; /* 8 bits */
                settings.c_lflag = ICANON; /* canonical mode */
                settings.c_oflag &= ~OPOST; /* raw output */

                tcsetattr(fd, TCSANOW, &settings); /* apply the settings */
                tcflush(fd, TCOFLUSH);
		return 0;
	}
	int DispClear(){
		m_str="CLS(0);";
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

	void WriteString( int x, int y,int color,const std::string& str){
		m_str+="DS16("+std::to_string(x)+","+std::to_string(y)+",'"+str+"',"+std::to_string(color)+");";
		//std::cout << ">>>"<< x <<" "<< y<<" " << str <<std::endl;
	}
	void WritePos(int y,float pos){
		int tp = pos*400;
		std::string tps = std::to_string(tp);
		std::string ys = std::to_string(y);
		std::string yys = std::to_string(y+14);
		m_str+="BOXF(0,"+ys+","+tps+","+yys+",7);";
		m_str+="BOXF("+tps+","+ys+",399,"+yys+",8);";
	}
	
	void Flush()
	{
		if(m_str.size()>0){
			m_str+="\r\n";

			char inbuff[4096];
                        char strbuff[4096];
                        strcpy(inbuff,m_str.c_str());
                        u2g(inbuff,strlen(inbuff),strbuff,sizeof(strbuff));
                        write(fd,strbuff,strlen(strbuff));
			//write(fd,m_str.c_str(),m_str.size());
			std::cout << m_str << std::endl;
			m_str = "";
		}
	}

	const DispSize&    GetSize()
	{
		return  m_tDispSize;
	}
	

protected:
	int fd;
	DispSize     m_tDispSize;
	std::string  m_str;
};

#endif
