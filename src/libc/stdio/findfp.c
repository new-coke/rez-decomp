#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"
#include "libkernl_glue.h"

extern struct _reent_a *_impure_ptr;

/*
 * Std: initialises a standard stream (flags, fd, cookie = itself, read/write/seek/close hooks
 * __sread/50a0/5120/5188, reent).
 */
/* 0x001f3268 */
void std(FILE_a *ptr, int flags, int file, struct _reent_a *data)
{
    ptr->_p = NULL;
    ptr->_r = 0;
    ptr->_w = 0;
    ptr->_flags = flags;
    ptr->_file = file;
    ptr->_bf._base = NULL;
    ptr->_lbfsize = 0;
    ptr->_cookie = ptr;
    ptr->_read = __sread;
    ptr->_write = __swrite;
    ptr->_seek = __sseek;
    ptr->_close = __sclose;
    ptr->_data = data;
}

/* __sfmoreglue: _malloc_r a glue block followed by n zeroed FILEs (memset memset). */
/* 0x001f32c0 */
struct _glue_a *__sfmoreglue(struct _reent_a *d, int n)
{
    struct _glue_a *g;
    FILE_a *p;

    g = (struct _glue_a *)_malloc_r(d, sizeof(*g) + n * sizeof(FILE_a));
    if (g == NULL)
        return NULL;
    p = (FILE_a *)(g + 1);
    g->_next = NULL;
    g->_niobs = n;
    g->_iobs = p;
    memset(p, 0, n * sizeof(FILE_a));
    return g;
}

/*
 * __sfp: runs __sinit if needed, returns the first free FILE (flags 0) of the glue chain at
 * reent+472, adding blocks of 4 with __sfmoreglue; sets errno 12 (ENOMEM) and returns NULL on
 * failure; resets the found FILE (flags 1, file -1).
 */
/* 0x001f3330 */
FILE_a *__sfp(struct _reent_a *d)
{
    FILE_a *fp;
    int n;
    struct _glue_a *g;
    struct _reent_sinit_a *s = (struct _reent_sinit_a *)d;

    if (!s->__sdidinit)
        __sinit(d);
    for (g = &s->__sglue;; g = g->_next) {
        for (fp = g->_iobs, n = g->_niobs; --n >= 0; fp++)
            if (fp->_flags == 0)
                goto found;
        if (g->_next == NULL && (g->_next = __sfmoreglue(d, 4)) == NULL)
            break;
    }
    s->_errno = 12;
    return NULL;

found:
    fp->_flags = 1;
    fp->_p = NULL;
    fp->_w = 0;
    fp->_r = 0;
    fp->_bf._base = NULL;
    fp->_bf._size = 0;
    fp->_lbfsize = 0;
    fp->_file = -1;
    fp->_ub._base = NULL;
    fp->_ub._size = 0;
    fp->_lb._base = NULL;
    fp->_lb._size = 0;
    fp->_data = d;
    return fp;
}

/* _cleanup_r: walks every stream of the reent with fflush (_fwalk _fwalk, fflush fflush). */
/* 0x001f3410 */
void _cleanup_r(struct _reent_a *ptr)
{
    _fwalk(ptr, fflush);
}

/* _cleanup: _cleanup_r(_impure_ptr _impure_ptr) (_cleanup_r). */
/* 0x001f3420 */
void _cleanup(void)
{
    _cleanup_r(_impure_ptr);
}

/*
 * __sinit: sets __cleanup = _cleanup_r and __sdidinit, opens stdin/stdout/stderr with std (std:
 * flags 4/9/10, fds 0/1/2) and links them as the first glue block.
 */
/* 0x001f3430 */
void __sinit(struct _reent_a *ptr)
{
    struct _reent_sinit_a *s = (struct _reent_sinit_a *)ptr;

    s->__cleanup = _cleanup_r;
    s->__sdidinit = 1;
    std(&s->__sf[0], 4, 0, ptr);
    std(&s->__sf[1], 9, 1, ptr);
    std(&s->__sf[2], 10, 2, ptr);
    s->__sglue._next = NULL;
    s->__sglue._niobs = 3;
    s->__sglue._iobs = &s->__sf[0];
}
