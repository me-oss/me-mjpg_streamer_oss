#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>

#include "debuglog.h"
void startlog(char* name)
{
    openlog(name,LOG_PID|LOG_CONS|LOG_NDELAY,LOG_USER);
}
void printlog(const char* format, ...)
{
    va_list arg;
    va_start(arg,format);
    vsyslog(LOG_INFO,format,arg);
    va_end(arg);
}
