#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

extern const _LIB_VERSION_TYPE libm_lib_version;
extern const double expf_huge_val;

/*
 * Calls the core __ieee754_logf and, unless the library version is IEEE, x is NaN or positive,
 * reports log(0) (SING) and log(x < 0) (DOMAIN) as errors named "logf" (result -HUGE for SVID,
 * else -HUGE_VAL) through matherr and errno.
 */
/* HUGE_VAL */
/* 0x001e6188 */
float logf(float x)
{
    float z;
    struct exception exc;

    z = __ieee754_logf(x);
    if (libm_lib_version == _IEEE_ || isnanf(x) || x > (float)0.0)
        return z;
    exc.name = "logf";
    exc.err = 0;
    exc.arg1 = exc.arg2 = (double)x;
    if (libm_lib_version == _SVID_)
        exc.retval = -3.40282346638528859812e+38F;
    else
        exc.retval = -expf_huge_val;
    if (x == (float)0.0) {
        exc.type = 2;
        if (libm_lib_version == _POSIX_)
            *__errno() = 34;
        else if (!matherr(&exc)) {
            *__errno() = 33;
        }
    } else {
        exc.type = 1;
        if (libm_lib_version == _POSIX_)
            *__errno() = 33;
        else if (!matherr(&exc)) {
            *__errno() = 33;
        }
    }
    if (exc.err != 0)
        *__errno() = exc.err;
    return (float)exc.retval;
}
