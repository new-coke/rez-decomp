#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"
#include "libkernl_glue.h"

extern struct _reent_a *_impure_ptr;

/*
 * Fflush: NULL flushes all streams via _fwalk; otherwise CHECK_INIT (_impure_ptr _impure_ptr,
 * __sinit __sinit), then writes the pending buffer through fp->_write, setting __SERR (0x40) and
 * returning -1 on error.
 */
/* 0x001f3160 */
int fflush(FILE_a *fp)
{
    unsigned char *p;
    int n, t;

    if (fp == NULL)
        return _fwalk(_impure_ptr, fflush);

    do {
        if (fp->_data == NULL)
            fp->_data = _impure_ptr;
        if (!((struct _reent_init_a *)fp->_data)->__sdidinit)
            __sinit(fp->_data);
    } while (0);

    t = fp->_flags;
    if ((t & 8) == 0 || (p = fp->_bf._base) == NULL)
        return 0;
    n = fp->_p - p;
    fp->_p = p;
    fp->_w = t & (1 | 2) ? 0 : fp->_bf._size;
    while (n > 0) {
        t = ((int (*)(void *, char *, int))fp->_write)(fp->_cookie, (char *)p, n);
        if (t <= 0) {
            fp->_flags |= 0x40;
            return -1;
        }
        p += t;
        n -= t;
    }
    return 0;
}
