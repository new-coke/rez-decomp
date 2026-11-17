/* Trance Mission stage: shared declarations, enemy and generator works and records. */

#ifndef REZ_STG23_H
#define REZ_STG23_H

#include "types_common.h"


#include "types_overlays.h"

#include "debug.h"
#include "anim.h"
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
#include "nj_system.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_midise.h"
#include "stage.h"
#include "task.h"
#include "texforce.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "mathdef.h"

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

/* sine and cosine in two float scalars */
#define ROTS_Z(ang, s, c, x, y)     do {         float rot_u = x, rot_v = y;         ulSinCos(ang, &s, &c);         x = rot_u * c - rot_v * s;         y = rot_u * s + rot_v * c;     } while (0)

#define ROTS_X(ang, s, c, y, z)     do {         float rot_u = y, rot_v = z;         ulSinCos(ang, &s, &c);         y = rot_u * c - rot_v * s;         z = rot_u * s + rot_v * c;     } while (0)
#define ROTS_Y(ang, s, c, x, z)     do {         float rot_u = x, rot_v = z;         ulSinCos(ang, &s, &c);         x = rot_u * c + rot_v * s;         z = -rot_u * s + rot_v * c;     } while (0)

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

static inline void i3_set(int *dst, int x, int y, int z)
{
    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
}

static inline void v3_neg(Vec3 *dst, Vec3 *src)
{
    dst->x = -src->x;
    dst->y = -src->y;
    dst->z = -src->z;
}

static inline void i3_add(int *dst, int *src)
{
    dst[0] += src[0];
    dst[1] += src[1];
    dst[2] += src[2];
}

static inline void i3_scale(int *v, float k)
{
    v[0] = v[0] * k;
    v[1] = v[1] * k;
    v[2] = v[2] * k;
}

extern unsigned char kbd_key;

extern int ulMatrixPush(void);
extern void *ulMatrixCur;
extern struct GwVarNode *gw_varedit_root;

/* type enemy placement */
typedef struct TypeRec {
    char pad_00[44];
    float param1;               /* 0x2c */
    char pad_30[8];             /* 0x34 holds the Sphinx sweep length (frames) */
} TypeRec;

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

extern void *stage_textures;
extern void *stage_models;

/* step within a state; next-frame request, -1 = none */
typedef enum { EN_DOM_STEP_START } EnDomStep;

typedef enum { EN_DOM_NSTEP_NONE = -1 } EnDomNextStep;

/* en_dom, en_gufu, en_uni, en_zaku; 112 bytes beyond the object */
typedef struct EnDomWork {
    Vec3 *pos;
    Vec3 *vel;                  /* 0x04 */
    int *ang;                   /* 0x08 */
    Vec3 *scale;                /* 0x0c */
    int phase;                  /* 0x10: the move table's index */
    int next;                   /* 0x14: the phase for the next frame */
    EnDomStep step;
    EnDomNextStep next_step;
    int model;                  /* 0x20 */
    int anim;                   /* 0x24 */
    float rgba[4];              /* 0x28 */
    unsigned int color;         /* 0x38 */
    Vec3 drift;                 /* 0x3c: after the move */
    int spin[3];                /* 0x48 */
    Vec3 swing;                 /* 0x54: the swing offset added last frame */
    float swing_t;              /* 0x60: advanced by spd_swing */
    char pad_64[4];
    float frame;                /* 0x68 */
    float frames;               /* 0x6c */
} EnDomWork;

/* en_dom, en_gufu, en_uni, en_zaku: 8 words after name and label */
typedef struct EnDomParams {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    int score;                  /* 0x0c */
    float spd_swing;            /* 0x10: wobble time step */
    float r_swing;              /* 0x14: its amplitude */
    float ang_spd_rotate_z;     /* 0x18: degrees per second */
    float frm_escape_anticipate; /* 0x1c */
    float frm_escape;           /* 0x20 */
    int test;                   /* 0x24: prints the object's state */
} EnDomParams;

/* en_uni, en_zaku: 6 words after name and label */
typedef struct EnUniParams {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    int score;                  /* 0x0c */
    float spd_swing;            /* 0x10 */
    float r_swing;              /* 0x14 */
    float ang_spd_rotate_z;     /* 0x18 */
    int test;                   /* 0x1c */
} EnUniParams;

/* filled by the generators; the object's creation argument */
typedef struct EnPlacement {
    char pad_00[4];
    Vec3 pos;                   /* 0x04 */
    int ang[3];                 /* 0x10 */
    Vec3 dir;                   /* 0x1c: the object's velocity */
    float scene;                /* 0x28: first placement value (en_dom generator: its character) */
    float time;                 /* 0x2c: the second ('drift start.frm' or 'frm_life') */
    float param2;                  /* 0x30: 'swing_r.spd' (wara), 'frm_cycle' (zaku) */
    float param3;                  /* 0x34: 'generator' (wara), 'dst_cycle' (zaku) */
    float param4;                  /* 0x38: 'generator' (uni, zaku), 'num_once' (wara) */
    float param5;                  /* 0x3c: 'generator' (gufu), 'rad_swing' (wara) */
} EnPlacement;

/* own placement, the one filled per object, then counters */
typedef struct EnGeneratorWork {
    EnPlacement self;            /* 64 bytes */
    EnPlacement arg;             /* 0x40: the next object's placement */
    int count;                  /* 0x80: objects left */
    unsigned int mode;          /* 0x84: 1 position, 4 roll, 2 and 8 more, 0x10 all at once */
    float timer;                /* 0x88: frames to the next object */
} EnGeneratorWork;

struct EnPlacement;

#endif /* REZ_STG23_H */
