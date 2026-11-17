#include "types_libc.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"
#include "libkernl_glue.h"

extern int errno;

/*
 * Context wrapper of the write stub _write: clears the global error errno and copies it into the
 * context when the call fails (-1).
 */
/* 0x001f57c8 */
long _write_r(RtCtx *ptr, int fd, const void *buf, unsigned int n)
{
    long ret;

    errno = 0;
    if ((ret = _write(fd, buf, n)) == -1 && errno != 0)
        ptr->err = errno;
    return ret;
}
