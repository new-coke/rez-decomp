#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"
#include "libkernl_glue.h"

extern int errno;

/*
 * Reentrant sbrk: clears the global errno errno, calls _sbrk and on failure copies a non-zero
 * errno into the reentrancy block.
 */
/* 0x001ed748 */
void *_sbrk_r(struct _reent_a *ptr, int incr)
{
    void *ret;

    errno = 0;
    if ((ret = _sbrk(incr)) == (void *)-1 && errno != 0)
        ptr->_errno = errno;
    return ret;
}
