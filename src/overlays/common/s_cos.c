/* s_cos.c: one source for the copy of this unit that each of 3 overlays links (stg02 stg04 stg22);
 * every overlay's symbol map places the names at its own addresses. */

#include "types_common.h"
#include "types_libc.h"

#include "types_overlays.h"

#include "debug.h"
#include "s_cos.h"
#include "e_rem_pio2.h"
#include "k_cos.h"
#include "k_sin.h"

/* 0x00b201a0 stg02, 0x00b1f5b8 stg04, 0x00b28880 stg22 */
double cos(double x)
{
    double y[2], z = 0.0;
    int n, ix;

    GET_HIGH_WORD(ix, x);
    ix &= 0x7fffffff;
    if (ix <= 0x3fe921fb)
        return kernel_cos(x, z);
    else if (ix >= 0x7ff00000)
        return x - x;
    else {
        n = ieee754_rem_pio2(x, y);
        switch (n & 3) {
        case 0:
            return kernel_cos(y[0], y[1]);
        case 1:
            return -kernel_sin(y[0], y[1], 1);
        case 2:
            return -kernel_cos(y[0], y[1]);
        default:
            return kernel_sin(y[0], y[1], 1);
        }
    }
}
