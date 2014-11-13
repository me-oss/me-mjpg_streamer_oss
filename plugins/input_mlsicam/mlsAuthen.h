/*
 * mlsflash.h
 *
 *  Created on: Aug 23, 2011
 *      Author: root
 */

#ifndef MLSAUTHEN_H_
#define MLSAUTHEN_H_
#include <sys/socket.h>
#include <net/if.h>
#include "mlsInclude.h"
#include "mlsWifi.h"

#pragma pack(1)
typedef struct MasterKey_st
{
	char MasterKey[65];
	char RandomKey[9];
	char dummy[22];
	UInt32 crc;
}MasterKey_st;
#pragma pack()

mlsErrorCode_t mlsAuthen_ResetCreateSessionKeyProcess();

mlsErrorCode_t mlsAuthen_ResetReadMasterKeyProcess();

mlsErrorCode_t mlsAuthen_ReadMasterKey(MasterKey_st *pMasterKey);

mlsErrorCode_t mlsAuthen_SaveMasterKey(MasterKey_st *pMasterKey);


int mlsAuthen_GenerateSessionKey(char* RandomNumber, struct sockaddr_in* Client_IP, char* SessionKey);

char* mlsAuthen_RetrieveSessionKey();

mlsErrorCode_t mlsAuthen_ReadMACAddress(char* MACAddress_str,int* pMACAddress_val);

mlsErrorCode_t mlsAuthen_ReadNetMask(struct sockaddr_in* NetMask);

mlsErrorCode_t mlsAuthen_ReadCurrentIP(struct sockaddr_in* CurrentIP);

int mlsAuthen_CheckIPisLocalorRemoteorServer(struct sockaddr_in CheckIP, struct sockaddr_in HostIP, struct sockaddr_in NetworkMask);

void mlsAuthen_TimerReset();

int mlsAuthen_TimerCheck();  

void mlsAuthen_ClearRemoteIP();
void mlsAuthen_SetRemoteIP(char* ip);
int mlsAuthen_IsRemoteIPStrSet();

#endif /* MLSFLASH_H_ */
