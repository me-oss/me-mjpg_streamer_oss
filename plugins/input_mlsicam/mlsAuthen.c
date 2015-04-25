/*
 * mlsflash.c
 *
 *  Created on: Aug 23, 2011
 *      Author: root
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>


#include <linux/types.h>
#include <linux/ioctl.h>
#include <string.h>

#include "sha2.h"
#include "minIni.h"
#include "mlsflash.h"
#include "sys_configs.h"
#include "nxcCRC32.h"
#include "mlsAuthen.h"
#define SECTOR_SIZE 4096
#define TOTAL_TIMEOUT_SEC 600  //(10 minutes)

#include "../../debug.h"

static MasterKey_st *gMasterKey = NULL;
static int ReadMasterKeyDone=0;
static int CreateSessionKeyDone=0;
static char gSessionKey[65];
static int isRemoteIPStrSet = 0;
static char gRemoteIPStr[16];
static time_t start_time=0;
static time_t current_time;
static unsigned long ServerIPLongNum = 0;

mlsErrorCode_t mlsAuthen_ResetCreateSessionKeyProcess()
{
	if(gMasterKey == NULL)
		GetMasterKeyConfig(&gMasterKey);
	CreateSessionKeyDone=0;
	memset(gSessionKey,0,sizeof(gSessionKey));	
    memset(gMasterKey->RandomKey,0,sizeof(gMasterKey->RandomKey));
	return MLS_SUCCESS;
}

mlsErrorCode_t mlsAuthen_ResetReadMasterKeyProcess()
{
	gMasterKey = NULL;
	return MLS_SUCCESS;
}



int mlsAuthen_UpdateRandomNumberStr(char* RandomNumber)
{
	if(gMasterKey == NULL)
		GetMasterKeyConfig(&gMasterKey);
    strncpy(gMasterKey->RandomKey,RandomNumber,9);
    return MLS_SUCCESS;
}




int mlsAuthen_GenerateSessionKey(char* RandomNumber, struct sockaddr_in* Client_IP, char* SessionKey)
{
	SHA256_CTX	ctx256;
	//int i;
	char fullstring[128];
	unsigned long tmp;

	if (!CreateSessionKeyDone)
	{
		tmp = GetMasterKeyConfig(&gMasterKey);

		if (tmp != MLS_SUCCESS || RandomNumber == NULL) 
		{

	    	srand(time(NULL));
	    	sprintf(fullstring,"%s%08X%08X",gMasterKey->MasterKey,(unsigned int) rand(),(unsigned int) rand());
		
	    }
	    else
	    {
		if (Client_IP != NULL)
		{
		    printf("Use Internal RemoteIP\n");
	    	    tmp = Client_IP->sin_addr.s_addr;
    		    sprintf(fullstring,"%s%08X%s",gMasterKey->MasterKey,(unsigned int)tmp,RandomNumber);
    		}
    		else
    		{
    		    printf("Use External RemoteIP\n");
    		    if (isRemoteIPStrSet)
    		    {
    		        sprintf(fullstring,"%s%s%s",gMasterKey->MasterKey,gRemoteIPStr,RandomNumber);
    		    }
    		    else
    		    {
	    		srand(time(NULL));
	    		sprintf(fullstring,"%s%08X%08X",gMasterKey->MasterKey,(unsigned int) rand(),(unsigned int) rand());
    		    }
    		}
	    }
        
		// start SHA
		SHA256_Init(&ctx256);
		SHA256_Update(&ctx256, (unsigned char*)fullstring, strlen(fullstring));
		SHA256_End(&ctx256,gSessionKey);
		//printf("\nSession Key: %s\n",gSessionKey);

	}
	CreateSessionKeyDone = 1;
    if (SessionKey != NULL)
    {
        memcpy(SessionKey,gSessionKey,sizeof(gSessionKey));
    }
    return 0;
}
mlsErrorCode_t mlsAuthen_ReadMasterKey(MasterKey_st *pMasterKey)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	retVal = GetMasterKeyConfig(&gMasterKey);
    if (pMasterKey != NULL)
	{
		memcpy(pMasterKey,&gMasterKey,sizeof(MasterKey_st));
	}

	return retVal;
}
mlsErrorCode_t mlsAuthen_ReadMACAddress(char* MACAddress_str,int* pMACAddress_val)
{
    struct ifreq if_buffer;
    int if_socket;
	int i;
    memset(&if_buffer, 0x00, sizeof(if_buffer));
	if_socket=socket(PF_INET, SOCK_DGRAM, 0);

	strcpy(if_buffer.ifr_name, "ra0");
	ioctl(if_socket, SIOCGIFHWADDR, &if_buffer);
	close(if_socket);
	if (pMACAddress_val != NULL)
	{
		for (i=0;i<6;i++)
		{
			*pMACAddress_val = if_buffer.ifr_hwaddr.sa_data[i];
			pMACAddress_val++;
		}
	}
	if (MACAddress_str != NULL)
	{
		sprintf(MACAddress_str,"%02X%02X%02X%02X%02X%02X",\
			if_buffer.ifr_hwaddr.sa_data[0],\
			if_buffer.ifr_hwaddr.sa_data[1],\
			if_buffer.ifr_hwaddr.sa_data[2],\
			if_buffer.ifr_hwaddr.sa_data[3],\
			if_buffer.ifr_hwaddr.sa_data[4],\
			if_buffer.ifr_hwaddr.sa_data[5]\
			);

	}
	return MLS_SUCCESS;	
} 

mlsErrorCode_t mlsAuthen_ReadNetMask(struct sockaddr_in* NetMask)
{
    struct ifreq if_buffer;
    int if_socket;
	int ret;
    memset(&if_buffer, 0x00, sizeof(if_buffer));
	if_socket=socket(PF_INET, SOCK_DGRAM, 0);

	strcpy(if_buffer.ifr_name, "ra0");
	ret = ioctl(if_socket, SIOCGIFNETMASK, &if_buffer);
	close(if_socket);
	if (!ret)
	{
		memcpy(NetMask,(struct sockaddr_in*)&(if_buffer.ifr_netmask),sizeof(struct sockaddr_in));
		printf("NetMask = %s\n",inet_ntoa(NetMask->sin_addr));
	}
	
	return ret;	
} 

mlsErrorCode_t mlsAuthen_ReadCurrentIP(struct sockaddr_in* CurrentIP)
{
    struct ifreq if_buffer;
    int if_socket;
	int ret;
    memset(&if_buffer, 0x00, sizeof(if_buffer));
	if_socket=socket(PF_INET, SOCK_DGRAM, 0);

	strcpy(if_buffer.ifr_name, "ra0");
	ret = ioctl(if_socket, SIOCGIFADDR, &if_buffer);
	close(if_socket);
	if (!ret)
	{
		memcpy(CurrentIP,(struct sockaddr_in*)(&if_buffer.ifr_addr),sizeof(struct sockaddr_in));

	}	
	return MLS_SUCCESS;	
} 

// return 1 if REMOTE, 0 LOCAL, 2 SERVER if NO only Correct for IPV4
int mlsAuthen_CheckIPisLocalorRemoteorServer(struct sockaddr_in CheckIP, struct sockaddr_in HostIP, struct sockaddr_in NetworkMask)
{
#if 1
	//struct in_addr tmp_addr;
	
	if ((CheckIP.sin_addr.s_addr & NetworkMask.sin_addr.s_addr) == (HostIP.sin_addr.s_addr & NetworkMask.sin_addr.s_addr))
	{
		return 0;
	}
	if (CheckIP.sin_addr.s_addr == LOCALHOST_ADDRESS)  //127.0.0.1
	{
	    return 3;
	}

	//if (pglobal->CentralServerIP.sin_addr.s_addr
	//inet_aton(CENTRAL_SERVER_ADDRESS,&tmp_addr);
	if (ServerIPLongNum == 0)
	{
	    // update Server IP
	    int errorCode = 0;
	    char server_name[128];
	    
	    //struct hostent *pHostEnt = NULL;
	    //struct sockaddr_in SockAddr;
	    struct addrinfo hints, *peer;
	    memset(&hints, 0 , sizeof (struct addrinfo));

	    ini_gets("General","MainAppServerName",CENTRAL_SERVER_ADDRESS,server_name,128,"/mlsrb/serverconfig.ini");
	    sprintf(servername,"vpc-nat.monitoreverywhere.com");
	    hints.ai_flags = AI_PASSIVE;
	    hints.ai_family = AF_INET;
	    hints.ai_socktype = SOCK_STREAM;
    
	    //pHostEnt = gethostbyname(SERVER_NAME);
	    errorCode = getaddrinfo(server_name,"80",&hints,&peer);
	    if(0 !=  errorCode)
	    {
    		printf( "Error - Failed to look up server %s:'%s' \r\n", CENTRAL_SERVER_ADDRESS,gai_strerror(errorCode) );

	    }
	    else
	    {
		ServerIPLongNum = ((struct sockaddr_in*)(peer->ai_addr))->sin_addr.s_addr;
		freeaddrinfo(peer);
	    }
	} 
	
	if (CheckIP.sin_addr.s_addr == ServerIPLongNum)
	{
	    return 2;
	}
	

	return 1;
#else
	return 0;
#endif
}

int mlsAuthen_TimerCheck()
{
    current_time = time(NULL);
    //printf("Total time %d\n",current_time - start_time);
    if ((current_time - start_time) < TOTAL_TIMEOUT_SEC)
    {
        return 1;
    }
    else
    {
        // RESET THE SESSION KEY

        mlsAuthen_ResetCreateSessionKeyProcess();
        mlsAuthen_GenerateSessionKey(NULL,NULL,NULL);         
        return 0;
    }
}

void mlsAuthen_TimerReset()
{
    start_time = time(&current_time);
}

char* mlsAuthen_RetrieveSessionKey()
{
    return gSessionKey;
}


void mlsAuthen_ClearRemoteIP()
{

    memset(gRemoteIPStr,0,sizeof(gRemoteIPStr));
    isRemoteIPStrSet = 0;
}

void mlsAuthen_SetRemoteIP(char* ip)
{

    strncpy(gRemoteIPStr,ip,16);
    isRemoteIPStrSet = 1;
}

int mlsAuthen_IsRemoteIPSet()
{
    return isRemoteIPStrSet;
}

