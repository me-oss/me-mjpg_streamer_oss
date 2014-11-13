/*
 * mlsimage.h
 *
 *  Created on: Jul 6, 2011
 *      Author: mls@dev03
 */

#ifndef MLSIMAGE_H_
#define MLSIMAGE_H_

#include "mlsInclude.h"
#include "mlsjpeg.h"
#include "mlssensor.h"

//#if 1
//#define IMAGE_WIDTH  320//160//320
//#define IMAGE_HEIGHT 240//120//240
//#else
//#define IMAGE_WIDTH  640
//#define IMAGE_HEIGHT 480
//#endif

typedef enum icam_compression_enum
{
	icam_compression_standard = 0,
//	icam_compression_high = 1
}icam_compression_t;

typedef enum icam_resolution_enum
{
	icam_res_vga = 0,
	icam_res_qvga = 1,
	icam_res_qqvga = 2
}icam_resolution_t;

typedef struct rapbitImgConfig_st
{
	//videoin parameter
	int vinfd;//file description for video in
	videoin_param_t videoin_param;
	videoin_window_t videoin_window;
	videoin_encode_t tEncode;

	//jpegcode parameter
	int jpgfd;//file description for jpeg codec
	jpeg_param_t jpeg_param;

	unsigned int jpeg_buffer_size;
	char* pJpegBuffer;

}rapbitImgConfig_st;

///////////////////////////////////////////////////////
rapbitImgConfig_st rapbitImgConfig;
///////////////////////////////////////////////////////

mlsErrorCode_t mlsimageInit();
mlsErrorCode_t mlsimageDeInit();
mlsErrorCode_t mlsimageGetLastFrame(char * buffer, unsigned int *size);
mlsErrorCode_t mlsimage_SetResolution(icam_resolution_t resolutionMode);

#endif /* MLSIMAGE_H_ */
