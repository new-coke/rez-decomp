/* Mars boss overlay: shared declarations. */

#ifndef REZ_B_MARS_H
#define REZ_B_MARS_H

#include "types_common.h"


#include "mathdef.h"
#include "types_overlays.h"
#include "anim.h"
#include "callback.h"
#include "camera.h"
#include "camera_demo.h"
#include "color.h"
#include "console.h"
#include "debug.h"
#include "enemy_status.h"
#include "fog.h"
#include "gw_varedit.h"
#include "heap.h"
#include "item.h"
#include "libc.h"
#include "line_batch.h"
#include "lockon.h"
#include "map_manager.h"
#include "math_util.h"
#include "model.h"
#include "nj_cnk_vu1.h"
#include "nj_matrix.h"
#include "nj_strip.h"
#include "node_draw_state.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
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

static inline void i3_copy(int *d, int *s)
{
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

extern ScreenCfg screen_cfg;

extern int ulMatrixPush(void);
extern void *ulMatrixCur;
extern struct GwVarNode *gw_varedit_root;
extern void matrix_get_or_set(void *m, float *dst);

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

static inline void i3_sub(int *d, int *a, int *b)
{
    d[0] = a[0] - b[0];
    d[1] = a[1] - b[1];
    d[2] = a[2] - b[2];
}

extern CmTexAnimeWork *tex_anime_work;

#ifndef VERSION_US
void tree_pair_draw_posed_culled(WireObj *obj_a, WireObj *obj_b, void *mot, Vec3 *sa, Vec3 *sb, float t);
#else
struct BossPktNode;
#endif

extern float mars_amp_ease;

#define MTX_TRANSF(x, y, z) do { vu0_matrix_translate(ulMatrixCur, x, y, z); } while (0)

/* where the core docks and what it draws, from the boss work */
typedef struct MarsCoreArg {
    void *home;
    void *boss_ang;
    char *unk_08;                       /* 0x08: never set by mars_start */
    char *body;
    char *shell;
    void *parts;
    void *dust_part;
    void *boss;
} MarsCoreArg;

typedef struct MarsWallManArg {      /* 48 bytes, copied from the boss work */
    int params[12];
} MarsWallManArg;

#define MARS_ANG16(a) ((a) >= 0 ? (a) & 0xffff : (a) | 0xffff0000)

#define MARS_EASE(p, px, py, pz) \
    do { \
        float t_ = mars_amp_ease; \
        float s_ = 1.0f - t_; \
        (p)->x = (p)->x * t_ + s_ * (px); \
        (p)->y = (p)->y * t_ + s_ * (py); \
        (p)->z = (p)->z * t_ + s_ * (pz); \
    } while (0)

struct MarsCoreWork;

typedef struct MarsIV {
    int i;
    Vec3 v;
} MarsIV;

/* 8044 bytes */
typedef struct MarsWork {
    void *pack;
    void *textures;
    int arg0;
    Vec3 pl_pos;
    Vec3 pos;
    int ang[3];
    int dang[3];
    IVec3 unk_3c;                       /* 0x3c: zeroed by mars_start, never read */
    IVec3 unk_48;                       /* 0x48: zeroed by mars_start, never read */
    char pad_54[24];
    Vec3 amp;                           /* 0x6c */
    Vec3 face;
    int phase;
    int cycle;
    int phase_next;
    int rounds;
    int fog_id;
    int skip;
    int unk_9c;                         /* 0x9c: cleared by mars_exec and mars_restart, never read */
    int anim_flags;
    int sbm_stop;
    ModelPart pool[64];
    ModelPart *parts[64];
    float wob_ph;
    float wob_spd;
    float roll;
    float ypos;
    float ypos_scl;
    int timer;
    int timer_len;
    int demo_state;
    char pad_18c8[4];
    int unk_18cc;                       /* 0x18cc: zeroed by mars_start, never read */
    char pad_18d0[8];
    float dist;                         /* 0x18d8 */
    int unk_18dc;                       /* 0x18dc: zeroed by mars_exec and mars_start, never read */
    char pad_18e0[4];
    int tree_flag;                      /* 0x18e4 */
    int level;
    int core_open;
    char pad_18f0[24];
    int flip;                           /* 0x1908 */
    char pad_190c[8];
    float open_frm;                     /* 0x1914 */
    float open_to;
    int pair_hidden;                    /* halves of the pair not drawn: 0 none, 1 front, 2 both */
    int open_mot;
    int open_loops;
    int open_mode;
    float open_t;
    float open_len;
    struct MarsCoreWork *core;
    char pad_1938[512];
    MarsIV points[4];                   /* 0x1b38 */
    char pad_1b78[128];
    int fog_len;                        /* 0x1bf8 */
    int fog_t;
    int fog_from;
    int fog_to;
    ModelPart *fog_from_part;
    ModelPart *fog_to_part;
    char pad_1c10[12];
    void *vcc;                          /* 0x1c1c */
    void *flower;
    float vcc_alpha;
    void *sway;
    void *sbm;
    void *attack;
    int fl_mask;
    char pad_1c38[772];
    Vec3 ref_pos;                       /* 0x1f3c */
    int ref_ang[3];
    int debris_bounded;
    int bgm_idx;
    int unk_1f5c;                       /* 0x1f5c: set when the flower eats on the last round */
    char *map;
    BossStarCtl *star;
    int start_wait;
} MarsWork;

#endif /* REZ_B_MARS_H */
