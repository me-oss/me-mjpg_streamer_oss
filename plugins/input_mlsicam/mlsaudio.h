/*
 * mlsaudio.h
 *
 *  Created on: Aug 2, 2011
 *      Author: root
 */

#ifndef MLSAUDIO_H_
#define MLSAUDIO_H_

#include "mlsInclude.h"
#include "../input.h"
#include "pthread.h"






#define DEFAULT_TEMPERATURE_OFFSET 2 
pthread_cond_t dataAvailable;
pthread_mutex_t dataMicMutex;
typedef enum{
	VOX_INIT,
	VOX_STREAM_SPEAKER,
	VOX_STREAM_MIC,
	VOX_FREE,
	VOX_UNKNOWN
}audio_vox_ctrl;
//define vox, by mlsdev008 11/11/2011
mlsErrorCode_t mlsGetDirection(audio_vox_ctrl * reVal);
mlsErrorCode_t mlsSetDirection(audio_vox_ctrl reVal);
mlsErrorCode_t mlsGetStatusAudio(audio_vox_ctrl * reVal);
mlsErrorCode_t mlsGetDataFromMic(short *sample);
/*******************************************************/
mlsErrorCode_t mlsaudioInit();
mlsErrorCode_t mlsaudioDeInit();

mlsErrorCode_t mlsaudioGetData(char* buffer, unsigned int *size);
mlsErrorCode_t mlsaudioStart();

mlsErrorCode_t mlsaudioSetDirection(in_cmd_type controlID);

//mlsErrorCode_t mlsioBatteryLevel(int *value);

mlsErrorCode_t mlsaudioGetMelodyIndex(int *index);

mlsErrorCode_t mlsadcGetTemperature(int *value);

mlsErrorCode_t mlsaudioSpkVolume(int volume);
mlsErrorCode_t mlsaudioGetSpkVolume(int *volume);


typedef struct
{
    unsigned int finetune1;
    unsigned int finetune2;
    char dummy[8];
    unsigned int crc;
}AudioFineTune_st;

mlsErrorCode_t mlsaudio_SaveAudioFinetune(unsigned int finetune1, unsigned int finetune2);
mlsErrorCode_t mlsaudio_ReadAudioFinetune(unsigned int* pfinetune1, unsigned int* pfinetune2);

//mlsErrorCode_t mlsaudioReadTemperatureOffset(int* offset);
mlsErrorCode_t mlsaudioOpenPCMLog();
mlsErrorCode_t mlsaudioClosePCMLog();
void mlsaudioSetFinetune(unsigned int finetune1, unsigned int finetune2);
void mlsaudioGetFinetune(unsigned int* finetune1, unsigned int* finetune2);

#endif /* MLSAUDIO_H_ */
