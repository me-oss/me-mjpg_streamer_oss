/*
 * mlsio.h
 *
 *  Created on: Aug 4, 2011
 *      Author: root
 */

#ifndef MLSIO_H_
#define MLSIO_H_

#include "mlsInclude.h"
#include "../input.h"
#include "mlsWifi.h"

#define SETUPLED_ON  0
#define SETUPLED_OFF 1


#define GPA0			0
#define GPA5			GPA0 + 5
//define gpio group E
#define GPE0	120
#define GPE8	GPE0 + 8
//define gpio group D
#define GPD0	8
#define GPD1	GPD0 + 1
#define GPD2	GPD0 + 2
#define GPD3	GPD0 + 3
#define GPD5	GPD0 + 5
#define GPD6	GPD0 + 6
#define PANDA_GPIO_GET	2

mlsErrorCode_t mlsioInit();
mlsErrorCode_t mlsioDeInit();

mlsErrorCode_t mlsioSetBlinkDuration(UInt32 timeon, UInt32 timeoff);
int mlsioGetLedConfStop();
mlsErrorCode_t mlsioSetSetupLed(in_cmd_type controlID);
mlsErrorCode_t controlLed(char *arg);
mlsErrorCode_t getLedStatus(char *arg);
//mlsErrorCode_t mlsioBatteryLevel(int *value);
mlsErrorCode_t mlsioCheckGPA5(icamMode_t *icamMode,networkMode_t *networkMode);
void mlsioSetReg(unsigned int address, unsigned int value);
void mlsioGetReg(unsigned int address, unsigned int *value);
int  getHardwareVersion(void);
#endif /* MLSIO_H_ */
