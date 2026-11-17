#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

static const float
ln2_hi = 6.9313812256e-01,
ln2_lo = 9.0580006145e-06,
two25 = 3.355443200e+07,
Lg1 = 6.6666668653e-01,
Lg2 = 4.0000000596e-01,
Lg3 = 2.8571429849e-01,
Lg4 = 2.2222198546e-01,
Lg5 = 1.8183572590e-01,
Lg6 = 1.5313838422e-01,
Lg7 = 1.4798198640e-01;

static const float zero = 0.0;

/*
 * Natural log of a float: x = 2^k * (1+f), then log(1+f) from s = f/(2+f) and the Lg1..Lg7
 * polynomial; zero gives -inf, negatives NaN.
 */
/* 0x001e7720 */
float __ieee754_logf(float x)
{
    float hfsq, f, s, z, R, w, t1, t2, dk;
    int k, ix, i, j;

    GET_FLOAT_WORD(ix, x);

    k = 0;
    if (ix < 0x00800000) {
        if ((ix & 0x7fffffff) == 0)
            return -two25 / zero;
        if (ix < 0)
            return (x - x) / zero;
        k -= 25;
        x *= two25;
        GET_FLOAT_WORD(ix, x);
    }
    if (ix >= 0x7f800000)
        return x + x;
    k += (ix >> 23) - 127;
    ix &= 0x007fffff;
    i = (ix + (0x95f64 << 3)) & 0x800000;
    SET_FLOAT_WORD(x, ix | (i ^ 0x3f800000));
    k += (i >> 23);
    f = x - (float)1.0;
    if ((0x007fffff & (15 + ix)) < 16) {
        if (f == zero) {
            if (k == 0)
                return zero;
            else {
                dk = (float)k;
                return dk * ln2_hi + dk * ln2_lo;
            }
        }
        R = f * f * ((float)0.5 - (float)0.33333333333333333 * f);
        if (k == 0)
            return f - R;
        else {
            dk = (float)k;
            return dk * ln2_hi - ((R - dk * ln2_lo) - f);
        }
    }
    s = f / ((float)2.0 + f);
    dk = (float)k;
    z = s * s;
    i = ix - (0x6147a << 3);
    w = z * z;
    j = (0x6b851 << 3) - ix;
    t1 = w * (Lg2 + w * (Lg4 + w * Lg6));
    t2 = z * (Lg1 + w * (Lg3 + w * (Lg5 + w * Lg7)));
    i |= j;
    R = t2 + t1;
    if (i > 0) {
        hfsq = (float)0.5 * f * f;
        if (k == 0)
            return f - (hfsq - s * (hfsq + R));
        else
            return dk * ln2_hi - ((hfsq - (s * (hfsq + R) + dk * ln2_lo)) - f);
    } else {
        if (k == 0)
            return f - s * (f - R);
        else
            return dk * ln2_hi - ((s * (f - R) - dk * ln2_lo) - f);
    }
}
