/***********************************************************
 * mlsVox.c
 *
 *  Created on: Nov 10, 2011
 *      Author: mlsdev008
 ***********************************************************/
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <pthread.h>			//thread
#include <errno.h>
#include <unistd.h> 			//usleep
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>			//socket
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/soundcard.h>
#include <net/if.h>


#include "mlsVox.h"
#include "mlsaudio.h"
#include "mlstrigger.h"
#include "../../debug.h"
//define default value
#ifdef PC
#define DSP	"/dev/dsp"
#define MIX	"/dev/mixer"
#else
#define DSP	"/dev/dsp1"
#define MIX	"/dev/mixer1"
#endif
#define AMOUNT		4000 //0.5 second
#define A_TIMES		2
#define TIME_FREE	3
#define THRESHOLD	-20
#define PORT		51109
//define for network
#ifndef PC
#define INTERFACE	"ra0"
#else
#define INTERFACE	"eth0"
#endif

//global value
vox vox_config = {DISABLE,THRESHOLD,VOX_UNKNOWN};
static pthread_t p_thread;
int flag_exit_vox = DISABLE;
//define function
extern mlsErrorCode_t _error(int dsp,int mix);
extern mlsErrorCode_t openAudioDevice(int *dsp,int *mix);
extern mlsErrorCode_t initialAudioDevice(int dsp,int mix);
extern mlsErrorCode_t getMacAddr(struct ifreq *ifr);

/**********************************************************
 * functions
 **********************************************************/
void *vox_thread(void *arg)
{
	vox_config.voxStatus = ENABLE;
//	struct sockaddr_in serv_addr = trigger_Conf.broadCast.sever_addr;
//	int sockfd = trigger_Conf.broadCast.sockid;
	int times = -1,i = 0,rNum = 0,j = 0;
	int errcode;
	unsigned long total = 0;//data
	short sample;
	float result = 0;
	static int old_sec_sendVoxToServer=0;
	static int new_sec_sendVoxToServer=0; 
	char *Ip_server = (char*)arg;
	flag_exit_vox = ENABLE;
	times = 0;
	while(vox_config.voxStatus == ENABLE && flag_exit_vox == ENABLE)
	{
		total = 0;result = 0;sample = 0;

		if(mlsGetDataFromMic(&sample) != MLS_SUCCESS)
		{
			perror("get data from device error\n");
			continue;
		}
		if(flag_exit_vox != ENABLE) break;

		result = 20*log10f((float)(abs(sample))/32768);
		if(result >= vox_config.threshold)
		{

			times++;
			if(times >= A_TIMES)	//because 2s we calculate a time;
			{
				struct ifreq ifr = trigger_Conf.ifr;
				char buf[128] = {'\0'};
#if 1
				//prepare message to warning
				sprintf(buf,"VOX:%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\r\n",
							(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[0],
							(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[1],
							(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[2],
							(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[3],
							(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[4],
							(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[5]);
				//send message to server.
				new_sec_sendVoxToServer = time(NULL);
				if ((new_sec_sendVoxToServer - old_sec_sendVoxToServer) > 60) // 60s
				{
				    errcode = system("/mlsrb/push_notification 1 0 &");
				    if(errcode == -1)
					printf("Error: exe system call\n");
				    old_sec_sendVoxToServer = new_sec_sendVoxToServer;
				}	
				pthread_mutex_lock(&trigger_Conf.vox_ack.aMutex);
				trigger_Conf.vox_ack.flag = false;
				pthread_mutex_unlock(&trigger_Conf.vox_ack.aMutex);
				i = 0;
				while(trigger_Conf.vox_ack.flag == false && i*100 < BROADCAST_TIME*1000 && flag_exit_vox == ENABLE) //timeout is BROADCAST_TIME sec
				{
					pthread_mutex_lock(&trigger_Conf.broadCast.aMutex);
					syslog(LOG_DEBUG,"send data to warning: vox\n");
					if(sendto(trigger_Conf.broadCast.sockid,buf,strlen(buf),0,(struct sockaddr *)&trigger_Conf.broadCast.sever_addr,sizeof(trigger_Conf.broadCast.sever_addr)) < 0){
						perror("sendto error");
					}
					pthread_mutex_unlock(&trigger_Conf.broadCast.aMutex);
					//syslog(LOG_DEBUG,"WHY WHY WHY!!!!!!!!!!!!!\n");
					mlsGetRandomNumber(&rNum);
					for(j = 0; j <= rNum; j++,i++){
						if(trigger_Conf.exitTrigger.flag == true || flag_exit_vox != ENABLE) break;
						usleep(100000);//100ms
					}
				
				}
				//syslog(LOG_DEBUG,"VOX: Exit Why Loop\n");
				//wait after warnig
				if(trigger_Conf.vox_ack.flag == true){
					//deactivate from application
					rNum = SLP_DEACTIVE;
				}
				else{
					//time out
					rNum = SLP_TIMEOUT;
				}
				for(j = 0; j < rNum; j++){
					if(trigger_Conf.exitTrigger.flag == true || flag_exit_vox != ENABLE) break;
					sleep(1);//sleep 1 second
				}
				//set times
				times = 0;
				pthread_mutex_lock(&trigger_Conf.vox_ack.aMutex);
				trigger_Conf.vox_ack.flag = true;
				pthread_mutex_unlock(&trigger_Conf.vox_ack.aMutex);
#else
				do
				{
					sockfd = socket(AF_INET, SOCK_STREAM, 0);
					if(sockfd < 0){
						perror("ERROR opening socket");
						usleep(10000);
					}
				}while(sockfd < 0 && flag_exit_vox == ENABLE);
				if(flag_exit_vox != ENABLE) break;
				memset(&serv_addr,0,sizeof(struct sockaddr_in));
				serv_addr.sin_family = AF_INET;
				serv_addr.sin_addr.s_addr = inet_addr(Ip_server);
				serv_addr.sin_port = htons(PORT);
				while( connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0 && flag_exit_vox == ENABLE)
				{
					perror("ERROR connecting");
					usleep(10000);
				}
				if(flag_exit_vox != ENABLE) break;
				if(getMacAddr(&ifr) != MLS_SUCCESS)
				{
					print("getMacAddr() function error\n");
				}
				sprintf(buf,"VOX: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\r\n",
							(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[0],
							(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[1],
							(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[2],
							(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[3],
							(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[4],
							(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[5]);
				if(write(sockfd,buf,strlen(buf)) < 0)
				{
					perror("write error\n");
				}

				times = 0;
				close(sockfd);
				for(i = 0; i < TIME_FREE*100;i++)//sleep to wait
				{
					if(flag_exit_vox != ENABLE) break;
					usleep(10000);
				}
#endif
			}
		}
		else
		{

			times = 0;
		}

	}
	pthread_exit(NULL);
}

/*
	open device, id of device will returned
*/
mlsErrorCode_t openAudioDevice(int *dsp,int *mix)
{
	*dsp = open(DSP,O_RDONLY);
	if(*dsp < 0)
	{
		print("open device DSP error\n");
		return _error(*dsp,*mix);
	}
	*mix = open(MIX,O_RDONLY);
	if(*mix < 0)
	{
		print("open device MIX error\n");
		return _error(*dsp,*mix);
	}
	return MLS_SUCCESS;
}
/*
	init audio record
*/
mlsErrorCode_t initialAudioDevice(int dsp,int mix)
{
	//initial record
	int _channel = 1,_sample_rate = 8000,_format=AFMT_S16_LE;
//	int _volume = 0x0;
//	ioctl(mix, MIXER_WRITE(SOUND_MIXER_MIC), &_volume);
	ioctl(dsp, SNDCTL_DSP_SETFMT, &_format);
	ioctl(dsp, SNDCTL_DSP_CHANNELS, &_channel);
	ioctl(dsp, SNDCTL_DSP_SPEED, &_sample_rate);
	return MLS_SUCCESS;
}
/*
	close device
*/
mlsErrorCode_t _error(int dsp,int mix)
{
	close(dsp);
	close(mix);
	return MLS_ERROR;
}
//main function
mlsErrorCode_t mlsVoxEnable(char *addr)
{
	mlsErrorCode_t reVal = MLS_SUCCESS;
	if(vox_config.voxStatus == ENABLE)
	{
		print("Had a vox_thread\n");
//		reVal = MLS_ERROR;
		return reVal;
	}
	int thread_id = -1;
	thread_id = pthread_create(&p_thread,NULL,vox_thread,addr);
	if(thread_id != 0)
	{
		perror("create vox_thread error\n");
		reVal = MLS_ERROR;
		return reVal;
	}
	return reVal;
}
mlsErrorCode_t mlsVoxDisable()
{
	if(vox_config.voxStatus == DISABLE){
		return MLS_SUCCESS;
	}
	flag_exit_vox = DISABLE;
	while(pthread_join(p_thread,NULL) != 0){
		pthread_cond_broadcast(&dataAvailable);
		usleep(10000);
	}
	vox_config.voxStatus = DISABLE;
	return MLS_SUCCESS;
}
mlsErrorCode_t mlsVoxGetThreshold(int *reg)
{
	*reg = (int)vox_config.threshold;
	return MLS_SUCCESS;
}
mlsErrorCode_t mlsVoxSetThreshold(char *threshold)
{
	print("string in put for setThreshold:%s\n",threshold);
	vox_config.threshold = atof(threshold);
	return MLS_SUCCESS;
}
mlsErrorCode_t mlsVoxGetStatus(int *reg)
{
	*reg = vox_config.voxStatus;
	return MLS_SUCCESS;
}
/**********************************************************/
