/*
 * mlsmotion.c
 *
 *  Created on: Jul 25, 2011
 *      Author: mls@dev03
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
#include <sys/time.h>
#include <unistd.h>

#include "mlsmotion.h"
#include "../../debug.h"

#define debug_motion
#ifdef debug_motion
#define print(...) printf(__VA_ARGS__)
#else
#define print(...)
#endif
//#define PWM_DEV "/dev/pwm"
/* define to check motor boundary */
#define MOTOR_CHECK_BOUNDARY_LR		//left&right motor
#define MOTOR_CHECK_BOUNDARY_UD		//up&down motor


char motorAworking = 0;
char motorBworking = 0;
char motionCommandFlag = 0;

#define RAPIT_GPIO_NODE	"/dev/gpio0"

#define PANDA_GPIO_SET 1
#define PANDA_GPIO_GET	2

#define GPIO_LEVEL_HIGH  1
#define GPIO_LEVEL_LOW 0
#define USDURATION_STEP_MOTOR 200

static UInt8 motorlr_gpiomov[4]=   {MOTORLR_GPIOMOVE0,
									MOTORLR_GPIOMOVE1,
									MOTORLR_GPIOMOVE2,
									MOTORLR_GPIOMOVE3
									};

static UInt8 motorud_gpiomov[4]=   {MOTORUD_GPIOMOVE0,
									MOTORUD_GPIOMOVE1,
									MOTORUD_GPIOMOVE2,
									MOTORUD_GPIOMOVE3
									};


typedef struct mlsmotionConfig_st
{
	int fd;
//	unsigned int PWMoutcycle;

	in_cmd_type direction;
	float factor;

	pthread_mutex_t mutex;
	//motor A
	char mA_stop;
//	pthread_mutex_t mA_mutex;
	pthread_cond_t  mA_update;
	pthread_t   mA_thread;
	unsigned int mA_usduration;
	char mA_move_stop;

	//motor B
	char mB_stop;
//	pthread_mutex_t mB_mutex;
	pthread_cond_t  mB_update;
	pthread_t   mB_thread;
	unsigned int mB_usduration;
	char mB_move_stop;
	in_cmd_type mAdirection;
	in_cmd_type mBdirection;
}mlsmotionConfig_st;

mlsmotionConfig_st motionConfig;

////////////////////////////GPIO control////////////////////////////////////
static mlsErrorCode_t _mlsmotionSetGPIO(UInt8 pin, UInt8 state){
	Int32 iret;
	UInt8 gpioConfig[2] = {0,0};
	mlsErrorCode_t retVal = MLS_SUCCESS;

	gpioConfig[0] = pin;
	gpioConfig[1] = state > 0 ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW;
	iret = ioctl(motionConfig.fd, PANDA_GPIO_SET, gpioConfig);
	if (iret < 0)
	{
		retVal = MLS_ERROR;
		perror("Error led io control\n");
	}
    return retVal;
}

static mlsErrorCode_t _mlsmotionGetGPIO(UInt8 pin, Int8 *state){
	Int32 iret;
	UInt8 gpioConfig[2] = {0,0};
	mlsErrorCode_t retVal = MLS_SUCCESS;

	gpioConfig[0] = pin;
	iret = ioctl(motionConfig.fd, PANDA_GPIO_GET, gpioConfig);
	if (iret < 0)
	{
		retVal = MLS_ERROR;
		perror("Error led io control\n");
		return retVal;
	}
	*state = gpioConfig[1];
    return retVal;
}

static mlsErrorCode_t mlsmotionSetGPIOLR(UInt8 indexGPIO, UInt8 state)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	retVal = _mlsmotionSetGPIO( motorlr_gpiomov[indexGPIO], state);

	return retVal;
}

static mlsErrorCode_t mlsmotionSetGPIOUD(UInt8 indexGPIO, UInt8 state)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	retVal = _mlsmotionSetGPIO( motorud_gpiomov[indexGPIO], state);

	return retVal;
}
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////motor control thread////////////////////////////////////
//mls@dev03 motion thread
void *motion_motorA_thread(void *arg)
{
	mlsErrorCode_t retVal =MLS_SUCCESS;
	int iret,times = 0;
	Int8 boundaryFlag = 0;
	while (!motionConfig.mA_stop)
	{
		iret = pthread_cond_wait(&motionConfig.mA_update,&motionConfig.mutex);

		//run motor here
		print("mA is moving\n");
		motorAworking = 1;
		if(!motionConfig.mA_move_stop)motionConfig.mAdirection = motionConfig.direction;
		while (!motionConfig.mA_move_stop)
		{
			switch (motionConfig.direction)
			{
			case IN_CMD_MOV_FORWARD:
				retVal = mlsmotion_forward(motionConfig.mA_usduration,1);
				break;
			case IN_CMD_MOV_BACKWARD:
				retVal = mlsmotion_backward(motionConfig.mA_usduration,1);
				break;
			default:
				break;
			}

			if (retVal == MLS_ERR_MOTOR_BOUNDARY)
			{
				times++;
				if (times > 200)
				{
					motionConfig.mA_move_stop = 1;
				}
			}
			else
			{
				times = 0;
			}
		}

		retVal = _mlsmotorA_stop();
		motorAworking = 0;

	}
	return NULL;
}

/*
 *
 */
void *motion_motorB_thread(void *arg)
{
	mlsErrorCode_t retVal =MLS_SUCCESS;
	int iret,times = 0;
	Int8 boundaryFlag = 0;
	while (!motionConfig.mB_stop)
	{
		iret = pthread_cond_wait(&motionConfig.mB_update,&motionConfig.mutex);
		//check boundary here
		print("mB is moving\n");
		motorBworking = 1;
		if(!motionConfig.mB_move_stop)motionConfig.mBdirection = motionConfig.direction;
		while (!motionConfig.mB_move_stop)
		{
			switch (motionConfig.direction)
			{
			case IN_CMD_MOV_LEFT:
				retVal = mlsmotion_turnleft(motionConfig.mB_usduration,1);
				break;
			case IN_CMD_MOV_RIGHT:
				retVal = mlsmotion_turnright(motionConfig.mB_usduration,1);
				break;
			default:
				break;
			}

			if (retVal == MLS_ERR_MOTOR_BOUNDARY)
			{
				times++;
				if (times > 200)
				{
					motionConfig.mB_move_stop = 1;
				}
			}
			else
			{
				times = 0;
			}
		}

		retVal = _mlsmotorB_stop();
		motorBworking = 0;

//		motionConfig.mB_factor = motionConfig.factor;
//		printf("mB factor : %1.1f\n",motionConfig.mB_factor);
//		pthread_mutex_unlock(&motionConfig.mutex);
	}

	return NULL;
}

mlsErrorCode_t motion_thread_run()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	printf("run motion thread mA and mB \n");

	//start thread for motor A
	if( pthread_create(&motionConfig.mA_thread, 0, motion_motorA_thread, NULL) != 0)
	{
		printf("Can not start motion thread\n");
		retVal = MLS_ERROR;
		return retVal;
	};

	pthread_detach(motionConfig.mA_thread);

	//start thread for motor B
	if( pthread_create(&motionConfig.mB_thread, 0, motion_motorB_thread, NULL) != 0)
	{
		printf("Can not start motion thread\n");
		retVal = MLS_ERROR;
		return retVal;
	};

	pthread_detach(motionConfig.mB_thread);

	return retVal;
}
/////////////////////////////////////////////////////////////////////////////////////////

//////////////motor function//////////////////////////////////////////////

/***** Motor A *****/
mlsErrorCode_t _mlsmotorA_stop()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	int i;
	/* set low */
	/* set all gpio to 0 */
	for(i=0;i<sizeof(motorud_gpiomov);i++)
	{
		retVal = _mlsmotionSetGPIO( motorud_gpiomov[i], GPIO_LEVEL_LOW);
		if (retVal != MLS_SUCCESS)
		{
			printf("set motion gpio error!\n");
			retVal = MLS_ERROR;
			return retVal;
		}
	}

	return retVal;
}
mlsErrorCode_t _mlsmotorA_forward(UInt32 usduration,char checkBoundary)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	Int8 i,j,boundaryFlag;
	//*************** up *************************
	UInt8 logic[8][5]={"1000", "1100", "0100", "0101", "0001", "0011", "0010", "1010"};
//	UInt8 logic[8][5]={"1110", "1100", "1101", "0101", "0111", "0011", "1011", "1010"};
//	UInt8 logic[8][5]={"1010", "1011", "0011", "0111", "0101", "1101", "1100", "1110"};
//	UInt8 logic[4][5]={"1100","0110","0011","1001"};
//	UInt8 logic[4][5]={"0011","0110","1100","1001"};
//	UInt8 logic[4][5]={"1000","0100","0010","0001"};

	for (i=0;i<8;i++)
	{
		for(j=0;j<4;j++)
		{
			/* check stop flag */
			if (motionConfig.mA_move_stop)
			{
				return retVal;
			}


			/* set logic for gpio */
//			printf("%c ",logic[i][j]);
			switch (logic[i][j])
			{
			case '0':
				retVal = mlsmotionSetGPIOUD(j, 0);
				if(retVal != MLS_SUCCESS)
				{
					printf("set gpio to 0 failed\n");
					return retVal;
				}
				break;
			case '1':
				retVal = mlsmotionSetGPIOUD(j, 1);
				if(retVal != MLS_SUCCESS)
				{
					printf("set gpio to 1 failed\n");
					return retVal;
				}
				break;
			default:
				printf("undefine character \n");
				break;
			}
		}
//		printf("\n");
		usleep(usduration);
	}
#if defined(MOTOR_CHECK_BOUNDARY_UD)
			if (checkBoundary)
			{
				/* check boundray gpio */
				retVal = _mlsmotionGetGPIO(MOTORUD_GPIOBOUNDARY,&boundaryFlag);
				if (retVal != MLS_SUCCESS)
				{
					perror("get boundary gpio error!\n");
					return retVal;
				}
				if (boundaryFlag)
				{
					printf("motor hit the boundary! stop moving.\n");
					//_mlsmotorA_stop();
					retVal  = MLS_ERR_MOTOR_BOUNDARY;
					return retVal;
				}
			}
#endif
	return retVal;
}
mlsErrorCode_t _mlsmotorA_backward(UInt32 usduration,char checkBoundary)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	Int8 i,j,boundaryFlag;
	//*********** down *******************************
	UInt8 logic[8][5]={"1010", "0010", "0011", "0001", "0101", "0100", "1100", "1000"};
//	UInt8 logic[8][5]={"1110", "1100", "1101", "0101", "0111", "0011", "1011", "1010"};
//	UInt8 logic[8][5]={"1010", "1011", "0011", "0111", "0101", "1101", "1100", "1110"};
//	UInt8 logic[4][5]={"0011","0110","1100","1001"};
//	UInt8 logic[4][5]={"0011","0110","1100","1001"};
//	UInt8 logic[4][5]={"1000","0100","0010","0001"};

	for (i=0;i<8;i++)
	{
		for(j=0;j<4;j++)
		{
			/* check stop flag */
			if (motionConfig.mA_move_stop)
			{
				return retVal;
			}


			/* set logic for gpio */
//			printf("%c ",logic[i][j]);
			switch (logic[i][j])
			{
			case '0':
				retVal = mlsmotionSetGPIOUD(j, 0);
				if(retVal != MLS_SUCCESS)
				{
					printf("set gpio to 0 failed\n");
					return retVal;
				}
				break;
			case '1':
				retVal = mlsmotionSetGPIOUD(j, 1);
				if(retVal != MLS_SUCCESS)
				{
					printf("set gpio to 1 failed\n");
					return retVal;
				}
				break;
			default:
				printf("undefined character \n");
				break;
			}
		}
//		printf("\n");
		usleep(usduration);
	}
#if defined(MOTOR_CHECK_BOUNDARY_UD)
			if (checkBoundary)
			{
				/* check boundray gpio */
				retVal = _mlsmotionGetGPIO(MOTORUD_GPIOBOUNDARY,&boundaryFlag);
				if (retVal != MLS_SUCCESS)
				{
					perror("get boundary gpio error!\n");
					return retVal;
				}
				if (boundaryFlag)
				{
					printf("motor hit the boundary! stop moving.\n");
					//_mlsmotorA_stop();
					retVal  = MLS_ERR_MOTOR_BOUNDARY;
					return retVal;
				}
			}
#endif
	return retVal;
}

/**** Motor B ****/
mlsErrorCode_t _mlsmotorB_stop()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	int i;
	/* set low */
	/* set all gpio to 0 */
	for(i=0;i<sizeof(motorlr_gpiomov);i++)
	{
		retVal = _mlsmotionSetGPIO( motorlr_gpiomov[i], GPIO_LEVEL_LOW);
		if (retVal != MLS_SUCCESS)
		{
			printf("set motion gpio error!\n");
			retVal = MLS_ERROR;
			return retVal;
		}
	}

	return retVal;
}
mlsErrorCode_t _mlsmotorB_forward(UInt32 usduration,char checkBoundary)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	Int8 i,j,boundaryFlag;
	//******************************** turn left *****************************************
	UInt8 logic[8][5] = {"0100", "1100", "1000", "1010", "0010", "0011", "0001", "0101"};
//	UInt8 logic[8][5] = {"0101", "1101", "1100", "1110", "1010", "1011", "0011", "0111"};
//	UInt8 logic[8][5]={"0011","0110","1100","1001"};
//	UInt8 logic[4][5]={"0011","0110","1100","1001"};
//	UInt8 logic[4][5]={"1000","0100","0010","0001"};

	for (i=0;i<8;i++)
	{
		for(j=0;j<4;j++)
		{
			/* check stop flag */
			if (motionConfig.mB_move_stop)
			{
				return retVal;
			}


			/* set logic for gpio */
//			printf("%c ",logic[i][j]);
			switch (logic[i][j])
			{
			case '0':
				retVal = mlsmotionSetGPIOLR(j, 0);
				if(retVal != MLS_SUCCESS)
				{
					printf("set gpio to 0 failed\n");
					return retVal;
				}
				break;
			case '1':
				retVal = mlsmotionSetGPIOLR(j, 1);
				if(retVal != MLS_SUCCESS)
				{
					printf("set gpio to 1 failed\n");
					return retVal;
				}
				break;
			default:
				printf("undefined character \n");
				break;
			}

		}
//		printf("\n");
		usleep(usduration);
	}


#if defined(MOTOR_CHECK_BOUNDARY_LR)
		if (checkBoundary)
		{
			/* check boundary gpio */
			retVal = _mlsmotionGetGPIO(MOTORLR_GPIOBOUNDARY,&boundaryFlag);
			if (retVal != MLS_SUCCESS)
			{
				perror("get boundary gpio error!\n");
				return retVal;
			}
			if (boundaryFlag)
			{
				printf("motor hit the boundary! stop moving.\n");
				//_mlsmotorB_stop();
				retVal  = MLS_ERR_MOTOR_BOUNDARY;
				return retVal;
			}
		}
#endif

	return retVal;
}
mlsErrorCode_t _mlsmotorB_backward(UInt32 usduration,char checkBoundary)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	Int8 i,j,boundaryFlag;
	//************************** turn right *****************************************
	UInt8 logic[8][5]={"0101", "0001", "0011", "0010", "1010", "1000", "1100", "0100"};
//	UInt8 logic[8][5]={"0111", "0011", "1011", "1010", "1110", "1100", "1101", "0101"};
//	UInt8 logic[4][5]={"0011","1001","1100","0110"};
//	UInt8 logic[4][5]={"1100","0110","0011","1001"};
//	UInt8 logic[4][5]={"0001","0010","0100","1000"};

	for (i=0;i<8;i++)
	{
		for(j=0;j<4;j++)
		{
			/* check stop flag */
			if (motionConfig.mB_move_stop)
			{
				return retVal;
			}


			/* set logic for gpio */
//			printf("%c ",logic[i][j]);
			switch (logic[i][j])
			{
			case '0':
				retVal = mlsmotionSetGPIOLR(j, 0);
				if(retVal != MLS_SUCCESS)
				{
					printf("set gpio to 0 failed\n");
					return retVal;
				}
				break;
			case '1':
				retVal = mlsmotionSetGPIOLR(j, 1);
				if(retVal != MLS_SUCCESS)
				{
					printf("set gpio to 1 failed\n");
					return retVal;
				}
				break;
			default:
				printf("undefined character \n");
				break;
			}
		}
//		printf("\n");
		usleep(usduration);
	}
#if defined(MOTOR_CHECK_BOUNDARY_LR)
			if (checkBoundary)
			{
				/* check boundray gpio */
				retVal = _mlsmotionGetGPIO(MOTORLR_GPIOBOUNDARY,&boundaryFlag);
				if (retVal != MLS_SUCCESS)
				{
					perror("get boundary gpio error!\n");
					return retVal;
				}
				if (boundaryFlag)
				{
					printf("motor hit the boundary! stop moving.\n");
					//_mlsmotorB_stop();
					retVal  = MLS_ERR_MOTOR_BOUNDARY;
					return retVal;
				}
			}
#endif

	return retVal;
}
/////////////////////////////////////////////////////////////////////////

////////// motion function /////////////////////////////////////////////
mlsErrorCode_t mlsmotion_forward(UInt32 usduration,char checkBoundary)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	retVal = _mlsmotorA_forward(usduration,checkBoundary);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	return retVal;
}
mlsErrorCode_t mlsmotion_backward(UInt32 usduration,char checkBoundary)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	retVal = _mlsmotorA_backward(usduration,checkBoundary);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	return retVal;
}
mlsErrorCode_t mlsmotion_turnleft(UInt32 usduration, char checkBoundary)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;


	retVal = _mlsmotorB_forward(usduration,checkBoundary);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	return retVal;
}
mlsErrorCode_t mlsmotion_turnright(UInt32 usduration,char checkBoundary)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	retVal = _mlsmotorB_backward(usduration,checkBoundary);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	return retVal;
}
mlsErrorCode_t mlsmotion_stop()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	retVal = _mlsmotorA_stop();
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	retVal = _mlsmotorB_stop();
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	return retVal;
}

/////////////////////////////////////////////////////////////////////////
mlsErrorCode_t mlsmotionInit()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

#if 1
	/* open pwm device file */
	motionConfig.fd = open(RAPIT_GPIO_NODE, O_RDWR);
	if ( motionConfig.fd < 0 )
	{
		printf("open pwm device error\n");
		retVal = MLS_ERROR;
		return retVal;
	}

	motionConfig.mA_stop = 0;
	motionConfig.mA_usduration = 0;
	motionConfig.mA_move_stop = 1;

	motionConfig.mB_stop = 0;
	motionConfig.mB_usduration = 0;
	motionConfig.mB_move_stop = 1;

	motionConfig.mAdirection = IN_CMD_MOV_STOP;
	motionConfig.mBdirection = IN_CMD_MOV_STOP;
	/* this mutex and the conditional variable are used to synchronize access to the motor  */
	if( pthread_mutex_init(&motionConfig.mutex, NULL) != 0 )
	{
		printf("could not initialize motion mutex variable\n");
		retVal = MLS_ERROR;
		return retVal;
	}

	if( pthread_cond_init(&motionConfig.mA_update, NULL) != 0 )
	{
		printf("could not initialize mA condition variable\n");
		retVal = MLS_ERROR;
		return retVal;
	}

	if( pthread_cond_init(&motionConfig.mB_update, NULL) != 0 )
	{
		printf("could not initialize mB condition variable\n");
		retVal = MLS_ERROR;
		return retVal;
	}

	retVal = mlsmotion_stop();
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	retVal = motion_thread_run();
#endif

	return retVal;
}
mlsErrorCode_t mlsmotionDeInit()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
#if 1
	motionConfig.mA_stop = 1;
	motionConfig.mB_stop = 1;
	usleep(1000*1000);//wait thread mA and mB stop;

	pthread_cancel(motionConfig.mA_thread);
	pthread_cancel(motionConfig.mB_thread);

	close(motionConfig.fd);
#endif

	return retVal;
}

mlsErrorCode_t mlsmotion_SetMovement(in_cmd_type direction,float factor)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
#if 1
	int iret;

	motionConfig.direction = direction;
	motionConfig.factor = factor;

	switch (direction)
	{
#if 1
	case IN_CMD_MOV_FORWARD:
	case IN_CMD_MOV_BACKWARD:
		motorAworking = 1;
//		motionCommandFlag = 1;

		motionConfig.mA_move_stop = 0;
		motionConfig.mA_usduration = (UInt32)(factor*USDURATION_STEP_MOTOR);
		printf("=>motor UD: factor %f -> duration each step %d \n",factor,motionConfig.mA_usduration);
//		if (motionConfig.mA_factor != factor)
//		{
			/* signal fresh_frame */
			iret = pthread_cond_signal(&motionConfig.mA_update);

			if (iret != 0)
			{
				printf("signal mA error %d \n",iret);
				retVal = MLS_ERROR;
			}
//		}
		break;
	case IN_CMD_STOP_FB:
		motionConfig.mA_move_stop = 1;
		break;
#endif

	case IN_CMD_MOV_LEFT:
	case IN_CMD_MOV_RIGHT:
		motorBworking = 1;
//		motionCommandFlag = 1;

		motionConfig.mB_move_stop = 0;
		motionConfig.mB_usduration = (UInt32)(factor*USDURATION_STEP_MOTOR);
		printf("=>motor LR: factor %f -> duration each step %d \n",factor,motionConfig.mB_usduration);
//		if (motionConfig.mB_factor != factor)
//    	{
			/* signal fresh_frame */
			iret = pthread_cond_signal(&motionConfig.mB_update);

			if (iret != 0)
			{
				printf("signal mB  error %d \n",iret);
				retVal = MLS_ERROR;
			}
//    	}
		break;

	case IN_CMD_STOP_LR:
		motionConfig.mB_move_stop = 1;
		break;
    case IN_CMD_MOV_STOP:
    	mlsmotion_stop();
    	motorAworking = motorBworking = 0;
//    	motionConfig.mA_factor = motionConfig.mB_factor = 0;
    	motionConfig.mB_move_stop = motionConfig.mA_move_stop = 1;
	default:
		break;
	}
#endif

	return retVal;
}
