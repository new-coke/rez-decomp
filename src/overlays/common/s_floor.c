/* s_floor.c: one source for the copy of this unit that each of 3 overlays links (stg02 stg04
 * stg22); every overlay's symbol map places the names at its own addresses. */

#include "types_common.h"
#include "types_libc.h"

#include "types_overlays.h"

#include "debug.h"
#include "s_floor.h"

static const double floor_huge = 1.0e300;

/* 0x00b21850 stg02, 0x00b20c68 stg04, 0x00b29f30 stg22 */
double floor(double x)
{
    int i0, i1, j0;
    unsigned int i, j;

    EXTRACT_WORDS(i0, i1, x);
    j0 = ((i0 >> 20) & 0x7ff) - 0x3ff;
    if (j0 < 20) {
        if (j0 < 0) {
            if (floor_huge + x > 0.0) {
                if (i0 >= 0) {
                    i0 = i1 = 0;
                } else if (((i0 & 0x7fffffff) | i1) != 0) {
                    i0 = 0xbff00000;
                    i1 = 0;
                }
            }
        } else {
            i = (0x000fffff) >> j0;
            if (((i0 & i) | i1) == 0)
                return x;
            if (floor_huge + x > 0.0) {
                if (i0 < 0)
                    i0 += (0x00100000) >> j0;
                i0 &= (~i);
                i1 = 0;
            }
        }
    } else if (j0 > 51) {
        if (j0 == 0x400)
            return x + x;
        else
            return x;
    } else {
        i = ((0xffffffff)) >> (j0 - 20);
        if ((i1 & i) == 0)
            return x;
        if (floor_huge + x > 0.0) {
            if (i0 < 0) {
                if (j0 == 20)
                    i0 += 1;
                else {
                    j = i1 + (1 << (52 - j0));
                    if (j < i1)
                        i0 += 1;
                    i1 = j;
                }
            }
            i1 &= (~i);
        }
    }
    INSERT_WORDS(x, i0, i1);
    return x;
}
