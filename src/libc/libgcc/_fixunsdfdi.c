#include "types_libc.h"
#include "types_sce.h"

extern double __floatdidf(long u);

/*
 * Converts a double to an unsigned 64-bit integer (0 for negative values): the high word from x *
 * 2^-32, the low word from the remainder.
 */
/* 0x001fdfa0 */
unsigned long __fixunsdfdi(double a)
{
    double b;
    unsigned long v;

    if (a < 0)
        return 0;
    b = a / 4294967296.0;
    v = (unsigned int)b;
    v <<= 32;
    a -= (double)v;
    if (a < 0)
        v -= (unsigned int)(-a);
    else
        v += (unsigned int)a;
    return v;
}
