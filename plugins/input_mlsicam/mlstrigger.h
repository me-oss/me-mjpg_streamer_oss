/*
 * mlstrigger.h
 *
 *  Created on: Dec 20, 2011
 *      Author: mlsdev008
 */

#ifndef MLSTRIGGER_H_
#define MLSTRIGGER_H_

#include "mlsInclude.h"
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>

//define const
#define SLP_TIMEOUT		10
#define SLP_DEACTIVE	20
#define BROADCAST_TIME	10
#define LISTEN_PORT		htons(51111)
#define BROADCAST_PORT	htons(51110)

#ifndef _bool_
#define _bool_
typedef enum{
	true	=	1,
	false	=	0
}bool;
#endif
typedef struct{
	bool flag;
	pthread_mutex_t aMutex;
}flag_st;
typedef struct{
	int sockid;
	pthread_mutex_t aMutex;
	struct sockaddr_in sever_addr;
	struct sockaddr_in client_addr;
}socket_st;
typedef struct{
	flag_st exitTrigger;
	flag_st vox_ack;
	flag_st tempL_ack;
	flag_st tempH_ack;
	socket_st broadCast;
	struct ifreq ifr;//contain MAC address
}msltrigger_st;

msltrigger_st trigger_Conf;
//function
mlsErrorCode_t mlstriggerInit();
mlsErrorCode_t mlstriggerDeInit();
mlsErrorCode_t mlsSetupBroadCast();
extern mlsErrorCode_t getMacAddr(struct ifreq *ifr);
mlsErrorCode_t mlsCreateThreadListen();
mlsErrorCode_t parseMessage(char *buf,msltrigger_st *trigger_conf);
mlsErrorCode_t mlsSetupParameter();
mlsErrorCode_t mlsGetRandomNumber(int *rNum);
mlsErrorCode_t mlsGetIpAddressBroadcast(char *ip);
#endif /* MLSTRIGGER_H_ */
