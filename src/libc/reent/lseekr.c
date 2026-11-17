#include "types_libc.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"
#include "libkernl_glue.h"

extern int errno;

/*
 * Context wrapper of the lseek stub _lseek: clears the global error errno and copies it into the
 * context when the call fails (-1).
 */
/* steer(cast) */
/* 0x001f58e0 */
long _lseek_r(RtCtx *ptr, int fd, long pos, int whence)
{
    long ret;

    errno = 0;
    if ((ret = ((long (*)(int, long, int))_lseek)(fd, pos, whence)) == -1 && errno != 0)
        ptr->err = errno;
    return ret;
}
