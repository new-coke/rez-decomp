#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"

/* Absolute value of a double (clears the sign bit of the high word). Proposed name: libm_fabs */
/* 0x001e9a48 */
double libm_fabs(double x)
{
    unsigned int high;

    GET_HIGH_WORD(high, x);
    do {
        ieee_double_shape_type sh_u;
        sh_u.value = x;
        sh_u.parts.msw = high & 0x7fffffff;
        x = sh_u.value;
    } while (0);
    return x;
}
