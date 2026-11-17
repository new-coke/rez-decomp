/* Earth boss overlay: shared declarations. */

#ifndef REZ_B_EARTH_H
#define REZ_B_EARTH_H

#include "types_common.h"


#include "mathdef.h"
#include "types_overlays.h"
#include "anim.h"
#include "callback.h"
#include "camera.h"
#include "color.h"
#include "console.h"
#include "enemy_status.h"
#include "fog.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "light.h"
#include "line_batch.h"
#include "lockon.h"
#include "map_manager.h"
#include "math_util.h"
#include "model.h"
#include "nj_cnk_vu1.h"
#include "nj_math.h"
#include "nj_matrix.h"
#include "nj_strip.h"
#include "node_draw_state.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "screen.h"
#include "sound_beat.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "wipefade.h"
#include "../common/boss_common.h"

static inline void v3_copy(Vec3 *d, Vec3 *s)
{
    d->x = s->x;
    d->y = s->y;
    d->z = s->z;
}

static inline void v3_set(Vec3 *v, float x, float y, float z)
{
    v->x = x;
    v->y = y;
    v->z = z;
}

static inline void v3_add(Vec3 *d, Vec3 *s)
{
    d->x += s->x;
    d->y += s->y;
    d->z += s->z;
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

#define ROTS_Z(ang, s, c, x, y) \
    do { \
        float rot_u = x, rot_v = y; \
        ulSinCos(ang, &s, &c); \
        x = rot_u * c - rot_v * s; \
        y = rot_u * s + rot_v * c; \
    } while (0)

static inline void v3_sub(Vec3 *d, Vec3 *a, Vec3 *b)
{
    d->x = a->x - b->x;
    d->y = a->y - b->y;
    d->z = a->z - b->z;
}

static inline void v3_sum(Vec3 *d, Vec3 *a, Vec3 *b)
{
    d->x = a->x + b->x;
    d->y = a->y + b->y;
    d->z = a->z + b->z;
}

/* d = a + b */
static inline void v3_sum_to(Vec3 *a, Vec3 *b, Vec3 *d)
{
    d->x = a->x + b->x;
    d->y = a->y + b->y;
    d->z = a->z + b->z;
}

static inline void v3_scale(Vec3 *v, float k)
{
    v->x *= k;
    v->y *= k;
    v->z *= k;
}

static inline void v3_mul(Vec3 *d, Vec3 *a, float k)
{
    d->x = a->x * k;
    d->y = a->y * k;
    d->z = a->z * k;
}

static inline void v3_lerp(Vec3 *d, Vec3 *a, Vec3 *b, float k)
{
    d->x = a->x * k + b->x * (1.0f - k);
    d->y = a->y * k + b->y * (1.0f - k);
    d->z = a->z * k + b->z * (1.0f - k);
}

static inline int v3_is_zero(Vec3 *v)
{
    int z = 0;

    if (fabsf(v->x) < 0.01f && fabsf(v->y) < 0.01f && fabsf(v->z) < 0.01f)
        z = 1;
    return z;
}

/* random unit vector */
static inline void v3_rand(Vec3 *v)
{
    do {
        v->x = RAND_FLOAT_A() - 0.5f;
        v->y = RAND_FLOAT_A() - 0.5f;
        v->z = RAND_FLOAT_A() - 0.5f;
    } while (v3_is_zero(v));
    vec3_normalize(v, v);
}

static inline void i3_set(int *d, int x, int y, int z)
{
    d[0] = x;
    d[1] = y;
    d[2] = z;
}

/* v: three ints */
static inline void iv3_set(void *v, int x, int y, int z)
{
    struct { int x, y, z; } *p = v;

    p->x = x;
    p->y = y;
    p->z = z;
}

static inline void i3_copy(int *d, int *s)
{
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

extern ScreenCfg screen_cfg;

extern int ulMatrixPush(void);

#ifdef VERSION_US
#define dc_object_draw_us boss_dc_object_draw
#else
#define dc_object_draw_us dc_object_draw
#endif

extern void *ulMatrixCur;
extern struct GwVarNode *gw_varedit_root;

#include "debug.h"

struct ShotHit;

#define MAX_GE(a, b) ((a) >= (b) ? (a) : (b))

static inline void uv_vtx(float *v, float x, float y, float q, float t, float s, unsigned int c)
{
    *v++ = x;
    *v++ = y;
    *v++ = q;
    *v++ = t;
    *v = s;
    *(unsigned int *)(v + 1) = c;
}

extern unsigned char kbd_state[];

extern float project_point_size(Vec3 *p, Vec3 *out, float *sx, float *sy);

extern CmTexAnimeWork *tex_anime_work;
struct BossV4;

/* inline form of model_draw_box_culled */
static inline void boss_cull_draw(NjCnkModel *model)
{
    unsigned int clip = draw_cull_box(model->r, &model->center.x);
    if (!(clip & 0xff)) {
        if (clip & 0xff00)
            nj_cnk_vu1pkt_draw_clip(model);
        else
            nj_cnk_vu1pkt_draw_noclip(model);
    }
}

#define MTX_TRANS(p) \
    do { \
        Vec3 *p_ = (p); \
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z); \
    } while (0)

static inline void i3_sum(int *d, int *a, int *b)
{
    d[0] = a[0] + b[0];
    d[1] = a[1] + b[1];
    d[2] = a[2] + b[2];
}

/* "Arm" creation argument */
typedef struct BossArmArg {
    Vec3 *pos;
    Vec3 *ofs;                  /* 0x04 */
    Vec3 *dir;                  /* 0x08 */
    int *ang;                   /* 0x0c */
    int *roty;                  /* 0x10 */
    int *cmd;                   /* 0x14: the command cell (arm_command) */
    ModelPart *part;               /* 0x18 */
    void *cb_arg;               /* 0x1c */
    void (*update_cb)(void *w); /* 0x20: run by arm_exec (earth_arm_exec_cb) */
    void (*disp_cb)(void *w);   /* 0x24: run by arm_disp (earth_arm_disp_cb) */
} BossArmArg;

typedef struct BossPartList { ModelPart *p[8]; } BossPartList;
extern char arm_desc[], earth_b_vardef[];

/* "Cube_Light", 72 bytes; also its creation argument */
typedef struct BossCubeLight {
    char pad_00[4];
    void *list;                 /* 0x04: drawn with ulGxSetTextureList */
    Vec3 *pos;                  /* 0x08 */
    int *ang;                   /* 0x0c */
    int ang2[3];                /* 0x10 */
    Vec3 scl;                   /* 0x1c */
    int count;                  /* 0x28 */
    int total;                  /* 0x2c */
    int count0;                 /* 0x30 */
    int state;                  /* 0x34 */
    int tex;                    /* 0x38 */
    float z, dz, r;             /* 0x3c: cube_light_draw_frames's arguments */
} BossCubeLight;

/* bubble emitter, 76 bytes */
typedef struct BossBubbleGen {
    char pad_00[8];
    int state;                  /* 0x08: 0 waiting, 1 emitting */
    int wait;                   /* 0x0c */
    int timer;                  /* 0x10 */
    int end_flag;               /* 0x14: passed to the bubbles */
    int n;                      /* 0x18 */
    int count;                  /* 0x1c: bubbles per burst */
    int left;                   /* 0x20 */
    int period;                 /* 0x24 */
    int timer2;                 /* 0x28 */
    int level;                  /* 0x2c */
    int delay;                  /* 0x30 */
    int side;                   /* 0x34 */
    Vec3 dir;                   /* 0x38 */
    Vec3 *pos;                  /* 0x44 */
    BossMainWork *parent;          /* 0x48 */
} BossBubbleGen;

/* arm_levels entry */
typedef struct BossArmLevel {
    int count; /* launches after the first round */
    int n; /* 0x04: segments per launch */
    float period; /* 0x08: seconds between launches */
    int hp; /* 0x0c: a node's lock-on hits */
} BossArmLevel;

/* arm_slots entry, 220 bytes */
typedef struct BossSlot {
    int used;
    int data[54];
} BossSlot;

#endif /* REZ_B_EARTH_H */
