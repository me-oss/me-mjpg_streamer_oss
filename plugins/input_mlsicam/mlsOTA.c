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

//#include "mlsCrc.h"

///////////////////////////////////////////////////////////////////////////////////////////
/****************************************************************************
 *                                                                          *
 * Copyright (c) 2008 Nuvoton Technology Corp. All rights reserved.         *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 *
 * FILENAME
 *     spi.c
 *
 * DESCRIPTION
 *     This file is an SPI demo program
 *
 **************************************************************************/

#include <linux/types.h>
#include <linux/ioctl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "mlsflash.h"
#include "sys_configs.h"
#include "nxcCRC32.h"
#include "mlsOTA.h"
#include "../../debug.h"
/*
 * baby monitor uAP mode
 */

static OTAHeader_st *pOTAHeader;
//static int OTA_Process_Error = 0;


static unsigned int mlsOTA_GetBlockOffset()
{
	unsigned int block_offset=0;


	if (mlsflashGetFlashSize() == 8 * 1024 * 1024)
	{
	    block_offset = 111;
	}
	else
	{
	    block_offset = 0x3e;
	}
	//printf("Block Offset :%d\n",block_offset);
	return block_offset;
}


static unsigned int mlsOTA_GetMaxOTAFileSize()
{
	unsigned int maxfilesize = 0;

	if (mlsflashGetFlashSize() == 8 * 1024 * 1024)
	{
	    maxfilesize = (1024*1024);
	}
	else
	{
	    maxfilesize = (60 * 1024);
	}
	//printf("MaxSize = %d\n",maxfilesize);
	return maxfilesize;
}



mlsErrorCode_t mlsOTA_GetVersion(unsigned short* major_version, unsigned short* minor_version)
{

	if(pOTAHeader == NULL)
		return MLS_ERROR;

	*major_version = pOTAHeader->major_version;
	*minor_version = pOTAHeader->minor_version;
	return MLS_SUCCESS;
}


mlsErrorCode_t mlsOTA_GetDefaultVersion(unsigned short* major_version, unsigned short* minor_version)
{
    FILE *fp;
    mlsErrorCode_t retcode=MLS_SUCCESS;
    char tmpstr[16];
    char *tmpptr;
    int num;
    
    
    fp = fopen(DEFAULT_VERSION_FILE,"r");
    if (fp == NULL)
    {
	retcode = -1;
	goto exit;
    }
    num = fread(tmpstr,1,15,fp);
    tmpstr[num]='\0';
    tmpptr = strchr(tmpstr,'_');
    if (tmpptr == NULL)
    {
	retcode = -1;
	goto exit;
    }
    *tmpptr ='\0';
    *major_version = atoi(tmpstr);
    *minor_version = atoi(tmpptr+1);
    if (*major_version >99 || *minor_version >999)
    {
	retcode = -1;
	goto exit;
    }

exit:    
    if (fp != NULL)
	fclose(fp);
    return retcode;
}


mlsErrorCode_t OTA_upgrade()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	FILE* fp;
	int tmp_size = 0,total=0;
	char *buffer;
	char filename[32];
	char system_command[64];
	char md5str[33];

	retVal = GetOTAHeaderConfig(&pOTAHeader);
	if (retVal != MLS_SUCCESS )
	{
		// How to Force Download the OTA again??? Let the Main app handle
		return retVal;
	}
	else
	{
		if (pOTAHeader->filesize == 0)
		{
			return MLS_SUCCESS; // have nothing to OTA
		}
	}
	sprintf(filename,"%s/%s",TEMP_PATCH_PATH,TEMP_PATCH_FILENAME);
	unlink(filename);
	fp = fopen(filename,"wb");
	if (fp == NULL)
	{

		return MLS_ERR_OPENFILE;
	}
	buffer = malloc (SPIFLASH_BLOCKSIZE);
	if (buffer == NULL)
	{

		return MLS_ERROR;
	}
	total = 0;
	while(total < pOTAHeader->filesize)
	{
		tmp_size = (pOTAHeader->filesize - total);

		tmp_size = (tmp_size <  SPIFLASH_BLOCKSIZE)? tmp_size:SPIFLASH_BLOCKSIZE;
		mlsflashRead (mlsOTA_GetBlockOffset()*SPIFLASH_BLOCKSIZE+SPIFLASH_SECTORSIZE+total,(unsigned char*)buffer,tmp_size);
		if (fwrite(buffer,1,tmp_size,fp) != tmp_size)
		{

			free(buffer);
			fclose(fp);
			return MLS_ERROR;
		}
		total+=tmp_size;
	}
	fflush(fp);
	fclose(fp);
	free(buffer);

	chdir(TEMP_PATCH_PATH);
	sprintf(system_command,"md5sum %s >/tmp/xyz.md5",filename);
	system(system_command);
	fp = fopen("/tmp/xyz.md5","r");
	fgets(md5str,33,fp);
	fclose(fp);
	//unlink("/tmp/xyz.md5");
	printf("MD5 of the file is '%s'\n",md5str);
	if (strcmp(md5str,pOTAHeader->MD5_str))
	{
		// ERROR MD5 NOT MATCH WHAT DO I HAVE TO DO WITH IT????

		return MLS_ERR_OTA_WRONGMD5;
	}	
	sprintf(filename,"tar -xzf %s -C /",TEMP_PATCH_FILENAME);
	system(filename); // overwrite everything from folder "/"	
	return MLS_SUCCESS;
}

mlsErrorCode_t OTA_WriteUpgrade(char* filepath, char* filename, char* md5string, unsigned short major_version, unsigned short minor_version ) // for debug only
{
	int retVal = MLS_SUCCESS;
	FILE *fp;
	char fullfilename[32];
	struct stat sb;
	char *buffer;
	int tmp_size = 0,remain=0;
	if(pOTAHeader == NULL)
		return MLS_ERROR;
	sprintf(fullfilename,"%s/%s",filepath,filename);
	if (stat(fullfilename,&sb))
	{

		return MLS_ERR_OPENFILE;
	}
	if (sb.st_size > mlsOTA_GetMaxOTAFileSize())
	{
	    return MLS_ERR_OPENFILE;
	}
	pOTAHeader->filesize = (unsigned long)sb.st_size;
	pOTAHeader->major_version = major_version;
	pOTAHeader->minor_version = minor_version;
	strcpy(pOTAHeader->MD5_str,md5string); // make sure md5string has 0 truncate
	memset(pOTAHeader->dummy,0,sizeof(pOTAHeader->dummy));

	
	fp = fopen(fullfilename,"rb");
	if (fp == NULL)
	{
		return MLS_ERR_OPENFILE;
	}
	
	buffer = malloc (SPIFLASH_BLOCKSIZE);
	if (buffer == NULL)
	{
		printf("MLSOTA: Cant malloc %d bytes\n",SPIFLASH_BLOCKSIZE);
		return MLS_ERROR;
	}
	
	remain = pOTAHeader->filesize;
	while(remain)
	{
		tmp_size = (remain > SPIFLASH_BLOCKSIZE)? SPIFLASH_BLOCKSIZE:remain;
#ifdef DEBUG
		printf("tmp_size = %d\n",tmp_size);
#endif
		if (fread(buffer,1,tmp_size,fp) != tmp_size)
		{
			printf("Cant read file %s\n",fullfilename);
			fclose(fp);
			free(buffer);
			return MLS_ERR_OPENFILE;
		
		}
		mlsflashWrite (mlsOTA_GetBlockOffset()*SPIFLASH_BLOCKSIZE+SPIFLASH_SECTORSIZE+(pOTAHeader->filesize - remain),(unsigned char*)buffer,tmp_size);
		remain -= tmp_size;
	}
	free(buffer);
	retVal = SaveOTAHeaderConfig();
	return retVal;
}

