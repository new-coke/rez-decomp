#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"
#include "libkernl_glue.h"

#define MIN_a(a, b) ((a) < (b) ? (a) : (b))
#define COPY_a(n) (void)memmove((void *)fp->_p, (void *)p, (unsigned int)(n))

#define GETIOV_a(extra_work) \
    while (len == 0) {       \
        extra_work;          \
        p = iov->iov_base;   \
        len = iov->iov_len;  \
        iov++;               \
    }

/*
 * __sfvwrite: writes a uio vector to a stream (after __swsetup __swsetup): unbuffered in chunks of
 * at most 1024 through fp->_write, fully buffered (string streams copy and pretend all was written)
 * or line buffered (memchr memchr for the newline), copying with memmove and flushing with fflush;
 * sets __SERR and returns -1 on error.
 */
/* 0x001f3950 */
int __sfvwrite(FILE_a *fp, struct __suio_a *uio)
{
    unsigned int len;
    const char *p;
    struct __siov_a *iov;
    int w, s;
    char *nl;
    int nlknown, nldist;

    if ((len = uio->uio_resid) == 0)
        return 0;

    if (((fp->_flags & 8) == 0 || fp->_bf._base == NULL) && __swsetup(fp))
        return -1;

    iov = uio->uio_iov;
    len = 0;
    if (fp->_flags & 2) {
        do {
            GETIOV_a(;);
            w = ((int (*)(void *, const char *, int))fp->_write)(fp->_cookie, p, MIN_a(len, 1024));
            if (w <= 0)
                goto err;
            p += w;
            len -= w;
        } while ((uio->uio_resid -= w) != 0);
    } else if ((fp->_flags & 1) == 0) {
        do {
            GETIOV_a(;);
            w = fp->_w;
            if (fp->_flags & 0x200) {
                if (len < w)
                    w = len;
                COPY_a(w);
                fp->_w -= w;
                fp->_p += w;
                w = len;
            } else if (fp->_p > fp->_bf._base && len > w) {
                COPY_a(w);
                fp->_p += w;
                if (fflush(fp))
                    goto err;
            } else if (len >= (w = fp->_bf._size)) {
                w = ((int (*)(void *, const char *, int))fp->_write)(fp->_cookie, p, w);
                if (w <= 0)
                    goto err;
            } else {
                w = len;
                COPY_a(w);
                fp->_w -= w;
                fp->_p += w;
            }
            p += w;
            len -= w;
        } while ((uio->uio_resid -= w) != 0);
    } else {
        nlknown = 0;
        do {
            GETIOV_a(nlknown = 0);
            if (!nlknown) {
                nl = memchr((void *)p, '\n', len);
                nldist = nl ? nl + 1 - p : len + 1;
                nlknown = 1;
            }
            s = MIN_a(len, nldist);
            w = fp->_w + fp->_bf._size;
            if (fp->_p > fp->_bf._base && s > w) {
                COPY_a(w);
                fp->_p += w;
                if (fflush(fp))
                    goto err;
            } else if (s >= (w = fp->_bf._size)) {
                w = ((int (*)(void *, const char *, int))fp->_write)(fp->_cookie, p, w);
                if (w <= 0)
                    goto err;
            } else {
                w = s;
                COPY_a(w);
                fp->_w -= w;
                fp->_p += w;
            }
            if ((nldist -= w) == 0) {
                if (fflush(fp))
                    goto err;
                nlknown = 0;
            }
            p += w;
            len -= w;
        } while ((uio->uio_resid -= w) != 0);
    }
    return 0;

err:
    fp->_flags |= 0x40;
    return -1;
}
