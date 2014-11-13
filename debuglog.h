#ifndef DEBUGLOG_H_
#define DEBUGLOG_H_

#include <syslog.h>

void startlog(char* name);
void printlog(const char* format, ...);
#undef printf
#define printf printlog

#endif /* MLSCRC_H_ */
