/* 
 * mlsaudio.c
 *
 *  Created on: Aug 2, 2011
 *      Author: root
 */

/*
 * Version History
 * Author        ID     Date           Revision
 * jasonlee      0001   04/04/12       Initial Creation
 *
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/ioctl.h>

#include "step_motor.h"
#include "mlsaudio.h"
#include "mlsio.h"
#include "mlsVox.h"
#include "mlstrigger.h"
#include "mlsflash.h"
#include "mlsicam.h"
#include "postoffice.h"
#include "sys_configs.h"
#include "../../debug.h"

//define debug
//#define debug

//#define DEBUG_SOCKET



#ifdef DEBUG_SOCKET
#define DEBUGSOCKET(...)	printf(__VA_ARGS__)
#else
#define DEBUGSOCKET(...)
#endif

//define debug
#ifdef DEBUG
#define DEBUG(...)	printf(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

#define debug_ain3
#ifdef debug_ain3
#define print(...) printf(__VA_ARGS__)
#else
#define print(...)
#endif

#define IR_LED	GPE8
static char inVal_IR_LED[2] = {IR_LED,SETUPLED_OFF};
static char ir_status = SETUPLED_OFF;
static int ADC_reading_flag = 0;
static int average_value = 0;
static int is_mic_muted = 0;
static int is_speaker_init = 0;
static int cur_irled_status=SETUPLED_OFF;

#define FILE_TEST_RECORD

#define BLOCKSAMPLE	505
#define CHANNEL		1
#define VOLUME		0x3232//0x6464//0x3232
#define SAMPLE_RATE	8000

#define WAVE_HEADER	60 // Need to use TotalConverter to convert IAM ADPCM 4bit 8KHz mono

#if defined (MICAUDIOSTREAM_PCM)
#define AUDIO_BUFFER_SIZE (16*1024) //8080 sample PCM
#else
#define AUDIO_BUFFER_SIZE (256*2*8) //8080 sample PCM
#endif

/* speaker config */
#define FORMAT			AFMT_S16_LE	/* Little endian signed 16*/
#define BLOCK_SAMPLE_READ	(1024)//2046

/* socket config */
#define PORT_INET		51108
#define SERVER_IP		INADDR_ANY /*use my ip*/

/* melody config */
#define MELODY_FILE1 "/mlsrb/melody1.wav"
#define MELODY_FILE2 "/mlsrb/melody2.wav"
#define MELODY_FILE3 "/mlsrb/melody3.wav"
#define MELODY_FILE4 "/mlsrb/melody4.wav"
#define MELODY_FILE5 "/mlsrb/melody5.wav"

//vox
#define AMOUNT	4000	//0.5 s
unsigned long total_vox=0;
int vox_count = 0;
short *vox_buf = NULL;
char* lullaby_buffer=NULL;  // will be malloc later in code only malloc 1MB so please make sure the extract size is smaller than that
int lullaby_buffer_size = 0;
int lullaby_buffer_ptr=0;

int soft_gain=128;
int soft_gain_timer=0;




int noise_gate_low=18;			// 0 to 80
int noise_gate_high=24;			// 0 to 80
int noise_gate_min_gain=2;		// 0 to 128
int noise_gate_max_gain=128;	// 0 to 128
int noise_gate_attack_rate=24;	// 0 to 16 ( soft gain increase rate )
int noise_gate_decay_rate=1;	// 0 to 16 ( soft_gain decrease rate )
int soft_gain_limit=70;			// 0 to 80 ( Volume threhold to limit gain )
int soft_gain_max=120;			// 0 to 64
int soft_gain_rate=128;			// 80=10ms 128=16ms

// 12-04-27
int soft_gain_max_hold_limit=188;			//200=3.2s,188=3s
int	soft_gain_max_hold=0;			// hold time=soft_gain_rate(16ms)

///////////////////////////////////////////////////



AudioFineTune_st *audio_finetune;
mlsErrorCode_t temperature_thread_run();

void mlsaudioSetFinetune(unsigned int finetune1, unsigned int finetune2)
{
	if(audio_finetune != NULL)
	{
		printf("Finetune Set %08X - %08X\n",finetune1, finetune2);
		audio_finetune->finetune1 = finetune1;
		audio_finetune->finetune2 = finetune2;
		noise_gate_low = audio_finetune->finetune1 & 0x000000FF;
		noise_gate_high = (audio_finetune->finetune1 >> 8) & 0x000000FF;
		noise_gate_min_gain = (audio_finetune->finetune1 >> 16) & 0x000000FF;
		noise_gate_max_gain = (audio_finetune->finetune1 >> 24) & 0x000000FF;
		noise_gate_attack_rate = audio_finetune->finetune2 & 0x0000001F;
		noise_gate_decay_rate = (audio_finetune->finetune2 >> 5) & 0x00000007;
		soft_gain_limit = (audio_finetune->finetune2 >> 8) & 0x000000FF;
		soft_gain_max = (audio_finetune->finetune2 >> 16) & 0x000000FF;
		soft_gain_rate = (audio_finetune->finetune2 >> 24) & 0x000000FF;
	}
}

void mlsaudioGetFinetune(unsigned int* finetune1, unsigned int* finetune2)
{
	if(audio_finetune != NULL)
	{
		*finetune1 = audio_finetune->finetune1;
		*finetune2 = audio_finetune->finetune2;
	}
}


typedef enum audiodirection_t
{
	AUDIO_INIT,
	AUDIO_STREAM_SPEAKER,
	AUDIO_STREAM_MIC,
	AUDIO_MELODY_PLAY,
	AUDIO_MELODY_STOP,
	AUDIO_FREE
}audiodirection_t;

typedef struct melody_st
{
	FILE *fd;
//	char fileOpenFlag;
//	char playFlag;
	pthread_mutex_t mutex;
	int index;//index melody which is playing, 0: melody playback is off
}melody_st;

typedef struct audioConfig_st
{
	int fd_mixer,fd_dsp,spk_fd_mixer,spk_fd_dsp;

	char stop;

//	char bufferPCM[505*8*2];
//	unsigned int sizePCM;

	char bufferADPCM[AUDIO_BUFFER_SIZE];
	unsigned int sizeADPCM;

	pthread_mutex_t mutex;
	pthread_mutex_t mutex_recording;
	pthread_t   audio_thread;

	audiodirection_t direction;
	audiodirection_t status;

	/* socket to receive audio data */
	struct sockaddr_in server_address;
	struct sockaddr_in client_address;
	socklen_t sockaddr_len;
	int socketfd, newsocketfd,flag_accept;

	FILE *frecord;
	unsigned int fcount;

	melody_st melody;

	int spkvolume;
}audioConfig_st;

audioConfig_st audioConfig;

#define MOTOAWORKING	is_motor_ud_moving()
#define MOTOBWORKING	is_motor_lr_moving()
extern char motionCommandFlag;

//mls@dev03 20110909
#define	RAPIT_GPIO_NODE	"/dev/gpio0"
#define PANDA_BATTERY_LEVEL	50	//define by mlsdev008
#define SNDCTL_DSP_GETAIN2	1107
#define SNDCTL_DSP_GETAIN3	1108
#define SNDCTL_DSP_MIC_RESET_BUFFER	1109
#define RECBUF_GETALL	1110
#define RECBUF_CLEAR	1111
#define START_RECORDING 1112
#define TEMP_GET_PERIOD 5 //define period to get temperature value
typedef struct tempConfig_st
{
	int fd_gpio;
	int value;
	char stop;
	int period;//in seconds
	pthread_t working_thread;
	char readFlag;
}tempConfig_st;

int isStartRecording = 0;
tempConfig_st tempConfig;
int gAudioTestRecordEnable = 0;
///////////////////ADPCM encode/////////////////////
//define by others
#define BLOCK_TO_ADPCM(block_sample) 	\
			(block_sample - 1)/2 + (block_sample - 1)%2 + 4
#define BLOCK_TO_PCM(block_sample)	\
			block_sample * 2
#define ADPCM_TO_BLOCK(adpcm_data) \
			(adpcm_data - 4)*2 + 1

const int StepTab[ 89 ] = {

	7, 8, 9, 10, 11, 12, 13, 14,

	16, 17, 19, 21, 23, 25, 28, 31,

	34, 37, 41, 45, 50, 55, 60, 66,

	73, 80, 88, 97, 107, 118, 130, 143,

	157, 173, 190, 209, 230, 253, 279, 307,

	337, 371, 408, 449, 494, 544, 598, 658,

	724, 796, 876, 963, 1060, 1166, 1282, 1411,

	1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,

	3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,

	7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,

	15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,

	32767
};

static unsigned int debug_count = 0;
 // Static table used in the IMA/DVI ADPCM algorithm
const int IndexTab[ 16 ] = { -1, -1, -1, -1, 2, 4, 6, 8,
							-1, -1, -1, -1, 2, 4, 6, 8 };
void a_sample_from_pcm_to_ima_adpcm(int *__outAdpcm, short _inPcmSn,int * __sIndex,short * __inPrePcmSn)
{
	int diff = 0, _sIndex = *__sIndex,_outAdpcm = 0;
	short _inPrePcmSn = *__inPrePcmSn;

	static int snd_level=0;
	static int dec_soft_gain=0;

	if(lullaby_buffer_size == 0) // IN LULLABY MODE STOP ALL FILTERING
	{
		debug_count ++;
		snd_level = average_value/100;
		// snd_level 0 to 256, but actually should be under 128

		//snd_level=(snd_level*1999 + ( abs( _inPcmSn ) / 256 )) / 2000;


		_inPcmSn=_inPcmSn * soft_gain/128;
		if ((debug_count % 16000) == 0)
		{
			syslog(LOG_DEBUG, "S1=%d, G=%d, C=%d, P=%d\n",snd_level,soft_gain,_inPcmSn,_inPrePcmSn);
		}

		soft_gain_timer++;
		if ( soft_gain_timer >= soft_gain_rate ) {	  // Adust Gain in each 128 sample
			soft_gain_timer=0;
			if ( soft_gain_max_hold<soft_gain_max_hold_limit) soft_gain_max_hold++;

			if( snd_level < noise_gate_low ) {
				if ( (soft_gain > noise_gate_min_gain) && (soft_gain_max_hold>=soft_gain_max_hold_limit) ) {

					dec_soft_gain++;
					if ( dec_soft_gain==2 ) {
					dec_soft_gain=0;
					soft_gain-=noise_gate_decay_rate;
					if ( soft_gain < noise_gate_min_gain ) soft_gain=noise_gate_min_gain;
					}

				}
			}
			else {
				dec_soft_gain=0;
				if ( snd_level > noise_gate_high ) {
					if ( snd_level > soft_gain_limit ) {
						if ( soft_gain > soft_gain_max ) { 
							soft_gain=soft_gain_max;
							// 12-04-27
							// Reset max hold timer
							soft_gain_max_hold=0;

						}
					}
					else {
						if ( soft_gain < noise_gate_max_gain ) {
							soft_gain+=noise_gate_attack_rate;
							if ( soft_gain>=noise_gate_max_gain ) soft_gain=noise_gate_max_gain;
						}
					}
				}
			}
		}
		/////////////////////////////
	}


	diff = (int)_inPcmSn - (int)_inPrePcmSn;
    if(diff < 0)
    {
		_outAdpcm = 8;
		diff = -diff;
    }
    else
    {
    	_outAdpcm = 0;
    }
    if(diff >= StepTab[_sIndex])
    {
    	_outAdpcm = _outAdpcm | 4;
    	diff = diff - StepTab[_sIndex];
    }
    if(diff >= StepTab[_sIndex]>>1)
    {
    	_outAdpcm = _outAdpcm |2;
    	diff = diff - (StepTab[_sIndex]>> 1);
    }
    if(diff >= (StepTab[_sIndex] >> 2))
    {
    	_outAdpcm = _outAdpcm | 1;
    }
    diff = 0;
    if(_outAdpcm & 4)
		diff = diff + StepTab[_sIndex];
	if(_outAdpcm & 2)
		diff = diff + (StepTab[_sIndex] >> 1);
	if(_outAdpcm & 1)
		diff = diff + (StepTab[_sIndex] >> 2);
	diff = diff + (StepTab[_sIndex] >> 3);
	if(_outAdpcm & 8)
		diff = -diff;
	_inPrePcmSn = _inPrePcmSn + diff;
	if(_inPrePcmSn >= 32767) _inPrePcmSn = 32767;
	if(_inPrePcmSn < - 32767) _inPrePcmSn = -32767;
//	DEBUG("1********_sIndex:%d********_outAdpcm: %d\n",_sIndex,_outAdpcm);
	_sIndex = _sIndex + IndexTab[_outAdpcm];
	if(_sIndex < 0) _sIndex = 0;
	if(_sIndex > 88) _sIndex = 88;
//	DEBUG("2********_sIndex:%d********_outAdpcm: %d\n",_sIndex,_outAdpcm);
	//---------------------------
	*__sIndex = _sIndex;
	*__inPrePcmSn = _inPrePcmSn;
	*__outAdpcm = _outAdpcm;
}


int convert_pcm_to_adpcm(char * _pcmbuffer,int _BLOCKSAMPLE,char * _adpcmbuffer,int * _Index)
{
	//static short _inPrePcmSn = 0;
	if(_pcmbuffer == NULL)
	{

		return -1;
	}
	if(_adpcmbuffer == NULL)
	{
		printf("output buffer is NULL\n");
		return -1;
	}

	int inPos = 0, outPos = 0, i = 0;
	int _sindex = *_Index;



	short * _spcmbuffer = (short *) _pcmbuffer;
	short _inPrePcmSn = 0;
	if(lullaby_buffer_size != 0) // IN LULLABY MODE STOP ALL FILTERING
	{
		_inPrePcmSn = _spcmbuffer[0];
	}
	else
	{
		_inPrePcmSn = _spcmbuffer[0] * soft_gain/128;
	}
	_adpcmbuffer[outPos++] = _inPrePcmSn & 0xff;
	_adpcmbuffer[outPos++] = ( _inPrePcmSn >> 8 ) & 0xff;
	inPos+=2;

//	_adpcmbuffer[outPos++] = _pcmbuffer[inPos++] * soft_gain/64;
//	_adpcmbuffer[outPos++] = _pcmbuffer[inPos++] * soft_gain/64;



	_adpcmbuffer[outPos++] = _sindex & 0xff;

	_adpcmbuffer[outPos++] = 0;
	int outAdpcm = 0;
	for(i = 1; i < _BLOCKSAMPLE;i +=2)
	{
		a_sample_from_pcm_to_ima_adpcm(&outAdpcm, _spcmbuffer[i], &_sindex, &_inPrePcmSn);

		_adpcmbuffer[outPos] = outAdpcm;
		if( (i+1) < _BLOCKSAMPLE)
			a_sample_from_pcm_to_ima_adpcm(&outAdpcm, _spcmbuffer[i+1], &_sindex,&_inPrePcmSn);

		_adpcmbuffer[outPos] |= (outAdpcm << 4);
		outPos++;
	}
	*_Index = _sindex;
	return outPos;
}
/**********************************************************************
 *
 **********************************************************************/
void a_sample_from_ima_adpcm_to_pcm(int _inAdpcm, short *_outPcmSn,
										int * _Index,short * _inPrePcmSn)
{
	short outPcmSn = *_outPcmSn, inPrePcmSn = *_inPrePcmSn;
	int index = *_Index, diff = 0;
	/*************************************/
	if(_inAdpcm & 4)
		diff = diff + StepTab[index];
	if(_inAdpcm & 2)
		diff = diff + (StepTab[index] >> 1);
	if(_inAdpcm & 1)
		diff = diff + (StepTab[index] >> 2);
	diff = diff + (StepTab[index] >> 3);

	if(_inAdpcm & 8)
		diff = -diff;
	outPcmSn = inPrePcmSn + diff;
	if(outPcmSn >= 32767) outPcmSn = 32767;
	if(outPcmSn <= -32768) outPcmSn = -32768;
	inPrePcmSn = outPcmSn;

	index = index + IndexTab[_inAdpcm];
	if(index > 88) index = 88;
	if(index < 0) index = 0;
	/****************************************/
	*_outPcmSn = outPcmSn;
	*_inPrePcmSn = inPrePcmSn;
	*_Index = index;
}

int convert_adpcm_to_pcm(char * _adpcmbuffer,int _BLOCKSAMPLE,
								char * _pcmbuffer)
{
	if(_adpcmbuffer == NULL )
	{
		printf("_adpcmbuffer == NULL \n");
		return -1;
	}
	if(_pcmbuffer == NULL)
	{
		printf("_pcmbuffer == NULL\n");
		return -1;
	}
	int  _index = 0,inPos = 0, outPos = 0;
	int size = BLOCK_TO_ADPCM(_BLOCKSAMPLE), i = 0;
	short preSnSample = ((short *)_adpcmbuffer)[0], snSample = 0;
	_pcmbuffer[outPos++] = _adpcmbuffer[inPos++];
	_pcmbuffer[outPos++] = _adpcmbuffer[inPos++];
	_index = (int)_adpcmbuffer[inPos++];
	inPos++;//byte reserve
	for(i = 0; i < size - 4; i++)
	{
		int inAdpcm = (int)(_adpcmbuffer[inPos] &0x0f);
		a_sample_from_ima_adpcm_to_pcm(inAdpcm, &snSample,&_index,&preSnSample);
		_pcmbuffer[outPos++] = snSample &0xff;
		_pcmbuffer[outPos++] = (snSample >> 8) & 0xff;

		inAdpcm = (int)((_adpcmbuffer[inPos] >> 4) & 0x0f);
		a_sample_from_ima_adpcm_to_pcm(inAdpcm, &snSample,&_index,&preSnSample);
		_pcmbuffer[outPos++] = snSample &0xff;
		_pcmbuffer[outPos++] = (snSample >> 8) & 0xff;
		inPos++;
	}
	return outPos;
}

//////////////////////////////////SPEAKER/////////////////////////////////////////////////
mlsErrorCode_t mlsaudioSpkInit()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	printf("init spk device \n");
	int format = FORMAT, sample_rate = SAMPLE_RATE, channel = CHANNEL,freg = 0;

	//init audio
	if ((audioConfig.spk_fd_dsp >= 0)  || (audioConfig.spk_fd_mixer >= 0))
	{

		audioConfig.flag_accept = 1;
		return retVal;
	}
	audioConfig.spk_fd_dsp = open("/dev/dsp", O_RDWR);
	if( audioConfig.spk_fd_dsp < 0 ){

		retVal = MLS_ERROR;
		return retVal;
	}
	audioConfig.spk_fd_mixer = open("/dev/mixer", O_RDWR);
	if( audioConfig.spk_fd_mixer < 0 ){

		retVal = MLS_ERROR;
		return retVal;
	}

#if 1
	ioctl(audioConfig.spk_fd_dsp, SNDCTL_DSP_SETFMT, &format);
	ioctl(audioConfig.spk_fd_mixer, MIXER_WRITE(SOUND_MIXER_PCM), &audioConfig.spkvolume);
	ioctl(audioConfig.spk_fd_dsp, SNDCTL_DSP_SPEED, &sample_rate);
	ioctl(audioConfig.spk_fd_dsp, SNDCTL_DSP_CHANNELS, &channel);
	ioctl(audioConfig.spk_fd_dsp, SNDCTL_DSP_GETBLKSIZE, &freg);

#endif

	audioConfig.flag_accept = 1;
	is_speaker_init = 1;
	return retVal;
}

mlsErrorCode_t mlsaudioSpkDeInit()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;


	//device
	if (audioConfig.spk_fd_dsp >=0)
		close(audioConfig.spk_fd_dsp);
	if (audioConfig.spk_fd_mixer >=0)
		close(audioConfig.spk_fd_mixer);

	audioConfig.spk_fd_dsp = audioConfig.spk_fd_mixer = -1;

	if (audioConfig.newsocketfd >= 0)
	{
		close(audioConfig.newsocketfd);
		audioConfig.newsocketfd = -1;
	}
	is_speaker_init = 0;
	return retVal;
}

mlsErrorCode_t mlsaudioSpkVolume(int volume)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	audioConfig.spkvolume = volume;
	if (audioConfig.spk_fd_mixer > 0)
	{
		ioctl(audioConfig.spk_fd_mixer, MIXER_WRITE(SOUND_MIXER_PCM), &audioConfig.spkvolume);
	}

	return retVal;
}
mlsErrorCode_t mlsaudioGetSpkVolume(int *volume)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	*volume = audioConfig.spkvolume;

	return retVal;
}
/////////////////////////////////MIC ////////////////////////////////////////////////
mlsErrorCode_t mlsaudioMicInit()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	int iret;
	int channel = CHANNEL, sample_rate = SAMPLE_RATE, volume = VOLUME,
		freg = 0;


	audioConfig.sizeADPCM = 0;
	if ((audioConfig.fd_dsp >= 0)  || (audioConfig.fd_mixer >= 0))
	{
		printf("Mic already init\n");
		return retVal;
	}


	audioConfig.fd_dsp = open("/dev/dsp1",O_RDWR);	//open dsp device
	if(audioConfig.fd_dsp < 0)
	{
		printf("open dsp device to record error\n");
		retVal = MLS_ERROR;
		return retVal;
	}
	audioConfig.fd_mixer = open("/dev/mixer1",O_RDWR);	//open mixer device
	if(audioConfig.fd_mixer < 0)
	{
		printf("open mixer device to control\n");
		retVal = MLS_ERROR;
		return retVal;
	}
	//init audio
	iret = ioctl(audioConfig.fd_mixer, MIXER_WRITE(SOUND_MIXER_MIC), &volume);	/* set MIC max volume */
	if (iret < 0)
	{
		printf("ioctl audio error\n");
		retVal = MLS_ERROR;
		return retVal;
	}

	iret = ioctl(audioConfig.fd_dsp, SNDCTL_DSP_SPEED, &sample_rate);
	if (iret < 0)
	{
		printf("ioctl audio error\n");
		retVal = MLS_ERROR;
		return retVal;
	}

	iret = ioctl(audioConfig.fd_dsp, SNDCTL_DSP_CHANNELS, &channel);
	if (iret < 0)
	{
		printf("ioctl audio error\n");
		retVal = MLS_ERROR;
		return retVal;
	}
	iret = ioctl(audioConfig.fd_dsp, SNDCTL_DSP_GETBLKSIZE, &freg);
	if (iret < 0)
	{
		printf("ioctl audio GETBLKSIZE error\n");
		retVal = MLS_ERROR;
		return retVal;
	}
	else
	{
	    printf("Audio Blocksize = %d\n",freg);
	}

	

	mlsioSetReg(0xF800E030,0x8070C230);
	mlsioSetReg(0xF800E034,0x40013810); // AGC ON


	return retVal;
}

mlsErrorCode_t mlsaudioMicDeInit()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

/*
	if (audioConfig.fd_dsp >= 0)
	//close dsp device
		close(audioConfig.fd_dsp);

	if (audioConfig.fd_mixer >= 0)
		//close mixer device
		close(audioConfig.fd_mixer);
	audioConfig.fd_dsp = audioConfig.fd_mixer = -1;
*/
	return retVal;
}


mlsErrorCode_t mlsaudioOpenPCMLog()
{
	if (audioConfig.frecord == NULL)
	{
	    audioConfig.frecord = fopen("/mlsrb/mlswwwn/recrb.pcm","w+");
	    if (audioConfig.frecord == NULL)
	    {
		printf("open recrb.pcm error\n");
		//retVal = MLS_ERROR;
		return MLS_ERROR;
	    }
	    audioConfig.fcount = 0;
	}
	return MLS_SUCCESS;
}

mlsErrorCode_t mlsaudioClosePCMLog()
{
    if (audioConfig.frecord != NULL)
    {
	fclose(audioConfig.frecord);
	audioConfig.frecord = NULL;
    }
    return MLS_SUCCESS;
}


#include "VoiceFilterBank_msc3.h"
short filter_output[BLOCKSAMPLE];
void *audio_thread(void *arg)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	char *pcmbuffer = NULL, *pointer = NULL,*finalpcmbuffer=NULL;
	char *adpcmbuffer = NULL;

	int written_data = 0;
	int Index = 0;
	int err = -1;
	int readsize = BLOCKSAMPLE*2;
	int current_data,delay,bytes;
	int audio_buffer_count=0;
	audio_buf_info audio_info;

//	static char pcmbuffer[1024*2];

	//define vox by mlsdev008 15/11/2011
	int vox_status = 0;
	if(pthread_mutex_init(&dataMicMutex,NULL) != 0)
	{
		perror("init dataMicMutex error");
	}
	if(pthread_cond_init(&dataAvailable,NULL) != 0)
	{
		perror("init dataAvailable error");
	}

	pcmbuffer = (char *)malloc(BLOCK_SAMPLE_READ *2); //buffer contain pcm data
	if(pcmbuffer == NULL)
	{
		printf("allocate memory for pcmbuffer has problem\n");
		return NULL;
	}
	else
	{
		memset(pcmbuffer,'\0', (BLOCKSAMPLE*2));
	}
	VoiceFilterBank_init();
	adpcmbuffer = (char *)malloc((BLOCKSAMPLE + BLOCKSAMPLE%2) + 4); //buffer contain adpcm data
	if(adpcmbuffer == NULL)
	{
		printf("allocate memory for adpcmbuffer err\n");
		return NULL;
	}
	else
	{
		memset(adpcmbuffer,'\0',((BLOCKSAMPLE + BLOCKSAMPLE%2) + 4));
	}

//	memset(adpcmbuffer,'\0',((BLOCKSAMPLE/2 + BLOCKSAMPLE%2) + 4));
//	memset(pcmbuffer,'\0', (BLOCKSAMPLE*2));
	written_data = BLOCKSAMPLE*2;
	pointer = pcmbuffer;

#if defined (RECORD_READ_FILE)
	/* open pcm file */
	FILE* pFile = fopen("/mlsrb/demo_8k_16bit.wav","rb");
	if (pFile == NULL)
	{
		perror("can not open audio pcm file \n");
		while(1);
	}
#endif
#ifdef CONFIG_NORECINT
	printf("*************USE STEPH SOLUTION\n");
#endif

    //!Init postoffice
    PostOfficeInit();
	// Init first and no need to deinit anymore
	mlsaudioMicInit();
	//mlsaudioSpkInit();

	while (!audioConfig.stop)
	{
		if (audioConfig.direction == AUDIO_STREAM_MIC)
		{
			/////////////////////MIC recording/////////////////////////////////////
			if (audioConfig.status == AUDIO_INIT)
			{
//				printf("mic init \n");
				pthread_mutex_lock(&audioConfig.mutex_recording);
				audioConfig.status = AUDIO_STREAM_MIC;
				pthread_mutex_unlock(&audioConfig.mutex_recording);
				if (retVal != MLS_SUCCESS)
				{

					retVal = MLS_ERROR;
//					return retVal;
				}

				readsize = BLOCKSAMPLE*2;
//				Index = 0;


			}
			else if (audioConfig.status == AUDIO_STREAM_SPEAKER)
			{

				if (audioConfig.newsocketfd >= 0)
				{
					close(audioConfig.newsocketfd);
					audioConfig.newsocketfd = -1;
				}

				ioctl(audioConfig.fd_dsp,SNDCTL_DSP_MIC_RESET_BUFFER,NULL);
				usleep(200*1000);

				pthread_mutex_lock(&audioConfig.mutex_recording);
				mlsaudioSpkDeInit();

				audioConfig.status = AUDIO_STREAM_MIC;
				pthread_mutex_unlock(&audioConfig.mutex_recording);
				usleep(200*1000);
				if (is_mic_muted)
				{
					//printf("Unmute the Mic\n");
					mlsioSetReg(0xF800E030,0x8070C230);
					mlsioSetReg(0xF800E034,0x40013810); // AGC ON
					is_mic_muted = 0;
				}

				readsize = BLOCKSAMPLE*2;
				Index = 0;
#ifdef CONFIG_NORECINT
				ioctl(audioConfig.fd_dsp,RECBUF_CLEAR,NULL);
#endif
			}

//			printf("read data from micro device\n");

#ifndef CONFIG_NORECINT
			ioctl(audioConfig.fd_dsp,SNDCTL_DSP_GETISPACE,&audio_info);
			//printf("Mic free %d bytes\n",audio_info.bytes);
			readsize = BLOCKSAMPLE*2; // 2KB
			if (audio_info.bytes >= readsize) // 2KB
			{
				err = read(audioConfig.fd_dsp,pcmbuffer,readsize);
				if (err < 0)
				{
					printf("read audio dsp device error\n");
					continue;
				}
				readsize = err;
			}
			else
			{
				readsize = 0;
			}
			
#else
			if (isStartRecording == 0)
			{
			    ioctl(audioConfig.fd_dsp,START_RECORDING,NULL);
			    isStartRecording = 1;
			}
			ioctl(audioConfig.fd_dsp,SNDCTL_DSP_MIC_RESET_BUFFER,NULL);
			readsize = ioctl(audioConfig.fd_dsp, RECBUF_GETALL, pcmbuffer);	
#endif

			pthread_mutex_lock(&audioConfig.melody.mutex);
			//printf("DC 3\n");

			if (lullaby_buffer_size) 
			{
				if (is_speaker_init == 0)
				{

					mlsaudioSpkInit();
				}

				{
					int bytes_sent =0;
					audio_buf_info bi;
					bytes_sent = lullaby_buffer_size - lullaby_buffer_ptr;
					ioctl(audioConfig.spk_fd_dsp,SNDCTL_DSP_GETOSPACE,&bi);
					if (bi.bytes > 2 * 1024)
					{
						if (bytes_sent > (2*1024) ) bytes_sent = (2*1024);
						write(audioConfig.spk_fd_dsp,lullaby_buffer+lullaby_buffer_ptr,bytes_sent);
						lullaby_buffer_ptr += bytes_sent;
						if (lullaby_buffer_ptr == lullaby_buffer_size) lullaby_buffer_ptr = 0; //rewind
					}
				}
			}
			else
			{

				if (is_speaker_init == 1)
				{
				
					mlsaudioSpkDeInit();
				}
			}

			pthread_mutex_unlock(&audioConfig.melody.mutex);

		   if (readsize == 0)
            {

                usleep(20*1000);
                continue;
            }









			mlsVoxGetStatus(&vox_status);
			{
				short max = 0,average=0;
				int index=0;
				int total = 0;
				vox_buf = (short*)pointer;
				
				#define CLICK_BEFORE_THRESHOLD		3000
				#define CLICK_AFTER_THRESHOLD		2500
				if(pthread_mutex_lock(&dataMicMutex))
					perror("pthread mutex lock dataMicMutex");
				int i = 0;
				if (ADC_reading_flag > 0)
				{
				    audio_buffer_count++;
				    //if (audio_buffer_count > 5)
				    //{
				   //	ADC_reading_flag = 2;
				//    }
				    if (audio_buffer_count > 7)
				    {
	    				ADC_reading_flag = 0;
					audio_buffer_count = 0;
				    }
				}
				for(i = 0; i < BLOCKSAMPLE; i++)
				{
					//vox_buf[i]=0;
					total += abs(vox_buf[i]);
					if (max < abs(vox_buf[i]))
					{
					    max=abs(vox_buf[i]);
					    index = i;
					}
					vox_count++;
				}
				total_vox += total;
				average = total / BLOCKSAMPLE;
				average_value = average;
				ADC_reading_flag = 10;
				
				if (ADC_reading_flag == 1)
				{
				}
				
				#define max(a,b) ((a>b)?a:b)
				if ((average < 2000) && (ADC_reading_flag==1)) 
				{
				    if ( (index > 2) && (index < BLOCKSAMPLE-4))
				    {
					static int bef_change,aft_change;
					bef_change = max (abs(vox_buf[index]-vox_buf[index-1]),abs(vox_buf[index]-vox_buf[index-2]));
					aft_change = max (abs(vox_buf[index]-vox_buf[index+1]),abs(vox_buf[index]-vox_buf[index+2]));
					if(
					    (bef_change > CLICK_BEFORE_THRESHOLD)
					    &&
					    (aft_change > CLICK_AFTER_THRESHOLD)
				        )
				        {
				    	    if (vox_buf[index-3]>vox_buf[index+3])
				    	    {
				    	    
				    		int step = (vox_buf[index-3]-vox_buf[index+3])/6;
				    		for (i = 1;i<6;i++)
				    		{
				    		    vox_buf[index-3+i] = vox_buf[index-3] - i*step;
				    		}
				    	    

				    	    }
				    	    else
				    	    {
				    	    
				    		int step = (vox_buf[index+3]-vox_buf[index-3])/6;
				    		for (i = 1;i<6;i++)
				    		{
				    		    vox_buf[index+3-i] = vox_buf[index+3] - i*step;
				    		}
				    	    

				    	    }
				    	    ADC_reading_flag = 0;
				        }
				    }
				    else
				    {

				    }

				}


				if(pthread_mutex_unlock(&dataMicMutex))

				if(vox_count >= AMOUNT)
				{

					if (vox_status > 0)

					    pthread_cond_broadcast(&dataAvailable);
					else
					{
					    vox_count = 0;
					    total_vox = 0;
					}
				}
			}

			if (((MOTOAWORKING == 1)||(MOTOBWORKING == 1)) && (is_mic_muted == 0))
			{
				mlsioSetReg(0xF800E030,0x8070C000);  
				mlsioSetReg(0xF800E034,0x00013810); 
				is_mic_muted = 1;
			}


			if (((MOTOAWORKING == 0)&&(MOTOBWORKING == 0)) && (is_mic_muted == 1))
			{
				mlsioSetReg(0xF800E030,0x8070C230);
				mlsioSetReg(0xF800E034,0x40013810); // AGC ON
				is_mic_muted = 0;
			}

#ifdef FILE_TEST_RECORD
		    if (gAudioTestRecordEnable)
		    {
			//mlsaudioOpenPCMLog();
			if (audioConfig.fcount < 10*1024*1024 && (readsize > 0))
			{
				fwrite(pcmbuffer,readsize,1,audioConfig.frecord);
				audioConfig.fcount += readsize;
			}
		    }
#endif

		    if (lullaby_buffer_size == 0)  
		    {
		    	VoiceFilterBank_Filtering((short*)pcmbuffer,BLOCKSAMPLE,filter_output);

		    	finalpcmbuffer = (char*)filter_output;
		    }
		    else    	
		    {
		    	finalpcmbuffer = pcmbuffer;
		    }


		    	{
        		    err = convert_pcm_to_adpcm(finalpcmbuffer,BLOCKSAMPLE,adpcmbuffer,&Index);
        			if(err < 0)
        			{
        			
        				return NULL;
        			}

		    		if(PostOffice_SendMail(adpcmbuffer,err) != 0)
		    		{
		    		
		    		}
		    	}

			usleep(1000*30);
		}
		else if (audioConfig.direction == AUDIO_STREAM_SPEAKER)
		{


			if (audioConfig.status == AUDIO_INIT)
			{
				audioConfig.status = AUDIO_STREAM_SPEAKER;
				audioConfig.flag_accept = 1;
			}
			else if (audioConfig.status == AUDIO_STREAM_MIC)
			{
				pthread_mutex_lock(&audioConfig.mutex_recording);
				audioConfig.flag_accept = 1;
				audioConfig.status = AUDIO_STREAM_SPEAKER;

				if (is_speaker_init == 0)
				{

					mlsaudioSpkInit();
				}
				pthread_mutex_unlock(&audioConfig.mutex_recording);
				if (is_mic_muted == 0)
				{
					mlsioSetReg(0xF800E030,0x8070C000);  // Volume OFF
					mlsioSetReg(0xF800E034,0x00013810); // AGC OFF
					is_mic_muted = 1;
				}
			}

#if !defined (USE_SOCKET_UDP)
			if(audioConfig.flag_accept)
			{
				struct timeval tv;
				//printf("waiting................\n");
				audioConfig.newsocketfd = accept(audioConfig.socketfd,(struct sockaddr *) &audioConfig.client_address,&audioConfig.sockaddr_len);
				if (audioConfig.newsocketfd < 0)
				{

					usleep(1000*200);
					continue;
				}

				tv.tv_sec = 5;  /* Secs Timeout */
				tv.tv_usec = 0;



				setsockopt(audioConfig.newsocketfd, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));


				audioConfig.flag_accept = 0;
			}


			err = read(audioConfig.newsocketfd,pcmbuffer,BLOCK_SAMPLE_READ*2);

#else

			err = recvfrom(audioConfig.socketfd,pcmbuffer,BLOCK_SAMPLE_READ*2,0, (struct sockaddr*)&audioConfig.client_address, &audioConfig.sockaddr_len);
#endif
			if(err <= 0)
			{
				audioConfig.flag_accept = 1;
				close(audioConfig.newsocketfd);
				continue;
			}
			else current_data = err;
#if 1
			write(audioConfig.spk_fd_dsp, pcmbuffer, current_data);


#if 1
			ioctl(audioConfig.spk_fd_dsp,SNDCTL_DSP_GETISPACE,&audio_info);
			if (audio_info.bytes > 20000)
			{
				//printf("Mic Device Reset Buffer\n");
				ioctl(audioConfig.fd_dsp,SNDCTL_DSP_MIC_RESET_BUFFER);  // reset mic buffer
			}
#endif
			ioctl(audioConfig.spk_fd_dsp,SNDCTL_DSP_GETODELAY,&bytes);
			delay = (bytes *1000) / (SAMPLE_RATE * 2 * CHANNEL);

			if (delay > 0)
			{
				usleep(delay * 1000);
			}
			else
			{
				usleep(1000*50);
			}


#endif
		}
		else //nothing to do
		{
			sleep(1);
		}
	}

	free(pcmbuffer);
	free(adpcmbuffer);

#if defined (RECORD_READ_FILE)
	/* open pcm file */
	fclose(pFile);
#endif

	return NULL;
}

mlsErrorCode_t audio_thread_run()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;



	//start thread for motor A
	if( pthread_create(&audioConfig.audio_thread, 0, audio_thread, NULL) != 0)
	{

		retVal = MLS_ERROR;
		return retVal;
	};

	pthread_detach(audioConfig.audio_thread);

	return retVal;
}
////////////////////////////////////////////////////////////////////////////////////////
mlsErrorCode_t mlsaudioInit()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;



	GetAudioFineTuneConfig(&audio_finetune);


	mlsaudioSetFinetune (audio_finetune->finetune1,audio_finetune->finetune2);
	audioConfig.fd_mixer=audioConfig.fd_dsp=audioConfig.spk_fd_mixer = audioConfig.spk_fd_dsp = -1;

#ifdef FILE_TEST_RECORD
#if 0 // not init at boot any more. Instead init when request
	audioConfig.frecord = fopen("/mlsrb/mlswwwn/recrb.pcm","w+");
	if (audioConfig.frecord == NULL)
	{
		printf("open recrb.pcm error\n");
		retVal = MLS_ERROR;
		return retVal;
	}
	audioConfig.fcount = 0;
#else
	audioConfig.frecord = NULL;
#endif
#endif

#if 1
	tempConfig.fd_gpio = open(RAPIT_GPIO_NODE, O_RDWR);
	if (tempConfig.fd_gpio <= 0)
	{
		printf("Error open /dev/gpio0\n");
		return -1;
	}
#endif
	audioConfig.spkvolume = VOLUME;
	audioConfig.sizeADPCM = 0;

	audioConfig.stop = 0;
	tempConfig.stop = 0;
	tempConfig.value = 0;
	tempConfig.period = TEMP_GET_PERIOD;

	audioConfig.status = AUDIO_INIT;
	audioConfig.direction = AUDIO_STREAM_MIC;
	audioConfig.melody.fd = 0;
	audioConfig.melody.index = 0;
	audioConfig.socketfd = -1;

	lullaby_buffer = malloc(1024*1024);
	if (lullaby_buffer == NULL)
	{
		printf("********ERROR NOT ENOUGH 1MB TO MALLOC LULLABY BUFFER\n");
		return -1;
	}
//	audioConfig.direction = AUDIO_STREAM_SPEAKER;
	//mls@dev03 20110805 add speaker
//	mlsaudioSpkInit();

	/* this mutex and the conditional variable are used to synchronize access to audio buffer  */
	if( pthread_mutex_init(&audioConfig.mutex, NULL) != 0 )
	{
		printf("could not initialize audio mutex variable\n");
		retVal = MLS_ERROR;
		return retVal;
	}

	/* this mutex and the conditional variable are used to synchronize access to audio recording  */
	if( pthread_mutex_init(&audioConfig.mutex_recording, NULL) != 0 )
	{
		printf("could not initialize mic mutex variable\n");
		retVal = MLS_ERROR;
		return retVal;
	}

	/* this mutex and the conditional variable are used to synchronize access to audio buffer  */
	if( pthread_mutex_init(&audioConfig.melody.mutex, NULL) != 0 )
	{
		printf("could not initialize melody mutex variable\n");
		retVal = MLS_ERROR;
		return retVal;
	}

	////////////////////////////////////////////////////////////////////////
	/* init socket server to wait audio data */
	audioConfig.newsocketfd = -1;
	audioConfig.sockaddr_len = sizeof(audioConfig.server_address);
	audioConfig.server_address.sin_family = AF_INET;
	audioConfig.server_address.sin_port = htons(PORT_INET);
	audioConfig.server_address.sin_addr.s_addr = htonl(INADDR_ANY); //use my ip
#if !defined (USE_SOCKET_UDP)
	audioConfig.socketfd = socket(AF_INET,SOCK_STREAM,0);
#else
	audioConfig.socketfd = socket(AF_INET,/*SOCK_STREAM*/SOCK_DGRAM,IPPROTO_UDP);
#endif
	if(audioConfig.socketfd < 0)
	{
		printf("socket in server has problem\n");
		retVal = MLS_ERROR;
		return retVal;
	}

	if(bind(audioConfig.socketfd,(struct sockaddr *) &audioConfig.server_address,sizeof(audioConfig.server_address)) < 0)
	{
		printf("bind() in server has problem\n");
		retVal = MLS_ERROR;
		return retVal;
	}

#if !defined (USE_SOCKET_UDP)
	listen(audioConfig.socketfd,1);//listen for connection
	/* ignore "socket already in use" errors */
	retVal = 1;
	if (setsockopt(audioConfig.socketfd, SOL_SOCKET, SO_REUSEADDR, &retVal, sizeof(retVal))
			< 0) {
		perror("setsockopt(SO_REUSEADDR) failed");
		exit(EXIT_FAILURE);
	}
	retVal = 0;

	//mls@dev03 20110806 set NONBLOCK flag for socket
	fcntl(audioConfig.socketfd, F_SETFL, O_NONBLOCK | (fcntl(audioConfig.socketfd, F_GETFL,0)));
#endif
	/////////////////////////////////////////////////////////////////////////////

	retVal = mlsaudioStart();


	retVal = temperature_thread_run();
	if (retVal != MLS_SUCCESS)
	{

		return retVal;
	}

	

	return retVal;
}

mlsErrorCode_t mlsaudioDeInit()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	audioConfig.stop = 1;
	tempConfig.stop = 1;
	usleep(1000*1000);

	pthread_cancel(audioConfig.audio_thread);
	pthread_cancel(tempConfig.working_thread);



		mlsaudioSpkDeInit();

		mlsaudioMicDeInit();

	if (audioConfig.socketfd >= 0)
		close(audioConfig.socketfd);
	if (lullaby_buffer)
	{
		free(lullaby_buffer);
	}


	close(tempConfig.fd_gpio);

	return retVal;
}

mlsErrorCode_t mlsaudioGetData(char* bufferout, unsigned int *size)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	static int countOver = 0;

	if (audioConfig.direction == AUDIO_STREAM_MIC)
	{
		pthread_mutex_lock(&audioConfig.mutex);

		if ((MOTOAWORKING == 0)&&(MOTOBWORKING == 0)&&(countOver == 0))
		{
			*size = audioConfig.sizeADPCM;
			memcpy(bufferout,audioConfig.bufferADPCM,*size);

#ifdef FILE_TEST_RECORD0
			if (audioConfig.fcount < 10*1024*1024)
			{
				fwrite(bufferout,*size,1,audioConfig.frecord);
				audioConfig.fcount += *size;
			}
#endif

			//	printf("mlsaudio: get data and size %d\n",*size);

			//reset audio buffer
			audioConfig.sizeADPCM = 0;
			
		}
		else 
		{
			*size = audioConfig.sizeADPCM;
			memset(bufferout,0x00,*size);
			audioConfig.sizeADPCM = 0;
			if ((MOTOAWORKING == 1)||(MOTOBWORKING == 1))
			{
				countOver = 15;
			}
			else
			{
				countOver -= 1;
			}
		}
		pthread_mutex_unlock(&audioConfig.mutex);
	}
	else
	{
		*size = 0;
	}

//	printf("size of audio data %d \n",*size);

	return retVal;
}

mlsErrorCode_t mlsaudioStart()
{
	return audio_thread_run();
//	return MLS_SUCCESS;
}

static int mlsaudioMelodyExtractSong(char* filename)
{
	FILE* fp;
	char* ptr;
	int size,current_data ;
	fp = fopen(filename,"rb");
	char adpcmbuffer[256];

	if (!fp)
	{

		return -1;
	}
	ptr = lullaby_buffer;
	size = 0;
	fseek (fp,WAVE_HEADER,SEEK_SET);
	while(fread(adpcmbuffer,1,256,fp) == 256)  
	{
		current_data = convert_adpcm_to_pcm(adpcmbuffer,ADPCM_TO_BLOCK(256),(lullaby_buffer+size));
		size+= current_data;
	}
	fclose(fp);
	lullaby_buffer_size = size;
	lullaby_buffer_ptr = 0;
	return 0;
}

mlsErrorCode_t mlsaudioMelodyInit(char melodyIndex)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	pthread_mutex_lock(&audioConfig.melody.mutex);

	switch (melodyIndex)
	{
	case 1:
		audioConfig.melody.index = 1;
		mlsaudioMelodyExtractSong(MELODY_FILE1);
		break;
	case 2:
		audioConfig.melody.index = 2;
		mlsaudioMelodyExtractSong(MELODY_FILE2);
		break;
	case 3:
		audioConfig.melody.index = 3;
		mlsaudioMelodyExtractSong(MELODY_FILE3);
		break;
	case 4:
		audioConfig.melody.index = 4;
		mlsaudioMelodyExtractSong(MELODY_FILE4);
		break;
	case 5:
		audioConfig.melody.index = 5;
		mlsaudioMelodyExtractSong(MELODY_FILE5);
		break;

	default:
		lullaby_buffer_size = 0;
		lullaby_buffer_ptr = 0;
		break;
	}

	pthread_mutex_unlock(&audioConfig.melody.mutex);

	return retVal;
}

mlsErrorCode_t mlsaudioMelodyDeInit()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	pthread_mutex_lock(&audioConfig.melody.mutex);

	/* close melody file */
	lullaby_buffer_size = 0;
	audioConfig.melody.index = 0;
	pthread_mutex_unlock(&audioConfig.melody.mutex);

	return retVal;
}



mlsErrorCode_t mlsaudioSetDirection(in_cmd_type controlID)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	switch (controlID)
	{
	  case IN_CMD_MICDEV_ON:
		  audioConfig.direction = AUDIO_STREAM_SPEAKER;
		  break;
	  case IN_CMD_MICDEV_OFF:
		  audioConfig.direction = AUDIO_STREAM_MIC;
		  break;
	  case IN_CMD_MELODY1_PLAY:
		  retVal = mlsaudioMelodyInit(1);
		  break;
	  case IN_CMD_MELODY2_PLAY:
		  retVal = mlsaudioMelodyInit(2);
		  break;

	  case IN_CMD_MELODY3_PLAY:
		  retVal = mlsaudioMelodyInit(3);
		  break;
	  case IN_CMD_MELODY4_PLAY:
		  retVal = mlsaudioMelodyInit(4);
		  break;
	  case IN_CMD_MELODY5_PLAY:
		  retVal = mlsaudioMelodyInit(5);
		  break;
	  case IN_CMD_MELODY_STOP:
		  retVal = mlsaudioMelodyDeInit();
		  break;
	  default:
		  break;
	}

	return retVal;
}
//////////////////////////////////////////////////////////////////////////////////////////////////
mlsErrorCode_t _mlsadcCalcTempC(int valueAdc, int *valueTempC)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	float V,R,T,t;
	int n=0;
	static int counter = 0;
	//static char logstr[256];
	static int avarage = 0;
	counter++;
	avarage = valueAdc;

	syslog(LOG_DEBUG,"->avarage:%d->",avarage);

	V = ((float)avarage*3.3)/1023;

	R = (3.3*100000)/V - 100000;

	T = 1/(((log10(R)-4.0605)*2.3026/3850)+ 1/358.15);

	t = (T - 273.15)*10;
	*valueTempC = (int)(t);
	

	if (counter > 15)
	{
	    *valueTempC -= 5; // minus 0.5 Celsius 
	}
	if (counter > 30)
	{
	    *valueTempC -= 5; // minus 0.5 Celsius 
	}
	if (counter > 45)
	{
	    *valueTempC -= 5; // minus 0.5 Celsius 
	}
	if (counter > 60)
	{
	    *valueTempC -= 5; // minus 0.5 Celsius 
	}
	//*/
    if(getHardwareVersion()==2)
        n = 10;
    else
        n = 0;
	if (*valueTempC < 130)
	{
	    *valueTempC -= (n+3)*10 +mlsIcam_GetValueTempOffset();
	}
	else if ((*valueTempC >= 130) && (*valueTempC < 460))
	{
	    *valueTempC -= (n+2)*10 +mlsIcam_GetValueTempOffset();
	}
 	else if (*valueTempC >= 450)
 	{
 	    *valueTempC -= (n+3)*10 +mlsIcam_GetValueTempOffset() ;
 	}
	*valueTempC += 5;
	*valueTempC /= 10;


	return retVal;
}
/*
 * control leg
 */

//#define LIGHT_SENSOR_DEBUG 1

mlsErrorCode_t _mlsAdcCtrlLed(int valueAIN3,char *status)
{
#define LIGHT_THRESOLD_ON_CDS_TYPE1  540
#define LIGHT_THRESOLD_OFF_CDS_TYPE1 234
#define LIGHT_THRESOLD_ON_CDS_TYPE0  900
#define LIGHT_THRESOLD_OFF_CDS_TYPE0 450
#define LIGHT_THRESOLD_ON_CDS_TYPE2  1000
#define LIGHT_THRESOLD_OFF_CDS_TYPE2 800


	static int  max = 0, index = 0, avarage = 0,countTimes = 0;
	static int valueOn3 = LIGHT_THRESOLD_OFF_CDS_TYPE0,flagReadValueOn3 = -1;
	unsigned short cds_type = mlsIcam_GetCDSType();
	static int threshold_on=0,threshold_off=0;
	static int ir_reflect_counter = 0,ir_reflect_time = 0,ir_reflect_flag=0;
	
	avarage = valueAIN3;
#ifndef LIGHT_SENSOR_DEBUG

#else

#endif


#if 0 // new implementation
    
    countTimes++;
#ifdef LIGHT_SENSOR_DEBUG

#endif
    if (cds_type == 1)
    {
	threshold_on = LIGHT_THRESOLD_ON_CDS_TYPE1;
	threshold_off = LIGHT_THRESOLD_OFF_CDS_TYPE1;
    }
    else if(csd_type == 0) // type 0
    {

	threshold_on = LIGHT_THRESOLD_ON_CDS_TYPE0;
	threshold_off = LIGHT_THRESOLD_OFF_CDS_TYPE0;    
    }
    else if(cds_type == 99){
	
	threshold_on = pTempOffset->cds99_IR_On;
	threshold_off = pTempOffset->cds99_IR_Off;
    }
    else {
      ;
    }

	if ((*status == SETUPLED_OFF) && (valueAIN3 > threshold_on))
	{
			inVal_IR_LED[1] = SETUPLED_ON;
#ifndef LIGHT_SENSOR_DEBUG

#else

#endif

			mlssensorIRLED(1);
			controlLed(inVal_IR_LED);
			*status = cur_irled_status = SETUPLED_ON;			
			countTimes = 0;
	}
	
	else if ((*status == SETUPLED_ON) && (valueAIN3 < threshold_off))
	{
	
		

			if (countTimes == 1 ) // 1 time only blinking detect
			{
				ir_reflect_time++;
			}
			else
			{
			    ir_reflect_time = 0;    
			}
			
			if (ir_reflect_time == 2) // 2 blinking only (cant be 1)
			{

			    ir_reflect_flag = 1;
			}
			if (ir_reflect_flag == 0)
			{
			    inVal_IR_LED[1] = SETUPLED_OFF;
			    mlssensorIRLED(0);
			    controlLed(inVal_IR_LED);
			    *status = cur_irled_status = SETUPLED_OFF;
			}
			else
			{
			    if (countTimes >= 45) // 45 of 4 s is 3 min
			    {

				ir_reflect_time = 0;
				countTimes = 0;
				ir_reflect_flag = 0;
			    }

			}
	}
	else  
	{
	    if (cur_irled_status == SETUPLED_OFF) ir_reflect_time = 0;
	}

#else // old implementation


    if (cds_type == 1 || cds_type == 2 || cds_type == 99) // ADC TYPE 1
    {
	if (cds_type == 1)
	{
	    threshold_on = LIGHT_THRESOLD_ON_CDS_TYPE1;
	    threshold_off = LIGHT_THRESOLD_OFF_CDS_TYPE1;
	}
	else if(cds_type == 2)  // cds_type 2
	{
	    threshold_on = LIGHT_THRESOLD_ON_CDS_TYPE2;
	    threshold_off = LIGHT_THRESOLD_OFF_CDS_TYPE2;
	
	}
	else{
	    //get usr data
	    TempOffset_st *pTempOffset;
	    GetTempOffsetConfig(&pTempOffset);
	
	    threshold_on = pTempOffset->cds99_IR_On;
	    threshold_off = pTempOffset->cds99_IR_Off;
	}

	if ((*status == SETUPLED_OFF) && (valueAIN3 > threshold_on))
	{
			inVal_IR_LED[1] = SETUPLED_ON;
			mlssensorIRLED(1);
			controlLed(inVal_IR_LED);
			*status = cur_irled_status = SETUPLED_ON;
	
	}
	
	if ((*status == SETUPLED_ON) && (valueAIN3 < threshold_off))
	{
			inVal_IR_LED[1] = SETUPLED_OFF;
			mlssensorIRLED(0);
			controlLed(inVal_IR_LED);
			*status = cur_irled_status =SETUPLED_OFF;
	
	}

    }
    else  // OLD CDS TYPE
    {
	if (*status == SETUPLED_ON)
	{
		if (flagReadValueOn3 == 0)
		{
			flagReadValueOn3 = 1;
		}
		else if (flagReadValueOn3 == 1)
		{
			valueOn3 = avarage-100;
			flagReadValueOn3 = -1;
		}
	}
	
	if(avarage > LIGHT_THRESOLD_ON_CDS_TYPE0)
	{

		if(*status == SETUPLED_OFF) countTimes++;
		else countTimes  = 0;
		if(countTimes >= 2){
			inVal_IR_LED[1] = SETUPLED_ON;
			mlssensorIRLED(1);
			controlLed(inVal_IR_LED);
			*status = cur_irled_status =SETUPLED_ON;
			countTimes = 0;
			flagReadValueOn3 = 0;
			max = 0; index = 0; // reset average
		}
		else{
			inVal_IR_LED[1] = *status;
			controlLed(inVal_IR_LED);
			//printf("=>Don't change status of iR Led\n");
			//printf("\n");
		}
	}
	else if((avarage < LIGHT_THRESOLD_OFF_CDS_TYPE0)||(avarage < valueOn3))
	{
		//iR Led off
		if(*status == SETUPLED_ON) countTimes++;
		else countTimes  = 0;
		if(countTimes >= 2){
			inVal_IR_LED[1] = SETUPLED_OFF;
			mlssensorIRLED(0);
			controlLed(inVal_IR_LED);
			*status = cur_irled_status =SETUPLED_OFF;
			countTimes = 0;
			max = 0; index = 0; // reset average
		}
		else{
			inVal_IR_LED[1] = *status;
			controlLed(inVal_IR_LED);
			//printf("=>Don't change status of iR Led\n");
			//printf("\n");
		}
	}
	else
	{
		inVal_IR_LED[1] = *status;
		controlLed(inVal_IR_LED);
		countTimes = 0;
		//printf("=>Don't change status of iR Led\n");
		//printf("\n");
	}
    }
#endif // OLD IMPLEMENTATION


	return MLS_SUCCESS;
}
///////////////////////batterylevel thread to get the value of battery level
//trigger for temperature - 20111222 mlsdev008
void *temperature_trigger_thread(void *arg)
{
	int i = 0,j = 0,rNum = 0;
	char bufL[128] = {'\0'},bufH[128] = {'\0'};
	char cmd[128];
	int errcode;

	struct ifreq ifr = trigger_Conf.ifr;
	pthread_cond_t *p_thread_cond = (pthread_cond_t *)arg;
	sprintf(bufL,"TEMP_LOW:%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\r\n",
				(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[0],
				(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[1],
				(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[2],
				(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[3],
				(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[4],
				(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[5]);

	sprintf(bufH,"TEMP_HIGH:%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\r\n",
				(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[0],
				(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[1],
				(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[2],
				(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[3],
				(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[4],
				(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[5]);

	while(!trigger_Conf.exitTrigger.flag){
		pthread_mutex_lock(&trigger_Conf.broadCast.aMutex);
		pthread_cond_wait(p_thread_cond,&trigger_Conf.broadCast.aMutex);
		pthread_mutex_unlock(&trigger_Conf.broadCast.aMutex);
		i = 0;
		if(trigger_Conf.tempL_ack.flag == false){
			sprintf(cmd,"/mlsrb/push_notification %d %d &",3,tempConfig.value);
			errcode = system(cmd);
			if(errcode == -1)
				printf("Error: exe system call\n");
					
			while(trigger_Conf.tempL_ack.flag == false && i*100 < BROADCAST_TIME*1000) //timeout is 10sec
			{
				pthread_mutex_lock(&trigger_Conf.broadCast.aMutex);

				if(sendto(trigger_Conf.broadCast.sockid,bufL,strlen(bufL),0,(struct sockaddr *)&trigger_Conf.broadCast.sever_addr,sizeof(trigger_Conf.broadCast.sever_addr)) < 0){
					perror("sendto error");
				}
				pthread_mutex_unlock(&trigger_Conf.broadCast.aMutex);
				mlsGetRandomNumber(&rNum);
				for(j = 0; j <= rNum; j++,i++){
					if(trigger_Conf.exitTrigger.flag) break;
					usleep(100000);//100ms
				}
			}

			if(trigger_Conf.tempL_ack.flag == true){

				rNum = 30 * 60; 
			}
			else{

				rNum = 5 * 60;  
			}
			for(j = 0; j < rNum; j++){
				if(trigger_Conf.exitTrigger.flag) break;
				sleep(1);//sleep 1 second
			}

			pthread_mutex_lock(&trigger_Conf.tempL_ack.aMutex);
			trigger_Conf.tempL_ack.flag = true;
			pthread_mutex_unlock(&trigger_Conf.tempL_ack.aMutex);
		}else if(trigger_Conf.tempH_ack.flag == false){

			sprintf(cmd,"/mlsrb/push_notification %d %d &",2,tempConfig.value);
			errcode = system(cmd);
			if(errcode == -1)
				printf("Error: exe system call\n");
			while(trigger_Conf.tempH_ack.flag == false && i*100 < BROADCAST_TIME*1000) //timeout is 10sec
			{
				pthread_mutex_lock(&trigger_Conf.broadCast.aMutex);

				if(sendto(trigger_Conf.broadCast.sockid,bufH,strlen(bufH),0,(struct sockaddr *)&trigger_Conf.broadCast.sever_addr,sizeof(trigger_Conf.broadCast.sever_addr)) < 0){
					perror("sendto error");
				}
				pthread_mutex_unlock(&trigger_Conf.broadCast.aMutex);
				mlsGetRandomNumber(&rNum);
				for(j = 0; j <= rNum; j++,i++){
					if(trigger_Conf.exitTrigger.flag) break;
					usleep(100000);//100ms
				}
			}

			if(trigger_Conf.tempH_ack.flag == true){

				rNum = 30*60; 
			}
			else{

				rNum = 5*60;  
			}
			for(j = 0; j < rNum; j++){
				if(trigger_Conf.exitTrigger.flag) break;
				sleep(1);
			}

			pthread_mutex_lock(&trigger_Conf.tempH_ack.aMutex);
			trigger_Conf.tempH_ack.flag = true;
			pthread_mutex_unlock(&trigger_Conf.tempH_ack.aMutex);
		}
	}
	pthread_exit(NULL);
}

void *temperature_thread(void *arg)
{
	int valueAdc = 0,fd = 0,iret,valueAIN3 = 0,countTime = 0;
	pthread_cond_t p_thread_cond;
	pthread_t p_thread;
#define INFRARED_LED_FEATURED

	if(pthread_cond_init(&p_thread_cond,NULL) != 0){

	}
	if(pthread_create(&p_thread,NULL,temperature_trigger_thread,&p_thread_cond) != 0){

	}

    tempConfig.value = 0;

    while(!tempConfig.stop)
    {
#ifdef INFRARED_LED_FEATURED
    	if(countTime%4 == 0){

			pthread_mutex_lock(&audioConfig.mutex_recording);

#if 0
			if (audioConfig.status == AUDIO_STREAM_MIC)
			{
				fd = audioConfig.fd_dsp;
			}
			else if ((audioConfig.status == AUDIO_STREAM_SPEAKER)||(audioConfig.status == AUDIO_MELODY_PLAY))
			{
				fd = audioConfig.spk_fd_dsp;
			}
			else
			{
				pthread_mutex_unlock(&audioConfig.mutex_recording);
				sleep(1);
				continue;
			}
#endif
			fd = audioConfig.fd_dsp;
			
			if (cur_irled_status==SETUPLED_ON)
			{
			    extern mlsIcam_st gmlsIcam;
			    inVal_IR_LED[1] = SETUPLED_OFF;
			    controlLed(inVal_IR_LED);

			    usleep(gmlsIcam.debug_val1);


			}

			ioctl(fd,SNDCTL_DSP_GETAIN3,&valueAIN3);
			if (cur_irled_status==SETUPLED_ON)
			{

			    inVal_IR_LED[1] = SETUPLED_ON;
			    controlLed(inVal_IR_LED);

			}
			
			pthread_mutex_unlock(&audioConfig.mutex_recording);
			ADC_reading_flag = 1;
			_mlsAdcCtrlLed(valueAIN3,&ir_status);
    	}
#endif

    	if(countTime%60 == 5){
			pthread_mutex_lock(&audioConfig.mutex_recording);
			fd = audioConfig.fd_dsp;

			iret = ioctl(fd,SNDCTL_DSP_GETAIN2,&valueAdc);

			pthread_mutex_unlock(&audioConfig.mutex_recording);

			_mlsadcCalcTempC(valueAdc,&tempConfig.value);
			if(settingConfig == NULL)
			{

				continue;
			}
			if(tempConfig.value > settingConfig->extra.temp_alert.max){		

				pthread_mutex_lock(&trigger_Conf.tempH_ack.aMutex);
				trigger_Conf.tempH_ack.flag = false;
				pthread_mutex_unlock(&trigger_Conf.tempH_ack.aMutex);
				pthread_cond_broadcast(&p_thread_cond);
			}else if(tempConfig.value < settingConfig->extra.temp_alert.min){	

				pthread_mutex_lock(&trigger_Conf.tempL_ack.aMutex);
				trigger_Conf.tempL_ack.flag = false;
				pthread_mutex_unlock(&trigger_Conf.tempL_ack.aMutex);
				pthread_cond_broadcast(&p_thread_cond);
			}
		}


		if(!tempConfig.stop)
		{
			sleep(1);
			countTime++;

		}
		else
		{
			break;
		}
    }
	return NULL;
}

mlsErrorCode_t temperature_thread_run()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
#if 1


	//start thread for motor A
	if( pthread_create(&tempConfig.working_thread, 0, temperature_thread, NULL) != 0)
	{

		retVal = MLS_ERROR;
		return retVal;
	};

	pthread_detach(tempConfig.working_thread);
#else
	tempConfig.value = 4;
#endif
	return retVal;
}


mlsErrorCode_t mlsaudioGetMelodyIndex(int *index)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	*index = audioConfig.melody.index;


	return retVal;
}

mlsErrorCode_t mlsadcGetTemperature(int *value)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	*value = tempConfig.value;
	return retVal;
}

mlsErrorCode_t mlsGetDirection(audio_vox_ctrl * reVal)
{
	switch(audioConfig.direction)
	{
	case AUDIO_INIT:
		*reVal = VOX_INIT;
		break;
	case AUDIO_STREAM_SPEAKER:
		*reVal = VOX_STREAM_SPEAKER;
		break;
	case AUDIO_STREAM_MIC:
		*reVal = VOX_STREAM_MIC;
		break;
	case AUDIO_FREE:
		*reVal = VOX_FREE;
		break;
	default:

		*reVal = VOX_UNKNOWN;
		return MLS_ERROR;
	}
	return MLS_SUCCESS;
}

mlsErrorCode_t mlsSetDirection(audio_vox_ctrl reVal)
{
	switch(reVal)
	{
	case VOX_INIT:
		audioConfig.direction = AUDIO_INIT;
		break;
	case VOX_STREAM_SPEAKER:
		audioConfig.direction = AUDIO_STREAM_SPEAKER;
		break;
	case VOX_STREAM_MIC:
		audioConfig.direction = AUDIO_STREAM_MIC;
		break;
	case VOX_FREE:
		audioConfig.direction = AUDIO_FREE;
		break;
	default:

		return MLS_ERROR;
	}
	return MLS_SUCCESS;
}

mlsErrorCode_t mlsGetStatusAudio(audio_vox_ctrl * reVal)
{
	switch(audioConfig.status)
	{
	case AUDIO_INIT:
		*reVal = VOX_INIT;
		break;
	case AUDIO_STREAM_SPEAKER:
		*reVal = VOX_STREAM_SPEAKER;
		break;
	case AUDIO_STREAM_MIC:
		*reVal = VOX_STREAM_MIC;
		break;
	case AUDIO_FREE:
		*reVal = VOX_FREE;
		break;
	default:

		*reVal = VOX_UNKNOWN;
		return MLS_ERROR;
	}
	return MLS_SUCCESS;
}
mlsErrorCode_t mlsGetDataFromMic(short *sample)
{

	if(pthread_mutex_lock(&dataMicMutex))
		perror("pthread mutex lock dataMicMutex");

	if(pthread_cond_wait(&dataAvailable,&dataMicMutex) != 0)
		perror("pthread condition wait error");
	*sample = total_vox /vox_count;
	total_vox = 0;
	vox_count = 0;

	if(pthread_mutex_unlock(&dataMicMutex))
	{
		perror("pthread mutex unlock dataMicMutex");
	}
	return MLS_SUCCESS;
}
