/* Area 2 overlay: shared declarations. */
#ifndef REZ_STG02_H
#define REZ_STG02_H

#include "types_common.h"


#include "types_overlays.h"

#include "debug.h"
#include "anim.h"
#include "approach.h"
#include "callback.h"
#include "camera.h"
#include "color.h"
#include "debugdraw.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "fog.h"
#include "game.h"
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
        float rot_u = x, rot_v = y; \
        ulSinCos(ang, &sc[0], &sc[1]); \
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

/* as en_vishnu_string_init fills it */
typedef struct EnVishnuStringView {
    int owner, hand, idx;
    float len;                  /* 0x0c */
    Vec3 vel;                   /* 0x10 */
} EnVishnuStringView;

extern EShotSet en_shiva_sets[2];

typedef struct EnVishnuFlashView {
    Vec3 pos;
    float r;                    /* 0x0c */
    int alpha;                  /* 0x10 */
    char pad_14[8];
} EnVishnuFlashView;

/* as its hand sees it */
typedef struct EnbVishnu {
    int phase;
    int next;                  /* 0x04 */
    int step;
    int next_step;                  /* 0x0c */
    char pad_10[92];
    Vec3 base;                     /* 0x6c */
    char pad_78[12];
    Vec3 pos;                     /* 0x84 */
    char pad_90[76];
    float len;                 /* 0xdc */
} EnbVishnu;

static inline void i3_add(int *dst, int *src)
{
    dst[0] += src[0];
    dst[1] += src[1];
    dst[2] += src[2];
}

/* en_garuda_nest -> en_garuda_create, 64 bytes */
typedef struct EnGarudaArg {
    char fx_slot;
    char unk_01;                /* 0x01: placement header byte after kind; set to 0 by every spawner, never read */
    short link;                   /* 0x02 */
    Vec3 pos;                   /* 0x04 */
    int ang[3];                 /* 0x10 */
    char pad_1c[12];
    float pattern;                 /* 0x28 */
    float shot;                  /* 0x2c */
    Vec3 vec;                   /* 0x30 */
    char pad_3c[4];
} EnGarudaArg;

static inline void i3_mul(int *dst, int *src, float scale)
{
    dst[0] = src[0] * scale;
    dst[1] = src[1] * scale;
    dst[2] = src[2] * scale;
}

static inline void i3_scale(int *v, float k)
{
    v[0] = v[0] * k;
    v[1] = v[1] * k;
    v[2] = v[2] * k;
}

extern int en_shiva_level;

/* 368 bytes */
typedef enum { VH_STEP_START } EnVishnuHandStep;

typedef enum { VH_NSTEP_NONE = -1 } EnVishnuHandNextStep;

typedef struct EnVishnuHandWork {
    char pad_00[8];
    int phase;                  /* 0x08 */
    int next;                   /* 0x0c */
    EnVishnuHandStep step;                /* 0x10 */
    EnVishnuHandNextStep next_step;       /* 0x14 */
    ModelPart body;                /* 0x18 */
    Vec3 pos;                   /* 0x74 */
    int ang[3];                 /* 0x80 */
    Vec3 rel;                   /* 0x8c: from the owner when detached */
    void *owner;                /* 0x98: the en_vishnu object */
    void *circle;               /* 0x9c: the charge flash (en_vishnu_flash) */
    void *fx;                   /* 0xa0: the laser (enb_vishnu) */
    int score;                    /* 0xa4 */
    Vec3 vel;                   /* 0xa8 */
    int spin[3];                /* 0xb4 */
    LockObj lock;                  /* 0xc0 */
    Vec3 popup_pos;                  /* 0x100 */
    Vec3 lock_pos;                  /* 0x10c */
    Vec3 flash_pos;                  /* 0x118 */
    Vec3 string_pos;                  /* 0x124 */
    Vec3 wob;                   /* 0x130: the wobble offset */
    float wt;                   /* 0x13c */
    float freq[3];              /* 0x140 */
    float open_frm;                 /* 0x14c */
    float flash_frm;                 /* 0x150 */
    unsigned int ev;            /* 0x154 */
    unsigned int slot;          /* 0x158 */
    int idx;                    /* 0x15c */
    int flags;                  /* 0x160 */
    float cnt;                  /* 0x164: frames */
    float cnt_end;              /* 0x168 */
    float t;                    /* 0x16c */
} EnVishnuHandWork;

static inline void i3_sub(int *dst, int *src)
{
    dst[0] -= src[0];
    dst[1] -= src[1];
    dst[2] -= src[2];
}

static inline void v3_neg(Vec3 *dst, Vec3 *src)
{
    dst->x = -src->x;
    dst->y = -src->y;
    dst->z = -src->z;
}

typedef struct EnShivaRec {    /* 104 bytes, one per level */
    char *name;
    char *label;                /* 0x04 */
    int hp_core;                   /* 0x08: lock hp of the top (lock_c) */
    int hp_front;                   /* 0x0c: lock 8 */
    int hp_tail;                   /* 0x10: lock 9 */
    int hp_door;                   /* 0x14: locks b */
    int hp_tray;                   /* 0x18: locks a */
    int score_core;                   /* 0x1c: score of the top */
    int score_front;                   /* 0x20: lock 8 */
    int score_tail;                   /* 0x24: lock 9 */
    int score_door;
    int score_tray;                   /* 0x2c: locks a / b */
    int score_bonus;                   /* 0x30: score event when everything was shot */
    int frm_shot_interval;
    float acc_escape;                  /* 0x38: x 1/3600 */
    float spd_slide_max;                  /* 0x3c: x 1/60 */
    float acc_slide;                  /* 0x40: x 1/3600 */
    float dst_slide_div;                  /* 0x44 */
    float dst_slide_max1;                  /* 0x48 */
    float dst_slide_max2;                  /* 0x4c */
    float spd_ine_back;                  /* 0x50: x 1/60 */
    float pow_shot;                  /* 0x54 */
    float pow_laser;                  /* 0x58: push on a hit */
    float dst_clip_explode;                  /* 0x5c */
    float dst_clip_explode2;                  /* 0x60 */
    float dst_clip;                  /* 0x64 */
} EnShivaRec;

#define EN_SHIVA_REC (&((EnShivaRec *)en_shiva_sets)[en_shiva_level])

#endif /* REZ_STG02_H */
