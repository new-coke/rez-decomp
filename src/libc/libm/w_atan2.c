#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

extern const _LIB_VERSION_TYPE libm_lib_version;

/*
 * Calls the core __ieee754_atan2 and, unless the library version libm_lib_version is IEEE or an
 * argument is NaN (isnan), reports atan2(+-0, +-0) as a DOMAIN error named "atan2" through matherr
 * and errno (__errno, EDOM 33).
 */
/* 0x001e5b80 */
double atan2(double y, double x)
{
    double z;
    struct exception exc;

    z = __ieee754_atan2(y, x);
    if (libm_lib_version == _IEEE_ || isnan(x) || isnan(y))
        return z;
    if (x == 0.0 && y == 0.0) {
        exc.arg1 = y;
        exc.arg2 = x;
        exc.type = 1;
        exc.name = "atan2";
        exc.err = 0;
        exc.retval = 0.0;
        if (libm_lib_version == _POSIX_)
            *__errno() = 33;
        else if (!matherr(&exc)) {
            *__errno() = 33;
        }
        if (exc.err != 0)
            *__errno() = exc.err;
        return exc.retval;
    } else
        return z;
}
