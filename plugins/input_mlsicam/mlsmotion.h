/*
 * mlsmotion.h
 *
 *  Created on: Jul 25, 2011
 *      Author: mls@dev03
 */

#ifndef MLSMOTION_H_
#define MLSMOTION_H_

#include <asm/arch/w55fa93_reg.h>
#include "mlsInclude.h"
#include "../input.h"


////////////////#include "../pwm_driver/mlsw55fa93pwm.h"
//#define GPD0	8
//#define GPD1	GPD0 + 1
//#define GPD2	GPD0 + 2
//#define GPD3	GPD0 + 3

//define gpio group C
#define GPC0	100
#define GPC1	GPC0 + 1
#define GPC2	GPC0 + 2
#define GPC3	GPC0 + 3
#define GPC6	GPC0 + 6
//define gpio group D
#define GPD0	8
#define GPD1	GPD0 + 1
#define GPD2	GPD0 + 2
#define GPD3	GPD0 + 3
#define GPD6	GPD0 + 6
//define gpio group E
#define GPE0	120
#define GPE1	GPE0 + 1
#define GPE2	GPE0 + 2
#define GPE6	GPE0 + 6
#define GPE11	GPE0 + 11

#define MOTORLR_GPIOMOVE0   GPD0
#define MOTORLR_GPIOMOVE1	GPD1
#define MOTORLR_GPIOMOVE2	GPD2
#define MOTORLR_GPIOMOVE3	GPD3
#define MOTORLR_GPIOBOUNDARY GPD6

#define MOTORUD_GPIOMOVE0   GPC0
#define MOTORUD_GPIOMOVE1	GPC1
#define MOTORUD_GPIOMOVE2	GPC2
#define MOTORUD_GPIOMOVE3	GPC3
#define MOTORUD_GPIOBOUNDARY GPC6

mlsErrorCode_t mlsmotionInit();
mlsErrorCode_t mlsmotionDeInit();
mlsErrorCode_t mlsmotion_forward(UInt32 usduration,char checkBoundary);
mlsErrorCode_t mlsmotion_backward(UInt32 usduration,char checkBoundary);
mlsErrorCode_t mlsmotion_turnleft(UInt32 usduration,char checkBoundary);
mlsErrorCode_t mlsmotion_turnright(UInt32 usduration,char checkBoundary);
mlsErrorCode_t mlsmotion_stop();

mlsErrorCode_t _mlsmotorA_stop();
mlsErrorCode_t _mlsmotorA_forward(UInt32 usduration,char checkBoundary);
mlsErrorCode_t _mlsmotorA_backward(UInt32 usduration,char checkBoundary);

mlsErrorCode_t _mlsmotorB_stop();
mlsErrorCode_t _mlsmotorB_forward(UInt32 usduration,char checkBoundary);
mlsErrorCode_t _mlsmotorB_backward(UInt32 usduration,char checkBoundary);

mlsErrorCode_t mlsmotion_SetMovement(in_cmd_type direction,float factor);


#endif /* MLSMOTION_H_ */
