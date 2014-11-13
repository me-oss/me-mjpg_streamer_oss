
/*
=================================
APIs for accessing configures
(UPNP, MJPEGSTREAMER,...)
                |           |
================^===========V====
Read configures |           |   |
      ----------^--------   |   |
  --> | Cache Memory    | <--   |
  |   ----------V--------       |
  |    Write new configures     |
==^=============V================
NonVolatile memory for configures
---------------------------------
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "mlsflash.h"
#include "sys_configs.h"
#include "../../debug.h"
#include "nxcCRC32.h"



//! Cache memory
typedef struct CameraConfigs{
	char Version[10];
	char MACAddr[13]; //String
	int FlashSize;
#if defined(UPNP_CONFIGS) || defined(MJPEGSTREAMER_CONFIGS)
	PortMapList_st PortMapList;
#endif
#if defined(OTA_UPGRADE) || defined(MJPEGSTREAMER_CONFIGS)
	OTAHeader_st OTAHeader;
#endif
#ifdef MJPEGSTREAMER_CONFIGS
	settingConfig_st settingConfig;

	icamWifiConfig_st icamWifiConfig;
	uAPConfig_st uAPConfig;
	javaApplet_st javaApplet;
	TempOffset_st TempOffset;
	AudioFineTune_st AudioFineTune;
	MasterKey_st MasterKey;
	int  is_default_MasterKey;
	OTP_st OTP;
#endif
}CameraConfigs;

CameraConfigs  NonVolatileConfigs;


mlsErrorCode_t ReadNonVolatileMemory(int Addr, unsigned char* Data, int Size)
{

	return mlsflashRead(Addr,Data,Size);
}

mlsErrorCode_t WriteNonVolatileMemory(int Addr, unsigned char* Data, int Size)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	int sectorAddr = (Addr / SPIFLASH_SECTORSIZE)*SPIFLASH_SECTORSIZE;
	int sectorNum = Size / SPIFLASH_SECTORSIZE;
	int sectorOffset = Addr % SPIFLASH_SECTORSIZE;
	unsigned char buf [SPIFLASH_SECTORSIZE];
	if (Size > SPIFLASH_SECTORSIZE)
	{
	    perror("************ERROR SIZE CANT BE LARGER THAN SECTORSIZE\n");
	    exit(1);
	}
	if(Size % SPIFLASH_SECTORSIZE)
		sectorNum +=1;
	//buf = malloc(sectorNum * SPIFLASH_SECTORSIZE);
	if(buf != NULL)
	{
		retVal = mlsflashRead(sectorAddr, buf, sectorNum*SPIFLASH_SECTORSIZE);
		if(retVal == MLS_SUCCESS)
		{
			//! Update data
			usleep(100000);
			memcpy(buf + sectorOffset, Data, Size);
			printf("===WRITE TO FLASH===\n");
			retVal = mlsflashWrite(sectorAddr,buf,sectorNum*SPIFLASH_SECTORSIZE);
			usleep(100000);
		}
	}else
		retVal = MLS_ERROR;
	return retVal;
}
#if defined(UPNP_CONFIGS) || defined(MJPEGSTREAMER_CONFIGS)

void InitUPNPConfigs(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt32 crc32;
	PortMapList_st * pPortMapList = &NonVolatileConfigs.PortMapList;

	retVal = ReadNonVolatileMemory(FLASH_PORTMAPPING_ADDRESS,(unsigned char*)pPortMapList,sizeof(PortMapList_st));
	if (retVal == MLS_SUCCESS)
	{

		crc32 = nxcCRC32Compute(pPortMapList,sizeof(PortMapList_st) - sizeof(pPortMapList->crc));
		if (crc32 == pPortMapList->crc)
		{
			return;
		}
	}
	pPortMapList->type = 0;
	pPortMapList->numport = 2;
	pPortMapList->list[0].intPort = 80;
    pPortMapList->list[0].extPort = (random()%20000 + 40000);;
    pPortMapList->list[1].intPort = 51108;  // get 	from mlsaudio.c Audio Port
	pPortMapList->list[1].extPort = (random()%20000 + 40000);
	memset(pPortMapList->IGDUrl,0,sizeof(pPortMapList->IGDUrl));
}

mlsErrorCode_t mlsInternal_ReadPortMapList(PortMapList_st *pPortMapList)
{
	memcpy(pPortMapList, &NonVolatileConfigs.PortMapList,sizeof(PortMapList_st));

	return MLS_SUCCESS;
}
/**
 *
 */
mlsErrorCode_t mlsInternal_SavePortMapList(PortMapList_st *PortMapList)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	PortMapList_st * pPortMapList = &NonVolatileConfigs.PortMapList;
	
	memcpy(pPortMapList, PortMapList, sizeof(PortMapList_st));
	pPortMapList->crc = nxcCRC32Compute(pPortMapList,sizeof(PortMapList_st)-sizeof(pPortMapList->crc));
	
	retVal = WriteNonVolatileMemory(FLASH_PORTMAPPING_ADDRESS,(unsigned char*) pPortMapList, sizeof(PortMapList_st));
	if (retVal != MLS_SUCCESS)
	{
		printf("Error: Save Port Map List\n");
	}
	return retVal;
}
#endif
#if defined(OTA_UPGRADE) || defined(MJPEGSTREAMER_CONFIGS)
mlsErrorCode_t DefaultOTAHeaderConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	OTAHeader_st *pOTAHeader = &NonVolatileConfigs.OTAHeader;

	retVal = mlsOTA_GetDefaultVersion(&pOTAHeader->major_version,&pOTAHeader->minor_version);
	if(retVal != MLS_SUCCESS)
	{
		pOTAHeader->major_version = 0;
		pOTAHeader->minor_version = 0;
	}
	return MLS_SUCCESS;
}
/**
 *
 */
static mlsErrorCode_t InitOTAHeaderConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	OTAHeader_st *pOTAHeader = &NonVolatileConfigs.OTAHeader;
	UInt32 crc32;

	retVal = ReadNonVolatileMemory(FLASH_OTA_ADDRESS,(unsigned char*)pOTAHeader,sizeof(OTAHeader_st));
	if (retVal == MLS_SUCCESS)
	{
		crc32 = nxcCRC32Compute(pOTAHeader,sizeof(OTAHeader_st) - sizeof(pOTAHeader->crc));
		if(crc32 == pOTAHeader->crc)
		{
			//! Successfully
			goto _exit;
		}
	}


	DefaultOTAHeaderConfig();
_exit:

	return retVal;
}
mlsErrorCode_t GetOTAHeaderConfig(OTAHeader_st **pOTAHeader)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	*pOTAHeader = &NonVolatileConfigs.OTAHeader;
	return retVal;
}
/**
 *
 */
mlsErrorCode_t SaveOTAHeaderConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	OTAHeader_st *pOTAHeader = &NonVolatileConfigs.OTAHeader;

	pOTAHeader->crc = nxcCRC32Compute(pOTAHeader,sizeof(OTAHeader_st) - sizeof(pOTAHeader->crc));
	retVal = WriteNonVolatileMemory(FLASH_OTA_ADDRESS,(unsigned char*)pOTAHeader,sizeof(OTAHeader_st));
	return retVal;
}
#endif
#ifdef MJPEGSTREAMER_CONFIGS

//================================================
mlsErrorCode_t DefaultSettingConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	//	UInt32 CRC;
	settingConfig_st * psettingConfig = &NonVolatileConfigs.settingConfig ;
	memset(psettingConfig->SSIDuAP,0,strlen(psettingConfig->SSIDuAP));
	strcpy(psettingConfig->cameraName,CAMERA_NAME);
	psettingConfig->brightnessValue = 4;
	psettingConfig->cameraView = Normal;
	psettingConfig->contrastValue = 2;
	psettingConfig->vgaMode = QVGA;
	psettingConfig->voxStatus = Enable;
	psettingConfig->threshold = -20; //default
	psettingConfig->volume = 50;

	return retVal;
}
/**
 *
 */
mlsErrorCode_t InitSettingConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
//	UInt32 CRC;
	settingConfig_st * psettingConfig = &NonVolatileConfigs.settingConfig ;

	retVal = ReadNonVolatileMemory(FLASH_SETTINGCONFIG_ADDRESS, (unsigned char *)psettingConfig,sizeof(settingConfig_st));
	if(retVal == MLS_SUCCESS)
	{
		if( nxcCRC32Compute(psettingConfig,sizeof(settingConfig_st) - sizeof(psettingConfig->crc)) == psettingConfig->crc)
		{

			goto exitmlsSettingInit;
		}
	}
	DefaultSettingConfig();

exitmlsSettingInit:
	if(psettingConfig->extra.temp_alert.min == 0 && psettingConfig->extra.temp_alert.max == 0)
	{
		psettingConfig->extra.temp_alert.min = 14;  
		psettingConfig->extra.temp_alert.max = 29;
	}

	return retVal;
}
mlsErrorCode_t GetSettingConfig(settingConfig_st ** psettingConfig)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	*psettingConfig = &NonVolatileConfigs.settingConfig ;
	return retVal;
}
/**
 *
 */
mlsErrorCode_t SaveSettingConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	settingConfig_st * psettingConfig = &NonVolatileConfigs.settingConfig ;
	
	psettingConfig->crc = nxcCRC32Compute(psettingConfig,sizeof(settingConfig_st) - sizeof(psettingConfig->crc));
	retVal = WriteNonVolatileMemory(FLASH_SETTINGCONFIG_ADDRESS,(unsigned char*)psettingConfig,sizeof(settingConfig_st));
	return retVal;
}
char * GetVersionString(void)
{
	return NonVolatileConfigs.Version;
}
//================================================
mlsErrorCode_t DefaultIcamWifiConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	icamWifiConfig_st* pIcamWifiConfig = &NonVolatileConfigs.icamWifiConfig ;

	strcpy(pIcamWifiConfig->icamVersion,"00_000");
	pIcamWifiConfig->networkMode = wm_uAP;
	pIcamWifiConfig->channelAdhoc = WIFI_DEFAULT_ADHOC_CHANNEL;
	pIcamWifiConfig->authMode = wa_open;
	pIcamWifiConfig->keyIndex = 0;
	strcpy(pIcamWifiConfig->strSSID,WIFI_SSIDNAME_DEFAULT);
	pIcamWifiConfig->strKey[0] = '\0';

	pIcamWifiConfig->strFolderRecName[0] = '\0';

	pIcamWifiConfig->ipMode = ip_dhcp;
	pIcamWifiConfig->ipAddressStatic[0] = '\0';
	pIcamWifiConfig->ipSubnetMask[0] = '\0';
	pIcamWifiConfig->ipDefaultGateway[0] = '\0';
	strcpy(pIcamWifiConfig->workPort,WIFI_DEFAULT_WORK_PORT);


	// Need to replace by algorithm later*/
	sprintf(pIcamWifiConfig->userName,"camera");
	sprintf(pIcamWifiConfig->passWord,"000000");

	return retVal;
}
/**
 *
 */
static mlsErrorCode_t InitIcamWifiConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt32 crc32;
	icamWifiConfig_st* pIcamWifiConfig = &NonVolatileConfigs.icamWifiConfig ;
	
	retVal = ReadNonVolatileMemory(FLASH_SAVECONFIG_ADDRESS,(unsigned char*)pIcamWifiConfig,sizeof(icamWifiConfig_st));
	if (retVal == MLS_SUCCESS)
	{
		crc32 = nxcCRC32Compute(pIcamWifiConfig,sizeof(icamWifiConfig_st) - sizeof(pIcamWifiConfig->crc));
		if (crc32 == pIcamWifiConfig->crc)
		{

			if(strcmp(pIcamWifiConfig->userName,"camera")!=0)
			{
				strcpy(pIcamWifiConfig->userName,"camera");
			}

			goto _exit;
		}
	}

	DefaultIcamWifiConfig();
_exit:
	return retVal;
}
mlsErrorCode_t GetIcamWifiConfig(icamWifiConfig_st** pIcamWifiConfig)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	* pIcamWifiConfig = &NonVolatileConfigs.icamWifiConfig ;
	return retVal;
}
/**
 *
 */
mlsErrorCode_t SaveIcamWifiConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	icamWifiConfig_st * pIcamWifiConfig = &NonVolatileConfigs.icamWifiConfig ;
	
	pIcamWifiConfig->crc = nxcCRC32Compute(pIcamWifiConfig,sizeof(icamWifiConfig_st) - sizeof(pIcamWifiConfig->crc));
	retVal = WriteNonVolatileMemory(FLASH_SAVECONFIG_ADDRESS,(unsigned char*)pIcamWifiConfig,sizeof(icamWifiConfig_st));
	return retVal;
}
//================================================
mlsErrorCode_t DefaultUAPConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	uAPConfig_st *pUAPConfig = &NonVolatileConfigs.uAPConfig;
	unsigned long store, *pStore;
	unsigned int seed;


	pStore = (unsigned long *)(&NonVolatileConfigs.MACAddr);
	store = *pStore + *(pStore+1) + *(pStore+2);
	seed = time(NULL) + store;
//	srand(seed); //give a random number
	Int8 tmp = 1;//[1;11] / {1,6,11}
	while(tmp == 1 || tmp == 6 || tmp == 11) {
		tmp = (Int8) (random()%11 + 1);
	}
	pUAPConfig->channel = tmp;
	return retVal;
}
/**
 *
 */
static mlsErrorCode_t InitUAPConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt32 crc32;
	uAPConfig_st *pUAPConfig = &NonVolatileConfigs.uAPConfig;


	retVal = ReadNonVolatileMemory(FLASH_UAPCONFIG_ADDRESS,(unsigned char*)pUAPConfig,sizeof(uAPConfig_st));
	if (retVal == MLS_SUCCESS)
	{
		crc32 = nxcCRC32Compute(pUAPConfig,sizeof(uAPConfig_st) - sizeof(pUAPConfig->crc));
		if (crc32 == pUAPConfig->crc)
		{

			goto _exit;
		}
	}

	DefaultUAPConfig();
	
_exit:

	return retVal;
}
mlsErrorCode_t GetUAPConfig(uAPConfig_st **pUAPConfig)
{
	*pUAPConfig = &NonVolatileConfigs.uAPConfig;
	return MLS_SUCCESS;
}
/**
 *
 */
mlsErrorCode_t SaveUAPConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	uAPConfig_st *pUAPConfig = &NonVolatileConfigs.uAPConfig;
	
	pUAPConfig->crc = nxcCRC32Compute(pUAPConfig,sizeof(uAPConfig_st) - sizeof(pUAPConfig->crc));
	retVal = WriteNonVolatileMemory(FLASH_UAPCONFIG_ADDRESS,(unsigned char*)pUAPConfig,sizeof(uAPConfig_st));
	return retVal;
}
//================================================
mlsErrorCode_t DefaultJavaAppletConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	javaApplet_st *pJavaApplet= &NonVolatileConfigs.javaApplet;
	strcpy(pJavaApplet->path,"");
	return retVal;
}
/**
 *
 */
static mlsErrorCode_t InitJavaAppletConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt32 crc32;
	javaApplet_st *pJavaApplet= &NonVolatileConfigs.javaApplet;


	retVal = ReadNonVolatileMemory(FLASH_JAVAAPPLET_ADDRESS,(unsigned char*)pJavaApplet,sizeof(javaApplet_st));
	if (retVal == MLS_SUCCESS)
	{
		crc32 = nxcCRC32Compute(pJavaApplet,sizeof(javaApplet_st) - sizeof(pJavaApplet->crc));

		if (crc32 == pJavaApplet->crc)
		{
			//! Successfully
			goto _exit;
		}
	}

	DefaultJavaAppletConfig();
_exit:

	return retVal;
}
mlsErrorCode_t GetJavaAppletConfig(javaApplet_st **pJavaApplet)
{
	*pJavaApplet= &NonVolatileConfigs.javaApplet;
	return MLS_SUCCESS;
}
/**
 *
 */
mlsErrorCode_t SaveJavaAppletConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	javaApplet_st *pJavaApplet= &NonVolatileConfigs.javaApplet;
	
	pJavaApplet->crc = nxcCRC32Compute(pJavaApplet,sizeof(javaApplet_st) - sizeof(pJavaApplet->crc));
	retVal = WriteNonVolatileMemory(FLASH_JAVAAPPLET_ADDRESS,(unsigned char*)pJavaApplet,sizeof(javaApplet_st));
	return retVal;
}
//================================================
mlsErrorCode_t DefaultTempOffsetConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	TempOffset_st *pTempOffset = &NonVolatileConfigs.TempOffset;
	pTempOffset->offset = 0;
	pTempOffset->cds_type = 0;
	pTempOffset->video_test_finish = 0;
	return retVal;
}
/**
 *
 */
static mlsErrorCode_t InitTempOffsetConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt32 crc32;
	TempOffset_st *pTempOffset = &NonVolatileConfigs.TempOffset;
    retVal = ReadNonVolatileMemory(OTP_ADDRESS,(unsigned char*)pTempOffset,sizeof(TempOffset_st));
    if (retVal == MLS_SUCCESS)
    {
		crc32 = nxcCRC32Compute(pTempOffset,sizeof(TempOffset_st) - sizeof(pTempOffset->crc));
		if (crc32 == pTempOffset->crc)
		{
			//!Successfully
			goto _exit;
		}
    }
	printf("Error: read TempOffset configs, use defaults\n");
	DefaultTempOffsetConfig();

_exit:
	printf("Temp offset configs: %d\n",pTempOffset->offset);
	printf("VIDEO MODE is %s\n",(pTempOffset->video_test_finish==1)?"lock":"unlock");
	printf("IR_On: %d - IR_Off: %d - CDS: %d\n", pTempOffset->cds99_IR_On,pTempOffset->cds99_IR_Off,pTempOffset->cds_type);
	return retVal;
}
mlsErrorCode_t GetTempOffsetConfig(TempOffset_st **pTempOffset)
{
	*pTempOffset = &NonVolatileConfigs.TempOffset;
	return MLS_SUCCESS;
}
/**
 *
 */
mlsErrorCode_t SaveTempOffsetConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	TempOffset_st *pTempOffset = &NonVolatileConfigs.TempOffset;
	
	pTempOffset->crc = nxcCRC32Compute(pTempOffset,sizeof(TempOffset_st) - sizeof(pTempOffset->crc));
	retVal = WriteNonVolatileMemory(OTP_ADDRESS,(unsigned char*)pTempOffset,sizeof(TempOffset_st));
	return retVal;
}

//================================================
#define AUDIO_FINETUNE1_64DB 0x80020F0B
#define AUDIO_FINETUNE2_64DB 0x80784638
mlsErrorCode_t DefaultAudioFineTuneConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	AudioFineTune_st *pAudioFineTune = &NonVolatileConfigs.AudioFineTune;
	pAudioFineTune->finetune1 = AUDIO_FINETUNE1_64DB;
	pAudioFineTune->finetune2 = AUDIO_FINETUNE2_64DB;
	return retVal;
}
/**
 *
 */
static mlsErrorCode_t InitAudioFineTuneConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt32 crc32;

    AudioFineTune_st *pAudioFineTune = &NonVolatileConfigs.AudioFineTune;

    retVal = ReadNonVolatileMemory(FLASH_AUDIO_FINETUNING_ADDRESS, (unsigned char*)pAudioFineTune, sizeof(AudioFineTune_st));
    if (retVal == MLS_SUCCESS)
    {
		crc32 = nxcCRC32Compute(pAudioFineTune,sizeof(AudioFineTune_st) - sizeof(pAudioFineTune->crc));
		if (crc32 == pAudioFineTune->crc)
		{
			//!Successfully
			goto _exit;
		}
    }

    DefaultAudioFineTuneConfig();
_exit:

	return retVal;
}
mlsErrorCode_t GetAudioFineTuneConfig(AudioFineTune_st **pAudioFineTune)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	*pAudioFineTune = &NonVolatileConfigs.AudioFineTune;

	return retVal;
}
/**
 *
 */
mlsErrorCode_t SaveAudioFineTuneConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	AudioFineTune_st *pAudioFineTune = &NonVolatileConfigs.AudioFineTune;
	
	pAudioFineTune->crc = nxcCRC32Compute(pAudioFineTune,sizeof(AudioFineTune_st) - sizeof(pAudioFineTune->crc));
	retVal = WriteNonVolatileMemory(FLASH_AUDIO_FINETUNING_ADDRESS,(unsigned char*)pAudioFineTune,sizeof(AudioFineTune_st));
	return retVal;
}
//===============================================
mlsErrorCode_t DefaultMasterKeyConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	MasterKey_st *pMasterKey = &NonVolatileConfigs.MasterKey;
	memset(pMasterKey,0,sizeof(MasterKey_st));
	return retVal;
}
/**
 *
 */
static mlsErrorCode_t InitMasterKeyConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt32 crc32;
	MasterKey_st *pMasterKey = &NonVolatileConfigs.MasterKey;
	retVal = ReadNonVolatileMemory(FLASH_AUTHENKEY_ADDRESS,(unsigned char*)pMasterKey,sizeof(MasterKey_st));
	if (retVal == MLS_SUCCESS)
	{
		crc32 = nxcCRC32Compute(pMasterKey,sizeof(MasterKey_st) - sizeof(pMasterKey->crc));
		if(crc32 == pMasterKey->crc)
		{
			//! Successfully
			NonVolatileConfigs.is_default_MasterKey = 0;
			goto _exit;
		}
	}

	DefaultMasterKeyConfig();
	NonVolatileConfigs.is_default_MasterKey = 1;
_exit:

	return retVal;
}
mlsErrorCode_t GetMasterKeyConfig(MasterKey_st **pMasterKey)
{
	*pMasterKey = &NonVolatileConfigs.MasterKey;
	if(NonVolatileConfigs.is_default_MasterKey == 1)
		return MLS_ERROR;
	else
		return MLS_SUCCESS;
}
/**
 *
 */
mlsErrorCode_t SaveMasterKeyConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	MasterKey_st *pMasterKey = &NonVolatileConfigs.MasterKey;
	MasterKey_st checkMasterKey;
	int crc32;
	
	pMasterKey->crc = nxcCRC32Compute(pMasterKey,sizeof(MasterKey_st) - sizeof(pMasterKey->crc));
	retVal = WriteNonVolatileMemory(FLASH_AUTHENKEY_ADDRESS,(unsigned char*)pMasterKey,sizeof(MasterKey_st));
	if (retVal != 0)
	{
	    return retVal;
	}
	

	retVal = ReadNonVolatileMemory(FLASH_AUTHENKEY_ADDRESS,(unsigned char*)&checkMasterKey,sizeof(MasterKey_st));
	if (retVal == MLS_SUCCESS)
	{
		crc32 = nxcCRC32Compute(&checkMasterKey,sizeof(MasterKey_st) - sizeof(pMasterKey->crc));
		if(crc32 == checkMasterKey.crc)
		{
		    if (strcmp(checkMasterKey.MasterKey,pMasterKey->MasterKey) == 0)
		    {
		      
			NonVolatileConfigs.is_default_MasterKey = 0;
			return 0;  // working well
		    }

		}
		else
		{

		}
		return -1;

	}
	
	
	
	return retVal;
}
//==============================================
mlsErrorCode_t DefaultOTPConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	OTP_st *pOTP= &NonVolatileConfigs.OTP;
	memset(pOTP,0,sizeof(OTP_st));
	return retVal;
}
/**
 *
 */
static mlsErrorCode_t InitOTPConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt32 crc32;
	OTP_st *pOTP= &NonVolatileConfigs.OTP;
	// read uAP config from flash
	retVal = ReadNonVolatileMemory(TEST_OTP_ADDRESS,(unsigned char*)pOTP,sizeof(OTP_st));
	if (retVal == MLS_SUCCESS)
	{
		crc32 = nxcCRC32Compute(pOTP,sizeof(OTP_st) - sizeof(pOTP->crc));

		if ((crc32 == pOTP->crc) && (pOTP->crc != 0) )
		{
			//! Successfully
			goto _exit;
		}
	}
	printf("Error: read OTP configs\n");
	DefaultOTPConfig();
_exit:
	printf("OTP configs: MAC=%s\n",pOTP->MacAddress);
	strcpy(NonVolatileConfigs.MACAddr,pOTP->MacAddress);
	return retVal;
}
mlsErrorCode_t GetOTPConfig(OTP_st **pOTP)
{
	*pOTP = &NonVolatileConfigs.OTP;
	return MLS_SUCCESS;
}
/**
 *
 */
mlsErrorCode_t SaveOTPConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	OTP_st *pOTP= &NonVolatileConfigs.OTP;
	
	pOTP->crc = nxcCRC32Compute(pOTP,sizeof(OTP_st) - sizeof(pOTP->crc));
	retVal = WriteNonVolatileMemory(TEST_OTP_ADDRESS,(unsigned char*)pOTP,sizeof(OTP_st));
	return retVal;
}
//==============================================
mlsErrorCode_t DefaultPortMapListConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	PortMapList_st *pPortMapList= &NonVolatileConfigs.PortMapList;
	memset(pPortMapList,0,sizeof(PortMapList_st));
	pPortMapList->type = 0;
	pPortMapList->numport = 2;
	pPortMapList->list[0].intPort = 80;
	pPortMapList->list[0].extPort = (random()%20000 + 40000);;
	pPortMapList->list[1].intPort = 51108;  // get 	from mlsaudio.c Audio Port
	pPortMapList->list[1].extPort = (random()%20000 + 40000);
	printf("Set Default PortmapList = (%d,%d);(%d,%d)\n",pPortMapList->list[0].intPort,pPortMapList->list[0].extPort,
				pPortMapList->list[1].intPort,pPortMapList->list[1].extPort);
	return retVal;
}
/**
 *
 */
static mlsErrorCode_t InitPortMapListConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt32 crc32;
	PortMapList_st *pPortMapList= &NonVolatileConfigs.PortMapList;
	// read uAP config from flash
	retVal = ReadNonVolatileMemory(FLASH_PORTMAPPING_ADDRESS,(unsigned char*)pPortMapList,sizeof(PortMapList_st));
	if (retVal == MLS_SUCCESS)
	{
		crc32 = nxcCRC32Compute(pPortMapList,sizeof(PortMapList_st) - sizeof(pPortMapList->crc));

		if ((crc32 == pPortMapList->crc) && (pPortMapList->crc != 0) )
		{
			//! Successfully
			goto _exit;
		}
	}
	printf("Error: read OTP configs\n");
	DefaultPortMapListConfig();
	SavePortMapListConfig();
_exit:
	printf("PortMapList configs: ...");
	printf("(%d,%d);(%d,%d)\n",pPortMapList->list[0].intPort,pPortMapList->list[0].extPort,
				pPortMapList->list[1].intPort,pPortMapList->list[1].extPort);

	return retVal;
}
mlsErrorCode_t GetPortMapListConfig(PortMapList_st **pPortMapList)
{
	*pPortMapList = &NonVolatileConfigs.PortMapList;
	return MLS_SUCCESS;
}
/**
 *
 */
mlsErrorCode_t SavePortMapListConfig(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	PortMapList_st *pPortMapList= &NonVolatileConfigs.PortMapList;

	pPortMapList->crc = nxcCRC32Compute(pPortMapList,sizeof(PortMapList_st) - sizeof(pPortMapList->crc));
	retVal = WriteNonVolatileMemory(FLASH_PORTMAPPING_ADDRESS,(unsigned char*)pPortMapList,sizeof(PortMapList_st));
	return retVal;
}
/**
 *
 *
 *
 */
static void InitMJPEGSConfigs(void)
{
	InitOTPConfig();
	//!settingConfig_st
	InitSettingConfig();
	//! icamWifiConfig_st
	InitIcamWifiConfig();
	//! uAPConfig_st
	InitUAPConfig();
	//! javaApplet_st
	InitJavaAppletConfig();
	//! PortMapList_st
	//! TempOffset_st
	InitTempOffsetConfig();
	//! OTAHeader_st
	InitOTAHeaderConfig();
	//! AudioFineTune_st
	InitAudioFineTuneConfig();
	//! MasterKey_st
	InitMasterKeyConfig();
	InitPortMapListConfig();
	//!
	sprintf(NonVolatileConfigs.Version, "%02d_%03d",
			NonVolatileConfigs.OTAHeader.major_version,
			NonVolatileConfigs.OTAHeader.minor_version);
}


#endif
/**
 * Read all configures from NonVolatile memory into Cache Memory
 *
 */
int InitNonVolatileConfigs(void)
{
	//! OTP_st;
    unsigned int seed,seed1;
    int fd;
    struct timeval t1;
    char cmd[64];
    gettimeofday(&t1,NULL);
    fd = open("/dev/dsp",O_RDWR);
    ioctl(fd,1108,&seed); 
    ioctl(fd,1107,&seed1); 
    close(fd);
    seed = seed*seed1+t1.tv_usec+t1.tv_sec;

    srand(seed);
    sprintf(cmd,"echo %u >/dev/urandom",seed);
    system(cmd);

	memset(&NonVolatileConfigs,0,sizeof(CameraConfigs));
	NonVolatileConfigs.FlashSize = mlsflashGetFlashSize();
#ifdef UPNP_CONFIGS
	InitUPNPConfigs();
#endif
#ifdef OTA_UPGRADE
	InitOTAHeaderConfig();
#endif
#ifdef MJPEGSTREAMER_CONFIGS
	InitMJPEGSConfigs();
#endif
	return 0;
}
int GetFlashSize(void)
{
	return NonVolatileConfigs.FlashSize;
}
/**
 *
 *
 */
int DeinitNonVolatileConfigs(void)
{
#ifdef UPNP_CONFIGS

#endif

#ifdef MJPEGSTREAMER_CONFIGS
	
#endif
	return 0;
}

