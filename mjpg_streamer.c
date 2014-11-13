/*******************************************************************************
#                                                                              #
#      MJPG-streamer allows to stream JPG frames from an input-plugin          #
#      to several output plugins                                               #
#                                                                              #
#      Copyright (C) 2007 Tom Stöveken                                         #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/

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
#include <dlfcn.h>
#include <fcntl.h>
#include <syslog.h>

#include "utils.h"
#include "mjpg_streamer.h"

//mls@dev03 include
#include "./plugins/input_mlsicam/mlsicam.h"
#include "./plugins/output_http/output_http.h"
#include "./plugins/input_mlsicam/input_mlsicam.h"
/* globals */
static globals global;

////mls@dev03 blink led thread
//static pthread_t   blthread;
//
//void *blinkLed_thread(void *arg)
//{
//	unsigned long counter;
//
//	while(!global.stop)
//	{
//		pthread_mutex_lock( &global.blinkled_mutex );
//		counter = global.countRequestClient;
//		pthread_mutex_unlock( &global.blinkled_mutex );
//		/* check global counter */
//		if (counter > 0) //have connect
//		{
//			printf("have connection\n");
//			pthread_mutex_lock( &global.blinkled_mutex );
//			global.countRequestClient = 0;
//			pthread_mutex_unlock( &global.blinkled_mutex );
//		}
//		else //no connect
//		{
//			printf("no connection\n");
//		}
//		//sleep(1);
//	//	mlsicam_indicatorled(UInt32 timeOn, UInt32 timeOff);
//		mlsicam_indicatorled(180, 1000);
//	}
//	return NULL;
//}
//
//int blinkLed_run(void)
//{
//	  //mls@dev03 110130 add for blink led thread
//	  global.countRequestClient = 0;
//	     /* create mutex object for blink led counter */
//	  if (pthread_mutex_init(&global.blinkled_mutex,NULL) != 0)
//	  {
//		DBG("could not initialize mutex variable\n");
//		exit(EXIT_FAILURE);
//	  }
//	  if( pthread_create(&blthread, 0, blinkLed_thread, NULL) != 0)
//	  {
//	  	printf("Can not start blink led thread\n");
//	  	exit(EXIT_FAILURE);
//	  };
//
//	  //mls@dev03 110130
//	  pthread_detach(blthread);
//
//	  return 0;
//}


/******************************************************************************
Description.: Display a help message
Input Value.: argv[0] is the program name and the parameter progname
Return Value: -
******************************************************************************/
void help(char *progname)
{
  fprintf(stderr, "-----------------------------------------------------------------------\n");
  fprintf(stderr, "Usage: %s\n" \
                  "  -i | --input \"<input-plugin.so> [parameters]\"\n" \
                  "  -o | --output \"<output-plugin.so> [parameters]\"\n" \
                  " [-h | --help ]........: display this help\n" \
                  " [-v | --version ].....: display version information\n" \
                  " [-b | --background]...: fork to the background, daemon mode\n", progname);
  fprintf(stderr, "-----------------------------------------------------------------------\n");
  fprintf(stderr, "Example #1:\n" \
                  " To open an UVC webcam \"/dev/video1\" and stream it via HTTP:\n" \
                  "  %s -i \"input_uvc.so -d /dev/video1\" -o \"output_http.so\"\n", progname);
  fprintf(stderr, "-----------------------------------------------------------------------\n");
  fprintf(stderr, "Example #2:\n" \
                  " To open an UVC webcam and stream via HTTP port 8090:\n" \
                  "  %s -i \"input_uvc.so\" -o \"output_http.so -p 8090\"\n", progname);
  fprintf(stderr, "-----------------------------------------------------------------------\n");
  fprintf(stderr, "Example #3:\n" \
                  " To get help for a certain input plugin:\n" \
                  "  %s -i \"input_uvc.so --help\"\n", progname);
  fprintf(stderr, "-----------------------------------------------------------------------\n");
  fprintf(stderr, "In case the modules (=plugins) can not be found:\n" \
                  " * Set the default search path for the modules with:\n" \
                  "   export LD_LIBRARY_PATH=/path/to/plugins,\n" \
                  " * or put the plugins into the \"/lib/\" or \"/usr/lib\" folder,\n" \
                  " * or instead of just providing the plugin file name, use a complete\n" \
                  "   path and filename:\n" \
                  "   %s -i \"/path/to/modules/input_uvc.so\"\n", progname);
  fprintf(stderr, "-----------------------------------------------------------------------\n");
}

/******************************************************************************
Description.: pressing CTRL+C sends signals to this process instead of just
              killing it plugins can tidily shutdown and free allocated
              ressources. The function prototype is defined by the system,
              because it is a callback function.
Input Value.: sig tells us which signal was received
Return Value: -
******************************************************************************/
void signal_handler(int sig)
{
  int i;

  /* signal "stop" to threads */
  LOG("setting signal to stop\n");
  global.stop = 1;
  usleep(10000*1000);

  /* clean up threads */
  LOG("force cancelation of threads and cleanup ressources\n");
  global.in.stop();
  for(i=0; i<global.outcnt; i++) {
    global.out[i].stop(global.out[i].param.id);
  }
  usleep(1000*1000);

  /* close handles of input plugins */
  //dlclose(&global.in.handle);
  for(i=0; i<global.outcnt; i++) {
    /* skip = 0;
    DBG("about to decrement usage counter for handle of %s, id #%02d, handle: %p\n", \
        global.out[i].plugin, global.out[i].param.id, global.out[i].handle);
    for(j=i+1; j<global.outcnt; j++) {
      if ( global.out[i].handle == global.out[j].handle ) {
        DBG("handles are pointing to the same destination (%p == %p)\n", global.out[i].handle, global.out[j].handle);
        skip = 1;
      }
    }
    if ( skip ) {
      continue;
    }

    DBG("closing handle %p\n", global.out[i].handle);
    */
    //dlclose(global.out[i].handle);
  }
  DBG("all plugin handles closed\n");

  pthread_cond_destroy(&global.db_update);
  pthread_mutex_destroy(&global.db);

  LOG("done\n");

  closelog();
  exit(0);
  return;
}

#ifdef DEBUG_PCD

#include <sys/prctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "pcdapi.h"

void set_coredump()
{
    int ret;
    struct rlimit rlim;
    
    // SET DUMP STATE
    ret = prctl (PR_GET_DUMPABLE, 0, 0, 0, 0);
    printf("PR_GET_DUMPABLE returned %d\n",ret);
    
    ret = prctl (PR_SET_DUMPABLE, 1, 0, 0, 0);
    printf("PR_SET_DUMPABLE returned %d\n",ret);
    
    ret = prctl (PR_GET_DUMPABLE, 0, 0, 0, 0);
    printf("PR_GET_DUMPABLE returned %d\n",ret);
    
    ret = getrlimit (RLIMIT_CORE, &rlim);
    printf("RLIMIT_CORE return %d (%d _ %d)\n",ret,rlim.rlim_cur, rlim.rlim_max);
    
    rlim.rlim_cur = RLIM_INFINITY;
    rlim.rlim_max = RLIM_INFINITY;
    ret = setrlimit (RLIMIT_CORE, &rlim);
    printf("Set RLIMIT_CORE return %d (%d _ %d)\n",ret,rlim.rlim_cur, rlim.rlim_max);
    
    ret = getrlimit (RLIMIT_CORE, &rlim);
    printf("RLIMIT_CORE return %d (%d _ %d)\n",ret,rlim.rlim_cur, rlim.rlim_max);
    
}
#endif


/******************************************************************************
Description.:
Input Value.:
Return Value:
******************************************************************************/
int main(int argc, char *argv[])
{
  char *input  = "/mlrsb/input_mlsicam.so";
  char *output[MAX_OUTPUT_PLUGINS];
  int daemon=0, i, iret;
  long tmp=0;
  char strPort[400];
  char tempstr[50];
  char username[100];
  char password[100];
  char output_normal[100];

#ifdef DEBUG_PCD
    set_coredump();
    PCD_API_REGISTER_EXCEPTION_HANDLERS();

#endif

  output[0] = "/mlsrb/output_http.so -w mlswww";
  global.outcnt = 0;
   chdir("/mlsrb");
  /* parameter parsing */
  while(1) {
    int option_index = 0, c=0;
    static struct option long_options[] = \
    {
      {"h", no_argument, 0, 0},
      {"help", no_argument, 0, 0},
      {"i", required_argument, 0, 0},
      {"input", required_argument, 0, 0},
      {"o", required_argument, 0, 0},
      {"output", required_argument, 0, 0},
      {"v", no_argument, 0, 0},
      {"version", no_argument, 0, 0},
      {"b", no_argument, 0, 0},
      {"background", no_argument, 0, 0},
      {0, 0, 0, 0}
    };

    c = getopt_long_only(argc, argv, "", long_options, &option_index);

    /* no more options to parse */
    if (c == -1) break;

    /* unrecognized option */
    if(c=='?'){ help(argv[0]); return 0; }

    switch (option_index) {
      /* h, help */
      case 0:
      case 1:
        help(argv[0]);
        return 0;
        break;

      /* i, input */
      case 2:
      case 3:
        input = strdup(optarg);
        break;

      /* o, output */
      case 4:
      case 5:
        output[global.outcnt++] = strdup(optarg);
        break;

      /* v, version */
      case 6:
      case 7:
//        printf("MODEL : MPB2000\n" \
//               "FW VER : %s\n" ,Icam_VERSION);
        return 0;
        break;

      /* b, background */
      case 8:
      case 9:
        daemon=1;
        break;

      default:
        help(argv[0]);
        return 0;
    }
  }

  openlog("MJPG-streamer ", LOG_PID|LOG_CONS, LOG_USER);
  //openlog("MJPG-streamer ", LOG_PID|LOG_CONS|LOG_PERROR, LOG_USER);
  syslog(LOG_INFO, "starting application");
  syslog(LOG_INFO,"Build date %s - Build time %s\n",__DATE__,__TIME__);
  /* fork to the background */
  if ( daemon ) {
    LOG("enabling daemon mode");
    daemon_mode();
  }
  {
    pid_t pid;
    char str[50];
    pid = getpid();
    printf("PID = %d\n",pid);
    sprintf(str,"echo %d >/tmp/mjpeg_streamer.pid",pid);
    system(str);
    system("/bin/sh /mlsrb/post_ota.sh");
  }
  /* initialise the global variables */
  global.stop      = 0;
  global.buf       = NULL;
  global.size      = 0;
  global.in.plugin = NULL;
  memset(&global.RemoteIP,0,sizeof(struct sockaddr_in));
  /* this mutex and the conditional variable are used to synchronize access to the global picture buffer */
  if( pthread_mutex_init(&global.db, NULL) != 0 ) {
    LOG("could not initialize mutex variable\n");
    closelog();
    exit(EXIT_FAILURE);
  }
  if( pthread_cond_init(&global.db_update, NULL) != 0 ) {
    LOG("could not initialize condition variable\n");
    closelog();
    exit(EXIT_FAILURE);
  }

  /* ignore SIGPIPE (send by OS if transmitting to closed TCP sockets) */
  signal(SIGPIPE, SIG_IGN);

  /* register signal handler for <CTRL>+C in order to clean up */
  if (signal(SIGINT, signal_handler) == SIG_ERR) {
    LOG("could not register signal handler\n");
    closelog();
    exit(EXIT_FAILURE);
  }

  /*
   * messages like the following will only be visible on your terminal
   * if not running in daemon mode
   */
  LOG("MJPG Streamer Version.: %s\n", SOURCE_VERSION);

  /* check if at least one output plugin was selected */
  if ( global.outcnt == 0 ) {
    /* no? Then use the default plugin instead */
    global.outcnt = 1;
  }

  /* open input plugin */
  tmp = (size_t)(strchr(input, ' ')-input);
  global.in.plugin = (tmp > 0)?strndup(input, tmp):strdup(input);
  printf("INPUT PLUGIN=%s\n",global.in.plugin);
  global.in.handle = NULL;
  global.in.init = input_init;
  global.in.stop = input_stop;
  global.in.run = input_run;
  /* try to find optional command */
  global.in.cmd = input_cmd;

  global.in.param.parameter_string = strchr(input, ' ');
  global.in.param.global = &global;

  //mls@dev03 comment for input_icam application
/*  if ( global.in.init(&global.in.param) ) {
    LOG("input_init() return value signals to exit");
    closelog();
    exit(0);
  }*/

  //mls@dev03 101013
  iret = global.in.init(&global.in.param);
  switch (iret)
  {
	  case icamModeReset:
//		  output[0] = "./output_http.so -w mlswwwr";
		  global.in.cmd(IN_CMD_CONFIG_WIFI_READ_USRN,username);
		  global.in.cmd(IN_CMD_CONFIG_WIFI_READ_PW,password);
		  //sprintf(password,"000000");
		  printf("Reset: username '%s', passwd '%s'\n",username,password);		  
		  // dont use password anymore it always 00000000
		  if(strlen(username)!=0)
		  { /*username password is enable*/
			  sprintf(output_normal,"/mlsrb/output_http.so -w /mlsrb/mlswwwn -p 80 -c %s:%s",username,password);
		  }
		  else
		  {/*no username and password*/
			  sprintf(output_normal,"/mlsrb/output_http.so -w /mlsrb/mlswwwn -p 80");
		  }

		  output[0] = output_normal;
		  break;
	  case icamModeNormal:
		  //read port, http authenticate username and password from wifi config
		  global.in.cmd(IN_CMD_CONFIG_WIFI_READ_PORT,strPort);
		  global.in.cmd(IN_CMD_CONFIG_WIFI_READ_USRN,username);
		  global.in.cmd(IN_CMD_CONFIG_WIFI_READ_PW,password);
		  //sprintf(password,"000000");
		  printf("Normal: username '%s', passwd '%s'\n",username,password);
		  if(strlen(username)!=0)
		  { /*username password is enable*/
			  sprintf(output_normal,"mlsrb//output_http.so -w /mlsrb/mlswwwn -p %s -c %s:%s",strPort,username,password);
		  }
		  else
		  {/*no username and password*/
			  sprintf(output_normal,"/mlsrb/output_http.so -w /mlsrb/mlswwwn -p %s",strPort);
		  }

		  output[0] = output_normal;
		  break;
	  default:
		  break;
  }

  /* open output plugin */
    i = 0;
    global.out[0].plugin = strdup(output[0]);
    global.out[i].handle = NULL;
    global.out[i].init = output_init;
    global.out[i].stop = output_stop;
    global.out[i].run = output_run;
    /* try to find optional command */
    global.out[i].cmd = output_cmd;

    global.out[i].param.parameter_string = strchr(output[i], ' ');
    global.out[i].param.global = &global;
    global.out[i].param.id = i;
    if ( global.out[i].init(&global.out[i].param) ) {
      LOG("output_init() return value signals to exit");
      closelog();
      exit(0);
    }

  /* start to read the input, push pictures into global buffer */
  DBG("starting input plugin\n");
  syslog(LOG_INFO, "starting input plugin");
  global.in.run();

  DBG("starting %d output plugin(s)\n", global.outcnt);
    i = 0;
    syslog(LOG_INFO, "starting output plugin: %s (ID: %02d)", global.out[i].plugin, global.out[i].param.id);
    global.out[i].run(global.out[i].param.id);


  /* wait for signals */
  pause();

  return 0;
}
