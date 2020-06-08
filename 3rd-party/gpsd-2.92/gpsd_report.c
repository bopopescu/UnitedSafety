/* $Id: gpsd_report.c 6757 2009-12-07 20:29:57Z mledford $ */
#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include "gpsd.h"


# if __GNUC__ >= 3 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7)
void gpsd_report(int errlevel UNUSED, const char *fmt, ... ) __attribute__ ((weak));
#endif

void gpsd_report(int errlevel UNUSED, const char *fmt, ... )
/* stub logger for clients that don't supply one */
{
    va_list ap;

    va_start(ap, fmt);
    (void)vfprintf(stderr, fmt, ap);
    va_end(ap);
}
