#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include <stdarg.h>
#include "libc.h"

extern struct _reent_a *_impure_ptr;

/*
 * Vsprintf: formats into str through a string FILE (flags 0x208, unbounded size, _impure_ptr
 * _impure_ptr) with vfprintf, then terminates it.
 */
/* 0x001f1bf0 */
int vsprintf(char *str, const char *fmt, va_list ap)
{
    int ret;
    FILE_a f;

    f._flags = 0x208;
    f._bf._base = f._p = (unsigned char *)str;
    f._bf._size = f._w = 0x7fffffff;
    f._data = _impure_ptr;
    ret = vfprintf(&f, fmt, ap);
    *f._p = 0;
    return ret;
}
