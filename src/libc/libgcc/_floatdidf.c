#include "types_libc.h"
#include "types_sce.h"

/*
 * Converts a signed 64-bit integer to a double (high word scaled by 2^32 plus the unsigned low
 * word).
 */
/* 0x001fe090 */
double __floatdidf(long u)
{
    double d;

    d = (int)(u >> 32);
    d *= 65536.0;
    d *= 65536.0;
    d += (unsigned int)(u & 0xffffffffUL);
    return d;
}
