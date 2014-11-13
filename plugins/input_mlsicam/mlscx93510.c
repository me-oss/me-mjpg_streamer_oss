/*
 * mlscx93510.c
 *
 *  Created on: Aug 13, 2010
 *      Author: mlslnx003
 */

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
//#include <asm/io.h>
#include <linux/types.h>
//#include <linux/spi/spidev.h>
#include <sys/time.h>
#include <string.h>

#include "mlscx93510.h"
#include "mlsov7675.h"

#include "../../debug.h"
#define CX_FRAME_BUFFER_DIS_AUDIO 0x7fff
#define CX_FRAME_BUFFER_EN_AUDIO  0x7dff

//#define MLS_SPI_DEBUG;

/* enable one define frame capture mode */
#define CX_FRAMEMODE_CONTINUOUS
//#define CX_FRAMEMODE_LIMITED

/* enable one define resolution mode */
#define CX_RESOLUTION_VGA
//#define CX_RESOLUTION_QVGA

/*enable on define for quantization table */
#define CX_QUANTIZATIONTABLE_STD
//#define CX_QUANTIZATIONTABLE_HIGH

#define CX_CONFIG_DATA_LENGTH 688

mlsCX93510Config_st gCX;



static mlsErrorCode_t _mlsCX93510_SpiReadAddress(UInt8 address, UInt8 * buffRead, UInt32 length)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	Int32 i,ret = 0;
	UInt8 *tx;
	UInt8 *rx;
	UInt32 lenSpiLimit = 65535;//4096;
	UInt32 lenSpiTransfer = (length>lenSpiLimit)?lenSpiLimit:(length + 3);

	MLSCX_DEBUG_PRINTF(MLSCX_DEBUG_HEADER"SPI READ at address 0x%2X \n",address);

	/* malloc for tx and rx */
	tx = malloc(lenSpiTransfer);
	memset(tx,0xff,lenSpiTransfer);
	tx[0] = 0x00;
	tx[1] = address;
	tx[2] = 0x00;

	rx = malloc(lenSpiTransfer);
	memset(rx,0xff,lenSpiTransfer);


	gCX.spiTransfer.spiTransferConfig.tx_buf = (UInt64)tx;
	gCX.spiTransfer.spiTransferConfig.rx_buf = (UInt64)rx;

	if (lenSpiTransfer<lenSpiLimit)
	{
		gCX.spiTransfer.spiTransferConfig.len = lenSpiTransfer;

		ret = ioctl(gCX.spiTransfer.fd, SPI_IOC_MESSAGE(1), &gCX.spiTransfer.spiTransferConfig);
		if (ret < 1)
		{
			retVal = MLS_ERR_CX_SPI_FAIL;
			return retVal;
		}

		memcpy(buffRead,rx+3,length);

#if defined (MLS_SPI_DEBUG)
		MLSCX_DEBUG_PRINTF(MLSCX_DEBUG_HEADER"byte write:");
		for (i = 0; i<lenSpiTransfer ; i++)
		{
			MLSCX_DEBUG_PRINTF("0x%2x   ", tx[i]);
		}
		MLSCX_DEBUG_PRINTF("\n");

		MLSCX_DEBUG_PRINTF(MLSCX_DEBUG_HEADER"byte read:");
		for (i = 0; i<lenSpiTransfer ; i++)
		{
			MLSCX_DEBUG_PRINTF("0x%2x   ", rx[i]);
		}
		MLSCX_DEBUG_PRINTF("\n\n");
#endif

	}
	else
	{
		for (i = 0 ;i < (length/(lenSpiLimit-3)); i++)
		{
			gCX.spiTransfer.spiTransferConfig.tx_buf = (UInt64)tx;
			gCX.spiTransfer.spiTransferConfig.rx_buf = (UInt64)rx;

			gCX.spiTransfer.spiTransferConfig.len = lenSpiLimit;

			ret = ioctl(gCX.spiTransfer.fd, SPI_IOC_MESSAGE(1), &gCX.spiTransfer.spiTransferConfig);
			if (ret < 1)
			{
				retVal = MLS_ERR_CX_SPI_FAIL;
				return retVal;
			}

			memcpy(buffRead + i*(lenSpiLimit-3),rx+3,lenSpiLimit-3);
		}

		gCX.spiTransfer.spiTransferConfig.tx_buf = (UInt64)tx;
		gCX.spiTransfer.spiTransferConfig.rx_buf = (UInt64)rx;
		gCX.spiTransfer.spiTransferConfig.len = (length%(lenSpiLimit-3)) + 3;

		ret = ioctl(gCX.spiTransfer.fd, SPI_IOC_MESSAGE(1), &gCX.spiTransfer.spiTransferConfig);
		if (ret < 1)
		{
			retVal = MLS_ERR_CX_SPI_FAIL;
			return retVal;
		}

		memcpy(buffRead + (length/4093)*4093,rx+3,length%(lenSpiLimit-3));
	}

	free(tx);
	free(rx);

	return retVal;
}


static mlsErrorCode_t _mlsCX93510_SpiWriteAddress(UInt8 address, UInt8 *buffWriter, UInt32 length)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	int i,ret = 0;
	UInt8 *tx;
	UInt32 lenSpiTransfer = length + 3;

	MLSCX_DEBUG_PRINTF(MLSCX_DEBUG_HEADER"SPI WRITE data at address 0x%2X\n",address);

	tx = calloc(lenSpiTransfer, sizeof(UInt8));
	tx[0] = 0x80;
	tx[1] = address;
	tx[2] = 0x00;

	for (i = 3; i < lenSpiTransfer; i++)
	{
		tx[i] = buffWriter[i-3];
	}

	/*  write write command and address */
	gCX.spiTransfer.spiTransferConfig.tx_buf = (unsigned long)tx;
	gCX.spiTransfer.spiTransferConfig.rx_buf = (unsigned long)NULL;
	gCX.spiTransfer.spiTransferConfig.len = lenSpiTransfer;

	ret = ioctl(gCX.spiTransfer.fd, SPI_IOC_MESSAGE(1), &gCX.spiTransfer.spiTransferConfig);
	if (ret < 1 )
	{
		retVal = MLS_ERR_CX_SPI_FAIL;
		return retVal;
	}

#if defined (MLS_SPI_DEBUG)
	MLSCX_DEBUG_PRINTF(MLSCX_DEBUG_HEADER"byte write:");
	for (i = 0; i<ret ; i++)
	{
		MLSCX_DEBUG_PRINTF("0x%2x   ", tx[i]);
	}

	MLSCX_DEBUG_PRINTF("\n\n");
#endif

	free(tx);

	return retVal;
}


mlsErrorCode_t _mlsCX93510_WaitCondition(UInt8 reg,
										  UInt8 bit,
										  UInt8 valueWaitCond
										 )
{
	mlsErrorCode_t retVal;
	UInt8 value;

	do
	{
		// read register
		retVal = _mlsCX93510_SpiReadAddress(reg, &value,1);
		if (retVal != MLS_SUCCESS)
		{
			return retVal;
		}
	}
	while (((value&(1<<bit))>>bit) != valueWaitCond);

	return retVal;

}



/*
 * _cx_initial_spidev: initialize spi device
 */
static mlsErrorCode_t _cx_initial_spidev(spiTransfer_st *spi)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	Int32 ret;

	MLSCX_DEBUG_PRINTF(MLSCX_DEBUG_HEADER"Init SPI\n\n");

	spi->fd = open(gCX.spiTransfer.device, O_RDWR);

	if (spi->fd < 0)
	{
		MLSCX_DEBUG_PRINTF(MLSCX_DEBUG_HEADER"can't open device");
		retVal = MLS_ERR_CX_OPEN_SPIDEV;
		return retVal;
	}

	/*
	 * spi mode
	 */
//	printf("*** ioctl SPI_IOC_WR_MODE\n");
	ret = ioctl(spi->fd, SPI_IOC_WR_MODE, &(spi->mode));
	if (ret == -1)
	{
		MLSCX_DEBUG_PRINTF(MLSCX_DEBUG_HEADER"can't set spi mode");
		retVal = MLS_ERR_CX_SPI_FAIL;
		return retVal;
	}

//	printf("*** ioctl SPI_IOC_RD_MODE\n");
	ret = ioctl(spi->fd, SPI_IOC_RD_MODE, &(spi->mode));
	if (ret == -1)
	{
		MLSCX_DEBUG_PRINTF(MLSCX_DEBUG_HEADER"can't get spi mode");
		retVal = MLS_ERR_CX_SPI_FAIL;
		return retVal;
	}

	/*
	 * bits per word
	 */
//	printf("*** ioctl SPI_IOC_WR_BITS_PER_WORD\n");
	ret = ioctl(spi->fd, SPI_IOC_WR_BITS_PER_WORD, &(spi->spiTransferConfig.bits_per_word));
	if (ret == -1)
	{
		MLSCX_DEBUG_PRINTF(MLSCX_DEBUG_HEADER"can't set bits per word");
		retVal = MLS_ERR_CX_SPI_FAIL;
		return retVal;
	}

//	printf("*** ioctl SPI_IOC_RD_BITS_PER_WORD\n");
	ret = ioctl(spi->fd, SPI_IOC_RD_BITS_PER_WORD, &(spi->spiTransferConfig.bits_per_word));
	if (ret == -1)
	{
		MLSCX_DEBUG_PRINTF(MLSCX_DEBUG_HEADER"can't get bits per word");
		retVal = MLS_ERR_CX_SPI_FAIL;
		return retVal;
	}

	/*
	 * max speed hz
	 */
//	printf("*** ioctl SPI_IOC_WR_MAX_SPEED_HZ\n");
	ret = ioctl(spi->fd, SPI_IOC_WR_MAX_SPEED_HZ, &(spi->spiTransferConfig.speed_hz));
	if (ret == -1)
	{
		MLSCX_DEBUG_PRINTF(MLSCX_DEBUG_HEADER"can't set max speed hz");
		retVal = MLS_ERR_CX_SPI_FAIL;
		return retVal;
	}

//	printf("*** ioctl SPI_IOC_RD_MAX_SPEED_HZ\n");
	ret = ioctl(spi->fd, SPI_IOC_RD_MAX_SPEED_HZ, &(spi->spiTransferConfig.speed_hz));
	if (ret == -1)
	{
		MLSCX_DEBUG_PRINTF(MLSCX_DEBUG_HEADER"can't get max speed hz");
		retVal = MLS_ERR_CX_SPI_FAIL;
		return retVal;
	}

	return retVal;
}

/*
 *_cx_initial_interface: initialize interface of CX93510
 */
static mlsErrorCode_t _cx_initial_interface()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt8 spiRx[1];

	MLSCX_DEBUG_PRINTF(MLSCX_DEBUG_HEADER"CX init interface\n\n");

	/*1. delay 5ms */
	usleep(50000);

	/*2. Read register 0x50 first time */
	retVal = _mlsCX93510_SpiReadAddress(HI_SLAVE_SEL_CTRL, spiRx, 1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*3. Read resgiter 0x50 second time and check bits [1:0]*/
	retVal = _mlsCX93510_SpiReadAddress(HI_SLAVE_SEL_CTRL, spiRx, 1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	//check bits [1:0]
	if((spiRx[0]&0x03) == 0x01)
	{
		MLSCX_DEBUG_PRINTF(MLSCX_DEBUG_HEADER"SPI interface is initialized.\n");
		retVal = MLS_SUCCESS;
	}
	else
	{
		MLSCX_DEBUG_PRINTF(MLSCX_DEBUG_HEADER"SPI interface is failed while setup.\n");
		retVal = MLS_ERR_CX_SPI_FAIL;
	}

	return retVal;
}

/*
 *_cx_configure_sensor: configurate sensor OV7675
 */
static mlsErrorCode_t _cx_configure_sensor()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt8 reg,value;

	MLSCX_DEBUG_PRINTF(MLSCX_DEBUG_HEADER"configure sensor\n");

	/* check OV7675 sensor */
	/* Read PID of OV */
	retVal = mlsCX93510_SensorOVRead(0x0A,&value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	// this should be 0x76
	//printf(MLSCX_DEBUG_HEADER" PID =  %02X ?\n",value);
	if (value != 0x76)
	{
		retVal = MLS_ERR_OV_PID;
		return retVal;
	}

	/* Read VER of OV */
	retVal = mlsCX93510_SensorOVRead(0x0B,&value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	// this should be 0x73
	//printf(MLSCX_DEBUG_HEADER" VER =  %02X ?\n",value);
	if (value != 0x73)
	{
		retVal = MLS_ERR_OV_VER;
		return retVal;
	}

	/* Configurate OV7675 sensor */
	reg = 0x12; value = 0x80;//mls@dev03 20100901 0x00->0x10
	retVal = mlsCX93510_SensorOVWrite(reg,value);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

#if defined (CX_RESOLUTION_VGA)
	retVal = mlsOV7675_SetResolutionVGA();
#elif defined (CX_RESOLUTION_QVGA)
	retVal = mlsOV7675_SetResolutionQVGA();
#endif

	return retVal;
}

/*
 *_cx_setup_cx93510: setup CX93510
 */
static mlsErrorCode_t  _cx_setup_cx93510()
{
	mlsErrorCode_t retVal;
	UInt8 spiTx[1] = {0};
	UInt8 spiRx[1] = {0};

	MLSCX_DEBUG_PRINTF(MLSCX_DEBUG_HEADER"setup cx\n\n");

	/*0. Configure Sensor Interface */
	spiTx[0] = 0x24;
	retVal = _mlsCX93510_SpiWriteAddress(SI_SI_CFG_1,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/* Resolution Height */
	spiTx[0] = gCX.resolution.height/8;//0x1E;

	retVal = _mlsCX93510_SpiWriteAddress(0xA8,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/* Resolution Width */
	spiTx[0] = gCX.resolution.width/8;//0x28;

	retVal = _mlsCX93510_SpiWriteAddress(0xA5,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	//mls@dev03
	/* Select Quantization table */
	if (gCX.quantizationTable == cx_qt_standard)
	{
		//DCT table number for Luma
		spiTx[0] = 0x00;

		retVal = _mlsCX93510_SpiWriteAddress(JC_JPEG_ENC_DCT_LM,spiTx,1);
		if (retVal != MLS_SUCCESS)
		{
			return retVal;
		}

		//DCT table number for Chroma
		spiTx[0] = 0x01;

		retVal = _mlsCX93510_SpiWriteAddress(JC_JPEG_ENC_DCT_CH,spiTx,1);
		if (retVal != MLS_SUCCESS)
		{
			return retVal;
		}
	}
	else if (gCX.quantizationTable == cx_qt_highCompress)
	{
		//DCT table number for Luma
		spiTx[0] = 0x02;

		retVal = _mlsCX93510_SpiWriteAddress(JC_JPEG_ENC_DCT_LM,spiTx,1);
		if (retVal != MLS_SUCCESS)
		{
			return retVal;
		}

		//DCT table number for Chroma
		spiTx[0] = 0x03;

		retVal = _mlsCX93510_SpiWriteAddress(JC_JPEG_ENC_DCT_CH,spiTx,1);
		if (retVal != MLS_SUCCESS)
		{
			return retVal;
		}
	}

	/*1. Configuration parameters */
	// Set DIFF_JPEG_CTRL
	// Image mode
	// Frame buffer mode
	spiTx[0] = gCX.imageMode | gCX.frameBufferMode;

	retVal = _mlsCX93510_SpiWriteAddress(JC_DIFF_JPEG_CTRL,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*2. Set RST_COMP (bit 7 reg 0x20) */
	//Read reg 0x20
	spiRx[0] = 0;

	retVal = _mlsCX93510_SpiReadAddress(JC_DIFF_JPEG_CTRL,spiRx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	//Set RST_COMP
	spiTx[0] = (1<<7) | spiRx[0];

	retVal = _mlsCX93510_SpiWriteAddress(JC_DIFF_JPEG_CTRL,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*3. Read INIT_DONE at bit 0 reg 0x2B */
	retVal = _mlsCX93510_WaitCondition(JC_JPEG_DEC_STAT3,0,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*4. Clear RST_COMP(bit 7 reg 0x20) and set RELOAD_TABLES(bit 0 reg 0x26) */
	//Read reg 0x20
	spiRx[0] = 0;

	retVal = _mlsCX93510_SpiReadAddress(JC_DIFF_JPEG_CTRL,spiRx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	//Clear RST_COMP
	spiTx[0] = (~(1<<7))&spiRx[0];
	retVal = _mlsCX93510_SpiWriteAddress(JC_DIFF_JPEG_CTRL,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	//set RELOAD_TABLES(bit 0 reg 0x26)
	spiTx[0] = 0x21; //mls@dev03 20100827

	retVal = _mlsCX93510_SpiWriteAddress(JC_JPEG_ENC_CTL1,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*5. if limited frame capture, set frame # in bits [5:0] of reg 0xA2 */
	if (gCX.frameCapMode == cx_fc_limited)
	{
		spiTx[0] = 0x1F & gCX.frameLimitNumber; //mls@dev03 20100827

		retVal = _mlsCX93510_SpiWriteAddress(SI_SI_CFG_3,spiTx,1);
		if (retVal != MLS_SUCCESS)
		{
			return retVal;
		}
	}

	// enable audio
	retVal = _cx_enable_audio();

	return retVal;
}


/*
 *_cx_restart_capture: start or restart capture image from sensor and encode
 */
static mlsErrorCode_t _cx_restart_capture()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt8 spiTx[1];
	UInt8 spiRx[1];

	/*1. Clear bit 6 (EN_CFC) of reg 0xA1 */
	spiTx[0] = (~gCX.frameCapMode)&(0x0F&gCX.frameSkip);

	retVal = _mlsCX93510_SpiWriteAddress(SI_SI_CFG_2,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*2. Set RST_COMP (bit 7 reg 0x20) */
	//Read reg 0x20
	spiRx[0] = 0;

	retVal = _mlsCX93510_SpiReadAddress(JC_DIFF_JPEG_CTRL,spiRx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	//Set RST_COMP
	spiTx[0] = (1<<7)|spiRx[0];
	retVal = _mlsCX93510_SpiWriteAddress(JC_DIFF_JPEG_CTRL,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*3. Read INIT_DONE at bit 0 reg 0x2B */
	retVal = _mlsCX93510_WaitCondition(JC_JPEG_DEC_STAT3,1,0);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*4. Clear RST_COMP(bit 7 reg 0x20) and reload table*/
	//Read reg 0x20
	spiRx[0] = 0;

	retVal = _mlsCX93510_SpiReadAddress(JC_DIFF_JPEG_CTRL,spiRx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	//Clear RST_COMP
	spiTx[0] = (~(1<<7))&spiRx[0];

	retVal = _mlsCX93510_SpiWriteAddress(JC_DIFF_JPEG_CTRL,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	//set RELOAD_TABLES(bit 0 reg 0x26)
	spiTx[0] = 0x21;

	retVal = _mlsCX93510_SpiWriteAddress(JC_JPEG_ENC_CTL1,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*5. Set bit 6 (EN_CFC) of reg 0xA1 */
	spiTx[0] = gCX.frameCapMode|(0x0F&gCX.frameSkip);

	retVal = _mlsCX93510_SpiWriteAddress(SI_SI_CFG_2,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*6 Update last frame is config data */
	gCX.LastFBAddress = 0x0000;
	gCX.LastFrameSize = CX_CONFIG_DATA_LENGTH;

	return retVal;
}

/*
 *_cx_stop_capture: stop caputure and encode image data from sensor
 */
static mlsErrorCode_t _cx_stop_capture()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt8 spiTx[1];

	/*1. Clear bit 6 (EN_CFC) of reg 0xA1 */
	spiTx[0] = (~gCX.frameCapMode)&(0x0F&gCX.frameSkip);

	retVal = _mlsCX93510_SpiWriteAddress(SI_SI_CFG_2,spiTx,1);

	return retVal;
}

/*
 *_cx_enable_audio: enable buffer audio and setup audio data
 */
mlsErrorCode_t _cx_enable_audio()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt8 spiTx[1];
	/* Enable audio */
	spiTx[0] = 0x06;//ADPCM
//	spiTx[0] = 0x0E;//Raw PCM
	retVal = _mlsCX93510_SpiWriteAddress(HI_HWR_FB_EN,spiTx,1);

	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	gCX.frameBufferSize = CX_FRAME_BUFFER_EN_AUDIO;

	return retVal;
}

/*
 * _cx_disable_audio: disable buffer audio
 */
mlsErrorCode_t _cx_disable_audio()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt8 spiTx[1];
	/* Disable audio */
	spiTx[0] = 0x00;

	retVal = _mlsCX93510_SpiWriteAddress(HI_HWR_FB_EN,spiTx,1);

	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	gCX.frameBufferSize = CX_FRAME_BUFFER_DIS_AUDIO;

	return retVal;
}

/*
 * _cx_start_audio: start capture audio
 */
mlsErrorCode_t _cx_start_audio()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt8 spiTx[0];

	/* set micro boost PGA gain control */
	spiTx[0] = 0xf0;//gain = 36dB
//	spiTx[0] = 0xC0;//gain = 24dB
//	spiTx[0] = 0xD0;//gain = 30dB
	retVal = _mlsCX93510_SpiWriteAddress(ADC1,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/* enable ADC mic */
	spiTx[0] = 0x01;

	retVal = _mlsCX93510_SpiWriteAddress(ADC_CTRL_DIG1,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/* enable audio */
	retVal = _cx_enable_audio();

	return retVal;
}

/*
 *_cx_stop_audio: stop capture audio
 */
mlsErrorCode_t _cx_stop_audio()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt8 spiTx[1];

	/* set micro boost PGA gain control */
	spiTx[0] = 0x00;

	retVal = _mlsCX93510_SpiWriteAddress(0x00,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/* disable ADC mic */
	spiTx[0] = 0x00;

	retVal = _mlsCX93510_SpiWriteAddress(ADC_CTRL_DIG1,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/* disable audio */
	retVal = _cx_disable_audio();
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	return retVal;
}

/*
 * _cx_check_audio_full: check audio buffer full
 */
mlsErrorCode_t _cx_check_audio_full(Bool *blbuffAudioFull)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt8 spiRx[1];

	//mls@dev03 100918 check half buffer is full?
	retVal = _mlsCX93510_SpiReadAddress(HI_AUD_BUFF_STAT,spiRx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	if ((spiRx[0]&0x02) == 0x02) //full buffer
	{
		printf("Audio buffer full \n");
		*blbuffAudioFull = True;
	}
	else
	{
		*blbuffAudioFull = False;
	}

	return retVal;
}


/*
 *_cx_fba_getStatusQword: get status word of frame JPEG in CX93510 buffer, if had error,
 *will restart capture and read frame at address 0x0000
 */
static mlsErrorCode_t _cx_fba_getStatusQword(UInt32 *frameAddress, UInt8 *statusQword)
{
	mlsErrorCode_t retVal;
	UInt8 spiTx[1];
	UInt8 spiRx[8];
	UInt32 frameBufferAddress;

restart_capture_label:
	frameBufferAddress = *frameAddress;
	/*1. Load Frame buffer Address in reg 0x51(msb) and 0x52(lsb) */
	//load FB address msb to reg 0x51
	spiTx[0] = frameBufferAddress >> 8;

	retVal = _mlsCX93510_SpiWriteAddress(HI_FB_ADDR_0,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	//load FB address lsb to reg 0x52
	spiTx[0] =frameBufferAddress & 0xFF;

	retVal = _mlsCX93510_SpiWriteAddress(HI_FB_ADDR_1,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*2. Request Video Access by setting bits [4:3] of reg 0x53 = b'10 */
	spiTx[0] = 0x10;

	retVal = _mlsCX93510_SpiWriteAddress(HI_FB_ADDR_2,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*3. Read Error Status in reg 0x54, AND with 0xFC */
	retVal = _mlsCX93510_SpiReadAddress(HI_ERR_STAT,&gCX.errStatus,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	gCX.errStatus = gCX.errStatus & 0xFC;

	if (gCX.errStatus != 0x00)
	{
		/* error read, restart capture */
		printf("Restart capture, errStatus is %x \n",gCX.errStatus);

		// restart the capture sequences
		retVal = _cx_restart_capture();
		if (retVal != MLS_SUCCESS)
		{
			return retVal;
		}

		*frameAddress = 0x0000;

		goto restart_capture_label;

	}

	/*4. Read Prefetch_Done = 1? (bit 5, reg 0x53) */
	retVal = _mlsCX93510_WaitCondition(HI_FB_ADDR_2,5,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}


	/*5. Read 8 byte Status Qword from Frame Buffer (reg 0xCC) */
	retVal = _mlsCX93510_SpiReadAddress(HI_HRDDATA_FB,spiRx,8);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	memcpy(statusQword, spiRx, 8);

	return retVal;
}


/*
 *_cx_fba_readFrameBuffer: read frame data in buffer CX93510
 */
static mlsErrorCode_t _cx_fba_readFrameBuffer(UInt32 frameLength, UInt8 *buffer)
{
	mlsErrorCode_t retVal;

	retVal = _mlsCX93510_SpiReadAddress(HI_HRDDATA_FB,buffer,frameLength);

	return retVal;
}

/*
 * _cx_getConfigData: get configuration JPEG data in buffer CX93510
 */
static mlsErrorCode_t _cx_getConfigData()
{
	mlsErrorCode_t retVal;
	UInt8 statusQword[8];
	UInt8 frameType;
	UInt32 frameLength,frameAddress;

	frameAddress = 0x0000;

	retVal = _cx_fba_getStatusQword(&frameAddress, statusQword);

	//get frame type
	frameType = statusQword[0] >> 4;
	if (frameType != cx_ft_configuration)
	{
		retVal = MLS_ERR_CX_GETCONFIGDATA;
		return retVal;
	}

	//get frame length
	frameLength = (statusQword[6]<<8) + statusQword[7];

	if ((frameType == cx_ft_configuration)&&(frameLength == CX_CONFIG_DATA_LENGTH))
	{
		retVal = _cx_fba_readFrameBuffer(frameLength,gCX.configBuffer);
		if (retVal != MLS_SUCCESS)
		{
			retVal = MLS_ERR_CX_GETCONFIGDATA;
			return retVal;
		}
	}

	gCX.configBufferLength = CX_CONFIG_DATA_LENGTH;

	return retVal;
}

/*
 *_mlsCX93510_SetupDefault: setup default configuration parameters for CX93510
 *\param *cxConfig is the pointer to struct which used to configurate CX93510
 */
mlsErrorCode_t _mlsCX93510_SetupDefault(mlsCX93510Config_st *cxConfig)
{
	/* spi configuration */
	cxConfig->spiTransfer.device = "/dev/spidev0.0";
	cxConfig->spiTransfer.spiTransferConfig.speed_hz =   400000000;
//	cxConfig->spiTransfer.spiTransferConfig.speed_hz = 27000000;
	cxConfig->spiTransfer.spiTransferConfig.bits_per_word = 8;
	cxConfig->spiTransfer.spiTransferConfig.delay_usecs = 0;
	cxConfig->spiTransfer.spiTransferConfig.cs_change = 0;
	cxConfig->spiTransfer.mode = SPI_MODE_0;

#if defined (CX_RESOLUTION_VGA)
    cxConfig->resolution.width = 640;
	cxConfig->resolution.height = 480;
#elif defined (CX_RESOLUTION_QVGA)
    cxConfig->resolution.width = 320;
	cxConfig->resolution.height = 240;
#endif

#if defined (CX_QUANTIZATIONTABLE_STD)
	cxConfig->quantizationTable = cx_qt_standard;
#elif defined (CX_QUANTIZATIONTABLE_HIGH)
	cxConfig->quantizationTable = cx_qt_highCompress;
#endif
	/* cx93510 configuration */
#if defined (CX_FRAMEMODE_LIMITED)
	cxConfig->frameCapMode = cx_fc_limited;
	cxConfig->frameLimitNumber = 1;
	cxConfig->frameSkip = 0;
	cxConfig->frameBufferMode = cx_fb_limited;
	cxConfig->imageMode = cx_im_JPEG;
#elif defined (CX_FRAMEMODE_CONTINUOUS)
	cxConfig->frameCapMode = cx_fc_continuous;
	cxConfig->frameSkip = 0;

	cxConfig->frameBufferMode = cx_fb_continuous;

	cxConfig->imageMode = cx_im_JPEG;
#endif
	cxConfig->LastFBAddress = 0;
	cxConfig->LastFrameSize = 0;
	cxConfig->configBufferLength = 0;
	cxConfig->errStatus = 0x00;

	cxConfig->frameBufferSize = CX_FRAME_BUFFER_DIS_AUDIO;

	return MLS_SUCCESS;
}

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
mlsErrorCode_t mlsCX93510_Init()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	/* Setup default initial configuration for CX93510 */
	_mlsCX93510_SetupDefault(&gCX);

	// init spi device
	retVal = _cx_initial_spidev(&gCX.spiTransfer);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	//init interface with CX93510
	retVal = _cx_initial_interface();
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	//check and configure sensor
	retVal = _cx_configure_sensor();
	if (retVal != MLS_SUCCESS)
	{
		retVal = MLS_ERR_OV;
		return retVal;
	}

	//setup CX93510
	retVal = _cx_setup_cx93510();
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	//get configuration data for encoding JPEG from CX93510
	retVal = _cx_getConfigData();
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	return retVal;
}

//!DeInitialize encoder CX93510
/*!
 *
 * \param none
 * \return error code
 *  - MLS_SUCCESS : successful execution
 *  - MLS_ERR_CX_SPI_FAIL: SPI transmit error
 */
mlsErrorCode_t mlsCX93510_DeInit()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	retVal = _cx_stop_capture();
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	retVal = _cx_stop_audio();
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	if (gCX.spiTransfer.fd>0)
	{
		close(gCX.spiTransfer.fd);
	}
	return retVal;
}

//!Read value from the register address in sensor OV7675 via CX93510
/*!
 * \param ovAddress is the address of register in sensor OV7675
 * \param *value is the pointer to the variable which will get value from the register in sensor OV7675
 * \return error code
 *  - MLS_SUCCESS : successful execution
 *  - MLS_ERR_CX_SPI_FAIL: SPI transmit error
 */
mlsErrorCode_t mlsCX93510_SensorOVRead(UInt8 ovAddress, UInt8 *value)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt8 spiTx[1] = {0};
	UInt8 spiRx[1] = {0};

	/*1. read reg 0xB0 and check bit 2 (I2C_RACK) until ready (=1) */
	retVal = _mlsCX93510_WaitCondition(SI_I2C_CTL_3,1,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*2. Write sensor Device ID of read in bit [7:1] of register 0xA9 */
	// Write Sensor Device ID
	spiTx[0] = OV7675_ADDRESS;

	retVal = _mlsCX93510_SpiWriteAddress(SI_I2C_DADDR,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*3. Write value 0x51 to register 0xAF for OmniVision configure SCCB parameters */
	spiTx[0] = 0x51; //for OmniVision sensor;

	retVal = _mlsCX93510_SpiWriteAddress(SI_I2C_CTL_2,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*4, Write sensor register address in read 0xAA */
	spiTx[0] = ovAddress;

	retVal = _mlsCX93510_SpiWriteAddress(SI_I2C_LO_ADDR,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*5. Set reg 0xB0 = 0x20 for 8 bit read */
	spiTx[0] = 0x20;

	retVal = _mlsCX93510_SpiWriteAddress(SI_I2C_CTL_3,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*6. read reg 0xB0 and check bit 2 (I2C_RACK) until ready (=1) */
	retVal = _mlsCX93510_WaitCondition(SI_I2C_CTL_3,1,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*7. read data from reg 0xAC */
	retVal = _mlsCX93510_SpiReadAddress(SI_I2C_LO_DATA,spiRx,1);

	if (retVal == MLS_SUCCESS)
	{
		*value = spiRx[0];
	}

	return retVal;
}

//!Write value to the register address in sensor OV7675 via CX93510
/*!
 * \param ovAddress is the address of register in sensor OV7675
 * \param value is value which use to set value to the register in sensor OV7675
 * \return error code
 *  - MLS_SUCCESS : successful execution
 *  - MLS_ERR_CX_SPI_FAIL: SPI transmit error
 */
mlsErrorCode_t mlsCX93510_SensorOVWrite(UInt8 ovAddress, UInt8 value)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt8 spiTx[1] = {0};

	retVal = _mlsCX93510_WaitCondition(SI_I2C_CTL_3,1,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*2. Write sensor Device ID of read in bit [7:1] of register 0xA9 */
	// Write Sensor Device ID
	spiTx[0] = OV7675_ADDRESS;

	retVal = _mlsCX93510_SpiWriteAddress(SI_I2C_DADDR,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*3. Write value 0x51 to register 0xAF for OmniVision configure SCCB parameters */
	spiTx[0] = 0x51; //for OmniVision sensor;

	retVal = _mlsCX93510_SpiWriteAddress(SI_I2C_CTL_2,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*4, Write sensor register address in register 0xAA */
	spiTx[0] = ovAddress;

	retVal = _mlsCX93510_SpiWriteAddress(SI_I2C_LO_ADDR,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*5. Set reg 0xB0 = 0x00 for 8 bit write */
	spiTx[0] = 0x00;

	retVal = _mlsCX93510_SpiWriteAddress(SI_I2C_CTL_3,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*6. write data to register 0xAC */
	spiTx[0] = value;
	retVal = _mlsCX93510_SpiWriteAddress(SI_I2C_LO_DATA,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/*7. read reg 0xB0 and check bit 2 (I2C_RACK) until ready (=1) */
	retVal = _mlsCX93510_WaitCondition(SI_I2C_CTL_3,1,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	return retVal;
}

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
mlsErrorCode_t mlsCX93510_GetLastFrame(UInt8* buffer, UInt32 *size, UInt32 *nbFrameMiss)
{
	mlsErrorCode_t retVal;
	UInt8 statusQword[8];
	UInt32 newFrameAddress;
	cxFrameType_enum frameType;
	UInt32 frameSize;
	UInt8 i;
	Bool flag = True;
	Bool frameErr = False;
	Bool blAudioBufferFull;

	static unsigned int countFrame =0;
	static unsigned long countSize =0;
	static unsigned int countMiss =0;

#if defined (CX_FRAMEMODE_LIMITED)
	/* Clear Frame buffer of CX93510 */
	retVal = _cx_restart_capture();
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/* Get status qword from FB address 0x0000 */
	do
	{
		UInt32 frameAddress = 0x0057;
		retVal = _cx_fba_getStatusQword(&frameAddress,statusQword);
		if (retVal != MLS_SUCCESS)
		{
			return retVal;
		}
	}while (statusQword[0] == 0x00);

	/* Get frame size from status qword */
	frameSize = statusQword[6]<<8 | statusQword[7];

	/* Update new FB address */
	gCX.LastFBAddress = 0x0000;
	gCX.LastFrameSize = frameSize;

#elif defined (CX_FRAMEMODE_CONTINUOUS)

	UInt32 checkFrameAddress;
	UInt8 zeroQword[8] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

	*nbFrameMiss = 0;

	/*0. Start from last address in gCX */
	newFrameAddress = gCX.LastFBAddress + ((gCX.LastFrameSize+8)/ 8);
	if (gCX.LastFrameSize%8 != 0)
	{
		newFrameAddress+=1;
	}

	if(newFrameAddress >= gCX.frameBufferSize)
	{
		if (gCX.frameBufferMode == cx_fb_limited)
		{
			newFrameAddress = 0x0000;
		}else if (gCX.frameBufferMode == cx_fb_continuous)
		{
			newFrameAddress = newFrameAddress % (gCX.frameBufferSize+1);
		}

	}

	frameSize = gCX.LastFrameSize;

	do
	{
		/* 1. Get Status Qword */
		retVal = _cx_fba_getStatusQword(&newFrameAddress,statusQword);
		if (retVal != MLS_SUCCESS)
		{
			return retVal;
		}

		if ((statusQword[0]&0x0F) == 0x0)
		{
			frameType = statusQword[0]>>4;
			switch (frameType)
			{
				case cx_ft_configuration:
					frameSize = statusQword[6]<<8 | statusQword[7];
					newFrameAddress = 0x57;
					break;
				case cx_ft_difference:
				case cx_ft_direct:
				case cx_ft_reference:

					frameSize = statusQword[6]<<8 | statusQword[7];
//					printf("frame data size: %d\n",frameSize);

					/* check is the last frame */

					//Set checkFrameAddress to new offset of new Status Qword
					checkFrameAddress = newFrameAddress + ((frameSize+8)/ 8);
					if (frameSize%8 != 0)
					{
						checkFrameAddress+=1;
					}

					if(checkFrameAddress >= gCX.frameBufferSize)
					{
						checkFrameAddress = checkFrameAddress % (gCX.frameBufferSize+1);
					}
					//Read status Qword at new offset
					retVal = _cx_fba_getStatusQword(&checkFrameAddress,statusQword);
					if (retVal != MLS_SUCCESS)
					{
						return retVal;
					}

					//mls@dev03
//					printf(" %2X \n ",statusQword[0]);

					//check status Qword
					if (memcmp(statusQword,zeroQword,8) == 0)
					{
						//it is the last frame, stop looping
						flag = False;

						//return to address of newFrameAddress
						retVal = _cx_fba_getStatusQword(&newFrameAddress,statusQword);
						if (retVal != MLS_SUCCESS)
						{
							return retVal;
						}

//						printf("last frame\n");

					}
					else
					{
						//not the last frame, move to next frame
						newFrameAddress = checkFrameAddress;

						*nbFrameMiss=*nbFrameMiss+1;

						printf("miss\n");
					}

					break;
				case cx_ft_end:
					usleep(10000);
					printf("end\n");
					break;
				default:
					/* error status Qword */
					frameErr = True;
					break;
			}
		}
		else //Frame error
		{
			frameErr = True;
		}

			/* error read, restart capture */
		if (frameErr == True)
		{
			retVal = _cx_restart_capture();
			if (retVal != MLS_SUCCESS)
			{
				return retVal;
			}
			newFrameAddress = 0x0000;

			//mls@dev03 100928 restart audio
			retVal = _cx_start_audio();
			if (retVal != MLS_SUCCESS)
			{
				return retVal;
			}
			printf("\nFrame type error ,Restart capture\n");
//			printf("last frame size %d \n",frameSize);
//			printf("Status word at address %X :",newFrameAddress);
//			for (i  = 0; i<8; i++)
//			{
//				printf(" %2X ",statusQword[i]);
//			}

			printf("\n\n");

			frameErr = False;
		}

	}while (flag);

	//mls@dev03 check audio buffer full for 1s timeout
	retVal =  _cx_check_audio_full(&blAudioBufferFull);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}
	else if (blAudioBufferFull == True)
	{
		//mls@dev03 101018 reset audio buffer
		retVal = _cx_start_audio();
		if (retVal != MLS_SUCCESS)
		{
			return retVal;
		}
		return MLS_ERR_CX_TIMEOUT_1S;
	}

	/*1. Update new FB address */
	gCX.LastFBAddress = newFrameAddress;
	gCX.LastFrameSize = frameSize;
#endif //if 1 countinuous

	/* Get the JPEG compliant image data from last frame in CX93510 Frame Buffer */
	retVal = _cx_fba_readFrameBuffer(frameSize,buffer);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/* move JPEG compliant image data to (CX_CONFIG_DATA_LENGTH-4)bytes
	 * then copy configuration data (CX_CONFIG_DATA_LENGTH -2)bytes to the head of buffer
	 */
	memmove(buffer+CX_CONFIG_DATA_LENGTH-4,buffer,frameSize);
	memcpy(buffer,gCX.configBuffer,CX_CONFIG_DATA_LENGTH-2);

	*size = (CX_CONFIG_DATA_LENGTH-4) + frameSize;

	//mls@dev03 20110511 test fps
	countFrame++;
	countSize = countSize+(*size);
	if (*nbFrameMiss>0)
	{
		countMiss++;
	}

	if (countFrame == 1000)
	{
		printf("Get 1000 frames, total size is %ld, miss %d frames \n",countSize,countMiss);
		countFrame = 0;
		countSize = 0;
		countMiss = 0;
	}

	return retVal;
}

//!Start or restart CX93510 to capture image from sensor and encode to JPEG data
/*!
 * \param none
 * \return error code
 *  - MLS_SUCCESS : successful execution
 *  - MLS_ERR_CX_SPI_FAIL: SPI transmit error
 */
mlsErrorCode_t mlsCX93510_RestartCapture()
{
	return _cx_restart_capture();
}

//!Set compression mode in CX93510
/*!
 * \param compressionMOde is the compression mode which needs to change
 * \return error code
 * \sa enum icam_compression_t
 */
mlsErrorCode_t mlsCX93510_SetCompression(icam_compression_t compressionMode)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt8 spiTx[1];

	retVal = _cx_stop_capture();
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	switch (compressionMode)
	{
		case icam_compression_standard:
			//DCT table number for Luma
			spiTx[0] = 0x00;

			retVal = _mlsCX93510_SpiWriteAddress(JC_JPEG_ENC_DCT_LM,spiTx,1);
			if (retVal != MLS_SUCCESS)
			{
				return retVal;
			}

			//DCT table number for Chroma
			spiTx[0] = 0x01;

			retVal = _mlsCX93510_SpiWriteAddress(JC_JPEG_ENC_DCT_CH,spiTx,1);
			if (retVal != MLS_SUCCESS)
			{
				return retVal;
			}
			break;

		case icam_compression_high:
			//DCT table number for Luma
			spiTx[0] = 0x02;

			retVal = _mlsCX93510_SpiWriteAddress(JC_JPEG_ENC_DCT_LM,spiTx,1);
			if (retVal != MLS_SUCCESS)
			{
				return retVal;
			}

			//DCT table number for Chroma
			spiTx[0] = 0x03;

			retVal = _mlsCX93510_SpiWriteAddress(JC_JPEG_ENC_DCT_CH,spiTx,1);
			if (retVal != MLS_SUCCESS)
			{
				return retVal;
			}
			break;

		default:
			break;
	}

	retVal = _cx_restart_capture();

	return retVal;
}

//!Set resolution mode in CX93510
/*!
 * \param compressionMode is the resolution mode which needs to change
 * \return error code
 *  - MLS_SUCCESS : successful execution
 *  - MLS_ERR_CX_SPI_FAIL: SPI transmit error
 * \sa enum icam_resolution_t
 */
mlsErrorCode_t mlsCX93510_SetResolution(icam_resolution_t resolutionMode)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt8 spiTx[1];

	retVal = _cx_stop_capture();
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	switch (resolutionMode)
	{
		case icam_res_vga:
		    gCX.resolution.width = 640;
			gCX.resolution.height = 480;
			//mls@dev03 101017 add frame skip = 0;
			gCX.frameSkip = 0;
			retVal = mlsOV7675_SetResolutionVGA();
			if (retVal != MLS_SUCCESS)
			{
				printf("resolution vga error!\n");
				return retVal;
			}
			break;

		case icam_res_qvga:
		    gCX.resolution.width = 320;
			gCX.resolution.height = 240;
			//mls@dev03 101017 add frame skip = 1;
			gCX.frameSkip = 1;
			retVal = mlsOV7675_SetResolutionQVGA();
			if (retVal != MLS_SUCCESS)
			{
				printf("resolution qvga error\n");
				return retVal;
			}
			break;

		case icam_res_qqvga:
		    gCX.resolution.width = 160;
			gCX.resolution.height = 120;
			//mls@dev03 101017 add frame skip = 5;
			gCX.frameSkip = 5;
			retVal = mlsOV7675_SetResolutionQQVGA();
			if (retVal != MLS_SUCCESS)
			{
				printf("resolution qqvga error\n");
				return retVal;
			}
			break;

		default:
			break;
	}

	/* Setup resolution in CX93510 */
	/* Resolution Height */
	spiTx[0] = gCX.resolution.height/8;//0x1E;

	retVal = _mlsCX93510_SpiWriteAddress(0xA8,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/* Resolution Width */
	spiTx[0] = gCX.resolution.width/8;//0x28;

	retVal = _mlsCX93510_SpiWriteAddress(0xA5,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	retVal = _cx_restart_capture();

	return retVal;
}

//!Start capture audio in CX93510
/*!
 * \param none
 * \return error code
 *  - MLS_SUCCESS : successful execution
 *  - MLS_ERR_CX_SPI_FAIL: SPI transmit error
 */
mlsErrorCode_t mlsCX93510_StartAudio()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;

	retVal = _cx_start_audio();

	return retVal;
}

//!Get audio data in buffer of CX93510
/*!
 * \param *abBuff is the pointer to buffer that will contain audio data
 * \param *size is the pointer to variable that will get size of audio data
 * \return error code
 *  - MLS_SUCCESS : successful execution
 *  - MLS_ERR_CX_SPI_FAIL: SPI transmit error
 */
mlsErrorCode_t mlsCX93510_GetAudioData(UInt8* abBuff, UInt32 *size)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	UInt8 spiTx[1],spiRx[0];
	UInt16 nbQword,nbByte,nbBlock;

	/* check AUD_HALF_FULL=1, bit 0, reg 0x57 */
#if 0
	//check half buffer
	retVal = _mlsCX93510_WaitCondition(HI_AUD_BUFF_STAT,0,1);
	if (retVal != MLS_SUCCESS)
	{
		printf("half buffer error \n");
		return retVal;
	}

	//mls@dev03 100918 check is audio buffer is full
	retVal = _mlsCX93510_SpiReadAddress(HI_AUD_BUFF_STAT,spiRx,1);
	if (retVal != MLS_SUCCESS)
	{
		printf("full buffer error\n");
		return retVal;
	}
#else
	//mls@dev03 100918 check half buffer is full?
	retVal = _mlsCX93510_SpiReadAddress(HI_AUD_BUFF_STAT,spiRx,1);
	if (retVal != MLS_SUCCESS)
	{
		printf("read audio buffer reg error\n");
		return retVal;
	}

//	if ((spiRx[0]&0x01) == 0x00) //full half buffer
//	if ((spiRx[0]&0x02) == 0x00) //full buffer
	if ((spiRx[0]&0x04) == 0x01) //empty buffer
	{
		*size = 0;
		return MLS_SUCCESS;
	}
#endif

	/* request audio access bit [4:3] of reg 0x53 = 01 */
	spiTx[0] = 0x08;

	retVal = _mlsCX93510_SpiWriteAddress(HI_FB_ADDR_2,spiTx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/* check prefect done = 1 (bit 5, reg 0x53) */
	retVal = _mlsCX93510_WaitCondition(HI_FB_ADDR_2,5,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	/* read number of audio Qwords from registers 0x5A, 0x5B(MSB,LSB) */
	nbQword = 0;
	//Get MLB
	spiRx[0] = 0;
	retVal = _mlsCX93510_SpiReadAddress(HI_AUD_QWORDS_0,spiRx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	//MLB
	nbQword = spiRx[0]<<2;

	//Get LSB
	spiRx[0] = 0;
	retVal = _mlsCX93510_SpiReadAddress(HI_AUD_QWORDS_1,spiRx,1);
	if (retVal != MLS_SUCCESS)
	{
		return retVal;
	}

	//LSB
	nbQword = nbQword | (spiRx[0]&0x03);

	/* calculate number of bytes to read
	 * (Audio Qwords - 2 Qwords)*8
	 */

	nbBlock = (nbQword - 2)*8/256;
	nbBlock = nbQword*8/256;
	nbByte = nbBlock*256;

	/* Read calculated # of Audio Data Bytes from reg 0xCC*/
	retVal = _cx_fba_readFrameBuffer(nbByte,abBuff);

	*size = nbByte;

	return retVal;
}
