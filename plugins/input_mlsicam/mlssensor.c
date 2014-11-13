/*
 * mlssensor.c
 *
 *  Created on: Jul 8, 2011
 *      Author: root
 */


#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>



#include "mlsimage.h"
#include "mlssensor.h"

#include "../../debug.h"
/*
 *
 */
 
typedef struct sensor_reg_struct_t
{
    char address;
    char val;
}sensor_reg_struct_t;

UInt8 mlssensor_is_flipup_mirror(void)
{
	UInt8 flipup;
	if(ioctl(rapbitImgConfig.vinfd, VIDEOIN_SENSOROV76xx_CHECKFLIPUP, &flipup) < 0)
	{
		fprintf(stderr,"flip up error,ioctl VIDEOIN_SENSOROV76xx_CHECKFLIPUP failed:%d\n",errno);
		return 0;
	}
	return flipup;
}

#define SENSOR_RETRIES 5
mlsErrorCode_t mlssensorInit(int image_width, int image_height)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	static char startFlag = 1;
	int retries = SENSOR_RETRIES;
	while (retries--)
	{
	    rapbitImgConfig.vinfd = open(VIDEOIN_DEVICE, O_RDWR);
	    if ( rapbitImgConfig.vinfd >= 0 )
	    {
		break;
	    }
	    printf("Retries init Sensor\n");
	    sleep(1);
	}
	if (retries < 0)
	{
		printf("open videoin device error\n");
		return MLS_ERROR;
	}
	
	
	//mls@dev03 20110730 set default brightness and contrast
	if (startFlag)
	{
		mlssensorSetContrast(0x40);
		mlssensorSetBrightness(0x00);
		startFlag = 0;
	}
	//mls@dev03 20110720
	/* 1. VIDEOIN_S_VIEW_WINDOW */
	/*------------------------------------------------------------*/
	memset(&rapbitImgConfig.videoin_window, 0, sizeof(videoin_window_t));
	rapbitImgConfig.videoin_window.ViewStartPos.u32PosX = (LCM_WIDTH - 320)/2;//(LCM_WIDTH-320)/2;
	rapbitImgConfig.videoin_window.ViewStartPos.u32PosY = (LCM_HEIGHT - 240)/2;//(LCM_HEIGHT-240)/2;
	rapbitImgConfig.videoin_window.ViewWindow.u32ViewWidth = LCM_WIDTH;
	rapbitImgConfig.videoin_window.ViewWindow.u32ViewHeight = LCM_HEIGHT;
	if((ioctl(rapbitImgConfig.vinfd, VIDEOIN_S_VIEW_WINDOW, &rapbitImgConfig.videoin_window)) < 0)
	{
		fprintf(stderr,"set videoin_param VIDEOIN_S_VIEW_WINDIW failed:%d\n",errno);
		retVal = MLS_ERROR;
		return retVal;
	}


	/* 2. VIDEOIN_S_PARAM */
//	mls@dev03 change to planar 420 format
	memset(&rapbitImgConfig.videoin_param, 0, sizeof(videoin_param_t));
	rapbitImgConfig.videoin_param.format.input_format = 0;//eDRVVIDEOIN_IN_YUV422;
	rapbitImgConfig.videoin_param.format.output_format = 0;//eDRVVIDEOIN_OUT_YUV422;
	rapbitImgConfig.videoin_param.polarity.bVsync = TRUE;
	rapbitImgConfig.videoin_param.polarity.bHsync = FALSE;
	rapbitImgConfig.videoin_param.polarity.bPixelClk = TRUE;
	rapbitImgConfig.videoin_param.resolution.x   = LCM_WIDTH;//LCM_WIDTH;
	rapbitImgConfig.videoin_param.resolution.y   = LCM_HEIGHT;//LCM_HEIGHT;
	if((ioctl(rapbitImgConfig.vinfd, VIDEOIN_S_PARAM, &rapbitImgConfig.videoin_param)) < 0)
	{
		fprintf(stderr,"set videoin_param VIDEOIN_S_PARAM failed:%d\n",errno);
		retVal = MLS_ERROR;
		return retVal;
	}

	/* 2.1 */
	ioctl(rapbitImgConfig.vinfd, VIDEOIN_PREVIEW_PIPE_CTL, TRUE);

	/* 3. VIDEOIN_S_JPG_PARAM */
	//mls@dev03 20110719
//	ioctl(rapbitImgConfig.vinfd, VIDEOIN_PREVIEW_PIPE_CTL, TRUE);
	memset(&rapbitImgConfig.tEncode, 0, sizeof(videoin_encode_t));
	rapbitImgConfig.tEncode.u32Width = image_width;
	rapbitImgConfig.tEncode.u32Height = image_height;
	rapbitImgConfig.tEncode.u32Format = 1;//YUV420
	rapbitImgConfig.tEncode.u32Enable = 1; //enable planar pile
	if((ioctl(rapbitImgConfig.vinfd, VIDEOIN_S_JPG_PARAM, &rapbitImgConfig.tEncode)) < 0)
	{
		fprintf(stderr,"set videoin_param VIDEOIN_S_JPG_PARAM failed:%d\n",errno);
		retVal = MLS_ERROR;
		return retVal;
	}

	/* 4. VIDEOIN_ENCODE_PIPE_CTL */
	if (ioctl(rapbitImgConfig.vinfd, VIDEOIN_ENCODE_PIPE_CTL, TRUE)<0)
	{
		fprintf(stderr,"set videoin_param VIDEOIN_ENCODE_PIPE_CTL failed:%d\n",errno);
		retVal = MLS_ERROR;
		return retVal;
	}

	return retVal;
}

/*
 *
 */
mlsErrorCode_t mlssensorDeInit()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	close(rapbitImgConfig.vinfd);

	return retVal;
}

/*
 * set resolution
 */
mlsErrorCode_t mlssensorSetResolution(int image_width, int image_height)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	memset(&rapbitImgConfig.tEncode, 0, sizeof(videoin_encode_t));
	rapbitImgConfig.tEncode.u32Width = image_width;
	rapbitImgConfig.tEncode.u32Height = image_height;
	rapbitImgConfig.tEncode.u32Format = 1;//YUV420
	rapbitImgConfig.tEncode.u32Enable = 1; //enable planar pile
	if((ioctl(rapbitImgConfig.vinfd, VIDEOIN_S_JPG_PARAM, &rapbitImgConfig.tEncode)) < 0)
	{
		fprintf(stderr,"set videoin_param VIDEOIN_S_JPG_PARAM failed:%d\n",errno);
		retVal = MLS_ERROR;
		return retVal;
	}

	return retVal;
}

/*
 *
 */
mlsErrorCode_t mlssensorSetContrast(char value)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	if((ioctl(rapbitImgConfig.vinfd, VIDEOIN_SENSOROV76xx_SETCONTRAST, value)) < 0)
	{
		fprintf(stderr,"set videoin_param VIDEOIN_SENSOROV76xx_SETCONTRAST failed:%d\n",errno);
		retVal = MLS_ERROR;
	}

	return retVal;
}
/*
 * add by mlsdev008 8/12/2011
 */
mlsErrorCode_t mlssensorFlipup()
{
//	printf("execute command 'flipup'\n");
	if(ioctl(rapbitImgConfig.vinfd, VIDEOIN_SENSOROV76xx_FLIPUP, NULL) < 0)
	{
		fprintf(stderr,"flip up error,ioctl VIDEOIN_SENSOROV76xx_FLIPUP failed:%d\n",errno);
		return MLS_ERROR;
	}
	return MLS_SUCCESS;
}

mlsErrorCode_t mlssensorIRLED(int onoff)
{
//	printf("execute command 'flipup'\n");
	if(ioctl(rapbitImgConfig.vinfd, VIDEOIN_SENSOROV76xx_IRLED, onoff) < 0)
	{
		fprintf(stderr,"flip up error,ioctl VIDEOIN_SENSOROV76xx_IRLED failed:%d\n",errno);
		return MLS_ERROR;
	}
	return MLS_SUCCESS;
}


mlsErrorCode_t mlssensorSetReg(char address,char value)
{
//	printf("execute command 'flipup'\n");
	sensor_reg_struct_t reg_struct;
	reg_struct.address = address;
	reg_struct.val = value;
	if(ioctl(rapbitImgConfig.vinfd, VIDEOIN_SENSOROV76xx_SETREG, &reg_struct) < 0)
	{
		fprintf(stderr,"flip up error,ioctl VIDEOIN_SENSOROV76xx_SETREG failed:%d\n",errno);
		return MLS_ERROR;
	}
	return MLS_SUCCESS;
}


mlsErrorCode_t mlssensorGetReg(char address, char* value)
{
//	printf("execute command 'flipup'\n");
	sensor_reg_struct_t reg_struct;
	reg_struct.address = address;
	reg_struct.val = 0;

	if(ioctl(rapbitImgConfig.vinfd, VIDEOIN_SENSOROV76xx_GETREG, &reg_struct) < 0)
	{
		fprintf(stderr,"flip up error,ioctl VIDEOIN_SENSOROV76xx_GETREG failed:%d\n",errno);
		return MLS_ERROR;
	}
	*value = reg_struct.val;
	return MLS_SUCCESS;
}


/*
 *
 */
mlsErrorCode_t mlssensorSetBrightness(char value)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	if((ioctl(rapbitImgConfig.vinfd, VIDEOIN_SENSOROV76xx_SETBRIGHTNESS, value)) < 0)
	{
		fprintf(stderr,"set videoin_param VIDEOIN_SENSOROV76xx_SETBRIGHTNESS failed:%d\n",errno);
		retVal = MLS_ERROR;
	}

	return retVal;
}


/*
 *
 */
mlsErrorCode_t mlssensorCheckPID()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	char status;
	int vinfd;

	/* open videoin device */
	vinfd = open(VIDEOIN_DEVICE, O_RDWR);
	if ( vinfd < 0 )
	{
		printf("open videoin device error\n");
		retVal = MLS_ERROR;
		return retVal;
	}


	/* check camera PID */
	printf("Check camera PID...\n");
	fflush(stdout);
	if((ioctl(vinfd, VIDEOIN_SENSOROV76xx_CHECKPID, &status)) < 0)
	{
		fprintf(stderr,"set videoin_param VIDEOIN_SENSOROV76xx_SETBRIGHTNESS failed:%d\n",errno);
		retVal = MLS_ERROR;
	}

	if(status == 0)
	{
		printf("Read camera PID fail because of HW or i2c communication\n");
		retVal = MLS_ERR_OV_PID;
	}

	/* close videoin device */
	close(vinfd);

	return retVal;
}
