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
#include <signal.h>
#include "step_motor.h"
#include "../../debug.h"
#define 	GPIO_NODE		"/dev/gpio0"
#define 	PANDA_GPIO_SET 		1
#define 	PANDA_GPIO_GET		2

#define 	GPIO_LEVEL_HIGH  	1
#define 	GPIO_LEVEL_LOW 		0

#define		MOTOR_A				0
#define		MOTOR_B				1
//! States of motor
#define		MOTOR_STOP		0
#define		MOTOR_BACKWARD		-1
#define		MOTOR_FORWARD		1
#define 	MOTOR_UNKNOW		8

#define TIMER_WAIT	4   // 2 seconds

static UInt8 motor_a_gpio[5]=   {	MOTORLR_GPIOMOVE0,
									MOTORLR_GPIOMOVE1,
									MOTORLR_GPIOMOVE3,
									MOTORLR_GPIOMOVE2,
									MOTORLR_GPIOBOUNDARY};

static UInt8 motor_b_gpio[5]=   {	MOTORUD_GPIOMOVE0,
									MOTORUD_GPIOMOVE1,
									MOTORUD_GPIOMOVE3,
									MOTORUD_GPIOMOVE2,
									MOTORUD_GPIOBOUNDARY};
#define		MOTOR_STEP_MAX		8

static unsigned char step_data[8][4]= {	{1,0,0,0},{1,1,0,0},{0,1,0,0},{0,1,1,0},
										{0,0,1,0},{0,0,1,1},{0,0,0,1},{1,0,0,1}};

static int autostop_counter=0;
static int AUTOSTOP_COUNTER = 300;
static int motion_fd;
pthread_cond_t  motor_update;
pthread_mutex_t motion_mutex;
pthread_t motion_pthread;
int		run_motor_thread = 1;
typedef struct StepMotorCtrl{

	int	state;

	int 	hit_boundary_state;

	int last_bound;

	int	data_index;

	int	step_num;

	unsigned int	step_delay;

	unsigned char	*gpio;
}MotorCtrl;

struct StepMotorCtrl	a_motor,b_motor;

void * motor_thread(void * arg);


int is_motor_ud_moving(void)
{
	if(b_motor.state != MOTOR_STOP)
		return 1;
	else
		return 0;
}
int is_motor_lr_moving(void)
{
	if(a_motor.state != MOTOR_STOP)
		return 1;
	else
		return 0;
}

void	set_index(int *index, int dir)
{
	
	if(dir == MOTOR_FORWARD)
	{
		(*index) ++;
		if(*index > (MOTOR_STEP_MAX -1))
			*index =0;
		
	}else if (dir == MOTOR_BACKWARD)
	{
		(*index) --;
		if(*index < 0)
			*index = (MOTOR_STEP_MAX - 1);
	}
	
}
/**
 *
 *
 *
 */
int motion_open(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	//! open motion device file
	motion_fd = open(GPIO_NODE, O_RDWR);
	if ( motion_fd < 0 )
	{

		retVal = MLS_ERROR;
		return retVal;
	}

	if( pthread_mutex_init(&motion_mutex, NULL) != 0 )
	{
		printf("could not initialize motion mutex variable\n");
		retVal = MLS_ERROR;
		return retVal;
	}

	if( pthread_cond_init(&motor_update, NULL) != 0 )
	{
		printf("could not initialize condition variable\n");
		retVal = MLS_ERROR;
		return retVal;
	}

	a_motor.state = MOTOR_STOP;

	a_motor.last_bound = MOTOR_STOP;
	a_motor.data_index = 1;//
	a_motor.step_num = -1;
	a_motor.step_delay = 1; //minimum
	a_motor.gpio = motor_a_gpio;
	
	b_motor.state = MOTOR_STOP;
	//b_motor.hit_boundary_state = MOTOR_UNKNOW;
	b_motor.last_bound = MOTOR_STOP;
	b_motor.data_index = 0;//
	b_motor.step_num = -1;
	b_motor.step_delay = 1; //minimum
	b_motor.gpio = motor_b_gpio;

	run_motor_thread = 1;

	if( pthread_create(&motion_pthread, 0, motor_thread, NULL) != 0)
	{

		retVal = MLS_ERROR;
		return retVal;
	}
	pthread_detach(motion_pthread);

	usleep(10000);
	return retVal;
}
/**
 *
 *
 *
 */
int motion_close(void)
{
	int iret;
	mlsErrorCode_t retVal = MLS_SUCCESS;
	//! Exit motor thread
	run_motor_thread = 0;
	b_motor.state = MOTOR_STOP;
	a_motor.state = MOTOR_STOP;
	iret = pthread_cond_signal(&motor_update);
	if (iret != 0)
	{
		printf("signal mA error %d \n",iret);
		retVal = MLS_ERROR;
	}
	pthread_cancel(motion_pthread);
	//! close device
	close(motion_fd);
	return MLS_SUCCESS;
}

static void Timer_Stop()
{
    a_motor.state = b_motor.state = MOTOR_STOP;
}
mlsErrorCode_t mlsmotionInit()
{

	return motion_open();
}
mlsErrorCode_t mlsmotionDeInit()
{

	return motion_close();
}

/**
 *
 *
 *
 *
 */
int is_boundary(unsigned char pin)
{
	int iret;
	unsigned char gpioConfig[2] = {0,0};
	mlsErrorCode_t retVal = MLS_SUCCESS;
	if(motion_fd < 0)
		return MLS_ERROR;
	gpioConfig[0] = pin;
	iret = ioctl(motion_fd, PANDA_GPIO_GET, gpioConfig);
	if (iret < 0)
	{
		retVal = MLS_ERROR;

		return retVal;
	}
    return gpioConfig[1];
}
/**
 *
 *
 *
 *
 */
int set_step_motor(unsigned char *data, unsigned char *gpio_pins)
{
	int iret,i;
	unsigned char gpioConfig[2] = {0,0};
	mlsErrorCode_t retVal = MLS_SUCCESS;
	if(motion_fd < 0)
		return MLS_ERROR;
	for(i =0; i <4; i++)
	{
		gpioConfig[0] = gpio_pins[i];
		gpioConfig[1] = data[i] > 0 ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW;
		iret = ioctl(motion_fd, PANDA_GPIO_SET, &gpioConfig);
		if (iret < 0)
		{
			retVal = MLS_ERROR;
			perror("Error step motor io control\n");
		}
	}
	return retVal;
}

void * motor_thread(void * arg)
{
	int iret;
	int a_bound=0,b_bound=0;
	unsigned char low_gpio[4]={0,0,0,0};
	unsigned char (*ptr)[4], *p;
	unsigned int	a_step_delay=0, b_step_delay=0;
	ptr = step_data;
	while(run_motor_thread)
	{

		set_step_motor(low_gpio,a_motor.gpio);
		set_step_motor( low_gpio,b_motor.gpio);
		iret = pthread_cond_wait(&motor_update,&motion_mutex);
		autostop_counter = 0;
		
		while ((a_motor.state != MOTOR_STOP || b_motor.state != MOTOR_STOP) && (autostop_counter < AUTOSTOP_COUNTER))
		{
			if(a_motor.state != MOTOR_STOP && a_step_delay ==0)
			{	
			
				if(is_boundary(a_motor.gpio[4]))
				{	
``````````````````````````````a_bound +=a_motor.state;
					if(a_bound > BOUNDARY_DETECT_NUM || a_bound < -BOUNDARY_DETECT_NUM)
					{
						a_motor.last_bound = a_motor.state;
						a_motor.state = MOTOR_STOP;
						a_bound = 0;
						printf("A motor boundary\n");
					}

				}
				
				set_index(&(a_motor.data_index), a_motor.state);
				set_index(&(a_motor.data_index), a_motor.state);

				p = *( ptr+ a_motor.data_index);
				set_step_motor(p,a_motor.gpio);				
			
				a_step_delay = a_motor.step_delay;
			}
			
			if(b_motor.state != MOTOR_STOP && b_step_delay == 0)
			{
				if(is_boundary(b_motor.gpio[4]))
				{
b_bound +=b_motor.state;
					if(b_bound > BOUNDARY_DETECT_NUM || b_bound < -BOUNDARY_DETECT_NUM)
					{
						b_motor.last_bound = b_motor.state;
						b_motor.state = MOTOR_STOP;
						b_bound =0;
						printf("B motor boundary\n");
					}
				}
				set_index(&(b_motor.data_index), b_motor.state);
				p=*(ptr+b_motor.data_index);
				set_step_motor( p,b_motor.gpio);				
				
				b_step_delay = b_motor.step_delay;
			}
			
			exit_motion:
				usleep(QUATUM_DLAY_STEP_MOTOR);
				autostop_counter++;
				if (autostop_counter >= AUTOSTOP_COUNTER)
				{

				}
				if(a_step_delay)
					a_step_delay--;
				if(b_step_delay)
					b_step_delay--;
		}
	}

	pthread_exit(NULL);
}
mlsErrorCode_t mlsmotion_SetMovement(in_cmd_type direction,float factor)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	int iret;

	switch (direction)
	{
	case IN_CMD_STOP_FB:
		b_motor.state = MOTOR_STOP;
		break;
	case IN_CMD_MOV_FORWARD:
	case IN_CMD_MOV_BACKWARD:
		if(direction == IN_CMD_MOV_FORWARD) // UP
		{
			b_motor.state = MOTOR_BACKWARD;
		}else if(direction == IN_CMD_MOV_BACKWARD) //DOWN
		{
			b_motor.state = MOTOR_FORWARD;
		}
		b_motor.step_delay = (UInt32)(factor);
		b_motor.step_num = 8;
		autostop_counter = 0;
		iret = pthread_cond_signal(&motor_update);
		if (iret != 0)
		{
			printf("signal mA error %d \n",iret);
			retVal = MLS_ERROR;
		}
		break;
	case IN_CMD_STOP_LR:
		a_motor.state = MOTOR_STOP;
		break;
	case IN_CMD_MOV_LEFT:
	case IN_CMD_MOV_RIGHT:
		
		if(direction == IN_CMD_MOV_LEFT)
		{
			a_motor.state = MOTOR_BACKWARD;
		}else if(direction == IN_CMD_MOV_RIGHT)
		{	
			a_motor.state = MOTOR_FORWARD;
		}
		a_motor.step_delay = (UInt32)(factor);
		a_motor.step_num = 8;
		autostop_counter = 0;
		iret = pthread_cond_signal(&motor_update);
		if (iret != 0)
		{
			printf("signal mA error %d \n",iret);
			retVal = MLS_ERROR;
		}
		break;

    case IN_CMD_MOV_STOP:
		a_motor.state = MOTOR_STOP;
		b_motor.state = MOTOR_STOP;
    	//mlsmotion_stop();???
		break;
	default:
		break;
	}
	return retVal;
}

mlsErrorCode_t mlsmotion_SetAutoStopCounter(int counter)
{
    AUTOSTOP_COUNTER = counter;
    return 0;
}
