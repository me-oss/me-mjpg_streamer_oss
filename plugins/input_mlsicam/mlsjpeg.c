/*
 * mlsjpeg.c
 *
 *  Created on: Jul 8, 2011
 *      Author: root
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>
#include <asm/ioctl.h>
#include <asm/arch/hardware.h>
//#include "jpegcodec.h"
#include <linux/vt.h>
#include <linux/kd.h>
#include <linux/fb.h>
#include <unistd.h>

#include "mlsimage.h"
#include "mlsjpeg.h"

#include "../../debug.h"
#define JPEGCODEC_DEVICE "/dev/video1"

/*
 *
 */
mlsErrorCode_t mlsjpegInit(int image_width,int image_height)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	/*1. open jpeg codec device file */
	rapbitImgConfig.jpgfd = open(JPEGCODEC_DEVICE, O_RDWR);
	if ( rapbitImgConfig.jpgfd < 0 )
	{
		printf("open jpeg codec device error\n");
		return 0;
	}


	/*2. allocate memory for JPEG engine */
	if((ioctl(rapbitImgConfig.jpgfd , JPEG_GET_JPEG_BUFFER, (__u32)&rapbitImgConfig.jpeg_buffer_size)) < 0)
	{
		close(rapbitImgConfig.jpgfd);
		printf("JPEG_GET_JPEG_BUFFER error \n");
		retVal = MLS_ERROR;
		return retVal;
	}

	/*3. map buffer to JPEC CODEC */
	rapbitImgConfig.pJpegBuffer = mmap(NULL, rapbitImgConfig.jpeg_buffer_size, PROT_READ|PROT_WRITE, MAP_SHARED, rapbitImgConfig.jpgfd, 0);

	if(rapbitImgConfig.pJpegBuffer == MAP_FAILED)
	{
		printf("JPEG Map Failed!\n");
		retVal = MLS_ERROR;
		return retVal;
	}
	else
	{
//		printf("\tGet memory from jpeg engine: 0x%X\n",rapbitImgConfig.jpeg_buffer_size);
	}

	/*4. initial parameters for jpeg codec */
	memset((void*)&rapbitImgConfig.jpeg_param, 0, sizeof(jpeg_param_t));
	rapbitImgConfig.jpeg_param.encode = 1;			/* Encode */
	rapbitImgConfig.jpeg_param.encode_width = image_width;		/* JPEG width */
	rapbitImgConfig.jpeg_param.encode_height = image_height;		/* JPGE Height */
	rapbitImgConfig.jpeg_param.encode_source_format = DRVJPEG_ENC_SRC_PLANAR;	/* Source is Planar Format */
	rapbitImgConfig.jpeg_param.encode_image_format = DRVJPEG_ENC_PRIMARY_YUV420; /*DRVJPEG_ENC_PRIMARY_YUV422 */
	rapbitImgConfig.jpeg_param.buffersize = 0;		/* only for continuous shot */
	rapbitImgConfig.jpeg_param.buffercount = 1;
	rapbitImgConfig.jpeg_param.scale = 0;		/* Scale function is disabled when scale 0 */


	/* Set operation property to JPEG engine */
	if((ioctl(rapbitImgConfig.jpgfd, JPEG_S_PARAM, &rapbitImgConfig.jpeg_param)) < 0)
	{
		fprintf(stderr,"set jpeg parame failed:%d\n",errno);
		retVal = MLS_ERROR;
		goto out;
	}


	return retVal;
out:
	close(rapbitImgConfig.jpgfd);

    return retVal;
}

/*
 *
 */
mlsErrorCode_t mlsjpegDeInit()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	if(rapbitImgConfig.pJpegBuffer)
	{
		munmap(rapbitImgConfig.pJpegBuffer, rapbitImgConfig.jpeg_buffer_size);
	}

	close(rapbitImgConfig.jpgfd);

    return retVal;
}

/*
 *
 */
mlsErrorCode_t mlsjpegSetResolution(int image_width,int image_height)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	/*4. initial parameters for jpeg codec */
	memset((void*)&rapbitImgConfig.jpeg_param, 0, sizeof(jpeg_param_t));
	rapbitImgConfig.jpeg_param.encode = 1;			/* Encode */
	rapbitImgConfig.jpeg_param.encode_width = image_width;		/* JPEG width */
	rapbitImgConfig.jpeg_param.encode_height = image_height;		/* JPGE Height */
	rapbitImgConfig.jpeg_param.encode_source_format = DRVJPEG_ENC_SRC_PLANAR;	/* Source is Planar Format */
	rapbitImgConfig.jpeg_param.encode_image_format = DRVJPEG_ENC_PRIMARY_YUV420; /*DRVJPEG_ENC_PRIMARY_YUV422 */
	rapbitImgConfig.jpeg_param.buffersize = 0;		/* only for continuous shot */
	rapbitImgConfig.jpeg_param.buffercount = 1;
	rapbitImgConfig.jpeg_param.scale = 0;		/* Scale function is disabled when scale 0 */


	/* Set operation property to JPEG engine */
	if((ioctl(rapbitImgConfig.jpgfd, JPEG_S_PARAM, &rapbitImgConfig.jpeg_param)) < 0)
	{
		fprintf(stderr,"set jpeg parame failed:%d\n",errno);
		retVal = MLS_ERROR;
	}

	return retVal;
}
/*
 *
 */
mlsErrorCode_t mlsjpegEncode(char *bufOut, unsigned int *sizeOut)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	int fd;
	int len, jpeginfo_size;
	jpeg_info_t jpeginfo;

	fd = rapbitImgConfig.jpgfd;

	//mls@dev03 20110808

	/*1. Set Encode Source from VideoIn */
	ioctl(rapbitImgConfig.jpgfd,JPEG_SET_ENC_SRC_FROM_VIN,NULL);


	jpeginfo_size = sizeof(jpeg_info_t);

	if((ioctl(fd, JPEG_FLUSH_CACHE, NULL)) < 0)
	{
		fprintf(stderr,"JPEG_FLUSH_CACHE failed:%d\n",errno);
		retVal = MLS_ERROR;
		goto out;
	}


	/*2. Trigger JPEG engine */

	if((ioctl(fd, JPEG_TRIGGER, NULL)) < 0)
	{
		fprintf(stderr,"JPEG_TRIGGER failed:%d\n",errno);
		retVal = MLS_ERROR;

		goto out;
	}

	//usleep(1000*10);
	/*3. Get JPEG Encode information */
	len = read(fd, &jpeginfo, jpeginfo_size);

	if(len<0) {
		fprintf(stderr, "No data can get\n");
		if(jpeginfo.state == JPEG_MEM_SHORTAGE)
			printf("Memory Stortage\n");
		retVal = MLS_ERROR;
		goto out;
	}

	/*4. copy jpeg image to out buffer */
	*sizeOut = jpeginfo.image_size[0];

//	printf("size jpeg %d\n",*sizeOut);

	memcpy(bufOut, rapbitImgConfig.pJpegBuffer,jpeginfo.image_size[0]);

out:


	return retVal;
}


