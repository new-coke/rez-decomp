#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include <stdarg.h>
#include "libc.h"
#include "libc_internal.h"

extern struct _reent_a *_impure_ptr;

/*
 * Reentrant sprintf: formats into str through a string FILE (flags 0x208, unbounded) with vfprintf
 * and terminates it.
 */
/* 0x001ed7a8 */
int _sprintf_r(struct _reent_a *ptr, char *str, const char *fmt, ...)
{
    int ret;
    va_list ap;
    FILE_a f;

    f._flags = 0x208;
    f._bf._base = f._p = (unsigned char *)str;
    f._bf._size = f._w = 0x7fffffff;
    f._data = ptr;
    va_start(ap, fmt);
    ret = vfprintf(&f, fmt, ap);
    va_end(ap);
    *f._p = 0;
    return ret;
}

/*
 * Sprintf: formats into str through a string FILE (flags 0x208, unbounded, reentrancy block
 * _impure_ptr) with vfprintf and terminates it.
 */
/* 0x001ed810 */
int sprintf(char *str, const char *fmt, ...)
{
    int ret;
    va_list ap;
    FILE_a f;

    f._flags = 0x208;
    f._bf._base = f._p = (unsigned char *)str;
    f._bf._size = f._w = 0x7fffffff;
    f._data = _impure_ptr;
    va_start(ap, fmt);
    ret = vfprintf(&f, fmt, ap);
    va_end(ap);
    *f._p = 0;
    return ret;
}
