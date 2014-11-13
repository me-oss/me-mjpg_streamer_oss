/**********************************************************************************
 *  (C) Copyright 2009 Molisys Solutions Co., Ltd. , All rights reserved          *
 *                                                                                *
 *  This source code and any compilation or derivative thereof is the sole        *
 *  property of Molisys Solutions Co., Ltd. and is provided pursuant to a         *
 *  Software License Agreement.  This code is the proprietary information         *
 *  of Molisys Solutions Co., Ltd and is confidential in nature.  Its use and     *
 *  dissemination by any party other than Molisys Solutions Co., Ltd is           *
 *  strictly limited by the confidential information provisions of the            *
 *  Agreement referenced above.                                                   *
 **********************************************************************************/

#ifndef __MLSERRORS_H__
#define __MLSERRORS_H__

#include "mlsTypes.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef Int32 mlsErrorCode_t;

/*---------------------------------------------------
 * Common error codes
 * --------------------------------------------------*/
#define MLS_SUCCESS                    		0 
#define MLS_ERROR				
#define MLS_ERR_OPENFILE					-2
#define MLS_ERR_INVALID_PARAMETER   		-3 

#define MLS_ERR_MOTOR_BOUNDARY				-100 

//
#define MLS_ERR_OV_PID						-200 

#define MLS_ERR_Icam_MUTEX	 				-300 
#define MLS_ERR_Icam_OPEN_GPIODEV			-301 

#define MLS_ERR_WIFI_NOROUTER				-400 
#define MLS_ERR_WIFI_NOTCONNECTIVITY		-401 
#define MLS_ERR_WIFI_WRONGCRC				-402 
#define MLS_ERR_WIFI_OPENFILE				-403 
#define MLS_ERR_WIFI_DHCPIP 				-404 
#define MLS_ERR_WIFI_NOINTERFACE			-405 


#define MLS_ERR_OTA_BASE					-500
#define MLS_ERR_OTA_WRONGCRC				-501 
#define MLS_ERR_OTA_WRONGMD5				-502 


#ifdef __cplusplus
}
#endif

#endif /*__MLSERRORS_H__*/


