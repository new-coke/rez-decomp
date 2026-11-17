#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"

/* Absolute value of a float. Proposed name: libm_fabsf */
/* 0x001e9f88 */
float libm_fabsf(float x)
{
    unsigned int ix;

    GET_FLOAT_WORD(ix, x);
    SET_FLOAT_WORD(x, ix & 0x7fffffff);
    return x;
}
