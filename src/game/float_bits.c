/* Float bit-pattern helpers: mantissa rounding, depth nudges for layered 2D elements. */

#include "float_bits.h"

/* Round half up on the bit pattern. */
/* 0x00176a20 */
float float_round_mantissa(float x)
{
    *(int *)&x = (*(int *)&x + 0x7ff) & 0xfffff000;
    return x;
}

/* n units of bit 16: keeps one layer of the "Warning" box in front of another. */
/* 0x00176a48 */
float float_add_bits16(int n, float x)
{
    *(int *)&x += n << 16;
    return x;
}
