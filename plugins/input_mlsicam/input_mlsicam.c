/*
 * input_mlsicam.c
 *
 *  Created on: Aug 18, 2010
 *      Author: mlslnx003
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <linux/videodev.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pthread.h>
#include <syslog.h>

#include "sys_configs.h"
#include "../../mjpg_streamer.h"
#include "../../utils.h"

#include "mlsicam.h"
#include "mlsOTA.h"
#include "mlsAuthen.h"
#include "../../debug.h"

#define mlsdev008_debug
#ifdef mlsdev008_debug
#define print(...) printf(__VA_ARGS__)
#else
#define print(...)
#endif
/* private functions and variables to this plugin */
static pthread_t   worker;
static globals     *pglobal = NULL;
static pthread_mutex_t controls_mutex;
static char wticamstop = 0;/* flag to run worker thread to get JPEG and audio data in encoder */

void *worker_thread( void *);
void worker_cleanup(void *);

//mls@dev03 check connection thread
static pthread_t   checkconthread;
#define MOTOAWORKING	is_motor_ud_moving()
#define MOTOBWORKING	is_motor_lr_moving()



extern int gAudioTestRecordEnable;

int _checkARP()
{
	FILE *fpipe;
	char *command="arp -a";
	char line[255];
	int iret = 0;

	if ( !(fpipe = (FILE*)popen(command,"r")) )
	{  // If fpipe is NULL
		perror("Problems with pipe for command arp : -a");
		return iret;
	}

	while ( fgets( line, sizeof line, fpipe))
	{
		if (strstr(line,"on ra0") != NULL)
		{
			iret = 1;
			printf("There is at least an entry in ARP cache\n");
			break;
		}
	}
	pclose(fpipe);
	return iret;
}


int _checkDhcpLeases()
{
	int iret = 0;

	if (access("/var/lib/misc/udhcpd.leases",F_OK) == 0)
	{
		iret = 1;
		printf("There is at least a dhcp lease \n");
	}

	return iret;
}

void *checkConnect_thread(void *arg)
{
	unsigned long counter;
	char flagConnect = 0;
	char flagARPCheck = 1;
	int wdttime = 0;
	icamMode_t icamMode;
	mlsErrorCode_t errCode = MLS_SUCCESS;

	icamMode = (icamMode_t)arg;
//	printf("blinkled thread: icam mode %d \n",icamMode);

	mlsRapit_WdtGetTime(&wdttime);

	if(wdttime > 0)
	{
		flagARPCheck = 1;
		printf("Starting to check ARP cache and DHCP leases file to detect first wifi connection to other device...\n");
	}
	else
	{
		flagARPCheck = 0;
	}

	while(!pglobal->stop)
	{
		if (flagARPCheck)
		{
			if(_checkARP()||_checkDhcpLeases())
			{
				pthread_mutex_lock( &controls_mutex );
				mlsRapit_WdtStop();
				pthread_mutex_unlock( &controls_mutex );
				flagARPCheck = 0;
				printf("Stop check ARP cache and DHCP leases file \n");
			}

		}

		counter = pglobal->countRequestClient;
//		printf("counter is %ld\n",counter);
		/* check global counter */
		if (counter > 0) //have connect
		{
//			printf("having connection...\n");
			flagConnect = 1;

			pthread_mutex_lock( &pglobal->blinkled_mutex );
			pglobal->countRequestClient = 0;
			pthread_mutex_unlock( &pglobal->blinkled_mutex );
		}
		else //no connect
		{
			if (flagConnect == 1)
			{
				if ((MOTOAWORKING)||(MOTOBWORKING))
			
				{
					errCode = mlsIcam_SetMotion(IN_CMD_MOV_STOP,NULL);
					if (errCode != MLS_SUCCESS)
					{
						IPRINT("set all motors stop failed\n");
						exit(EXIT_FAILURE);
					}
					printf("no connection, rabot is moving...stop all motors!\n");
				}
				flagConnect = 0;
			}
//			printf("no connection....\n");
		}
		//mls@dev03 201108 period 500ms
		//usleep(500*1000);
		sleep(4);
	}
	return NULL;
}

int checkConnect_run(icamMode_t icamMode)
{

      if (pglobal == NULL)
	  {
		exit(EXIT_FAILURE);
	  }

	  pglobal->countRequestClient = 0;

	  if (pthread_mutex_init(&pglobal->blinkled_mutex,NULL) != 0)
	  {
		DBG("could not initialize mutex variable\n");
		exit(EXIT_FAILURE);
	  }
	  if( pthread_create(&checkconthread, 0, checkConnect_thread, (void*)icamMode) != 0)
	  {
	  	printf("Can not start blink led thread\n");
	  	exit(EXIT_FAILURE);
	  };

	  //mls@dev03 110130
	  pthread_detach(checkconthread);

	  return 0;
}




int input_init(input_parameter *param)
{
	mlsErrorCode_t errCode = MLS_SUCCESS;
	icamMode_t icamMode;
	networkMode_t networkMode;

	/* create mutex object for control command */
	if (pthread_mutex_init(&controls_mutex,NULL) != 0)
	{
		DBG("could not initialize mutex variable\n");
		exit(EXIT_FAILURE);
	}


	pglobal = param->global;


	errCode = mlsIcam_Init(&icamMode,&networkMode);
	if (errCode != MLS_SUCCESS)
	{
		IPRINT("init Icam failed \n");
		closelog();
		exit(EXIT_FAILURE);
	}

	mlsIcam_GetValueBrightness(&pglobal->sensorBrightValue);
	mlsIcam_GetValueConstract(&pglobal->sensorContrastValue);
	mlsIcam_GetValueVGAMode(&pglobal->resolutionValue);
	pglobal->compressionValue = icam_compression_standard;
	pglobal->size = 0;
	pglobal->sizeAudio = 0;
	pglobal->countOFBuffAudio = 0;
	pglobal->nbClient = 0;

    mlsIcam_SetResolution(pglobal->resolutionValue);
    pglobal->CentralServerIP.sin_addr.s_addr = 0;
    mlsAuthen_ReadNetMask(&pglobal->NetworkmaskIP);
    mlsAuthen_ReadCurrentIP(&pglobal->CurrentIP);
    {

        mlsAuthen_ResetCreateSessionKeyProcess();
        mlsAuthen_GenerateSessionKey(NULL,NULL,NULL);
    }
    /////////////////////////////////////////////////////////////////////////
    
	if (icamMode == icamModeReset)
	{

	}

	if (networkMode == wm_adhoc)
	{
	    pglobal->FlagAdhocMode = 1; 
	}
	else
	{
	    pglobal->FlagAdhocMode = 0;
	}

	checkConnect_run(icamMode);

	return icamMode;
}


int input_stop(void) {
  DBG("will cancel input thread\n");
  pthread_cancel(worker);
  pthread_cancel(checkconthread);

  return 0;
}

/******************************************************************************
Description.: spins of a worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int input_run(void)
{
  if (wticamstop == 1)
  {
	  printf("icam worker thread stop \n");
	  return 0;
  }

  pglobal->buf = malloc(mlsIcam_GetInputFrameSize());
  if (pglobal->buf == NULL) {
//    fprintf(stderr, "could not allocate memory\n");
      exit(EXIT_FAILURE);
  }

  pglobal->bufAudio = malloc(GLOBAL_AUDIOBUFFER_SIZE);
  if (pglobal->bufAudio == NULL)
  {
	  exit(EXIT_FAILURE);
  }

  pglobal->remotebufAudio = malloc(GLOBAL_AUDIOBUFFER_SIZE);
  if (pglobal->remotebufAudio == NULL)
  {
	  exit(EXIT_FAILURE);
  }

  if( pthread_create(&worker, 0, worker_thread, NULL) != 0) {
    if (pglobal->buf != NULL)
	{
    	free(pglobal->buf);
	}
//    fprintf(stderr, "could not start worker thread\n");
    exit(EXIT_FAILURE);
  }
  pthread_detach(worker);

  return 0;
}

/******************************************************************************
Description.: process commands, allows to set certain runtime configurations
              and settings like pan/tilt, colors, saturation etc.
Input Value.: * cmd specifies the command, a complete list is maintained in
                the file "input.h"
              * value is used for commands that make use of a parameter.
Return Value: depends in the command, for most cases 0 means no errors and
              -1 signals an error.
******************************************************************************/
int input_cmd(in_cmd_type cmd, char*  strValue) {
  mlsErrorCode_t errCode = MLS_SUCCESS;
  int res=0;
  
  UInt8 newValue = 0;
  char strWifi[400];

  pthread_mutex_lock( &controls_mutex );

  switch (cmd)
  {


	  default:
		  res = -99;
		  break;
  }

  pthread_mutex_unlock( &controls_mutex );

  return res;
}

/******************************************************************************
Description.: this functions cleans up allocated ressources
Input Value.: arg is unused
Return Value: -
******************************************************************************/
void worker_cleanup(void *arg)
{
  static unsigned char first_run=1;

  if ( !first_run ) {
    DBG("already cleaned up ressources\n");
    return;
  }

  first_run = 0;
  DBG("cleaning up ressources allocated by input thread\n");

  //clean up mlsicam
  mlsIcam_DeInit();

  /* free global buffer */
  if (pglobal->buf != NULL) free(pglobal->buf);

  /* free global audio buffer */
  if (pglobal->bufAudio != NULL) free(pglobal->bufAudio);

  if (pglobal->remotebufAudio != NULL) free(pglobal->remotebufAudio);

}

/******************************************************************************
Description.: this thread worker grabs a frame and copies it to the global buffer
Input Value.: unused
Return Value: unused, always NULL
******************************************************************************/
void *worker_thread( void *arg ) {
  /* set cleanup handler to cleanup allocated ressources */
  mlsErrorCode_t errCode;
  Int32 iret;

  UInt32 cxSizeAudio;

  static unsigned long countframe = 0;

  pthread_cleanup_push(worker_cleanup, NULL);

  errCode = mlsIcam_StartCapture();
  if (errCode != MLS_SUCCESS)
  {
      IPRINT("Error start capture\n");
      exit(EXIT_FAILURE);
  }

//  errCode = mlsIcam_StartAudio();
//  if (errCode != MLS_SUCCESS)
//  {
//      IPRINT("Error start audio\n");
//      exit(EXIT_FAILURE);
//  }

  while( !pglobal->stop )
  {

	errCode = mlsIcam_GrabFrame();
    if( errCode != MLS_SUCCESS)
    {
      IPRINT("Error grabbing frames\n");
      exit(EXIT_FAILURE);
    }


    pthread_mutex_lock( &pglobal->db );


    pglobal->flagReset = gmlsIcam.flagReset;
	pglobal->indexCurrentJPEG =  gmlsIcam.indexCurrentJPEG;


    errCode = mlsIcam_GetFrameData((UInt8*)pglobal->buf,(UInt32*)&pglobal->size);
    if (errCode != MLS_SUCCESS)
    {
        IPRINT("Error copy frames\n");
        exit(EXIT_FAILURE);
    }


    errCode = mlsRapit_GetTemperature(&pglobal->tempValue);


    /* signal fresh_frame */
    iret = pthread_cond_broadcast(&pglobal->db_update);

    if (iret != 0)
    {
    	printf("broadcast error %d \n",iret);
    }

    pthread_mutex_unlock( &pglobal->db );
    usleep(30*1000);
    DBG("waiting for next frame\n");

  }

  DBG("leaving input thread, calling cleanup function now\n");
  pthread_cleanup_pop(1);

  return NULL;
}
