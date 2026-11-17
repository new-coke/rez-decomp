#include "types_libc.h"
#include "types_sce.h"

extern unsigned long __fixunsdfdi(double a);

/* Converts a double to a signed 64-bit integer through the unsigned conversion __fixunsdfdi. */
/* 0x001fdf40 */
static long __fixdfdi(double a)
{
    if (a < 0)
        return -__fixunsdfdi(-a);
    return __fixunsdfdi(a);
}
