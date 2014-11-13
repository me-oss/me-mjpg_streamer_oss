#ifndef _SYS_CONFIGS_H_
#define _SYS_CONFIGS_H_

#define MJPEGSTREAMER_CONFIGS

#define FLASH_SETTINGCONFIG_ADDRESS (SPIFLASH_SIZE-SPIFLASH_BLOCK64KB)
#define FLASH_SAVECONFIG_ADDRESS (SPIFLASH_SIZE - 4096)
#define FLASH_UAPCONFIG_ADDRESS (SPIFLASH_SIZE - 1024)
#define FLASH_JAVAAPPLET_ADDRESS (SPIFLASH_SIZE - 2048)
#define FLASH_AUDIO_FINETUNING_ADDRESS (FLASH_JAVAAPPLET_ADDRESS + 300)
#define FLASH_AUTHENKEY_ADDRESS (SPIFLASH_SIZE - 1016) 
#define FLASH_PORTMAPPING_ADDRESS (FLASH_AUTHENKEY_ADDRESS + 128)
#define FLASH_OTA_ADDRESS (111*SPIFLASH_BLOCKSIZE)

#if defined(UPNP_CONFIGS)
	#include "mlsInternal.h"
mlsErrorCode_t mlsInternal_ReadPortMapList(PortMapList_st *pPortMapList);
mlsErrorCode_t mlsInternal_SavePortMapList(PortMapList_st *PortMapList);
#endif
#if defined(OTA_UPGRADE) || defined(MJPEGSTREAMER_CONFIGS)
#include "mlsOTA.h"
mlsErrorCode_t DefaultOTAHeaderConfig(void);
mlsErrorCode_t GetOTAHeaderConfig(OTAHeader_st **pOTAHeader);
mlsErrorCode_t SaveOTAHeaderConfig(void);
#endif
#ifdef MJPEGSTREAMER_CONFIGS
	#include "mlsflash.h"
	#include "mlsWifi.h"
	#include "mlsaudio.h"
	#include "mlsAuthen.h"
	#define CAMERA_NAME	"Camera"

mlsErrorCode_t DefaultSettingConfig(void);
mlsErrorCode_t GetSettingConfig(settingConfig_st ** psettingConfig);
mlsErrorCode_t SaveSettingConfig(void);
char * GetVersionString(void);

mlsErrorCode_t DefaultIcamWifiConfig(void);
mlsErrorCode_t GetIcamWifiConfig(icamWifiConfig_st ** pIcamWifiConfig);
mlsErrorCode_t SaveIcamWifiConfig(void);

mlsErrorCode_t DefaultUAPConfig(void);
mlsErrorCode_t GetUAPConfig(uAPConfig_st **pUAPConfig);
mlsErrorCode_t SaveUAPConfig(void);

mlsErrorCode_t DefaultJavaAppletConfig(void);
mlsErrorCode_t GetJavaAppletConfig(javaApplet_st **pJavaApplet);
mlsErrorCode_t SaveJavaAppletConfig(void);

mlsErrorCode_t DefaultTempOffsetConfig(void);
mlsErrorCode_t GetTempOffsetConfig(TempOffset_st **pTempOffset);
mlsErrorCode_t SaveTempOffsetConfig(void);

mlsErrorCode_t DefaultAudioFineTuneConfig(void);
mlsErrorCode_t GetAudioFineTuneConfig(AudioFineTune_st **pAudioFineTune);
mlsErrorCode_t SaveAudioFineTuneConfig(void);

mlsErrorCode_t DefaultMasterKeyConfig(void);
mlsErrorCode_t GetMasterKeyConfig(MasterKey_st **pMasterKey);
mlsErrorCode_t SaveMasterKeyConfig(void);

mlsErrorCode_t DefaultOTPConfig(void);
mlsErrorCode_t GetOTPConfig(OTP_st **pOTP);
mlsErrorCode_t SaveOTPConfig(void);

mlsErrorCode_t DefaultPortMapListConfig(void);
mlsErrorCode_t GetPortMapListConfig(PortMapList_st **pPortMapList);
mlsErrorCode_t SavePortMapListConfig(void);
#endif

int DeinitNonVolatileConfigs(void);
int InitNonVolatileConfigs(void);
int GetFlashSize(void);
#endif //_SYS_CONFIGS_H_
