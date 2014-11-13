/*
 * mlsio.c
 *
 *  Created on: Aug 4, 2011
 *      Author: root
 */



#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/soundcard.h>
#include <sys/poll.h>
#include <math.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>

#include "mlsio.h"
#include "../../debug.h"

#define	RAPIT_GPIO_NODE	"/dev/gpio0"
#define PANDA_GPIO_SET 1
#define PANDA_BATTERY_LEVEL	50	//define by mlsdev008
#define PANDA_REG_SET	99
#define PANDA_REG_GET	100

typedef struct reg_struct_t
{
    unsigned int address;
    unsigned int val;
}reg_struct_t;



//define gpio group A
#define GPA0	0
#define GPA1	GPA0 + 1
#define GPA2	GPA0 + 2
//define gpio group E
#define GPE0	120
#define GPE1	GPE0 + 1
#define GPE2	GPE0 + 2
#define GPE11	GPE0 + 11
/*****************************************************/

/* define SETUP LED */
#if 1
#define SETUP_LED_GPIO GPE11
#else
#define SETUP_LED_GPIO GPA2
#endif

typedef enum setupledstate_t
{
	LED_ON = 0,
	LED_OFF = 1,
	LED_BLINK
}setupledstate_t;

typedef struct mlsioConfig_st
{
	int fd_gpio;
}mlsioConfig_st;

typedef struct setupledConfig_st
{
	//mls@dev03 blink led thread
	pthread_t   belthread;
	int stop;
	setupledstate_t status;
	pthread_cond_t  update;
	pthread_mutex_t mutex;
	UInt32 timeon;
	UInt32 timeoff;
}setupledConfig_st;

mlsioConfig_st ioConfig;
setupledConfig_st setupledConfig;


mlsErrorCode_t _mlsioSetSetupLed(int state){
	int iret;
	char gpioConfig[2] = {0,0};
	mlsErrorCode_t retVal = MLS_SUCCESS;

	gpioConfig[0] = SETUP_LED_GPIO;
	//gpioConfig[1] = state > 0 ? SETUPLED_OFF : SETUPLED_ON;
	gpioConfig[1] = state > 0 ? 0 : 1; // reverse because GPIO reverse
	iret = ioctl(ioConfig.fd_gpio, PANDA_GPIO_SET, gpioConfig);
	if (iret < 0)
	{
		retVal = MLS_ERROR;
		perror("Error led io control\n");
	}
    return retVal;
}
/*
 * _mls_CheckGPA5: check GPIO_GROUP_A_5
 */
mlsErrorCode_t mlsioCheckGPA5(icamMode_t *icamMode,networkMode_t *networkMode)
{
	mlsErrorCode_t reVal = MLS_SUCCESS;
	int i = 0,f_id = -1;
	char buf[2] = {GPA5,0};
	f_id = open("/dev/gpio0",O_RDWR);
	if(f_id < 0)
	{
		printf("open file has problem\n");
		reVal = MLS_ERR_OPENFILE;
		return reVal;
	}
	for(i = 0; i <= 5; i++)  // 5sec only
	{
		ioctl(f_id,PANDA_GPIO_GET,buf);
//		printf("value is returned:%d\n",buf[1]);
		if(buf[1] > 0)
		{
				*icamMode = icamModeNormal;//add by mlsdev008
				*networkMode = wm_infra;
				close(f_id);
				return reVal;
		}
		printf("Button is hold...\n");
		if(i < 10) sleep(1);
	}
	*icamMode = icamModeReset;
	*networkMode = wm_adhoc;
	close(f_id);
	return reVal;
}


mlsErrorCode_t mlsioShortCheckGPA5()
{
	mlsErrorCode_t reVal = MLS_SUCCESS;
	int i = 0,f_id = -1;
	char buf[2] = {GPA5,0};
	f_id = open("/dev/gpio0",O_RDWR);
	if(f_id < 0)
	{
		printf("open file has problem\n");
		reVal = MLS_ERR_OPENFILE;
		return reVal;
	}
	for(i = 0; i <= 5; i++)  // 5sec only
	{
		ioctl(f_id,PANDA_GPIO_GET,buf);
//		printf("value is returned:%d\n",buf[1]);
		if(buf[1] > 0)
		{
				close(f_id);
				return reVal;
		}
		printf("Button is hold...\n");
		sleep(1);
	}
	reVal = -1;
	close(f_id);
	return reVal;
}

/*!
 * time in ms
 */
mlsErrorCode_t mlsioSetBlinkDuration(UInt32 timeon, UInt32 timeoff)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	setupledConfig.timeon = timeon;
	setupledConfig.timeoff = timeoff;

	return retVal;
}

/*!
 *mlsRapit_Setledsetup set the led status
 */
mlsErrorCode_t mlsioSetSetupLed(in_cmd_type controlID)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	int iret;

	switch(controlID){
	case IN_CMD_LED_EAR_ON:
		printf("led on\n");
		setupledConfig.status=  LED_ON;
		retVal = _mlsioSetSetupLed(SETUPLED_ON);
		break;
	case IN_CMD_LED_EAR_OFF:
		printf("led off\n");
		setupledConfig.status=  LED_OFF;
		retVal = _mlsioSetSetupLed(SETUPLED_OFF);
		break;
	case IN_CMD_LED_EAR_BLINK:
		printf("led blink: time on %d ms - time off %d ms\n", setupledConfig.timeon,setupledConfig.timeoff);
	    /* signal fresh_frame */
		if (setupledConfig.status !=  LED_BLINK)
		{
			setupledConfig.status =  LED_BLINK;
		    iret = pthread_cond_signal(&setupledConfig.update);

		    if (iret != 0)
		    {
		    	printf("signal setupled blink error %d \n",iret);
		    	retVal = MLS_ERROR;
		    }
		}
		break;
	default:
		break;
	}

	return retVal;
}


void *blinkSetupLed_thread(void *arg)
{
	int iret;
//	printf("blinkled thread: icam mode %d \n",icamMode);

	while(!setupledConfig.stop)
	{
		if (setupledConfig.status !=  LED_BLINK)
		{
			iret = pthread_cond_wait(&setupledConfig.update,&setupledConfig.mutex);
		}
//		pthread_mutex_unlock(&motionConfig.mutex);
		//led on
		_mlsioSetSetupLed(SETUPLED_ON);
		usleep(1000*setupledConfig.timeon);

		if (setupledConfig.stop)
		{
			break;
		}

		if (setupledConfig.status !=  LED_BLINK)
		{
			if (setupledConfig.status ==  LED_OFF)
			{
				_mlsioSetSetupLed(SETUPLED_OFF);
			}
			else if (setupledConfig.status == LED_ON)
			{
				_mlsioSetSetupLed(SETUPLED_ON);
			}
			continue;
		}

		//ledoff
		_mlsioSetSetupLed(SETUPLED_OFF);
		usleep(1000*setupledConfig.timeoff);
	}

	printf("blink led thread ends up!\n");

	return NULL;
}

int blinkSetupLed_run()
{
	  printf("run blink led thread\n");
	  if( pthread_create(&setupledConfig.belthread, 0, blinkSetupLed_thread, NULL) != 0)
	  {
	  	printf("Can not start blink led thread\n");
	  	exit(EXIT_FAILURE);
	  };

	  //mls@dev03 110130
	  pthread_detach(setupledConfig.belthread);

	  return 0;
}

mlsErrorCode_t mlsioInit()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	setupledConfig.stop = 0;
	setupledConfig.status = LED_OFF;
	setupledConfig.timeon = 0;
	setupledConfig.timeoff = 0;

	ioConfig.fd_gpio = open(RAPIT_GPIO_NODE, O_RDWR);
	if (ioConfig.fd_gpio <= 0)
	{
		printf("Error open /dev/gpio0\n");
		return -1;
	}


	if( pthread_mutex_init(&setupledConfig.mutex, NULL) != 0 )
	{
		printf("could not initialize motion mutex variable\n");
		retVal = MLS_ERROR;
		return retVal;
	}

	if( pthread_cond_init(&setupledConfig.update, NULL) != 0 )
	{
		printf("could not initialize mA condition variable\n");
		retVal = MLS_ERROR;
		return retVal;
	}

	//_mlsioSetSetupLed(SETUPLED_OFF);

	blinkSetupLed_run();

	return retVal;
}

mlsErrorCode_t mlsioDeInit()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	setupledConfig.stop = 1;
	usleep(1000*1000);//wait end thread blink led;

	pthread_cancel(setupledConfig.belthread);

	close(ioConfig.fd_gpio);

	return retVal;
}
mlsErrorCode_t controlLed(char *arg)
{
	ioctl(ioConfig.fd_gpio,PANDA_GPIO_SET,arg);
	return MLS_SUCCESS;
}
mlsErrorCode_t getLedStatus(char *arg)
{
	ioctl(ioConfig.fd_gpio,PANDA_GPIO_GET,arg);
	return MLS_SUCCESS;
}
//mlsErrorCode_t mlsioBatteryLevel(int *value)
//{
//	mlsErrorCode_t retVal = MLS_SUCCESS;
//#if 0
//	int data = 0;
//
//	if(ioctl(ioConfig.fd_gpio,PANDA_BATTERY_LEVEL,&data) < 0)
//	{
//		printf("ioctl has problem\n");
//		retVal = MLS_ERROR;
//		return retVal;
//	}
////	printf("value of ADC : %d\n",data);//debug
//	if(data <= 552 && data >= 0) //[0 - 3.6] V
//	{
//		data = 1;
//	}
//	else if(data <= 585) //(3.6 - 3.8] V
//	{
//		data = 2;
//	}
//	else if(data <= 616) // (3.8 - 4.0] V
//	{
//		data = 3;
//	}
//	else if(data <= 1023)// >4.0 V
//	{
//		data = 4;
//	}
//	else
//	{
//		//printf("what is it\n");
//		data = 0;
//	}
//
////	printf("return value: %d\n",data);
//
//	*value = data;
//#else
//	*value = 4;
//#endif
//
//	return retVal;
//}


//get led stop for set time blink in wifi - mlsdev008 8/12/2011
int mlsioGetLedConfStop()
{
	return setupledConfig.stop;
}

void mlsioSetReg(unsigned int address, unsigned int value)
{
    reg_struct_t reg_struct;
    reg_struct.address = address;
    reg_struct.val = value;
    printf("Wanna SET REG address %08X value %08X\n",address,value);
    ioctl(ioConfig.fd_gpio,PANDA_REG_SET,&reg_struct);
}

void mlsioGetReg(unsigned int address, unsigned int *value)
{
    reg_struct_t reg_struct;
    reg_struct.address = address;
    reg_struct.val = 0;
    ioctl(ioConfig.fd_gpio,PANDA_REG_GET,&reg_struct);
    printf("Wanna GET REG address %08X value %08X\n",reg_struct.address,reg_struct.val);
    *value = reg_struct.val;

}
/**
 *
 *
 * \return : > 0: is version
 *          = 0: no version 
 *          < 0: error
 */
int  getHardwareVersion(void)
{
    char buf[2] = {GPD5,0};
    if (ioConfig.fd_gpio <= 0)
	{
		printf("Error open /dev/gpio0\n");
		return -1;
	}
    ioctl(ioConfig.fd_gpio,PANDA_GPIO_GET,buf);
    if( buf[1] > 0)
        return 1;
    else if(buf[1] == 0)
        return 2;
    else
        return -1;
}