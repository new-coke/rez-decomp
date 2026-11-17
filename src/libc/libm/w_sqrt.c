#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

extern const _LIB_VERSION_TYPE libm_lib_version;

/*
 * Calls the core __ieee754_sqrt and, unless the library version libm_lib_version is IEEE or x is
 * NaN, reports x < 0 as a DOMAIN error named "sqrt" (result 0 for SVID, else NaN) through matherr
 * and errno (__errno, EDOM 33).
 */
/* 0x001e5c98 */
double sqrt(double x)
{
    double z;
    struct exception exc;

    z = __ieee754_sqrt(x);
    if (libm_lib_version == _IEEE_ || isnan(x))
        return z;
    if (x < 0.0) {
        exc.type = 1;
        exc.name = "sqrt";
        exc.err = 0;
        exc.arg1 = exc.arg2 = x;
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
        return exc.retval;
    } else
        return z;
}
