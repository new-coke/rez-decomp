/* Area 3 overlay: shared declarations. */
#ifndef REZ_STG03_H
#define REZ_STG03_H

#include "types_common.h"


#include "types_overlays.h"
#include "anim.h"
#include "approach.h"
#include "camera.h"
#include "color.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "fog.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "line_batch.h"
#include "lockon.h"
#include "map_manager.h"
#include "math_util.h"
#include "model.h"
#include "model_wire_break.h"
#include "nj_cnk_vu1.h"
#include "nj_matrix.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "pass.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "stage.h"
#include "task.h"
#include "texforce.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"

/* ModelPart motion fields as floats, for raw-work access */
typedef struct Part92F {
    char pad_00[60];
    float frame;                /* 0x3c: the motion's current frame */
    float frames;               /* 0x40: its length */
    float speed;                /* 0x44: frames per tick */
} Part92F;

#define P92F(p) ((Part92F *)(p))

#include "mathdef.h"
#include "debug.h"

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

static inline void v3_sub_from(Vec3 *rhs, Vec3 *lhs, Vec3 *dst)
{
    dst->x = lhs->x - rhs->x;
    dst->y = lhs->y - rhs->y;
    dst->z = lhs->z - rhs->z;
}

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

extern int ulMatrixPush(void);
extern void *ulMatrixCur;
extern struct GwVarNode *gw_varedit_root;

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

extern void project_xyz(float x, float y, float z, float *out);
extern float cam_near;

extern NjClip cam_far;

extern void *stage_textures;
extern void *stage_models;
extern void *stage_sequence;

static inline void i3_sub(int *dst, int *lhs, int *rhs)
{
    dst[0] = lhs[0] - rhs[0];
    dst[1] = lhs[1] - rhs[1];
    dst[2] = lhs[2] - rhs[2];
}

#endif /* REZ_STG03_H */
