#include "types_libc.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

/*
 * Stream read callback: reads through the context wrapper _read_r, advances the cached offset on
 * success, clears flag 0x1000 on error.
 */
/* 0x001f5658 */
int __sread(void *cookie, char *buf, int n)
{
    Stream *fp = (Stream *)cookie;
    int ret;

    ret = _read_r(fp->ctx, fp->file, buf, n);
    if (ret >= 0)
        fp->offset += ret;
    else
        fp->flags &= ~0x1000;
    return ret;
}

/*
 * Stream write callback: seeks to the end first in append mode (flag 0x100, _lseek_r), clears flag
 * 0x1000, writes through _write_r.
 */
/* 0x001f56c0 */
int __swrite(void *cookie, const char *buf, int n)
{
    Stream *fp = (Stream *)cookie;

    if (fp->flags & 0x100)
        (void)_lseek_r(fp->ctx, fp->file, 0, 2);
    fp->flags &= ~0x1000;
    return _write_r(fp->ctx, fp->file, buf, n);
}

/*
 * Stream seek callback: seeks through _lseek_r and caches the offset (flag 0x1000) or clears the
 * flag on failure.
 */
/* 0x001f5740 */
long __sseek(void *cookie, long offset, int whence)
{
    Stream *fp = (Stream *)cookie;
    long ret;

    ret = _lseek_r(fp->ctx, fp->file, offset, whence);
    if (ret == -1L)
        fp->flags &= ~0x1000;
    else {
        fp->flags |= 0x1000;
        fp->offset = ret;
    }
    return ret;
}

/* Stream close callback: closes the descriptor through _close_r. */
/* 0x001f57a8 */
int __sclose(void *cookie)
{
    Stream *fp = (Stream *)cookie;

    return _close_r(fp->ctx, fp->file);
}
