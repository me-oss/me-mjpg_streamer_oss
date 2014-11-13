#ifndef _OUTPUT_HTTP_H
#define _OUTPUT_HTTP_H
#include "../output.h"
int output_init(output_parameter *param);
int output_stop(int id);
int output_run(int id);
int output_cmd(int id, out_cmd_type cmd, int value);

#endif
