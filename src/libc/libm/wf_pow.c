#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

extern const _LIB_VERSION_TYPE libm_lib_version;
extern const double expf_huge_val;

/*
 * Calls the core __ieee754_powf and, unless the library version is IEEE or y is NaN, reports the
 * pow special cases (NaN**0, 0**0, 0**negative, negative**non-integer, overflow, underflow) as
 * errors named "powf" through matherr and errno (__errno).
 */
/* HUGE_VAL */
/* 0x001e62d0 */
float powf(float x, float y)
{
    float z;
    struct exception exc;

    z = __ieee754_powf(x, y);
    if (libm_lib_version == _IEEE_ || isnanf(y))
        return z;
    if (isnanf(x)) {
        if (y == (float)0.0) {
            exc.type = 1;
            exc.name = "powf";
            exc.err = 0;
            exc.arg1 = (double)x;
            exc.arg2 = (double)y;
            exc.retval = x;
            if (libm_lib_version == _IEEE_ || libm_lib_version == _POSIX_)
                exc.retval = 1.0;
            else if (!matherr(&exc)) {
                *__errno() = 33;
            }
            if (exc.err != 0)
                *__errno() = exc.err;
            return (float)exc.retval;
        } else
            return z;
    }
    if (x == (float)0.0) {
        if (y == (float)0.0) {
            exc.type = 1;
            exc.name = "powf";
            exc.err = 0;
            exc.arg1 = (double)x;
            exc.arg2 = (double)y;
            exc.retval = 0.0;
            if (libm_lib_version != _SVID_)
                exc.retval = 1.0;
            else if (!matherr(&exc)) {
                *__errno() = 33;
            }
            if (exc.err != 0)
                *__errno() = exc.err;
            return (float)exc.retval;
        }
        if (finitef(y) && y < (float)0.0) {
            exc.type = 1;
            exc.name = "powf";
            exc.err = 0;
            exc.arg1 = (double)x;
            exc.arg2 = (double)y;
            if (libm_lib_version == _SVID_)
                exc.retval = 0.0;
            else
                exc.retval = -expf_huge_val;
            if (libm_lib_version == _POSIX_)
                *__errno() = 33;
            else if (!matherr(&exc)) {
                *__errno() = 33;
            }
            if (exc.err != 0)
                *__errno() = exc.err;
            return (float)exc.retval;
        }
        return z;
    }
    if (!finitef(z)) {
        if (finitef(x) && finitef(y)) {
            if (isnanf(z)) {
                exc.type = 1;
                exc.name = "powf";
                exc.err = 0;
                exc.arg1 = (double)x;
                exc.arg2 = (double)y;
                if (libm_lib_version == _SVID_)
                    exc.retval = 0.0;
                else
                    exc.retval = 0.0 / 0.0;
                if (libm_lib_version == _POSIX_)
                    *__errno() = 33;
                else if (!matherr(&exc)) {
                    *__errno() = 33;
                }
                if (exc.err != 0)
                    *__errno() = exc.err;
                return (float)exc.retval;
            } else {
                exc.type = 3;
                exc.name = "powf";
                exc.err = 0;
                exc.arg1 = (double)x;
                exc.arg2 = (double)y;
                if (libm_lib_version == _SVID_) {
                    exc.retval = 3.40282346638528859812e+38F;
                    y *= 0.5;
                    if (x < 0.0 && rint(y) != y)
                        exc.retval = -3.40282346638528859812e+38F;
                } else {
                    exc.retval = expf_huge_val;
                    y *= 0.5;
                    if (x < 0.0 && rint(y) != y)
                        exc.retval = -expf_huge_val;
                }
                if (libm_lib_version == _POSIX_)
                    *__errno() = 34;
                else if (!matherr(&exc)) {
                    *__errno() = 34;
                }
                if (exc.err != 0)
                    *__errno() = exc.err;
                return (float)exc.retval;
            }
        }
    }
    if (z == (float)0.0 && finitef(x) && finitef(y)) {
        exc.type = 4;
        exc.name = "powf";
        exc.err = 0;
        exc.arg1 = (double)x;
        exc.arg2 = (double)y;
        exc.retval = 0.0;
        if (libm_lib_version == _POSIX_)
            *__errno() = 34;
        else if (!matherr(&exc)) {
            *__errno() = 34;
        }
        if (exc.err != 0)
            *__errno() = exc.err;
        return (float)exc.retval;
    }
    return z;
}
