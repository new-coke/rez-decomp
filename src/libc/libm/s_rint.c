#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"

static const double
TWO52[2] = {
    4.50359962737049600000e+15,
    -4.50359962737049600000e+15,
};

/*
 * Rint: rounds a double to an integral value in the current rounding mode by adding and
 * subtracting 2^52 with the sign of x (TWO52 table).
 */
/* 0x001e9ae0 */
double rint(double x)
{
    int i0, j0, sx;
    unsigned int i, i1;
    double w, t;

    EXTRACT_WORDS(i0, i1, x);
    sx = (i0 >> 31) & 1;
    j0 = ((i0 >> 20) & 0x7ff) - 0x3ff;
    if (j0 < 20) {
        if (j0 < 0) {
            if (((i0 & 0x7fffffff) | i1) == 0)
                return x;
            i1 |= (i0 & 0x0fffff);
            i0 &= 0xfffe0000;
            i0 |= ((i1 | -i1) >> 12) & 0x80000;
            SET_HIGH_WORD(x, i0);
            w = TWO52[sx] + x;
            t = w - TWO52[sx];
            GET_HIGH_WORD(i0, t);
            SET_HIGH_WORD(t, (i0 & 0x7fffffff) | (sx << 31));
            return t;
        } else {
            i = (0x000fffff) >> j0;
            if (((i0 & i) | i1) == 0)
                return x;
            i >>= 1;
            if (((i0 & i) | i1) != 0) {
                if (j0 == 19)
                    i1 = 0x40000000;
                else
                    i0 = (i0 & (~i)) | ((0x20000) >> j0);
            }
        }
    } else if (j0 > 51) {
        if (j0 == 0x400)
            return x + x;
        else
            return x;
    } else {
        i = ((unsigned int)(0xffffffff)) >> (j0 - 20);
        if ((i1 & i) == 0)
            return x;
        i >>= 1;
        if ((i1 & i) != 0)
            i1 = (i1 & (~i)) | ((0x40000000) >> (j0 - 20));
    }
    INSERT_WORDS(x, i0, i1);
    w = TWO52[sx] + x;
    return w - TWO52[sx];
}
