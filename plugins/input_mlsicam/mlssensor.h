/*
 * mlssensor.h
 *
 *  Created on: Jul 8, 2011
 *      Author: root
 */

#ifndef MLSSENSOR_H_
#define MLSSENSOR_H_

#include "mlsInclude.h"

//#include <asm-arm/types.h>
//#include <linux/types.h>

///////////////////////////////////////////////////////////////////////////////
//mls@dev03
#define VIDEOIN_DEVICE "/dev/video0"

#define LCM_WIDTH			320//640//320
#define LCM_HEIGHT			240//480//240
#define LCM_BPP				16		//16 bits/pixel

#define FALSE		0
#define TRUE		1

///////////////////////////////////////////////////////////////////////////////

#define	CONFIG_JPEG

#define CONFIG_VIDEOIN_BUFFER_COUNT				1
//#define CONFIG_VIDEOIN_VPOST_OVERLAY_BUFFER

#define CONFIG_VIDEOIN_PREVIEW_BUFFER_SIZE  (LCDWIDTH*LCDHEIGHT*LCDBPP/8)

#define OUTPUT_2_IDLE	1
#define OUTPUT_2_LCD		2
#define OUTPUT_2_JPEG	3

//shot mode
#define CONTINUOUS		1//continue to shot
#define ONESHOT			2//one shot
#define CON_NUMBER		3//continue shot the number of jpegs

//parameters for set/get
typedef struct videoin_param{
	__u32	vaddr;
	__u32	paddr;
	struct{
		__u32	input_format;			/* VideoIn input format from Sensor  */
		__u32	output_format;		/* VideoIn output format to Panel */
	}format;
	struct{
		__u32 x;						/* Preview resolution */
		__u32 y;
	}resolution;
	struct{
		__u32 bVsync;       // TRUE: High Active, FALSE: Low Active
		__u32 bHsync;       // TRUE: High Active, FALSE: Low Active
		__u32 bPixelClk;     // TRUE: Falling Edge, FALSE: Rising Edge;
	}polarity;
}videoin_param_t;

typedef struct videoin_viewwindow{
	struct{
		__u32	u32ViewWidth;		//Packet Width
		__u32	u32ViewHeight;		//Packet Height
	}ViewWindow;
	struct{
		__u32	u32PosX;			// Packet output start position to pannel
		__u32	u32PosY;
	}ViewStartPos;
}videoin_window_t;

typedef struct videoin_encode{
		__u32	u32Width;		//Planar Width
		__u32	u32Height;		//Planar Height
		__u32	u32Format;		//Planar YUV422 or YUV420
		__u32	u32Enable;		//Planar pipe enable?
}videoin_encode_t;

//information for get
typedef struct videoin_info{
    __u32	bufferend;		/*video in buffer end for round-robin[0, CONFIG_VIDEOIN_BUFFER_COUNT)*/
}videoin_info_t;

#define VIDEOIN_PREVIEW_PIPE_START			_IOW('v',130, struct videoin_param)
#define VIDEOIN_S_PARAM					_IOW('v',131, struct videoin_param)
//#define VIDEOIN_G_PARAM					_IOR('v',132, struct videoin_param)
#define VIDEOIN_STATE						_IOR('v',133, struct videoin_param)
//#define VIDEOIN_G_INFO					_IOR('v',134, struct videoin_info)
#define VIDEOIN_S_RESOLUTION				_IOW('v',135, struct videoin_info)
#define VIDEOIN_PREVIEW_PIPE_CTL			_IOW('v',136, __u32)
#define VIDEOIN_ENCODE_PIPE_CTL			_IOW('v',137, __u32)
#define VIDEOIN_SELECT_FRAME_BUFFER		_IOW('v',138, __u32)
#define VIDEOIN_S_VIEW_WINDOW			_IOW('v',139, struct videoin_viewwindow)
#define VIDEOIN_S_JPG_PARAM				_IOW('v',140, struct videoin_encode)

//mls@dev03 define ioctl to set contrast and brightness for OV76xx
#define VIDEOIN_SENSOROV76xx_SETCONTRAST _IOW('v',141, __u32)
#define VIDEOIN_SENSOROV76xx_SETBRIGHTNESS _IOW('v',142, __u32)
#define VIDEOIN_SENSOROV76xx_CHECKPID _IOR('v',143, __u32)
//mlsdev008 define ioctl to implement flip up
#define VIDEOIN_SENSOROV76xx_FLIPUP	_IOW('v',144,__u32)
#define VIDEOIN_SENSOROV76xx_IRLED	_IOW('v',145,__u32)
#define VIDEOIN_SENSOROV76xx_SETREG	_IOW('v',146,__u32)
#define VIDEOIN_SENSOROV76xx_GETREG	_IOW('v',147,__u32)
#define VIDEOIN_SENSOROV76xx_CHECKFLIPUP	_IOW('v',148,__u32)

typedef __s32 (*set_engine)(void *dev_id, void* sbuffer);
typedef __s32 (*get_status)(void *dev_id);
typedef __s32 (*set_status)(void *dev_id, __s32);
typedef __s8 (*is_finished)(void *dev_id);

typedef struct videoin_output_engine_ops{
	set_engine		setengine;
	get_status		getstatus;//for videoin to get jpeg status
	set_status		setstatus;
	is_finished		isfinished;
}vout_ops_t;

/*
 *
 */
mlsErrorCode_t mlssensorInit();

/*
 *
 */
mlsErrorCode_t mlssensorDeInit();

/*
 *
 */
//mlsErrorCode_t mlssensorGetImage();

/*
 *
 */
mlsErrorCode_t mlssensorSetContrast(char value);

/*
 *
 */
mlsErrorCode_t mlssensorSetBrightness(char value);

/*
 *
 */
mlsErrorCode_t mlssensorCheckPID();
/*
 *
 */
mlsErrorCode_t mlssensorFlipup();

mlsErrorCode_t mlssensorIRLED(int onoff);
UInt8 mlssensor_is_flipup_mirror(void);
mlsErrorCode_t mlssensorGetReg(char address, char* value);
mlsErrorCode_t mlssensorSetReg(char address,char value);
mlsErrorCode_t mlssensorSetResolution(int image_width, int image_height);
#endif /* MLSSENSOR_H_ */
