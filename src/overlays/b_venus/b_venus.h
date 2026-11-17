/* Venus boss overlay: shared declarations. */

#ifndef REZ_B_VENUS_H
#define REZ_B_VENUS_H

/* allocation tag line numbers of the original source */
#ifdef VERSION_US
#define SRC_LINE(early, us) (us)
#else
#define SRC_LINE(early, us) (early)
#endif

#include "types_common.h"


#include "mathdef.h"
#include "types_overlays.h"
#include "anim.h"
#include "callback.h"
#include "camera.h"
#include "color.h"
#include "console.h"
#include "debug.h"
#include "enemy_status.h"
#include "file.h"
#include "fog.h"
#include "gw_varedit.h"
#include "heap.h"
#include "item.h"
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

#define ROTS_Z(ang, s, c, x, y) \
    do { \
        float rot_u = x, rot_v = y; \
        ulSinCos(ang, &s, &c); \
        x = rot_u * c - rot_v * s; \
        y = rot_u * s + rot_v * c; \
    } while (0)

#define ROTS_X(ang, s, c, y, z) \
    do { \
        float rot_u = y, rot_v = z; \
        ulSinCos(ang, &s, &c); \
        y = rot_u * c - rot_v * s; \
        z = rot_u * s + rot_v * c; \
    } while (0)

#define ROTS_Y(ang, s, c, x, z) \
    do { \
        float rot_u = x, rot_v = z; \
        ulSinCos(ang, &s, &c); \
        x = rot_u * c + rot_v * s; \
        z = -rot_u * s + rot_v * c; \
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

#ifdef VERSION_US
#define dc_object_draw_us boss_dc_object_draw
#else
#define dc_object_draw_us dc_object_draw
#endif

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

struct BossV4;

extern int system_fps;

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

/* fire pod, one of 16, 96 bytes */
typedef struct VenusFbManPod {
    Vec3 pos;
    int ang[3];                 /* 0x0c: [1] the pod's place on the circle */
    LockObj lock;                  /* 0x18: hits from cmd_fpod_hp */
    int alive;                 /* 0x58: set to 1 */
    int unk_5c;                 /* 0x5c: cleared by venus_fb_man_set_on and venus_fb_man_revive, never read */
} VenusFbManPod;

/* fire pod ring: 16 lockable pods circling the core and firing; see venus_fb_man_set_on */
typedef struct VenusFbManWork {
    char pad_00[4];
    int textures;               /* 0x04: the texture list */
    int models;                 /* 0x08: the model bank (venus_fb_man_create) */
    int pos;                    /* 0x0c: Vec3 * the pods circle */
    int ang;                    /* 0x10: int * to the owner's angles */
    int dang;                   /* 0x14: the owner's yaw change this frame */
    int ang_y;                  /* 0x18: the owner's yaw last frame */
    int dang_prev;              /* 0x1c: the last yaw change kept */
    int parent;                 /* 0x20: the creating task */
    VenusFbManPod r[16];            /* 0x24 */
    int n;                      /* 0x624: 16 when on */
    int arg;                    /* 0x628 */
    int on;                     /* 0x62c */
    int stopped;                /* 0x630: set to 1 by venus_fb_man_stop */
    int bursts;                /* 0x634 */
    int burst_mode;                /* 0x638 */
} VenusFbManWork;

/*
 * plate_set_data entry, 20 bytes: seconds to the next set, forward and side distance from the
 * player, grid size. The ball code passes its collision cylinders through the same type.
 */
typedef struct VenusParam {
    float x, y, z;
    int data_x;                      /* 0x0c */
    int data_z;                      /* 0x10 */
} VenusParam;

/* Thunder line creation record, named by the 'cm_thl' variables cm_thl_rec_init copies in. */
typedef struct ThlRec {
    Vec3 *from;
    Vec3 *to;                   /* 0x04 */
    int tex;                    /* 0x08 */
    int num;                    /* 0x0c: points */
    float len_rate;             /* 0x10 */
    float stiff;                /* 0x14 */
    float damp;                 /* 0x18 */
    float twist;                /* 0x1c */
    float amp_min;              /* 0x20 */
    float amp_max;              /* 0x24 */
    float spd_min;              /* 0x28 */
    float spd_max;              /* 0x2c */
    Vec3 ofs_from;              /* 0x30 */
    Vec3 ofs_to;                /* 0x3c */
} ThlRec;

/* per-point wave of a thunder line, 32 bytes */
typedef struct ThlWave {
    Vec3 ofs;
    float phase;                /* 0x0c */
    float spd;                  /* 0x10 */
    float amp;                  /* 0x14 */
    char pad_18[8];
} ThlWave;

/* thunder line (cm_thl_desc, 120 bytes): a jointed chain between two points */
typedef struct VenusPartObj {
    void *models;
    void *textures;             /* 0x04 */
    ThlWave *waves;             /* 0x08 */
    BossJointLink *joints;      /* 0x0c */
    BossJointLink *joints2;     /* 0x10 */
    Vec3 *pts;                  /* 0x14 */
    Vec3 *from;                 /* 0x18 */
    Vec3 *to;                   /* 0x1c */
    Vec3 ofs_from;              /* 0x20 */
    Vec3 ofs_to;                /* 0x2c */
    Vec3 dir;                   /* 0x38 */
    int tex;                    /* 0x44 */
    int num;                    /* 0x48 */
    int fade_count;             /* 0x4c */
    int fade_total;             /* 0x50 */
    float len_rate;             /* 0x54 */
    float stiff;                /* 0x58 */
    float damp;                 /* 0x5c */
    float twist;                /* 0x60 */
    float twist_phase;          /* 0x64 */
    float w0, w1, w2;           /* 0x68: line widths */
    unsigned int col;           /* 0x74 */
} VenusPartObj;

/* thunder line, 48 bytes */
typedef struct VenusPartEnt {
    VenusPartObj *o;
    char pad_04[8];
    Vec3 jolt_vel;              /* 0x0c */
    Vec3 jolt;                  /* 0x18: random kicks added by cm_thl_man_jolt */
    Vec3 end;                   /* 0x24: the bolt's end point (cm_thl_create rec[1]) */
} VenusPartEnt;

/* fog fade (venus_fog_desc, 12 bytes): mode 0 fades in, 1 halves the far plane */
typedef struct VenusFogWork {
    int frm;
    int frm_max;                /* 0x04 */
    int mode;                   /* 0x08 */
} VenusFogWork;

/* thunder line manager's bundle of lines (cm_thl_man_desc, 24 bytes) */
typedef struct VenusPartSet {
    void *models;
    void *textures;             /* 0x04 */
    int n;                      /* 0x08 */
    VenusPartEnt *e;            /* 0x0c */
    Vec3 *from;                 /* 0x10 */
    Vec3 *to;                   /* 0x14 */
} VenusPartSet;

/* spark burst; finishes once all its sprays have */
typedef struct VenusTaskList {
    char pad_00[8];
    int n;                      /* 0x08 */
    int *t[8];                  /* 0x0c: the length is not known */
} VenusTaskList;

extern unsigned int star_color;
extern EffStarWork *star_ctl;
extern unsigned int cm_fog_col;
extern float cmd_fog_n;
extern float cmd_fog_f;
extern BossFog venus_fog_cur;

/* ending dust cloud, 56 bytes: a calloc'ed block of n 64-byte records */
typedef struct VenusDustWork {
    void *pack;
    void *textures;
    int arg;
    Vec3 centre;
    float rad;
    float spin;
    char *buf;
    int n;
    int life;
    int life_max;
    int shown;
    char pad_34[4];
} VenusDustWork;

/* per-vertex tables of a model with animated vertex colours, 44 bytes */
typedef struct CmVccManWork {
    char pad_00[8];
    int *m;                     /* 0x08 */
    char **pts;                 /* 0x0c: the vertex records */
    float *len;                 /* 0x10: their distances from the y axis (cm_vcc_man) */
    int *val;                   /* 0x14: their saved colours (cm_vcc_man2) */
    int n;                      /* 0x18 */
    int frm, frm_max;               /* 0x1c */
    int col;                    /* 0x24 */
    void *parent;               /* 0x28 */
} CmVccManWork;

/* 52 bytes */
typedef struct PlateManWork {
    char pad_00[8];
    int frames;                 /* 0x08 */
    int shot_num;                    /* 0x0c */
    char pad_10[4];
    float plate_y;                  /* 0x14 */
    char pad_18[4];
    void *parent;               /* 0x1c */
    VenusParam *tab;            /* 0x20: plate_set_data */
    int frames2;                /* 0x24 */
    int set_idx;                    /* 0x28 */
    int obj;                    /* 0x2c */
    CmVccManWork *pts;             /* 0x30 */
} PlateManWork;

#endif /* REZ_B_VENUS_H */
