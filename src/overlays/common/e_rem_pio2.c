/* e_rem_pio2.c: one source for the copy of this unit that each of 3 overlays links (stg02 stg04
 * stg22); every overlay's symbol map places the names at its own addresses. */

#include "types_common.h"

#include "types_overlays.h"

#include "debug.h"
#include "libc.h"
#include "e_rem_pio2.h"
#include "k_rem_pio2.h"

extern int two_over_pi[];
extern int npio2_hw[];

static const double rem_pio2_zero = 0.00000000000000000000e+00, rem_pio2_half = 5.00000000000000000000e-01,
    rem_pio2_two24 = 1.67772160000000000000e+07, rem_pio2_invpio2 = 6.36619772367581382433e-01,
    rem_pio2_pio2_1 = 1.57079632673412561417e+00, rem_pio2_pio2_1t = 6.07710050650619224932e-11,
    rem_pio2_pio2_2 = 6.07710050630396597660e-11, rem_pio2_pio2_2t = 2.02226624879595063154e-21,
    rem_pio2_pio2_3 = 2.02226624871116645580e-21, rem_pio2_pio2_3t = 8.47842766036889956997e-32;

/* 0x00b203a0 stg02, 0x00b1f7b8 stg04, 0x00b28a80 stg22 */
int ieee754_rem_pio2(double x, double *y)
{
    double z, w, t, r, fn;
    double tx[3];
    int e0, i, j, nx, n, ix, hx;
    unsigned int low;

    GET_HIGH_WORD(hx, x);
    ix = hx & 0x7fffffff;
    if (ix <= 0x3fe921fb) {
        y[0] = x;
        y[1] = 0;
        return 0;
    }
    if (ix < 0x4002d97c) {
        if (hx > 0) {
            z = x - rem_pio2_pio2_1;
            if (ix != 0x3ff921fb) {
                y[0] = z - rem_pio2_pio2_1t;
                y[1] = (z - y[0]) - rem_pio2_pio2_1t;
            } else {
                z -= rem_pio2_pio2_2;
                y[0] = z - rem_pio2_pio2_2t;
                y[1] = (z - y[0]) - rem_pio2_pio2_2t;
            }
            return 1;
        } else {
            z = x + rem_pio2_pio2_1;
            if (ix != 0x3ff921fb) {
                y[0] = z + rem_pio2_pio2_1t;
                y[1] = (z - y[0]) + rem_pio2_pio2_1t;
            } else {
                z += rem_pio2_pio2_2;
                y[0] = z + rem_pio2_pio2_2t;
                y[1] = (z - y[0]) + rem_pio2_pio2_2t;
            }
            return -1;
        }
    }
    if (ix <= 0x413921fb) {
        t = libm_fabs(x);
        n = (int)(t * rem_pio2_invpio2 + rem_pio2_half);
        fn = (double)n;
        r = t - fn * rem_pio2_pio2_1;
        w = fn * rem_pio2_pio2_1t;
        if (n < 32 && ix != npio2_hw[n - 1]) {
            y[0] = r - w;
        } else {
            unsigned int high;

            j = ix >> 20;
            y[0] = r - w;
            GET_HIGH_WORD(high, y[0]);
            i = j - ((high >> 20) & 0x7ff);
            if (i > 16) {
                t = r;
                w = fn * rem_pio2_pio2_2;
                r = t - w;
                w = fn * rem_pio2_pio2_2t - ((t - r) - w);
                y[0] = r - w;
                GET_HIGH_WORD(high, y[0]);
                i = j - ((high >> 20) & 0x7ff);
                if (i > 49) {
                    t = r;
                    w = fn * rem_pio2_pio2_3;
                    r = t - w;
                    w = fn * rem_pio2_pio2_3t - ((t - r) - w);
                    y[0] = r - w;
                }
            }
        }
        y[1] = (r - y[0]) - w;
        if (hx < 0) {
            y[0] = -y[0];
            y[1] = -y[1];
            return -n;
        } else
            return n;
    }
    if (ix >= 0x7ff00000) {
        y[0] = y[1] = x - x;
        return 0;
    }
    GET_LOW_WORD(low, x);
    SET_LOW_WORD(z, low);
    e0 = ((ix >> 20) - 1046);
    SET_HIGH_WORD(z, ix - (e0 << 20));
    for (i = 0; i < 2; i++) {
        tx[i] = (double)((int)(z));
        z = (z - tx[i]) * rem_pio2_two24;
    }
    tx[2] = z;
    nx = 3;
    while (tx[nx - 1] == rem_pio2_zero)
        nx--;
    n = kernel_rem_pio2(tx, y, e0, nx, 2, two_over_pi);
    if (hx < 0) {
        y[0] = -y[0];
        y[1] = -y[1];
        return -n;
    }
    return n;
}
