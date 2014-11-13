/*
 * mlsimage.c
 *
 *  Created on: Jul 6, 2011
 *      Author: mls@dev03
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <asm/ioctl.h>
#include <errno.h>


#include "mlsimage.h"
#include "../../debug.h"
/*
 *
 */
mlsErrorCode_t mlsimage_SetResolution(icam_resolution_t resolutionMode)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
#if 0
	retVal = mlsimageDeInit();
	usleep(500*1000);
	if(retVal == MLS_SUCCESS)
	{
		retVal = mlsimageInit(resolutionMode);
	}
#else
	switch(resolutionMode)
	{
	case icam_res_vga:
		/* init sensor */
		retVal = mlssensorSetResolution(640,480);
		/* init jpeg codec */
		if(retVal == MLS_SUCCESS)
		{
			retVal = mlsjpegSetResolution(640, 480);
		}
	break;
	case icam_res_qqvga:
		/* init sensor */
		retVal = mlssensorSetResolution(160,120);
		/* init jpeg codec */
		if(retVal == MLS_SUCCESS)
		{
			retVal = mlsjpegSetResolution(160,120);
		}
	break;
	default://case icam_res_qvga
		/* init sensor */
		retVal = mlssensorSetResolution(320,240);
		/* init jpeg codec */
		if(retVal == MLS_SUCCESS)
		{
			retVal = mlsjpegSetResolution(320,240);
		}
	break;
	}
#endif

	return retVal;
}
/*
 *
 */
mlsErrorCode_t mlsimageInit(icam_resolution_t image_resolution)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	//init value for image_size
//	if(image_resolution != VGA && image_resolution != QVGA && image_resolution != QQVGA)
//		image_resolution = QVGA;

	switch(image_resolution)
	{
	case icam_res_vga:
		/* init sensor */
		retVal = mlssensorInit(640,480);
		/* init jpeg codec */
		if(retVal == MLS_SUCCESS)
		{
			retVal = mlsjpegInit(640, 480);
		}
	break;
	case icam_res_qqvga:
		/* init sensor */
		retVal = mlssensorInit(160,120);
		/* init jpeg codec */
		if(retVal == MLS_SUCCESS)
		{
			retVal = mlsjpegInit(160,120);
		}
	break;
	default://case icam_res_qvga
		/* init sensor */
		retVal = mlssensorInit(320,240);
		/* init jpeg codec */
		if(retVal == MLS_SUCCESS)
		{
			retVal = mlsjpegInit(320,240);
		}
	break;
	}
	return retVal;
}

/*
 *
 */
mlsErrorCode_t mlsimageDeInit()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	/* deinit jpeg codec */
	retVal = mlsjpegDeInit();
	/* deinit sensor */
	retVal = mlssensorDeInit();
	return retVal;
}


void vin_dump_preview_framebuffer(unsigned char *pVideoinBuffer, unsigned long VinBufferSize,unsigned int filenum)
{
	char szFileName[256];
	char szPostName[128];
	FILE *fptr;

	szFileName[0]= 0;
	szPostName[0]= 0;
	sprintf(szFileName, "/tmp/Packet_YUV422_");
	sprintf(szPostName, "%d.jpg", filenum);
	strcat(szFileName, szPostName);
	fptr = fopen(szFileName, "wb" );
	fwrite(pVideoinBuffer, 1, VinBufferSize, fptr);
	fclose(fptr);
	sync();
}

void vin_dump_preview_framebuffer_yuv(unsigned char *pVideoinBuffer, unsigned long VinBufferSize,unsigned int filenum)
{
	char szFileName[256];
	char szPostName[128];
	FILE *fptr;

	szFileName[0]= 0;
	szPostName[0]= 0;
	sprintf(szFileName, "/tmp/Packet_YUV422_");
	sprintf(szPostName, "%d.yuv", filenum);
	strcat(szFileName, szPostName);
	fptr = fopen(szFileName, "wb" );
	fwrite(pVideoinBuffer, 1, VinBufferSize, fptr);
	fclose(fptr);
	sync();
}

mlsErrorCode_t mlsimageGetLastFrame(char * buffer, unsigned int *size)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	/* encode frame to jpeg by jpeg codec then copy to out buffer*/
	retVal = mlsjpegEncode(buffer,size);
	//usleep(1000*30);

	return retVal;
}
