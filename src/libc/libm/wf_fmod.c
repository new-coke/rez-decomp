#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

extern const _LIB_VERSION_TYPE libm_lib_version;

/*
 * Calls the core __ieee754_fmodf and, unless the library version is IEEE or an argument is NaN,
 * reports y == 0 as a DOMAIN error named "fmodf" (result x for SVID, else NaN) through matherr and
 * errno (EDOM 33).
 */
/* 0x001e6048 */
float fmodf(float x, float y)
{
    float z;
    struct exception exc;

    z = __ieee754_fmodf(x, y);
    if (libm_lib_version == _IEEE_ || isnanf(y) || isnanf(x))
        return z;
    if (y == (float)0.0) {
        exc.type = 1;
        exc.name = "fmodf";
        exc.err = 0;
        exc.arg1 = (double)x;
        exc.arg2 = (double)y;
        if (libm_lib_version == _SVID_)
            exc.retval = (double)x;
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
    } else
        return z;
}
