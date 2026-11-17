/* s_copysign.c: one source for the copy of this unit that each of 3 overlays links (stg02 stg04
 * stg22); every overlay's symbol map places the names at its own addresses. */

#include "types_common.h"
#include "types_libc.h"

#include "types_overlays.h"

#include "debug.h"
#include "s_copysign.h"

/* 0x00b21be0 stg02, 0x00b20ff8 stg04, 0x00b2a2c0 stg22 */
double copysign(double x, double y)
{
    unsigned int hx, hy;

    GET_HIGH_WORD(hx, x);
    GET_HIGH_WORD(hy, y);
    SET_HIGH_WORD(x, (hx & 0x7fffffff) | (hy & 0x80000000));
    return x;
}
