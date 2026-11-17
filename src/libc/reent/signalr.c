#include "types_libc.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"
#include "libkernl_glue.h"

extern int errno;

/*
 * Context wrapper of the kill stub _kill: clears the global error errno and copies it into the
 * context when the call fails (-1).
 */
/* 0x001f55d8 */
int _kill_r(RtCtx *ptr, int pid, int sig)
{
    int ret;

    errno = 0;
    if ((ret = _kill(pid, sig)) == -1 && errno != 0)
        ptr->err = errno;
    return ret;
}

/* Context wrapper of the process-id stub _getpid. */
/* 0x001f5638 */
int _getpid_r(RtCtx *ptr)
{
    return _getpid();
}
