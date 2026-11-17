/*
 * Maths helpers: two seedable LCGs, lengths and distances, Bezier and cardinal splines, transforms
 * by the current matrix, quaternions (w first).
 */

#include "libc.h"
#include "math_util.h"
#include "ul_vu0.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "nj_matrix.h"
#include "system.h"

typedef struct SplineCfg {
    char pad_00[8];
    float *tension;             /* 8 */
} SplineCfg;

extern int rand_lcg_state_a;
extern int rand_lcg_state_b;
extern void vu0_matrix_identity(int);

/* Also resets its call count. */
/* 0x00178840 */
void rand_lcg_seed_a(int seed)
{
    rand_lcg_state_a = seed;
    rand_lcg_count_a = 0;
}

/* Also resets its call count. */
/* 0x00178858 */
void rand_lcg_seed_b(int seed)
{
    rand_lcg_state_b = seed;
    rand_lcg_count_b = 0;
}

/* x * 1103515245 + 12345; counts calls; returns the low 15 bits. */
/* 0x00178870 */
int rand_lcg_next_a(void)
{
    rand_lcg_state_a = rand_lcg_state_a * 1103515245 + 12345;
    rand_lcg_count_a++;
    return rand_lcg_state_a & 0x7fff;
}

/* As rand_lcg_next_a, own state. */
/* 0x001788a8 */
int rand_lcg_next_b(void)
{
    rand_lcg_state_b = rand_lcg_state_b * 1103515245 + 12345;
    rand_lcg_count_b++;
    return rand_lcg_state_b & 0x7fff;
}

/* modff into a scratch buffer, result dropped: no effect. */
/* steer(cast) */
/* 0x001788e0 */
void float_frac(void)
{
    int buf[4];

    ((void (*)(void *))modff)(buf);
}

/* y untouched. Returns 1 / length. */
/* 0x00178900 */
float vec2_normalize_xz(float *vec, float *out)
{
    float len = sqrtf(vec[0] * vec[0] + vec[2] * vec[2]);
    float inv = 1.0f / len;

    out[0] = vec[0] * inv;
    out[2] = vec[2] * inv;
    return inv;
}

/* Returns 1 / length. */
/* 0x00178990 */
float vec3_normalize_into(float *out, float x, float y, float z)
{
    float inv = rsqrtf(x * x + y * y + z * z);

    out[0] = x * inv;
    out[1] = y * inv;
    out[2] = z * inv;
    return inv;
}

/* 0x00178a08 */
float vec3_length_squared(float *vec)
{
    return vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2];
}

/* 0x00178a30 */
float vec3_distance_xz(float *va, float *vb)
{
    float x = va[0] - vb[0];
    float z = va[2] - vb[2];

    return sqrtf(x * x + z * z);
}

/* 0x00178a90 */
float vec3_distance_squared(Vec3 *va, Vec3 *vb)
{
    float x = va->x - vb->x;
    float y = va->y - vb->y;
    float z = va->z - vb->z;

    return x * x + y * y + z * z;
}

/* t in 0..1. */
/* 0x00178ad0 */
void bezier_cubic_eval(float *pts, float *out, float t)
{
    float u = 1.0f - t;
    float b0 = u * u * u;
    float b1 = u * 3.0f * u * t;
    float b2 = u * 3.0f * t * t;
    float b3 = t * t * t;

    out[0] = b0 * pts[0] + b1 * pts[3] + b2 * pts[6] + b3 * pts[9];
    out[1] = b0 * pts[1] + b1 * pts[4] + b2 * pts[7] + b3 * pts[10];
    out[2] = b0 * pts[2] + b1 * pts[5] + b2 * pts[8] + b3 * pts[11];
}

/* Between the middle two of four points, t in 0..1, tension from cfg. */
/* 0x00178ba0 */
void spline_cardinal_eval(float t, float *pts, float *out, SplineCfg *cfg)
{
    float t_sq = t * t;
    float t_cube = t_sq * t;
    float tan_scale = (1.0f - *cfg->tension) * 0.5f;
    float w0 = tan_scale * (-t_cube + (t_sq + t_sq) - t);
    float w1 = (2.0f - tan_scale) * t_cube + (tan_scale - 3.0f) * t_sq + 1.0f;
    float w2 = (tan_scale - 2.0f) * t_cube + (3.0f - (tan_scale + tan_scale)) * t_sq + tan_scale * t;
    float w3 = tan_scale * (t_cube - t_sq);

    out[0] = w0 * pts[0] + w1 * pts[3] + w2 * pts[6] + w3 * pts[9];
    out[1] = w0 * pts[1] + w1 * pts[4] + w2 * pts[7] + w3 * pts[10];
    out[2] = w0 * pts[2] + w1 * pts[5] + w2 * pts[8] + w3 * pts[11];
}

/* mtx 0: current matrix. */
/* 0x00178cc0 */
void matrix_unit_set(int mtx)
{
    if (mtx == 0)
        mtx = ulMatrixCur;
    vu0_matrix_identity(mtx);
}

/* No translation. */
/* 0x00178cd8 */
void matrix_apply_vectors(float *src, float *dst, int n)
{
    if (n > 0) {
        void *ctx = ulMatrixCur;

        do {
            ulMatrixApplyVector(ctx, src, dst);
            src += 3;
            dst += 3;
        } while (--n != 0);
    }
}

/* 0x00178d48 */
void matrix_apply_points(float *src, float *dst, int n)
{
    if (n > 0) {
        void *ctx = ulMatrixCur;

        do {
            ulMatrixApplyPoint(ctx, src, dst);
            src += 3;
            dst += 3;
        } while (--n != 0);
    }
}

/* Point-and-direction pairs. */
/* 0x00178db8 */
void matrix_apply_pairs(float *src, float *dst, int n)
{
    if (n > 0) {
        void *ctx = ulMatrixCur;

        do {
            ulMatrixApplyPoint(ctx, src, dst);
            ulMatrixApplyVector(ctx, src + 3, dst + 3);
            src += 6;
            dst += 6;
        } while (--n != 0);
    }
}

/* out = qa * qb. */
/* 0x00178e38 */
void quat_multiply(float *qa, float *qb, float *out)
{
    float ax = qa[0];
    float ay = qa[1];
    float az = qa[2];
    float aw = qa[3];
    float bx = qb[0];
    float by = qb[1];
    float bz = qb[2];
    float bw = qb[3];
    float x;
    float y;
    float z;
    float w;

    x = ax * bx - ay * by - az * bz - aw * bw;
    y = ay * bx + ax * by - aw * bz + az * bw;
    z = az * bx + aw * by + ax * bz - ay * bw;
    w = aw * bx - az * by + ay * bz + ax * bw;
    out[0] = x;
    out[1] = y;
    out[2] = z;
    out[3] = w;
}

/* Shorter way round (to flipped when they point apart), normalised. */
/* 0x00178ee0 */
void quat_nlerp(float *from, float *to, float t, float *out)
{
    float ax = from[0];
    float aw = from[3];
    float ay = from[1];
    float az = from[2];
    float bx = to[0];
    float by = to[1];
    float bz = to[2];
    float bw = to[3];
    float dot = ax * bx + ay * by + az * bz + aw * bw;
    float u = t;
    float inv;

    t = 1.0f - u;
    if (dot > 0.0f) {
        ax = ax * t + bx * u;
        ay = ay * t + by * u;
        az = az * t + bz * u;
        aw = aw * t + bw * u;
    } else {
        ax = ax * t - bx * u;
        ay = ay * t - by * u;
        az = az * t - bz * u;
        aw = aw * t - bw * u;
    }
    inv = rsqrtf(ax * ax + ay * ay + az * az + aw * aw);
    out[0] = ax * inv;
    out[1] = ay * inv;
    out[2] = az * inv;
    out[3] = aw * inv;
}

/* 0x00179020 */
void quat_from_euler(int *ang, float *quat)
{
    float sincos[6];

    ulSinCos(ang[0] >> 1, &sincos[0], &sincos[1]);
    ulSinCos(ang[1] >> 1, &sincos[2], &sincos[3]);
    ulSinCos(ang[2] >> 1, &sincos[4], &sincos[5]);
    quat[0] = sincos[1] * sincos[3] * sincos[5] + sincos[0] * sincos[2] * sincos[4];
    quat[1] = -sincos[1] * sincos[2] * sincos[4] + sincos[0] * sincos[3] * sincos[5];
    quat[2] = sincos[3] * sincos[4] * sincos[0] + sincos[2] * sincos[5] * sincos[1];
    quat[3] = -sincos[5] * sincos[0] * sincos[2] + sincos[4] * sincos[1] * sincos[3];
}

/* Unit quaternion; multiplies the current matrix. */
/* 0x00179108 */
void quat_to_matrix(float *quat)
{
    float mtx[16];
    float qw = quat[0];
    float qx = quat[1];
    float qy = quat[2];
    float qz = quat[3];
    float diag = (qw + qw) * qw - 1.0f;

    mtx[3] = 0.0f;
    mtx[7] = 0.0f;
    mtx[11] = 0.0f;
    mtx[12] = 0.0f;
    mtx[13] = 0.0f;
    mtx[14] = 0.0f;
    mtx[15] = 1.0f;
    mtx[0] = (qx + qx) * qx + diag;
    mtx[1] = (qy * qx + qw * qz) * 2.0f;
    mtx[2] = (qz * qx - qw * qy) * 2.0f;
    mtx[4] = (qy * qx - qw * qz) * 2.0f;
    mtx[5] = (qy + qy) * qy + diag;
    mtx[6] = (qz * qy + qw * qx) * 2.0f;
    mtx[8] = (qz * qx + qw * qy) * 2.0f;
    mtx[9] = (qz * qy - qw * qx) * 2.0f;
    mtx[10] = (qz + qz) * qz + diag;
    njMultiMatrix(0, mtx);
}

/* As quat_to_matrix with w apart from vec (x, y, z). */
/* 0x00179200 */
void quat_to_matrix_split(float qw, float *vec)
{
    float mtx[16];
    float qx = vec[0];
    float qy = vec[1];
    float qz = vec[2];
    float diag = (qw + qw) * qw - 1.0f;

    mtx[3] = 0.0f;
    mtx[7] = 0.0f;
    mtx[11] = 0.0f;
    mtx[12] = 0.0f;
    mtx[13] = 0.0f;
    mtx[14] = 0.0f;
    mtx[15] = 1.0f;
    mtx[0] = (qx + qx) * qx + diag;
    mtx[1] = (qy * qx + qw * qz) * 2.0f;
    mtx[2] = (qz * qx - qw * qy) * 2.0f;
    mtx[4] = (qy * qx - qw * qz) * 2.0f;
    mtx[5] = (qy + qy) * qy + diag;
    mtx[6] = (qz * qy + qw * qx) * 2.0f;
    mtx[8] = (qz * qx + qw * qy) * 2.0f;
    mtx[9] = (qz * qy - qw * qx) * 2.0f;
    mtx[10] = (qz + qz) * qz + diag;
    njMultiMatrix(0, mtx);
}
