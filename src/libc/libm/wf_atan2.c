#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

extern const _LIB_VERSION_TYPE libm_lib_version;

/*
 * Calls the core __ieee754_atan2f and, unless the library version is IEEE or an argument is NaN
 * (isnanf), reports atan2f(+-0, +-0) as a DOMAIN error named "atan2f" through matherr and errno
 * (EDOM 33).
 */
/* 0x001e5da8 */
float atan2f(float y, float x)
{
    float z;
    struct exception exc;

    z = __ieee754_atan2f(y, x);
    if (libm_lib_version == _IEEE_ || isnanf(x) || isnanf(y))
        return z;
    if (x == (float)0.0 && y == (float)0.0) {
        exc.arg1 = (double)y;
        exc.arg2 = (double)x;
        exc.err = 0;
        exc.type = 1;
        exc.name = "atan2f";
        exc.retval = 0.0;
        if (libm_lib_version == _POSIX_)
            *__errno() = 33;
        else if (!matherr(&exc)) {
            *__errno() = 33;
        }
        if (exc.err != 0)
            *__errno() = exc.err;
        return (float)exc.retval;
    } else
        return z;
}
