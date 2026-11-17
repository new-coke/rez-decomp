/*
 * Enkidu, Area 3 enemy: circles a point that travels with the player; brakes to face the player,
 * attacks (ray and shot if enabled), then speeds on around the circle.
 */

#include "stg03.h"
#include "mathdef.h"
#include "anim.h"
#include "color.h"
#include "debug.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_draw.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "e_shot.h"
#include "../common/attack_ray.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"

/* Named by the 'enkidu' VarDefs. */
typedef struct EnkiduParams {
    int hp;
    int same_lock;              /* 0x04 */
    int score;                  /* 0x08 */
    float hp_reduce;            /* 0x0c */
    float crash_r;              /* 0x10 */
    float crash_damage;         /* 0x14 */
    int wire;                   /* 0x18 */
    int wire_linear;            /* 0x1c */
    int wire_col;               /* 0x20 */
    int wire_col2;              /* 0x24 */
    float wire_near;            /* 0x28 */
    float wire_far;             /* 0x2c */
    float wire_near_rate;       /* 0x30 */
    float wire_far_rate;        /* 0x34 */
    int wire_beat;              /* 0x38 */
    float bullet_spd;           /* 0x3c */
    float start_pow;            /* 0x40 */
    float keep_sec;             /* 0x44 */
    float break_sec;            /* 0x48 */
    float break_pow;            /* 0x4c */
    float break_r_swing_pow;    /* 0x50 */
    float break_r_rate;         /* 0x54 */
    float break_ang_pow;        /* 0x58 */
    float break_ang_rate;       /* 0x5c */
    float break_ang_rate_end;   /* 0x60 */
    int break_ang_add;          /* 0x64 */
    int break_ang_spd;          /* 0x68 */
    float break_ang_pow2;       /* 0x6c */
    float motion_start_rate;    /* 0x70 */
    float motion_end_rate;      /* 0x74 */
    float stop_swing_pow;       /* 0x78 */
    float stop_swing_r;         /* 0x7c */
    float shoot_frame_ray;      /* 0x80 */
    float shoot_frame_ray_wait; /* 0x84 */
    float accel_sec;            /* 0x88 */
    float accel_pow;            /* 0x8c */
    float accel_r_rate;         /* 0x90 */
    float accel_ang_spd;        /* 0x94 */
    float accel_ang_pow;        /* 0x98 */
    float accel_ang_rate;       /* 0x9c */
    float away_sec;             /* 0xa0 */
    float away_spd;             /* 0xa4 */
    float motion_spd_rate;      /* 0xa8 */
    float ray_sec;              /* 0xac */
} EnkiduParams;

typedef struct EnkiduVars {
    char *name;
    char *label;                /* 0x04 */
    EnkiduParams p;             /* 0x08 */
} EnkiduVars;

typedef struct EnkiduWork {
    EnkiduVars *type;
    int frames;                 /* 0x04: frames in the state */
    Vec3 hitv;                  /* 0x08: the lock-on record's vector when hit */
    int state;                  /* 0x14 */
    int life;                   /* 0x18: frames left; 0 = no limit */
    Vec3 core;                  /* 0x1c: lock-on point */
    Vec3 muzzle;                /* 0x28 */
    LockObj lock;               /* 0x34 */
    ModelPart cover;            /* 0x74: the wire cover */
    Vec3 base;                  /* 0xd0: player relative */
    int base_ang[3];            /* 0xdc */
    Vec3 orbit;                 /* 0xe8: offset on the orbit circle */
    int tilt[3];                /* 0xf4 */
    int tilt_from[3];           /* 0x100 */
    int orbit_ang;              /* 0x10c */
    int orbit_ang_from;         /* 0x110 */
    int spin_from;              /* 0x114 */
    float radius;               /* 0x118: of the orbit */
    float radius_from;          /* 0x11c */
    int ray_started;            /* 0x120 */
    int shoot_ready;            /* 0x124 */
    int shoots;                 /* 0x128 */
    float fade;                 /* 0x12c */
    int yaw_ofs;                /* 0x130 */
    int orbit_spd;              /* 0x134 */
    int ray_frame;              /* 0x138: frame the attack ray starts */
} EnkiduWork;

extern int enkidu_init_arg;
extern void (*enkidu_exec_table[])(void *o, EnkiduWork *w);
extern void (*enkidu_disp_table[])(void *o, EnkiduWork *w);
void enkidu_vars_register(void);
void enkidu_vars_unregister(void);
int enkidu_vars_init(int *fx_id);
int enkidu_vars_release(void);
void enkidu_exec(void *obj, EnkiduWork *w);
void enkidu_disp(void *obj, EnkiduWork *w);
void enkidu_rotate_yxz(int *ang);
extern EnkiduVars enkidu_types[1];
extern VarDef enkidu_vardef;
extern VarDef enkidu_vardefs[44];
extern int (*enkidu_states[])(void *o, EnkiduWork *w);
void enkidu_attack_ray_start(void *obj, EnkiduWork *w);
void enkidu_pose_model(void *obj, EnkiduWork *w);
void enkidu_fire_shot(void *obj, EnkiduWork *w);
void enkidu_update(void *obj, EnkiduWork *w);
int enkidu_collect_mounts(int *pos_a, int *pos_b, unsigned int *node, int cnt);
extern ObjFuncs enkidu_funcs;
extern char enkidu_name[];
extern char enkidu_lock_name[];
extern Vec3 enkidu_start_scl;

/* 0x00afe3c8 */
void enkidu_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *prm;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&enkidu_vardef);
    for (i = 0; i < 1; i++) {
        def = enkidu_vardefs;
        prm = (int *)&enkidu_types[i].p;
        enkidu_vardef.name = enkidu_types[i].name;
        enkidu_vardef.fmt = enkidu_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&enkidu_vardef);
        for (j = 0; j < 44; j++) {
            def->ptr = prm;
            prm++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)enkidu_vardefs);
    }
}

/* 0x00afe4e0 */
void enkidu_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00afe4e8 */
int enkidu_vars_init(int *fx_id)
{
    enkidu_init_arg = *fx_id;
    enkidu_vars_register();
    return 0;
}

/* 0x00afe510 */
int enkidu_vars_release(void)
{
    enkidu_vars_unregister();
    return 0;
}

/* steer(cast) */
/* 0x00afe530 */
int enkidu_marker_draw(int obj, Placement *place)
{
    Vec3 *pos = &place->pos;
    float sc[2];
    float org;

    if (view_sphere_visible(pos, place->param3)) {
        ulSinCos(DEG_TO_ANG(place->param2), &sc[0], &sc[1]);
        ulGxSetTextureList(stage_textures);
        do {
            ulMatrixPush();
        } while (0);
        vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
        enkidu_rotate_yxz(place->ang);
        do {
            org = 0.0f;
        } while (0);
        ulMatrixPush();
        vu0_matrix_translate(ulMatrixCur, org, place->param3 * sc[0], org);
        vu0_matrix_translate(ulMatrixCur, place->param3 * sc[1], org, org);
        nj_object_draw_shapes((void *)*((int *)pnb_get_entry(stage_models, 10)));
        ulMatrixPop();
        vu0_matrix_rotate_x(ulMatrixCur, ANG_90);
        ((int (*)(float, unsigned int, int))draw_ring_xz)(place->param3, RGBA(0xff, 0xff, 0xff, 0xff), 8);
        ulMatrixPop();
    }
    return 0;
}

/* 0x00afe668 */
int enkidu_get_init_arg(void)
{
    return enkidu_init_arg;
}

/* 0x00afe678 */
int enkidu_collect_mounts(int *pos_a, int *pos_b, unsigned int *node, int cnt)
{
    do {
        ulMatrixPush();
        anim_apply_node(node);
        if (node[0] & 0x2000000) {
            ulMatrixGetTranslation((float *)ulMatrixCur, (float *)pos_a);
            if (++cnt >= 2) {
                ulMatrixPop();
                return cnt;
            }
        }
        if (node[0] & 0x1000000) {
            ulMatrixGetTranslation((float *)ulMatrixCur, (float *)pos_b);
            if (++cnt >= 2) {
                ulMatrixPop();
                return cnt;
            }
        }
        if (!(node[0] & 0x10)) {
            cnt = enkidu_collect_mounts(pos_a, pos_b, (unsigned int *)node[11], cnt);
            if (cnt >= 2) {
                ulMatrixPop();
                return cnt;
            }
        }
        ulMatrixPop();
        node = (unsigned int *)node[12];
    } while (node);
    return cnt;
}

/* 0x00afe7b0 */
void enkidu_pose_model(void *obj, EnkiduWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 pos;
    Vec3 org;
    int ang[3];

    ulMatrixPush();
    matrix_unit_set(0);
    player_get_frame_pos(&org);
    do {
        Vec3 *p_ = &org;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    do {
        Vec3 *p_ = &w->base;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    enkidu_rotate_yxz(w->base_ang);
    do {
        Vec3 *p_ = &w->orbit;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    enkidu_rotate_yxz(w->tilt);
    ulMatrixGetTranslation((float *)ulMatrixCur, (float *)&pos);
    model_camera_angles(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    anim_start((void *)gfx->clip, gfx->frame);
    enkidu_collect_mounts((int *)&w->core, (int *)&w->muzzle, (unsigned int *)gfx->root, 0);
    ulMatrixPop();
    enemy_task_set_pos(obj, &pos);
    enemy_task_set_rot(obj, ang);
}

/* 0x00afe8d0 */
void enkidu_ray_timing(void *obj, EnkiduWork *w)
{
    EnkiduVars *typ;
    ModelPart *gfx;

    w->ray_frame = 0;
    if (w->shoots == 0) {
        w->ray_started = 1;
    } else {
        typ = w->type;
        gfx = enemy_task_gfx_ptr(obj);
        w->ray_started = 0;
        w->ray_frame -= typ->p.ray_sec * FPS;
        w->ray_frame += enemy_task_get_param(obj, 1) * 60.0f / 60.0f + 1.0f;
        w->ray_frame += typ->p.keep_sec * FPS + 1.0f;
        w->ray_frame += typ->p.break_sec * FPS + 1.0f;
        w->ray_frame += (32.0f - typ->p.shoot_frame_ray) / gfx->speed;
    }
}

/* 0x00afea18 */
void enkidu_attack_ray_start(void *obj, EnkiduWork *w)
{
    EnkiduVars *typ;
    int n;
    AttackRayArg ray;

    n = enemy_task_get_time(obj);
    typ = w->type;
    if (n >= w->ray_frame && w->ray_started == 0) {
        w->ray_started = 1;
        heap_bzero((int *)&ray, 16);
        ray.pos = &w->muzzle;
        ray.ang = enemy_task_rot_ptr(obj);
        ray.scale = enemy_task_scl_ptr(obj);
        ray.dur = typ->p.ray_sec * FPS;
        if (w->ray_frame <= 0)
            ray.dur += w->ray_frame;
        if (ray.dur > 0.0f)
            attack_ray_create(obj, &ray);
    }
}

/* steer(cast) */
/* 0x00afeaf8 */
void enkidu_init(void *obj, EnkiduWork *w, int place, int *kind)
{
    EnkiduVars *typ = &enkidu_types[kind[0]];
    ModelPart *gfx;
    Vec3 org;
    Vec3 pl_vel;
    float yaw;

    w->type = typ;
    lockon_obj_init(&w->lock, obj, &w->core, 0, enkidu_lock_name, enemy_task_get_status_slot(obj), typ->p.hp, typ->p.same_lock, 0);
    lockon_entry(&w->lock);
    enemy_task_clear_flags(obj, 64);
    w->life = enemy_task_get_param(obj, 4);
    w->state = 0;
    w->frames = 0;
    enemy_task_set_scl(obj, &enkidu_start_scl);
    player_get_frame_pos(&org);
    player_get_frame_vel(&pl_vel);
    enemy_task_get_pos(obj, &w->base);
    enemy_task_get_rot(obj, w->base_ang);
    v3_add(&org, &pl_vel);
    v3_sub(&w->base, &w->base, &org);
    yaw = DEG_TO_ANG(enemy_task_get_param(obj, 2));
    i3_set((int *)&w->orbit, 0, 0, 0);
    w->orbit_ang = yaw;
    i3_set(w->tilt, 0, 0, 0);
    w->radius = enemy_task_get_param(obj, 3);
    enkidu_pose_model(obj, w);
    dc_object_init(&w->cover, *((struct DcNode **)pnb_get_entry(*(void **)obj, 11)), *((struct DcClip **)pnb_get_entry(*(void **)obj, 13)), 0x500);
    dc_object_set_wire(&w->cover, model_pnb_get_wire(*(void **)obj, 11));
    gfx = enemy_task_gfx_ptr(obj);
    w->ray_started = 0;
    gfx->radius = w->cover.radius = 135.0f;
    w->shoots = 0.5f < enemy_task_get_param(obj, 5);
    w->shoot_ready = 1;
    enkidu_ray_timing(obj, w);
    w->fade = 0;
    w->yaw_ofs = 0.0f < enemy_task_get_param(obj, 0) ? 0 : ANG_180;
    w->orbit_spd = typ->p.away_spd * ANG_TURN * 0.5f / PI / enemy_task_get_param(obj, 3);
    gfx = enemy_task_gfx_ptr(obj);
    gfx->speed *= typ->p.motion_spd_rate;
}

/* 0x00afedf8 */
void enkidu_release(void *obj, EnkiduWork *w)
{
    dc_object_release(&w->cover);
    lockon_remove(&w->lock);
}

/* 0x00afee28 */
int enkidu_next_state(void *obj, EnkiduWork *w, int n, int state)
{
    if (++w->frames > n) {
        w->state = state;
        w->frames = 1;
        w->orbit_ang_from = w->orbit_ang;
        i3_copy(w->tilt_from, w->tilt);
        return 1;
    }
    return 0;
}

/* 0x00afee88 */
void enkidu_fire_shot(void *obj, EnkiduWork *w)
{
    Vec3 dir;
    int ang[3];
    float sc[2][2];
    float x, y, z;

    x = 0.0f;
    y = 0.0f;
    z = -(w->type->p.bullet_spd * (1.0f / FPS));
    enemy_task_get_rot(obj, ang);
    ROT_X(ang[0], sc[0], y, z);
    ROT_Y(ang[1], sc[1], x, z);
    v3_set(&dir, x, y, z);
    e_shot_fire(obj, &w->muzzle, &dir, 8);
}

/* 0x00afef80 */
int enkidu_state_start(void *obj, EnkiduWork *w)
{
    EnkiduVars *typ;
    float unit, rate, sec, aspd;
    float sc[2];
    int n;

    unit = 1.0f;
    n = enemy_task_get_param(obj, 1) * 60.0f / 60.0f + unit;
    typ = w->type;
    sec = n;
    rate = w->frames / sec;
    sec = sec / FPS;
    aspd = enemy_task_get_param(obj, 0);
    {
        float ease = safe_powf(sec * rate, typ->p.start_pow + unit);

        aspd = aspd / ((typ->p.start_pow + unit) * sec);
        aspd *= ease;
    }
    w->orbit_ang = DEG_TO_ANG(enemy_task_get_param(obj, 2) + aspd);
    ulSinCos(w->orbit_ang, &sc[0], &sc[1]);
    w->orbit.y = sc[0] * w->radius;
    w->orbit.x = sc[1] * w->radius;
    w->tilt[2] = w->orbit_ang + w->yaw_ofs;
    enkidu_next_state(obj, w, n, 1);
    return 0;
}

/* 0x00aff0f8 */
int enkidu_state_keep(void *obj, EnkiduWork *w)
{
    EnkiduVars *typ = w->type;
    float sc[2];
    int n;

    n = typ->p.keep_sec * FPS + 1.0f;
    w->orbit_ang += (int)DEG_TO_ANG(enemy_task_get_param(obj, 0) / FPS);
    ulSinCos(w->orbit_ang, &sc[0], &sc[1]);
    w->orbit.y = sc[0] * w->radius;
    w->orbit.x = sc[1] * w->radius;
    w->tilt[2] = w->orbit_ang + w->yaw_ofs;
    enkidu_next_state(obj, w, n, 2);
    return 0;
}

/* 0x00aff1e8 */
int enkidu_state_break(void *obj, EnkiduWork *w)
{
    EnkiduVars *typ = w->type;
    float unit = 1.0f;
    int n = typ->p.break_sec * FPS + unit;
    float rate = (float)w->frames / n;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 prev;
    Vec3 dv;
    Vec3 pl;
    Vec3 pos;
    Vec3 dpl;
    int ang[3];
    float sc[3][2];
    float aspd, norm, crv;

    v3_copy(&prev, &w->orbit);
    aspd = enemy_task_get_param(obj, 0);
    norm = safe_powf(typ->p.break_sec, typ->p.break_pow);
    crv = safe_powf(typ->p.break_sec * (unit - rate), typ->p.break_pow + unit) - safe_powf(typ->p.break_sec, typ->p.break_pow + unit);
    w->orbit_ang = w->orbit_ang_from + (int)DEG_TO_ANG(-aspd / ((typ->p.break_pow + unit) * norm) * crv);
    ulSinCos(w->orbit_ang, &sc[0][0], &sc[0][1]);
    w->orbit.y = sc[0][0] * w->radius;
    w->orbit.x = sc[0][1] * w->radius;
    ulSinCos(rate * ANG_TURN, &sc[0][0], &sc[0][1]);
    w->radius = enemy_task_get_param(obj, 3) * (unit - typ->p.break_r_rate + typ->p.break_r_rate * sc[0][1] * (unit - safe_powf(rate, typ->p.break_r_swing_pow)));
    v3_sub(&dv, &w->orbit, &prev);
    if (typ->p.motion_start_rate < rate) {
        float frac = (rate - typ->p.motion_start_rate) / (typ->p.motion_end_rate - typ->p.motion_start_rate) * typ->p.shoot_frame_ray;
        gfx->frame = MIN(MAX_GE(frac, 0.0f), typ->p.shoot_frame_ray);
    }
    player_get_pos(&pl);
    enemy_task_get_pos(obj, &pos);
    v3_sub(&dpl, &pl, &pos);
    do {
        ang[1] = RAD_TO_ANG(atan2f(-dpl.x, -dpl.z));
        ang[0] = RAD_TO_ANG(atan2f(dpl.y, sqrtf(dpl.x * dpl.x + dpl.z * dpl.z)));
    } while (0);
    i3_sub(ang, ang, w->base_ang);
    yuki_lerp3(w->tilt, w->tilt_from, ang, rate);
    if (rate < typ->p.break_ang_rate) {
        w->spin_from = w->tilt[2] = (w->orbit_ang + w->yaw_ofs) | (int)0xffff0000;
    } else if (rate < typ->p.break_ang_rate_end) {
        float frac = (rate - typ->p.break_ang_rate) / (typ->p.break_ang_rate_end - typ->p.break_ang_rate);
        frac = MIN(MAX_GE(frac, 0.0f), 1.0f);
        ulSinCos(frac * ANG_HALF - ANG_QUARTER, &sc[1][0], &sc[1][1]);
        w->tilt[2] = w->spin_from * (1.0f - (sc[1][0] + 1.0f) * 0.5f);
    } else {
        float frac = (rate - typ->p.break_ang_rate_end) / (1.0f - typ->p.break_ang_rate_end);
        ulSinCos(typ->p.break_ang_spd * frac, &sc[2][0], &sc[2][1]);
        w->tilt[2] = typ->p.break_ang_add * sc[2][0] * safe_powf(1.0f - frac, typ->p.break_ang_pow2);
    }
    if (enkidu_next_state(obj, w, n, 3))
        w->radius_from = w->radius;
    return 0;
}

/* 0x00aff748 */
int enkidu_state_ray(void *obj, EnkiduWork *w)
{
    EnkiduVars *typ = w->type;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    int n = gfx->frames / gfx->speed;
    int fire = 0;
    Vec3 pl;
    Vec3 p;
    Vec3 dpl;
    int ang[3];
    float sc[2];
    float frm;

    ulSinCos((float)w->frames / n * ANG_TURN, &sc[0], &sc[1]);
    w->radius = w->radius_from * (sc[0] * typ->p.stop_swing_r + 1.0f);
    ulSinCos(w->orbit_ang, &sc[0], &sc[1]);
    w->orbit.y = sc[0] * w->radius;
    w->orbit.x = sc[1] * w->radius;
    dc_object_advance_time(gfx);
    frm = gfx->frame;
    if (w->shoot_ready && frm > 32.0f) {
        w->shoot_ready = 0;
        fire = 1;
    }
    player_get_pos(&pl);
    enemy_task_get_pos(obj, &p);
    v3_sub(&dpl, &pl, &p);
    do {
        ang[1] = (int)RAD_TO_ANG(atan2f(-dpl.x, -dpl.z));
        ang[0] = (int)RAD_TO_ANG(atan2f(dpl.y, sqrtf(dpl.x * dpl.x + dpl.z * dpl.z)));
    } while (0);
    {
        int *base = w->base_ang;
        i3_sub(w->tilt, ang, base);
    }
    w->tilt[2] = 0;
    enkidu_next_state(obj, w, n, 4);
    return fire && w->shoots;
}

/* 0x00aff9e0 */
int enkidu_state_turn(void *obj, EnkiduWork *w)
{
    EnkiduVars *typ = w->type;
    float unit = 1.0f;
    int n = typ->p.accel_sec * FPS + unit;
    float rate = (float)w->frames / n;
    float aspd;
    float crv;
    float rang;
    float sc[2];

    aspd = enemy_task_get_param(obj, 0) * typ->p.accel_ang_spd;
    crv = safe_powf(rate, typ->p.accel_pow + unit);
    w->orbit_ang = w->orbit_ang_from + (int)DEG_TO_ANG(aspd / (typ->p.accel_pow + unit) * typ->p.accel_sec * crv);
    ulSinCos(w->orbit_ang, &sc[0], &sc[1]);
    w->orbit.y = sc[0] * w->radius;
    w->orbit.x = sc[1] * w->radius;
    ulSinCos(rate * ANG_TURN, &sc[0], &sc[1]);
    w->radius = enemy_task_get_param(obj, 3) * typ->p.accel_r_rate * rate + w->radius_from * (unit - rate);
    w->tilt[0] = (w->tilt_from[0] & 0x8000 ? w->tilt_from[0] | (int)0xffff0000 : w->tilt_from[0] & 0xffff) * (unit - rate);
    w->tilt[1] = (w->tilt_from[1] & 0x8000 ? w->tilt_from[1] | (int)0xffff0000 : w->tilt_from[1] & 0xffff) * (1.0f - rate);
    crv = rate / typ->p.accel_ang_rate;
    rang = (w->orbit_ang + w->yaw_ofs) & 0xffff;
    w->tilt[2] = crv > 1.0f ? rang : rang * crv;
    enkidu_next_state(obj, w, n, 5);
    return 0;
}

/* 0x00affc48 */
int enkidu_state_accel(void *obj, EnkiduWork *w)
{
    EnkiduVars *typ = w->type;
    float sc[2];

    w->orbit_ang += (int)DEG_TO_ANG(enemy_task_get_param(obj, 0) * typ->p.accel_ang_spd / FPS);
    ulSinCos(w->orbit_ang, &sc[0], &sc[1]);
    w->orbit.y = sc[0] * w->radius;
    w->orbit.x = sc[1] * w->radius;
    w->tilt[2] = w->orbit_ang + w->yaw_ofs;
    return 0;
}

/* 0x00affd00 */
void enkidu_update(void *obj, EnkiduWork *w)
{
    int (*fn)(void *obj, EnkiduWork *w) = enkidu_states[w->state];
    int fire = 0;

    if (fn)
        fire = fn(obj, w);
    enkidu_attack_ray_start(obj, w);
    enkidu_pose_model(obj, w);
    if (fire)
        enkidu_fire_shot(obj, w);
}

/* 0x00affd98 */
void enkidu_exec_appear(void *obj, EnkiduWork *w)
{
    switch (enemy_task_get_phase(obj)) {
    case 0:
        enemy_task_appear_scale(obj, w);
        break;
    case 2:
        enemy_task_appear_scale(obj, w);
        w->lock.flags |= 0x100;
        break;
    default:
        enkidu_update(obj, w);
        if (w->life > 0 && --w->life <= 0) {
            w->lock.flags &= ~0x100;
            enemy_task_set_state(obj, 2);
        }
        enemy_task_appear_scale(obj, w);
        break;
    }
}

/* 0x00affe68 */
void enkidu_exec_normal(void *obj, EnkiduWork *w)
{
    LockObj *lock = &w->lock;

    if (enemy_task_get_phase(obj) != 1)
        return;
    enkidu_update(obj, w);
    if (w->life > 0 && --w->life <= 0) {
        lock->flags &= ~LOCK_LOCKABLE;
        enemy_task_set_state(obj, 2);
    }
}

/* 0x00affee8 */
void enkidu_exec_leave(void *obj, EnkiduWork *w)
{
    float fade;

    if (enemy_task_get_phase(obj) == 1) {
        enkidu_update(obj, w);
        w->fade = enemy_task_get_state_time(obj) / (w->type->p.away_sec * FPS);
        if (w->fade > 1.0f)
            FINISH_IF(obj);
        fade = w->fade;
        fade = MIN(fade >= 0.0f ? fade : 0.0f, 1.0f);
        w->fade = fade;
    }
}

/* 0x00afffd0 */
void enkidu_exec_destroyed(void *obj, EnkiduWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 0) {
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 14;
        desc.models = stage_models;
        desc.lock = &w->lock;
        desc.kind = 0;
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        v3_copy(&desc.v, &w->hitv);
        enemy_shatter_create(obj, &desc, 0, 0);
        FINISH_IF(obj);
    }
}

/* 0x00b000a0 */
void enkidu_exec(void *obj, EnkiduWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (enkidu_exec_table[i])
        enkidu_exec_table[i](obj, w);
}

/* steer(cast) */
/* 0x00b000f8 */
void enkidu_post_exec(void *obj, EnkiduWork *w)
{
    LockObj *lock = &w->lock;
    Vec3 p;

    if (!enemy_status_get_ticks())
        enkidu_pose_model(obj, w);
    if (lock->flags & LOCK_HIT)
        v3_copy(&w->hitv, &lock->imp.v);
    if (lock->life <= 0 && ((unsigned int)enemy_task_get_pstate(obj)) < 2) {
        int mult = lockon_hits_max_shot_cnt(lock);
        EnkiduVars *typ = w->type;

        enemy_task_get_pos(obj, &p);
        enemy_status_score_popup(enemy_task_get_status_slot(obj), &p, typ->p.score, mult);
        enemy_task_set_state(obj, 3);
    }
}

/* steer(cast) */
/* 0x00b001d8 */
void enkidu_disp_body(void *obj, EnkiduWork *w)
{
    Vec3 p;
    int ang[4];
    Vec3 scl;
    float rgba[4];
    ModelPart *gfx;
    EnkiduVars *typ;
    float eps, frm;
    int blend;

    gfx = enemy_task_gfx_ptr(obj);
    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    enemy_task_get_scl(obj, &scl);
    if (view_sphere_visible(&p, gfx->radius)) {
        eps = FLT_EPSILON;
        blend = 0;
        if (!(fabsf(w->fade) < eps)) {
            blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
            draw_push_strip_flags();
            draw_push_override_color();
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 0x800);
            heap_bzero(rgba, 16);
            rgba[0] = -w->fade;
            draw_set_override_color(rgba);
        }
        frm = 0.0f;
        ulGxSetTextureList(((void **)obj)[1]);
        ulMatrixPush();
        if (w->shoots)
            frm = gfx->frame;
        typ = w->type;
        do {
            Vec3 *p_ = &p;

            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        enkidu_rotate_yxz(ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
        model_object_draw(gfx, frm);
        if (typ->p.wire) {
            draw_set_color(yuki_beat_color(typ->p.wire_col, typ->p.wire_col2, typ->p.wire_beat, typ->p.wire_linear, 1.0f - w->fade,
                                          typ->p.wire_near, typ->p.wire_far, typ->p.wire_near_rate,
                                          typ->p.wire_far_rate));
            draw_set_blend(4, 5);
            model_object_draw((struct DcObjectTreeView *)&w->cover, frm);
        }
        ulMatrixPop();
        if (!(fabsf(w->fade) < eps)) {
            draw_pop_strip_flags();
            draw_pop_override_color();
            draw_set_override_flags(blend);
        }
    }
}

/* 0x00b003f0 */
void enkidu_disp(void *obj, EnkiduWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (enkidu_disp_table[i])
        enkidu_disp_table[i](obj, w);
}

/* 0x00b00448 */
int enkidu_create(void *parent, Placement *place)
{
    ObjDesc desc;
    ObjFuncs funcs = enkidu_funcs;
    int kind;
    EnkiduVars *typ;

    kind = 0;
    typ = enkidu_types;
    nj_fill8(&desc, 0, sizeof desc);
    desc.funcs = &funcs;
    desc.arg = place;
    desc.kind = &kind;
    desc.name = enkidu_name;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.flags = 18;
    desc.hp = typ->p.hp;
    desc.same_lock = typ->p.same_lock;
    desc.model_no = 10;
    desc.model_no2 = 12;
    desc.score = typ->p.score;
    desc.hp_reduce = typ->p.hp_reduce;
    desc.crash_r = typ->p.crash_r;
    desc.crash_damage = typ->p.crash_damage;
    desc.time = ((float *)place)[14];
    desc.size = 316;
    desc.dc_flags = 0;
    return enemy_task_task_create(parent, &desc);
}

/* 0x00b00580 */
void enkidu_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
