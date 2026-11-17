#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include <stdarg.h>
#include "libc.h"
#include "libc_internal.h"

extern struct _reent_a *_impure_ptr;

/*
 * Reentrant printf: formats to the stdout of the given reentrancy block through _vfprintf_r
 * (vfprintf core).
 */
/* 0x001ed6b8 */
int _printf_r(struct _reent_a *r, const char *fmt, ...)
{
    int ret;
    va_list ap;

    va_start(ap, fmt);
    ret = _vfprintf_r(r, r->_stdout, fmt, ap);
    va_end(ap);
    return ret;
}

/*
 * Printf: points stdout's reentrancy block at _impure_ptr and formats to stdout through vfprintf.
 */
/* 0x001ed6f8 */
int printf(const char *fmt, ...)
{
    int ret;
    va_list ap;

    va_start(ap, fmt);
    ((FILE_a *)_impure_ptr->_stdout)->_data = _impure_ptr;
    ret = vfprintf((FILE_a *)_impure_ptr->_stdout, fmt, ap);
    va_end(ap);
    return ret;
}
