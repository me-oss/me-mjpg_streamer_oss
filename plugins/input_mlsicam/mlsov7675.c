/*
 * mlsov7675.c
 *
 *  Created on: Aug 13, 2010
 *      Author: mlslnx003
 */

#include <fcntl.h>
#include "mlsov7675.h"
#include "mlscx93510.h"
#include <sys/ioctl.h>
#include <unistd.h>
#include "../../debug.h"
#define TREK_GPIO_SET 1
#define TREK_GPIO_SET_DIRECTION 2

mlsOVcontrol_st gmlsOVControl;

/*
 * _mlsicam_gpio_powerdnOV:Set GPIO4 to low to deassert OV powerdown pin
 */
//mlsErrorCode_t _mlsicam_gpio_powerdnOV()
//{
//	mlsErrorCode_t retVal = MLS_SUCCESS;
//	Char gpioConfig[2] = {0,1<<4|1<<2};
//	Int32 fd_gpio,iret;
//
//	fd_gpio = open("/dev/trek_gpio", O_RDWR);
//
//	if (fd_gpio <= 0)
//	{
//		printf("Error open /dev/trek_gpio\n");
//		retVal = MLS_ERR_OV_OPEN_GPIODEV;
//		return retVal;
//	}
//
//	//Set GPIO4 direction to ouput
//	iret = ioctl(fd_gpio, TREK_GPIO_SET_DIRECTION, gpioConfig);
//
//	gpioConfig[1] = 0;//0<<4; //set low
//
//	//Set GPIO4 to LOW
//	iret = ioctl(fd_gpio, TREK_GPIO_SET, gpioConfig);
//
//	if (iret < 0)
//	{
//		retVal = MLS_ERROR;
//	}
//
//	printf("Set output GPIO4 to LOW successfully\n");
//
//	close(fd_gpio);
//
//	return retVal;
//}

//!Initialize senor OV7675
/*! Set GPIO4 to LOW
 \return
	 - MLS_SUCCESS:successful execution
	 - MLS_ERR_OV_OPEN_GPIODEV: file device trek_gpio open error
	 - MLS_ERR: other errors
 */
mlsErrorCode_t mlsOV7675_Init()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	gmlsOVControl.brightnessValue = 0x00;
	gmlsOVControl.contrastValue = 0x40;

	/* mls@dev03 101012 set gpio4 to permanently LOW */
//	retVal = _mlsicam_gpio_powerdnOV();

	return retVal;
}

//!DeInitialize senor OV7675
/*!
 \return
	 - MLS_SUCCESS:successful execution
 */
mlsErrorCode_t mlsOV7675_DeInit()
{
	return MLS_SUCCESS;
}

//!Set contrast of senor OV7675
/*!
 \param value is value to set to contrast register(0->255)
 \return
	 - MLS_SUCCESS:successful execution
	 - MLS_ERR_CX_OPEN_SPIDEV: open SPI device error
	 - MLS_ERR_CX_SPI_FAIL: SPI transmits error
 */
mlsErrorCode_t mlsOV7675_SetContrast(UInt8 value)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	retVal = mlsCX93510_SensorOVWrite(OV_CONTRAST_REG,value);
	if(retVal == MLS_SUCCESS)
	{
		gmlsOVControl.contrastValue = value;
	}

	return retVal;
}

//!Get contrast value of senor OV7675
/*!
 \param *value is the pointer to get value from contrast register in sensor(0->255)
 \return
	 - MLS_SUCCESS:successful execution
	 - MLS_ERR_CX_OPEN_SPIDEV: open SPI device error
	 - MLS_ERR_CX_SPI_FAIL: SPI transmits error
 */
mlsErrorCode_t mlsOV7675_GetContrast(UInt8 *value)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	retVal = mlsCX93510_SensorOVRead(OV_CONTRAST_REG, value);

	return retVal;
}

//!Set brightness of sensor OV7675
/*!
 \param value is value to set to brightness register(0->255)
 \return
	 - MLS_SUCCESS:successful execution
	 - MLS_ERR_CX_OPEN_SPIDEV: open SPI device error
	 - MLS_ERR_CX_SPI_FAIL: SPI transmits error
 */
mlsErrorCode_t mlsOV7675_SetBrightness(UInt8 value)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	retVal = mlsCX93510_SensorOVWrite(OV_BRIGHT_REG,value);
	if(retVal == MLS_SUCCESS)
	{
		gmlsOVControl.brightnessValue = value;
	}

	return retVal;
}

//!Get brightness value of sensor OV7675
/*!
 \param *value is the pointer to get value of brightness register in sensor(0->255)
 \return
	 - MLS_SUCCESS:successful execution
	 - MLS_ERR_CX_OPEN_SPIDEV: open SPI device error
	 - MLS_ERR_CX_SPI_FAIL: SPI transmits error
 */
mlsErrorCode_t mlsOV7675_GetBrightness(UInt8 *value)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	retVal = mlsCX93510_SensorOVRead(OV_BRIGHT_REG, value);

	return retVal;
}

//!set resolution VGA for sensor OV7675
/*!
 \return
	 - MLS_SUCCESS:successful execution
	 - MLS_ERR_CX_OPEN_SPIDEV: open SPI device error
	 - MLS_ERR_CX_SPI_FAIL: SPI transmits error
 */
mlsErrorCode_t mlsOV7675_SetResolutionVGA()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt8 reg,value;

	/* Configuration Sensor OV7675R1B_A15_VGA_YUV30fps_M24Mhz*/
	reg = 0x12; value = 0x80;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x09; value = 0x10;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xc1; value = 0x7f;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x11; value = 0x80; //30 fps
	//reg = 0x11; value = 0x81; //15fps
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x3a; value = 0x0c;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x3d; value = 0xc0;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}


	reg = 0x12; value = 0x00;//mls@dev03 100922 change to 0x01 for test
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x15; value = 0x40;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x17; value = 0x13;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x18; value = 0x01;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x32; value = 0xbf;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x19; value = 0x02;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x1a; value = 0x7a;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x03; value = 0x0a;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x0c; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x3e; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x70; value = 0x3a;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x71; value = 0x35;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x72; value = 0x11;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x73; value = 0xf0;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xa2; value = 0x02;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x7a; value = 0x20;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x7b; value = 0x03;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x7c; value = 0x0a;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x7d; value = 0x1a;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x7e; value = 0x3f;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x7f; value = 0x4e;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x80; value = 0x5b;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x81; value = 0x68;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x82; value = 0x75;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x83; value = 0x7f;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x84; value = 0x89;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x85; value = 0x9a;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x86; value = 0xa6;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x87; value = 0xbd;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x88; value = 0xd3;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x89; value = 0xe8;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x13; value = 0xe0;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x00; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x10; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x0d; value = 0x40;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x14; value = 0x28;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xa5; value = 0x02;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xab; value = 0x02;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x24; value = 0x68;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x25; value = 0x58;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x26; value = 0xc2;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x9f; value = 0x78;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xa0; value = 0x68;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xa1; value = 0x03;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xa6; value = 0xd8;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xa7; value = 0xd8;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xa8; value = 0xf0;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xa9; value = 0x90;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xaa; value = 0x14;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x13; value = 0xe5;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x0e; value = 0x61;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x0f; value = 0x4b;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x16; value = 0x02;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x1e; value = 0x07;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x21; value = 0x02;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x22; value = 0x91;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x29; value = 0x07;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x33; value = 0x0b;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x35; value = 0x0b;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x37; value = 0x1d;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x38; value = 0x71;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x39; value = 0x2a;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x3c; value = 0x78;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x4d; value = 0x40;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x4e; value = 0x20;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x69; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x6b; value = 0x0a;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x74; value = 0x10;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x8d; value = 0x4f;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x8e; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x8f; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x90; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x91; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x96; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x9a; value = 0x80;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xb0; value = 0x84;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xb1; value = 0x0c;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xb2; value = 0x0e;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xb3; value = 0x82;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xb8; value = 0x0a;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x43; value = 0x0a;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x44; value = 0xf2;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x45; value = 0x39;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x46; value = 0x62;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x47; value = 0x3d;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x48; value = 0x55;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x59; value = 0x83;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x5a; value = 0x0d;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x5b; value = 0xcd;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x5c; value = 0x8c;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x5d; value = 0x77;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x5e; value = 0x16;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x6c; value = 0x0a;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x6d; value = 0x65;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x6e; value = 0x11;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x6a; value = 0x40;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x01; value = 0x56;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x02; value = 0x44;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x13; value = 0xe7;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x4f; value = 0x88;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x50; value = 0x8b;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x51; value = 0x04;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x52; value = 0x11;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x53; value = 0x8c;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x54; value = 0x9d;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/* default brightness */
	reg = 0x55; value = gmlsOVControl.brightnessValue;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/* default contrast */
	reg = 0x56; value = gmlsOVControl.contrastValue;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x57; value = 0x80;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x58; value = 0x9a;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x41; value = 0x08;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x3f; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x75; value = 0x04;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x76; value = 0x60;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x4c; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x77; value = 0x01;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x3d; value = 0xc2;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x4b; value = 0x09;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xc9; value = 0x30;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x41; value = 0x38;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x34; value = 0x11;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x3b; value = 0x12;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xa4; value = 0x00;//mls@dev03 TODO test 30 fps:0x88->0x00
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x96; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x97; value = 0x30;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x98; value = 0x20;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x99; value = 0x30;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x9a; value = 0x84;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x9b; value = 0x29;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x9c; value = 0x03;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x9d; value = 0x99;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x9e; value = 0x7f;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x78; value = 0x04;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x79; value = 0x01;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xc8; value = 0xf0;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x79; value = 0x0f;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xc8; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x79; value = 0x10;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xc8; value = 0x7e;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x79; value = 0x0a;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xc8; value = 0x80;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x79; value = 0x0b;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xc8; value = 0x01;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x79; value = 0x0c;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xc8; value = 0x0f;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x79; value = 0x0d;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xc8; value = 0x20;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x79; value = 0x09;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xc8; value = 0x80;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x79; value = 0x02;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xc8; value = 0xc0;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x79; value = 0x03;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xc8; value = 0x40;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x79; value = 0x05;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xc8; value = 0x30;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x79; value = 0x26;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x62; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x63; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x64; value = 0x06;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x65; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x66; value = 0x05;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x94; value = 0x05;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x95; value = 0x09;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x2a; value = 0x10;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x2b; value = 0xc2;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x15; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x3a; value = 0x04;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x3d; value = 0xc3;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x19; value = 0x03;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x1a; value = 0x7b;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x2a; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x2b; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x18; value = 0x01;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x66; value = 0x05;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x62; value = 0x10;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x63; value = 0x0b;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x65; value = 0x07;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x64; value = 0x0f;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x94; value = 0x0e;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x95; value = 0x10;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x4f; value = 0x87;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x50; value = 0x68;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x51; value = 0x1e;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x52; value = 0x15;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x53; value = 0x7c;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x54; value = 0x91;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x58; value = 0x1e;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x41; value = 0x38;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x76; value = 0xe0;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x24; value = 0x40;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x25; value = 0x38;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x26; value = 0x91;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x7a; value = 0x09;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x7b; value = 0x0c;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x7c; value = 0x16;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x7d; value = 0x28;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x7e; value = 0x48;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x7f; value = 0x57;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x80; value = 0x64;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x81; value = 0x71;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x82; value = 0x7e;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x83; value = 0x89;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x84; value = 0x94;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x85; value = 0xa8;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x86; value = 0xba;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x87; value = 0xd7;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x88; value = 0xec;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x89; value = 0xf9;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x09; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

		return retVal;
}

//!set resolution QVGA for sensor OV7675
/*!
 \return
	 - MLS_SUCCESS:successful execution
	 - MLS_ERR_CX_OPEN_SPIDEV: open SPI device error
	 - MLS_ERR_CX_SPI_FAIL: SPI transmits error
 */
mlsErrorCode_t mlsOV7675_SetResolutionQVGA()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt8 reg,value;

	/* Configuration Sensor OV7675 QVGA*/
	reg = 0x92; value = 0x88;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x93; value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xb9; value = 0x30;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x19; value = 0x02;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}


	reg = 0x1a; value =  0x3e;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x17; value =  0x13;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x18; value =  0x3b;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x03; value =  0x0a;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xe6; value =  0x05;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xd2; value =  0x1c;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}


	return retVal;
}

//!set resolution QQVGA for sensor OV7675
/*!
 \return
	 - MLS_SUCCESS:successful execution
	 - MLS_ERR_CX_OPEN_SPIDEV: open SPI device error
	 - MLS_ERR_CX_SPI_FAIL: SPI transmits error
 */
mlsErrorCode_t mlsOV7675_SetResolutionQQVGA()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt8 reg,value;

	reg = 0x92;value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x93;value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xb9;value = 0x00;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x19;value = 0x03;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x1a;value = 0x21;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x17;value = 0x13;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x18;value = 0x27;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0x03;value = 0x05;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xe6;value = 0x80;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	reg = 0xd2;value = 0x1c;
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	return retVal;

}
