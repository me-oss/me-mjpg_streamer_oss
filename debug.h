#ifndef DEBUG_H_
#define DEBUG_H_
#include <syslog.h>
#undef printf
#define printf(...) { char _bf[1024] = {0}; snprintf(_bf, sizeof(_bf)-1, __VA_ARGS__); syslog(LOG_INFO, "%s", _bf); }
#endif

