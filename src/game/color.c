/* Packed PS2 colours (8 bits per channel, 0x80 = full): add, subtract, blend, 0..255 access. */

#include "mathdef.h"
#include "libc.h"
#include "ul_math.h"
#include "color.h"
#include "line_batch.h"

/* XZ plane around the current matrix's origin, n segments. */
/* steer(cast) */
/* 0x00109088 */
int draw_ring_xz(float radius, int col, int n)
{
    float va[4];
    float vb[4];
    float sc[2];
    float *prev = va;
    float *cur = vb;
    float *t;
    float cs;
    float sn;
    int i;
    int step = 0x10000 / n;
    int ang = step;
    void *batch;

    batch = line_set_color(col);
    vb[0] = radius;
    cur[1] = 0;
    cur[2] = 0;
    for (i = 0; i < n; i++) {
        t = prev;
        prev = cur;
        cur = t;
        ulSinCos(ang, &sc[0], &sc[1]);
        cs = sc[1];
        sn = sc[0];
        cur[0] = radius * cs;
        cur[1] = 0;
        cur[2] = radius * sn;
        batch = line_add_v(batch, prev, cur);
        ang += step;
    }
    prev[0] = radius;
    prev[1] = 0;
    prev[2] = 0;
    return ((int (*)(void *))line_flush)(line_add_v(batch, prev, cur));
}

/* 0x001091c0 */
static int color_return_zero(void)
{
    return 0;
}

/* 0x001091c8 */
static int color_return_zero_2(void)
{
    return 0;
}

/* 0x001091d0 */
static int color_return_zero_3(void)
{
    return 0;
}

/* RGB saturate at 255 per byte; alphas added on the 0..255 scale, clamped, stored in PS2 units. */
/* 0x001091d8 */
static unsigned int color_add(unsigned int col_a, unsigned int col_b)
{
    unsigned int lo = 0x7f7f7f;
    unsigned int hi = 0x808080;
    unsigned int x;
    unsigned int y;
    int al;

    al = color_get_a(col_a) + color_get_a(col_b);
    x = ((col_a & lo) + (col_b & hi)) ^ ((col_a ^ col_b) & hi);
    y = col_a + col_b - x;
    y -= y >> 8;
    col_a = x | y;
    if (255 < al)
        al = 255;
    return (col_a & 0xffffff) | (((al + 1) >> 1) << 24);
}

/* RGB clamp at 0 (~(~a + b) with color_add's saturation); alpha clamped at 0. */
/* 0x00109298 */
static unsigned int color_sub(unsigned int col_a, unsigned int col_b)
{
    unsigned int lo = 0x7f7f7f;
    unsigned int hi = 0x808080;
    unsigned int x;
    unsigned int y;
    int al;

    al = color_get_a(col_a) - color_get_a(col_b);
    col_a ^= 0xffffff;
    x = (col_a & lo) + (col_b & hi);
    x ^= (col_a ^ col_b) & hi;
    y = col_a + col_b - x;
    y -= y >> 8;
    col_a = (x | y) ^ 0xffffff;
    if (al < 0)
        al = 0;
    return (col_a & 0xffffff) | (((al + 1) >> 1) << 24);
}

/*
 * Equal channels stay. Steps in (-1, 1) pushed one unit from zero so the blend always moves; the
 * else branch also catches steps >= 1, which lose a unit instead.
 */
/* 0x00109360 */
unsigned int color_blend(int ia, int ib, float t)
{
    float from[4];
    float to[4];
    float x;
    float diff;

    ulColorUnpack(ia, from);
    ulColorUnpack(ib, to);
    x = from[0];
    diff = to[0] - x;
    if (!(fabsf(diff) < FLT_EPSILON)) {
        diff = diff * t;
        if (0.0f <= diff && diff < 1.0f)
            diff = diff + 1.0f;
        else if (-1.0f < diff)
            diff = diff - 1.0f;
        from[0] = x + diff;
    }
    diff = to[1] - from[1];
    if (!(fabsf(diff) < FLT_EPSILON)) {
        diff = diff * t;
        if (0.0f <= diff && diff < 1.0f)
            diff = diff + 1.0f;
        else if (-1.0f < diff)
            diff = diff - 1.0f;
        from[1] = from[1] + diff;
    }
    diff = to[2] - from[2];
    if (!(fabsf(diff) < FLT_EPSILON)) {
        diff = diff * t;
        if (0.0f <= diff && diff < 1.0f)
            diff = diff + 1.0f;
        else if (-1.0f < diff)
            diff = diff - 1.0f;
        from[2] = from[2] + diff;
    }
    diff = to[3] - from[3];
    if (!(fabsf(diff) < FLT_EPSILON)) {
        diff = diff * t;
        if (0.0f <= diff && diff < 1.0f)
            diff = diff + 1.0f;
        else if (-1.0f < diff)
            diff = diff - 1.0f;
        from[3] = from[3] + diff;
    }
    return ulColorPack(from);
}

/* PS2 units -> 0..255: a * 2 - 1, 0 stays 0. */
/* 0x001095b8 */
int color_get_a(unsigned int color)
{
    unsigned int val = color >> 23;
    if (val == 0)
        return 0;
    return val - 1;
}

/* 0..255, as color_get_a. */
/* 0x001095d8 */
int color_get_r(unsigned char color)
{
    int val = color << 1;
    if (val == 0)
        return 0;
    return val - 1;
}

/* 0..255. */
/* 0x001095f8 */
int color_get_g(unsigned int color)
{
    int val = (color >> 8 & 0xff) << 1;
    if (val == 0)
        return 0;
    return val - 1;
}

/* 0..255. */
/* 0x00109618 */
int color_get_b(unsigned int color)
{
    int val = (color >> 16 & 0xff) << 1;
    if (val == 0)
        return 0;
    return val - 1;
}
