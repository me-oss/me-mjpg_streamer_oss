/*
 * mlsicam.c
 *
 *  Created on: Aug 13, 2010
 *      Author: mlslnx003
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>


#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/watchdog.h>
#include "sys_configs.h"
#include "../../debug.h"
#include "mlsicam.h"
#define Icam_FILENAME_CONFIG "/dev/mtdblock2"

#define TREK_GPIO_GET 0
#define TREK_GPIO_SET 1
#define TREK_GPIO_SET_DIRECTION 2
#define PANDA_GPA5_IRQE	4
#define PANDA_GPA5_IRQD	5

#define SENSOR_BRIGHT_LEVEL    9
#define SENSOR_CONSTRAST_LEVEL 9

static UInt8 _abSensorBrightValue[SENSOR_BRIGHT_LEVEL]   	= {255,223,192,159,0,31,63,95,127};
static UInt8 _abSensorContrastValue[SENSOR_CONSTRAST_LEVEL] = {0,32,64,96,128,160,192,224,255};

static mlsErrorCode_t _mlsicam_getMACaddress(char* pMACaddress)
{
    struct ifreq if_buffer;
    int if_socket;
    //char mac_buffer[20];

    memset(&if_buffer, 0x00, sizeof(if_buffer));
	if_socket=socket(PF_INET, SOCK_DGRAM, 0);

	strcpy(if_buffer.ifr_name, "ra0");
	ioctl(if_socket, SIOCGIFHWADDR, &if_buffer);
	close(if_socket);

	sprintf(pMACaddress,"%02X%02X%02X%02X%02X%02X",\
		if_buffer.ifr_hwaddr.sa_data[0],\
		if_buffer.ifr_hwaddr.sa_data[1],\
		if_buffer.ifr_hwaddr.sa_data[2],\
		if_buffer.ifr_hwaddr.sa_data[3],\
		if_buffer.ifr_hwaddr.sa_data[4],\
		if_buffer.ifr_hwaddr.sa_data[5]\
		);


	return MLS_SUCCESS;
}


static mlsErrorCode_t _mlsicam_InitGPIO(Int32 *gfd_gpio, icamMode_t *icamMode)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	Char gpioConfig[2] = {0,0};
	Int32 fd_gpio,iret;

	fd_gpio = open("/dev/trek_gpio", O_RDWR);

	if (fd_gpio <= 0)
	{

		retVal = MLS_ERR_Icam_OPEN_GPIODEV;
		return retVal;
	}

	gpioConfig[0] = 0;
	iret = ioctl(fd_gpio, TREK_GPIO_GET, &gpioConfig[0]);

	if (iret < 0)
	{
		retVal = MLS_ERROR;
	}

	if(iret & Icam_RESETSWITCH_MASK)
	{
		*icamMode = icamModeNormal;

	}
	else
	{
		*icamMode = icamModeReset;
		}


	gpioConfig[1] = 1<<4|1<<2;
	iret = ioctl(fd_gpio, TREK_GPIO_SET_DIRECTION, gpioConfig);
	if (iret < 0)
	{
		retVal = MLS_ERROR;
	}


	gpioConfig[1] = 0<<4|0<<2;
	iret = ioctl(fd_gpio, TREK_GPIO_SET, gpioConfig);

	if (iret < 0)
	{
		retVal = MLS_ERROR;
	}



	*gfd_gpio = fd_gpio;

	return retVal;
}
mlsErrorCode_t mlsicam_VoxSetAsFlash(settingConfig_st *pflashConfig)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	if(pflashConfig->voxStatus == Enable){

		char buf[32] = {'\0'};
		sprintf(buf,"%d",pflashConfig->threshold);
		retVal = mlsVoxSetThreshold(buf);
		if(retVal != MLS_SUCCESS){

			return retVal;
		}
		retVal = mlsVoxEnable(NULL);
		if(retVal != MLS_SUCCESS){

			return retVal;
		}
	}
	else{
		//disable VOX trigger
		int voxStatus = 0;
		retVal = mlsVoxGetStatus(&voxStatus);
		if(retVal != MLS_SUCCESS){

			return retVal;
		}
		if(voxStatus == ENABLE){
			retVal = mlsVoxDisable();
			if(retVal != MLS_SUCCESS){

				return retVal;
			}
		}
	}
	return retVal;
}

mlsErrorCode_t mlsicam_indicatorled(UInt32 timeOn, UInt32 timeOff)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	Char gpioConfig[2] = {0,0/*1<<4|1<<2*/};
	Int32 fd_gpio,iret,valueport;

	fd_gpio = gmlsIcam.fd_gpio;
	if (fd_gpio <= 0)
	{
		printf("Error open /dev/trek_gpio\n");
		retVal = MLS_ERR_Icam_OPEN_GPIODEV;
		return retVal;
	}


	gpioConfig[0] = 0;
	valueport = ioctl(fd_gpio, TREK_GPIO_GET, &gpioConfig[0]);
	if (valueport < 0)
	{
		retVal = MLS_ERROR;
		return retVal;
	}

	if ((timeOn > 0)&&(timeOff == 0))
	{


		gpioConfig[1] = 0xFB & valueport;//0b11111011 & valueport //set low
		iret = ioctl(fd_gpio, TREK_GPIO_SET, gpioConfig);
		if (iret < 0)
		{
			retVal = MLS_ERROR;
		}
	}
	else if ((timeOn == 0)&&(timeOff > 0)) 
	{


		gpioConfig[1] = (1<<2) | (valueport&0xFF); //set high
		iret = ioctl(fd_gpio, TREK_GPIO_SET, gpioConfig);
		if (iret < 0)
		{
			retVal = MLS_ERROR;
		}
	}
	else
	{
		gpioConfig[0] = 0;

		gpioConfig[1] = (1<<2) | (valueport&0xFF); //set high
		iret = ioctl(fd_gpio, TREK_GPIO_SET, gpioConfig);
		if (iret < 0)
		{
			retVal = MLS_ERROR;
			return retVal;
		}
		usleep(timeOff*1000);

		gpioConfig[1] = 0xFB & valueport; //0b11111011 & valueport //set low
		iret = ioctl(fd_gpio, TREK_GPIO_SET, gpioConfig);
		if (iret < 0)
		{
			retVal = MLS_ERROR;
			return retVal;
		}
		usleep(timeOn*1000);
	}

	return retVal;
}
/*
 *
 */
mlsErrorCode_t mlsicam_VolumeSetAsFlash(settingConfig_st *pflashConfig)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	int value = pflashConfig->volume;

	if ((0<=value)&&(value<=100))
	{
		value = (value<<8)|value;
	}
	else
	{
		printf("invalid value %d \n",value);
		return MLS_ERROR;
	}

	retVal = mlsaudioSpkVolume(value);
	if(retVal != MLS_SUCCESS){

		return retVal;
	}
	return retVal;
}

mlsErrorCode_t mlsIcam_SetingAsFlashConf(settingConfig_st *pflashConfig)
{

	mlsErrorCode_t retVal = MLS_SUCCESS;
	retVal = mlsimage_SetResolution((icam_resolution_t)pflashConfig->vgaMode);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}
	gmlsIcam.vgaMode = (icam_resolution_t)pflashConfig->vgaMode;

	retVal = mlssensorSetContrast(_abSensorContrastValue[(unsigned int)pflashConfig->contrastValue]);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}
	gmlsIcam.sensorContrastIndex = pflashConfig->contrastValue;

	retVal = mlssensorSetBrightness(_abSensorBrightValue[(unsigned int)pflashConfig->brightnessValue]);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}
	gmlsIcam.sensorBrightIndex = pflashConfig->brightnessValue;


	if(pflashConfig->cameraView == Flipup)
	{
		retVal = mlssensorFlipup();
		if(retVal != MLS_SUCCESS)
		{

			return retVal;
		}
	}

	retVal = mlsicam_VoxSetAsFlash(pflashConfig);
	if(retVal != MLS_SUCCESS){

		return retVal;
	}
	retVal = mlsicam_VolumeSetAsFlash(pflashConfig);
	if(retVal != MLS_SUCCESS){

		return retVal;
	}
	return retVal;
}

mlsErrorCode_t mlsIcam_Init(icamMode_t *icamMode,networkMode_t *networkMode)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	Bool flagErrCam = False;
	Bool flagErrWifi = False;
	Bool flagflashConfig = False;
	settingConfig_st *pflashConfig;
	char MACaddress[15];


	InitNonVolatileConfigs();


	gmlsIcam.frameSize = 0;
	gmlsIcam.frameChecksum = 0;
	gmlsIcam.frameSizeIn = Icam_JPEGBUFFER_SIZE;

	gmlsIcam.sensorBrightIndex = 4;
	gmlsIcam.sensorContrastIndex = 2;
	gmlsIcam.vgaMode = icam_res_qvga;

	gmlsIcam.audioSize = 0;

	gmlsIcam.indexCurrentJPEG = 0;
	gmlsIcam.indexBeginAudio = 0;
	gmlsIcam.flagGetAudio = True;
	gmlsIcam.temp_offset = 0;
	gmlsIcam.flagReset = 0;
	gmlsIcam.IsConnectToInternet = 0;
	gmlsIcam.fd_gpio = 0;
	gmlsIcam.debug_val1 = 50000; //
	system("ifconfig lo 127.0.0.1 up");
	
	/* create mutex */
	if (pthread_mutex_init(&gmlsIcam.mutex,NULL) != 0)
	{
		retVal = MLS_ERR_Icam_MUTEX;
		return retVal;
	}

	///////////////////////////
	retVal = mlssensorCheckPID();
	if(retVal != MLS_SUCCESS)
	{
		flagErrCam = True;



		system("./led_blink 200 300 &");

		system("./beep_arm 10000 2000 200 800 &");
	}

	int f = open("/tmp/pressGpa5.txt",O_RDONLY);
	if( f < 0){
	}
	else{
		close(f);

FACTORY_RESET:
		*icamMode = icamModeReset;
		*networkMode = wm_adhoc;
        retVal = 1; 
        if (retVal == MLS_SUCCESS)  
        {

            mlsRapit_SwitchtoUAP();
        }
        else
        {

            mlsRapit_ResetFactory();
        }
	}

		flagflashConfig = True;


	{
	    // Have to read Flash to know whether the previous boot user wanna switch to uAP or not
	    // very lousy solution
	    int ret;
	    icamWifiConfig_st *icamWifiConfig;
	    ret = GetIcamWifiConfig(&icamWifiConfig);    
	    if (ret != MLS_SUCCESS)
	    {

		goto FACTORY_RESET;
	    }
	    if(icamWifiConfig->networkMode == wm_adhoc || icamWifiConfig->networkMode == wm_uAP)
	    {

		*icamMode = icamModeReset;
	    
	    }
	    else
	    {

		*icamMode = icamModeNormal;
	    }
	    gmlsIcam.networkMode = icamWifiConfig->networkMode;
	}


	retVal = mlsioInit();
	if(retVal != MLS_SUCCESS)
	{

		return retVal;
	}


	system("killall -IO bmmanager");

    
	//! setup wifi
	retVal = mlsWifi_Setup(icamMode,networkMode);
//////////////////////////////////
	if(retVal != MLS_SUCCESS)
	{
		flagErrWifi = True;
	}

	if (gmlsIcam.networkMode != wm_infra)
	{

	    system("./beep_arm 10000 2000 200 300 900&");
	    sleep(1);
	}

	if ((flagErrCam == True)&&(flagErrWifi == True))
	{


		system("killall led_blink");
		system("killall beep_arm");

		sleep(1);

		system("./led_blink 200 300 &");

		system("./beep_arm 10000 2000 200 300 &");

//		while(1);//stand here
	}
	else if (flagErrWifi == True)//wifi error
	{


		/* blink indicator led*/
		system("./led_blink 200 300 &");
		system("./beep_arm 10000 2000 200 0 &");
		sleep(10);
		mlsRapit_WdtRestart(1);
		sleep(20);
	}

	if ((flagErrCam == True)||(flagErrWifi == True))
	{
		while(1);
	}


	retVal = mlstriggerInit();
	if(retVal != MLS_SUCCESS){

	}
	retVal = mlsimageInit(icam_res_qvga);

	if(retVal != MLS_SUCCESS)
	{

		return retVal;
	}

	retVal = mlsmotionInit();

	if(retVal != MLS_SUCCESS)
	{

		return retVal;
	}

	retVal = mlsaudioInit();
	if(retVal != MLS_SUCCESS)
	{

		return retVal;
	}


	mlsSettingInit();
	GetSettingConfig(&pflashConfig);
	retVal = mlsIcam_SetingAsFlashConf(pflashConfig);


	TempOffset_st *pTempOffset;
	GetTempOffsetConfig(&pTempOffset);
	gmlsIcam.temp_offset = pTempOffset->offset;
	gmlsIcam.cds_type = pTempOffset->cds_type;
	gmlsIcam.video_test_finish = pTempOffset->video_test_finish;
	if (gmlsIcam.networkMode == wm_infra && pTempOffset->video_test_finish)
	{

	    pTempOffset->video_test_finish = 0;
	    SaveTempOffsetConfig();
	    gmlsIcam.video_test_finish = pTempOffset->video_test_finish;
	}

	return retVal;
}

mlsErrorCode_t mlsIcam_DeInit()
{
	mlsErrorCode_t retVal;

	if (gmlsIcam.fd_gpio>0)
	{
		close(gmlsIcam.fd_gpio);
	}

	//mls@dev03 20110713 rapbit
#if 0
	retVal = mlsCX93510_DeInit();
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	retVal = mlsOV7675_DeInit();
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}
#else
	//printf("deinit image \n");
	retVal = mlsimageDeInit();

	//printf("deinit motion \n");
	retVal = mlsmotionDeInit();

	//printf("deinit audio \n");
	retVal = mlsaudioDeInit();

	//printf("deinit io \n");
	retVal = mlsioDeInit();

#endif

	//printf("deinit wifi \n");
	retVal = mlsWifi_DeInit();
	retVal = mlsSettingDeInit();
	retVal = mlstriggerDeInit();

	return retVal;
}


UInt32 mlsIcam_GetInputFrameSize()
{
	return gmlsIcam.frameSizeIn;
}


mlsErrorCode_t mlsIcam_GrabFrame()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt32 frameMiss = 0;


    pthread_mutex_lock(&gmlsIcam.mutex);


	retVal = mlsimageGetLastFrame((char*)gmlsIcam.frameBuffer,(unsigned int*)&gmlsIcam.frameSize);
	if (retVal != MLS_SUCCESS)
	{
		printf("get image error\n");
		goto mlsicam_grabframe_exit;
	}

	// used the indexCurrentJPEG for Checksum

	frameMiss = 0;
	gmlsIcam.indexCurrentJPEG=(gmlsIcam.indexCurrentJPEG+1+frameMiss)%(MLSIcam_INDEXJPEG_MAX+1);


mlsicam_grabframe_exit:
	pthread_mutex_unlock(&gmlsIcam.mutex);
	return retVal;
}


mlsErrorCode_t mlsIcam_GetFrameData(UInt8 *frameBuffer,UInt32 *frameSize)
{
    pthread_mutex_lock(&gmlsIcam.mutex);
	memcpy(frameBuffer,gmlsIcam.frameBuffer,gmlsIcam.frameSize);
	*frameSize = gmlsIcam.frameSize;
    pthread_mutex_unlock(&gmlsIcam.mutex);

	return MLS_SUCCESS;
}


mlsErrorCode_t mlsIcam_GetAudioData(UInt8 *audioBuffer, UInt32 *audioSize)
{
    pthread_mutex_lock(&gmlsIcam.mutex);
	memcpy(audioBuffer,gmlsIcam.audioBuffer,gmlsIcam.audioSize);
	*audioSize = gmlsIcam.audioSize;
    pthread_mutex_unlock(&gmlsIcam.mutex);

	return MLS_SUCCESS;
}


mlsErrorCode_t mlsIcam_GetAudioSize(UInt32 *audioSize)
{
    pthread_mutex_lock(&gmlsIcam.mutex);
	*audioSize = gmlsIcam.audioSize;
    pthread_mutex_unlock(&gmlsIcam.mutex);

	return MLS_SUCCESS;
}


mlsErrorCode_t mlsIcam_StartCapture()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	pthread_mutex_lock(&gmlsIcam.mutex);
//	retVal = mlsCX93510_RestartCapture();
    pthread_mutex_unlock(&gmlsIcam.mutex);

	return retVal;
}


mlsErrorCode_t mlsIcam_StartAudio()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	pthread_mutex_lock(&gmlsIcam.mutex);
	retVal = mlsaudioStart();
    pthread_mutex_unlock(&gmlsIcam.mutex);

	return retVal;
}

mlsErrorCode_t mlsIcam_Flipup()
{
	mlsErrorCode_t reVal = MLS_SUCCESS;
	reVal = mlssensorFlipup();
	if(reVal != MLS_SUCCESS)
	{
		return reVal;
	}
	if(mlsSettingSaveCameraView() != MLS_SUCCESS)
	{
		printf("save camera view into flash error\n");
		return MLS_ERROR;
	}
	return MLS_SUCCESS;
}

mlsErrorCode_t mlsIcam_SetSensorControl(in_cmd_type controlID,
										 UInt8 *newValue)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;


    pthread_mutex_lock(&gmlsIcam.mutex);

	switch (controlID) {

		case IN_CMD_SET_CONTRAST:

			/* break when max value */
			if (*newValue > (SENSOR_CONSTRAST_LEVEL -1) )
			{
			    printf("Value too big %d\n",*newValue);
			    *newValue = SENSOR_CONSTRAST_LEVEL - 1;
			}
			if (gmlsIcam.sensorContrastIndex == *newValue)
			{
			    printf("Same Value. No need to set");
			    break;
			}
			gmlsIcam.sensorContrastIndex = *newValue;
			/* Set contrast */

			retVal = mlssensorSetContrast(_abSensorContrastValue[gmlsIcam.sensorContrastIndex]);
			if (retVal != MLS_SUCCESS)
			{
				goto exit;
			}

			//save value of contrast into flash
			printf("save contrast value into flash\n");
			mlsSettingSaveContrast(gmlsIcam.sensorContrastIndex);

//			retVal = mlsOV7675_GetContrast(&sensorValue);
//			if (retVal != MLS_SUCCESS)
//			{
//				goto exit;
//			}
//			printf("OV reg contrast: %d\n",sensorValue);
		  break;


		case IN_CMD_SET_BRIGHTNESS:

			/* break when max value */
			if (*newValue > (SENSOR_BRIGHT_LEVEL -1))
			{
			    printf("Value too big %d\n",*newValue);
			    *newValue = SENSOR_BRIGHT_LEVEL - 1;
			}
			if (gmlsIcam.sensorBrightIndex == *newValue)
			{
			    printf("Same Value. No need to set");
			    break;
			}
			gmlsIcam.sensorBrightIndex = *newValue;
			/* Set contrast */

			retVal = mlssensorSetBrightness(_abSensorBrightValue[gmlsIcam.sensorBrightIndex]);
			if (retVal != MLS_SUCCESS)
			{
				goto exit;
			}

			//save value of contrast into flash
			printf("save brightness value into flash\n");
			mlsSettingSaveBrightness(gmlsIcam.sensorBrightIndex);

//			retVal = mlsOV7675_GetContrast(&sensorValue);
//			if (retVal != MLS_SUCCESS)
//			{
//				goto exit;
//			}
//			printf("OV reg contrast: %d\n",sensorValue);
		  break;


		case IN_CMD_CONTRAST_PLUS:

			/* break when max value */
			if (gmlsIcam.sensorContrastIndex >= (SENSOR_CONSTRAST_LEVEL-1))
			{
				//Get new Value index
				*newValue = gmlsIcam.sensorContrastIndex;
				break;
			}

			/* Set contrast */

			retVal = mlssensorSetContrast(_abSensorContrastValue[++gmlsIcam.sensorContrastIndex]);
			if (retVal != MLS_SUCCESS)
			{
				goto exit;
			}



			//Get new Value index
			*newValue = gmlsIcam.sensorContrastIndex;
			//save value of contrast into flash
			printf("save contrast value into flash\n");
			mlsSettingSaveContrast(gmlsIcam.sensorContrastIndex);

//			retVal = mlsOV7675_GetContrast(&sensorValue);
//			if (retVal != MLS_SUCCESS)
//			{
//				goto exit;
//			}
//			printf("OV reg contrast: %d\n",sensorValue);
		  break;

		case IN_CMD_CONTRAST_MINUS:
			/* break when min value */
			if (gmlsIcam.sensorContrastIndex <= 0)
			{
				//Get new Value index
				*newValue = gmlsIcam.sensorContrastIndex;
				break;
			}

			/* Set contrast */
			retVal = mlssensorSetContrast(_abSensorContrastValue[--gmlsIcam.sensorContrastIndex]);
			if (retVal != MLS_SUCCESS)
			{
				goto exit;
			}

//			--gmlsIcam.sensorContrastIndex;

			//Get new Value index
			*newValue = gmlsIcam.sensorContrastIndex;
			//save value of contrast into flash
			printf("save contrast value into flash\n");
			mlsSettingSaveContrast(gmlsIcam.sensorContrastIndex);
//			retVal = mlsOV7675_GetContrast(&sensorValue);
//			if (retVal != MLS_SUCCESS)
//			{
//				goto exit;
//			}
//			printf("OV reg contrast: %d\n",sensorValue);
			break;

		case IN_CMD_BRIGHTNESS_PLUS:
			/* break when max value */
			if (gmlsIcam.sensorBrightIndex >= (SENSOR_BRIGHT_LEVEL-1))
			{
				//Get new Value index
				*newValue = gmlsIcam.sensorBrightIndex;
				break;
			}

			/* Set brightness */
			retVal = mlssensorSetBrightness(_abSensorBrightValue[++gmlsIcam.sensorBrightIndex]);
			if (retVal != MLS_SUCCESS)
			{
				goto exit;
			}

//			++gmlsIcam.sensorBrightIndex;
			//Get new Value index
			*newValue = gmlsIcam.sensorBrightIndex;
			//save bright into flash
			printf("save bright value into flash\n");
			mlsSettingSaveBrightness(gmlsIcam.sensorBrightIndex);
//			retVal = mlsOV7675_GetBrightness(&sensorValue);
//			if (retVal != MLS_SUCCESS)
//			{
//				goto exit;
//			}
//			printf("OV reg brightness: %d\n",sensorValue);

		  break;

		case IN_CMD_BRIGHTNESS_MINUS:

			if (gmlsIcam.sensorBrightIndex <= 0)
			{
				//Get new Value index
				*newValue = gmlsIcam.sensorBrightIndex;
				break;
			}


			retVal = mlssensorSetBrightness(_abSensorBrightValue[--gmlsIcam.sensorBrightIndex]);
			if (retVal != MLS_SUCCESS)
			{
				goto exit;
			}

//			--gmlsIcam.sensorBrightIndex;

			//Get new Value index
			*newValue = gmlsIcam.sensorBrightIndex;
			//save bright into flash
			printf("save bright value into flash\n");
			mlsSettingSaveBrightness(gmlsIcam.sensorBrightIndex);
			break;

		default:
			break;
	}

exit:
    pthread_mutex_unlock(&gmlsIcam.mutex);

	return retVal;
}

mlsErrorCode_t mlsIcam_SetResolution(icam_resolution_t resolutionMode)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	int check_diff = 0;

	pthread_mutex_lock(&gmlsIcam.mutex);
	if(gmlsIcam.vgaMode != resolutionMode)
	{

	retVal = mlsimage_SetResolution(resolutionMode);
	if (retVal != MLS_SUCCESS)
	{
		pthread_mutex_unlock(&gmlsIcam.mutex);
		return retVal;
	}
	gmlsIcam.vgaMode = resolutionMode;
		check_diff = 1;
	}	
    pthread_mutex_unlock(&gmlsIcam.mutex);
	if(check_diff)
	{
    if(mlsSettingSaveVGAMode(resolutionMode) != MLS_SUCCESS)
    {

    }
	}

	return retVal;
}


mlsErrorCode_t mlsIcam_Bootswitch()
{
	//simple call
	return mlsWifi_Bootswitch();
}


mlsErrorCode_t mlsIcam_SaveWifiConfig(String strDataIn)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt8 lenSSID,lenKey,lenIP,lenSubnet,lenGW,lenwp,lenUsrN,lenPassW;
	String charPointer = strDataIn;
	String strData = malloc(strlen(strDataIn));
	Int32 k,strLen,index;
	Char strInt[10];
	icamWifiConfig_st *wifiConfig;

	GetIcamWifiConfig(&wifiConfig);

	pthread_mutex_lock(&gmlsIcam.mutex);

	strLen = 0;

	charPointer = strDataIn;

	while (strstr(strDataIn, "%20") != NULL)
	{

		strDataIn = strstr(strDataIn, "%20");

		k = strDataIn - charPointer;

		memcpy(&strData[strLen], charPointer,k);
		strData[strLen+k] = ' ';
		strLen += k+1;

		strDataIn += 3;
		charPointer = strDataIn;
	}

	memcpy(&strData[strLen],strDataIn,strlen(strDataIn)+1);

	index = 0;

	switch (strData[index++])
	{
		case '0':
			wifiConfig->networkMode = wm_adhoc;
			break;
		case '1':
			wifiConfig->networkMode = wm_infra;
			break;
		default:
			break;
	}
	//get channel to set in adhoc mode
	strncpy(strInt,strData+index,2);
	strInt[2] = '\0';
	wifiConfig->channelAdhoc = atoi(strInt);
	if((wifiConfig->channelAdhoc <= 0)||(14<wifiConfig->channelAdhoc))
	{
		wifiConfig->channelAdhoc = WIFI_DEFAULT_ADHOC_CHANNEL;//default channel
	}
	index += 2;

	//get authentication mode
	switch (strData[index++])
	{
		case '0':
			wifiConfig->authMode = wa_open;
			break;
		case '1':
			wifiConfig->authMode = wa_shared;
			break;
		case '2':
			wifiConfig->authMode = wa_wpa;//or wpa2,
			break;
		default:
			break;
	}

	//get keyindex if in open or shared mode
	if ((wifiConfig->authMode == wa_open)||(wifiConfig->authMode == wa_shared))
	{
		switch (strData[index]){
			case '0':
				wifiConfig->keyIndex = 0;
				break;
			case '1':
				wifiConfig->keyIndex = 1;
				break;
			case '2':
				wifiConfig->keyIndex = 2;
				break;
			case '3':
				wifiConfig->keyIndex = 3;
				break;
			default:
				break;
		}
		wifiConfig->encryptProtocol = we_wep;

	}
	else
	{
	    //wifiConfig->encryptProtocol = ;
	}
	index++;

#if 0
	//get length ssid
	strncpy(strInt,strData+3,3);
	strInt[3] = '\0';
	lenSSID = atoi(strInt);

	//get length key
	strncpy(strInt,strData+3+3,2);
	strInt[2] = '\0';
	lenKey = atoi(strInt);

	//get string ssid
	strncpy(wifiConfig->strSSID,strData+3+3+2,lenSSID);
	wifiConfig->strSSID[lenSSID] = '\0';

	//get string key
	strncpy(wifiConfig->strKey,strData+3+3+2+lenSSID,lenKey);

	wifiConfig->strKey[lenKey] = '\0';

#else
	//get ip mode
	switch (strData[index++])
	{
		case '0':
			wifiConfig->ipMode = ip_dhcp;
			break;
		case '1':
			wifiConfig->ipMode = ip_static;
			break;
		default:
			break;
	}

	/* Get lenght */

	//get length ssid
	strncpy(strInt,strData+index,3);
	strInt[3] = '\0';
	lenSSID = atoi(strInt);
	index += 3;


	//get length key
	strncpy(strInt,strData+index,2);
	strInt[2] = '\0';
	lenKey = atoi(strInt);
	index += 2;

	if (lenKey == 0)
	{

	    wifiConfig->encryptProtocol = we_nonsecure;
	}
	//get length ip
	strncpy(strInt,strData+index,2);
	strInt[2] = '\0';
	lenIP = atoi(strInt);
	index += 2;

	//get length subnet mask
	strncpy(strInt,strData+index,2);
	strInt[2] = '\0';
	lenSubnet = atoi(strInt);
	index += 2;

	//get length default gateway
	strncpy(strInt,strData+index,2);
	strInt[2] = '\0';
	lenGW = atoi(strInt);
	index += 2;

	//Thai //get length working port
	strncpy(strInt,strData+index,1);
	strInt[1] = '\0';
	lenwp = atoi(strInt);
	index += 1;


	//Thai //get length username
	strncpy(strInt,strData+index,2);
	strInt[2] = '\0';
	lenUsrN = atoi(strInt);
	index += 2;


	//Thai //get length password
	strncpy(strInt,strData+index,2);
	strInt[2] = '\0';
	lenPassW = atoi(strInt);
	index += 2;


	/* Get string for each component */
	//get string ssid
	strncpy(wifiConfig->strSSID,	strData+index,lenSSID);
	wifiConfig->strSSID[lenSSID] = '\0';
	
	//get string key
	strncpy(wifiConfig->strKey,	strData+index+lenSSID,lenKey);

	wifiConfig->strKey[lenKey] = '\0';

	//mls@dev03 101111
	if (wifiConfig->ipMode == ip_static){
		strncpy(wifiConfig->ipAddressStatic,	strData+index+lenSSID+lenKey,lenIP);
		wifiConfig->ipAddressStatic[lenIP] = '\0';

		strncpy(wifiConfig->ipSubnetMask,	strData+index+lenSSID+lenKey+lenIP,lenSubnet);
		wifiConfig->ipSubnetMask[lenSubnet] = '\0';

		strncpy(wifiConfig->ipDefaultGateway,strData+index+lenSSID+lenKey+lenIP+lenSubnet,lenGW);
		wifiConfig->ipDefaultGateway[lenGW] = '\0';

		if(lenwp!=0){
			strncpy(wifiConfig->workPort,		strData+index+lenSSID+lenKey+lenIP+lenSubnet+lenGW,lenwp);
			wifiConfig->workPort[lenwp] = '\0';
		}
		else{
			memset(wifiConfig->workPort,'\0',sizeof(wifiConfig->workPort));
			strcpy(wifiConfig->workPort,"80");
		}

#ifdef DEBUG_STATIC_PORT

#endif
	}
	else{
		wifiConfig->ipAddressStatic[0] = '\0';
		wifiConfig->ipSubnetMask[0] = '\0';
		wifiConfig->ipDefaultGateway[0] = '\0';
		memset(wifiConfig->workPort,'\0',sizeof(wifiConfig->workPort));
		strcpy(wifiConfig->workPort,"80");
	}

	if(wifiConfig->networkMode==wm_infra){
		strncpy(wifiConfig->userName,		strData+index+lenSSID+lenKey+lenIP+lenSubnet+lenGW+lenwp,lenUsrN);
		wifiConfig->userName[lenUsrN] = '\0';
		strncpy(wifiConfig->passWord,		strData+index+lenSSID+lenKey+lenIP+lenSubnet+lenGW+lenwp+lenUsrN,lenPassW);
		wifiConfig->passWord[lenPassW] = '\0';
	}
	else{
		wifiConfig->userName[0]='0';
		wifiConfig->passWord[0]='0';
	}

#endif
	retVal = SaveIcamWifiConfig();

	if (retVal == MLS_SUCCESS)
	{
		system("/mlsrb/beep_arm 100000 500 500 0 2000 &");
	}
	else
	{
		system("/mlsrb/beep_arm 100000 500 100 400 1500 &");
	}
	free (strData);
	pthread_mutex_unlock(&gmlsIcam.mutex);

	return retVal;
}


mlsErrorCode_t mlsIcam_ReadWifiConfig(String strData)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	icamWifiConfig_st *icamWifiConfig;
	char abMACaddress[13];

	pthread_mutex_lock(&gmlsIcam.mutex);
	retVal = GetIcamWifiConfig(&icamWifiConfig);
	if (retVal != MLS_SUCCESS)
	{
		//Setting wifi default
		printf("Wifi Read Configuration error! \n ");

	}

	_mlsicam_getMACaddress(abMACaddress);

#define PRINT_WORK_PORT_SIZE
#define	PRINT_WORK_PORT
	sprintf(strData,"%s%s%d%02d%d%d%d%03d%02d%02d%02d%02d%01d%02d%02d%s%s%s%s%s%s%s%s",
			GetVersionString(),//5 bytes
											 abMACaddress,//12 bytes
											 icamWifiConfig->networkMode,
											 icamWifiConfig->channelAdhoc,
											 icamWifiConfig->authMode,
											 icamWifiConfig->keyIndex,
											 icamWifiConfig->ipMode,
											 strlen(icamWifiConfig->strSSID),
											 strlen(icamWifiConfig->strKey),
											 strlen(icamWifiConfig->ipAddressStatic),
											 strlen(icamWifiConfig->ipSubnetMask),
											 strlen(icamWifiConfig->ipDefaultGateway),
											 strlen(icamWifiConfig->workPort),
											 strlen(icamWifiConfig->userName),
											 strlen(icamWifiConfig->passWord),
											 icamWifiConfig->strSSID,
											 icamWifiConfig->strKey,
											 icamWifiConfig->ipAddressStatic,
											 icamWifiConfig->ipSubnetMask,
											 icamWifiConfig->ipDefaultGateway,
											 icamWifiConfig->workPort,
											 icamWifiConfig->userName,
											 icamWifiConfig->passWord
											 );


	pthread_mutex_unlock(&gmlsIcam.mutex);

	return retVal;
}


mlsErrorCode_t mlsIcam_ReadPortConfig(String strData)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	icamWifiConfig_st *icamWifiConfig;
	pthread_mutex_lock(&gmlsIcam.mutex);
	retVal = GetIcamWifiConfig(&icamWifiConfig);
	if (retVal != MLS_SUCCESS)
	{
		//Setting wifi default
		printf("Wifi Read Configuration error! \n ");
	}

	sprintf(strData,"%s",icamWifiConfig->workPort);

	printf("Port is: %s\n",strData);
	pthread_mutex_unlock(&gmlsIcam.mutex);
	return retVal;
}


mlsErrorCode_t mlsIcam_SaveUsernamePasswdConfig(String strData)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	icamWifiConfig_st *icamWifiConfig;
	char *username = malloc(strlen(strData)+1);
	if(username == NULL)
	{
		printf("Error: malloc\n");
		return -1;
	}
	strcpy(username,strData);
	//printf("Hai->test:%s\n",username);
	char *passwd = strchr(username,':');
	if(passwd == NULL)
	{

		free(username);
		return -1;
	}
	*passwd=0;
	passwd ++;
	//printf("username:%s\n",username);
	//printf("passwd:%s\n",passwd);
	pthread_mutex_lock(&gmlsIcam.mutex);
	//! Read config, if error return default config
	retVal = GetIcamWifiConfig(&icamWifiConfig);
	if (retVal != MLS_SUCCESS)
	{
	}
	//!Change user:pass
	strcpy(icamWifiConfig->userName,username);
	strcpy(icamWifiConfig->passWord,passwd);
	//! Write back to flash
	retVal = SaveIcamWifiConfig();
	if (retVal != MLS_SUCCESS)
	{

	}

	pthread_mutex_unlock(&gmlsIcam.mutex);
	free(username);
	return retVal;
}

mlsErrorCode_t mlsIcam_ReadUserNameConfig(String strData)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	icamWifiConfig_st *icamWifiConfig;
	pthread_mutex_lock(&gmlsIcam.mutex);
	retVal = GetIcamWifiConfig(&icamWifiConfig);
	if (retVal != MLS_SUCCESS)
	{
		//Setting wifi default
		printf("Wifi Read Configuration error! \n ");
	}

	sprintf(strData,"%s",icamWifiConfig->userName);
#ifdef DEBUG_USRNAME
	printf("Readout from mjpeg_streamer,Username is: %s\n",strData);
#endif
	pthread_mutex_unlock(&gmlsIcam.mutex);
	return retVal;
}

mlsErrorCode_t mlsIcam_ReadPassWordConfig(String strData)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	icamWifiConfig_st *icamWifiConfig;
	pthread_mutex_lock(&gmlsIcam.mutex);
	retVal = GetIcamWifiConfig(&icamWifiConfig);
	if (retVal != MLS_SUCCESS)
	{
		//Setting wifi default
		printf("Wifi Read Configuration error! \n ");
	}
	sprintf(strData,"%s",icamWifiConfig->passWord);
#ifdef DEBUG_USRNAME
	printf("Readout from mjpeg_streamer, passWord is: %s\n",strData);
#endif
	pthread_mutex_unlock(&gmlsIcam.mutex);
	return retVal;
}


mlsErrorCode_t mlsIcam_ReadNetWorkModeConfig(String strData)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	icamWifiConfig_st * icamWifiConfig;
	pthread_mutex_lock(&gmlsIcam.mutex);
	retVal = GetIcamWifiConfig(&icamWifiConfig);
	if (retVal != MLS_SUCCESS)
	{
		//Setting wifi default
		printf("Wifi Read Configuration error! \n ");
	}
	if(icamWifiConfig->networkMode==wm_infra){
		sprintf(strData,"INFRA");
	}
	else
	{
		sprintf(strData,"ADHOC");
	}

	pthread_mutex_unlock(&gmlsIcam.mutex);
	return retVal;
}
/*!
 *
 */
mlsErrorCode_t mlsIcam_SaveFolderRecName(String strFolderName)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	javaApplet_st *javaAppletConfig;

	pthread_mutex_lock(&gmlsIcam.mutex);

	GetJavaAppletConfig(&javaAppletConfig);
	//check path length is valid
	if (strlen(strFolderName)>=sizeof(javaAppletConfig->path))
	{
		printf("Path length is too long");
		retVal = MLS_ERROR;
	}else
	{

		strcpy(javaAppletConfig->path,strFolderName);
		retVal = SaveJavaAppletConfig();
	}

	pthread_mutex_unlock(&gmlsIcam.mutex);

	return retVal;
}

/*!
 *
 */
mlsErrorCode_t mlsIcam_ReadFolderRecName(String strValue)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	javaApplet_st *javaAppletConfig;

	pthread_mutex_lock(&gmlsIcam.mutex);

	retVal = GetJavaAppletConfig(&javaAppletConfig);
	strcpy(strValue, javaAppletConfig->path);

	pthread_mutex_unlock(&gmlsIcam.mutex);

	return retVal;
}

/*!
 *
 */
mlsErrorCode_t mlsIcam_SetMotion(in_cmd_type controlID,Int8* strduty)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

#if 1
	static float dutycycle = 0;

	if (strduty != NULL)
	{
		dutycycle = atof((char*)strduty);
//		printf("dutycycle is %s, change to float %f\n",strduty,dutycycle);
	}
	else
	{
		dutycycle = 1;
	}

	retVal = mlsmotion_SetMovement(controlID,dutycycle);
#endif

	return retVal;
}


/*!
 *mlsRapit_Setledear set the led status
 */
mlsErrorCode_t mlsRapit_Setledear(in_cmd_type controlID)
{
	 return mlsioSetSetupLed( controlID);
}

/*!
 *mlsRapit_SetAudio set the audio streaming
 */
mlsErrorCode_t mlsRapit_SetAudio(in_cmd_type controlID)
{
	 return mlsaudioSetDirection( controlID);
}
//////////////////////////////////////////////////////

mlsErrorCode_t mlsRapit_WifiSignal(int *value_wifi)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
#if 1
	FILE *fpipe;
	char *command="iwconfig ra0";
	char line[256];
	char *pchar = NULL;
	char strlq[4] = {'\0','\0','\0','\0'};
	int valuelq;

	if ( !(fpipe = (FILE*)popen(command,"r")) )
	{  // If fpipe is NULL
		perror("Problems with pipe for command iwconfig");
		retVal = MLS_ERROR;
		return retVal;
	}

	while ( fgets( line, sizeof line, fpipe))
	{
		if (strstr(line,"Link Quality=") != NULL)
		{
			break;
		}
	}

	pchar = strstr(line,"Link Quality=");
	pchar += strlen("Link Quality=");

	if ((strstr(pchar,"/100")-pchar)>=3)
	{
		strcpy(strlq,"100");
	}
	else
	{
		strlq[0] = *pchar;
		strlq[1] = *(pchar+1);
		strlq[2] = '\0';
	}

	valuelq = atoi(strlq);

	if (valuelq == 0)
	{
		*value_wifi = 0;
	}
	else if (valuelq <= 25)
	{
		*value_wifi = 1;
	}
	else if (valuelq <= 50)
	{
		*value_wifi = 2;
	}
	else if (valuelq <= 75)
	{
		*value_wifi = 3;
	}
	else if (valuelq <= 100)
	{
		*value_wifi = 4;
	}
	else
	{
		*value_wifi = -1;
	}

//	printf("Link Quality is %s/100->value wifi is %d \n",strlq,*value_wifi);

	pclose(fpipe);
#else
	static int value = 4;
	*value_wifi = value;
	value -= 1;

	if (value < 0)
	{
		value = 4;
	}
	printf("value wifi is : %d \n",*value_wifi);
#endif
	return retVal;
}

mlsErrorCode_t mlsRapit_BatteryLevel(int *value_battery)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
#if 1
	*value_battery = 4;
#else
	retVal = mlsioBatteryLevel(value_battery);
//	printf("value battery is : %d \n",*value_battery);
#endif
	return retVal;
}

mlsErrorCode_t mlsRapit_GetByteWBvalue(char * value)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	int wifivalue,batteryvalue;

#if 0
	retVal = mlsRapit_BatteryLevel(&batteryvalue);
	if (retVal != MLS_SUCCESS)
	{
		printf("Get battery level value error\n");
		return retVal;
	}
#else
	batteryvalue = 4;
#endif

	retVal = mlsRapit_WifiSignal(&wifivalue);
	if (retVal != MLS_SUCCESS)
	{
		printf("Get wifi signal strength value error\n");
		return retVal;
	}

	*value = ((wifivalue&0x0F)<<4) | (batteryvalue&0x0F);

	return retVal;
}

/////////////////////////// watchdog timer //////////////////////////////
static int wdtfd = 0;
static int wdttime = -1;
mlsErrorCode_t mlsRapit_WdtRestart(int seconds)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	if (wdtfd <= 0)
	{
		wdtfd = open("/dev/watchdog", O_WRONLY);
		if (wdtfd < 0)
		{
			perror("watchdog restart ");
			retVal = MLS_ERROR;
			return retVal;
		}
	}
	wdttime = seconds;

	ioctl(wdtfd, WDIOC_SETTIMEOUT,&wdttime );



	return retVal;
}
mlsErrorCode_t mlsRapit_WdtGetTime(int *time)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	*time = wdttime;
	return retVal;
}

mlsErrorCode_t mlsRapit_WdtKeepAlive()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	if (wdtfd <= 0)
	{
		wdtfd = open("/dev/watchdog", O_WRONLY);
		if (wdtfd < 0)
		{
			perror(__FUNCTION__);
			retVal = MLS_ERROR;
			return retVal;
		}
	}

	ioctl(wdtfd, WDIOC_KEEPALIVE);

	return retVal;
}

mlsErrorCode_t mlsRapit_WdtStop()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	if ((wdtfd > 0)&&(wdttime > 0))
	{
		close(wdtfd);
		wdtfd = 0;
		wdttime = -1;

	}

	return retVal;
}
//////////////////////////////////////////////////////////////////////////////////

mlsErrorCode_t mlsRapbit_GetMelodyIndex(int *index)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	retVal = mlsaudioGetMelodyIndex(index);

	return retVal;
}

mlsErrorCode_t  mlsRapit_GetTemperature(int *value)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	retVal = mlsadcGetTemperature(value);

	return retVal;
}

mlsErrorCode_t mlsRapit_ResetFactory()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

    //! defaults icamWifiConfig_st data
	DefaultIcamWifiConfig();
	retVal = SaveIcamWifiConfig();
	//retVal = mlsflashEraseSectors(FLASH_SAVECONFIG_ADDRESS, 1);
	if (retVal != MLS_SUCCESS)
	{
		printf("reset factory mode :erase error\n");
		retVal = MLS_ERROR;
	}

    //! defaults settingConfig_st data
	DefaultSettingConfig();
    retVal = SaveSettingConfig();
	if (retVal != MLS_SUCCESS)
	{
		printf("reset factory mode :erase error\n");
		retVal = MLS_ERROR;
	}
	return retVal;
}


mlsErrorCode_t mlsRapit_SwitchtoUAP()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	icamWifiConfig_st *icamWifiConfig;
	retVal = GetIcamWifiConfig(&icamWifiConfig);
	icamWifiConfig->networkMode = wm_adhoc;
	sprintf(icamWifiConfig->passWord,"000000");
	SaveIcamWifiConfig ();

	return retVal;
}


/*
 *
 */
mlsErrorCode_t mlsIcam_ReadConfigUAP(String strData)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	uAPConfig_st *puAPConfig;
	char abMACaddress[13];

	pthread_mutex_lock(&gmlsIcam.mutex);
	retVal = GetUAPConfig(&puAPConfig);
	if (retVal != MLS_SUCCESS)
	{
		//Setting uAP default
		printf("uAP Read Configuration error! \n ");
	}
	_mlsicam_getMACaddress(abMACaddress);
	sprintf(strData,"%s%s%02d",GetVersionString() //5 characters
							  ,abMACaddress	 //12 characters
							  ,puAPConfig->channel);//2 characters

	printf("String setting uAP: %s\n",strData);

	pthread_mutex_unlock(&gmlsIcam.mutex);

	return retVal;
}

/*
 *
 */
mlsErrorCode_t mlsIcam_SaveConfigUAP(String strDataIn)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	uAPConfig_st *puAPConfig;
	GetUAPConfig(&puAPConfig);
	pthread_mutex_lock(&gmlsIcam.mutex);

	puAPConfig->channel = (char) atoi(strDataIn);

	retVal = SaveUAPConfig();

	pthread_mutex_unlock(&gmlsIcam.mutex);

	return retVal;
}
/*********************************************************
 * define for vox, mlsdev008 10/11/2011
 *********************************************************/
mlsErrorCode_t mlsicamVoxEnable(char *addr)
{
	mlsErrorCode_t reVal = MLS_SUCCESS;
	reVal = mlsVoxEnable(addr);
	if(reVal == MLS_SUCCESS){
		//save flash if need
		reVal = mlsSettingSaveVoxStatus(Enable);
		if(reVal != MLS_SUCCESS){
			printf("Save vox status error\n");
		}
	}
	else printf("Enable vox error\n");
	return reVal;
}
mlsErrorCode_t mlsicamVoxDisable(char *strValue)
{
	mlsErrorCode_t reVal = MLS_SUCCESS;
	reVal = mlsVoxDisable();
	if(reVal == MLS_SUCCESS){
		//save flash if need
		if(strValue == NULL || strcmp(strValue,"persistent") == 0){
			//save flash
			reVal = mlsSettingSaveVoxStatus(Disable);
			if(reVal != MLS_SUCCESS){
				printf("Save vox status error\n");
			}
		}
		//strValue="non-persistent" and others is not save
	}
	else printf("Disable vox error\n");
	return reVal;
}
mlsErrorCode_t mlsicamVoxGetThreshold(int *reg)
{
	return mlsVoxGetThreshold(reg);
}
mlsErrorCode_t mlsicamVoxSetThreshold(char *threshold)
{
	mlsErrorCode_t reVal = MLS_SUCCESS;
	reVal = mlsVoxSetThreshold(threshold);
	if(reVal == MLS_SUCCESS){
		signed char vthreshold = (signed char)atoi(threshold);
		reVal = mlsSettingSaveVoxThreshold(vthreshold);
		if(reVal != MLS_SUCCESS){
			printf("save threshold error\n");
		}
	}
	return reVal;
}
mlsErrorCode_t mlsicamVoxGetStatus(int *reg)
{
	return mlsVoxGetStatus(reg);
}
/**********************************************************
 * define for save camera name
 **********************************************************/
mlsErrorCode_t mlsicamSaveCameraName(char *name)
{
	return mlsSettingSaveCameraName(name);
}
/*
 *
 */
mlsErrorCode_t mlsRapit_RestartApp()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	system("killall -USR1 bmmanager");

	return retVal;
}

/*
 *
 */
mlsErrorCode_t mlsIcam_SetSpkVolume(String strValue)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	int value = atoi(strValue);

	if ((0<value)&&(value<=100))
	{
		value = (value<<8)|value;
	}
	else
	{
		printf("invalid value %d \n",value);
		return MLS_ERROR;
	}
	printf("Change speaker volume to 0x%4X\n",value);
	retVal = mlsaudioSpkVolume(value);
	if(retVal != MLS_SUCCESS){
		printf("mlsaudioSpkVolume error\n");
		return retVal;
	}
	char save = value&0xff;
	retVal = mlsSettingSaveVolume(save);
	if(retVal != MLS_SUCCESS){
		printf("save volume into flash error\n");
		return retVal;
	}
	return retVal;
}
mlsErrorCode_t mlsIcam_GetSpkVolume(int *valueVol)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	int value;

	mlsaudioGetSpkVolume(&value);
	value = value & 0xff;
	*valueVol = value;
	printf("Speaker volume is %d\n",*valueVol);

	return retVal;
}

mlsErrorCode_t mlsIcam_GetValueBrightness(UInt8 *value)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	*value = gmlsIcam.sensorBrightIndex;
	return retVal;
}

mlsErrorCode_t mlsIcam_GetValueConstract(UInt8 *value)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	*value = gmlsIcam.sensorContrastIndex;
	return retVal;
}

mlsErrorCode_t mlsIcam_GetValueVGAMode(UInt8 *value)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	*value = gmlsIcam.vgaMode;
	return retVal;
}

unsigned int mlsIcam_GetValueTempOffset()
{
	
	return gmlsIcam.temp_offset;
}

void mlsIcam_SetValueTempOffset(int val)
{
	TempOffset_st *pTempOffset;
	GetTempOffsetConfig(&pTempOffset);
	pTempOffset = val;
	gmlsIcam.temp_offset = val;
	SaveTempOffsetConfig();
//	return gmlsIcam.temp_offset;
}

unsigned short mlsIcam_GetCDSType()
{
	return gmlsIcam.cds_type;
}

int mlsIcam_IsinUAPMode()
{
//    printf("Network Mode = %d\n",gmlsIcam.networkMode);
    return (gmlsIcam.networkMode == wm_adhoc) || (gmlsIcam.networkMode == wm_uAP);
}

int mlsIcam_IsVideoTestFinish()
{
//    printf("Video Lock = %d\n",gmlsIcam.video_test_finish);
    return (gmlsIcam.video_test_finish);
}

int mlsIcam_SetConnectToInternet(int isConnectToInternet)
{
    gmlsIcam.IsConnectToInternet = (unsigned char) isConnectToInternet;
    return 0;
}

int mlsIcam_IsConnectToInternet()
{
    return (int) (gmlsIcam.IsConnectToInternet);
}

char is_flipup(void )
{
    settingConfig_st *pflashConfig;
    GetSettingConfig(&pflashConfig);
    return pflashConfig->cameraView;
}

