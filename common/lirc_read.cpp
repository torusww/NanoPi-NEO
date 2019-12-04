#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> //read
#include <fcntl.h> //--O_RDONLY,open
#include <sys/ioctl.h> //ioctl
#include <stdint.h>
#include <string.h>
#include <errno.h>
//#include <lirc.h>

#define LIRC_DAT_PAUSE 0xc2
#define LIRC_DAT_NEXT 0x02
#define LIRC_DAT_PREV  0x22
#define LIRC_DAT_PLUS 0xa8
#define LIRC_DAT_MINUS 0xe0
#define LIRC_DAT_CH 0x62
#define LIRC_DAT_CH_MINUS 0xa2
#define LIRC_DAT_CH_PLUS 0xe2

uint8_t getLircData(const uint32_t* lirc_buf)
{
	int i;
	uint32_t lirc_codes=0;//--decoded lirc code  [userID][userID'][DATA][~DATA]
	uint8_t lirc_data=0;  //--- received lirc DATA =lirce_codes[15:8]

	for(i=0;i<32;i++) //--read [8bit_UserID][8bit_UserID2][8bit_DATA][8bit_~DATA]
	{
		if( *(lirc_buf+2*i+3+1) > 1200 )
			lirc_codes+=(1<<(31-i)); //--check if it's 1.
	}
	//printf("----- LIRC CODEs: 0x%08x -----\n",lirc_codes);

	//--extract LIRC DATA ---
	lirc_data=(lirc_codes&(0xff00))>>8;
	if( lirc_data+(lirc_codes&(0xff)) == 0xFF)//---complement check  
		printf("----- LIRC DATA: 0x%02x -----\n",lirc_data);
	else
	{
		printf("----- LIRC DATA: complement check fails! -----\n");
		lirc_data=0;
	}

	return lirc_data;
}



void main(void)
{
  //--------- for LIRC dev ---
  char *dev_lirc="/dev/lirc0";
  int fd_lirc;// file handle to lirc dev.
  int vol=15; //0-31 volume
  uint32_t lirc_buf[100]={0}; //--4bytesx68=272bytes,  for raw lirc data from hardware IR receiver
  uint8_t  lirc_data; //--- received lirc DATA =lirce_codes[15:8]



  int ret;
  int param;
  int i,j;


   //--------open lirc device ------
  if((fd_lirc=open(dev_lirc, O_RDWR)) <0 )
  {
	perror("Open LIRC dev");
	close(fd_lirc);
	exit(-1);
   }
  else
	printf("Open %s successfully!\n",dev_lirc);

/*

   //------  read some defaults ------
  ioctl(fd_lirc,LIRC_GET_FEATURES,&param);
  printf("LIRC_GET_FEATURES: 0x%08x\n",param);

  ioctl(fd_lirc,LIRC_GET_LENGTH,&param);
  printf("LIRC_GET_LENGTH: 0x%08x\n",param);

  ioctl(fd_lirc,LIRC_GET_MIN_TIMEOUT,&param);
  printf("LIRC_GET_MIN_TIMEOUT: %d ms\n",param);

  ioctl(fd_lirc,LIRC_GET_MAX_TIMEOUT,&param);
  printf("LIRC_GET_MAX_TIMEOUT: %d ms\n",param);
*/

  //---------------- loop reading LIRC data  -------------------
  while(1)
  {
    ret=read(fd_lirc,lirc_buf,sizeof(lirc_buf));
    printf("ret=%d\n",ret);

	int m= ret/4;
        for(i=0;i<m;i++){
		uint32_t t = lirc_buf[i];
		if(t&(1<<24))printf("+");
		else printf("_");
		//printf("%x ",t);
		printf("%x ",(t&0xffffff));
 		//printf("%d ",lirc_buf[2*i+4]);
	}
        printf("\n");

    if(ret>270)
    //if(ret>270 && lirc_buf[0]>0x20000) //if receive complete signal && preamble signal long enough
    {

	//---preamble---
	//printf("---preamble: 0x%08x\n",lirc_buf[0]);

	lirc_data=getLircData(lirc_buf);
	printf("---lircdata: 0x%08x\n",lirc_data);

    }
    else{
/*
	ret = ret/4;
        for(i=0;i<ret;i++){
 		printf("%08x ",lirc_buf[i]);
	}
       printf("\n");
*/
       usleep(200000);
    }
    //else usleep(200000);

   }

 close(fd_lirc);

}
 

