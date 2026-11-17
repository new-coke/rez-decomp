/* w_acos.c: one source for the copy of this unit that each of 2 overlays links (b_uranus b_venus);
 * every overlay's symbol map places the names at its own addresses. */

#include "types_common.h"

#include "mathdef.h"
#include "types_overlays.h"
#include "libc.h"
#include "libc_internal.h"
#include "w_acos.h"
#include "e_acos.h"

extern const int libm_lib_version;

/* 0x00ccb8e8 b_uranus, 0x00cce068 b_venus */
double acos(double x)
{
    double z;
    LibmException exc;

    z = libm_acos_kernel(x);
    if (libm_lib_version == -1 || isnan(x))
        return z;
    if (libm_fabs(x) > 1.0) {
        exc.type = 1;
        exc.name = "acos";
        exc.err = 0;
        exc.arg1 = exc.arg2 = x;
        exc.retval = 0.0;
        if (libm_lib_version == 2)
            *__errno() = 33;
        else if (!matherr(&exc))
            *__errno() = 33;
        if (exc.err != 0)
            *__errno() = exc.err;
        return exc.retval;
    } else
        return z;
}
