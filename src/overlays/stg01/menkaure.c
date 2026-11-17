/*
 * Menkaure, Area 1 enemy (two kinds): rolls along its placement vector, slower near the player,
 * turns to its E_Shot aim, charges an attack ray and fires, then leaves.
 */

#include "stg01.h"
#include "mathdef.h"
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
#include "player.h"
#include "pnb.h"
#include "ps_gs.h"
#include "rez_wire_compile.h"
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

/* 176 bytes; names in menkaure_vardefs */
typedef struct MenkaureParams {
    int hp;                         /* 0x08 */
    int same_lock;                  /* 0x0c */
    int score;                      /* 0x10 */
    float hp_reduce;                /* 0x14 */
    float crash_r;                  /* 0x18 */
    float crash_damage;             /* 0x1c */
    float backfire_scl;             /* 0x20 */
    float shot_motion_sec;          /* 0x24 */
    float shot_motion_sec_dif;      /* 0x28 */
    float break_sec;                /* 0x2c */
    float break_pow;                /* 0x30 */
    float break_backfire_pow;       /* 0x34 */
    float break_ang_pow;            /* 0x38 */
    float break_ang_pow2;           /* 0x3c */
    float shot_sec;                 /* 0x40 */
    float accel_sec;                /* 0x44 */
    float accel_pow;                /* 0x48 */
    float accel_spd_over_rate;      /* 0x4c */
    float accel_backfire_scl;       /* 0x50 */
    float accel_backfire_pow;       /* 0x54 */
    float accel_roll_spd_rate;      /* 0x58 */
    float accel_roll_pow;           /* 0x5c */
    float accel_ang_pow;            /* 0x60 */
    float accel_ang_pow2;           /* 0x64 */
    float accel_ang_pow3;           /* 0x68 */
    float accel_ang_pow4;           /* 0x6c */
    float accel_ang_over_rate;      /* 0x70 */
    float finish_sec;               /* 0x74 */
    float spd_near;                 /* 0x78 */
    float spd_far;                  /* 0x7c */
    float spd_rate;                 /* 0x80 */
    int wire;                       /* 0x84 */
    int wire_linear;                /* 0x88 */
    int wire_col;                   /* 0x8c */
    int wire_col2;                  /* 0x90 */
    float wire_near;                /* 0x94 */
    float wire_far;                 /* 0x98 */
    float wire_near_rate;           /* 0x9c */
    float wire_far_rate;            /* 0xa0 */
    int wire_beat;                  /* 0xa4 */
    float parts_roll_spd;           /* 0xa8 */
    float attack_ray_scl;           /* 0xac */
} MenkaureParams;

typedef struct MenkaureVars {
    char *name;
    char *label;                /* 0x04 */
    MenkaureParams p;            /* 0x08 */
} MenkaureVars;

/* 320 bytes */
typedef struct MenkaureWork {
    MenkaureVars *type;
    int state;                  /* 0x04 */
    int frames;                 /* 0x08: frames in the state */
    Vec3 hitv;                  /* 0x0c: the lock-on record's vector when hit */
    char pad_18[12];
    float size;                 /* 0x24 */
    int roll[3];                /* 0x28 */
    int ang[3];                 /* 0x34: its angles when created */
    float speed;                /* 0x40 */
    char pad_44[12];
    int phase;                  /* 0x50: of its shot (0: none, 1: charge, 2: charging, 3: shot) */
    int charge_start, charge_end;               /* 0x54: the times the charge starts and ends */
    Vec3 pos;                   /* 0x5c */
    LockObj lock;                  /* 0x68 */
    Vec3 tip;                   /* 0xa8 */
    int shot_set;               /* 0xb4: its 'E_Shot' set */
    int ang0[3];                /* 0xb8: its angles when its state last changed */
    int aim[3];                 /* 0xc4: the facing of a shot of its set (e_shot_initial_angles) */
    ModelPart body;                /* 0xd0 */
    float alpha;                /* 0x12c */
    int shot;                   /* 0x130 */
    Vec3 scale;                 /* 0x134 */
} MenkaureWork;

extern int menkaure_init_arg;
extern MenkaureVars menkaure_types[2];
extern VarDef menkaure_vardef;
extern VarDef menkaure_vardefs[42];
void menkaure_vars_register(void);
void menkaure_vars_unregister(void);
int menkaure_vars_release(void);
void menkaure_rotate_yxz(int *ang);
extern char menkaure_lock_name[];
extern char menkaure_name[];
extern ObjFuncs menkaure_funcs;
extern void (*menkaure_states[])();
extern void (*menkaure_exec_table[])(void *o, MenkaureWork *w);
extern void (*menkaure_disp_table[])(void *o, MenkaureWork *w);
void menkaure_init(void *act, MenkaureWork *w, int plp, int *rec);
void menkaure_release(void *act, MenkaureWork *w);
int menkaure_next_state(void *act, MenkaureWork *w, int n, int state);
float menkaure_speed_factor(void *act, MenkaureWork *w);
void menkaure_shot_charge(void *act, MenkaureWork *w);
void menkaure_state_move(void *act, MenkaureWork *w);
void menkaure_state_break(void *act, MenkaureWork *w);
void menkaure_state_shot(void *act, MenkaureWork *w);
void menkaure_state_accel(void *act, MenkaureWork *w);
void menkaure_state_finish(void *act, MenkaureWork *w);
void menkaure_state_cruise(void *act, MenkaureWork *w);
void menkaure_update(void *act, MenkaureWork *w);
void menkaure_exec_appear(void *act, MenkaureWork *w);
void menkaure_exec_normal(void *act, MenkaureWork *w);
void menkaure_exec_leave(void *act, MenkaureWork *w);
void menkaure_exec_destroyed(void *act, MenkaureWork *w);
void menkaure_pre_exec(void);
void menkaure_exec(void *act, MenkaureWork *w);
void menkaure_post_exec(void *act, MenkaureWork *w);
void menkaure_disp_body(void *act, MenkaureWork *w);
void menkaure_disp(void *act, MenkaureWork *w);
int menkaure_create(void *parent, Placement *pl);

/* 0x00affc08 */
void menkaure_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *vd;
    int *fld;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&menkaure_vardef);
    for (i = 0; i < 2; i++) {
        vd = menkaure_vardefs;
        fld = (int *)&menkaure_types[i].p;
        menkaure_vardef.name = menkaure_types[i].name;
        menkaure_vardef.fmt = menkaure_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&menkaure_vardef);
        for (j = 0; j < 42; j++) {
            vd->ptr = fld;
            fld++;
            vd++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)menkaure_vardefs);
    }
}

/* 0x00affd20 */
void menkaure_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00affd28 */
int menkaure_get_init_arg(void)
{
    return menkaure_init_arg;
}

/* 0x00affd38 */
int menkaure_vars_init(int *prm)
{
    menkaure_init_arg = *prm;
    menkaure_vars_register();
    return 0;
}

/* 0x00affd60 */
int menkaure_vars_release(void)
{
    menkaure_vars_unregister();
    return 0;
}

/* 0x00affd80 */
int menkaure_marker_draw(int obj, void *act)
{
    int *ent;

    ulGxSetTextureList(stage_textures);
    ent = pnb_get_entry(stage_models, 37);
    return obj_draw_at(obj, act, (void *)*ent);
}

/* 0x00affdd8 */
int menkaure_roll_node_cb(unsigned int *nd, int ni, MenkaureWork *w)
{
    if (*nd & 0x8000000)
        vu0_matrix_rotate_z(ulMatrixCur, w->roll[1] + w->roll[2]);
    return 1;
}

/* 0x00affe18 */
int menkaure_node_cb(unsigned int *nd, int ni, MenkaureWork *w)
{
    if (*nd & 0x4000000)
        ulMatrixGetTranslation((float *)ulMatrixCur, &w->tip.x);
    if (*nd & 0x1000000)
        ulMatrixGetTranslation((float *)ulMatrixCur, &w->pos.x);
    return 0;
}

/* 0x00affe88 */
void menkaure_pose_model(void *act, MenkaureWork *w)
{
    Vec3 cpos;
    int ang[4];
    void *gfx = enemy_task_gfx_ptr(act);

    enemy_task_get_pos(act, &cpos);
    enemy_task_get_rot(act, ang);
    dc_object_set_callback(gfx, menkaure_node_cb, w);
    do {
        ulMatrixPush();
    } while (0);
    matrix_unit_set(0);
    vu0_matrix_translate(ulMatrixCur, cpos.x, cpos.y, cpos.z);
    menkaure_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(gfx);
    ulMatrixPop();
    dc_object_set_callback(gfx, menkaure_roll_node_cb, w);
}

/* steer(cast) */
/* 0x00afff58 */
void menkaure_init(void *act, MenkaureWork *w, int plp, int *rec)
{
    Vec3 v;
    float sfrm;
    int ang[3];
    BackFireLink lnk;
    float sc[2][2];
    MenkaureVars *typ;
    float x, y, z;
    int n;

    typ = &menkaure_types[*rec];
    w->type = typ;
    lockon_obj_init(&w->lock, act, &w->pos, 0, menkaure_lock_name, enemy_task_get_status_slot(act), typ->p.hp,
                  typ->p.same_lock, 0);
    lockon_entry(&w->lock);
    w->lock.flags &= ~LOCK_LOCKABLE;
    enemy_task_clear_flags(act, 64);
    enemy_task_get_param_vec(act, &v);
    enemy_task_get_rot(act, ang);
    if (v3_is_zero(&v)) {
        x = 0.0f;
        y = 0.0f;
        z = -1.0f;
        ROT_X(ang[0], sc[0], y, z);
        ROT_Y(ang[1], sc[1], x, z);
        w->speed = 0.0f;
        v3_set(&v, x, y, z);
    } else {
        w->speed = vec3_length(&v);
        vec3_normalize(&v, &v);
        do {
            ang[1] = (int)RAD_TO_ANG(atan2f(-v.x, -v.z));
            ang[0] = (int)RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
        } while (0);
    }
    enemy_task_set_param_vec(act, &v);
    enemy_task_set_rot(act, ang);
    i3_copy(w->ang, ang);
    w->roll[0] = 0;
    w->roll[2] = 0;
    w->roll[1] = 0;
    if ((int)enemy_task_get_param(act, 1) == 0) {
        w->phase = 0;
        w->state = 5;
    } else {
        w->state = 0;
        w->phase = 1;
        n = enemy_task_get_param(act, 1) * 60.0f / 60.0f;
        sfrm = typ->p.shot_motion_sec * FPS;
        w->charge_end = n + (typ->p.break_sec + typ->p.shot_sec) * FPS;
        w->frames = 1;
        w->charge_start = w->charge_end - sfrm;
        {
            int es = ((float (*)(float))float_frac)(enemy_task_get_param(act, 1)) * 1000.0f + 0.5f;
            es = es < 0 ? 0 : es;
            w->shot_set = es > 7 ? 7 : es;
        }
    }
    w->shot = 0;
    enemy_task_set_appear(act, 2, 30);
    enemy_task_set_vanish(act, 2, 60);
    heap_bzero(&lnk, sizeof(lnk));
    lnk.gfx = enemy_task_gfx_ptr(act);
    lnk.pos = enemy_task_pos_ptr(act);
    lnk.ang = enemy_task_rot_ptr(act);
    lnk.obj_scale = enemy_task_scl_ptr(act);
    lnk.scale = &w->size;
    lnk.alpha = &w->alpha;
    lnk.flags = 0x2000000;
    lnk.use_pose = 1;
    en_backfire_create(act, &lnk, 0);
    w->size = typ->p.backfire_scl;
    dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(*(void **)act, 39)), *((struct DcClip **)pnb_get_entry(*(void **)act, 40)),
                  0x500);
    ((ModelPart *)enemy_task_gfx_ptr(act))->radius = w->body.radius = 24.0f;
    dc_object_set_wire(&w->body, model_pnb_get_wire(*(void **)act, 39));
    dc_object_set_callback(enemy_task_gfx_ptr(act), menkaure_roll_node_cb, w);
    dc_object_set_callback(&w->body, menkaure_roll_node_cb, w);
    enemy_task_set_appear(act, 2, 30);
    enemy_task_set_vanish(act, 2, 60);
    w->alpha = 0;
}

/* 0x00b00468 */
void menkaure_release(void *act, MenkaureWork *w)
{
    lockon_remove(&w->lock);
    dc_object_release(&w->body);
}

/* 0x00b00498 */
int menkaure_next_state(void *act, MenkaureWork *w, int n, int state)
{
    if (++w->frames > n) {
        w->state = state;
        w->frames = 1;
        enemy_task_get_rot(act, w->ang0);
        return 1;
    }
    return 0;
}

/* 0x00b004e0 */
float menkaure_speed_factor(void *act, MenkaureWork *w)
{
    Vec3 cpos;
    Vec3 plp;
    MenkaureVars *typ = w->type;
    float dis, fac;

    player_get_pos(&plp);
    enemy_task_get_pos(act, &cpos);
    dis = ulVec3Distance(&cpos, &plp);
    if (typ->p.spd_far < dis)
        fac = 1.0f;
    else if (dis < typ->p.spd_near)
        fac = typ->p.spd_rate;
    else
        fac = (dis - typ->p.spd_near) / (typ->p.spd_far - typ->p.spd_near) * (1.0f - typ->p.spd_rate) + typ->p.spd_rate;
    return fac;
}

/* 0x00b00598 */
void menkaure_shot_charge(void *act, MenkaureWork *w)
{
    AttackRayArg ray;
    ModelPart *gfx;
    float rate, frm;

    rate = (enemy_task_get_time(act) - w->charge_start) / (float)(w->charge_end - w->charge_start);
    gfx = enemy_task_gfx_ptr(act);
    frm = gfx->frames;
    if (rate < 1.0f)
        frm *= rate;
    if (w->phase == 0 || frm < 0.0f)
        return;
    gfx->frame = frm;
    if (frm < 0.0f)
        return;
    switch (w->phase) {
    case 1:
        ray.pos = &w->tip;
        ray.ang = enemy_task_rot_ptr(act);
        ray.scale = &w->scale;
        ray.dur = (w->charge_end - w->charge_start) * 39.0f / gfx->frames;
        attack_ray_create(act, &ray);
        w->phase = 2;
        break;
    case 2:
        if (frm < 39.0f)
            break;
        w->shot = 1;
        w->phase = 3;
        break;
    case 3:
        w->shot = 0;
        break;
    }
}

/* 0x00b00728 */
void menkaure_state_move(void *act, MenkaureWork *w)
{
    Vec3 cpos;
    Vec3 v;
    int n;

    n = enemy_task_get_param(act, 1) * 60.0f / 60.0f <= 1.0f ? 1
        : (int)(enemy_task_get_param(act, 1) * 60.0f / 60.0f);
    enemy_task_get_pos(act, &cpos);
    enemy_task_get_param_vec(act, &v);
    v3_scale(&v, w->speed * menkaure_speed_factor(act, w));
    v3_add(&cpos, &v);
    enemy_task_set_pos(act, &cpos);
    w->roll[0] += (int)DEG_TO_ANG(enemy_task_get_param(act, 2)) / FPS;
    menkaure_shot_charge(act, w);
    if (menkaure_next_state(act, w, n, 1))
        e_shot_initial_angles(w->aim, &cpos, 0, w->shot_set);
}

/* 0x00b008d0 */
void menkaure_state_break(void *act, MenkaureWork *w)
{
    int ang[4];
    Vec3 cpos;
    Vec3 dv;
    float sc[3][2];
    MenkaureVars *typ = w->type;
    float rate, rprev, spd, dis, x, y, z;
    int n;

    n = typ->p.break_sec * FPS;
    rate = w->frames / (float)n;
    rprev = (w->frames - 1) / (float)n;
    ulSinCos((int)(safe_powf(rate, typ->p.break_ang_pow) * ANG_HALF - ANG_QUARTER), &sc[0][0],
             &sc[0][1]);
    yuki_lerp3(ang, w->ang, w->aim, safe_powf((sc[0][0] + 1.0f) * 0.5f, typ->p.break_ang_pow2));
    enemy_task_set_rot(act, ang);
    rate = 1.0f - rate;
    w->roll[0] = (unsigned short)w->roll[0] * rate;
    enemy_task_get_pos(act, &cpos);
    x = 0.0f;
    y = 0.0f;
    z = -1.0f;
    ROT_X(ang[0], sc[1], y, z);
    ROT_Y(ang[1], sc[2], x, z);
    v3_set(&dv, x, y, z);
    spd = w->speed * FPS * menkaure_speed_factor(act, w);
    spd = -spd;
    dis = spd / (typ->p.break_pow + 1.0f) * typ->p.break_sec * (safe_powf(rate, typ->p.break_pow + 1.0f) - 1.0f);
    dis -= spd / (typ->p.break_pow + 1.0f) * typ->p.break_sec * (safe_powf(1.0f - rprev, typ->p.break_pow + 1.0f) - 1.0f);
    v3_scale(&dv, dis);
    v3_add(&cpos, &dv);
    enemy_task_set_pos(act, &cpos);
    w->size = safe_powf(rate, typ->p.break_backfire_pow) * typ->p.backfire_scl;
    menkaure_shot_charge(act, w);
    menkaure_next_state(act, w, n, 2);
}

/* 0x00b00be0 */
void menkaure_state_shot(void *act, MenkaureWork *w)
{
    int n = w->type->p.shot_sec * FPS;

    menkaure_shot_charge(act, w);
    if (menkaure_next_state(act, w, n, 3)) {
        ModelPart *gfx = enemy_task_gfx_ptr(act);

        gfx->frame = gfx->frames;
    }
}

/* 0x00b00c60 */
void menkaure_state_accel(void *act, MenkaureWork *w)
{
    int ang[4];
    Vec3 cpos;
    Vec3 dv;
    float sc[3][2];
    MenkaureVars *typ = w->type;
    float rate, rprev, spd, dis, acrv, rspd, x, y, z;
    int n;

    n = typ->p.accel_sec * FPS;
    rate = w->frames / (float)n;
    rprev = (w->frames - 1) / (float)n;
    ulSinCos((int)(safe_powf(rate, typ->p.accel_ang_pow) * ANG_HALF - ANG_QUARTER), &sc[0][0],
             &sc[0][1]);
    acrv = safe_powf((sc[0][0] + 1.0f) * 0.5f, typ->p.accel_ang_pow2);
    ulSinCos((int)(safe_powf(rate, typ->p.accel_ang_pow3) * ANG_TURN - ANG_QUARTER), &sc[0][0],
             &sc[0][1]);
    acrv += safe_powf((sc[0][0] + 1.0f) * 0.5f, typ->p.accel_ang_pow4) * typ->p.accel_ang_over_rate;
    enemy_task_get_rot(act, ang);
    yuki_lerp3(ang, w->ang0, w->ang, acrv);
    enemy_task_set_rot(act, ang);
    rspd = enemy_task_get_param(act, 2);
    w->roll[0] += (int)DEG_TO_ANG(rspd) * (typ->p.accel_roll_spd_rate + 1.0f) / FPS
                  * safe_powf(rate, typ->p.accel_roll_pow);
    enemy_task_get_pos(act, &cpos);
    x = 0.0f;
    y = 0.0f;
    z = -1.0f;
    ROT_X(ang[0], sc[1], y, z);
    ROT_Y(ang[1], sc[2], x, z);
    v3_set(&dv, x, y, z);
    spd = w->speed * FPS * menkaure_speed_factor(act, w);
    dis = spd / (typ->p.accel_pow + 1.0f) * typ->p.accel_sec * (typ->p.accel_spd_over_rate + 1.0f)
        * safe_powf(rate, typ->p.accel_pow + 1.0f);
    dis -= spd / (typ->p.accel_pow + 1.0f) * typ->p.accel_sec * (typ->p.accel_spd_over_rate + 1.0f)
         * safe_powf(rprev, typ->p.accel_pow + 1.0f);
    v3_scale(&dv, dis);
    v3_add(&cpos, &dv);
    enemy_task_set_pos(act, &cpos);
    w->size = typ->p.accel_backfire_scl * safe_powf(rate, typ->p.accel_backfire_pow);
    menkaure_next_state(act, w, n, 4);
}

/* 0x00b01030 */
void menkaure_state_finish(void *act, MenkaureWork *w)
{
    Vec3 cpos;
    Vec3 v;
    MenkaureVars *typ = w->type;
    float rate, rprev, sspd, espd, dis;
    int n;

    n = typ->p.finish_sec * FPS;
    rate = w->frames / (float)n;
    rprev = (w->frames - 1) / (float)n;
    sspd = w->speed * FPS * menkaure_speed_factor(act, w);
    espd = sspd * (typ->p.accel_spd_over_rate + 1.0f);
    dis = ((sspd - espd) * rate * 0.5f + espd) * (w->frames / FPS)
        - ((sspd - espd) * rprev * 0.5f + espd) * ((w->frames - 1) / FPS);
    enemy_task_get_pos(act, &cpos);
    enemy_task_get_param_vec(act, &v);
    v3_scale(&v, dis);
    v3_add(&cpos, &v);
    enemy_task_set_pos(act, &cpos);
    w->roll[0] += (int)DEG_TO_ANG(enemy_task_get_param(act, 2)) * (typ->p.accel_roll_spd_rate * (1.0f - rate) + 1.0f)
                  / FPS;
    w->size = typ->p.backfire_scl + (typ->p.accel_backfire_scl - typ->p.backfire_scl) * (1.0f - rate);
    menkaure_next_state(act, w, n, 5);
}

/* 0x00b01268 */
void menkaure_state_cruise(void *act, MenkaureWork *w)
{
    Vec3 cpos;
    Vec3 v;

    enemy_task_get_pos(act, &cpos);
    enemy_task_get_param_vec(act, &v);
    v3_scale(&v, w->speed * menkaure_speed_factor(act, w));
    v3_add(&cpos, &v);
    enemy_task_set_pos(act, &cpos);
    w->roll[0] += (int)DEG_TO_ANG(enemy_task_get_param(act, 2)) / FPS;
}

/* 0x00b01368 */
void menkaure_update(void *act, MenkaureWork *w)
{
    int ang[4];
    Vec3 dv;
    float sc[2][2];
    float x, y, z;

    if (menkaure_states[w->state])
        menkaure_states[w->state]();  /* the handler receives act and w as passed in here */
    menkaure_pose_model(act, w);
    if (w->shot) {
        enemy_task_get_rot(act, ang);
        x = 0.0f;
        y = 0.0f;
        z = -1.0f;
        ROT_X(ang[0], sc[0], y, z);
        ROT_Y(ang[1], sc[1], x, z);
        v3_set(&dv, x, y, z);
        e_shot_fire(act, &w->tip, &dv, w->shot_set);
        w->shot = 0;
    }
}

/* 0x00b01480 */
void menkaure_exec_appear(void *act, MenkaureWork *w)
{
    enemy_task_appear_scale(act, w);
    if (w->alpha > 0.0f)
        w->lock.flags |= LOCK_LOCKABLE;
    switch (enemy_task_get_phase(act)) {
    case 0:
        w->lock.flags &= ~LOCK_LOCKABLE;
        break;
    case 2:
        w->lock.flags |= LOCK_LOCKABLE;
        break;
    default:
        menkaure_update(act, w);
    }
}

/* 0x00b01520 */
void menkaure_exec_normal(void *act, MenkaureWork *w)
{
    float t;

    if (enemy_task_get_phase(act) != 1)
        return;
    menkaure_update(act, w);
    t = enemy_task_get_time(act);
    if (fabsf(enemy_task_get_param(act, 0)) * 60.0f / 60.0f < t)
        enemy_task_set_state(act, 2);
}

/* 0x00b015c0 */
void menkaure_exec_leave(void *act, MenkaureWork *w)
{
    do { enemy_task_vanish_scale(act, w); } while (0);
    switch (enemy_task_get_phase(act)) {
    case 0:
        w->lock.flags &= ~LOCK_LOCKABLE;
        break;
    case 2:
        break;
    default:
        menkaure_update(act, w);
    }
}

/* 0x00b01638 */
void menkaure_exec_destroyed(void *act, MenkaureWork *w)
{
    SpawnDesc dsc;

    if (enemy_task_get_phase(act) == 0) {
        nj_fill8(&dsc, 0, sizeof dsc);
        dsc.type = 41;
        dsc.models = stage_models;
        dsc.lock = &w->lock;
        dsc.kind = 0;
        enemy_task_get_pos(act, &dsc.pos);
        enemy_task_get_rot(act, dsc.ang);
        enemy_task_get_scl(act, &dsc.scale);
        enemy_task_get_param_vec(act, &dsc.v);
        enemy_shatter_create(act, &dsc, 0, 0);
        FINISH_IF(act);
    }
}

/* 0x00b016f8 */
void menkaure_pre_exec(void)
{
}

/* 0x00b01700 */
void menkaure_exec(void *act, MenkaureWork *w)
{
    unsigned int i = enemy_task_get_pstate(act);

    if (menkaure_exec_table[i])
        menkaure_exec_table[i](act, w);
}

/* steer(cast) */
/* 0x00b01758 */
void menkaure_post_exec(void *act, MenkaureWork *w)
{
    MenkaureVars *typ = w->type;
    LockObj *lk = &w->lock;
    Vec3 cpos;
    float alp;

    enemy_task_get_scl(act, &w->scale);
    v3_scale(&w->scale, typ->p.attack_ray_scl);
    switch (((unsigned int)enemy_task_get_pstate(act))) {
    case 1:
        alp = enemy_task_get_state_time(act) / 30.0f;
        w->alpha = alp;
        alp = MIN(alp >= 0.0f ? alp : 0.0f, 1.0f);
        w->alpha = alp;
        break;
    case 0:
        w->alpha = 1.0f;
        break;
    case 2:
        alp = 1.0f - enemy_task_get_state_time(act) / FPS;
        w->alpha = alp;
        alp = MIN(alp >= 0.0f ? alp : 0.0f, 1.0f);
        w->alpha = alp;
        break;
    default:
        w->alpha = 0.0f;
        break;
    }
    if (!enemy_status_get_ticks())
        menkaure_pose_model(act, w);
    if (lk->flags & LOCK_HIT) {
        v3_copy(&w->hitv, &lk->imp.v);
        if (lk->life <= 0 && ((unsigned int)enemy_task_get_pstate(act)) < 2) {
            int mult = lockon_hits_max_shot_cnt(lk);

            enemy_task_get_pos(act, &cpos);
            enemy_status_score_popup(enemy_task_get_status_slot(act), &cpos, typ->p.score, mult);
            enemy_task_set_state(act, 3);
        }
    }
}

/* steer(cast) */
/* 0x00b01980 */
void menkaure_disp_body(void *act, MenkaureWork *w)
{
    Vec3 cpos;
    int ang[4];
    float rgba[4];
    ModelPart *gfx;
    MenkaureVars *typ;
    int bl;

    gfx = enemy_task_gfx_ptr(act);
    enemy_task_get_pos(act, &cpos);
    enemy_task_get_rot(act, ang);
    if (view_sphere_visible(&cpos, gfx->radius)) {
        bl = 0;
        typ = w->type;
        if (w->alpha < 1.0f) {
            bl = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
            heap_bzero(rgba, 16);
            draw_push_strip_flags();
            draw_push_override_color();
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 0x800);
            rgba[0] = w->alpha - 1.0f;
            draw_set_override_color(rgba);
            psGsSetAlphaTest(0, 1, 128, 1);
        }
        ulGxSetTextureList(((void **)act)[1]);
        do {
            ulMatrixPush();
        } while (0);
        vu0_matrix_translate(ulMatrixCur, cpos.x, cpos.y, cpos.z);
        menkaure_rotate_yxz(ang);
        vu0_matrix_rotate_z(ulMatrixCur, w->roll[0]);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        dc_object_draw(gfx);
        if (typ->p.wire) {
            draw_set_color(yuki_beat_color(typ->p.wire_col, typ->p.wire_col2, typ->p.wire_beat,
                                          typ->p.wire_linear, w->alpha, typ->p.wire_near,
                                          typ->p.wire_far, typ->p.wire_near_rate,
                                          typ->p.wire_far_rate));
            draw_set_blend(4, 5);
            model_object_draw(&w->body, gfx->frame);
        }
        ulMatrixPop();
        if (w->alpha < 1.0f) {
            psGsSetAlphaTest(1, 5, 128, 1);
            draw_pop_strip_flags();
            draw_pop_override_color();
            draw_set_override_flags(bl);
        }
    }
}

/* 0x00b01b78 */
void menkaure_disp(void *act, MenkaureWork *w)
{
    unsigned int i = enemy_task_get_pstate(act);

    if (menkaure_disp_table[i])
        menkaure_disp_table[i](act, w);
}

/* steer(cast) */
/* 0x00b01bd0 */
int menkaure_create(void *parent, Placement *pl)
{
    ObjDesc dsc;
    ObjFuncs fns = menkaure_funcs;
    int knd;
    MenkaureVars *typ;
    int n;

    n = ((float (*)(float))float_frac)(pl->param0) * 1000.0f + 0.5f;
    n = n < 0 ? 0 : n;
    knd = n != 0;
    typ = &menkaure_types[knd];
    nj_fill8(&dsc, 0, sizeof dsc);
    dsc.kind = &knd;
    dsc.name = menkaure_name;
    dsc.funcs = &fns;
    dsc.arg = pl;
    dsc.models = stage_models;
    dsc.tex = stage_textures;
    dsc.hp = typ->p.hp;
    dsc.same_lock = typ->p.same_lock;
    dsc.flags = 18;
    dsc.dc_flags = 0;
    dsc.model_no = 37;
    dsc.model_no2 = 38;
    dsc.score = typ->p.score;
    dsc.hp_reduce = typ->p.hp_reduce;
    dsc.crash_r = typ->p.crash_r;
    dsc.crash_damage = typ->p.crash_damage;
    dsc.time = fabsf(pl->param0) * 60.0f / 60.0f;
    dsc.size = 320;
    return enemy_task_task_create(parent, &dsc);
}

/* 0x00b01d68 */
void menkaure_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
