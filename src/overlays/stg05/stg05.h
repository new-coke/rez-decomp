/* Area 5 overlay: shared declarations. */
#ifndef REZ_STG05_H
#define REZ_STG05_H

#include "types_common.h"



#include "types_overlays.h"
#include "debug.h"
#include "anim.h"
#include "approach.h"
#include "camera.h"
#include "color.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "fog.h"
#include "game.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "map_manager.h"
#include "math_util.h"
#include "model.h"
#include "model_wire_break.h"
#include "nj_cnk_vu1.h"
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
#include "stage.h"
#include "task.h"
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

static inline void v3_sum_to(Vec3 *lhs, Vec3 *rhs, Vec3 *dst)
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

static inline void i3_add(int *dst, int *src)
{
    dst[0] += src[0];
    dst[1] += src[1];
    dst[2] += src[2];
}

static inline void i3_scale(int *v, float k)
{
    v[0] *= k;
    v[1] *= k;
    v[2] *= k;
}

static inline void i3_mul(int *dst, int *src, float scale)
{
    dst[0] = src[0] * scale;
    dst[1] = src[1] * scale;
    dst[2] = src[2] * scale;
}

static inline void i3_sub(int *dst, int *lhs, int *rhs)
{
    dst[0] = lhs[0] - rhs[0];
    dst[1] = lhs[1] - rhs[1];
    dst[2] = lhs[2] - rhs[2];
}

extern ScreenCfg screen_cfg;

extern int ulMatrixPush(void);
extern void *ulMatrixCur;
extern struct GwVarNode *gw_varedit_root;
extern void *scr_root;


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

struct ThronesChildWork;
struct ThronesCoreWork;
struct VirtuesWallWork;
struct DominionsBombWork;

/* 28 bytes, copied into the work */
typedef struct SeraphimBulletArg {
    Vec3 *pos;
    int *ang;                   /* 0x04 */
    Vec3 vel;                   /* 0x08 */
    float speed;                /* 0x14: distance per second */
    int type;                   /* 0x18: above 0 selects record 1 */
} SeraphimBulletArg;

struct SeraphimMissileWork;

/* 64 bytes, copied into the work */
typedef struct EnDominionsHoleArg {
    char pad_00[4];
    Vec3 pos;                   /* 0x04 */
    int ang[3];                 /* 0x10 */
    char pad_1c[12];
    float sel;                  /* 0x28: pass index of the second hole (negative: none) */
    float time;                 /* 0x2c: seconds before it opens */
    float n;                    /* 0x30: DominionsBody count */
    float speed;                /* 0x34: DominionsHead course speed (percent per second) */
    float param4;                    /* 0x38 */
    float param5;                    /* 0x3c */
} EnDominionsHoleArg;

extern void *stage_textures;
extern void *stage_models;

/* head of an action object's work */
typedef struct ObjMT {
    void *models;
    void *tex;
} ObjMT;

typedef struct OpenEffArg {
    Vec3 *pos;
    int *ang;                   /* 0x04 */
    int frames;                 /* 0x08 */
} OpenEffArg;

/* 40 bytes */
typedef struct EnSeraphimDmgArg {
    Vec3 *pos;
    int *ang;                   /* 0x04 */
    ModelPart *src;                /* 0x08: the model part copied in mode 0 */
    int type;                   /* 0x0c: the break model in mode 0 */
    int mode;                   /* 0x10: 0 = falling part, 1 = fading debris */
    int set;                    /* 0x14: the record pair */
    int delay;                  /* 0x18: frames */
    Vec3 off;                   /* 0x1c: added to the position in mode 1 */
} EnSeraphimDmgArg;

/* group name, label, 18 values */
typedef struct DominionsBombSet {
    char *name;
    char *label;                /* 0x04 */
    int v[18];                  /* 0x08 */
} DominionsBombSet;

typedef struct DominionsBombWork {
    struct DominionsBombRec *type;
    int state;                  /* 0x04 */
    Vec3 hitv;                  /* 0x08: the lock-on record's vector when hit */
    Vec3 pts[8];                /* 0x14: the eight bomblets */
    int angs[8][3];             /* 0x74: and their angles */
    ModelPart body;                /* 0xd4 */
    void *tail;                 /* 0x130: the "Tail" task (324-byte work) */
    int ang[3];                 /* 0x134 */
    int count;                  /* 0x140: frames */
} DominionsBombWork;

/* 40 bytes, copied into the work: the launcher's frames */
typedef struct SeraphimMissileArg {
    Vec3 *pos;
    int *ang;                   /* 0x04 */
    int open;                   /* 0x08: frames of the opening animation */
    int hold;                   /* 0x0c */
    int close;                  /* 0x10 */
    int wait;                   /* 0x14 */
    int lock_at;                /* 0x18: lockable from this frame */
    int type;                   /* 0x1c: above 0 selects record 1 */
    int *killed;                /* 0x20: set when shot down */
    int *launched;              /* 0x24: set when launched */
} SeraphimMissileArg;

typedef struct SeraphimMissileWork {
    struct SeraphimMissileRec *type;
    int state;                  /* 0x04: indexes seraphim_missile_states */
    int count;                  /* 0x08: frames */
    Vec3 hitv;                  /* 0x0c: the lock-on record's vector when hit */
    SeraphimMissileArg arg;          /* 0x18 */
    Vec3 pos;                   /* 0x40: the lock-on position (a model node) */
    LockObj lock;                  /* 0x4c */
    void *owner;                /* 0x8c */
    float fire;                 /* 0x90: the back fire size */
    int roll;                   /* 0x94 */
    Vec3 dir;                   /* 0x98: the flight direction when it separates */
    Vec3 tip;                   /* 0xa4: a mount point of the model */
} SeraphimMissileWork;

typedef struct Rgba4 {
    float rgba[4];
} Rgba4;

/* the enVirtuesBase a VirtuesWall hangs on */
typedef struct PosAngRec {
    char pad_00[12];
    Vec3 pos;                   /* 0x0c */
    int ang[3];                 /* 0x18 */
} PosAngRec;

/* as en_virtues_laser_detach sees it */
typedef struct EnVirtuesLaserTarget {
    char pad_00[36];
    Vec3 *target;               /* 0x24 */
    char pad_28[8];
    int latch;                  /* 0x30: when set, the target is held in held */
    char pad_34[12];
    Vec3 held;                  /* 0x40 */
} EnVirtuesLaserTarget;

typedef struct VirtuesWallWork {
    struct VirtuesWallRec *type;
    int count;                  /* 0x04 */
    int state;                  /* 0x08: indexes virtues_wall_states */
    Vec3 hitv;                  /* 0x0c: the lock-on record's vector when hit */
    int locked;                 /* 0x18: the lock-on record is registered */
    int mdl;                    /* 0x1c: index into virtues_wall_model_sets */
    PosAngRec *rec;             /* 0x20: the enVirtuesBase it hangs on */
    LockObj lock;                  /* 0x24 */
    Vec3 pos;                   /* 0x64 */
    float y, h;                 /* 0x70: height offset and height */
    enum { VW_NOT_HIT, VW_HIT } hit; /* 0x78: touched the player */
    ModelPart body;                /* 0x7c */
    void *save;                 /* 0xd8: the saved model drawn by virtues_wall_disp_reflect */
} VirtuesWallWork;

struct DominionsHeadShooterWork;

/* 32 bytes; en_virtues_laser_place_on_path places the base from it */
typedef struct EnVirtuesBaseArg {
    Vec3 *pts;                  /* VirtuesLaser aim points */
    int n;                      /* 0x04 */
    float h;                    /* 0x08: path position; negative ends the base */
    int frames;                 /* 0x0c: the VirtuesLaser time unit */
    Vec3 off;                   /* 0x10: offset in the path frame */
    float fall;                 /* 0x1c: path speed */
} EnVirtuesBaseArg;

/* 384 bytes */
typedef struct EnVirtuesBaseWork {
    void *models;
    void *tex;                  /* 0x04 */
    struct EnVirtuesBaseRec *type;      /* 0x08 */
    Vec3 pos;                   /* 0x0c */
    int ang[3];                 /* 0x18 */
    EnVirtuesBaseArg arg;               /* 0x24 */
    int count;                  /* 0x44: frames */
    ModelPart body;                /* 0x48 */
    ModelPart parts[2];            /* 0xa4: the blinking lights */
    int phase;                  /* 0x15c: which spawn starts first (0..3) */
    void *objs[4];              /* 0x160: the walls it raised */
    int sel;                    /* 0x170: the light part drawn */
    float fade;                 /* 0x174: light alpha */
    float t;                    /* 0x178: spawn clock */
    float scale;                /* 0x17c */
} EnVirtuesBaseWork;

typedef struct DominionsHeadShooterWork {
    float *type;                /* dominions_head_shooter_types record */
    int count;                  /* 0x04 */
    int state;                  /* 0x08: indexes dominions_head_shooter_states */
    void *child;                /* 0x0c: the first body of its chain (dominions_body_and_tail) */
    Vec3 hitv;                  /* 0x10: the lock-on record's vector when hit */
    Vec3 tgt;                   /* 0x1c: the point it turns toward */
    Vec3 start;                 /* 0x28 */
    Vec3 pos;                   /* 0x34: its placement */
    int ang[3];                 /* 0x40 */
    LockObj lock;                  /* 0x4c */
    Vec3 lpos;                  /* 0x8c: the lock-on point (a model node) */
} DominionsHeadShooterWork;

extern void *stage_sequence;

/* ef_mdl_brk_start, ef_mdl_brk_start_relative */
typedef struct EfMdlBrkArg {
    Vec3 pos;
    int ang[3];                 /* 0x0c */
    Vec3 scatter_dir;                   /* 0x18 */
    ModelPart *gfx;                /* 0x24 */
    int colored;                /* 0x28 */
    Rgba4 color;                /* 0x2c */
    int one;                    /* 0x3c */
    float decay, amp, scale, range, fall;  /* 0x40 */
} EfMdlBrkArg;

/* view of dominions_bomb_types */
typedef struct DominionsBombRec {
    char *name;
    char *label;                /* 0x04 */
    int he;                     /* 0x08: lock-on parameters */
    int hp;                     /* 0x0c */
    int score;                    /* 0x10: the score kind */
    float hp_reduce, crash_r, crash_damage;        /* 0x14: object parameters */
    float dur;                  /* 0x20: seconds of the fall (states 0 and 2) */
    float speed;                /* 0x24: distance per second */
    float trail_sec;            /* 0x28: the tail's length */
    float trail_f;              /* 0x2c */
    int trail_n;                /* 0x30 */
    int roll;                   /* 0x34: roll per second */
    float ease;                 /* 0x38: exponent of the fall curve */
    float drop;                 /* 0x3c: fall distance */
    float wait;                 /* 0x40: seconds of state 1 */
    float jitter;               /* 0x44: position jitter in state 1 */
    float life;                 /* 0x48: seconds of state 2 before it ends */
    float anim;                 /* 0x4c: fall ratio where the animation starts */
} DominionsBombRec;

/* view of en_virtues_base_types */
typedef struct EnVirtuesBaseRec {
    char *name;
    char *label;                /* 0x04 */
    char pad_08[20];
    float grow_sec;             /* 0x1c: seconds of the growth */
    float spawn_sec;            /* 0x20: seconds of one spawn round */
    float fade_sec;             /* 0x24: seconds of the fade-out */
    float rise_sec;             /* 0x28 */
    int col;                    /* 0x2c: blink colour */
    int div;                    /* 0x30: its beat */
} EnVirtuesBaseRec;

/* 68 bytes; view of virtues_wall_types */
typedef struct VirtuesWallRec {
    char *name;
    char *label;                /* 0x04 */
    int he;                     /* 0x08: lock-on parameters */
    int hp;                     /* 0x0c */
    int score;                    /* 0x10: the score kind */
    float hp_reduce, crash_r, crash_damage;        /* 0x14: also the touch radius and damage */
    int glow;                   /* 0x20: draws the glow, coloured by yuki_beat_color: */
    int glow_linear;            /* 0x24 */
    int glow_a, glow_b;         /* 0x28 */
    float glow_znear, glow_zfar, glow_anear, glow_afar;     /* 0x30 */
    int glow_div;               /* 0x40 */
} VirtuesWallRec;

/* view of seraphim_missile_types */
typedef struct SeraphimMissileRec {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    int same_lock;              /* 0x0c */
    int score;                  /* 0x10 */
    float hp_recover;           /* 0x14 */
    float crash_r;              /* 0x18 */
    float crash_damage;         /* 0x1c */
    float tail_sec;             /* 0x20 */
    float tail_width;           /* 0x24 */
    int tail_skip;              /* 0x28 */
    float accel_sec;            /* 0x2c */
    float accel;                /* 0x30 */
    float fire_pow;             /* 0x34 */
    float fire_pow2;            /* 0x38 */
    float fire_size;            /* 0x3c */
    float accel_ang_sec;        /* 0x40 */
    float accel_ang_rate;       /* 0x44 */
    int roll_spd;               /* 0x48: degrees per second */
    float separate;             /* 0x4c: distance from the player where it stops homing */
} SeraphimMissileRec;

/* 24 bytes: poses a DominionsBody chain follows */
typedef struct DominionsLinkArg {
    Vec3 *pos;
    int *ang;                   /* 0x04 */
    Vec3 *pos2;                 /* 0x08 */
    int *ang2;                  /* 0x0c */
    Vec3 *pos3;                 /* 0x10 */
    int *ang3;                  /* 0x14 */
} DominionsLinkArg;

/* 0xb05a78's third argument: the launcher's mount and timing. */
typedef struct DominionsChildLaunch {
    Vec3 *pos;
    int *ang;                   /* 0x04 */
    int seq;                    /* 0x08: the carrier's child_seq, a sequence index */
    int param;                  /* 0x0c: the carrier's child_param */
    int wait;                   /* 0x10: the launcher's wait */
} DominionsChildLaunch;

/* ModelPart, animation clock named */
typedef struct ModelPartAnim {
    char pad_00[60];
    float time;                 /* 0x3c */
    float length;               /* 0x40 */
    char pad_44[12];
    float radius;               /* 0x50 */
    char pad_54[8];
} ModelPartAnim;

#endif /* REZ_STG05_H */
