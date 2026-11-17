/*
 * Sphinx, Area 1 enemy with three lock-on parts: sweeps side to side, bobbing, firing from its
 * nodes (ready, open, fire, close); may arrive falling. A set number shot down scores a bonus.
 */

#include "stg01.h"
#include "mathdef.h"
#include "approach.h"
#include "debug.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player_status.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "e_shot.h"
#include "../common/attack_ray.h"
#include "../common/en_backfire.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"

/* sine and cosine in two float scalars */
#define ROTS_Z(ang, s, c, x, y)     do {         float rot_u = x, rot_v = y;         ulSinCos(ang, &s, &c);         x = rot_u * c - rot_v * s;         y = rot_u * s + rot_v * c;     } while (0)

#define ROTS_X(ang, s, c, y, z)     do {         float rot_u = y, rot_v = z;         ulSinCos(ang, &s, &c);         y = rot_u * c - rot_v * s;         z = rot_u * s + rot_v * c;     } while (0)
#define ROTS_Y(ang, s, c, x, z)     do {         float rot_u = x, rot_v = z;         ulSinCos(ang, &s, &c);         x = rot_u * c + rot_v * s;         z = -rot_u * s + rot_v * c;     } while (0)

/* 380 bytes; names in sphinx_vardefs */
typedef struct SphinxParams {
    int hp;                         /* 0x08 */
    int same_lock;                  /* 0x0c */
    int score;                      /* 0x10 */
    float hp_reduce;                /* 0x14 */
    float crash_r;                  /* 0x18 */
    float crash_damage;             /* 0x1c */
    float break_sec_rate;           /* 0x20 */
    float break_pow_x;              /* 0x24 */
    float break_pow_z;              /* 0x28 */
    float break_pow_roll;           /* 0x2c */
    float accel_pow_x;              /* 0x30 */
    float accel_pow_z;              /* 0x34 */
    float accel_pow_roll;           /* 0x38 */
    float dis_y_rate;               /* 0x3c */
    float pow_y;                    /* 0x40 */
    float pow_y2;                   /* 0x44 */
    float y_dif;                    /* 0x48 */
    int roll_z;                     /* 0x4c: an angle */
    int roll_y;                     /* 0x50: an angle */
    float roll_y_pow;               /* 0x54 */
    float roll_y_pow2;              /* 0x58 */
    float roll_y_pow3;              /* 0x5c */
    float roll_y_pow4;              /* 0x60 */
    int roll_x;                     /* 0x64: an angle */
    float roll_x_pow;               /* 0x68 */
    float roll_x_pow2;              /* 0x6c */
    float roll_x_rate;              /* 0x70 */
    float roll_x_dif;               /* 0x74 */
    float turn_z_rate_sec;          /* 0x78 */
    float turn_z_rate_spd;          /* 0x7c */
    float stop_rate_sec;            /* 0x80 */
    float stop_rate_spd;            /* 0x84 */
    int stop_roll_x;                /* 0x88: an angle */
    float stop_roll_x_pow;          /* 0x8c */
    float stop_roll_x_pow2;         /* 0x90 */
    float stop_roll_y_pow;          /* 0x94 */
    float stop_roll_y_pow2;         /* 0x98 */
    float accel2stop_pow_x;         /* 0x9c */
    float accel2stop_pow_z;         /* 0xa0 */
    float accel2stop_roll_x_pow;    /* 0xa4 */
    float accel2stop_roll_x_pow2;   /* 0xa8 */
    float accel2stop_roll_z_pow;    /* 0xac */
    float break2stop_pow_x;         /* 0xb0 */
    float break2stop_pow_z;         /* 0xb4 */
    float break2stop_backfire_scl;  /* 0xb8 */
    float break2stop_backfire_pow;  /* 0xbc */
    float accel2away_sec;           /* 0xc0 */
    float accel2away_pow_z;         /* 0xc4 */
    float accel2away_pow_y;         /* 0xc8 */
    float accel2away_spd_over_rate; /* 0xcc */
    float accel2away_roll_rate;     /* 0xd0 */
    float accel2away_roll_pow;      /* 0xd4 */
    float accel2away_roll_pow2;     /* 0xd8 */
    float accel2away_backfire_scl;  /* 0xdc */
    float accel2away_backfire_pow;  /* 0xe0 */
    float away_sec;                 /* 0xe4 */
    float away_spd;                 /* 0xe8 */
    float fall_sec;                 /* 0xec */
    float fall_spd_start;           /* 0xf0 */
    float fall_acc;                 /* 0xf4 */
    int fall_ang;                   /* 0xf8: an angle */
    float stop_falling_sec;         /* 0xfc */
    float stop_falling_pow;         /* 0x100 */
    int stop_falling_ang;           /* 0x104: an angle */
    float stop_falling_ang_pow;     /* 0x108 */
    float start_sec;                /* 0x10c */
    float start_up_dis;             /* 0x110 */
    float start_up_pow;             /* 0x114 */
    float start_up_pow2;            /* 0x118 */
    float start_z_dis;              /* 0x11c */
    float finish_sec;               /* 0x120 */
    int wire;                       /* 0x124 */
    int wire_linear;                /* 0x128 */
    unsigned int wire_col;          /* 0x12c */
    unsigned int wire_col2;         /* 0x130 */
    float wire_near;                /* 0x134 */
    float wire_far;                 /* 0x138 */
    float wire_near_rate;           /* 0x13c */
    float wire_far_rate;            /* 0x140 */
    int wire_beat;                  /* 0x144 */
    float shot_sec_dif;             /* 0x148 */
    int shot_num;                   /* 0x14c */
    float shot_sec_ready;           /* 0x150 */
    float shot_sec_open;            /* 0x154 */
    float shot_sec_close;           /* 0x158 */
    float shot_sec_reset;           /* 0x15c */
    float attack_ray_scl;           /* 0x160 */
    float damage_motion_back_sec;   /* 0x164: the damage motion (label garbled) */
    float damage_motion_ret_sec;    /* 0x168: the damage motion (label garbled) */
    int damage_motion_ang;          /* 0x16c: an angle: the damage motion (label garbled) */
    int damage_motion_roll;         /* 0x170: an angle: the damage motion (label garbled) */
    int bonus_num;                  /* 0x174 */
    int bonus_score;                /* 0x178 */
} SphinxParams;

typedef struct SphinxVars {
    char *name;
    char *label;                /* 0x04 */
    SphinxParams p;            /* 0x08 */
} SphinxVars;

/* 76 bytes */
typedef struct SphinxPart {
    Vec3 pos;
    LockObj lock;                  /* 0x0c */
} SphinxPart;

/* 28 bytes */
typedef struct SphinxWobble {
    int ang[3];                 /* amplitude */
    int count;                  /* 0x0c: frames */
    int delay;                  /* 0x10 */
    int len;                    /* 0x14 */
    int freq;                   /* 0x18 */
} SphinxWobble;

/* 732 bytes */
typedef struct SphinxWork {
    SphinxVars *type;
    int state;                  /* 0x04 */
    int frames;                 /* 0x08: frames in the state */
    Vec3 hitv;                  /* 0x0c: the lock-on record's vector when hit */
    SphinxPart part[3];            /* 0x18 */
    int fire;                   /* 0xfc: a shot was fired */
    float backfire_alpha;       /* 0x100: linked as the 'enBackFire' alpha */
    int end;                    /* 0x104: the placement's end time */
    int sweeps_left, rounds_left, sweeps, rounds;      /* 0x108: from the creation record */
    float sweep_sec[4];         /* 0x118: from the creation record */
    float spd_x, spd_z;         /* 0x128: placement parameters 4 and 5 ('SpdX(m/s)', 'SpdZ(m/s)') */
    Vec3 right;                 /* 0x130 */
    Vec3 up;                    /* 0x13c */
    Vec3 fwd;                   /* 0x148 */
    int ang[3];                 /* 0x154: its angles when created */
    int ang0[3];                /* 0x160: its angles when its state last changed */
    float dis_y;                /* 0x16c */
    Vec3 node_pos[3];           /* 0x170: node positions (flag 0x2000000) */
    Vec3 node_axis[3];          /* 0x194: and their axes */
    int sign_x, sign_z;         /* 0x1b8: the signs of parameters 4 and 5 */
    int turn_z;
    float dy;                   /* 0x1c4: the last height offset */
    int roll_x;                 /* 0x1c8 */
    int fall_ang0[3];           /* 0x1cc: the direction it falls in along */
    ModelPart body;                /* 0x1d8 */
    int shot_count;             /* 0x234: frames (parameter 2 at first) */
    int shot_state;             /* 0x238 */
    int shot_k;                 /* 0x23c: the chosen shot motion */
    Vec3 shot_scale;            /* 0x240 */
    int nwob;                   /* 0x24c */
    int base_ang[3];            /* 0x250: its angles without the damage wobbles */
    SphinxWobble wob[3];              /* 0x25c */
    float backfire_scl;         /* 0x2b0: linked as the 'enBackFire' sprites' size */
    Vec3 backfire_pos[3];       /* 0x2b4: node positions (flag 0x2000000, second pass), linked to 'enBackFire's */
    int nparts;                 /* 0x2d8 */
} SphinxWork;

/* node positions found so far */
typedef struct SphinxCbArg {
    SphinxWork *work;
    int nparts, npos, nbackfire;
} SphinxCbArg;

extern int sphinx_init_arg;
extern SphinxVars sphinx_types[1];
extern VarDef sphinx_vardef;
extern VarDef sphinx_vardefs[93];
extern int sphinx_destroyed_count;
void sphinx_vars_register(void);
void sphinx_vars_unregister(void);
int sphinx_vars_release(void);
void sphinx_rotate_yxz(int *ang);
extern char sphinx_lock_name[];
extern Vec3 sphinx_node_axis;
void sphinx_wobble_add(void *act, SphinxWork *w);
void sphinx_wobble_sum(void *act, SphinxWork *w, int *out);
int sphinx_node_cb(unsigned int *nd, int ni, SphinxCbArg *cb);
void sphinx_pose_model(void *act, SphinxWork *w);
void sphinx_init(void *act, SphinxWork *w, int plp, int *rec);
void sphinx_release(void *act, SphinxWork *w);
int sphinx_next_state(void *act, SphinxWork *w, int n, int state);
extern Vec3 sphinx_yaw_axis;
extern Vec3 sphinx_pitch_axis;
void sphinx_shot_cycle(void *act, SphinxWork *w);
void sphinx_swing_yaw(void *act, SphinxWork *w, float *mtx, float rate);
void sphinx_swing_pitch(void *act, SphinxWork *w, float *mtx, float rate);
float sphinx_bob_delta(void *act, SphinxWork *w, float rate, float rprev);
void sphinx_bob_move(void *act, SphinxWork *w, Vec3 *cpos, float rate, float rprev);
extern char sphinx_bonus_name[];
extern void (*sphinx_states[])(void *o, SphinxWork *w);
extern void (*sphinx_exec_table[])(void *o, SphinxWork *w);
extern void (*sphinx_disp_table[])(void *o, SphinxWork *w);
void sphinx_stop_yaw(void *act, SphinxWork *w, float *mtx, float rate);
void sphinx_stop_pitch(void *act, SphinxWork *w, float *mtx, float rate);
void sphinx_update(void *act, SphinxWork *w);
void sphinx_exec_appear(void *act, SphinxWork *w);
void sphinx_exec_normal(void *act, SphinxWork *w);
void sphinx_exec_leave(void *act, SphinxWork *w);
void sphinx_exec_destroyed(void *act, SphinxWork *w);
void sphinx_pre_exec(void);
void sphinx_exec(void *act, SphinxWork *w);
void sphinx_disp(void *act, SphinxWork *w);
extern char sphinx_backfire_scale[];
void sphinx_state_break(void *act, SphinxWork *w);
void sphinx_state_accel(void *act, SphinxWork *w);
void sphinx_state_accel2stop(void *act, SphinxWork *w);
void sphinx_state_break2stop(void *act, SphinxWork *w);
void sphinx_state_accel2away(void *act, SphinxWork *w);
void sphinx_state_away(void *act, SphinxWork *w);
void sphinx_state_cruise(void *act, SphinxWork *w);
void sphinx_state_fall(void *act, SphinxWork *w);
void sphinx_state_stop_falling(void *act, SphinxWork *w);
void sphinx_state_start(void *act, SphinxWork *w);
extern int sphinx_part_sounds[];
extern char sphinx_name[];
extern ObjFuncs sphinx_funcs;
void sphinx_post_exec(void *act, SphinxWork *w);
void sphinx_disp_body(void *act, SphinxWork *w);
int sphinx_create_state(void *parent, Placement *pl, int state, int *ang, int extra);
int sphinx_create(void *parent, Placement *pl);
int sphinx_create_falling(void *parent, Placement *pl, int *ang);

/* 0x00b062f8 */
void sphinx_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *vd;
    int *fld;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&sphinx_vardef);
    for (i = 0; i < 1; i++) {
        vd = sphinx_vardefs;
        fld = (int *)&sphinx_types[i].p;
        sphinx_vardef.name = sphinx_types[i].name;
        sphinx_vardef.fmt = sphinx_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&sphinx_vardef);
        for (j = 0; j < 93; j++) {
            vd->ptr = fld;
            fld++;
            vd++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)sphinx_vardefs);
    }
}

/* 0x00b06410 */
void sphinx_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b06418 */
int sphinx_get_init_arg(void)
{
    return sphinx_init_arg;
}

/* 0x00b06428 */
int sphinx_vars_init(int *prm)
{
    sphinx_init_arg = *prm;
    sphinx_vars_register();
    sphinx_destroyed_count = 0;
    return 0;
}

/* 0x00b06458 */
int sphinx_vars_release(void)
{
    sphinx_vars_unregister();
    return 0;
}

/* 0x00b06478 */
int sphinx_marker_draw(int obj, void *act)
{
    int *ent;

    ulGxSetTextureList(stage_textures);
    ent = pnb_get_entry(stage_models, 47);
    return obj_draw_at(obj, act, (void *)*ent);
}

/* 0x00b064d0 */
void sphinx_wobble_reset(void *act, SphinxWork *w)
{
    w->nwob = 0;
    enemy_task_get_rot(act, w->base_ang);
}

/* 0x00b064e0 */
void sphinx_wobble_add(void *act, SphinxWork *w)
{
    float sc[2];
    SphinxWobble *wb;
    SphinxVars *typ;

    wb = w->wob;
    if (w->nwob < 3) {
        typ = w->type;
        wb += w->nwob++;
        ulSinCos((int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f), &sc[0], &sc[1]);
        wb->ang[0] = sc[0] * typ->p.damage_motion_ang;
        wb->ang[2] = sc[1] * typ->p.damage_motion_ang;
        wb->count = 0;
        wb->delay = typ->p.damage_motion_back_sec * FPS;
        wb->len = typ->p.damage_motion_ret_sec * FPS;
        wb->freq = typ->p.damage_motion_roll;
    }
}

/* 0x00b065e8 */
void sphinx_wobble_sum(void *act, SphinxWork *w, int *out)
{
    int dang[4];
    float sc[2];
    SphinxWobble *wb;
    float rate, amp;
    int i;

    out[0] = 0;
    out[1] = 0;
    out[2] = 0;
    for (i = 0, wb = w->wob; i < w->nwob; i++, wb++) {
        if (++wb->count >= wb->delay) {
            rate = (wb->count - wb->delay) / (float)wb->len;
            ulSinCos((int)(wb->freq * rate), &sc[0], &sc[1]);
            amp = sc[0] * (1.0f - rate);
            dang[0] = wb->ang[0] * amp;
            dang[1] = wb->ang[1] * amp;
            dang[2] = wb->ang[2] * amp;
            out[0] -= dang[0];
            out[1] -= dang[1];
            out[2] -= dang[2];
        }
        if (wb->count >= wb->delay + wb->len) {
            *wb = w->wob[w->nwob - 1];
            i--;
            wb--;
            w->nwob--;
        }
    }
}

/* 0x00b067b0 */
int sphinx_node_cb(unsigned int *nd, int ni, SphinxCbArg *cb)
{
    /* never taken: (x & 1) >> 1 is 0 */
    if ((*nd & 1) >> 1)
        cb = 0;
    if (*nd & 0x2000000) {
        Vec3 ax = sphinx_node_axis;
        SphinxWork *w = cb->work;

        ulMatrixGetTranslation((float *)ulMatrixCur, &w->node_pos[cb->npos].x);
        ulMatrixApplyVector((float *)ulMatrixCur, &ax.x, &w->node_axis[cb->npos++].x);
    }
    if (*nd & 0x1000000)
        ulMatrixGetTranslation((float *)ulMatrixCur, &cb->work->part[cb->nparts++].pos.x);
    if (*nd & 0x2000000)
        ulMatrixGetTranslation((float *)ulMatrixCur, &cb->work->backfire_pos[cb->nbackfire++].x);
    return 0;
}

/* 0x00b068f0 */
void sphinx_pose_model(void *act, SphinxWork *w)
{
    Vec3 cpos;
    int ang[4];
    SphinxCbArg cb;
    void *gfx = enemy_task_gfx_ptr(act);

    cb.work = w;
    cb.nparts = 0;
    cb.npos = 0;
    cb.nbackfire = 0;
    enemy_task_get_pos(act, &cpos);
    enemy_task_get_rot(act, ang);
    dc_object_set_callback(gfx, sphinx_node_cb, &cb);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &cpos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    sphinx_rotate_yxz(ang);
    dc_object_draw(gfx);
    ulMatrixPop();
    dc_object_set_callback(gfx, 0, w);
}

/* steer(cast) */
/* 0x00b069c8 */
void sphinx_init(void *act, SphinxWork *w, int plp, int *rec)
{
    int ang[4];
    Vec3 v;
    SphinxVars *typ = 0;
    SphinxPart *pt = 0;
    int *fall = 0;
    float x, y, z;
    int i;

    typ = &sphinx_types[*rec];
    w->type = typ;
    for (i = 0, pt = w->part; i < 3; i++, pt++) {
        lockon_obj_init(&pt->lock, act, &pt->pos, 0, sphinx_lock_name, enemy_task_get_status_slot(act), typ->p.hp,
                      typ->p.same_lock, 128);
        lockon_entry(&pt->lock);
        pt->lock.flags &= ~LOCK_LOCKABLE;
    }
    w->nparts = 3;
    enemy_task_clear_flags(act, 64);
    w->fire = 0;
    enemy_task_set_appear(act, 2, 30);
    enemy_task_set_vanish(act, 2, 60);
    w->backfire_alpha = 0.0f;
    w->end = rec[1];
    w->state = rec[9];
    w->frames = 1;
    enemy_task_get_rot(act, ang);
    i3_copy(w->ang, ang);
    {
        float sx, cx, sy, cy;

        x = 0.0f;
        y = 0.0f;
        z = -1.0f;
        ROTS_X(ang[0], sx, cx, y, z);
        ROTS_Y(ang[1], sy, cy, x, z);
        v3_set(&w->fwd, x, y, z);
    }
    {
        float sz, cz, sx, cx, sy, cy;

        x = 0; /* steer(dead) */
        x = 1.0f;
        y = 0.0f;
        z = 0.0f;
        ROTS_Z(ang[2], sz, cz, x, y);
        ROTS_X(ang[0], sx, cx, y, z);
        ROTS_Y(ang[1], sy, cy, x, z);
        v3_set(&w->right, x, y, z);
    }
    {
        float x, y, z;

        x = 0.0f;
        do { y = 1.0f; } while (0);
        z = 0.0f;
        {
            float sz, cz, sx, cx, sy, cy;

            ROTS_Z(ang[2], sz, cz, x, y);
            ROTS_X(ang[0], sx, cx, y, z);
            ROTS_Y(ang[1], sy, cy, x, z);
        }
        v3_set(&w->up, x, y, z);
    }
    w->sweeps_left = rec[2] + rec[4];
    w->rounds_left = rec[3];
    w->sweeps = rec[2];
    w->rounds = rec[3];
    w->turn_z = 0;
    w->sign_x = 0.0f < enemy_task_get_param(act, 4) ? 1 : -1;
    w->sign_z = 0.0f < enemy_task_get_param(act, 5) ? 1 : -1;
    w->sweep_sec[0] = ((float *)rec)[5];
    w->sweep_sec[1] = ((float *)rec)[6];
    w->sweep_sec[2] = ((float *)rec)[7];
    w->sweep_sec[3] = ((float *)rec)[8];
    w->spd_x = enemy_task_get_param(act, 4);
    w->spd_z = enemy_task_get_param(act, 5);
    w->dis_y = w->spd_x / (typ->p.break_pow_x + 1.0f) * w->sweep_sec[0];
    w->dis_y = w->dis_y + w->spd_x / (typ->p.accel_pow_x + 1.0f) * w->sweep_sec[2];
    w->dis_y = w->dis_y * typ->p.dis_y_rate;
    dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(*(void **)act, 48)), 0, 0x500);
    dc_object_set_wire(&w->body, model_pnb_get_wire(*(void **)act, 48));
    ((ModelPart *)enemy_task_gfx_ptr(act))->radius = w->body.radius = 230.0f;
    w->shot_count = enemy_task_get_param(act, 2);
    fall = ((int **)rec)[10];
    w->shot_state = 0;
    if (fall) {
        float x, y, z;

        i3_copy(w->fall_ang0, fall);
        x = 0.0f;
        do { y = 1.0f; } while (0);
        z = 0.0f;
        {
            float sz, cz, sy, cy, sx, cx;

            ROTS_Z(fall[2], sz, cz, x, y);
            ROTS_Y(fall[0], sy, cy, x, z);
            ROTS_X(fall[1], sx, cx, y, z);
        }
        v3_set(&v, x, y, z);
        enemy_task_set_param_vec(act, &v);
    }
    sphinx_wobble_reset(act, w);
}

/* 0x00b07010 */
void sphinx_release(void *act, SphinxWork *w)
{
    SphinxPart *pt;
    int i;

    dc_object_release(&w->body);
    for (i = 2, pt = w->part; i >= 0; i--, pt++)
        lockon_remove(&pt->lock);
}

/* 0x00b07058 */
int sphinx_next_state(void *act, SphinxWork *w, int n, int state)
{
    if (++w->frames > n) {
        w->state = state;
        w->frames = 1;
        enemy_task_get_rot(act, w->ang0);
        return 1;
    }
    return 0;
}

/* Picks one of three shot motions at random (53/54, 55/56, 51/52). */
/* steer(cast) */
static inline void sphinx_shot_pick(SphinxWork *w, ModelPart *gfx, ModelPart *bdy)
{
    int k = rand_lcg_next_a() % 3;

    w->shot_state = 2;
    w->shot_count = 0;
    switch (k) {
    case 0:
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 53)));
        dc_object_set_clip(bdy, *((struct DcClip **)pnb_get_entry(stage_models, 54)));
        break;
    case 1:
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 55)));
        dc_object_set_clip(bdy, *((struct DcClip **)pnb_get_entry(stage_models, 56)));
        break;
    case 2:
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 51)));
        dc_object_set_clip(bdy, *((struct DcClip **)pnb_get_entry(stage_models, 52)));
        break;
    }
    w->shot_k = k;
    gfx->frame = 0.0f;
}

/* 0x00b070a0 */
void sphinx_shot_cycle(void *act, SphinxWork *w)
{
    AttackRayArg ray;
    SphinxVars *typ = w->type;
    ModelPart *gfx = enemy_task_gfx_ptr(act);
    ModelPart *bdy = &w->body;
    int n, dif;

    switch (w->shot_state) {
    case 0:
        if (--w->shot_count > 0)
            break;
        if (w->state >= 4)
            break;
        sphinx_shot_pick(w, gfx, bdy);
        break;
    case 1:
        n = ++w->shot_count;
        if (n < typ->p.shot_sec_ready * FPS) {
            gfx->frame = gfx->frames * (n / (typ->p.shot_sec_ready * FPS));
            break;
        }
        sphinx_shot_pick(w, gfx, bdy);
        break;
    case 2:
        n = ++w->shot_count;
        if (typ->p.shot_sec_open * FPS < n) {
            w->shot_count = 0;
            w->shot_state = 3;
            break;
        }
        gfx->frame = gfx->frames * (n / (typ->p.shot_sec_open * FPS));
        break;
    case 3:
        dif = typ->p.shot_sec_dif * FPS;
        if (dif * typ->p.shot_num < w->shot_count) {
            w->shot_count = 0;
            w->shot_state = 4;
        } else if (w->shot_count % dif == 0) {
            if (w->shot_count < dif * typ->p.shot_num) {
                ray.pos = &w->node_pos[w->shot_k];
                ray.scale = &w->shot_scale;
                ray.dur = dif;
                attack_ray_create(act, &ray);
            }
            if (w->shot_count > 0)
                w->fire = 1;
        }
        w->shot_count++;
        break;
    case 4:
        n = ++w->shot_count;
        if (typ->p.shot_sec_close * FPS < n) {
            w->shot_state = 0;
            w->shot_count = enemy_task_get_param(act, 2);
            break;
        }
        gfx->frame = gfx->frames * (1.0f - n / (typ->p.shot_sec_close * FPS));
        break;
    case 5:
        n = ++w->shot_count;
        if (typ->p.shot_sec_reset * FPS < n) {
            w->shot_state = 0;
            w->shot_count = enemy_task_get_param(act, 2);
            break;
        }
        gfx->frame = gfx->frames * (1.0f - n / (typ->p.shot_sec_reset * FPS));
        break;
    }
}

/* 0x00b07490 */
void sphinx_swing_yaw(void *act, SphinxWork *w, float *mtx, float rate)
{
    SphinxVars *typ = w->type;
    Vec3 ax = sphinx_yaw_axis;
    float sc[2];
    int yaw;

    ulMatrixApplyVector((float *)ulMatrixCur, &ax.x, &ax.x);
    if (w->turn_z) {
        ulSinCos((int)(safe_powf(rate, typ->p.roll_y_pow3) * ANG_TURN - ANG_QUARTER), &sc[0],
                 &sc[1]);
        rate = safe_powf((sc[0] + 1.0f) * 0.5f, typ->p.roll_y_pow4);
        yaw = typ->p.roll_y * (rate + rate - 1.0f);
        matrix_from_axis_angle(mtx, &ax, -yaw * w->sign_x * w->sign_z);
    } else {
        ulSinCos((int)(safe_powf(rate, typ->p.roll_y_pow) * ANG_HALF - ANG_QUARTER), &sc[0],
                 &sc[1]);
        rate = safe_powf((sc[0] + 1.0f) * 0.5f, typ->p.roll_y_pow2);
        yaw = typ->p.roll_y * (rate + rate - 1.0f);
        matrix_from_axis_angle(mtx, &ax, yaw * w->sign_x * w->sign_z);
    }
}

/* 0x00b07648 */
void sphinx_swing_pitch(void *act, SphinxWork *w, float *mtx, float rate)
{
    Vec3 ax = sphinx_pitch_axis;
    float sc[2];
    SphinxVars *typ = w->type;
    float ph, sgn;

    ph = rate + typ->p.roll_x_dif;
    while (ph < 0.0f)
        ph += 1.0f;
    while (1.0f < ph)
        ph -= 1.0f;
    ulMatrixApplyVector((float *)ulMatrixCur, &ax.x, &ax.x);
    if (!w->turn_z) {
        ulSinCos((int)(safe_powf(ph, typ->p.roll_x_pow) * ANG_TURN), &sc[0], &sc[1]);
        ph = safe_powf((sc[0] + 1.0f) * 0.5f, typ->p.roll_x_pow2);
        w->roll_x = typ->p.roll_x * (ph + ph - 1.0f) * w->sign_z;
    } else {
        sgn = w->sign_z * rate - w->sign_z * (1.0f - rate);
        ulSinCos((int)(safe_powf(ph, typ->p.roll_x_pow) * ANG_TURN), &sc[0], &sc[1]);
        ph = safe_powf((sc[0] + 1.0f) * 0.5f, typ->p.roll_x_pow2);
        w->roll_x = typ->p.roll_x * (ph + ph - 1.0f) * sgn;
    }
    matrix_from_axis_angle(mtx, &ax, w->roll_x);
}

/* 0x00b07880 */
float sphinx_bob_delta(void *act, SphinxWork *w, float rate, float rprev)
{
    float sc[2];
    SphinxVars *typ = w->type;
    float crv;

    rate += typ->p.y_dif;
    while (rate < 0.0f)
        rate += 1.0f;
    while (1.0f < rate)
        rate -= 1.0f;
    rprev += typ->p.y_dif;
    while (rprev < 0.0f)
        rprev += 1.0f;
    while (1.0f < rprev)
        rprev -= 1.0f;
    ulSinCos((int)(safe_powf(rate, typ->p.pow_y) * ANG_TURN + ANG_QUARTER), &sc[0], &sc[1]);
    crv = safe_powf((sc[0] + 1.0f) * 0.5f, typ->p.pow_y2);
    ulSinCos((int)(safe_powf(rprev, typ->p.pow_y) * ANG_TURN + ANG_QUARTER), &sc[0], &sc[1]);
    crv -= safe_powf((sc[0] + 1.0f) * 0.5f, typ->p.pow_y2);
    return w->dis_y * crv;
}

/* 0x00b07a78 */
void sphinx_bob_move(void *act, SphinxWork *w, Vec3 *cpos, float rate, float rprev)
{
    Vec3 ofs;

    w->dy = sphinx_bob_delta(act, w, rate, rprev);
    v3_mul(&ofs, &w->up, w->dy);
    v3_add(cpos, &ofs);
}

/* 0x00b07af8 */
void sphinx_state_break(void *act, SphinxWork *w)
{
    Vec3 cpos;
    Vec3 ofs;
    int ang[4];
    float mx[16];
    float my[16];
    BackFireLink lnk;
    float sc[2];
    SphinxVars *typ = w->type;
    Vec3 *bp;
    float sec, accel_sec, vx, vz, rate, rprev, srate, sprev, dis;
    int n, accel_frames, roll, i;

    sec = w->sweeps_left == 1 ? w->sweep_sec[1] : w->sweep_sec[0];
    accel_sec = w->turn_z ? w->sweep_sec[3] : w->sweep_sec[2];
    if (w->turn_z || w->sweeps_left == 1) {
        vx = w->spd_x * typ->p.turn_z_rate_spd;
        vz = w->spd_z * typ->p.turn_z_rate_spd;
    } else {
        vx = w->spd_x;
        vz = w->spd_z;
    }
    n = sec * FPS;
    accel_frames = accel_sec * FPS;
    rate = w->frames / (float)n;
    rprev = (w->frames - 1) / (float)n;
    srate = (accel_frames + w->frames) / (float)(accel_frames + n);
    sprev = (w->frames + accel_frames - 1) / (float)(accel_frames + n);
    enemy_task_get_pos(act, &cpos);
    {
        float x = -vx, z = -vz;

        dis = x / (typ->p.break_pow_x + 1.0f) * sec * (safe_powf(1.0f - rate, typ->p.break_pow_x + 1.0f) - 1.0f);
        dis -= x / (typ->p.break_pow_x + 1.0f) * sec * (safe_powf(1.0f - rprev, typ->p.break_pow_x + 1.0f) - 1.0f);
        v3_mul(&ofs, &w->right, dis);
        v3_add(&cpos, &ofs);
        dis = z / (typ->p.break_pow_z + 1.0f) * sec * (safe_powf(1.0f - rate, typ->p.break_pow_z + 1.0f) - 1.0f);
        dis -= z / (typ->p.break_pow_z + 1.0f) * sec * (safe_powf(1.0f - rprev, typ->p.break_pow_z + 1.0f) - 1.0f);
        v3_mul(&ofs, &w->fwd, dis);
        v3_add(&cpos, &ofs);
    }
    sphinx_bob_move(act, w, &cpos, srate, sprev);
    enemy_task_set_pos(act, &cpos);
    ulSinCos((int)(safe_powf(rate, typ->p.break_pow_roll) * ANG_HALF - ANG_QUARTER), &sc[0],
             &sc[1]);
    roll = typ->p.roll_z * w->sign_x * (sc[0] + 1.0f) * 0.5f;
    ulMatrixPush();
    matrix_unit_set(0);
    vu0_matrix_rotate_z(ulMatrixCur, roll);
    sphinx_swing_pitch(act, w, mx, srate);
    sphinx_swing_yaw(act, w, my, srate);
    matrix_unit_set(0);
    sphinx_rotate_yxz(w->ang);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, my);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, mx);
    vu0_matrix_rotate_z(ulMatrixCur, roll);
    model_camera_angles(ang);
    ulMatrixPop();
    enemy_task_set_rot(act, ang);
    if (sphinx_next_state(act, w, n, 1)) {
        w->spd_x = -w->spd_x;
        w->sign_x = -w->sign_x;
        w->turn_z = 0;
        if (--w->sweeps_left > 0)
            return;
        w->turn_z = 1;
        w->spd_z = -w->spd_z;
        w->sign_z = -w->sign_z;
        w->sweeps_left = w->sweeps;
        if (--w->rounds_left > 0)
            return;
        w->state = 2;
        heap_bzero(&lnk, sizeof(lnk));
        lnk.obj_scale = (Vec3 *)sphinx_backfire_scale;
        lnk.scale = &w->backfire_scl;
        lnk.alpha = &w->backfire_alpha;
        for (bp = w->backfire_pos, i = 2; i >= 0; bp++, i--) {
            lnk.pos = bp;
            en_backfire_create(act, &lnk, 0);
            w->backfire_scl = 0.0f;
        }
    }
}

/* 0x00b08020 */
void sphinx_state_accel(void *act, SphinxWork *w)
{
    Vec3 cpos;
    Vec3 ofs;
    int ang[4];
    float mx[16];
    float my[16];
    float sc[2];
    SphinxVars *typ = w->type;
    float sec, break_sec, vx, vz, rate, rprev, srate, sprev, dis;
    int n, accel_frames, roll;

    break_sec = w->sweeps_left == 1 ? w->sweep_sec[1] : w->sweep_sec[0];
    sec = w->turn_z ? w->sweep_sec[3] : w->sweep_sec[2];
    if (w->turn_z || w->sweeps_left == 1) {
        vx = w->spd_x * typ->p.turn_z_rate_spd;
        vz = w->spd_z * typ->p.turn_z_rate_spd;
    } else {
        vx = w->spd_x;
        vz = w->spd_z;
    }
    n = break_sec * FPS;
    accel_frames = sec * FPS;
    rate = w->frames / (float)accel_frames;
    srate = accel_frames + n;
    sprev = (w->frames - 1) / srate;
    srate = w->frames / srate;
    rprev = (w->frames - 1) / (float)accel_frames;
    enemy_task_get_pos(act, &cpos);
    dis = vx / (typ->p.accel_pow_x + 1.0f) * sec * safe_powf(rate, typ->p.accel_pow_x + 1.0f);
    dis -= vx / (typ->p.accel_pow_x + 1.0f) * sec * safe_powf(rprev, typ->p.accel_pow_x + 1.0f);
    v3_mul(&ofs, &w->right, dis);
    v3_add(&cpos, &ofs);
    dis = vz / (typ->p.accel_pow_z + 1.0f) * sec * safe_powf(rate, typ->p.accel_pow_z + 1.0f);
    dis -= vz / (typ->p.accel_pow_z + 1.0f) * sec * safe_powf(rprev, typ->p.accel_pow_z + 1.0f);
    v3_mul(&ofs, &w->fwd, dis);
    v3_add(&cpos, &ofs);
    sphinx_bob_move(act, w, &cpos, srate, sprev);
    enemy_task_set_pos(act, &cpos);
    ulSinCos((int)(safe_powf(1.0f - rate, typ->p.accel_pow_roll) * ANG_HALF - ANG_QUARTER), &sc[0],
             &sc[1]);
    roll = -typ->p.roll_z * w->sign_x * (sc[0] + 1.0f) * 0.5f;
    ulMatrixPush();
    matrix_unit_set(0);
    vu0_matrix_rotate_z(ulMatrixCur, roll);
    sphinx_swing_pitch(act, w, mx, srate);
    sphinx_swing_yaw(act, w, my, srate);
    matrix_unit_set(0);
    sphinx_rotate_yxz(w->ang);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, my);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, mx);
    vu0_matrix_rotate_z(ulMatrixCur, roll);
    model_camera_angles(ang);
    ulMatrixPop();
    enemy_task_set_rot(act, ang);
    sphinx_next_state(act, w, accel_frames, 0);
}

/* 0x00b08440 */
void sphinx_stop_yaw(void *act, SphinxWork *w, float *mtx, float rate)
{
    Vec3 ax = sphinx_yaw_axis;
    float sc[2];
    float crv;
    SphinxVars *typ = w->type;

    ulMatrixApplyVector((float *)ulMatrixCur, &ax.x, &ax.x);
    ulSinCos((int)(safe_powf(rate, typ->p.stop_roll_y_pow) * ANG_HALF - ANG_QUARTER), &sc[0],
             &sc[1]);
    crv = 1.0f - safe_powf((sc[0] + 1.0f) * 0.5f, typ->p.stop_roll_y_pow2);
    matrix_from_axis_angle(mtx, &ax, (int)(typ->p.roll_y * crv) * w->sign_x * w->sign_z);
}

/* 0x00b08558 */
void sphinx_stop_pitch(void *act, SphinxWork *w, float *mtx, float rate)
{
    Vec3 ax = sphinx_pitch_axis;
    float sc[2];
    SphinxVars *typ = w->type;

    ulMatrixApplyVector((float *)ulMatrixCur, &ax.x, &ax.x);
    ulSinCos((int)(safe_powf(rate, typ->p.stop_roll_x_pow) * ANG_HALF - ANG_QUARTER), &sc[0],
             &sc[1]);
    rate = safe_powf((sc[0] + 1.0f) * 0.5f, typ->p.stop_roll_x_pow2);
    matrix_from_axis_angle(mtx, &ax, (-typ->p.stop_roll_x - w->roll_x) * rate + w->roll_x);
}

/* 0x00b08668 */
void sphinx_state_accel2stop(void *act, SphinxWork *w)
{
    Vec3 cpos;
    Vec3 ofs;
    int ang[4];
    float mx[16];
    float my[16];
    float sc[2];
    SphinxVars *typ = w->type;
    float sec, break_sec, vx, vz, rate, rprev, srate, sprev, dis;
    int n, accel_frames, roll;

    break_sec = w->sweep_sec[0] * typ->p.stop_rate_sec;
    sec = w->sweep_sec[2] * typ->p.stop_rate_sec;
    vz = -fabsf(w->spd_z);
    vx = w->spd_x * typ->p.stop_rate_spd;
    n = break_sec * FPS;
    accel_frames = sec * FPS;
    vz *= typ->p.stop_rate_spd;
    rate = w->frames / (float)accel_frames;
    rprev = (w->frames - 1) / (float)accel_frames;
    srate = w->frames / (float)(accel_frames + n);
    sprev = (w->frames - 1) / (float)(accel_frames + n);
    enemy_task_get_pos(act, &cpos);
    dis = vx / (typ->p.accel2stop_pow_x + 1.0f) * sec * safe_powf(rate, typ->p.accel2stop_pow_x + 1.0f);
    dis -= vx / (typ->p.accel2stop_pow_x + 1.0f) * sec * safe_powf(rprev, typ->p.accel2stop_pow_x + 1.0f);
    v3_mul(&ofs, &w->right, dis);
    v3_add(&cpos, &ofs);
    dis = vz / (typ->p.accel2stop_pow_z + 1.0f) * sec * safe_powf(rate, typ->p.accel2stop_pow_z + 1.0f);
    dis -= vz / (typ->p.accel2stop_pow_z + 1.0f) * sec * safe_powf(rprev, typ->p.accel2stop_pow_z + 1.0f);
    v3_mul(&ofs, &w->fwd, dis);
    v3_add(&cpos, &ofs);
    sphinx_bob_move(act, w, &cpos, srate, sprev);
    enemy_task_set_pos(act, &cpos);
    ulSinCos((int)(safe_powf(1.0f - rate, typ->p.accel2stop_roll_z_pow) * ANG_HALF - ANG_QUARTER),
                  &sc[0], &sc[1]);
    roll = -typ->p.roll_z * w->sign_x * (sc[0] + 1.0f) * 0.5f;
    ulMatrixPush();
    matrix_unit_set(0);
    vu0_matrix_rotate_z(ulMatrixCur, roll);
    sphinx_stop_pitch(act, w, mx, srate);
    sphinx_stop_yaw(act, w, my, srate);
    matrix_unit_set(0);
    sphinx_rotate_yxz(w->ang);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, my);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, mx);
    vu0_matrix_rotate_z(ulMatrixCur, roll);
    model_camera_angles(ang);
    ulMatrixPop();
    enemy_task_set_rot(act, ang);
    sphinx_next_state(act, w, accel_frames, 3);
}

/* 0x00b08a58 */
void sphinx_state_break2stop(void *act, SphinxWork *w)
{
    Vec3 cpos;
    Vec3 ofs;
    int ang[4];
    float mx[16];
    float my[16];
    float sc[2];
    SphinxVars *typ = w->type;
    float sec, accel_sec, vx, vz, rate, rprev, srate, sprev, dis;
    int n, accel_frames;

    sec = w->sweep_sec[0] * typ->p.stop_rate_sec;
    accel_sec = w->sweep_sec[2] * typ->p.stop_rate_sec;
    vz = -fabsf(w->spd_z);
    vx = w->spd_x;
    n = sec * FPS;
    accel_frames = accel_sec * FPS;
    srate = (accel_frames + w->frames) / (float)(accel_frames + n);
    vx *= typ->p.stop_rate_spd;
    vz *= typ->p.stop_rate_spd;
    rprev = (w->frames - 1) / (float)n;
    rate = w->frames / (float)n;
    sprev = (w->frames + accel_frames - 1) / (float)(accel_frames + n);
    enemy_task_get_pos(act, &cpos);
    vz = -vz;
    vx = -vx;
    dis = vx / (typ->p.break2stop_pow_x + 1.0f) * sec * (safe_powf(1.0f - rate, typ->p.break2stop_pow_x + 1.0f) - 1.0f);
    dis -= vx / (typ->p.break2stop_pow_x + 1.0f) * sec * (safe_powf(1.0f - rprev, typ->p.break2stop_pow_x + 1.0f) - 1.0f);
    v3_mul(&ofs, &w->right, dis);
    v3_add(&cpos, &ofs);
    dis = vz / (typ->p.break2stop_pow_z + 1.0f) * sec * (safe_powf(1.0f - rate, typ->p.break2stop_pow_z + 1.0f) - 1.0f);
    dis -= vz / (typ->p.break2stop_pow_z + 1.0f) * sec * (safe_powf(1.0f - rprev, typ->p.break2stop_pow_z + 1.0f) - 1.0f);
    v3_mul(&ofs, &w->fwd, dis);
    v3_add(&cpos, &ofs);
    sphinx_bob_move(act, w, &cpos, srate, sprev);
    enemy_task_set_pos(act, &cpos);
    ulMatrixPush();
    matrix_unit_set(0);
    sphinx_stop_pitch(act, w, mx, srate);
    sphinx_stop_yaw(act, w, my, srate);
    matrix_unit_set(0);
    sphinx_rotate_yxz(w->ang);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, my);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, mx);
    model_camera_angles(ang);
    ulMatrixPop();
    enemy_task_set_rot(act, ang);
    ulSinCos((int)(safe_powf(rate, typ->p.break2stop_backfire_pow) * ANG_HALF - ANG_QUARTER),
             &sc[0], &sc[1]);
    w->backfire_scl = typ->p.break2stop_backfire_scl * (sc[0] + 1.0f) * 0.5f;
    sphinx_next_state(act, w, n, 4);
}

/* 0x00b08e58 */
void sphinx_state_accel2away(void *act, SphinxWork *w)
{
    int ang[4];
    Vec3 cpos;
    Vec3 dv;
    Vec3 ofs;
    float sc[4][2];
    SphinxVars *typ = w->type;
    float rate, rprev, crv, dis, spd, x, y, z, yspd, ydis;
    int n;

    n = typ->p.accel2away_sec * FPS;
    rate = w->frames / (float)n;
    rprev = (w->frames - 1) / (float)n;
    ulSinCos((int)(safe_powf(rate, typ->p.accel2away_roll_pow) * ANG_HALF + ANG_QUARTER), &sc[0][0],
             &sc[0][1]);
    crv = (sc[0][0] + 1.0f) * 0.5f;
    ulSinCos((int)(safe_powf(rate, typ->p.accel2away_roll_pow2) * ANG_TURN - ANG_QUARTER),
             &sc[0][0], &sc[0][1]);
    crv -= (sc[0][0] + 1.0f) * 0.5f * typ->p.accel2away_roll_rate;
    ulMatrixPush();
    matrix_unit_set(0);
    sphinx_rotate_yxz(w->ang);
    vu0_matrix_rotate_x(ulMatrixCur, (int)(-crv * typ->p.stop_roll_x));
    model_camera_angles(ang);
    ulMatrixPop();
    enemy_task_set_rot(act, ang);
    enemy_task_get_pos(act, &cpos);
    x = 0.0f;
    y = 0.0f;
    z = -1.0f;
    ROT_X(ang[0], sc[1], y, z);
    ROT_Y(ang[1], sc[2], x, z);
    v3_set(&dv, x, y, z);
    spd = typ->p.away_spd;
    dis = spd / (typ->p.accel2away_pow_z + 1.0f) * typ->p.accel2away_sec * (typ->p.accel2away_spd_over_rate + 1.0f)
        * safe_powf(rate, typ->p.accel2away_pow_z + 1.0f);
    dis -= spd / (typ->p.accel2away_pow_z + 1.0f) * typ->p.accel2away_sec * (typ->p.accel2away_spd_over_rate + 1.0f)
         * safe_powf(rprev, typ->p.accel2away_pow_z + 1.0f);
    v3_scale(&dv, dis);
    v3_add(&cpos, &dv);
    yspd = -(w->dy * FPS);
    ydis = yspd / (typ->p.accel2away_pow_y + 1.0f) * typ->p.accel2away_sec
        * (safe_powf(1.0f - rate, typ->p.accel2away_pow_y + 1.0f) - 1.0f);
    ydis -= yspd / (typ->p.accel2away_pow_y + 1.0f) * typ->p.accel2away_sec
         * (safe_powf(1.0f - rprev, typ->p.accel2away_pow_y + 1.0f) - 1.0f);
    v3_mul(&ofs, &w->up, ydis);
    v3_add(&cpos, &ofs);
    enemy_task_set_pos(act, &cpos);
    ulSinCos((int)(safe_powf(rate, typ->p.accel2away_backfire_pow) * ANG_HALF + ANG_QUARTER),
             &sc[3][0], &sc[3][1]);
    sc[3][0] = (sc[3][0] + 1.0f) * 0.5f;
    sc[3][0] = typ->p.break2stop_backfire_scl * sc[3][0] + typ->p.accel2away_backfire_scl * (1.0f - sc[3][0]);
    w->backfire_scl = sc[3][0];
    sphinx_next_state(act, w, n, 5);
}

/* 0x00b092c8 */
void sphinx_state_away(void *act, SphinxWork *w)
{
    Vec3 cpos;
    Vec3 dv;
    int ang[4];
    float sc[2][2];
    SphinxVars *typ = w->type;
    float sspd, espd, rate, rprev, dis, x, y, z;
    int n;

    n = typ->p.away_sec * FPS;
    sspd = typ->p.away_spd;
    espd = typ->p.accel2away_spd_over_rate * sspd + sspd;
    rate = w->frames / (float)n;
    rprev = (w->frames - 1) / (float)n;
    x = 0.0f;
    y = 0.0f;
    z = -1.0f;
    enemy_task_get_rot(act, ang);
    ROT_X(ang[0], sc[0], y, z);
    ROT_Y(ang[1], sc[1], x, z);
    v3_set(&dv, x, y, z);
    dis = ((sspd - espd) * rate * 0.5f + espd) * (w->frames / FPS)
        - ((sspd - espd) * rprev * 0.5f + espd) * ((w->frames - 1) / FPS);
    enemy_task_get_pos(act, &cpos);
    v3_scale(&dv, dis);
    v3_add(&cpos, &dv);
    enemy_task_set_pos(act, &cpos);
    sphinx_next_state(act, w, n, 6);
}

/* 0x00b094f8 */
void sphinx_state_cruise(void *act, SphinxWork *w)
{
    Vec3 cpos;
    Vec3 dv;
    int ang[4];
    float sc[2][2];
    float spd, x, y, z;

    spd = w->type->p.away_spd;
    x = 0.0f;
    y = 0.0f;
    enemy_task_get_rot(act, ang);
    z = -1.0f;
    ROT_X(ang[0], sc[0], y, z);
    ROT_Y(ang[1], sc[1], x, z);
    v3_set(&dv, x, y, z);
    enemy_task_get_pos(act, &cpos);
    v3_scale(&dv, spd * (1.0f / FPS));
    v3_add(&cpos, &dv);
    enemy_task_set_pos(act, &cpos);
}

/* 0x00b09630 */
void sphinx_state_fall(void *act, SphinxWork *w)
{
    Vec3 cpos;
    Vec3 v;
    int ang[4];
    float sc[2];
    SphinxVars *typ = w->type;
    float rate, dis;
    int n;

    n = typ->p.fall_sec * FPS;
    dis = (typ->p.fall_spd_start + w->frames / FPS * typ->p.fall_acc) * (1.0f / FPS)
        + typ->p.fall_acc * 0.5f * (1.0f / FPS) * (1.0f / FPS);
    rate = w->frames / (float)n;
    enemy_task_get_pos(act, &cpos);
    enemy_task_get_param_vec(act, &v);
    v3_scale(&v, -dis);
    v3_add(&cpos, &v);
    enemy_task_set_pos(act, &cpos);
    ulSinCos((int)(rate * ANG_HALF - ANG_QUARTER), &sc[0], &sc[1]);
    ulMatrixPush();
    matrix_unit_set(0);
    sphinx_rotate_yxz(w->fall_ang0);
    vu0_matrix_rotate_x(ulMatrixCur, (int)((sc[0] * typ->p.fall_ang + typ->p.fall_ang) * 0.5f));
    model_camera_angles(ang);
    ulMatrixPop();
    enemy_task_set_rot(act, ang);
    sphinx_next_state(act, w, n, 8);
}

/* 0x00b09818 */
void sphinx_state_stop_falling(void *act, SphinxWork *w)
{
    Vec3 cpos;
    Vec3 v;
    int ang[4];
    float sc[2];
    SphinxVars *typ = w->type;
    float rate, rprev, spd, dis;
    int n;

    n = typ->p.stop_falling_sec * FPS;
    rate = w->frames / (float)n;
    rprev = (w->frames - 1) / (float)n;
    spd = typ->p.fall_spd_start + typ->p.fall_sec * typ->p.fall_acc;
    spd = -spd;
    dis = spd / (typ->p.stop_falling_pow + 1.0f) * typ->p.stop_falling_sec
        * (safe_powf(1.0f - rate, typ->p.stop_falling_pow + 1.0f) - 1.0f);
    dis -= spd / (typ->p.stop_falling_pow + 1.0f) * typ->p.stop_falling_sec
         * (safe_powf(1.0f - rprev, typ->p.stop_falling_pow + 1.0f) - 1.0f);
    enemy_task_get_pos(act, &cpos);
    enemy_task_get_param_vec(act, &v);
    v3_scale(&v, -dis);
    v3_add(&cpos, &v);
    enemy_task_set_pos(act, &cpos);
    ulSinCos((int)(rate * ANG_HALF - ANG_QUARTER), &sc[0], &sc[1]);
    ulMatrixPush();
    matrix_unit_set(0);
    sphinx_rotate_yxz(w->fall_ang0);
    vu0_matrix_rotate_x(ulMatrixCur, (int)((typ->p.stop_falling_ang - typ->p.fall_ang)
                                   * safe_powf((sc[0] + 1.0f) * 0.5f, typ->p.stop_falling_ang_pow)
                                   + typ->p.fall_ang));
    model_camera_angles(ang);
    ulMatrixPop();
    enemy_task_set_rot(act, ang);
    if (sphinx_next_state(act, w, n, 9))
        w->sign_x = -w->sign_x;
}

/* 0x00b09aa0 */
void sphinx_state_start(void *act, SphinxWork *w)
{
    Vec3 cpos;
    Vec3 v;
    int ang[4];
    int ccur[4];
    int tgt[4];
    float mx[16];
    float my[16];
    float sc[4][2];
    SphinxVars *typ = w->type;
    float rate, rprev, crv, dis, x, y, z;
    int n, roll;

    n = typ->p.start_sec * FPS;
    rate = w->frames / (float)n;
    rprev = (w->frames - 1) / (float)n;
    x = 0.0f;
    y = 0.0f;
    enemy_task_get_pos(act, &cpos);
    ulSinCos((int)(safe_powf(rate, typ->p.start_up_pow) * ANG_HALF - ANG_QUARTER), &sc[0][0],
             &sc[0][1]);
    dis = safe_powf((sc[0][0] + 1.0f) * 0.5f, typ->p.start_up_pow2);
    ulSinCos((int)(safe_powf(rprev, typ->p.start_up_pow) * ANG_HALF - ANG_QUARTER), &sc[0][0],
             &sc[0][1]);
    crv = dis - safe_powf((sc[0][0] + 1.0f) * 0.5f, typ->p.start_up_pow2);
    dis = crv * typ->p.start_up_dis;
    dis += sphinx_bob_delta(act, w, rate, rprev) * rate;
    enemy_task_get_pos(act, &cpos);
    enemy_task_get_param_vec(act, &v);
    vec3_approach(&w->up, &v, rate);
    v3_scale(&v, dis);
    v3_add(&cpos, &v);
    enemy_task_get_rot(act, ang);
    z = -1.0f;
    ROT_X(ang[0], sc[1], y, z);
    ROT_Y(ang[1], sc[2], x, z);
    v3_set(&v, x, y, z);
    v3_scale(&v, -crv * typ->p.start_z_dis);
    v3_add(&cpos, &v);
    enemy_task_set_pos(act, &cpos);
    ulSinCos((int)(safe_powf(rate, typ->p.break_pow_roll) * ANG_HALF - ANG_QUARTER), &sc[3][0],
             &sc[3][1]);
    roll = typ->p.roll_z * w->sign_x * (sc[3][0] + 1.0f) * 0.5f;
    ulMatrixPush();
    matrix_unit_set(0);
    vu0_matrix_rotate_z(ulMatrixCur, roll);
    sphinx_swing_pitch(act, w, mx, rate);
    sphinx_swing_yaw(act, w, my, rate);
    matrix_unit_set(0);
    sphinx_rotate_yxz(w->ang);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, my);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, mx);
    vu0_matrix_rotate_z(ulMatrixCur, roll);
    model_camera_angles(tgt);
    ulMatrixPop();
    i3_copy(ccur, w->ang0);
    ang3_approach(tgt, ccur, rate);
    enemy_task_set_rot(act, ccur);
    if (sphinx_next_state(act, w, n, 1))
        w->sign_x = -w->sign_x;
}

/* steer(cast) */
/* 0x00b09f18 */
void sphinx_update(void *act, SphinxWork *w)
{
    int ang[4];
    int wob[4];
    int lv;

    enemy_task_set_rot(act, w->base_ang);
    if (sphinx_states[w->state])
        sphinx_states[w->state](act, w);
    enemy_task_get_rot(act, w->base_ang);
    sphinx_wobble_sum(act, w, wob);
    ulMatrixPush();
    matrix_unit_set(0);
    sphinx_rotate_yxz(w->base_ang);
    sphinx_rotate_yxz(wob);
    model_camera_angles(ang);
    ulMatrixPop();
    enemy_task_set_rot(act, ang);
    sphinx_pose_model(act, w);
    sphinx_shot_cycle(act, w);
    if (w->fire) {
        lv = ((float (*)(float))float_frac)(enemy_task_get_param(act, 2)) * 1000.0f + 0.5f;
        lv = lv > -1 ? lv : 0;
        e_shot_fire(act, &w->node_pos[w->shot_k], &w->node_axis[w->shot_k], lv < 8 ? lv : 7);
        w->fire = 0;
    }
}

/* 0x00b0a078 */
void sphinx_exec_appear(void *act, SphinxWork *w)
{
    SphinxPart *pt;
    int i;

    enemy_task_appear_scale(act, w);
    if (w->backfire_alpha > 0.0f)
        for (i = 0, pt = w->part; i < 3; i++, pt++)
            pt->lock.flags |= LOCK_LOCKABLE;
    switch (enemy_task_get_phase(act)) {
    case 0:
        break;
    case 2:
        {
            SphinxPart *pp = w->part;
            int k;

            for (k = 2; k >= 0; k--, pp++)
                pp->lock.flags |= LOCK_LOCKABLE;
        }
        break;
    default:
        sphinx_update(act, w);
    }
}

/* 0x00b0a140 */
void sphinx_exec_normal(void *act, SphinxWork *w)
{
    float t;

    if (enemy_task_get_phase(act) != 1)
        return;
    sphinx_update(act, w);
    t = enemy_task_get_time(act);
    if (w->end < t)
        enemy_task_set_state(act, 2);
}

/* 0x00b0a1b0 */
void sphinx_exec_leave(void *act, SphinxWork *w)
{
    SphinxPart *pt;
    int i;

    do { enemy_task_vanish_scale(act, w); } while (0);
    switch (enemy_task_get_phase(act)) {
    case 0:
        for (i = 0, pt = w->part; i < 3; i++, pt++)
            pt->lock.flags &= ~LOCK_LOCKABLE;
        break;
    case 2:
        break;
    default:
        sphinx_update(act, w);
    }
}

/* 0x00b0a240 */
void sphinx_exec_destroyed(void *act, SphinxWork *w)
{
    SpawnDesc dsc;
    SphinxVars *typ;

    if (enemy_task_get_phase(act) == 0) {
        typ = w->type;
        if (++sphinx_destroyed_count == typ->p.bonus_num) {
            sphinx_destroyed_count = 0;
            player_status_add_score_event(sphinx_bonus_name, typ->p.bonus_score);
        }
        nj_fill8(&dsc, 0, sizeof dsc);
        dsc.type = 57;
        dsc.models = stage_models;
        dsc.lock = &w->part[0].lock;
        dsc.kind = 0;
        enemy_task_get_pos(act, &dsc.pos);
        enemy_task_get_rot(act, dsc.ang);
        enemy_task_get_scl(act, &dsc.scale);
        enemy_task_get_param_vec(act, &dsc.v);
        enemy_shatter_create(act, &dsc, 0, 0);
        FINISH_IF(act);
    }
}

/* 0x00b0a330 */
void sphinx_pre_exec(void)
{
}

/* 0x00b0a338 */
void sphinx_exec(void *act, SphinxWork *w)
{
    unsigned int i = enemy_task_get_pstate(act);

    if (sphinx_exec_table[i])
        sphinx_exec_table[i](act, w);
}

/* steer(cast) */
/* 0x00b0a390 */
void sphinx_post_exec(void *act, SphinxWork *w)
{
    Vec3 spos;
    SphinxVars *typ = w->type;
    SphinxPart *pt = w->part;
    int i, mult;

    switch (((unsigned int)enemy_task_get_pstate(act))) {
    case 1:
        w->backfire_alpha = enemy_task_get_state_time(act) / 30.0f;
        break;
    case 0:
        w->backfire_alpha = 1.0f;
        break;
    case 2:
        w->backfire_alpha = 1.0f - enemy_task_get_state_time(act) / FPS;
        break;
    default:
        w->backfire_alpha = 0.0f;
        break;
    }
    if (!enemy_status_get_ticks())
        sphinx_pose_model(act, w);
    enemy_task_get_scl(act, &w->shot_scale);
    v3_scale(&w->shot_scale, typ->p.attack_ray_scl);
    for (i = 0; i < 3; i++, pt++) {
        if (pt->lock.flags & LOCK_HIT) {
            sphinx_wobble_add(act, w);
            v3_copy(&w->hitv, &pt->lock.imp.v);
            if (pt->lock.life > 0)
                continue;
            mult = lockon_hits_max_shot_cnt(&pt->lock);
            enemy_status_score_popup(enemy_task_get_status_slot(act), &pt->pos, typ->p.score, mult);
            if (--w->nparts <= 0 && ((unsigned int)enemy_task_get_pstate(act)) < 2)
                enemy_task_set_state(act, 3);
            ulMatrixApplyPoint((float *)ulMatrixCur, &pt->pos.x, &spos.x);
            project_point(&spos, &spos);
            SoundRhythmSe_EnemyPos(sphinx_init_arg, sphinx_part_sounds[w->nparts], &spos.x);
        }
    }
}

/* steer(cast) */
/* 0x00b0a5d0 */
void sphinx_disp_body(void *act, SphinxWork *w)
{
    Vec3 cpos;
    int ang[4];
    float rgba[4];
    ModelPart *gfx;
    SphinxVars *typ;
    int bl;

    gfx = enemy_task_gfx_ptr(act);
    enemy_task_get_pos(act, &cpos);
    enemy_task_get_rot(act, ang);
    if (view_sphere_visible(&cpos, gfx->radius)) {
        bl = 0;
        typ = w->type;
        if (w->backfire_alpha < 1.0f) {
            bl = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
            heap_bzero(rgba, 16);
            draw_push_strip_flags();
            draw_push_override_color();
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 0x800);
            rgba[0] = w->backfire_alpha - 1.0f;
            draw_set_override_color(rgba);
        }
        ulGxSetTextureList(((void **)act)[1]);
        do {
            ulMatrixPush();
        } while (0);
        vu0_matrix_translate(ulMatrixCur, cpos.x, cpos.y, cpos.z);
        sphinx_rotate_yxz(ang);
        dc_object_draw(gfx);
        if (typ->p.wire) {
            draw_set_color(yuki_beat_color(typ->p.wire_col, typ->p.wire_col2, typ->p.wire_beat,
                                          typ->p.wire_linear, w->backfire_alpha, typ->p.wire_near,
                                          typ->p.wire_far, typ->p.wire_near_rate,
                                          typ->p.wire_far_rate));
            draw_set_blend(4, 5);
            model_object_draw(&w->body, gfx->frame);
        }
        ulMatrixPop();
        if (w->backfire_alpha < 1.0f) {
            draw_pop_strip_flags();
            draw_pop_override_color();
            draw_set_override_flags(bl);
        }
    }
}

/* 0x00b0a790 */
void sphinx_disp(void *act, SphinxWork *w)
{
    unsigned int i = enemy_task_get_pstate(act);

    if (sphinx_disp_table[i])
        sphinx_disp_table[i](act, w);
}

/* steer(cast) */
/* 0x00b0a7e8 */
int sphinx_create_state(void *parent, Placement *pl, int state, int *ang, int extra)
{
    int rec[11];
    ObjDesc dsc;
    ObjFuncs fns = sphinx_funcs;
    SphinxVars *typ;
    float sweep, bsec, tsec;
    int afrm, tfrm, n, rounds, total;
    float ms = 1000.0f, half = 0.5f;

    ((float (*)(float))float_frac)(pl->param0);
    typ = sphinx_types;
    sweep = pl->param3 / 60.0f;
    n = pl->param1;
    rounds = fabsf(pl->param0);
    rec[0] = 0;
    rec[2] = n;
    rec[3] = rounds;
    tsec = sweep * typ->p.turn_z_rate_sec;
    bsec = sweep * typ->p.break_sec_rate;
    ((float *)rec)[7] = sweep;
    ((float *)rec)[8] = tsec;
    ((float *)rec)[5] = bsec;
    ((float *)rec)[6] = tsec;
    rec[4] = ((float (*)(float))float_frac)(pl->param1) * ms + half;
    afrm = (sweep + bsec) * FPS;
    tfrm = (tsec + tsec) * FPS;
    rec[9] = state;
    rec[10] = (int)ang;
    total = (afrm * (n - 2) + tfrm * 2) * (rounds - 1) + (afrm * (n + rec[4] - 2) + tfrm * 2);
    rec[1] = (int)(total + (afrm * typ->p.stop_rate_sec + typ->p.away_sec * FPS))
             + (extra + typ->p.finish_sec * FPS);
    nj_fill8(&dsc, 0, sizeof dsc);
    dsc.name = sphinx_name;
    dsc.funcs = &fns;
    dsc.arg = pl;
    dsc.kind = rec;
    dsc.models = stage_models;
    dsc.tex = stage_textures;
    dsc.hp = typ->p.hp;
    dsc.same_lock = typ->p.same_lock;
    dsc.flags = 18;
    dsc.model_no = 47;
    dsc.model_no2 = -1;
    dsc.score = typ->p.score;
    dsc.hp_reduce = typ->p.hp_reduce;
    dsc.crash_r = typ->p.crash_r;
    dsc.crash_damage = typ->p.crash_damage;
    dsc.time = rec[1];
    dsc.size = 732;
    dsc.dc_flags = 0;
    return enemy_task_task_create(parent, &dsc);
}

/* 0x00b0aac0 */
int sphinx_create(void *parent, Placement *pl)
{
    return sphinx_create_state(parent, pl, 0, 0, 0);
}

/* steer(cast) */
/* 0x00b0aae8 */
int sphinx_create_falling(void *parent, Placement *pl, int *ang)
{
    SphinxVars *typ;

    ((float (*)(float))float_frac)(pl->param0);
    typ = sphinx_types;
    return sphinx_create_state(parent, pl, 7, ang,
                                (typ->p.fall_sec + typ->p.stop_falling_sec + typ->p.start_sec) * FPS);
}

/* 0x00b0ab70 */
void sphinx_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
