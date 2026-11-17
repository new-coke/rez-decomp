/* Eden boss overlay: shared declarations. */

#ifndef REZ_B_EDEN_H
#define REZ_B_EDEN_H

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
#include "enemy_status.h"
#include "fade.h"
#include "file.h"
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
#include "screen.h"
#include "sound_beat.h"
#include "sound_midiseq.h"
#include "sound_pcmstream.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"


/* set by one task, consumed by another (cmd_claw_at_sw, cmd_ch_sw) */
typedef enum { EDEN_REQ_NONE, EDEN_REQ_SET } EdenReq;

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

static inline void i3_set(int *d, int x, int y, int z)
{
    d[0] = x;
    d[1] = y;
    d[2] = z;
}

static inline void i3_mul(int *d, int *s, float k)
{
    d[0] = s[0] * k;
    d[1] = s[1] * k;
    d[2] = s[2] * k;
}

static inline void i3_copy(int *d, int *s)
{
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

#include "debug.h"

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

extern struct EdenMapWork *eden_map_task;
typedef struct EdenHumanPt { int v[3]; } EdenHumanPt;
/* a cell on the snake form, 36 bytes */
typedef struct EdenSnakeCell {
    Vec3 pos;
    Vec3 dir;
    int ang[3];
} EdenSnakeCell;
/* 220 bytes */
typedef struct EdenPieceCell {
    Vec3 rest;                  /* pos when the shake ended */
    Vec3 pos;
    void *point;
    int ang[3];
    void *point_vec;
    Vec3 vel;
    Vec3 ofs;
    int flags;                  /* 1 live, 2 lock-on registered, 4 falling */
    int timer;
    int timer_max;
    int row;
    int col;
    LockObj lock;
    float charge;               /* row-charge phase, 0..pi; scales the cell while nonzero */
    float charge_spd;
    int charge_cnt;             /* on a row's first cell */
    int charge_time;
    Vec3 scale;
    int fire_time;              /* counts down to the shot; draws the outline model meanwhile */
    float frame;
    float frame_max;
    float flare;
    Vec3 spring;
    Vec3 spring_vel;
} EdenPieceCell;

static inline void i3_add(int *d, int *s)
{
    d[0] += s[0];
    d[1] += s[1];
    d[2] += s[2];
}

#define ANG_WRAP(a) ((a) < 0 ? (a) | 0xffff0000 : (a) & 0xffff)

/* 32 bytes; three per map in eden_map_fog_keys */
typedef struct EdenFogKey {
    int unk_00;                 /* 0x00: 2 on the maps that draw the pack model */
    char pad_04[4];
    float deg;                  /* 0x08: spin per frame */
    float near, far;
    unsigned int col;
    int m_col, m_bcol;
} EdenFogKey;

extern EdenFogKey eden_map_fog_keys[];
#define FOGKEY eden_map_fog_keys

/* piece of an EdenGrp, 96 bytes */
typedef struct EdenGrpItem {
    Vec3 home;
    Vec3 pos;
    Vec3 target;
    Vec3 vel;
    int ang[3];
    int dang[3];
    int flags;                  /* 2 = done, 0x20 = fading in */
    int timer;
    char pad_50[8];
    float scl;                  /* 0x58 */
    WireObj *obj;
} EdenGrpItem;

/* group of pieces that springs to its owner's point and spins, 220 bytes */
typedef struct EdenGrp {
    EdenGrpItem *items;         /* 0x00: heap */
    int count;                  /* 0x04 */
    int flags;                  /* 1, 2 = emptied, 4 lock-on registered, 0x20 fading in, 0x40 */
    int timer;
    int delay;                  /* the shortest shock-wave delay so far */
    int life;                   /* frames: cmd_piece_lt plus random cmd_piece_ldt */
    LockObj lock;
    Vec3 org;                   /* the offset from the owner's point, turned by ang_to */
    Vec3 pos;
    Vec3 target;
    Vec3 vel;                   /* springs pos to the owner's point */
    int ang[3];
    int dang[3];                /* spin per frame, damped as blend rises */
    int ang_to[3];
    float spring_k;
    float blend;                /* rises to 1 (eden_dust_rang_rate) */
    float phase;                /* random 0..2pi */
    float alpha;
    void **model;               /* [1] drawn with nj_cnk_vu1_draw */
    float scl;
    float phase2;               /* random 0..2pi */
    int ang0[3];
    int kind;
    int tgt;
} EdenGrp;

/* lockable cluster of piece groups, 188 bytes: an eden_font glyph or an eden_model orb */
typedef struct EdenOrb {
    EdenGrp *grp;               /* 0x00 */
    int ngrp;                   /* 0x04 */
    char pad_08[4];
    int flags;                  /* 0x0c */
    int move_time;              /* 0x10: frames to the next target */
    int timer;                  /* 0x14 */
    Vec3 ofs;                   /* 0x18 */
    Vec3 pos;                   /* 0x24 */
    Vec3 target;                /* 0x30 */
    int ang[3];                 /* 0x3c */
    int dang[3];                /* 0x48 */
    char pad_54[24];
    LockObj lock;                  /* 0x6c */
    char pad_ac[8];
    float fade;                 /* 0xb4 */
    char pad_b8[4];
} EdenOrb;

/* text made of pieces, 60 bytes */
typedef struct EdenText {
    char pad_00[8];
    int arg;                    /* 0x08 */
    EdenOrb *g;                  /* 0x0c: the glyph records (eden_font_text_build) */
    Vec3 pos;                   /* 0x10 */
    Vec3 ofs;                   /* 0x1c */
    int n;                      /* 0x28 */
    int next;                   /* 0x2c */
    int *order;                 /* 0x30 */
    char pad_34[4];
    float bounce;               /* 0x38: scaled by cmd_font_bscl in eden_font_disp */
} EdenText;

static inline void v3_neg(Vec3 *d, Vec3 *s)
{
    d->x = -s->x;
    d->y = -s->y;
    d->z = -s->z;
}

#endif /* REZ_B_EDEN_H */
