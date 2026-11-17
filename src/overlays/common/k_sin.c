/* k_sin.c: one source for the copy of this unit that each of 3 overlays links (stg02 stg04 stg22);
 * every overlay's symbol map places the names at its own addresses. */

#include "types_common.h"
#include "types_libc.h"

#include "types_overlays.h"

#include "debug.h"
#include "k_sin.h"

static const double kernel_sin_half = 5.00000000000000000000e-01,
    kernel_sin_S1 = -1.66666666666666324348e-01, kernel_sin_S2 = 8.33333333332248946124e-03,
    kernel_sin_S3 = -1.98412698298579493134e-04, kernel_sin_S4 = 2.75573137070700676789e-06,
    kernel_sin_S5 = -2.50507602534068634195e-08, kernel_sin_S6 = 1.58969099521155010221e-10;

/* fdlibm's __kernel_sin, floor and scalbn stay last in the file: their constants end the
   overlay image, as here they end .rodata. */
/* 0x00b21678 stg02, 0x00b20a90 stg04, 0x00b29d58 stg22 */
double kernel_sin(double x, double y, int iy)
{
    double z, r, v;
    int ix;

    GET_HIGH_WORD(ix, x);
    ix &= 0x7fffffff;
    if (ix < 0x3e400000) {
        if ((int)x == 0)
            return x;
    }
    z = x * x;
    v = z * x;
    r = kernel_sin_S2 +
        z * (kernel_sin_S3 +
             z * (kernel_sin_S4 + z * (kernel_sin_S5 + z * kernel_sin_S6)));
    if (iy == 0)
        return x + v * (kernel_sin_S1 + z * r);
    else
        return x - ((z * (kernel_sin_half * y - v * r) - y) - v * kernel_sin_S1);
}
