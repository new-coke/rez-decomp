#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

static const float
half = 5.0000000000e-01,
S1 = -1.6666667163e-01,
S2 = 8.3333337680e-03,
S3 = -1.9841270114e-04,
S4 = 2.7557314297e-06,
S5 = -2.5050759689e-08,
S6 = 1.5896910177e-10;

/* Sine kernel on [-pi/4, pi/4]: polynomial in x*x (S1..S6); iy != 0 means the tail y is used. */
/* 0x001e9298 */
float __kernel_sinf(float x, float y, int iy)
{
    float z, r, v;
    int ix;

    GET_FLOAT_WORD(ix, x);
    ix &= 0x7fffffff;
    if (ix < 0x32000000) {
        if ((int)x == 0)
            return x;
    }
    z = x * x;
    v = z * x;
    r = S2 + z * (S3 + z * (S4 + z * (S5 + z * S6)));
    if (iy == 0)
        return x + v * (S1 + z * r);
    else
        return x - ((z * (half * y - v * r) - y) - v * S1);
}
