/*
 * CX93510.h
 *
 *  Created on: Aug 16, 2010
 *      Author: mlslnx003
 */

#ifndef CX93510_H_
#define CX93510_H_

#include "mlsInclude.h"

#define SI_Base	0xA0
#define JC_Base	0x20
#define HI_Base 0x50


typedef enum SensorInterface_enum
{
	SI_SI_CFG_1 = SI_Base,
	SI_SI_CFG_2 = SI_Base + 1,
	SI_SI_CFG_3,
	SI_H_ACT,
	SI_H_CAP_DLY,
	SI_H_CAP_WIDTH,
	SI_V_ACT,
	SI_V_CAP_DLY,
	SI_V_CAP_HEIGHT,
	SI_I2C_DADDR,
	SI_I2C_LO_ADDR,
	SI_I2C_HI_ADDR,
	SI_I2C_LO_DATA,
	SI_I2C_HI_DATA,
	SI_I2C_CTL_1,
	SI_I2C_CTL_2,
	SI_I2C_CTL_3,
	SI_FILT_CFG
}SensorInterface_TypeDef;


#pragma pack(1)
typedef enum JPEGController_enum
{
	JC_DIFF_JPEG_CTRL = JC_Base,
	JC_JPEG_ENC_STAT = JC_Base +1,
	JC_JPEG_ENC_STAT2,
	JC_JPEG_ENC_PVAL_TYPE,
	JC_JPEG_ENC_PVALUE1,
	JC_JPEG_ENC_PVALUE2,
	JC_JPEG_ENC_CTL1,
	JC_JPEG_ENC_DCT_LM,
	JC_JPEG_ENC_DCT_CH,
	JC_JPEG_DEC_STAT1,
	JC_JPEG_DEC_STAT2,
	JC_JPEG_DEC_STAT3
}JPEGController_type;
#pragma pack()


typedef enum HostInterface_enum
{
	HI_SLAVE_SEL_CTRL = HI_Base,
	HI_FB_ADDR_0 = HI_Base +1,
	HI_FB_ADDR_1,
	HI_FB_ADDR_2,
	HI_ERR_STAT,
	HI_HWR_FB_EN,
	HI_HWDATA_FB,
	HI_AUD_BUFF_STAT,
	HI_AUD_RD_PTR_0,
	HI_AUD_RD_PTR_1,
	HI_AUD_QWORDS_0,
	HI_AUD_QWORDS_1,
	HI_ADPCM_N_0,
	HI_ADPCM_N_1,
	HI_ADPCM,
	HI_PHCELL_OUT_0,
	HI_PHCELL_OUT_1,
	HI_HRDDATA_FB  = 0xCC
}HostInterface_t;

#define ADC_CTRL_DIG1 0x0A
#define ADC1 		  0x00
#endif /* CX93510_H_ */
