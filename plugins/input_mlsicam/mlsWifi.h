/*
 * mlsWifi.h
 *
 *  Created on: Oct 9, 2010
 *      Author: mlslnx003
 */

#ifndef MLSWIFI_H_
#define MLSWIFI_H_

#include "mlsInclude.h"
#include "mlsicamConfig.h"

#define Icam_RESETSWITCH_MASK 0x01

#define WIFI_MODE_DEFAULT "ad-hoc"
#define WIFI_Icam_IP_DEFAULT	  "192.168.2.1"



#define FLASH_SAVECONFIG_ADDRESS (SPIFLASH_SIZE - 4096)
#define FLASH_UAPCONFIG_ADDRESS (SPIFLASH_SIZE - 1024)
#define FLASH_JAVAAPPLET_ADDRESS (SPIFLASH_SIZE - 2048)

#define FLASH_AUDIO_FINETUNING_ADDRESS (FLASH_JAVAAPPLET_ADDRESS + 300)

#define FLASH_AUTHENKEY_ADDRESS (SPIFLASH_SIZE - 1016)
#define FLASH_PORTMAPPING_ADDRESS (FLASH_AUTHENKEY_ADDRESS + 128)

#define WIFI_CONFIGSUPPLIANCE_FILE "/tmp/wifisuppliance.conf"

#define WIFI_RESOVLDHCP_FILE "/etc/resolv.conf"

typedef enum networkMode_enum
{
	wm_adhoc = 0,
	wm_infra = 1,
	wm_uAP = 2
}networkMode_t;

typedef enum authMode_enum
{
	wa_open = 0,
	wa_shared = 1,
	wa_wpa = 2,
	wa_wpa2 = 3
}authMode_t;

typedef enum encryptProtocol_enum
{
	we_nonsecure = 0,
	we_wep = 1,
	we_tkip = 2,
	we_aes = 3
}encryptProtocol_t;

typedef enum ipMode_enum
{
	ip_dhcp = 0,
	ip_static = 1,
}ipMode_t;

#pragma pack(1)


#define PORTFORWARD_MAXPORT 3
typedef struct PortMap_st
{
    unsigned short intPort;
    unsigned short extPort;
} PortMap_st;

typedef struct PortMapList_st
{
    unsigned int numport;
    PortMap_st list[PORTFORWARD_MAXPORT];
    unsigned short type;
    char IGDUrl[256];
    char dummy[32];
    unsigned int crc;
}PortMapList_st;


typedef struct icamWifiConfig_st
{
	Char icamVersion[10];

	Char networkMode;

	Char channelAdhoc;  

	Char authMode;	

	Char encryptProtocol;

	Char keyIndex;

	Char strSSID[256];

	Char strKey[30];

	Char strFolderRecName[256];

	Char ipMode;

	Char ipAddressStatic[16];

	Char ipSubnetMask[16];

	Char ipDefaultGateway[16];

	Char workPort[8];

	Char userName[100];

	Char passWord[100];

	UInt32 crc; 
}icamWifiConfig_st;

typedef struct uAPConfig_st
{
	Int8 channel;
	UInt32 crc;
}uAPConfig_st;

typedef struct javaApplet_st
{
	Char path[256];//string path to save file in java applet
	UInt32 crc; //CRC32
}javaApplet_st;

typedef struct TempOffset_st
{
	int offset;
	unsigned short cds_type;
	unsigned short video_test_finish;
	int cds99_IR_On;
	int cds99_IR_Off;
	char dummy[20];
	unsigned int crc;
}TempOffset_st;

typedef struct OTP_st
{
    char MacAddress[13];
    char dummy [1024];
    unsigned int crc;
}OTP_st;
#pragma pack()

//#pragma pack()

typedef struct boot_opition_st
{
	Char swith_option_string[26];
	UInt32 crc;
}boot_opition_st;

typedef enum icamMode_enum
{
	icamModeReset= -100,
	icamModeNormal = -101
}icamMode_t;


mlsErrorCode_t mlsWifi_SetupReset();


mlsErrorCode_t mlsWifi_SetupNormal();


mlsErrorCode_t mlsWifi_SaveConfiguration(icamWifiConfig_st *icamWifiConfig);


mlsErrorCode_t mlsWifi_Bootswitch();


mlsErrorCode_t mlsWifi_ReadConfiguration(icamWifiConfig_st* icamWifiConfig);


mlsErrorCode_t mlsWifi_Setup(icamMode_t *icamMode,networkMode_t *networkMode);

mlsErrorCode_t mlsWifi_DeInit();

/*
 *
 */
mlsErrorCode_t mlsWifi_ReadConfigUAP(uAPConfig_st *puAPConfigOut);

/*
 *
 */
mlsErrorCode_t mlsWifi_SaveConfigUAP(uAPConfig_st *puAPConfig);

mlsErrorCode_t mlsWifi_SetSSID(char *ssid);
mlsErrorCode_t mlsWifi_SavePortMapList(PortMapList_st *pPortMapList);
mlsErrorCode_t mlsWifi_ReadPortMapList(PortMapList_st *pPortMapList);
mlsErrorCode_t mlsWifi_SaveOTPStruct(OTP_st *pOTP);
mlsErrorCode_t mlsWifi_ReadOTPStruct(OTP_st *pOTP);
int get_routers_list_xml(char * out);
int Mac_calculate_code (char* macaddress, char* userinput);
#endif /* MLSWIFI_H_ */
