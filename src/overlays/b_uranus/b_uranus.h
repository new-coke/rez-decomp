/* Uranus boss overlay: shared declarations. */

#ifndef REZ_B_URANUS_H
#define REZ_B_URANUS_H

struct UranusObj;
struct UranusPartWork;
struct UranusMapWork;

#include "types_common.h"


#include "mathdef.h"
#include "types_overlays.h"
#include "anim.h"
#include "callback.h"
#include "camera.h"
#include "camera_demo.h"
#include "color.h"
#include "config.h"
#include "console.h"
#include "debug.h"
#include "enemy_status.h"
#include "fog.h"
#include "game.h"
#include "gw_varedit.h"
#include "heap.h"
#include "item.h"
#include "lens.h"
#include "libc.h"
#include "light.h"
#include "line_batch.h"
#include "lockon.h"
#include "map_manager.h"
#include "math_util.h"
#include "model.h"
#include "nj_cnk_vu1.h"
#include "nj_matrix.h"
#include "nj_strip.h"
#include "node_draw_state.h"
#include "pad_vib.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "task.h"
#include "texforce.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"

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
        int a_ = ang; \
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

/* ROT_Z with the sine and cosine in two scalars */
#define ROTS_Z(ang, s, c, x, y) \
    do { \
        float rot_u = x, rot_v = y; \
        ulSinCos(ang, &s, &c); \
        x = rot_u * c - rot_v * s; \
        y = rot_u * s + rot_v * c; \
    } while (0)

#define ROTS_ZA(ang, s, c, x, y) \
    do { \
        int a_ = ang; \
        float rot_u = x, rot_v = y; \
        ulSinCos(a_, &s, &c); \
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

/* uranus_nodes pool entry: 512 nodes of 0x20 bytes */
typedef struct UranusNode {
    struct UranusNode *parent;
    struct UranusNode *next;
    struct UranusNode *child;
    int data;                   /* the Vec3 the node positions */
    int flags;
    int obj;                    /* the UranusObj bound to it */
    int index;
    int used;
} UranusNode;

enum {
    BEH_HOME,
    BEH_WANDER,
    BEH_DRIFT,
    BEH_CENTRE,
    BEH_FOLLOW,
    BEH_AVOID,
    BEH_PUSH,
    BEH_WALL
};

/* steering behaviour, 0x40 bytes */
typedef struct UranusBeh {
    void (*fn)(struct UranusPartWork *pm, struct UranusObj *obj, struct UranusBeh *beh);
    float weight;
    int on;                     /* 1 = start, 2 = running */
    Vec3 push;                  /* wander: its heading; wall: the push off a wall; push: the kicks */
    float ease;                 /* home: eases from 0 towards 1 */
    char pad_1c[12];
    int timer;                  /* 0x28: frames left of the wall push */
    char pad_2c[20];
} UranusBeh;

/* one of the body's 200 cubes, 0x300 bytes */
typedef struct UranusObj {
    UranusBeh beh[8];
    UranusNode *node[2];        /* its node in each form slot */
    struct UranusPartWork *owner;
    void *task;
    struct UranusObj *parent;   /* the object this one follows or avoids */
    char pad_214[8];
    int index;                  /* 0x21c */
    int flags;                  /* 1 live, 8 core, 0x10 new this frame, 0x20 glowing, 0x40 glow on, 0x100 hit */
    int lvl;                    /* size 0..2; below 2 it splits when shot */
    char pad_228[4];
    int hp;                     /* 0x22c */
    int wait;
    float weight;
    float r;
    float glow;
    LockObj lock;
    Vec3 pos;
    Vec3 vel;
    Vec3 base_pos;
    Vec3 vel0;
    Vec3 hit_pos;
    Vec3 hit_dir;
    Vec3 spin_vel;
    Vec3 spin;                  /* degrees added to ang each frame */
    Vec3 spin_to;
    int ang[3];
    char pad_2f8[8];
} UranusObj;

/* fog fade, 0x2c bytes */
typedef struct VenusFogWork {
    int count;
    int total;
    char pad_08[4];
    BossFog from;               /* 0x0c */
    BossFog to;
} VenusFogWork;

enum {
    FORM_MODEL,
    FORM_SNAKE,
    FORM_ORBIT,
    FORM_ORBIT2,
    FORM_PIT,
    FORM_WALL,
    FORM_MODEL2
};

/* form layout: the eight behaviour weights and eight more, 0x40 bytes */
typedef struct UranusPmLayout {
    float wgts[16];
} UranusPmLayout;

/* boss main task, 0x184c bytes */
typedef struct UranusWork {
    void *models;
    void *textures;
    int arg0;
    int level;
    int state;                  /* 1 loaded, 2 running */
    int unk_14;                 /* 0x14: its address goes to uranus_map_create, never read */
    char pad_18[20];
    int phase;                  /* 0x2c: 1 loaded, 2 finishing, 3 ended */
    int fin_wait;
    ModelPart pool[64];
    ModelPart *parts[64];
    struct UranusMapWork *map;
    struct UranusPartWork *pm;
    void *tc;
    void *seq;
    char pad_1844[4];
    BossStarCtl *stars;         /* 0x1848 */
} UranusWork;

/* part manager, 0x59c bytes */
typedef struct UranusPartWork {
    void *models;
    void *textures;
    int boss_arg;
    struct UranusWork *boss;
    void *tc;
    struct UranusMapWork *map;
    ModelPart **parts;
    int state;
    int nobjs;
    int wait;
    int wait_max;
    Vec3 *home;                 /* per cube, where the home behaviour pulls it */
    Vec3 *pool;
    Vec3 *slot_pts[2][3];       /* per slot: root, link and point position buffers */
    int slot_task[2];
    void (*sw_cb[2])(int);
    int slot_type[2];
    int level_type[2];          /* the form types for this level */
    int level;
    int level_frm;
    float blend;
    float blend_spd;
    int cube_hp[2];
    Vec3 pl_pos;
    Vec3 dir;
    Vec3 drift;
    ModelPart *body;
    float unk_ac;               /* 0xac: set to 0 by uranus_pm_init, never read */
    float unk_b0;               /* 0xb0: set to 1 by uranus_pm_init, never read */
    float unk_b4;               /* 0xb4: the body motion's frame count (uranus_pm_init), never read */
    Vec3 centre;
    int unk_c4;                 /* 0xc4: stepped by cmd_sp_frm modulo 200 each frame, never used */
    Vec3 kill_sum;
    Vec3 kill_ctr;
    int kill_cnt;
    int kill_cnt_prev;
    int hits;
    UranusPmLayout wgt;
    UranusPmLayout wgt_from;
    UranusPmLayout wgt_to;
    int wgt_cnt;
    int wgt_total;
    char pad_1b4[8];
    int col_cnt;                /* 0x1bc */
    int col_total;
    float width_from;
    float width_to;
    float width;
    unsigned int col_from;
    unsigned int col_to;
    unsigned int col;
    char pad_1dc[16];
    int col_kind;               /* 0x1ec */
    int col_kind_to;
    int form;                   /* level % 4: picks the cube models and sounds */
    char pad_1f8[804];
    UranusObj *objs;            /* 0x51c */
    UranusObj *core;
    int cube_life;
    int frm;
    int gauge;
    int seq_frm;
    int mode;
    int sw_wait;
    int sw_delay;
    int sw_req[2];
    int cur;
    int live;
    int live_a;
    int live_b;
    int live_ab;
    UranusNode *node[2];
    void *unk_564;              /* 0x564: a task finished by uranus_pm_reset; nothing sets it */
    float map_spd;
    int end;                    /* set when the body scatters; missiles and the laser stop */
    int core_state;
    int core_wait;
    float core_frm;
    float core_frm_max;
    float hit_col[4];
    BossCfKeyWork *hit_fade;
    int dmg_kind;
    int dmg_score;
} UranusPartWork;

static inline void v3_addto(Vec3 *d, Vec3 *s)
{
    d->x = s->x + d->x;
    d->y = s->y + d->y;
    d->z = s->z + d->z;
}

/* ray: point and direction, 24 bytes */
typedef struct UranusPtDir {
    Vec3 p;
    Vec3 dir;
} UranusPtDir;

#endif /* REZ_B_URANUS_H */
