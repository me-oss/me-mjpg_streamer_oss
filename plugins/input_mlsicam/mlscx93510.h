/*
 * mlscx93510.h
 *
 *  Created on: Aug 13, 2010
 *      Author: mlslnx003
 */

#ifndef MLSCX93510_H_
#define MLSCX93510_H_

#include <stdio.h>
#include <linux/types.h>
//#include <linux/spi/spidev.h>
//#include "/home/ubuntu-sanght/project/flucard/trek/kernel/linux-2.6.25/include/linux/spi/spidev.h"
//#include "../../../../linux-2.6.25/include/linux/spi/spidev.h"
//#include <linux/spi/spidev.h>
#include "spidev.h"

#include "mlsInclude.h"
#include "CX93510.h"
#include "mlsov7675.h"

//typedef enum icam_compression_enum
//{
//	icam_compression_standard = 0,
//	icam_compression_high = 1
//}icam_compression_t;
//
//typedef enum icam_resolution_enum
//{
//	icam_res_vga = 0,
//	icam_res_qvga = 1,
//	icam_res_qqvga = 2
//}icam_resolution_t;

/* CX debug message */
//#define MLSCX_DEBUG

#define MLSCX_DEBUG_HEADER "CX93510:"

#if defined (MLSCX_DEBUG)
#define MLSCX_DEBUG_PRINTF printf
#else
#define MLSCX_DEBUG_PRINTF
#endif

#define CX_JPEGBUFFER_SIZE 65536
#define CX_AUDIOBUFFER_SIZE 4096 //1s audio

#pragma pack(1)
typedef struct spiTransfer_st
{
	Int32 fd;//file descriptor
	Int8 *device;//pointer to name of spi device
	UInt8 mode;//spi mode

	struct spi_ioc_transfer spiTransferConfig;
}spiTransfer_st;
#pragma pack()

#pragma pack(1)
typedef struct cxResolution_st
{
	UInt32 height;
	UInt32 width;
}cxResolution_st;
#pragma pack()

typedef enum cxQuantizationTable_enum
{
	cx_qt_standard =0,
	cx_qt_highCompress = 1
}cxQuantizationTable_t;

typedef enum cxImageMode_enum
{
	cx_im_JPEG = 0<<0,
	cx_im_MJPEG = 1<<0
}cxImageMode_t;

typedef enum cxFrameCapMode_t
{
	cx_fc_limited = 1<<7,
	cx_fc_continuous = 1<<6
}cxFrameCapMode_t;

typedef enum cxFBMode_enum
{
	cx_fb_limited = 0<<5,
	cx_fb_continuous = 1<<5
}cxFBMode_enum;

typedef enum cxFrameType_enum
{
	cx_ft_end = 0x00,
	cx_ft_direct = 0x01,
	cx_ft_difference = 0x02,
	cx_ft_configuration = 0x04,
	cx_ft_reference = 0x08
}cxFrameType_enum;

typedef struct mlsCX93510Config_st
{
	spiTransfer_st spiTransfer;/*!< configuration parameters for spi device */

	cxResolution_st resolution;/*!< resolution mode */
	cxQuantizationTable_t quantizationTable;/*!< quntization table */

	cxImageMode_t imageMode;/*!< image mode to encode: JPEG or MJPEG */
	cxFrameCapMode_t frameCapMode;/*!< capture frame mode: limit or continuous */
	cxFBMode_enum frameBufferMode;/*!< buffer mode: limit or continuous */

	UInt8 frameLimitNumber;/*!< number of frame to capture in limit capturing mode */
	UInt8 frameSkip;/*!< number of frame to skip between sensor and CX93510 */

	UInt32 LastFBAddress;/*!< last frame buffer address in CX93510 buffer */
	UInt32 LastFrameSize;/*!< last frame buffer size */

	UInt8 configBuffer[688];/*!< buffer to contain configuration data for JPEG image in CX93510 */
	UInt32 configBufferLength;/*!< length bytes of configuration data */

	UInt16 frameBufferSize;/*!< JPEG frame buffer size in CX93510 */

	UInt8 errStatus;/*!< error status */
}mlsCX93510Config_st;

//!Initialize encoder CX93510
/*!
 * - Init SPI driver
 * - Init interface CX93510
 * - Check and setup camera ov7675
 * - Setup CX93510
 * \param none
 * \return error code
 * 	- MLS_SUCCESS : successful execution
 *  - MLS_ERR_CX_OPEN_SPIDEV: open file device SPI error
 *  - MLS_ERR_CX_SPI_FAIL: SPI transmit error
 *  - MLS_ERR_CX_GETCONFIGDATA: read configuration JPEG data in buffer CX93510 error
 *  - MLS_ERR_OV_PID: wrong PID of camera OV7576
 *  - MLS_ERR_OV_VER: wrong version of camera OV7576
 */
mlsErrorCode_t mlsCX93510_Init();

//!DeInitialize encoder CX93510
/*!
 *
 * \param none
 * \return error code
 *  - MLS_SUCCESS : successful execution
 *  - MLS_ERR_CX_SPI_FAIL: SPI transmit error
 */
mlsErrorCode_t mlsCX93510_DeInit();

//!Get last frame JPEG image from encoder CX93510 to buffer
/*!
 * \param *abBuff is the pointer to buffer which will contain JPEG data
 * \param *size is the pointer to variable which will get size of JPEG data
 * \param *nbFrameMiss is the pointer to variable which will get number of missed frame JPEG
 * in encoder CX93510 from the previous last frame to the current last frame
 * \return error code
 * 	- MLS_SUCCESS : successful execution
 *  - MLS_ERR_CX_SPI_FAIL: SPI transmit error
 *  - MLS_ERR_CX_TIMEOUT_1S: ADPCM audio buffer full means timeout 1 second
 */
mlsErrorCode_t mlsCX93510_GetLastFrame(UInt8 * abBuff, UInt32 *size, UInt32 *nbFrameMiss);

//!Read value from the register address in sensor OV7675 via CX93510
/*!
 * \param ovAddress is the address of register in sensor OV7675
 * \param *value is the pointer to the variable which will get value from the register in sensor OV7675
 * \return error code
 *  - MLS_SUCCESS : successful execution
 *  - MLS_ERR_CX_SPI_FAIL: SPI transmit error
 */
mlsErrorCode_t mlsCX93510_SensorOVRead(UInt8 ovAddress, UInt8 *value);

//!Write value to the register address in sensor OV7675 via CX93510
/*!
 * \param ovAddress is the address of register in sensor OV7675
 * \param value is value which use to set value to the register in sensor OV7675
 * \return error code
 *  - MLS_SUCCESS : successful execution
 *  - MLS_ERR_CX_SPI_FAIL: SPI transmit error
 */
mlsErrorCode_t mlsCX93510_SensorOVWrite(UInt8 ovAddress, UInt8 value);

//!Start or restart CX93510 to capture image from sensor and encode to JPEG data
/*!
 * \param none
 * \return error code
 *  - MLS_SUCCESS : successful execution
 *  - MLS_ERR_CX_SPI_FAIL: SPI transmit error
 */
mlsErrorCode_t mlsCX93510_RestartCapture();

//!Set compression mode in CX93510
/*!
 * \param compressionMOde is the compression mode which needs to change
 * \return error code
 * \sa enum icam_compression_t
 */
mlsErrorCode_t mlsCX93510_SetCompression(icam_compression_t compressionMode);

//!Set resolution mode in CX93510
/*!
 * \param compressionMode is the resolution mode which needs to change
 * \return error code
 *  - MLS_SUCCESS : successful execution
 *  - MLS_ERR_CX_SPI_FAIL: SPI transmit error
 * \sa enum icam_resolution_t
 */
mlsErrorCode_t mlsCX93510_SetResolution(icam_resolution_t resolutionMode);

//!Start capture audio in CX93510
/*!
 * \param none
 * \return error code
 *  - MLS_SUCCESS : successful execution
 *  - MLS_ERR_CX_SPI_FAIL: SPI transmit error
 */
mlsErrorCode_t mlsCX93510_StartAudio();

//!Get audio data in buffer of CX93510
/*!
 * \param *abBuff is the pointer to buffer that will contain audio data
 * \param *size is the pointer to variable that will get size of audio data
 * \return error code
 *  - MLS_SUCCESS : successful execution
 *  - MLS_ERR_CX_SPI_FAIL: SPI transmit error
 */
mlsErrorCode_t mlsCX93510_GetAudioData(UInt8* abBuff, UInt32 *size);

#endif /* MLSCX93510_H_ */
