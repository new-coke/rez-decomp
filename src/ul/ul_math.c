/*
 * Scalar maths: colour packing, distance, table trigonometry on 16-bit angles (65536 = full turn).
 */

#include "types_common.h"
#include "mathdef.h"
#include "libc.h"
#include "ul_math.h"
#include "ul_vu0.h"

extern float ulAtanTable[];
extern float ulAsinTable[];
extern float ulAcosTable[];
extern float ulSinPolyTable[];
extern float ulCosPolyTable[];
extern float ulSinTable[];

/* Four floats 0..255, highest byte first. */
/* 0x001b5860 */
void ulColorUnpack(unsigned int col, float *out)
{
    out[0] = (float)((col >> 24) & 0xff);
    out[1] = (float)((col >> 16) & 0xff);
    out[2] = (float)((col >> 8) & 0xff);
    out[3] = (float)(col & 0xff);
}

/* Inverse of ulColorUnpack. */
/* 0x001b5948 */
unsigned int ulColorPack(float *col)
{
    return ((int)col[0] & 0xff) << 24 | ((int)col[1] & 0xff) << 16 | ((int)col[2] & 0xff) << 8 | ((int)col[3] & 0xff);
}

/* 0x001b59a0 */
static float ulPointDistance(float *from, float *to)
{
    float dx;
    float dy;
    float dz;

    dx = to[0] - from[0];
    dy = to[1] - from[1];
    dz = to[2] - from[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

/* Same as ulPointDistance. */
/* 0x001b5a10 */
float ulVec3Distance(Vec3 *from, Vec3 *to)
{
    float dx;
    float dy;
    float dz;

    dx = to->x - from->x;
    dy = to->y - from->y;
    dz = to->z - from->z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

/*
 * 16-bit angle; piecewise fit (ulAtanTable, eight intervals of 0..1); slopes > 1 via the
 * reciprocal.
 */
/* 0x001b5a80 */
static int ulAtan(float x)
{
    float ax;
    float ang;
    float c0;
    float c1;
    float c2;
    float c3;
    int j;

    ax = fabsf(x);
    if (ax > 1.0f) {
        ax = 1.0f / ax;
        j = (int)(ax * 8.0f) * 4;
        c0 = ulAtanTable[j];
        c1 = ulAtanTable[j + 1];
        c2 = ulAtanTable[j + 2];
        c3 = ulAtanTable[j + 3];
        ang = c0 * rsqrtf(ax + 1.0f) + ((c1 * ax + c2) * ax + c3);
        ang = ANG_QUARTER - ang;
    } else {
        j = (int)(ax * 8.0f) * 4;
        c0 = ulAtanTable[j];
        c1 = ulAtanTable[j + 1];
        c2 = ulAtanTable[j + 2];
        c3 = ulAtanTable[j + 3];
        ang = c0 * rsqrtf(ax + 1.0f) + ((c1 * ax + c2) * ax + c3);
    }
    if (x < 0.0f)
        ang = -ang;
    return (int)ang;
}

/* 16-bit angle; piecewise fit (ulAsinTable, eight intervals of 0..1). */
/* 0x001b5c10 */
int ulAsin(float x)
{
    float ax;
    float ang;
    float c0;
    float c1;
    float c2;
    float c3;
    int i;

    ax = fabsf(x);
    i = (int)(ax * 8.0f);
    c1 = ulAsinTable[i * 4 + 1];
    c2 = ulAsinTable[i * 4 + 2];
    c3 = ulAsinTable[i * 4 + 3];
    c0 = ulAsinTable[i * 4];
    ang = (c1 * ax + c2) * ax + c3 + c0 * sqrtf(1.0f - ax);
    if (x >= 0.0f)
        return (int)ang;
    return -(int)ang;
}

/* 16-bit angle 0..32768; piecewise fit (ulAcosTable, eight intervals of 0..1). */
/* 0x001b5d10 */
static int ulAcos(float x)
{
    float ax;
    float ang;
    float poly;
    int i;
    int j;

    ax = fabsf(x);
    i = (int)(ax * 8.0f);
    j = i * 3;
    poly = (ulAcosTable[j] * ax + ulAcosTable[j + 1]) * ax + ulAcosTable[j + 2];
    ang = poly * sqrtf(1.0f - ax);
    if (x >= 0.0f)
        return (int)ang;
    return (int)(ANG_HALF - ang);
}

/* 0x001b5e00 */
float ulTan(int angle)
{
    float sc[2];

    ulSinCos(angle, &sc[0], &sc[1]);
    return sc[0] / sc[1];
}

/* Piecewise cubic (ulSinPolyTable, one per 2048 units) instead of the table. */
/* 0x001b5e38 */
float ulSinPoly(int angle)
{
    int i;
    float x;

    i = (angle & 0xf800) >> 9;
    x = (float)(angle & 0xffff);
    return x * (x * (x * ulSinPolyTable[i] + ulSinPolyTable[i + 1]) + ulSinPolyTable[i + 2]) + ulSinPolyTable[i + 3];
}

/* Piecewise cubic (ulCosPolyTable). */
/* 0x001b5eb0 */
static float ulCosPoly(int angle)
{
    int i;
    float x;

    i = (angle & 0xf800) >> 9;
    x = (float)(angle & 0xffff);
    return x * (x * (x * ulCosPolyTable[i] + ulCosPolyTable[i + 1]) + ulCosPolyTable[i + 2]) + ulCosPolyTable[i + 3];
}

/* 0x001b5f28 */
static float ulTanPoly(int angle)
{
    float n;

    n = ulSinPoly(angle);
    return n / ulCosPoly(angle);
}

/* 65536-entry ulSinTable. */
/* 0x001b5f70 */
float ulSin(int angle)
{
    return ulSinTable[angle & 0xffff];
}

/* Sine a quarter turn ahead. */
/* 0x001b5f90 */
float ulCos(int angle)
{
    return ulSinTable[(angle + ANG_90) & 0xffff];
}

#ifndef VERSION_US

/* Either output may be 0. */
/* 0x001b5fb0 */
void ulSinCos(int angle, float *s, float *c)
{
    if (s)
        *s = ulSinTable[angle & 0xffff];
    if (c)
        *c = ulSinTable[(angle + ANG_90) & 0xffff];
}

#endif /* !VERSION_US */

#ifdef VERSION_US

/* Both outputs required. */
/* 0x001b5fb0 */
void ulSinCos(int angle, float *sin_out, float *cos_out)
{
    *sin_out = ulSinTable[angle & 0xffff];
    *cos_out = ulSinTable[(angle + ANG_90) & 0xffff];
}

#endif /* VERSION_US */

/* Radians; either output may be 0. */
/* 0x001b5fe8 */
void ulSinCosf(float angle, float *s, float *c)
{
    if (s)
        *s = sinf(angle);
    if (c)
        *c = cosf(angle);
}

/* 0x001b6048 */
float ulRoundAway(float x)
{
    if (x < 0.0f)
        return -ceilf(fabsf(x));
    return ceilf(fabsf(x));
}
