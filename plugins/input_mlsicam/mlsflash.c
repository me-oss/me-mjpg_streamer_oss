/*
 * mlsflash.c
 *
 *  Created on: Aug 23, 2011
 *      Author: root
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/watchdog.h>
#include <semaphore.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include "mlsflash.h"
#include "sys_configs.h"
#include "mlsWifi.h"
#include "../../debug_version.h"

#define DEBUG

#define CAMERA_NAME	"Camera-"
#define SECTOR_SIZE 4096

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
#include "../../debug.h"
struct spi_parameter{
	unsigned int active_level:1;
	unsigned int lsb:1, tx_neg:1, rx_neg:1, divider:16;
	unsigned int sleep:4;
};

struct spi_data{
	unsigned int write_data;
	unsigned int read_data;
	unsigned int bit_len;
};

#define SPI_MAJOR		231

#define SPI_IOC_MAGIC			'u'
#define SPI_IOC_MAXNR			3

#define SPI_IOC_GETPARAMETER	_IOR(SPI_IOC_MAGIC, 0, struct usi_parameter *)
#define SPI_IOC_SETPARAMETER	_IOW(SPI_IOC_MAGIC, 1, struct usi_parameter *)
#define SPI_IOC_SELECTSLAVE	_IOW(SPI_IOC_MAGIC, 2, int)
#define SPI_IOC_TRANSIT			_IOW(SPI_IOC_MAGIC, 3, struct usi_data *)
//-- function return value
#define	   Successful  0
#define	   Fail        1

#define UINT32	unsigned int
#define UINT16	unsigned short
#define UINT8	unsigned char

static __inline unsigned short Swap16(unsigned short val)
{
    return (val<<8) | (val>>8);
}

#define SpiSelectCS0()				ioctl(spi_fd, SPI_IOC_SELECTSLAVE, 0)
#define SpiDeselectCS0()			ioctl(spi_fd, SPI_IOC_SELECTSLAVE, 1)
#define SpiDoTransit()				ioctl(spi_fd, SPI_IOC_TRANSIT, &gdata)
#define SpiSetBitLen(x)				gdata.bit_len = x
#define SpiSetData(x)					gdata.write_data = x
#define SpiGetData()					gdata.read_data

#define FLASH_BUFFER_SIZE	2048

UINT8 Flash_Buf[FLASH_BUFFER_SIZE];
UINT8 ReadBackBuffer[FLASH_BUFFER_SIZE];


static int spi_fd=-1;
unsigned int flashsize=0;
struct spi_data gdata;
static int is_init_already=0;
static int semid=-1;
union semun {
            int val;
            struct semid_ds *buf;
            unsigned short  *array;
} arg;
int exit_reboot()
{
	int timeout = 0;
	int fd = open("/dev/watchdog", O_WRONLY);
	if (fd == -1) {
		printf("open watchdog error\n");
		return -1;
	}
	ioctl(fd, WDIOC_SETTIMEOUT,&timeout );
	printf("REBOOT NOW...\n");
	sleep(1108);
	return 0;
}
int spiCheckBusy()
{
	// check status
	SpiSelectCS0();	// CS0

	// status command
	SpiSetData(0x05);
	SpiSetBitLen(8);
	SpiDoTransit();

	// get status
	while(1)
	{
		SpiSetData(0xff);
		SpiSetBitLen(8);
		SpiDoTransit();
		if (((SpiGetData() & 0xff) & 0x01) != 0x01)
			break;
	}
	SpiDeselectCS0();	// CS0
	return Successful;
}

/*
	addr: memory address
	len: byte count
	buf: buffer to put the read back data
*/
int spiRead(UINT32 addr, UINT32 len, UINT8 *buf)
{
	int volatile i;

    spiCheckBusy();

	SpiSelectCS0();	// CS0

	// read command
	SpiSetData(03);
	SpiSetBitLen(8);
	SpiDoTransit();

	// address
	SpiSetData(addr);
	SpiSetBitLen(24);
	SpiDoTransit();

	// data
	for (i=0; i<len; i++)
	{
		SpiSetData(0xff);
		SpiSetBitLen(8);
		SpiDoTransit();
		*buf++ = SpiGetData() & 0xff;
	}

	SpiDeselectCS0();	// CS0

	return Successful;
}

int spiReadFast(UINT32 addr, UINT32 len, UINT8 *buf)
{
	int volatile i;

    spiCheckBusy();

	SpiSelectCS0();	// CS0

	// read command
	SpiSetData(0x0b);
	SpiSetBitLen(8);
	SpiDoTransit();

	// address
	SpiSetData(addr);
	SpiSetBitLen(24);
	SpiDoTransit();

	// dummy byte
	SpiSetData(0xff);
	SpiSetBitLen(8);
	SpiDoTransit();

	// data
	for (i=0; i<len; i++)
	{
		SpiSetData(0xff);
		SpiSetBitLen(8);
		SpiDoTransit();
		*buf++ = SpiGetData() & 0xff;
	}

	SpiDeselectCS0();	// CS0

	return Successful;
}

int spiReadJEDEC(UINT8 *MfID, UINT8 *MemID, UINT8 *CapacityID)
{
	//int volatile i;

    spiCheckBusy();

	SpiSelectCS0();	// CS0

	// read command
	SpiSetData(0x9f);
	SpiSetBitLen(8);
	SpiDoTransit();

	SpiSetData(0xff);
	SpiSetBitLen(8);
	SpiDoTransit();
	*MfID = SpiGetData() & 0xff;


	SpiSetData(0xff);
	SpiSetBitLen(8);
	SpiDoTransit();
	*MemID = SpiGetData() & 0xff;


	SpiSetData(0xff);
	SpiSetBitLen(8);
	SpiDoTransit();
	*CapacityID = SpiGetData() & 0xff;

	SpiDeselectCS0();	// CS0

	return Successful;
    
}

int spiWriteEnable()
{
	SpiSelectCS0();// CS0

	SpiSetData(0x06);
	SpiSetBitLen(8);
	SpiDoTransit();

	SpiDeselectCS0();	// CS0

	return Successful;
}

int spiWriteDisable()
{
	SpiSelectCS0();	// CS0

	SpiSetData(0x04);
	SpiSetBitLen(8);
	SpiDoTransit();

	SpiDeselectCS0();	// CS0

	return Successful;
}

/*
	addr: memory address
	len: byte count
	buf: buffer with write data
*/
int spiWrite(UINT32 addr, UINT32 len, unsigned short *buf)
{
	int volatile count=0, page, i;


    spiCheckBusy();
	count = len / 256;
	if ((len % 256) != 0)
		count++;

	for (i=0; i<count; i++)
	{
		// check data len
		if (len >= 256)
		{
			page = 128;
			len = len - 256;
		}
		else
			page = len/2;

		spiWriteEnable();

		SpiSelectCS0();	// CS0

		// write command
		SpiSetData(0x02);
		SpiSetBitLen(8);
		SpiDoTransit();

		// address
		SpiSetData(addr+i*256);
		SpiSetBitLen(24);
		SpiDoTransit();
		// write data
		while (page-- > 0)
		{
			SpiSetData(Swap16(*buf++));
			SpiSetBitLen(16);
			SpiDoTransit();
		}

		SpiDeselectCS0();	// CS0

		// check status
		spiCheckBusy();
	}

	return Successful;
}

/*	mls@dev03 20110823
	addr: memory address
	len: byte count
	buf: char buffer with write data
*/
int spiWriteChar(UINT32 addr, UINT32 len, UINT8 *buf)
{
	int volatile count=0, page, i;

	spiCheckBusy();
	count = len / 256;
	if ((len % 256) != 0)
		count++;

	for (i=0; i<count; i++)
	{
		// check data len
		if (len >= 256)
		{
			page = 256;
			len = len - 256;
		}
		else
			page = len;

		spiWriteEnable();

		SpiSelectCS0();	// CS0

		// write command
		SpiSetData(0x02);
		SpiSetBitLen(8);
		SpiDoTransit();

		// address
		SpiSetData(addr+i*256);
		SpiSetBitLen(24);
		SpiDoTransit();
		// write data
		while (page-- > 0)
		{
			SpiSetData(*buf++);
			SpiSetBitLen(8);
			SpiDoTransit();
		}

		SpiDeselectCS0();	// CS0

		// check status
		spiCheckBusy();
	}

	return Successful;
}


int spiEraseSector(UINT32 addr, UINT32 secCount)
{
	int volatile i;

    spiCheckBusy();
	for (i=0; i<secCount; i++)
	{
		spiWriteEnable();

		SpiSelectCS0();	// CS0

		// erase command
		SpiSetData(0x20);
		SpiSetBitLen(8);
		SpiDoTransit();

		// address
		SpiSetData(addr+i*SECTOR_SIZE);
		SpiSetBitLen(24);
		SpiDoTransit();

		SpiDeselectCS0();	// CS0

		// check status
		spiCheckBusy();
	}
	return Successful;
}

int spiEraseAll()
{
	spiWriteEnable();

	SpiSelectCS0();// CS0

	SpiSetData(0xc7);
	SpiSetBitLen(8);
	SpiDoTransit();

	SpiDeselectCS0();	// CS0

	// check status
	spiCheckBusy();

	return Successful;
}

unsigned short spiReadID()
{
	unsigned short volatile id;

	SpiSelectCS0();	// CS0

	// command 8 bit
	SpiSetData(0x90);
	SpiSetBitLen(8);
	SpiDoTransit();

	// address 24 bit
	SpiSetData(0x000000);
	SpiSetBitLen(24);
	SpiDoTransit();

	// data 16 bit
	SpiSetData(0xffff);
	SpiSetBitLen(16);
	SpiDoTransit();
	id = SpiGetData() & 0xffff;

	SpiDeselectCS0();	// CS0

	return id;
}

UINT8 spiStatusRead()
{
	UINT32 status;
	SpiSelectCS0();		// CS0

	// status command
	SpiSetData(0x05);
	SpiSetBitLen(8);
	SpiDoTransit();

	// get status
	SpiSetData(0xff);
	SpiSetBitLen(8);
	SpiDoTransit();
	status = SpiGetData() & 0xff;

	SpiDeselectCS0();	// CS0
	return status;
}

int spiStatusWrite(UINT8 data)
{
	spiWriteEnable();

	SpiSelectCS0();	// CS0

	// status command
	SpiSetData(0x01);
	SpiSetBitLen(8);
	SpiDoTransit();

	// write status
	SpiSetData(data);
	SpiSetBitLen(8);
	SpiDoTransit();

	SpiDeselectCS0();	// CS0

	// check status
	spiCheckBusy();

	return Successful;
}

int spiFlashTest()
{
  int volatile i;

	for (i=0; i<FLASH_BUFFER_SIZE; i++)
		Flash_Buf[i] = i;

	printf("flash ID [0x%04x]\n", spiReadID());
	printf("flash status [0x%02x]\n", spiStatusRead());

//	printf("erase all\n");
//	spiEraseAll();
	printf("erase sector\n");
	i = FLASH_BUFFER_SIZE / 256;
	if ((FLASH_BUFFER_SIZE % 256) != 0)
		i++;
	spiEraseSector(0, i);

	printf("write\n");
	spiWrite(0, FLASH_BUFFER_SIZE, (unsigned short *)Flash_Buf);

	printf("read\n");
	spiRead(0, FLASH_BUFFER_SIZE, ReadBackBuffer);
//	spiReadFast(0, FLASH_BUFFER_SIZE, ReadBackBuffer);

	printf("compare\n");
	for (i=0; i<FLASH_BUFFER_SIZE; i++)
	{
		if (ReadBackBuffer[i] != Flash_Buf[i])
		{
			printf("error! [%d]->wrong[%x] / correct[%x]\n", i, ReadBackBuffer[i], Flash_Buf[i]);
			break;
		}
	}

	printf("finish..\n");
	return 0;
}


/////////////////////////////////////////////////////////////////////////////////////////////////

mlsErrorCode_t mlsflashInit_SemInit()
{

    semid = semget(0x100,1, IPC_CREAT | IPC_EXCL | 0666);
    if (semid >= 0) // new one
    {
#ifdef DEBUG
        printf("New Semaphore Create\n");
#endif    
        arg.val=1;
        if (semctl(semid, 0, SETVAL, arg) == -1) {
            perror("semctl");
        }
    }
    else if (errno == EEXIST)
    {
#ifdef DEBUG
        printf("Retrieve the old Semaphore\n");
#endif    

        semid = semget(0x100, 1, 0); /* get the id */
        if (semid < 0)
        {
            perror("semget failed Reason:");
        }
    }
    else
    {
      perror("semget failed Reason:");
    }
    return MLS_SUCCESS;
}

mlsErrorCode_t mlsflashInit()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	struct spi_parameter para;
    //struct sembuf sb;
    //struct semid_ds buf;
    //union semun arg;
    int retry_times = 0;
 
    if (semid < 0)
    {
	mlsflashInit_SemInit();
    }
    if (is_init_already)
    {
        return retVal;
    }
    retry_times = 0;
	do{
	    spi_fd = open("/dev/spi0", O_RDWR);
	    if ( spi_fd < 0)
	    {
            printf("Open spi error\n");
            printf("Flash Init Fail %d (%d)\n",spi_fd,getpid());
            //retVal = MLS_ERROR;
            is_init_already = 0;
            //return retVal;
            sleep(2);
	    }
        retry_times++;
	if(retry_times > 8)
	{

#ifndef INTERNAL_VERSION_TESTING
        
		//dump_crash_data();
        	exit_reboot();
    	    }
#else
	    dump_crash_data();
	    system("./led_blink 200 300 &");
	    /* flat beep tone */
	    system("./beep_arm 10000 2000 200 800 &");
	    while(1)
	    {
		sleep(3);
	    }
	    }
#endif
	}while(spi_fd < 0);

	ioctl(spi_fd, 100, (unsigned long)spi_fd);  // Mark 01.14, Michael

	para.divider = 30;				// SCK = PCLK/16 (20MHz)
	para.active_level = 0;	// CS active low
	para.tx_neg = 1;				// Tx: falling edge, Rx: rising edge
	para.rx_neg = 0;
	para.lsb = 0;
	para.sleep = 0;

	ioctl(spi_fd, SPI_IOC_SETPARAMETER, &para);
	ioctl(spi_fd, SPI_IOC_GETPARAMETER, &para);

//	printf("para.lsb : %d\n", para.lsb);
//	printf("para.tx_neg : %d\n", para.tx_neg);
//	printf("para.rx_neg : %d\n", para.rx_neg);
//	printf("para.divider : %d\n", para.divider);
//	printf("para.active_level : %d\n", para.active_level);
//	printf("para.sleep : %d\n", para.sleep);
    is_init_already = 0;
#ifdef DEBUG
    printf("Finish init Flash\n");
#endif
    printf("Flash Init %d (%d)\n",spi_fd,getpid());
    return retVal;
}


mlsErrorCode_t mlsflashDeInit()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	int ret;
    printf("Flash DeInit %d (%d)\n",spi_fd,getpid());    
    if (spi_fd >= 0)
    {
	ret = close(spi_fd);
	if (ret != 0)
	{
	    printf("CANT CLOSE SPI_FD.WHYYYYYYYYY********\n");
	    while(1)
	    {
		sleep(1);
	    }
	}
	spi_fd = -1;
    }
#ifdef DEBUG
    printf("Finish deinit Flash\n");    
#endif

    is_init_already = 0;
	return retVal;
}

mlsErrorCode_t mlsflashRead(unsigned int address,unsigned char* bufferRead, unsigned int lenRead)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
    struct sembuf sb = {0, -1, 0};  /* set to allocate resource */

#ifdef DEBUG
    printf("Before sem wait\n");
#endif    


    if (semid < 0)
    {
	mlsflashInit_SemInit();
    }

   if (semop(semid, &sb, 1) == -1) {
            perror("semop");
    }
#ifdef DEBUG
    printf("After sem wait\n");    
#endif
	retVal = mlsflashInit();
	if(retVal != MLS_SUCCESS)
	{
		printf("spi for flash init fail\n");
    		sb.sem_op = 1; /* free resource */
    		if (semop(semid, &sb, 1) == -1) {
        	    perror("semop");
    		}

		return retVal;
	}

	spiRead(address, lenRead, bufferRead);
#ifdef DEBUG
    printf("Before sem post\n");
#endif    
	mlsflashDeInit();
     sb.sem_op = 1; /* free resource */
        if (semop(semid, &sb, 1) == -1) {
            perror("semop");
        }
#ifdef DEBUG
    printf("After sem post\n");
#endif        


	return retVal;
}


mlsErrorCode_t mlsflashWrite(unsigned int address,unsigned char* bufferWrite, unsigned int lenWrite)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	int nsect;
    struct sembuf sb = {0, -1, 0};  /* set to allocate resource */


    if (semid < 0)
    {
	mlsflashInit_SemInit();
    }

	nsect = lenWrite/(SECTOR_SIZE);
	if (lenWrite%SECTOR_SIZE)
	{
		nsect += 1;
	}

#ifdef DEBUG
    printf("Before sem wait\n");
#endif    

   if (semop(semid, &sb, 1) == -1) {
            perror("semop");
    }
#ifdef DEBUG
    printf("After sem wait\n");    
#endif

    	retVal = mlsflashInit();

	if(retVal != MLS_SUCCESS)
	{
		printf("spi for flash init fail\n");
    		sb.sem_op = 1; /* free resource */
    		if (semop(semid, &sb, 1) == -1) {
        	    perror("semop");
    		}

		return retVal;
	}


	spiEraseSector(address, nsect);

	spiWriteChar(address, lenWrite,(unsigned char*)bufferWrite);
#ifdef DEBUG
    printf("Before sem post\n");
#endif    

	mlsflashDeInit();
   // sem_post(my_semaphore);
     sb.sem_op = 1; /* free resource */
        if (semop(semid, &sb, 1) == -1) {
            perror("semop");
        }
#ifdef DEBUG
    printf("After sem post\n");
#endif        

	return retVal;
}

/*
 *
 */
mlsErrorCode_t mlsflashEraseSectors(unsigned int address, int nsectors)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
    struct sembuf sb = {0, -1, 0};  /* set to allocate resource */
   // sem_wait(my_semaphore);

    if (semid < 0)
    {
	mlsflashInit_SemInit();
    }

    if (semop(semid, &sb, 1) == -1) {
            perror("semop");
    }  
	retVal = mlsflashInit();
	if(retVal != MLS_SUCCESS)
	{
		printf("spi for flash init fail\n");
     sb.sem_op = 1; /* free resource */
	
        if (semop(semid, &sb, 1) == -1) {
            perror("semop");
        }

		return retVal;
	}

	spiEraseSector(address, nsectors);
  //  sem_post(my_semaphore);
	mlsflashDeInit();

     sb.sem_op = 1; /* free resource */
	
        if (semop(semid, &sb, 1) == -1) {
            perror("semop");
        }
	return retVal;
}

unsigned int mlsflashGetFlashSize()
{
    char signature[32];
    mlsErrorCode_t retVal = MLS_SUCCESS;


    if (semid < 0)
    {
	mlsflashInit_SemInit();
    }

    if (flashsize == 0)
    {
    
	unsigned char MfgID, MemTypeID, CapacityID;
	struct sembuf sb = {0, -1, 0};  /* set to allocate resource */	
	
   // sem_wait(my_semaphore);
    if (semop(semid, &sb, 1) == -1) {
            perror("semop");
    }  

	retVal = mlsflashInit();
	if(retVal != MLS_SUCCESS)
	{
		printf("spi for flash init fail\n");
     sb.sem_op = 1; /* free resource */
	
        if (semop(semid, &sb, 1) == -1) {
            perror("semop");
        }

		return retVal;
	}

	spiReadJEDEC(&MfgID, &MemTypeID, &CapacityID);
	printf("Flash Read JEDEC (%02X-%02X-%02X)\n",MfgID, MemTypeID, CapacityID);
	printf("SPI ID = %04X\n",spiReadID());
	if (CapacityID == 0x16 )
	{
	    printf("********************This is 4MB Board******************\n");
	    flashsize = 4*1024 * 1024;
	}
	else
	{
	    printf("********************This is 8MB Board******************\n");
	    flashsize = 8 * 1024 * 1024;
	}
	
	mlsflashDeInit();
         sb.sem_op = 1; /* free resource */
	

        if (semop(semid, &sb, 1) == -1) {
            perror("semop");
        }

    
    }
    return flashsize;
}



/////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////// setting configuration in SPI FLASH/////////////////////////////////////////
settingConfig_st * settingConfig = NULL;
#define FLASH_SETTINGCONFIG_ADDRESS (SPIFLASH_SIZE-SPIFLASH_BLOCK64KB)

mlsErrorCode_t mlsSettingInit()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	GetSettingConfig(&settingConfig);
	return retVal;
}

mlsErrorCode_t mlsGetConfig(settingConfig_st *reVal)
{
	if(reVal == NULL || settingConfig == NULL){
		return MLS_ERROR;
	}
	memcpy(reVal,settingConfig,sizeof(settingConfig_st));
	return MLS_SUCCESS;
}
mlsErrorCode_t mlsSettingDeInit()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	return retVal;
}

mlsErrorCode_t mlsSettingSaveCameraName(char *name)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	if(name == NULL || strlen(name) == 0 || strlen(name)>12 || settingConfig == NULL)
	{
		printf("Camera name is invalid\n");
		return MLS_ERROR;
	}
	strcpy(settingConfig->cameraName,name);
	retVal = SaveSettingConfig();
	return retVal;
}
mlsErrorCode_t mlsSettingSaveBrightness(char brightnessValue)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	if(settingConfig == NULL)
	{
		printf("SettingConfig null\n");
		return MLS_ERROR;
	}
	settingConfig->brightnessValue = brightnessValue;

	retVal = SaveSettingConfig();

	return retVal;
}
mlsErrorCode_t mlsSettingSaveContrast(char contrastValue)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	if(settingConfig == NULL)
	{
		printf("SettingConfig null\n");
		return MLS_ERROR;
	}
	settingConfig->contrastValue = contrastValue;

	retVal = SaveSettingConfig();


	return retVal;
}
mlsErrorCode_t mlsSettingSaveVoxStatus(char voxStatus)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	if(settingConfig == NULL)
	{
		printf("SettingConfig null\n");
		return MLS_ERROR;
	}
	settingConfig->voxStatus = voxStatus;

	retVal = SaveSettingConfig();

	return retVal;
}
mlsErrorCode_t mlsSettingSaveVoxThreshold(signed char threshold)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	if(settingConfig == NULL)
	{
		printf("SettingConfig null\n");
		return MLS_ERROR;
	}
	settingConfig->threshold = threshold;
	retVal = SaveSettingConfig();
	return retVal;
}
mlsErrorCode_t mlsSettingSaveVGAMode(char vgaMode)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	if(settingConfig == NULL)
	{
		printf("SettingConfig null\n");
		return MLS_ERROR;
	}
	settingConfig->vgaMode = vgaMode;

	retVal = SaveSettingConfig();

	return retVal;
}
mlsErrorCode_t mlsSettingSaveCameraView()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	if(settingConfig == NULL)
	{
		printf("SettingConfig null\n");
		return MLS_ERROR;
	}
	if(settingConfig->cameraView == Normal)
	{
		settingConfig->cameraView = Flipup;
	}
	else
	{
		settingConfig->cameraView = Normal;
	}

	retVal = SaveSettingConfig();

	return retVal;
}
mlsErrorCode_t mlsSettingSaveConfig(settingConfig_st *Config)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	retVal = SaveSettingConfig();
	return retVal;
}
mlsErrorCode_t mlsSettingSaveVolume(char volume)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	if(settingConfig == NULL)
	{
		printf("SettingConfig null\n");
		return MLS_ERROR;
	}
	settingConfig->volume = volume;
	retVal = SaveSettingConfig();
	return retVal;
}
mlsErrorCode_t mlsSettingSaveTempAlert(unsigned int min, unsigned int max)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	if(settingConfig == NULL)
	{
		printf("SettingConfig null\n");
		return MLS_ERROR;
	}
	if(min > max)
		retVal = MLS_ERROR;
	else
	{
		settingConfig->extra.temp_alert.min = min;
		settingConfig->extra.temp_alert.max = max;
		//Save into flash
		retVal = SaveSettingConfig();
	}
	return retVal;
}

void dump_crash_data()
{
    system("/mlsrb/process_dump.sh");
}
