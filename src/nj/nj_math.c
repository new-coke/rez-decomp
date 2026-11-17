/* Ninja maths: trig (65536 = full turn), sqrt, lerp, distance, Bezier, projection. */

#include "ninja.h"
#include "libc.h"
#include "nj_math.h"
#include "ul_math.h"

extern void vu0_matrix_apply(void *m, NjVec4 *in, NjVec4 *out);

/* Screen x, y to out[0..1]; returns 1/w. */
/* 0x001a1670 */
static float nj_project_point(NjVec3 *pos, float *out)
{
    NjSys *sys = &ulGxState;
    NjVec4 proj;
    float inv_w;

    inv_w = 1.0f;
    proj.x = pos->x;
    proj.y = pos->y;
    proj.z = pos->z;
    proj.w = inv_w;
    vu0_matrix_apply(&sys->proj_mtx, &proj, &proj);
    inv_w = inv_w / proj.w;
    out[0] = proj.x * inv_w;
    out[1] = proj.y * inv_w;
    return inv_w;
}

/* 0x001a1700 */
float njTan(int ang)
{
    return ulTan(ang);
}

/* 0x001a1720 */
float njSin(int ang)
{
    return ulSin(ang);
}

/* 0x001a1740 */
float njCos(int ang)
{
    return ulCos(ang);
}

/* 0x001a1760 */
static float njSqrt(float x)
{
    return sqrtf(x);
}

/* 0x001a1798 */
static float njInvertSqrt(float x)
{
    return 1.0f / sqrtf(x);
}

static inline void lerp3(NjVec3 *from, NjVec3 *to, NjVec3 *out, float t)
{
    float one_minus_t = 1.0f - t;
    out->x = from->x * one_minus_t + to->x * t;
    out->y = from->y * one_minus_t + to->y * t;
    out->z = from->z * one_minus_t + to->z * t;
}

/* t = 0 -> pts[0], 1 -> pts[1]. */
/* 0x001a17e8 */
static void nj_vec3_lerp(NjVec3 *pts, NjVec3 *out, float t)
{
    lerp3(&pts[0], &pts[1], out, t);
}

/* 0x001a1848 */
static float nj_round_away(float x)
{
    return ulRoundAway(x);
}

/* 0x001a1868 */
static float nj_vec3_distance(NjVec3 *from, NjVec3 *to)
{
    float dx = to->x - from->x;
    float dy = to->y - from->y;
    float dz = to->z - from->z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

/* C(n, k) = n..n-k+1 / k!. */
/* 0x001a18d8 */
static int nj_binomial(int n, int k)
{
    int den = 1;
    int num = 1;
    while (k) {
        den *= k;
        num *= n - k + 1;
        k--;
    }
    return num / den;
}

/* t in 0..1, *curve->npts control points, Bernstein-weighted. */
/* 0x001a1928 */
static void nj_bezier_point(float t, NjVec3 *ctrl, NjVec3 *out, NjCurve *curve)
{
    int i, n;
    float one_minus_t, weight;

    one_minus_t = 1.0f - t;
    out->x = 0.0f;
    out->y = 0.0f;
    out->z = 0.0f;
    n = *curve->npts - 1;
    for (i = 0; i <= n; i++) {
        weight = (float)nj_binomial(n, i) * powf(t, (float)i) * powf(one_minus_t, (float)(n - i));
        out->x += weight * ctrl[i].x;
        out->y += weight * ctrl[i].y;
        out->z += weight * ctrl[i].z;
    }
}
