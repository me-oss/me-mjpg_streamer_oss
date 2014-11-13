/*
 * mlsflash.h
 *
 *  Created on: Aug 23, 2011
 *      Author: root
 */

#ifndef MLSFLASH_H_
#define MLSFLASH_H_

#include "mlsInclude.h"

//#define SPIFLASH_SIZE (4096*1024) //4MB
#define SPIFLASH_SIZE   (mlsflashGetFlashSize())
#define SPIFLASH_SECTORSIZE 4096
#define SPIFLASH_BLOCKSIZE (SPIFLASH_SECTORSIZE * 16)
#define SPIFLASH_BLOCK64KB (64*1024)
#define OTP_ADDRESS   (SPIFLASH_SIZE - 2 * SPIFLASH_SECTORSIZE) // previous sector of the config sector
#define TEST_OTP_ADDRESS (14*4096) // sector 14
//mlsErrorCode_t mlsflashInit();
//mlsErrorCode_t mlsflashDeInit();

mlsErrorCode_t mlsflashRead(unsigned int address,unsigned char* bufferRead, unsigned int lenRead);

mlsErrorCode_t mlsflashWrite(unsigned int address,unsigned char* bufferWrite, unsigned int lenWrite);

mlsErrorCode_t mlsflashEraseSectors(unsigned int address, int nsectors);



unsigned int mlsflashGetFlashSize();
////////////////////////////////////////////////////////////////////////////////////////////
typedef enum{
	Disable	= 0,
	Enable	= 1,

	VGA 	= 0,
	QVGA	= 1,
	QQVGA	= 2,

	Normal	= 0,
	Flipup	= 1,

	MinLevel	= 0,
	MaxLevel	= 8,
}parameter;
typedef struct settingConfig_st
{
	char SSIDuAP[33];
	char cameraName[13];
	char brightnessValue;
	char contrastValue;
	char voxStatus;
	signed char threshold;
	char vgaMode;
	char cameraView;
	char volume;
	union Extra{
		struct TempAlert{
			unsigned int min;
			unsigned int max;
		}temp_alert;
		char Reserved[128];
	}extra;
	unsigned int crc; //CRC32
}settingConfig_st;
extern settingConfig_st *settingConfig;
mlsErrorCode_t mlsSettingInit();
mlsErrorCode_t mlsGetConfig(settingConfig_st *reVal);
mlsErrorCode_t mlsSettingDeInit();
mlsErrorCode_t mlsSettingConfig(settingConfig_st *settingConfig);
mlsErrorCode_t mlsSettingSaveCameraName(char *name);
mlsErrorCode_t mlsSettingSaveBrightness(char brightnessValue);//ok
mlsErrorCode_t mlsSettingSaveContrast(char contrastValue);//ok
mlsErrorCode_t mlsSettingSaveVoxStatus(char voxStatus);
mlsErrorCode_t mlsSettingSaveVoxThreshold(signed char threshold);
mlsErrorCode_t mlsSettingSaveVGAMode(char vgaMode);//ok
mlsErrorCode_t mlsSettingSaveCameraView();//ok
mlsErrorCode_t mlsSettingSaveConfig(settingConfig_st *Config);
mlsErrorCode_t mlsSettingSaveVolume(char volume);
mlsErrorCode_t mlsSettingSaveTempAlert(unsigned int min, unsigned int max);
void dump_crash_data();

#define MLSSETTING_ERROR_FLASHREAD -1
#define MLSSETTING_ERROR_FLASHWRITE -2
#define MLSSETTING_ERROR_CRC -3


#endif /* MLSFLASH_H_ */
