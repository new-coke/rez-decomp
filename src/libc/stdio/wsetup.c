#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

extern struct _reent_a *_impure_ptr;

/*
 * __swsetup: CHECK_INIT (_impure_ptr _impure_ptr, __sinit __sinit); makes a stream writable (fails
 * with -1 unless __SRW), dropping read state and the ungetc buffer (_free_r _free_r), allocates a
 * buffer with __smakebuf and sets _w/_lbfsize for line/unbuffered modes.
 */
/* 0x001f1c48 */
int __swsetup(FILE_a *fp)
{
    do {
        if (fp->_data == NULL)
            fp->_data = _impure_ptr;
        if (!((struct _reent_init_a *)fp->_data)->__sdidinit)
            __sinit(fp->_data);
    } while (0);

    if ((fp->_flags & 8) == 0) {
        if ((fp->_flags & 0x10) == 0)
            return -1;
        if (fp->_flags & 4) {
            if (fp->_ub._base != NULL) {
                if (fp->_ub._base != fp->_ubuf)
                    _free_r(fp->_data, fp->_ub._base);
                fp->_ub._base = NULL;
            }
            fp->_flags &= ~(4 | 0x20);
            fp->_r = 0;
            fp->_p = fp->_bf._base;
        }
        fp->_flags |= 8;
    }
    if (fp->_bf._base == NULL)
        __smakebuf(fp);
    if (fp->_flags & 1) {
        fp->_w = 0;
        fp->_lbfsize = -fp->_bf._size;
    } else
        fp->_w = fp->_flags & 2 ? 0 : fp->_bf._size;
    return 0;
}
