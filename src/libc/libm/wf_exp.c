#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

extern const _LIB_VERSION_TYPE libm_lib_version;
extern const double expf_huge_val;

/* HUGE_VAL */
static const float o_threshold = 8.8721679688e+01;

static const float u_threshold = -1.0397208405e+02;

/*
 * Calls the core __ieee754_expf and, unless the library version is IEEE, reports overflow (x >
 * 88.72168) and underflow (x < -103.97208) of finite arguments (finitef) as errors named "expf"
 * through matherr and errno (ERANGE 34).
 */
/* 0x001e5ed0 */
float expf(float x)
{
    float z;
    struct exception exc;

    z = __ieee754_expf(x);
    if (libm_lib_version == _IEEE_)
        return z;
    if (finitef(x)) {
        if (x > o_threshold) {
            exc.type = 3;
            exc.name = "expf";
            exc.err = 0;
            exc.arg1 = exc.arg2 = (double)x;
            if (libm_lib_version == _SVID_)
                exc.retval = 3.40282346638528859812e+38F;
            else
                exc.retval = expf_huge_val;
            if (libm_lib_version == _POSIX_)
                *__errno() = 34;
            else if (!matherr(&exc)) {
                *__errno() = 34;
            }
            if (exc.err != 0)
                *__errno() = exc.err;
            return exc.retval;
        } else if (x < u_threshold) {
            exc.type = 4;
            exc.name = "expf";
            exc.err = 0;
            exc.arg1 = exc.arg2 = (double)x;
            exc.retval = 0.0;
            if (libm_lib_version == _POSIX_)
                *__errno() = 34;
            else if (!matherr(&exc)) {
                *__errno() = 34;
            }
            if (exc.err != 0)
                *__errno() = exc.err;
            return exc.retval;
        }
    }
    return z;
}
