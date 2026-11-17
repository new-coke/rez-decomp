/* Area 4 overlay: shared declarations. */
#ifndef REZ_STG04_H
#define REZ_STG04_H

/* allocation tag line numbers differ by build */
#ifdef VERSION_US
#define SRC_LINE(early, us) (us)
#else
#define SRC_LINE(early, us) (early)
#endif

#include "types_common.h"


#include "types_overlays.h"

#define PART_MOTION(p) ((ModelPart *)(p))

#include "debug.h"
#include "anim.h"
#include "approach.h"
#include "billboard.h"
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
#include "nj_cnk_vu1.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "stage.h"
#include "system.h"
#include "task.h"
#include "texforce.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
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

extern NjClip cam_far;

extern void *stage_textures;
extern void *stage_models;

/* 564 bytes: wire-break effect fading out */
typedef struct BalloonBurstWork {
    char pad_00[24];
    unsigned int color;         /* 0x18: alpha in the top byte */
    int wb[132];                /* 0x1c: the wire-break state; word 0 is a heap block */
    float t;                    /* 0x22c: frames shown */
    char pad_230[4];
} BalloonBurstWork;

typedef struct EnFlashArg {
    int v;
    float frm, scl;
} EnFlashArg;

/* 168 bytes */
typedef struct BalloonCtrlbarWork {
    char pad_00[8];
    int prev;                   /* 0x08: the state of the last frame */
    int state;                  /* 0x0c: index into balloon_ctrlbar_states */
    int cnt;                    /* 0x10 */
    int cnt_end;                /* 0x14 */
    ModelPart part;                /* 0x18 */
    Vec3 pos;                   /* 0x74 */
    int ang[3];                 /* 0x80 */
    Vec3 vel;                   /* 0x8c */
    float fall;                 /* 0x98: the fall speed */
    float turn;                 /* 0x9c: ang3_approach's rate */
    char pad_a0[8];
} BalloonCtrlbarWork;


/* closing enemy_status_event (3), set on open (op 2) */
typedef enum { ENB_BALLOON_FX_NONE = 0, ENB_BALLOON_FX_CLOSE = 3 } EnbBalloonFxOp;

/* 5248 bytes */
typedef struct EnbBalloonWork {
    char pad_00[4];
    void *textures;             /* 0x04: the texture list */
    int state;                  /* 0x08: index into enb_balloon_states */
    int next;                   /* 0x0c */
    unsigned int step;          /* 0x10 */
    int next_step;              /* 0x14 */
    Vec3 fx_pos;                /* 0x18: the core's position */
    int ang[3];                 /* 0x24 */
    Vec3 vel;                   /* 0x30 */
    Vec3 pts[44];               /* 0x3c: the fragments */
    int angs[44][3];            /* 0x24c */
    Vec3 dirs[44];              /* 0x45c: their velocities */
    float delay[44];            /* 0x66c: mode b's frames before they slow down */
    ModelPart part;                /* 0x71c */
    LockObj lock;                  /* 0x778 */
    LockObj locks[44];             /* 0x7b8 */
    void *sub;                  /* 0x12b8 */
    Vec3 lpos;                  /* 0x12bc: the link's position */
    Vec3 lscale;                /* 0x12c8: the link's scale (sca_fire) */
    char pad_12d4[8];
    int flags;                  /* 0x12dc: bit 0 = released */
    unsigned char st[44];       /* 0x12e0: bit 0 = fragment gone */
    int n;                      /* 0x130c: fragments left */
    int fx_op;                  /* 0x1310 */
    EnbBalloonFxOp fx_ops[44];  /* 0x1314: each fragment's closing event */
    int fx;                     /* 0x13c4 */
    int fxs[44];                /* 0x13c8 */
    float unk_1478;             /* 0x1478: set to 1.0 on entering enb_balloon_state_a/b, never read */
    char pad_147c[4];
} EnbBalloonWork;

/* 460 bytes */
typedef struct EnbHopperWork {
    char pad_00[8];
    int state;                  /* 0x08: index into enb_hopper_states */
    int next;                   /* 0x0c */
    unsigned int step;          /* 0x10 */
    int next_step;              /* 0x14 */
    ModelPart body;                   /* 0x18: body (models 131/132) */
    ModelPart glow;                   /* 0x74: models 134/135, drawn at the body's frame */
    ModelPart outline;                   /* 0xd0: models 129/130 */
    int wire;                   /* 0x12c: model 136, the colour source */
    Vec3 fx_pos;                /* 0x130: the position */
    int ang[3];                 /* 0x13c */
    Vec3 dir;                   /* 0x148 */
    Vec3 lpos;                  /* 0x154: enb_hopper_calc_points's first point (the link's position) */
    Vec3 scale;                 /* 0x160: the link's scale (test_float) */
    Vec3 tpos;                  /* 0x16c: enb_hopper_calc_points's second point (the trail's position) */
    char pad_178[4];
    unsigned int col;           /* 0x17c: col_wire with the alpha of enb_hopper_wire_color */
    LockObj lock;                  /* 0x180 */
    int fx_op;                  /* 0x1c0 */
    int fx;                     /* 0x1c4 */
    char pad_1c8[4];
} EnbHopperWork;

/* 28 bytes */
typedef struct EnHopperCircleWork {
    Vec3 pos;
    float r;                    /* 0x0c */
    int alpha;                  /* 0x10 */
    float t;                    /* 0x14: frames shown */
    float end;                  /* 0x18 */
} EnHopperCircleWork;

typedef enum { MARIMO_BRK_KIND_FIRST } MarimoBrkKind;

typedef struct EnMarimoBreakArg {
    MarimoBrkKind kind;
    Vec3 *pos;                  /* 0x04 */
    int *ang;                   /* 0x08 */
    LockObj *lock;                 /* 0x0c */
    Vec3 *target;               /* 0x10 */
    float time;                 /* 0x14 */
} EnMarimoBreakArg;

/* 176 bytes */
typedef struct EnMarimoBreakWork {
    MarimoBrkKind kind;          /* index into en_marimo_break_kinds; 0..3 move and fade */
    Vec3 pos;                   /* 0x04 */
    int ang[3];                 /* 0x10 */
    float rgba[4];              /* 0x1c: colour offset (alpha fades) */
    LockObj *lock;                 /* 0x2c */
    Vec3 *target;               /* 0x30 */
    ModelPart part;                /* 0x34 */
    void *model;                /* 0x90 */
    void *clip;                 /* 0x94: 0 without */
    void *wire;                 /* 0x98 */
    int cache;                  /* 0x9c: model_pnb_get_wire's part cache */
    float time;                 /* 0xa0 */
    int flag;                   /* 0xa4 */
    float t;                    /* 0xa8 */
    float end;                  /* 0xac */
} EnMarimoBreakWork;

/* placement as en_perfectwall_manager passes it on, 64 bytes */

/* leading words of en_hopper's placement */
typedef struct EnHopperPlace {
    unsigned char se;           /* rhythm SE slot */
    char pad_01[1];
    short link;                 /* 0x02: -1 */
} EnHopperPlace;

extern void debug_set_color(unsigned int c);

static inline void i3_scale(int *v, float k)
{
    v[0] *= k;
    v[1] *= k;
    v[2] *= k;
}

static inline void i3_add(int *dst, int *src)
{
    dst[0] += src[0];
    dst[1] += src[1];
    dst[2] += src[2];
}

/* placement for en_marimo_spawn_hoppers, 64 bytes */
typedef struct EnMarimoPlace {
    unsigned char se;
    char unk_01;                /* 0x01: placement header byte after kind; set to 0 by every spawner, never read */
    short link;                 /* 0x02 */
    Vec3 pos;                   /* 0x04 */
    int ang[3];                 /* 0x10 */
    Vec3 scale;                 /* 0x1c */
    float prm[6];               /* 0x28 */
} EnMarimoPlace;

static inline void i3_subfrom(int *dst, int *src)
{
    dst[0] -= src[0];
    dst[1] -= src[1];
    dst[2] -= src[2];
}

#endif /* REZ_STG04_H */
