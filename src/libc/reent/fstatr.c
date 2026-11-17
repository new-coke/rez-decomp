#include "types_libc.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"
#include "libkernl_glue.h"

extern int errno;

/*
 * Context wrapper of the fstat stub _fstat: clears the global error errno and copies it into the
 * context when the call fails (-1).
 */
/* 0x001f5880 */
int _fstat_r(RtCtx *ptr, int fd, void *st)
{
    int ret;

    errno = 0;
    if ((ret = _fstat(fd, st)) == -1 && errno != 0)
        ptr->err = errno;
    return ret;
}
