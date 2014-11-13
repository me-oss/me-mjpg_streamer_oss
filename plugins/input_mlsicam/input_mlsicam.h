#ifndef _INPUT_MLSIcam_H
#define _INPUT_MLSIcam_H
#include "../input.h"
int input_init(input_parameter *param);
int input_stop(int id);
int input_run(int id);
int input_cmd(int id, in_cmd_type cmd, int value);

#endif
