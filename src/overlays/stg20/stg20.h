/* Tutorial stage: shared declarations. */

#ifndef REZ_STG20_H
#define REZ_STG20_H

#include "types_common.h"


#include "types_overlays.h"

#include "debug.h"
#include "debugdraw.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "player.h"
#include "player_camera.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "stage.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"

/* 312 bytes */
typedef struct NulWork {
    char pad_00[8];
    Vec3 pos;                   /* 0x08 */
    int ang[3];                 /* 0x14 */
    Vec3 vel;                   /* 0x20 */
    ModelPart body;                /* 0x2c */
    ModelPart glow;                /* 0x88 */
    float fade;                 /* 0xe4 */
    LockObj lock;                  /* 0xe8 */
    int fx;                     /* 0x128 */
    int fx_op;                  /* 0x12c */
    int move;                   /* 0x130: frames left to move */
    int delay;                  /* 0x134: frames left before firing */
} NulWork;

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

static inline void i3_copy(int *dst, int *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

extern int ulMatrixPush(void);
extern void *ulMatrixCur;
extern struct GwVarNode *gw_varedit_root;

#endif /* REZ_STG20_H */
