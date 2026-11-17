#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

/*
 * Tangent of a float: small arguments go to the tangent kernel __kernel_tanf, infinities/NaN give
 * x-x, others are reduced by __ieee754_rem_pio2f (odd quadrants give -1/tan).
 */
/* 0x001e5af8 */
float tanf(float x)
{
    float y[2], z = 0.0;
    int n, ix;

    GET_FLOAT_WORD(ix, x);
    ix &= 0x7fffffff;
    if (ix <= 0x3f490fda)
        return __kernel_tanf(x, z, 1);
    else if (ix >= 0x7f800000)
        return x - x;
    else {
        n = __ieee754_rem_pio2f(x, y);
        return __kernel_tanf(y[0], y[1], 1 - ((n & 1) << 1));
    }
}
