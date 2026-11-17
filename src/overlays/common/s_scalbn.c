/* s_scalbn.c: one source for the copy of this unit that each of 3 overlays links (stg02 stg04
 * stg22); every overlay's symbol map places the names at its own addresses. */

#include "types_common.h"
#include "types_libc.h"

#include "types_overlays.h"

#include "debug.h"
#include "s_scalbn.h"
#include "s_copysign.h"

static const double scalbn_two54 = 1.80143985094819840000e+16, scalbn_twom54 = 5.55111512312578270212e-17, scalbn_huge = 1.0e+300, scalbn_tiny = 1.0e-300;

/* 0x00b21a28 stg02, 0x00b20e40 stg04, 0x00b2a108 stg22 */
double scalbn(double x, int n)
{
    int k, hx, lx;

    EXTRACT_WORDS(hx, lx, x);
    k = (hx & 0x7ff00000) >> 20;
    if (k == 0) {
        if ((lx | (hx & 0x7fffffff)) == 0)
            return x;
        x *= scalbn_two54;
        GET_HIGH_WORD(hx, x);
        k = ((hx & 0x7ff00000) >> 20) - 54;
        if (n < -50000)
            return scalbn_tiny * x;
    }
    if (k == 0x7ff)
        return x + x;
    k = k + n;
    if (k > 0x7fe)
        return scalbn_huge * copysign(scalbn_huge, x);
    if (k > 0) {
        SET_HIGH_WORD(x, (hx & 0x800fffff) | (k << 20));
        return x;
    }
    if (k <= -54) {
        if (n > 50000)
            return scalbn_huge * copysign(scalbn_huge, x);
        else
            return scalbn_tiny * copysign(scalbn_tiny, x);
    }
    k += 54;
    SET_HIGH_WORD(x, (hx & 0x800fffff) | (k << 20));
    return x * scalbn_twom54;
}
