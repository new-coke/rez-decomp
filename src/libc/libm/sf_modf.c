#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"

/* Splits a float into integral part (stored through iptr) and signed fractional part. */
/* 0x001e5968 */
float modff(float x, float *iptr)
{
    int i0, j0;
    unsigned int i;

    GET_FLOAT_WORD(i0, x);
    j0 = ((i0 >> 23) & 0xff) - 0x7f;
    if (j0 < 23) {
        if (j0 < 0) {
            SET_FLOAT_WORD(*iptr, i0 & 0x80000000);
            return x;
        } else {
            i = (0x007fffff) >> j0;
            if ((i0 & i) == 0) {
                unsigned int ix;
                *iptr = x;
                GET_FLOAT_WORD(ix, x);
                SET_FLOAT_WORD(x, ix & 0x80000000);
                return x;
            } else {
                SET_FLOAT_WORD(*iptr, i0 & (~i));
                return x - *iptr;
            }
        }
    } else {
        unsigned int ix;
        *iptr = x;
        GET_FLOAT_WORD(ix, x);
        SET_FLOAT_WORD(x, ix & 0x80000000);
        return x;
    }
}
