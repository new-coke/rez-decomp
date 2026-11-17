/* Stage 6 overlay: shared declarations. */
#ifndef REZ_STG06_H
#define REZ_STG06_H

#include "types_common.h"


#include "mathdef.h"
#include "types_overlays.h"
#include "callback.h"
#include "debug.h"
#include "enemy_status.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_strip.h"
#include "sound_midiseq.h"
#include "task.h"
#include "ul_math.h"
#include "ul_vu0.h"

static inline void v3_copy(Vec3 *dst, Vec3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

static inline void v3_set(Vec3 *v, float x, float y, float z)
{
    v->x = x;
    v->y = y;
    v->z = z;
}

static inline void v3_add(Vec3 *dst, Vec3 *src)
{
    dst->x += src->x;
    dst->y += src->y;
    dst->z += src->z;
}

/* sc receives the sine and cosine */
#define ROT_Z(ang, sc, x, y) \
    do { \
        int a_ = ang; \
        float rot_u = x, rot_v = y; \
        ulSinCos(a_, &sc[0], &sc[1]); \
        x = rot_u * sc[1] - rot_v * sc[0]; \
        y = rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

#define ROT_X(ang, sc, y, z) \
    do { \
        float rot_u = y, rot_v = z; \
        ulSinCos(ang, &sc[0], &sc[1]); \
        y = rot_u * sc[1] - rot_v * sc[0]; \
        z = rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

#define ROT_Y(ang, sc, x, z) \
    do { \
        float rot_u = x, rot_v = z; \
        ulSinCos(ang, &sc[0], &sc[1]); \
        x = rot_u * sc[1] + rot_v * sc[0]; \
        z = -rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

static inline void v3_sub(Vec3 *dst, Vec3 *lhs, Vec3 *rhs)
{
    dst->x = lhs->x - rhs->x;
    dst->y = lhs->y - rhs->y;
    dst->z = lhs->z - rhs->z;
}

static inline void v3_sum(Vec3 *dst, Vec3 *lhs, Vec3 *rhs)
{
    dst->x = lhs->x + rhs->x;
    dst->y = lhs->y + rhs->y;
    dst->z = lhs->z + rhs->z;
}

static inline void v3_scale(Vec3 *v, float k)
{
    v->x *= k;
    v->y *= k;
    v->z *= k;
}

static inline void v3_mul(Vec3 *dst, Vec3 *src, float scale)
{
    dst->x = src->x * scale;
    dst->y = src->y * scale;
    dst->z = src->z * scale;
}

static inline int v3_is_zero(Vec3 *v)
{
    int z = 0;

    if (fabsf(v->x) < 0.01f && fabsf(v->y) < 0.01f && fabsf(v->z) < 0.01f)
        z = 1;
    return z;
}

static inline void v3_rand(Vec3 *v)
{
    do {
        v->x = RAND_FLOAT_A() - 0.5f;
        v->y = RAND_FLOAT_A() - 0.5f;
        v->z = RAND_FLOAT_A() - 0.5f;
    } while (v3_is_zero(v));
    vec3_normalize(v, v);
}

static inline void i3_set(int *dst, int x, int y, int z)
{
    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
}

static inline void i3_copy(int *dst, int *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

extern unsigned char kbd_key;

extern void *ulMatrixCur;

struct ShotHit;

#define MAX_GE(a, b) ((a) >= (b) ? (a) : (b))

static inline void uv_vtx(float *v, float x, float y, float q, float t, float s, unsigned int col)
{
    *v++ = x;
    *v++ = y;
    *v++ = q;
    *v++ = t;
    *v = s;
    *(unsigned int *)(v + 1) = col;
}

#endif /* REZ_STG06_H */
