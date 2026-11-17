#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

static const float one = 1.0, tiny = 1.0e-30;

/*
 * Bit-by-bit square root of a float: NaN/Inf give x*x+x, +-0 is returned, negatives give
 * (x-x)/(x-x); subnormals are normalised first and the result is rounded to nearest.
 */
/* 0x001e86b8 */
float __ieee754_sqrtf(float x)
{
    float z;
    int sign = (int)0x80000000;
    unsigned int r;
    int ix, s, q, m, t, i;

    GET_FLOAT_WORD(ix, x);

    if ((ix & 0x7f800000) == 0x7f800000) {
        return x * x + x;
    }
    if (ix <= 0) {
        if ((ix & (~sign)) == 0)
            return x;
        else if (ix < 0)
            return (x - x) / (x - x);
    }
    m = (ix >> 23);
    if (m == 0) {
        for (i = 0; (ix & 0x00800000) == 0; i++)
            ix <<= 1;
        m -= i - 1;
    }
    m -= 127;
    ix = (ix & 0x007fffff) | 0x00800000;
    if (m & 1)
        ix += ix;
    m >>= 1;

    ix += ix;
    q = s = 0;
    r = 0x01000000;

    while (r != 0) {
        t = s + r;
        if (t <= ix) {
            s = t + r;
            ix -= t;
            q += r;
        }
        ix += ix;
        r >>= 1;
    }

    if (ix != 0) {
        z = one - tiny;
        if (z >= one) {
            z = one + tiny;
            if (z > one)
                q += 2;
            else
                q += (q & 1);
        }
    }
    ix = (q >> 1) + 0x3f000000;
    ix += (m << 23);
    SET_FLOAT_WORD(z, ix);
    return z;
}
