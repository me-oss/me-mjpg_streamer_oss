/*
 * mlsflash.h
 *
 *  Created on: Aug 23, 2011
 *      Author: root
 */

#ifndef MLSOTA_H_
#define MLSOTA_H_

#include "mlsInclude.h"
//#include "mlsWifi.h"




#define TEMP_PATCH_PATH		"/tmp"
#define TEMP_PATCH_FILENAME "patch.tar.gz"
#define TEMP_PATCH_MD5_FILENAME "patch.md5"
#define DEFAULT_VERSION_FILE	"/etc/version"

#pragma pack(1)
typedef struct OTAHeader_st
{
	unsigned int filesize;
	char MD5_str[33];
	unsigned short major_version;
	unsigned short minor_version;
	char server_ip[32];
	int isUsedDevTest;
	char dummy[24];
	UInt32 crc;
}OTAHeader_st;

#pragma pack()


mlsErrorCode_t mlsOTA_GetVersion(unsigned short* major_version, unsigned short* minor_version);

mlsErrorCode_t OTA_WriteUpgrade(char* filepath, char* filename, char* md5string, unsigned short major_version, unsigned short minor_version );

mlsErrorCode_t OTA_upgrade();

mlsErrorCode_t mlsOTA_GetDefaultVersion(unsigned short* major_version, unsigned short* minor_version);
/*
 * Get version string aa_bbb
 */
String mlsOTA_GetVersionStr();
#endif /* MLSOTA_H_ */
