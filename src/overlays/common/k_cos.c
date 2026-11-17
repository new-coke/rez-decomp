/* k_cos.c: one source for the copy of this unit that each of 3 overlays links (stg02 stg04 stg22);
 * every overlay's symbol map places the names at its own addresses. */

#include "types_common.h"
#include "types_libc.h"

#include "types_overlays.h"

#include "debug.h"
#include "k_cos.h"

static const double kernel_cos_one = 1.00000000000000000000e+00,
    kernel_cos_C1 = 4.16666666666666019037e-02, kernel_cos_C2 = -1.38888888888741095749e-03,
    kernel_cos_C3 = 2.48015872894767294178e-05, kernel_cos_C4 = -2.75573143513906633035e-07,
    kernel_cos_C5 = 2.08757232129817482790e-09, kernel_cos_C6 = -1.13596475577881948265e-11;

/* 0x00b208c0 stg02, 0x00b1fcd8 stg04, 0x00b28fa0 stg22 */
double kernel_cos(double x, double y)
{
    double a, hz, z, r, qx;
    int ix;

    GET_HIGH_WORD(ix, x);
    ix &= 0x7fffffff;
    if (ix < 0x3e400000) {
        if (((int)x) == 0)
            return kernel_cos_one;
    }
    z = x * x;
    r = z * (kernel_cos_C1 +
             z * (kernel_cos_C2 +
                  z * (kernel_cos_C3 +
                       z * (kernel_cos_C4 +
                            z * (kernel_cos_C5 + z * kernel_cos_C6)))));
    if (ix < 0x3FD33333)
        return kernel_cos_one - (0.5 * z - (z * r - x * y));
    else {
        if (ix > 0x3fe90000) {
            qx = 0.28125;
        } else {
            INSERT_WORDS(qx, ix - 0x00200000, 0);
        }
        hz = 0.5 * z - qx;
        a = kernel_cos_one - qx;
        return a - (hz - (z * r - x * y));
    }
}
