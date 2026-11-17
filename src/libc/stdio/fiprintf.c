#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include <stdarg.h>
#include "libc.h"

/* Fiprintf-style varargs front end: forwards fp, fmt and the argument list to vfiprintf. */
/* 0x001eca98 */
int fiprintf(void *fp, const char *fmt, ...)
{
    int ret;
    va_list ap;

    va_start(ap, fmt);
    ret = vfiprintf(fp, fmt, ap);
    va_end(ap);
    return ret;
}
