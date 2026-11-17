#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"

/* Returns 1 if a float is finite, else 0. */
/* 0x001e9fa8 */
int finitef(float x)
{
    int ix;

    GET_FLOAT_WORD(ix, x);
    return (int)((unsigned int)((ix & 0x7fffffff) - 0x7f800000) >> 31);
}
