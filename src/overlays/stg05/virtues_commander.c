/*
 * Virtues commander, Area 5 enemy: brakes with a swing and hovers before the player, fires up to
 * three lasers (en_virtues_laser_create) that set Virtues bases ahead, then leaves. Outer parts
 * swing on the beat; hits throw streaks (en_vts_cmd_dmg_eff).
 */

#include "stg05.h"
#include "mathdef.h"
#include "debug.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "en_vts_cmd_dmg_eff.h"
#include "../common/en_shatter.h"
#include "../common/enemy_vars.h"

extern int virtues_commander_init_arg;
extern void (*virtues_commander_exec_table[])(void *o, TypeEnemyWork *w);
extern void (*virtues_commander_disp_table[])(void *o, TypeEnemyWork *w);
void virtues_commander_vars_register(void);
void virtues_commander_vars_unregister(void);
int virtues_commander_vars_init(int *prm);
int virtues_commander_vars_release(void);
int virtues_commander_next_state(void *obj, TypeEnemyWork *w, int n, int state);
void virtues_commander_exec(void *obj, TypeEnemyWork *w);
void virtues_commander_disp(void *obj, TypeEnemyWork *w);
void virtues_commander_rotate_yxz(int *ang);
void virtues_commander_undo_damage_angles(void *obj, TypeEnemyWork *w);

/* group name, label, 59 values */
typedef struct VirtuesCommanderSet {
    char *name;
    char *label;                /* 0x04 */
    int v[59];                  /* 0x08 */
} VirtuesCommanderSet;

extern VirtuesCommanderSet virtues_commander_types[1];
extern VarDef virtues_commander_vardef;
extern VarDef virtues_commander_vardefs[59];
extern void (*virtues_commander_states[])(void *o, TypeEnemyWork *w);
void virtues_commander_pose_model(void *obj, TypeEnemyWork *w);
void virtues_commander_update(void *obj, TypeEnemyWork *w);

/* 244 bytes; names in virtues_commander_vardefs */
typedef struct VirtuesCommanderRec {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    int same_lock;              /* 0x0c */
    int score;                  /* 0x10 */
    float hp_reduce;            /* 0x14 */
    float crash_r;              /* 0x18 */
    float crash_damage;         /* 0x1c */
    int parts0_roll_spd;        /* 0x20 */
    int parts1_roll_spd;        /* 0x24 */
    float start_acc;            /* 0x28 */
    float break_sec;            /* 0x2c */
    float break_pow;            /* 0x30 */
    float break_swing_sec;      /* 0x34 */
    float break_swing_dis_rate; /* 0x38 */
    int break_swing_spd;        /* 0x3c */
    float break_swing_pow;      /* 0x40 */
    float break_swing_pow2;     /* 0x44 */
    float break_swing_den_pow;  /* 0x48 */
    float break2_sec;           /* 0x4c */
    float break2_pow;           /* 0x50 */
    float accel_sec;            /* 0x54 */
    float accel_pow;            /* 0x58 */
    float accel_spd_over_rate;  /* 0x5c */
    int hover_acc_chg_spd_xz;   /* 0x60 */
    int hover_acc_chg_spd_y;    /* 0x64 */
    float hover_dis_xz;         /* 0x68 */
    float hover_dis_y;          /* 0x6c */
    float hover_pow;            /* 0x70 */
    float open_sec;             /* 0x74 */
    float laser_sec;            /* 0x78 */
    float wait_sec;             /* 0x7c */
    float close_sec;            /* 0x80 */
    float close_rate;           /* 0x84 */
    float fall_acc;             /* 0x88 */
    float break3_sec;           /* 0x8c */
    float break3_pow;           /* 0x90 */
    float accel2_sec;           /* 0x94 */
    float accel2_pow;           /* 0x98 */
    float accel2_spd_over_rate; /* 0x9c */
    float accel2_motion_pow;    /* 0xa0 */
    float finish_sec;           /* 0xa4 */
    int wire;                   /* 0xa8 */
    int wire_linear;            /* 0xac */
    unsigned int wire_col;      /* 0xb0 */
    unsigned int wire_col2;     /* 0xb4 */
    unsigned int wire_col_core; /* 0xb8 */
    unsigned int wire_col2_core; /* 0xbc */
    float wire_near;            /* 0xc0 */
    float wire_far;             /* 0xc4 */
    float wire_near_rate;       /* 0xc8 */
    float wire_far_rate;        /* 0xcc */
    int wire_beat;              /* 0xd0 */
    float damage_sec;           /* 0xd4 */
    float damage_dis;           /* 0xd8 */
    float damage_pow;           /* 0xdc */
    float damage_pow2;          /* 0xe0 */
    int damage_ang;             /* 0xe4 */
    int damage_roll_spd;        /* 0xe8 */
    float damage_roll_pow;      /* 0xec */
    float damage_roll_pow2;     /* 0xf0 */
} VirtuesCommanderRec;

/* 28 bytes, virtues_commander_laser_sets: count, offsets of the second and third */
typedef struct VirtuesCommanderLasers {
    int n;
    Vec3 off[2];                /* 0x04 */
} VirtuesCommanderLasers;

/* 52 bytes */
typedef struct VirtuesCommanderDamage {
    int n;                      /* 0x00: frames of the damage motion */
    Vec3 move;                  /* 0x04 */
    int ang[3];                 /* 0x10 */
    int ang0[3];                /* 0x1c */
    int cur[3];                 /* 0x28: the angle offset applied */
} VirtuesCommanderDamage;

/* 340 bytes */
typedef struct VirtuesCommanderWork {
    VirtuesCommanderRec *type;
    int state;                  /* 0x04: indexes virtues_commander_states */
    int count;                  /* 0x08: frames of the current state */
    int delay[3];               /* 0x0c: frames to each laser */
    Vec3 hitv;                  /* 0x18: the lock-on record's vector when hit */
    Vec3 tip[1];                /* 0x24: model nodes of flag 0x4000000 */
    void *laser[3];             /* 0x30: the VirtuesLaser tasks (en_virtues_laser_create) */
    float speed;                /* 0x3c */
    int roll[2];                /* 0x40: beat swing of parts 0 and 1 */
    int roll_off[2];            /* 0x48: accumulated on each beat */
    int k1;                     /* 0x50: creation word 1 */
    VirtuesCommanderLasers *lasers;        /* 0x54 */
    ModelPart body;                /* 0x58: models 202/205 */
    ModelPart wire;                /* 0xb4: model 203 */
    int hover;                  /* 0x110: frames of the hover state */
    int interval;               /* 0x114: frames between lasers */
    float t;                    /* 0x118: hover time */
    float t0;                   /* 0x11c: the previous hover time */
    VirtuesCommanderDamage dm;                /* 0x120: the damage motion */
} VirtuesCommanderWork;

typedef struct VirtuesCommanderNodeArg {
    VirtuesCommanderWork *work;
    int n;
} VirtuesCommanderNodeArg;

extern char virtues_commander_lock_name[];
extern char virtues_commander_name[];
extern VirtuesCommanderLasers virtues_commander_laser_sets[5];
extern char virtues_commander_funcs[];
int virtues_commander_node_cb(unsigned int *n, int idx, VirtuesCommanderWork *w);
int virtues_commander_tip_node_cb(unsigned int *n, int idx, VirtuesCommanderNodeArg *prm);
void virtues_commander_damage_reset(void *obj, VirtuesCommanderWork *w);
void virtues_commander_damage_move(void *obj, VirtuesCommanderWork *w, Vec3 *out);
void virtues_commander_damage_angles(void *obj, VirtuesCommanderWork *w, int *out);
void virtues_commander_damage_step(void *obj, VirtuesCommanderWork *w);
void virtues_commander_damage_start(void *obj, VirtuesCommanderWork *w);
float virtues_commander_hover_offset(void *obj, VirtuesCommanderWork *w, int spd, float dis);
void virtues_commander_hover_step(void *obj, VirtuesCommanderWork *w);

/* 0x00b1bb78 */
void virtues_commander_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&virtues_commander_vardef);
    for (i = 0; i < 1; i++) {
        def = virtues_commander_vardefs;
        val = virtues_commander_types[i].v;
        virtues_commander_vardef.name = virtues_commander_types[i].name;
        virtues_commander_vardef.fmt = virtues_commander_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&virtues_commander_vardef);
        for (j = 0; j < 59; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)virtues_commander_vardefs);
    }
}

/* 0x00b1bc90 */
void virtues_commander_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b1bc98 */
int virtues_commander_get_init_arg(void)
{
    return virtues_commander_init_arg;
}

/* 0x00b1bca8 */
int virtues_commander_vars_init(int *prm)
{
    virtues_commander_init_arg = *prm;
    virtues_commander_vars_register();
    return 0;
}

/* 0x00b1bcd0 */
int virtues_commander_vars_release(void)
{
    virtues_commander_vars_unregister();
    return 0;
}

/* 0x00b1bcf0 */
int virtues_commander_marker_draw(int obj, void *place)
{
    int *mdl;

    ulGxSetTextureList(stage_textures);
    mdl = pnb_get_entry(stage_models, 201);
    return obj_draw_at(obj, place, (void *)*mdl);
}

/* 0x00b1bd48 */
int virtues_commander_node_cb(unsigned int *n, int idx, VirtuesCommanderWork *w)
{
    if (n[0] & 0x1000000)
        vu0_matrix_rotate_y(ulMatrixCur, w->roll[0] + w->roll_off[0]);
    if (n[0] & 0x2000000)
        vu0_matrix_rotate_y(ulMatrixCur, w->roll[1] + w->roll_off[1]);
    return 1;
}

/* 0x00b1bdc8 */
int virtues_commander_tip_node_cb(unsigned int *n, int idx, VirtuesCommanderNodeArg *prm)
{
    if (n[0] & 0x1000000)
        vu0_matrix_rotate_y(ulMatrixCur, prm->work->roll[0] + prm->work->roll_off[0]);
    if (n[0] & 0x2000000)
        vu0_matrix_rotate_y(ulMatrixCur, prm->work->roll[1] + prm->work->roll_off[1]);
    if (n[0] & 0x4000000)
        ulMatrixGetTranslation((float *)ulMatrixCur, &prm->work->tip[prm->n++].x);
    return 0;
}

/* 0x00b1be90 */
void virtues_commander_pose_model(void *obj, TypeEnemyWork *work)
{
    VirtuesCommanderWork *w = (VirtuesCommanderWork *)work;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 p;
    int ang[3];
    Vec3 scl;
    VirtuesCommanderNodeArg prm;

    prm.work = w;
    prm.n = 0;
    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    enemy_task_get_scl(obj, &scl);
    dc_object_set_callback(gfx, virtues_commander_tip_node_cb, &prm);
    do {
        ulMatrixPush();
    } while (0);
    matrix_unit_set(0);
    vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
    virtues_commander_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
    dc_object_draw(gfx);
    ulMatrixPop();
    dc_object_set_callback(gfx, virtues_commander_node_cb, w);
}

/* 0x00b1bf88 */
void virtues_commander_damage_reset(void *obj, VirtuesCommanderWork *w)
{
    VirtuesCommanderRec *typ = w->type;

    w->dm.n = typ->damage_sec * FPS;
    v3_set(&w->dm.move, 0, 0, 0);
    i3_set(w->dm.ang, 0, 0, 0);
    i3_set(w->dm.cur, 0, 0, 0);
}

/* 0x00b1bfd8 */
void virtues_commander_release_hook(void *obj, VirtuesCommanderWork *w)
{
}

/* 0x00b1bfe0 */
void virtues_commander_damage_move(void *obj, VirtuesCommanderWork *w, Vec3 *out)
{
    VirtuesCommanderRec *typ = w->type;
    int n = typ->damage_sec * FPS;
    float sc[2];
    float rate, prev, ease;

    if (w->dm.n >= n) {
        v3_set(out, 0, 0, 0);
    } else {
        rate = w->dm.n / (float)n;
        prev = (w->dm.n - 1) / (float)n;
        ulSinCos(safe_powf(rate, typ->damage_pow) * ANG_HALF - ANG_QUARTER, &sc[0], &sc[1]);
        ease = safe_powf((sc[0] + 1.0f) * 0.5f, typ->damage_pow2);
        ulSinCos(safe_powf(prev, typ->damage_pow) * ANG_HALF - ANG_QUARTER, &sc[0], &sc[1]);
        ease -= safe_powf((sc[0] + 1.0f) * 0.5f, typ->damage_pow2);
        v3_mul(out, &w->dm.move, ease);
    }
}

/* 0x00b1c180 */
void virtues_commander_damage_angles(void *obj, VirtuesCommanderWork *w, int *out)
{
    VirtuesCommanderRec *typ = w->type;
    int n = typ->damage_sec * FPS;
    int dang[3];
    float sc[2];
    float rate, k;
    int ang;
    VirtuesCommanderDamage *dmg = &w->dm;

    if (dmg->n >= n) {
        i3_set(out, 0, 0, 0);
    } else {
        rate = dmg->n / (float)n;
        ang = typ->damage_roll_spd * safe_powf(rate, typ->damage_roll_pow);
        if (ang < ANG_90)
            ulSinCos(ang * 2 - ANG_90, &sc[0], &sc[1]);
        else
            ulSinCos(ang, &sc[0], &sc[1]);
        k = sc[0] * safe_powf(1.0f - rate, typ->damage_roll_pow2);
        i3_mul(out, dmg->ang, k);
        i3_mul(dang, dmg->ang0, safe_powf(1.0f - rate, typ->damage_roll_pow2));
        i3_add(out, dang);
    }
}

/* 0x00b1c340 */
void virtues_commander_undo_damage_angles(void *obj, TypeEnemyWork *work)
{
    VirtuesCommanderWork *w = (VirtuesCommanderWork *)work;
    int ang[3];
    int *dang;

    enemy_task_get_rot(obj, ang);
    i3_sub(ang, ang, w->dm.cur);
    enemy_task_set_rot(obj, ang);
}

/* 0x00b1c3b8 */
void virtues_commander_damage_step(void *obj, VirtuesCommanderWork *w)
{
    VirtuesCommanderRec *typ = w->type;
    Vec3 p;
    Vec3 mv;
    int ang[3];
    int *dang = w->dm.cur;

    enemy_task_get_pos(obj, &p);
    virtues_commander_damage_move(obj, w, &mv);
    v3_add(&p, &mv);
    enemy_task_set_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    virtues_commander_damage_angles(obj, w, dang);
    ang[0] += dang[0];
    ang[1] += dang[1];
    ang[2] += dang[2];
    enemy_task_set_rot(obj, ang);
    w->dm.n = MIN(w->dm.n + 1, typ->damage_sec * FPS);
}

/* 0x00b1c4f8 */
void virtues_commander_damage_start(void *obj, VirtuesCommanderWork *w)
{
    VirtuesCommanderRec *typ = w->type;
    Vec3 mv;
    Vec3 push;
    float sc[2];
    VirtuesCommanderDamage *dmg = &w->dm;

    vec3_normalize(&push, &w->hitv);
    v3_scale(&push, typ->damage_dis);
    virtues_commander_damage_move(obj, w, &mv);
    v3_sum(&dmg->move, &push, &mv);
    virtues_commander_damage_angles(obj, w, dmg->ang0);
    ulSinCos(DEG_TO_ANG(RAND_FLOAT_A() * 360.0f), &sc[0], &sc[1]);
    dmg->ang[0] = sc[0] * typ->damage_ang;
    dmg->ang[1] = 0;
    dmg->ang[2] = sc[1] * typ->damage_ang;
    w->dm.n = 1;
}

/* steer(cast) */
/* 0x00b1c658 */
void virtues_commander_init(void *obj, VirtuesCommanderWork *w, void *prm, int *k)
{
    VirtuesCommanderRec *typ = (VirtuesCommanderRec *)&virtues_commander_types[*k];
    Vec3 v;
    int ang[3];
    int *root;
    int *clip;
    ModelPart *gfx;
    int i;
    ModelPart *body, *wr;

    w->type = typ;
    enemy_task_clear_flags(obj, 64);
    do {
        void *m_ = stage_models, *t_ = stage_textures;
        ((void **)obj)[0] = m_;
        ((void **)obj)[1] = t_;
    } while (0);
    body = &w->body;
    wr = &w->wire;
    root = ((int *)pnb_get_entry(((void **)obj)[0], 202));
    clip = ((int *)pnb_get_entry(((void **)obj)[0], 205));
    dc_object_init(body, *root, *clip, 1280);
    dc_object_set_wire(body, model_pnb_get_wire(((void **)obj)[0], 202));
    dc_object_set_callback(body, virtues_commander_node_cb, w);
    dc_object_init(wr, *((struct DcNode **)pnb_get_entry(((void **)obj)[0], 203)), 0, 1280);
    dc_object_set_wire(wr, model_pnb_get_wire(((void **)obj)[0], 203));
    gfx = enemy_task_gfx_ptr(obj);
    gfx->radius = w->body.radius = w->wire.radius = 35.0f;
    lockon_obj_set_name(enemy_task_lock_ptr(obj), virtues_commander_lock_name);
    ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_SILENT;
    w->hover = k[2];
    w->interval = k[3];
    w->lasers = ((VirtuesCommanderLasers **)k)[4];
    for (i = 0; i < 3; i++) {
        w->delay[i] = k[3] * i;
        w->laser[i] = 0;
    }
    enemy_task_get_param_vec(obj, &v);
    w->speed = vec3_length(&v);
    v.y = -1.0f;
    v.x = 0;
    v.z = 0;
    enemy_task_set_param_vec(obj, &v);
    enemy_task_get_rot(obj, ang);
    ang[2] = 0;
    ang[0] = 0;
    enemy_task_set_rot(obj, ang);
    w->roll_off[1] = 0;
    w->count = 1;
    w->roll_off[0] = 0;
    w->roll[1] = 0;
    w->roll[0] = 0;
    w->state = 0;
    w->t = 0;
    w->k1 = k[1];
    w->t0 = 0;
    enemy_task_set_appear(obj, 2, 30);
    enemy_task_set_vanish(obj, 2, 60);
    virtues_commander_damage_reset(obj, w);
}

/* 0x00b1c8c0 */
void virtues_commander_release(void *obj, VirtuesCommanderWork *w)
{
    int i;

    virtues_commander_release_hook(obj, w);
    dc_object_release(&w->body);
    dc_object_release(&w->wire);
    for (i = 0; i < 3; i++) {
        if (w->laser[i])
            en_virtues_laser_detach(w->laser[i]);
    }
}

/* 0x00b1c930 */
int virtues_commander_next_state(void *obj, TypeEnemyWork *w, int n, int state)
{
    if (++w->frames > n) {
        w->state = state;
        w->frames = 1;
        return 1;
    }
    return 0;
}

/* 0x00b1c960 */
void virtues_commander_state_arrive(void *obj, VirtuesCommanderWork *w)
{
    VirtuesCommanderRec *typ = w->type;
    int n = enemy_task_get_param(obj, 4);
    Vec3 p;
    Vec3 v;
    float spd;

    enemy_task_get_param_vec(obj, &v);
    spd = (w->speed * FPS + w->count / FPS * typ->start_acc) * (1.0f / FPS)
        + typ->start_acc * 0.5f * (1.0f / FPS) * (1.0f / FPS);
    v3_scale(&v, spd);
    enemy_task_get_pos(obj, &p);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    if (virtues_commander_next_state(obj, (TypeEnemyWork *)w, n, 1))
        w->speed = spd;
}

/* 0x00b1caa8 */
void virtues_commander_state_break(void *obj, VirtuesCommanderWork *w)
{
    VirtuesCommanderRec *typ = w->type;
    Vec3 p, v;
    float unit, rate, prev, dis;
    int n;

    unit = 1.0f;
    n = typ->break_sec * FPS;
    rate = w->count / (float)n;
    prev = (w->count - 1) / (float)n;
    dis = -(w->speed * FPS) / (typ->break_pow + unit) * typ->break_sec * (safe_powf(unit - rate, typ->break_pow + unit) - unit);
    dis -= -(w->speed * FPS) / (typ->break_pow + unit) * typ->break_sec * (safe_powf(unit - prev, typ->break_pow + unit) - unit);
    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, dis);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    ((ModelPart *)enemy_task_gfx_ptr(obj))->frame = rate * 14.0f;
    virtues_commander_next_state(obj, (TypeEnemyWork *)w, n, 2);
}

/* 0x00b1cc80 */
void virtues_commander_state_accel(void *obj, VirtuesCommanderWork *w)
{
    VirtuesCommanderRec *typ = w->type;
    Vec3 p, v;
    float rate, prev, dis;
    int n;

    n = typ->accel_sec * FPS;
    rate = w->count / (float)n;
    prev = (w->count - 1) / (float)n;
    enemy_task_get_pos(obj, &p);
    dis = w->speed * FPS / (typ->accel_pow + 1.0f) * typ->accel_sec * (typ->accel_spd_over_rate + 1.0f)
        * safe_powf(rate, typ->accel_pow + 1.0f);
    dis -= w->speed * FPS / (typ->accel_pow + 1.0f) * typ->accel_sec * (typ->accel_spd_over_rate + 1.0f)
         * safe_powf(prev, typ->accel_pow + 1.0f);
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, -dis);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    ((ModelPart *)enemy_task_gfx_ptr(obj))->frame = rate * 9.0f + 14.0f;
    if (virtues_commander_next_state(obj, (TypeEnemyWork *)w, n, 3))
        w->speed = dis;
}

/* 0x00b1ce68 */
void virtues_commander_state_break2(void *obj, VirtuesCommanderWork *w)
{
    VirtuesCommanderRec *typ = w->type;
    Vec3 p, v;
    float unit, rate, prev, dis;
    int n;

    unit = 1.0f;
    n = typ->break2_sec * FPS;
    rate = w->count / (float)n;
    prev = (w->count - 1) / (float)n;
    dis = -(w->speed * FPS) / (typ->break2_pow + unit) * typ->break2_sec * (safe_powf(unit - rate, typ->break2_pow + unit) - unit);
    dis -= -(w->speed * FPS) / (typ->break2_pow + unit) * typ->break2_sec * (safe_powf(unit - prev, typ->break2_pow + unit) - unit);
    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, -dis);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    ((ModelPart *)enemy_task_gfx_ptr(obj))->frame = rate * 11.0f + 23.0f;
    virtues_commander_next_state(obj, (TypeEnemyWork *)w, n, 4);
}

/* 0x00b1d050 */
float virtues_commander_hover_offset(void *obj, VirtuesCommanderWork *w, int spd, float dis)
{
    float sc[2];
    float v;
    int ang;

    ang = spd * (w->t / FPS);
    if (ang < ANG_90) {
        ulSinCos(ang * 2 - ANG_90, &sc[0], &sc[1]);
        v = safe_powf((sc[0] + 1.0f) * 0.5f, w->type->hover_pow);
    } else {
        ulSinCos(ang, &sc[0], &sc[1]);
        if (fabsf(sc[0]) < FLT_EPSILON)
            v = sc[0];
        else if (0.0f < sc[0])
            v = safe_powf(sc[0], w->type->hover_pow);
        else
            v = -safe_powf(-sc[0], w->type->hover_pow);
    }
    ang = spd * (w->t0 / FPS);
    if (ang < ANG_90) {
        ulSinCos(ang * 2 - ANG_90, &sc[0], &sc[1]);
        v -= safe_powf((sc[0] + 1.0f) * 0.5f, w->type->hover_pow);
    } else {
        ulSinCos(ang, &sc[0], &sc[1]);
        if (fabsf(sc[0]) < FLT_EPSILON)
            v -= sc[0];
        else if (0.0f < sc[0])
            v -= safe_powf(sc[0], w->type->hover_pow);
        else
            v += safe_powf(-sc[0], w->type->hover_pow);
    }
    return dis * v;
}

/* 0x00b1d240 */
void virtues_commander_state_break_swing(void *obj, VirtuesCommanderWork *w)
{
    VirtuesCommanderRec *typ = w->type;
    int n = typ->break_swing_sec * FPS;
    float rate = w->count / (float)n;
    float prev = (w->count - 1) / (float)n;
    Vec3 p;
    int ang[3];
    float sc[2][2];
    float dis, ease, ease_prev, hover;

    enemy_task_get_pos(obj, &p);
    dis = w->speed * FPS * typ->break_sec * typ->break_swing_dis_rate;
    ulSinCos(typ->break_swing_spd * safe_powf(rate, typ->break_swing_pow) - ANG_QUARTER, &sc[0][0],
             &sc[0][1]);
    ease = safe_powf((sc[0][0] + 1.0f) * 0.5f, typ->break_swing_pow2);
    ease *= safe_powf(1.0f - rate, typ->break_swing_den_pow);
    ulSinCos(typ->break_swing_spd * safe_powf(prev, typ->break_swing_pow) - ANG_QUARTER, &sc[0][0],
             &sc[0][1]);
    ease_prev = safe_powf((sc[0][0] + 1.0f) * 0.5f, typ->break_swing_pow2);
    ease_prev *= safe_powf(1.0f - prev, typ->break_swing_den_pow);
    p.y -= dis * (ease - ease_prev);
    hover = virtues_commander_hover_offset(obj, w, typ->hover_acc_chg_spd_xz, typ->hover_dis_xz);
    enemy_task_get_rot(obj, ang);
    ulSinCos(ang[1], &sc[1][0], &sc[1][1]);
    p.x += sc[1][1] * hover;
    p.z += sc[1][0] * hover;
    hover = virtues_commander_hover_offset(obj, w, typ->hover_acc_chg_spd_y, typ->hover_dis_y);
    w->t0 = w->t;
    p.y += hover;
    w->t = w->t + rate;
    enemy_task_set_pos(obj, &p);
    {
        ModelPart *gfx = enemy_task_gfx_ptr(obj);
        gfx->frame = (gfx->frames - 14.0f) * rate + 14.0f;
    }
    virtues_commander_next_state(obj, (TypeEnemyWork *)w, n, 5);
}

/* 0x00b1d4f0 */
void virtues_commander_hover_step(void *obj, VirtuesCommanderWork *w)
{
    VirtuesCommanderRec *typ = w->type;
    Vec3 p;
    int ang[3];
    float sc[2];
    float hover;

    enemy_task_get_pos(obj, &p);
    hover = virtues_commander_hover_offset(obj, w, typ->hover_acc_chg_spd_xz, typ->hover_dis_xz);
    enemy_task_get_rot(obj, ang);
    ulSinCos(ang[1], &sc[0], &sc[1]);
    p.x += sc[1] * hover;
    p.z += sc[0] * hover;
    hover = virtues_commander_hover_offset(obj, w, typ->hover_acc_chg_spd_y, typ->hover_dis_y);
    w->t0 = w->t;
    p.y += hover;
    w->t = w->t + 1.0f;
    enemy_task_set_pos(obj, &p);
}

/* steer(cast) */
/* 0x00b1d5d8 */
void virtues_commander_state_hover(void *obj, VirtuesCommanderWork *w)
{
    int n = w->hover;
    ModelPart *gfx;

    virtues_commander_hover_step(obj, w);
    if (virtues_commander_next_state(obj, (TypeEnemyWork *)w, n, 6)) {
        gfx = enemy_task_gfx_ptr(obj);
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(((void **)obj)[0], 206)));
        gfx->frame = 0;
        dc_object_set_clip(&w->body, *((struct DcClip **)pnb_get_entry(((void **)obj)[0], 207)));
        w->body.frame = 0;
    }
}

/* 0x00b1d680 */
void virtues_commander_state_open(void *obj, VirtuesCommanderWork *w)
{
    int n = w->type->open_sec * FPS;
    float rate = w->count / (float)n;
    ModelPart *gfx;

    virtues_commander_hover_step(obj, w);
    gfx = enemy_task_gfx_ptr(obj);
    gfx->frame = gfx->frames * rate;
    virtues_commander_next_state(obj, (TypeEnemyWork *)w, n, 7);
}

/* steer(cast) */
/* 0x00b1d720 */
void virtues_commander_state_laser(void *obj, VirtuesCommanderWork *w)
{
    VirtuesCommanderRec *typ = w->type;
    VirtuesCommanderLasers *lasers = w->lasers;
    int n = (float)(w->interval * (lasers->n - 1)) + (typ->laser_sec + typ->wait_sec) * FPS;
    EnVirtuesBaseArg prm;
    ModelPart *gfx;
    int i;

    virtues_commander_hover_step(obj, w);
    for (i = 0; i < lasers->n; i++) {
        if (--w->delay[i] == -1) {
            prm.pts = w->tip;
            prm.n = 1;
            prm.h = (int)enemy_task_get_param(obj, 1);
            prm.frames = typ->laser_sec * FPS;
            prm.off.x = enemy_task_get_param(obj, 2);
            prm.off.y = enemy_task_get_param(obj, 3);
            prm.off.z = 0;
            prm.fall = enemy_task_get_param(obj, 5);
            if (i > 0) {
                Vec3 *p = &lasers->off[i - 1];
                prm.off.x += p->x;
                prm.off.y += p->y;
                prm.off.z = p->z;
            }
            w->laser[i] = en_virtues_laser_create(obj, &prm);
            SoundRhythmSe_Enemy(virtues_commander_init_arg, 8);
        } else if (w->laser[i] && (TASK_OF(w->laser[i])->flags & TASK_F_END))
            w->laser[i] = 0;
    }
    if (virtues_commander_next_state(obj, (TypeEnemyWork *)w, n, 8)) {
        gfx = enemy_task_gfx_ptr(obj);
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(((void **)obj)[0], 208)));
        gfx->frame = 0;
        dc_object_set_clip(&w->body, *((struct DcClip **)pnb_get_entry(((void **)obj)[0], 209)));
        w->body.frame = 0;
    }
}

/* 0x00b1d980 */
void virtues_commander_state_close(void *obj, VirtuesCommanderWork *w)
{
    VirtuesCommanderRec *typ = w->type;
    int n = typ->close_sec * FPS;
    float unit = 1.0f;
    float rate = w->count / (float)n;
    Vec3 p;
    int ang[3];
    float sc[2];
    float fall, hover;
    ModelPart *gfx;

    enemy_task_get_pos(obj, &p);
    fall = w->count / FPS * typ->fall_acc * (1.0f / FPS) + typ->fall_acc * 0.5f * (1.0f / FPS) * (1.0f / FPS);
    p.y -= fall;
    hover = virtues_commander_hover_offset(obj, w, typ->hover_acc_chg_spd_xz, typ->hover_dis_xz);
    enemy_task_get_rot(obj, ang);
    ulSinCos(ang[1], &sc[0], &sc[1]);
    p.x += sc[1] * hover;
    p.z += sc[0] * hover;
    hover = virtues_commander_hover_offset(obj, w, typ->hover_acc_chg_spd_y, typ->hover_dis_y);
    w->t0 = w->t;
    p.y += hover;
    w->t = w->t + (unit - rate);
    enemy_task_set_pos(obj, &p);
    gfx = enemy_task_gfx_ptr(obj);
    {
        float k = rate / typ->close_rate;
        if (unit < k)
            k = 23.0f;
        else
            k = k * 23.0f;
        gfx->frame = k;
    }
    if (virtues_commander_next_state(obj, (TypeEnemyWork *)w, n, 9))
        w->speed = fall;
}

/* 0x00b1db70 */
void virtues_commander_state_break3(void *obj, VirtuesCommanderWork *w)
{
    VirtuesCommanderRec *typ = w->type;
    Vec3 p, v;
    float unit, rate, prev, dis;
    int n;

    unit = 1.0f;
    n = typ->break3_sec * FPS;
    rate = w->count / (float)n;
    prev = (w->count - 1) / (float)n;
    dis = -(w->speed * FPS) / (typ->break3_pow + unit) * typ->break3_sec * (safe_powf(unit - rate, typ->break3_pow + unit) - unit);
    dis -= -(w->speed * FPS) / (typ->break3_pow + unit) * typ->break3_sec * (safe_powf(unit - prev, typ->break3_pow + unit) - unit);
    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, dis);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    ((ModelPart *)enemy_task_gfx_ptr(obj))->frame = rate * 17.0f + 23.0f;
    virtues_commander_next_state(obj, (TypeEnemyWork *)w, n, 10);
}

/* 0x00b1dd58 */
void virtues_commander_state_accel2(void *obj, VirtuesCommanderWork *w)
{
    VirtuesCommanderRec *typ = w->type;
    Vec3 p, v;
    float rate, prev, dis;
    int n;
    ModelPart *gfx;

    n = typ->accel2_sec * FPS;
    rate = w->count / (float)n;
    prev = (w->count - 1) / (float)n;
    enemy_task_get_pos(obj, &p);
    dis = w->speed * FPS / (typ->accel2_pow + 1.0f) * typ->accel2_sec * (typ->accel2_spd_over_rate + 1.0f)
        * safe_powf(rate, typ->accel2_pow + 1.0f);
    dis -= w->speed * FPS / (typ->accel2_pow + 1.0f) * typ->accel2_sec * (typ->accel2_spd_over_rate + 1.0f)
         * safe_powf(prev, typ->accel2_pow + 1.0f);
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, -dis);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    gfx = enemy_task_gfx_ptr(obj);
    gfx->frame = (gfx->frames - 40.0f) * safe_powf(rate, typ->accel2_motion_pow) + 40.0f;
    virtues_commander_next_state(obj, (TypeEnemyWork *)w, n, 11);
}

/* 0x00b1df58 */
void virtues_commander_state_finish(void *obj, VirtuesCommanderWork *w)
{
    Vec3 p, v;
    float spd_base, spd_over, dis;
    int n;

    spd_base = w->speed * FPS;
    n = w->type->finish_sec * FPS;
    spd_over = w->type->accel2_spd_over_rate * spd_base + spd_base;
    dis = ((spd_base - spd_over) * (w->count / (float)n) * 0.5f + spd_over) * (w->count / FPS)
        - ((spd_base - spd_over) * ((w->count - 1) / (float)n) * 0.5f + spd_over) * ((w->count - 1) / FPS);
    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, -dis);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    virtues_commander_next_state(obj, (TypeEnemyWork *)w, n, 12);
}

/* 0x00b1e0c8 */
void virtues_commander_state_retreat(void *obj, TypeEnemyWork *w)
{
    Vec3 p;
    Vec3 v;

    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, -w->speed);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
}

/* 0x00b1e168 */
void virtues_commander_update(void *obj, TypeEnemyWork *w)
{
    void (*fn)(void *obj, TypeEnemyWork *w) = virtues_commander_states[w->state];

    if (fn)
        fn(obj, w);
    virtues_commander_pose_model(obj, w);
}

/* 0x00b1e1c0 */
void virtues_commander_exec_appear(void *obj, TypeEnemyWork *w)
{
    enemy_task_appear_scale(obj, w);
    switch (enemy_task_get_phase(obj)) {
    case 0:
        return;
    case 2:
        ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_LOCKABLE;
        return;
    }
    virtues_commander_update(obj, w);
}

/* 0x00b1e240 */
void virtues_commander_exec_normal(void *obj, TypeEnemyWork *w)
{
    float frm;

    if (enemy_task_get_phase(obj) != 1)
        return;
    virtues_commander_update(obj, w);
    frm = enemy_task_get_time(obj);
    if (w->start < frm)
        enemy_task_set_state(obj, 2);
}

/* 0x00b1e2b0 */
void virtues_commander_exec_leave(void *obj, TypeEnemyWork *w)
{
    enemy_task_vanish_scale(obj, w);
    switch (enemy_task_get_phase(obj)) {
    case 0:
        ((LockObj *)enemy_task_lock_ptr(obj))->flags &= ~LOCK_LOCKABLE;
        break;
    case 2:
        break;
    default:
        virtues_commander_update(obj, w);
    }
}

/* 0x00b1e330 */
void virtues_commander_exec_destroyed(void *obj, TypeEnemyWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 0) {
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 210;
        desc.models = stage_models;
        desc.kind = 0;
        desc.lock = enemy_task_lock_ptr(obj);
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        enemy_task_get_param_vec(obj, &desc.v);
        en_shatter_spawn(obj, &desc, 2);
        FINISH_IF(obj);
    }
}

/* 0x00b1e3e8 */
void virtues_commander_pre_exec(void *obj, TypeEnemyWork *w)
{
    virtues_commander_undo_damage_angles(obj, w);
}

/* 0x00b1e3f0 */
void virtues_commander_exec(void *obj, TypeEnemyWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (virtues_commander_exec_table[i])
        virtues_commander_exec_table[i](obj, w);
}

/* 0x00b1e448 */
void virtues_commander_post_exec(void *obj, VirtuesCommanderWork *w)
{
    VirtuesCommanderRec *typ = w->type;
    LockObj *lock = enemy_task_lock_ptr(obj);
    void *arg[2];
    Vec3 vpos;
    float sc[2];

    if (!beat_grid_hit(0)) {
        ulSinCos(beat_clock_get_phase() * ANG_QUARTER, &sc[0], &sc[1]);
        w->roll[0] = typ->parts0_roll_spd * sc[0];
        w->roll[1] = typ->parts1_roll_spd * sc[0];
    } else {
        w->roll_off[0] += typ->parts0_roll_spd;
        w->roll[0] = 0;
        w->roll_off[1] += typ->parts1_roll_spd;
        w->roll[1] = 0;
    }
    virtues_commander_damage_step(obj, w);
    virtues_commander_pose_model(obj, (TypeEnemyWork *)w);
    if (lock->flags & LOCK_HIT) {
        v3_copy(&w->hitv, &lock->imp.v);
        virtues_commander_damage_start(obj, w);
        arg[1] = enemy_task_pos_ptr(obj);
        arg[0] = 0;
        en_vts_cmd_dmg_eff_create(obj, arg);
        enemy_task_get_pos(obj, &vpos);
        ulMatrixApplyPoint((float *)ulMatrixCur, &vpos.x, &vpos.x);
        project_point(&vpos, &vpos);
        switch (lock->life) {
        case 0:
            SoundRhythmSe_EnemyPos(virtues_commander_init_arg, 1, &vpos.x);
            break;
        case 1:
            SoundRhythmSe_EnemyPos(virtues_commander_init_arg, 7, &vpos.x);
            break;
        case 2:
            SoundRhythmSe_EnemyPos(virtues_commander_init_arg, 6, &vpos.x);
            break;
        case 3:
            SoundRhythmSe_EnemyPos(virtues_commander_init_arg, 5, &vpos.x);
            break;
        case 4:
            SoundRhythmSe_EnemyPos(virtues_commander_init_arg, 4, &vpos.x);
            break;
        case 5:
            SoundRhythmSe_EnemyPos(virtues_commander_init_arg, 3, &vpos.x);
            break;
        case 6:
            SoundRhythmSe_EnemyPos(virtues_commander_init_arg, 2, &vpos.x);
            break;
        case 7:
            SoundRhythmSe_EnemyPos(virtues_commander_init_arg, 0, &vpos.x);
            break;
        }
    }
}

/* steer(cast) */
/* 0x00b1e6c0 */
void virtues_commander_disp_body(void *obj, VirtuesCommanderWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 p;
    int ang[3];
    Vec3 scl;
    float col_add[4];
    VirtuesCommanderRec *typ;
    float k;
    int prev;

    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    enemy_task_get_scl(obj, &scl);
    if (view_sphere_visible(&p, gfx->radius)) {
        ulGxSetTextureList(((void **)obj)[1]);
        k = 1.0f;
        prev = 0;
        ulMatrixPush();
        typ = w->type;
        switch (((unsigned int)enemy_task_get_pstate(obj))) {
        case 1:
            prev = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
            heap_bzero(col_add, 16);
            draw_push_strip_flags();
            draw_push_override_color();
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 0x800);
            k = enemy_task_get_state_time(obj) / 30.0f;
            k = 0.0f <= k ? (k < 1.0f ? (0.0f <= k ? k : 0.0f) : 1.0f) : 0.0f;
            col_add[0] = k - 1.0f;
            draw_set_override_color(col_add);
            break;
        case 2:
            prev = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
            heap_bzero(col_add, 16);
            draw_push_strip_flags();
            draw_push_override_color();
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 0x800);
            k = 1.0f - enemy_task_get_state_time(obj) / FPS;
            k = 0.0f <= k ? (k < 1.0f ? (0.0f <= k ? k : 0.0f) : 1.0f) : 0.0f;
            col_add[0] = k - 1.0f;
            draw_set_override_color(col_add);
            break;
        }
        do {
            Vec3 *p_ = &p;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        virtues_commander_rotate_yxz(ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
        dc_object_draw(gfx);
        if (typ->wire) {
            draw_set_blend(4, 5);
            draw_set_color(yuki_beat_color(typ->wire_col, typ->wire_col2, typ->wire_beat, typ->wire_linear, k, typ->wire_near,
                                           typ->wire_far, typ->wire_near_rate, typ->wire_far_rate));
            model_object_draw(&w->body, gfx->frame);
            draw_set_color(yuki_beat_color(typ->wire_col_core, typ->wire_col2_core, typ->wire_beat, typ->wire_linear, k,
                                           typ->wire_near, typ->wire_far, typ->wire_near_rate, typ->wire_far_rate));
            dc_object_draw(&w->wire);
        }
        switch (((unsigned int)enemy_task_get_pstate(obj))) {
        case 1:
        case 2:
            draw_pop_strip_flags();
            draw_pop_override_color();
            draw_set_override_flags(prev);
            break;
        }
        ulMatrixPop();
    }
}

/* 0x00b1ea30 */
void virtues_commander_disp(void *obj, TypeEnemyWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (virtues_commander_disp_table[i])
        virtues_commander_disp_table[i](obj, w);
}

/* steer(cast) */
/* 0x00b1ea88 */
int virtues_commander_create(void *obj, ShotArg *place)
{
    int k[5];
    ObjDesc desc;
    ObjFuncs funcs = *(ObjFuncs *)virtues_commander_funcs;
    VirtuesCommanderRec *typ = (VirtuesCommanderRec *)virtues_commander_types;
    int i;

    k[0] = 0;
    k[2] = place->move;
    k[3] = ((float (*)(float))float_frac)(place->delay) * 1000.0f + 0.5f;
    i = ((float (*)(float))float_frac)(*(float *)&place->params[2]) * 1000.0f + 0.5f;
    i = i > -1 ? i : 0;
    if (i >= 5U)
        i = 4;
    k[4] = (int)&virtues_commander_laser_sets[i];
    k[1] = (float)(int)*(float *)&place->params[2]
           + (typ->break_sec + typ->break2_sec + typ->accel_sec + typ->break_swing_sec + typ->open_sec) * FPS + (float)k[2]
           + (float)(k[3] * (virtues_commander_laser_sets[i].n - 1))
           + (typ->laser_sec + typ->wait_sec + typ->close_sec + typ->break3_sec + typ->accel2_sec + typ->finish_sec) * FPS;
    nj_fill8(&desc, 0, sizeof desc);
    desc.name = virtues_commander_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.kind = k;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->hp;
    desc.same_lock = typ->same_lock;
    desc.flags = 16;
    desc.dc_flags = 0;
    desc.model_no = 201;
    desc.model_no2 = 204;
    desc.score = typ->score;
    desc.hp_reduce = typ->hp_reduce;
    desc.crash_r = typ->crash_r;
    desc.crash_damage = typ->crash_damage;
    desc.time = k[1];
    desc.size = 340;
    return enemy_task_task_create(obj, &desc);
}

/* 0x00b1ed08 */
void virtues_commander_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
