/*
 * mlsicam.h
 *
 *  Created on: Aug 13, 2010
 *      Author: mlslnx003
 */

#ifndef MLSIcam_H_
#define MLSIcam_H_

#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#include "../input.h"
#include "mlsInclude.h"
//#include "mlscx93510.h"
//#include "mlsov7675.h"
#include "mlsWifi.h"

//mls@dev03 20110713 include rapbit
#include "mlsimage.h"
//#include "mlsmotion.h"
#include "step_motor.h"
#include "mlsaudio.h"
#include "mlsio.h"
#include "mlsflash.h"
#include "mlsVox.h"
#include "mlstrigger.h"
#include "mlsOTA.h"

#define Icam_VERSION mlsOTA_GetVersionStr()

#define MLSIcam_DEBUG
#define DEBUG_STATIC_PORT

#define MLSIcam_DEBUG_HEADER "icam:"

#if defined (MLSIcam_DEBUG)
#define MLSIcam_DEBUG_PRINTF printf
#else
#define MLSIcam_DEBUG_HEADER
#define MLSIcam_DEBUG_PRINTF
#endif

#define Icam_JPEGBUFFER_SIZE   100*1024//CX_JPEGBUFFER_SIZE
#define Icam_AUDIOBUFFER_SIZE	16*1024//CX_AUDIOBUFFER_SIZE

#define MLSIcam_INDEXJPEG_MAX 9999

/*define indicator led blink time (unit ms)*/
#define INDICATORLED_CAMERR_ON	 1000
#define INDICATORLED_CAMERR_OFF  1000

#define INDICATORLED_WIFIERR_ON  2000
#define INDICATORLED_WIFIERR_OFF 1000

#define INDICATORLED_WIFICAMERR_ON	 2000
#define INDICATORLED_WIFICAMERR_OFF  2000

#define INDICATORLED_ADNORMCON_ON	 180
#define INDICATORLED_ADNORMCON_OFF  1000

#define INDICATORLED_ADRESETCON_ON  500
#define INDICATORLED_ADRESETCON_OFF 500

typedef struct mlsIcam_st
{
	pthread_mutex_t mutex;/*! mutex for icam layer */
//	pthread_mutex_t mutexMotion;/*! mutex for motion control */

	Int32 fd_gpio;/*! file decriptor for gpio */

	UInt32 frameSizeIn;/*! max frame size JPEG in CX93510 */

	UInt8 frameBuffer[Icam_JPEGBUFFER_SIZE];/*! buffer to contain JPEG data */
	UInt32 frameSize;/*! size of JPEG data in buffer */
	UInt32 frameChecksum;
	UInt8 sensorBrightIndex;/*! current brightness index (0->8) */
	UInt8 sensorContrastIndex;/*! current contrast index (0->8) */
	UInt8 vgaMode;/*! vga mode: 0-VGA 1-QVGA 2-QQVGA*/

	UInt8 audioBuffer[Icam_AUDIOBUFFER_SIZE];/*! buffer to contain audio data */
	UInt32 audioSize;/*! audio data size in audio buffer */

	UInt32 indexCurrentJPEG;/*! current index of JPEG frame */
	UInt32 indexBeginAudio;/*! mark the index of JPEG frame to start audio data */
	Bool flagGetAudio;/*! flag is asserted when icam audio buffer is not empty */
	int networkMode;
	unsigned char IsConnectToInternet;
	unsigned int temp_offset;
	unsigned short cds_type;
	unsigned short video_test_finish;
	unsigned char flagReset;/*! flag is asserted when audio buffer in CX93510 is full or time out over 1second */
	int debug_val1;
}mlsIcam_st;

mlsIcam_st gmlsIcam;

//!Initialize icam component
/*!
 * \param icamMode is pointer to get icam operation mode: normal or reset
 * \return error code
 */
mlsErrorCode_t mlsIcam_Init(icamMode_t *icamMode,networkMode_t* networkMode);

//!DeInitialize icam component
/*!
 * \return error code
 */
mlsErrorCode_t mlsIcam_DeInit();

//! Get max size of JPEG data in encoder CX93510
/*!
 * \param none
 * \return max size of JPEG data in encoder CX93510
 */
UInt32 mlsIcam_GetInputFrameSize();

//!Start capture JPEG image in encoder CX93510
/*! Start capture JPEG image in encoder CX93510
 * \return error code
 */
mlsErrorCode_t mlsIcam_StartCapture();

//!Get last frame JPEG image and audio data from encoder CX93510
/*!
 * \return error code
 */
mlsErrorCode_t mlsIcam_GrabFrame();

//!Get JPEG data from icam buffer
/*!
 * \param *frameBuffer is the pointer to buffer which will contain JPEG data
 * \param *frameSize is the pointer to variable which will get size of JPEG data
 * \return error code
 */
mlsErrorCode_t mlsIcam_GetFrameData(UInt8 *frameBuffer,UInt32 *frameSize);

//!control sensor or get value
/*!
 * \param controlID is the ID of control of sensor which define in enum ovSensorControl_t
 * \param *newVal is the pointer to varibale to set value to sensor or get value from sensor
 * \return error code
 * \sa enum ovSensorControl_t
 */
//mlsErrorCode_t mlsIcam_SetSensorControl(ovSensorControl_t controlID, UInt8 *newValue);

//!Set compression mode in encoder CX93510
/*!
 * \param compressionMode is the compression mode needing to set
 * \return
 * \sa enum icam_compression_t
 */
//mlsErrorCode_t mlsIcam_SetCompression(icam_compression_t compressionMode);

//!Set resolution mode in sensor and encoder
/*!
 * \param resolutionMode is the resolution mode needing to set
 * \return error code
 * \sa enum icam_resolution_t
 */
mlsErrorCode_t mlsIcam_SetResolution(icam_resolution_t resolutionMode);

//!Switch boot partition to upgrade partition
/*!
 * \param strData is string data which format contains wifi setting data
 * \return error code
 */
mlsErrorCode_t mlsIcam_Bootswitch();

//!Save wifi setting data from input string to flash
/*!
 * \param strData is string data which format contains wifi setting data
 * \return error code
 */
mlsErrorCode_t mlsIcam_SaveWifiConfig(String strData);

//!Read wifi setting data from flash
/*!
 * \param strValue is the string which will contains the wifi setting data read from flash
 * \return
 */
mlsErrorCode_t mlsIcam_ReadWifiConfig(String strValue);

//!Start encoding audio in CX93510
/*!
 * \param none
 * \return error code
 */
mlsErrorCode_t mlsIcam_StartAudio();

//!Get audio data from icam buffer
/*!
 * \param *audioBuffer is the pointer to buffer will contains audio data
 * \param *audioSize is the pointer to variable to get audio size
 * \return error code
 */
mlsErrorCode_t mlsIcam_GetAudioData(UInt8 *audioBuffer, UInt32 *audioSize);

//!Get audio size in icam buffer
/*!
 * \param *audioSize is the pointer to variable to get audio size
 * \return error code
 */
mlsErrorCode_t mlsIcam_GetAudioSize(UInt32 *audioSize);

//!Save wifi setting data from input string to flash
/*!
 * \param strData is string data which format contains wifi setting data
 * \return error code
 */
mlsErrorCode_t mlsIcam_SaveWifiConfig(String strDataIn);
//!Read wifi setting data from flash
/*!
 * \param strValue is the string which will contains the wifi setting data read from flash
 * \return error code
 */
mlsErrorCode_t mlsIcam_ReadWifiConfig(String strData);
//!Read port setting data from flash
/*!
 * \param strValue is the string which will contains the wifi setting data read from flash
 * \return error code
 */
mlsErrorCode_t mlsIcam_ReadPortConfig(String strData);
//!Read username setting data from flash
/*!
 * \param strValue is the string which will contains the wifi setting data read from flash
 * \return error code
 */
mlsErrorCode_t mlsIcam_ReadUserNameConfig(String strData);
//!Read passWord setting data from flash
/*!
 * \param strValue is the string which will contains the wifi setting data read from flash
 * \return error code
 */
mlsErrorCode_t mlsIcam_ReadPassWordConfig(String strData);
//!Read network mode setting data from flash
/*!
 * \param strValue is the string which will contains the wifi setting data read from flash
 * \return error code
 */
mlsErrorCode_t mlsIcam_ReadNetWorkModeConfig(String strData);
/*!
 *
 */
mlsErrorCode_t mlsIcam_SaveFolderRecName(String strFolderName);

/*!
 *
 */
mlsErrorCode_t mlsIcam_ReadFolderRecName(String strFolderName);

/*!Control indicator LED
 *
 */
mlsErrorCode_t mlsicam_indicatorled(UInt32 timeOn, UInt32 timeOff);

/*!Motion control
 *
 * @param controlID direction ID
 * @param strduty is string duty cycle ("0.0"->"1.0")
 * @return
 */
mlsErrorCode_t mlsIcam_SetMotion(in_cmd_type controlID,Int8* srtduty);

mlsErrorCode_t mlsRapit_SetAudio(in_cmd_type controlID);


mlsErrorCode_t mlsRapit_WifiSignal(int *value_wifi);

mlsErrorCode_t mlsRapit_BatteryLevel(int *value_battery);

mlsErrorCode_t mlsRapit_GetByteWBvalue(char * value);

mlsErrorCode_t mlsRapit_WdtRestart(int seconds);
mlsErrorCode_t mlsRapit_WdtGetTime(int *time);
mlsErrorCode_t mlsRapit_WdtKeepAlive();
mlsErrorCode_t mlsRapit_WdtStop();

mlsErrorCode_t mlsRapbit_GetMelodyIndex(int *index);

mlsErrorCode_t  mlsRapit_GetTemperature(int *value);

mlsErrorCode_t mlsRapit_ResetFactory();

mlsErrorCode_t mlsIcam_ReadConfigUAP(String strData);

mlsErrorCode_t mlsIcam_SaveConfigUAP(String strDataIn);

mlsErrorCode_t mlsRapit_ResetFactory();

//define for Vox, mlsdev008 10/11/2011
mlsErrorCode_t mlsicamVoxEnable(char *addr);
mlsErrorCode_t mlsicamVoxDisable(char *strValue);
mlsErrorCode_t mlsicamVoxGetThreshold(int *reg);
mlsErrorCode_t mlsicamVoxSetThreshold(char *threshold);
mlsErrorCode_t mlsicamVoxGetStatus(int *reg);
//define flip up 8/12/2011
mlsErrorCode_t mlsIcam_Flipup();
//define save camera name 17/12/2011
mlsErrorCode_t mlsicamSaveCameraName(char *name);

mlsErrorCode_t mlsIcam_GetValueBrightness(UInt8 *value);
mlsErrorCode_t mlsIcam_GetValueConstract(UInt8 *value);
mlsErrorCode_t mlsIcam_GetValueVGAMode(UInt8 *value);
mlsErrorCode_t mlsIcam_SaveUsernamePasswdConfig(String strData);
unsigned int mlsIcam_GetValueTempOffset();
unsigned short mlsIcam_GetCDSType();

int mlsIcam_IsinUAPMode();
int mlsIcam_IsVideoTestFinish();

mlsErrorCode_t mlsRapit_Setledear(in_cmd_type controlID);
mlsErrorCode_t mlsRapit_RestartApp();
mlsErrorCode_t mlsRapit_SwitchtoUAP();
mlsErrorCode_t mlsIcam_SetSpkVolume(String strValue);
mlsErrorCode_t mlsIcam_GetSpkVolume(int *valueVol);
mlsErrorCode_t mlsIcam_SetSensorControl(in_cmd_type controlID,
										 UInt8 *newValue);
char is_flipup(void);

#endif /* MLSIcam_H_ */
