/**********************************************************
 * mlsVox.h
 *
 *  Created on: Nov 10, 2011
 *      Author: mlsdev008
 ***********************************************************/

#ifndef MLSVOX_H_
#define MLSVOX_H_
#include "mlsInclude.h"
#include "../input.h"
#include "mlsaudio.h"
//define to debug
#define debug
#ifdef debug
#define print(...) printf(__VA_ARGS__)
#else
#define print(...)
#endif
//define type
#define ENABLE		1
#define DISABLE		0
typedef struct{
	int voxStatus;
	float threshold;
	audio_vox_ctrl direction;
}vox;

//function
extern mlsErrorCode_t mlsVoxEnable(char *addr);
extern mlsErrorCode_t mlsVoxDisable();
extern mlsErrorCode_t mlsVoxGetThreshold(int *reg);
extern mlsErrorCode_t mlsVoxSetThreshold(char *threshold);
extern mlsErrorCode_t mlsVoxGetStatus(int *reg);

#endif /* MLSVOX_H_ */
