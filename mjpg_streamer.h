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

#define SOURCE_VERSION "2.0"
#define MAX_OUTPUT_PLUGINS 1

//#define DEBUG
#define	DEBUG_USERNAME
#ifdef DEBUG
#define DBG(...) fprintf(stderr, " DBG(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__)
#else
#define DBG(...)
#endif

#define LOG(...) { char _bf[1024] = {0}; snprintf(_bf, sizeof(_bf)-1, __VA_ARGS__); fprintf(stderr, "%s", _bf); syslog(LOG_INFO, "%s", _bf); }

#include "plugins/input.h"
#include "plugins/output.h"
#include <sys/socket.h>
#include <netinet/in.h>


//mls@dev03 101005 debug
//long countInput = 0,countOutput = 0,countInputAudio = 0,countOutputAudio = 0;

#define GLOBAL_AUDIOBUFFER_SIZE (4096*4)

/*enable this define to limit just one client access*/
//#define MS_LIMIT_CLIENT

typedef struct _globals globals;
struct _globals {
  int stop;

  /* signal fresh frames */
  pthread_mutex_t db;
  pthread_cond_t  db_update;
  char nbClient;

  /* global JPG frame, this is more or less the "database" */
  unsigned char *buf;
  int size;

  /* input plugin */
  input in;

  /* output plugin */
  output out[MAX_OUTPUT_PLUGINS];
  int outcnt;

  //mls@dev03 100917
  unsigned char sensorBrightValue;
  unsigned char sensorContrastValue;

  //mls@dev03 100924
  unsigned char resolutionValue;
  unsigned char compressionValue;

  //mls@dev03 100928 audio data
  unsigned char *bufAudio;
  int sizeAudio;

  unsigned char *remotebufAudio;
  int remotesizeAudio;

  //mls@dev03 101005 synchronize audio and jpeg stream
  unsigned int indexCurrentJPEG;
  //mls@dev03 101016 add count number buffer audio overflow
  unsigned char countOFBuffAudio;

  unsigned char flagReset;

  //mls@dev03 110130 count number request of client in priod time to check connect
  unsigned long countRequestClient;
  char FlagAdhocMode;
  pthread_mutex_t blinkled_mutex;

  //mls@dev03 201108019 byte for wifi strength and battery level values
  char wbvalue;

  //mls@dev03 20111103 temperature value
  int tempValue;
  
  // JOHN add for AUTHEN
  struct sockaddr_in CurrentIP;
  struct sockaddr_in NetworkmaskIP;
  struct sockaddr_in CentralServerIP;
  struct sockaddr_in RemoteIP;
  char RandomKey[9];
  char	RemoteIPFlag;
};


//#pragma pack(1)
/*
 * mls@dev03 101006 out http icam header for video and audio streams
 */
typedef struct mlsIcamHeaderVA_st
{
	unsigned char flagReset;
	char  abLenAudio[4];
	char  abIndexCurrentJpgFrame[4];
	char  resolution;
	unsigned char countOFbuffAudio;
	char abTempValue[4];//temperature value 20111103
}mlsIcamHeaderVA_st;
//#pragma pack()

/*
 * mls@dev03 101013 out http icam header for video stream
 */
typedef struct mlsIcamHeaderV_st
{
	char  resolutionJpg;
}mlsIcamHeaderV_st;
