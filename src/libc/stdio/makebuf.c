#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"
#include "libkernl_glue.h"

struct stat_a {
    int st_dev;
    unsigned int st_mode;
    char pad_08[104];
};

struct _reent_cleanup_a {
    char pad_00[60];
    void (*__cleanup)(struct _reent_a *);
};

/*
 * __smakebuf: gives a stream its buffer: the 1-byte _nbuf when unbuffered or malloc fails, else a
 * 1024-byte _malloc_r block (sets __cleanup = _cleanup_r _cleanup_r); uses _fstat_r to mark
 * regular files seekable (__SOPT with __sseek __sseek) and isatty character devices line-buffered.
 */
/* steer(cast) */
/* 0x001f3eb0 */
void __smakebuf(FILE_a *fp)
{
    unsigned int size, couldbetty;
    void *p;
    struct stat_a st;

    if (fp->_flags & 2) {
        fp->_bf._base = fp->_p = fp->_nbuf;
        fp->_bf._size = 1;
        return;
    }
    if (fp->_file < 0 || _fstat_r(fp->_data, fp->_file, &st) < 0) {
        couldbetty = 0;
        size = 1024;
        fp->_flags |= 0x800;
    } else {
        couldbetty = (st.st_mode & 0xf000) == 0x2000;
        size = 1024;
        if ((st.st_mode & 0xf000) == 0x8000 && fp->_seek == (void *)__sseek) {
            fp->_flags |= 0x400;
            fp->_blksize = size;
        } else
            fp->_flags |= 0x800;
    }
    if ((p = _malloc_r(fp->_data, size)) == NULL) {
        fp->_flags |= 2;
        fp->_bf._base = fp->_p = fp->_nbuf;
        fp->_bf._size = 1;
    } else {
        ((struct _reent_cleanup_a *)fp->_data)->__cleanup = _cleanup_r;
        fp->_flags |= 0x80;
        fp->_bf._base = fp->_p = (unsigned char *)p;
        fp->_bf._size = size;
        if (couldbetty && ((int (*)(int))isatty)(fp->_file))
            fp->_flags |= 1;
    }
}
