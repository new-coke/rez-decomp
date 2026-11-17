#include "types_libc.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"
#include "libkernl_glue.h"

extern int errno;

/*
 * Context wrapper of the close stub _close: clears the global error errno and copies it into the
 * context when the call fails (-1).
 */
/* steer(cast) */
/* 0x001f5828 */
int _close_r(RtCtx *ptr, int fd)
{
    int ret;

    errno = 0;
    if ((ret = ((int (*)(int))_close)(fd)) == -1 && errno != 0)
        ptr->err = errno;
    return ret;
}
