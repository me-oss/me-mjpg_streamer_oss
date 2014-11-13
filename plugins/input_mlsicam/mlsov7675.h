/*
 * mlsov7675.h
 *
 *  Created on: Aug 13, 2010
 *      Author: mlslnx003
 */

#ifndef MLSOV7675_H_
#define MLSOV7675_H_

#include "mlsInclude.h"
#include "OV7675.h"

/* CX debug message */
#define MLSOV_DEBUG

#define MLSOV_DEBUG_HEADER "OV7675:"

#if defined (MLSOV_DEBUG)
#define MLSOV_DEBUG_PRINTF printf
#else
#define MLSOV_DEBUG_PRINTF
#endif

/* OV7675 address */
#define OV7675_ADDRESS  0x42

/* define value for brightness */
#define OV_BRIGHTNESS_DEFAULT	0x00
#define OV_BRIGHTNESS_MAX		0x7F
#define OV_BRIGHTNESS_MIN		0xFF

/* define value for contrast */
#define OV_CONTRAST_DEFAULT		0x40
#define OV_CONTRAST_MAX			0xFF
#define OV_CONTRAST_MIN			0x00

/* copy from struct input_cmd in MJPEG Streamer input.h */
/* commands which can be send to the input plugin */
typedef enum  ovSensorControl_enum
{
  OV_IN_CMD_UNKNOWN = 0,
  OV_IN_CMD_HELLO,
  OV_IN_CMD_RESET,
  OV_IN_CMD_RESET_PAN_TILT,
  OV_IN_CMD_RESET_PAN_TILT_NO_MUTEX,
  OV_IN_CMD_PAN_SET,
  OV_IN_CMD_PAN_PLUS,
  OV_IN_CMD_PAN_MINUS,
  OV_IN_CMD_TILT_SET,
  OV_IN_CMD_TILT_PLUS,
  OV_IN_CMD_TILT_MINUS,
  OV_IN_CMD_SATURATION_PLUS,
  OV_IN_CMD_SATURATION_MINUS,
  OV_IN_CMD_CONTRAST_PLUS,
  OV_IN_CMD_CONTRAST_MINUS,
  OV_IN_CMD_BRIGHTNESS_PLUS,
  OV_IN_CMD_BRIGHTNESS_MINUS,
  OV_IN_CMD_GAOV_IN_PLUS,
  OV_IN_CMD_GAOV_IN_MINUS,
  OV_IN_CMD_FOCUS_PLUS,
  OV_IN_CMD_FOCUS_MINUS,
  OV_IN_CMD_FOCUS_SET,
  OV_IN_CMD_LED_ON,
  OV_IN_CMD_LED_OFF,
  OV_IN_CMD_LED_AUTO,
  OV_IN_CMD_LED_BLINK
}ovSensorControl_t;

/*! struct control OV7675 */
typedef struct mlsOVControl_st
{
	UInt8 brightnessValue;/*!< current brightness value in sensor */
	UInt8 contrastValue;/*!< current contrast value in sensor */
}mlsOVcontrol_st;

//!Initialize senor OV7675
/*! Set GPIO4 to LOW
 \return
	 - MLS_SUCCESS:successful execution
	 - MLS_ERR_OV_OPEN_GPIODEV: file device trek_gpio open error
	 - MLS_ERR: other errors
 */
mlsErrorCode_t mlsOV7675_Init();

//!DeInitialize senor OV7675
/*!
 \return
	 - MLS_SUCCESS:successful execution
 */
mlsErrorCode_t mlsOV7675_DeInit();

//!Set contrast of senor OV7675
/*!
 \param value is value to set to contrast register(0->255)
 \return
	 - MLS_SUCCESS:successful execution
	 - MLS_ERR_CX_OPEN_SPIDEV: open SPI device error
	 - MLS_ERR_CX_SPI_FAIL: SPI transmits error
 */
mlsErrorCode_t mlsOV7675_SetContrast(UInt8 value);

//!Get contrast value of senor OV7675
/*!
 \param *value is the pointer to get value from contrast register in sensor(0->255)
 \return
	 - MLS_SUCCESS:successful execution
	 - MLS_ERR_CX_OPEN_SPIDEV: open SPI device error
	 - MLS_ERR_CX_SPI_FAIL: SPI transmits error
 */
mlsErrorCode_t mlsOV7675_GetContrast(UInt8 *value);

//!Set brightness of sensor OV7675
/*!
 \param value is value to set to brightness register(0->255)
 \return
	 - MLS_SUCCESS:successful execution
	 - MLS_ERR_CX_OPEN_SPIDEV: open SPI device error
	 - MLS_ERR_CX_SPI_FAIL: SPI transmits error
 */
mlsErrorCode_t mlsOV7675_SetBrightness(UInt8 value);

//!Get brightness value of sensor OV7675
/*!
 \param *value is the pointer to get value of brightness register in sensor(0->255)
 \return
	 - MLS_SUCCESS:successful execution
	 - MLS_ERR_CX_OPEN_SPIDEV: open SPI device error
	 - MLS_ERR_CX_SPI_FAIL: SPI transmits error
 */
mlsErrorCode_t mlsOV7675_GetBrightness(UInt8 *value);

//!set resolution VGA for sensor OV7675
/*!
 \return
	 - MLS_SUCCESS:successful execution
	 - MLS_ERR_CX_OPEN_SPIDEV: open SPI device error
	 - MLS_ERR_CX_SPI_FAIL: SPI transmits error
 */
mlsErrorCode_t mlsOV7675_SetResolutionVGA();

//!set resolution QVGA for sensor OV7675
/*!
 \return
	 - MLS_SUCCESS:successful execution
	 - MLS_ERR_CX_OPEN_SPIDEV: open SPI device error
	 - MLS_ERR_CX_SPI_FAIL: SPI transmits error
 */
mlsErrorCode_t mlsOV7675_SetResolutionQVGA();

//!set resolution QQVGA for sensor OV7675
/*!
 \return
	 - MLS_SUCCESS:successful execution
	 - MLS_ERR_CX_OPEN_SPIDEV: open SPI device error
	 - MLS_ERR_CX_SPI_FAIL: SPI transmits error
 */
mlsErrorCode_t mlsOV7675_SetResolutionQQVGA();

#endif /* MLSOV7675_H_ */
