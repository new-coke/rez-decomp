#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

/*
 * Cosine of a float: small arguments go straight to the kernel __kernel_cosf, infinities/NaN give
 * x-x, others are reduced by __ieee754_rem_pio2f (pi/2 remainder) and dispatched on the quadrant
 * to the cosine (__kernel_cosf) or sine (__kernel_sinf) kernels.
 */
/* 0x001e5798 */
float cosf(float x)
{
    float y[2], z = 0.0;
    int n, ix;

    GET_FLOAT_WORD(ix, x);
    ix &= 0x7fffffff;
    if (ix <= 0x3f490fd8)
        return __kernel_cosf(x, z);
    else if (ix >= 0x7f800000)
        return x - x;
    else {
        n = __ieee754_rem_pio2f(x, y);
        switch (n & 3) {
        case 0:
            return __kernel_cosf(y[0], y[1]);
        case 1:
            return -__kernel_sinf(y[0], y[1], 1);
        case 2:
            return -__kernel_cosf(y[0], y[1]);
        default:
            return __kernel_sinf(y[0], y[1], 1);
        }
    }
}
