#include "types_libc.h"
#include "types_sce.h"
#include "libc.h"

/* Returns 1 when x is an infinity (exponent all ones, zero mantissa), else 0. */
/* 0x001f5240 */
int isinf(double x)
{
    int hx, lx;

    do {
        DShape ew_u;
        ew_u.value = x;
        hx = ew_u.parts.msw;
        lx = ew_u.parts.lsw;
    } while (0);
    hx &= 0x7fffffff;
    hx |= (unsigned int)(lx | (-lx)) >> 31;
    hx = 0x7ff00000 - hx;
    return 1 - (int)((unsigned int)(hx | (-hx)) >> 31);
}
