#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"

/* Returns 1 if a double is a NaN, else 0 (bit test on the high/low words). */
/* 0x001e9a80 */
int isnan(double x)
{
    int hx, lx;

    EXTRACT_WORDS(hx, lx, x);
    hx &= 0x7fffffff;
    hx |= (unsigned int)(lx | (-lx)) >> 31;
    hx = 0x7ff00000 - hx;
    return (int)(((unsigned int)(hx)) >> 31);
}
