/*
 * mlstrigger.c
 *
 *  Created on: Dec 20, 2011
 *      Author: mlsdev008
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "mlstrigger.h"
#include "../../debug.h"


#define INTERFACE	"ra0"
#define UDPMSGSIZE	10000

static struct in_addr *
getbroadcastaddr(char *inter, int sock, char *buf){
	struct ifconf ifc;
	struct ifreq ifreq,*ifr;
	static struct in_addr addrs;
	struct sockaddr_in *sin;

	memset(&addrs,0,sizeof(addrs));
	ifc.ifc_len = UDPMSGSIZE;
	ifc.ifc_buf = buf;
	if (ioctl(sock, SIOCGIFCONF, (char *)&ifc) < 0) {
	   perror("broadcast: ioctl (get interface configuration)");
	   return(&addrs);
	}
	ifr = ifc.ifc_req;
	for (ifr=ifc.ifc_req;ifr->ifr_name[0];ifr++) {
	   if (strcmp(ifr->ifr_name,inter)) continue;
		   ifreq = *ifr;
		   if (ioctl(sock, SIOCGIFFLAGS, (char *)&ifreq) < 0) {
			   perror("broadcast: ioctl (get interface flags)");
			   continue;
		   }
		   if ((ifreq.ifr_flags & IFF_BROADCAST) &&
			   (ifreq.ifr_flags & IFF_UP) &&
			   (ifr->ifr_addr.sa_family == AF_INET)) {
				sin = (struct sockaddr_in *)&ifr->ifr_addr;
				if (ioctl(sock, SIOCGIFBRDADDR, (char *)&ifreq) < 0) {
					addrs = inet_makeaddr(inet_netof(sin->sin_addr), INADDR_ANY);
				} else {
					addrs = ((struct sockaddr_in *)(&ifreq.ifr_broadaddr))->sin_addr;
				}

				return(&addrs);
		   }
	}
	return(NULL);
}

mlsErrorCode_t mlsSetupParameter()
{
	if(pthread_mutex_init(&trigger_Conf.exitTrigger.aMutex,NULL) != 0){

		return MLS_ERROR;
	}
	if(pthread_mutex_init(&trigger_Conf.vox_ack.aMutex,NULL) != 0){

		return MLS_ERROR;
	}
	if(pthread_mutex_init(&trigger_Conf.tempL_ack.aMutex,NULL) != 0){

		return MLS_ERROR;
	}
	if(pthread_mutex_init(&trigger_Conf.tempH_ack.aMutex,NULL) != 0){

		return MLS_ERROR;
	}
	if(pthread_mutex_init(&trigger_Conf.broadCast.aMutex,NULL) != 0){

		return MLS_ERROR;
	}
	pthread_mutex_lock(&trigger_Conf.exitTrigger.aMutex);
	trigger_Conf.exitTrigger.flag = false;	//don't exit
	pthread_mutex_unlock(&trigger_Conf.exitTrigger.aMutex);
	pthread_mutex_lock(&trigger_Conf.vox_ack.aMutex);
	trigger_Conf.vox_ack.flag = true;		//don't send data
	pthread_mutex_unlock(&trigger_Conf.vox_ack.aMutex);
	pthread_mutex_lock(&trigger_Conf.tempL_ack.aMutex);
	trigger_Conf.tempL_ack.flag = true;
	pthread_mutex_unlock(&trigger_Conf.tempL_ack.aMutex);
	pthread_mutex_lock(&trigger_Conf.tempH_ack.aMutex);
	trigger_Conf.tempH_ack.flag = true;
	pthread_mutex_unlock(&trigger_Conf.tempH_ack.aMutex);
	return MLS_SUCCESS;
}

mlsErrorCode_t parseMessage(char *buf,msltrigger_st *trigger_Conf)
{
	int i = 0;
	for(i = 0; i < strlen(buf);i++)
	{
		if(buf[i] == ':'){
			buf[i] = '\0';
			break;
		}
	}

	if(strcmp(buf,"VOX") == 0){
		//message for vox trigger
		pthread_mutex_lock(&trigger_Conf->vox_ack.aMutex);
		trigger_Conf->vox_ack.flag = true;
		pthread_mutex_unlock(&trigger_Conf->vox_ack.aMutex);
	}else if(strcmp(buf,"TEMP_HIGH") == 0){
		//message for temperature too high
		pthread_mutex_lock(&trigger_Conf->tempH_ack.aMutex);
		trigger_Conf->tempH_ack.flag = true;
		pthread_mutex_unlock(&trigger_Conf->tempH_ack.aMutex);
	}else if(strcmp(buf,"TEMP_LOW") == 0){
		//message for temperature too low
		pthread_mutex_lock(&trigger_Conf->tempL_ack.aMutex);
		trigger_Conf->tempL_ack.flag = true;
		pthread_mutex_unlock(&trigger_Conf->tempL_ack.aMutex);
	}else{
		//unknown, what is it?

		return MLS_ERROR;
	}
	return MLS_SUCCESS;
}
/***********************************************************
 * threat listen and change flag ack in msltrigger_conf structure
 ***********************************************************/
void *mlsThreadListen(void *arg)
{
	struct sockaddr_in serv_addr,client_addr;
	int sockfd, newsockfd;
	char buf[127] = {'\0'};
	socklen_t sizeofsockaddr = sizeof(struct sockaddr_in);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0)
	{
		perror("create socket error\n");
		pthread_exit(NULL);
	}
	memset((char *)&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = LISTEN_PORT;
	if (bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0){
		perror("bind error\n");
		pthread_exit(NULL);
	}
	listen(sockfd,1);
	while(!trigger_Conf.exitTrigger.flag){
		newsockfd = accept(sockfd,(struct sockaddr *) &client_addr,&sizeofsockaddr);
		if(newsockfd < 0)
		{
			perror("accept error\n");
			continue;
		}
		memset(buf,0,sizeof(buf));
		if(read(newsockfd,buf,127) < 0)
		{
			perror("read data from client error\n");
			close(newsockfd);
			continue;
		}
		if(parseMessage(buf,&trigger_Conf) != MLS_SUCCESS){
			close(newsockfd);
			continue;
		}
		close(newsockfd);
	}
	close(sockfd);
	pthread_exit(NULL);
}
/***********************************************************
 *	listen ack (tcp)
 ***********************************************************/
mlsErrorCode_t mlsCreateThreadListen()
{
	int thread_id = -1;
	pthread_t p_thread;
	thread_id = pthread_create(&p_thread,NULL,mlsThreadListen,NULL);
	if(thread_id < 0)
	{
		printf("create thread listen error\n");
		return MLS_ERROR;
	}
	return MLS_SUCCESS;
}
/***********************************************************
 * broad cast (udp)
 ***********************************************************/
mlsErrorCode_t mlsSetupBroadCast()
{
	int on = 1;char buf[UDPMSGSIZE] = {'\0'};
	trigger_Conf.broadCast.sockid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(trigger_Conf.broadCast.sockid < 0){
		perror("create socket for broadcast error\n");
		return MLS_ERROR;
	}
	if(setsockopt(trigger_Conf.broadCast.sockid, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0) {
		perror("setsockopt(SO_REUSEADDR) failed");
		return MLS_ERROR;
	}
	memset((char *)&trigger_Conf.broadCast.sever_addr,0,sizeof(trigger_Conf.broadCast.sever_addr));
	trigger_Conf.broadCast.sever_addr.sin_family = AF_INET;
	trigger_Conf.broadCast.sever_addr.sin_addr = *(getbroadcastaddr(INTERFACE,trigger_Conf.broadCast.sockid,buf));
	trigger_Conf.broadCast.sever_addr.sin_port = BROADCAST_PORT;
//	memset((char *)trigger_Conf.broadCast.sever_addr.sin_zero,0,sizeof(trigger_Conf.broadCast.sever_addr.sin_zero));
	return MLS_SUCCESS;
}
/***********************************************************
 * initialize for trigger.
 **********************************************************/
mlsErrorCode_t mlstriggerInit()
{
	mlsErrorCode_t reVal = MLS_SUCCESS;

	reVal = getMacAddr(&trigger_Conf.ifr);
	if(reVal != MLS_SUCCESS)
	{
		printf("get Mac address error\n");
		goto mlstriggerInit_exit;
	}
	reVal = mlsSetupParameter();
	if(reVal != MLS_SUCCESS)
	{
		printf("setup parameter error\n");
		goto mlstriggerInit_exit;
	}
	reVal = mlsSetupBroadCast();
	if(reVal != MLS_SUCCESS)
	{
		printf("setup broad cast error\n");
		goto mlstriggerInit_exit;
	}
	reVal = mlsCreateThreadListen();
	if(reVal != MLS_SUCCESS)
	{
		printf("create thread to listen ack error\n");
		goto mlstriggerInit_exit;
	}
mlstriggerInit_exit:
	return reVal;
}
/***********************************************************
 * remove any thing if need
 ***********************************************************/
mlsErrorCode_t mlstriggerDeInit()
{
	pthread_mutex_lock(&trigger_Conf.exitTrigger.aMutex);
	trigger_Conf.exitTrigger.flag = true;
	pthread_mutex_unlock(&trigger_Conf.exitTrigger.aMutex);
	close(trigger_Conf.broadCast.sockid);
	pthread_mutex_destroy(&trigger_Conf.broadCast.aMutex);
	pthread_mutex_destroy(&trigger_Conf.vox_ack.aMutex);
	pthread_mutex_destroy(&trigger_Conf.tempL_ack.aMutex);
	pthread_mutex_destroy(&trigger_Conf.tempH_ack.aMutex);
	return MLS_SUCCESS;
}
/***********************************************************
 * get a random number
 ***********************************************************/
mlsErrorCode_t mlsGetRandomNumber(int *rNum)
{
	*rNum = random();
	*rNum = *rNum%3 + 3;
//	printf("number random is :%d\n",*rNum);
	return MLS_SUCCESS;
}
/***********************************************************
 ***********************************************************/
