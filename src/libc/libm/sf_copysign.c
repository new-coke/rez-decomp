#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"

/* Returns x with the sign of y (float). */
/* 0x001ea158 */
float copysignf(float x, float y)
{
    unsigned int ix, iy;

    GET_FLOAT_WORD(ix, x);
    GET_FLOAT_WORD(iy, y);
    SET_FLOAT_WORD(x, (ix & 0x7fffffff) | (iy & 0x80000000));
    return x;
}
