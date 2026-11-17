/* Debug logging: level-masked printf and the assertion handler. */

#include <stdarg.h>
#include "libc.h"
#include "ps_log.h"

extern int psLogMask;

/* Returns the previous mask. */
/* 0x001b4e10 */
static int psLogSetMask(int mask)
{
    int old;

    old = psLogMask;
    psLogMask = mask;
    return old;
}

/* Up to 1024 bytes; only when level has a bit in psLogMask. */
/* 0x001b4e20 */
void psLogPrintf(int level, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    if (level & psLogMask)
        printf(buf);
}

/* Prints file and line, then crashes on purpose by writing to address 0. */
/* 0x001b4e80 */
void psAssertFail(const char *file, int line)
{
    printf("ASSERTION failed: file \"%s\", line:%d\n", file, line);
    *(int *)0 = 0;
}
