#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"

/* Returns 1 if a float is a NaN, else 0. */
/* 0x001e9fd0 */
int isnanf(float x)
{
    int ix;

    GET_FLOAT_WORD(ix, x);
    ix &= 0x7fffffff;
    ix = 0x7f800000 - ix;
    return (int)(((unsigned int)(ix)) >> 31);
}
