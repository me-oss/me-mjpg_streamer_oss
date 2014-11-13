/*******************************************************************************
#                                                                              #
#      MJPG-streamer allows to stream JPG frames from an input-plugin          #
#      to several output plugins                                               #
#                                                                              #
#      Copyright (C) 2007 Tom Stöveken                                         #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/
#ifndef INPUT_H_
#define INPUT_H_

#define INPUT_PLUGIN_PREFIX " i: "
#define IPRINT(...) { char _bf[1024] = {0}; snprintf(_bf, sizeof(_bf)-1, __VA_ARGS__); fprintf(stderr, "%s", INPUT_PLUGIN_PREFIX); fprintf(stderr, "%s", _bf); syslog(LOG_INFO, "%s", _bf); }

/* parameters for input plugin */
typedef struct _input_parameter input_parameter;
struct _input_parameter {
  char *parameter_string;
  struct _globals *global;
};

/* commands which can be send to the input plugin */
typedef enum _in_cmd_type in_cmd_type;
enum _in_cmd_type {
  IN_CMD_UNKNOWN = 0,
  IN_CMD_HELLO,
  IN_CMD_RESET,
  IN_CMD_RESET_PAN_TILT,
  IN_CMD_RESET_PAN_TILT_NO_MUTEX,
  IN_CMD_PAN_SET,
  IN_CMD_PAN_PLUS,
  IN_CMD_PAN_MINUS,
  IN_CMD_TILT_SET,
  IN_CMD_TILT_PLUS,
  IN_CMD_TILT_MINUS,
  IN_CMD_SATURATION_PLUS,
  IN_CMD_SATURATION_MINUS,
  IN_CMD_CONTRAST_PLUS,
  IN_CMD_CONTRAST_MINUS,
  IN_CMD_BRIGHTNESS_PLUS,
  IN_CMD_BRIGHTNESS_MINUS,
  IN_CMD_GAIN_PLUS,
  IN_CMD_GAIN_MINUS,
  IN_CMD_FOCUS_PLUS,
  IN_CMD_FOCUS_MINUS,
  IN_CMD_FOCUS_SET,
  IN_CMD_LED_ON,
  IN_CMD_LED_OFF,
  IN_CMD_LED_AUTO,
  IN_CMD_LED_BLINK,

  //mls@dev03  100917
  IN_CMD_VALUE_CONTRACT,
  IN_CMD_VALUE_BRIGHTNESS,
  IN_CMD_SET_CONTRAST,
  IN_CMD_SET_BRIGHTNESS,

  //mls@dev03 100923
  IN_CMD_RESOLUTION_VGA,
  IN_CMD_RESOLUTION_QVGA,
  IN_CMD_RESOLUTION_QQVGA,
  IN_CMD_COMPRESSION_STD,
  IN_CMD_COMPRESSION_HIGH,

  //mls$dev06 110124
  IN_CMD_BOOT_TO_UPGRADE,

  //mls@dev03 101012
  IN_CMD_CONFIG_WIFI_SAVE,
  IN_CMD_CONFIG_WIFI_READ,
  IN_CMD_CONFIG_WIFI_READ_PORT,
  IN_CMD_CONFIG_HTTP_SAVE_USR_PW,

  //mls@dev04 100924
  IN_CMD_VALUE_RESOLUTION,
  IN_CMD_VALUE_COMPRESSION,

  //mls@dev04 101012
  IN_CMD_MICRO_BOOSTGAIN_PLUS,
  IN_CMD_MICRO_BOOSTGAIN_MINUS,
  IN_CMD_MICRO_BOOSTGAIN_VALUE,

  IN_CMD_MICRO_DIGITALGAIN_PLUS,
  IN_CMD_MICRO_DIGITALGAIN_MINUS,
  IN_CMD_MICRO_DIGITALGAIN_VALUE,

  //mls@dev03 101111
  IN_CMD_GET_STORE_FOLDER,
  IN_CMD_SET_STORE_FOLDER,

  //mls@dev03 20110729
  IN_CMD_MOV_FORWARD,
  IN_CMD_MOV_BACKWARD,
  IN_CMD_MOV_LEFT,
  IN_CMD_MOV_RIGHT,
  IN_CMD_STOP_FB,
  IN_CMD_STOP_LR,
  IN_CMD_MOV_STOP,
  IN_CMD_MOV_FORWARD_CONT,
  IN_CMD_MOV_BACKWARD_CONT,
  IN_CMD_MOV_LEFT_CONT,
  IN_CMD_MOV_RIGHT_CONT,

  //mlsdev006 20110804
  IN_CMD_LED_EAR_ON,
  IN_CMD_LED_EAR_OFF,
  IN_CMD_LED_EAR_BLINK,
  IN_CMD_LED_SETUPLED_VALUE,

  //mls@dev03 20110805 for mic on device
  IN_CMD_MICDEV_OFF,
  IN_CMD_MICDEV_ON,

  //mls@dev03 20110812 for wifi signal strength and battery level
  IN_CMD_WIFISIGNAL_STRENGTH,
  IN_CMD_BATTERY_LEVEL,

  //mls@dev03 20110904 restart system
  IN_CMD_RESTART_SYSTEM,
  //mls@dev03 20111130 restart application
  IN_CMD_RESTART_APP,

  //mls@dev03 20110921 melody playback
  IN_CMD_MELODY1_PLAY,
  IN_CMD_MELODY2_PLAY,
  IN_CMD_MELODY3_PLAY,
  IN_CMD_MELODY4_PLAY,
  IN_CMD_MELODY5_PLAY,
  IN_CMD_MELODY_STOP,
  IN_CMD_MELODY_INDEX,

  //mls@dev03 20110927 temperature sensor
  IN_CMD_TEMPERATURE,

  //mls@dev03 20111010 reset factory mode
  IN_CMD_RESET_FACTORY,
  IN_CMD_SWICH_TO_UAP,

  //mls@dev03 20111014 uAP read and write
  IN_CMD_UAPCONFIG_READ,
  IN_CMD_UAPCONFIG_SAVE,
  //mlsdev008 20111110 vox command
  IN_CMD_VOX_GET_THRESHOLD,
  IN_CMD_VOX_SET_THRESHOLD,
  IN_CMD_VOX_ENABLE,
  IN_CMD_VOX_DISABLE,
  IN_CMD_VOX_GET_STATUS,
  IN_CMD_GET_VERSION,
  IN_CMD_GET_DEFAULT_VERSION,
  //mlsdev008 8/12/2011
  IN_CMD_VIDEO_FLIPUP,
  //mlsdev008 17/12/2011
  IN_CMD_CAMERA_NAME,
//mls@dev03 20111214
  IN_CMD_SPK_VOLUME,
  IN_CMD_GET_SPK_VOLUME,
  // JohnLe added
  IN_CMD_SERVER_IS_CAM_READY,
  IN_CMD_SERVER_SET_RANDOM_NUMBER,
  IN_CMD_SERVER_SET_RANDOM_NUMBER2,
  IN_CMD_SERVER_GET_SESSION_KEY,
  IN_CMD_SET_MASTER_KEY,
  IN_CMD_GET_UPNP_STATUS,
  IN_CMD_RESET_UPNP,
  IN_CMD_SET_UPNP_PORT,
  IN_CMD_GET_UPNP_PORT,
  IN_CMD_GET_REG,
  IN_CMD_SET_REG,
  IN_CMD_GET_LOG,
  IN_CMD_SET_LOG_LEVEL,
  IN_CMD_ENABLE_PCM_LOG,
  IN_CMD_DISABLE_PCM_LOG,
  IN_CMD_SET_AUDIO_FINETUNE,
  IN_CMD_GET_AUDIO_FINETUNE,
  IN_CMD_SET_SENSOR_REG,
  IN_CMD_GET_SENSOR_REG,
  IN_CMD_GET_SESSION_KEY,
  IN_CMD_SET_DELAY_OUTPUT,
  IN_CMD_GET_HW_VERSION,
  IN_CMD_TAKE_SNAPSHOT,
  IN_CMD_GET_ROUTERS_LIST,
  IN_CMD_CHECK_FW_UPGRADE,
  IN_CMD_REQUEST_FW_UPGRADE,
  IN_CMD_GET_MAC_ADDRESS,
  IN_CMD_GET_MAC_ADDRESS_IN_FLASH,
  IN_CMD_SET_MAC_ADDRESS_IN_FLASH,
  IN_CMD_SET_TEMP_ALERT,
  IN_CMD_DISABLE_VIDEO_IN_UAP,
  IN_CMD_SET_INT_INTERNET_CONNECTED,
  IN_CMD_GET_DEBUG_VAL1,
  IN_CMD_SET_DEBUG_VAL1,
  IN_CMD_SET_TEMP_OFFSET,
  IN_CMD_GET_TEMP_OFFSET,
  IN_CMD_SERVER_SET_REMOTE_IP,
  IN_CMD_GET_CODECS_SUPPORT,
};

/* structure to store variables/functions for input plugin */
typedef struct _input input;
struct _input {
  char *plugin;
  void *handle;
  input_parameter param;

  int (*init)(input_parameter *);
  int (*stop)(void);
  int (*run)(void);
  int (*cmd)(in_cmd_type, char*);
};

#endif
