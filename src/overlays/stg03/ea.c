/*
 * Ea, Area 3 enemy (three kinds): drifts in rolling, then steps: brakes, turns to its shot aim,
 * fires an attack ray on the steps its placement selects, dashes sideways to the next.
 */

#include "stg03.h"
#include "mathdef.h"
#include "anim.h"
#include "debug.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_cnk_vu1.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "node_tree.h"
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

/* Named by the 'ea' VarDefs. */
typedef struct EaParams {
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
    int life_time;              /* 0x40 */
    int stairs_time;            /* 0x44 */
    float break_sec;            /* 0x48 */
    float break_rate;           /* 0x4c */
    int break_ang;              /* 0x50 */
    float break_ang_pow;        /* 0x54 */
    float up_sec;               /* 0x58 */
    float up_over_dis_rate;     /* 0x5c */
    float up_pow;               /* 0x60 */
    float up_spd;               /* 0x64 */
    float accel_sec;            /* 0x68 */
    int tail_roll_ang;          /* 0x6c */
    float tail_roll_accel;      /* 0x70 */
    float atck_motion_spd_rate; /* 0x74 */
    float roll_back_pow;        /* 0x78 */
    float ray_sec;              /* 0x7c */
    float ray_add_sec;          /* 0x80 */
    float ray_scl;              /* 0x84 */
} EaParams;

typedef struct EaVars {
    char *name;
    char *label;                /* 0x04 */
    EaParams p;                 /* 0x08 */
} EaVars;

typedef struct EaWork {
    EaVars *type;
    int frames;                 /* 0x04: frames in the state */
    int life;                   /* 0x08: frames left; 0 = no limit */
    Vec3 hitv;                  /* 0x0c: the lock-on record's vector when hit */
    LockObj lock;               /* 0x18 */
    int state;                  /* 0x58 */
    ModelPart cover;            /* 0x5c: the wire cover */
    Vec3 muzzle;                /* 0xb8 */
    Vec3 core;                  /* 0xc4: lock-on point */
    Vec3 from;                  /* 0xd0: position at the state's start */
    Vec3 dir;                   /* 0xdc */
    float speed;                /* 0xe8 */
    int base_ang[3];            /* 0xec */
    int from_ang[3];            /* 0xf8 */
    int aim_ang[3];             /* 0x104 */
    int stair;                  /* 0x110: steps climbed */
    int step[2][6];             /* 0x114: direction xyz, length, roll, roll sign */
    int roll_from;              /* 0x144 */
    int roll_to;                /* 0x148 */
    int tail_roll;              /* 0x14c */
    int ray_count;              /* 0x150 */
    int ray_mask;               /* 0x154 */
    int shot_level;             /* 0x158 */
    int rays[2][3];             /* 0x15c: start frame, end frame, started */
} EaWork;

void ea_schedule_rays(void *obj, EaWork *w);

extern int ea_init_arg;
extern void (*ea_exec_table[])(void *o, EaWork *w);
extern void (*ea_disp_table[])(void *o, EaWork *w);
void ea_vars_register(void);
void ea_vars_unregister(void);
int ea_vars_init(int *fx_id);
int ea_vars_release(void);
int ea_marker_draw(int obj, void *place);
void ea_exec(void *obj, EaWork *w);
void ea_disp(void *obj, EaWork *w);
void ea_rotate_yxz(int *ang);
extern EaVars ea_types[3];
extern VarDef ea_vardef;
extern VarDef ea_vardefs[34];
void ea_update(void *obj, EaWork *w);
extern int (*ea_states[])(void *o, EaWork *w);
void ea_attack_rays(void *obj, EaWork *w);
void ea_pose_model(void *obj, EaWork *w);
void ea_fire_shot(void *obj, EaWork *w);
int ea_collect_mounts(int *pos_a, int *pos_b, unsigned int *node, int cnt);
extern Vec3 ea_start_scl;
extern char ea_lock_name[];
extern Vec3 ea_ray_scl;
extern ObjFuncs ea_funcs;
extern char ea_name[];
void ea_dash_roll(void *obj, EaWork *w, float *stair, int n, float rate);

/* 0x00afb658 */
void ea_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *prm;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&ea_vardef);
    for (i = 0; i < 3; i++) {
        def = ea_vardefs;
        prm = (int *)&ea_types[i].p;
        ea_vardef.name = ea_types[i].name;
        ea_vardef.fmt = ea_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&ea_vardef);
        for (j = 0; j < 34; j++) {
            def->ptr = prm;
            prm++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)ea_vardefs);
    }
}

/* 0x00afb770 */
void ea_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00afb778 */
int ea_vars_init(int *fx_id)
{
    ea_init_arg = *fx_id;
    ea_vars_register();
    return 0;
}

/* 0x00afb7a0 */
int ea_vars_release(void)
{
    ea_vars_unregister();
    return 0;
}

/* 0x00afb7c0 */
int ea_marker_draw(int obj, void *place)
{
    int *ent;

    ulGxSetTextureList(stage_textures);
    ent = pnb_get_entry(stage_models, 0);
    return obj_draw_at(obj, place, (void *)*ent);
}

/* 0x00afb818 */
int ea_get_init_arg(void)
{
    return ea_init_arg;
}

/* 0x00afb828 */
int ea_collect_mounts(int *pos_a, int *pos_b, unsigned int *node, int cnt)
{
    do {
        ulMatrixPush();
        anim_apply_node(node);
        if (node[0] & 0x4000000) {
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
            cnt = ea_collect_mounts(pos_a, pos_b, (unsigned int *)node[11], cnt);
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

/* 0x00afb960 */
void ea_schedule_rays(void *obj, EaWork *w)
{
    EaVars *typ = w->type;
    int *rec = w->rays[0];
    int mask = w->ray_mask;
    int frame = 0;
    int i;
    int j = 0;

    for (i = 0; i < typ->p.stairs_time; i++, rec += 3) {
        int on = mask % 4 & (j + 1) % 3;
        float *stair = (float *)w->step[j % 2];
        float len;

        frame += enemy_task_get_param(obj, 1) * 60.0f / 60.0f + 1.0f;
        frame += typ->p.break_sec * FPS + 1.0f;
        if (on <= 0) {
            rec[0] = -1;
            rec[2] = 1;
            rec[1] = frame;
        } else {
            ModelPart *gfx = enemy_task_gfx_ptr(obj);
            float v;

            frame += typ->p.ray_add_sec * FPS + 1.0f;
            if (!i) {
                v = frame - typ->p.ray_sec * FPS;
                rec[0] = v <= 0.0f ? 0 : (int)v;
            } else {
                float prev = rec[-2];

                v = frame - typ->p.ray_sec * FPS;
                rec[0] = prev <= v ? v : prev;
            }
            rec[1] = frame;
            rec[2] = 0;
            frame += (gfx->frames - 32.0f) / (gfx->speed * w->type->p.atck_motion_spd_rate);
        }
        len = stair[3];
        if (fabsf(len) < FLT_EPSILON)
            frame += 1;
        else
            frame += len / (typ->p.up_spd * FRAME_SEC);
        frame += typ->p.accel_sec * FPS + 1.0f;
        j++;
    }
}

/* 0x00afbc60 */
void ea_attack_rays(void *obj, EaWork *w)
{
    int *sched = w->rays[0];
    int now = enemy_task_get_time(obj);
    EaVars *typ = w->type;
    AttackRayArg ray;
    ModelPart *gfx;
    int i;

    for (i = 0; i < typ->p.stairs_time; i++, sched += 3) {
        if (now < sched[0])
            continue;
        if (sched[2] == 0) {
            sched[2] = 1;
            heap_bzero(&ray, sizeof ray);
            ray.pos = &w->muzzle;
            ray.ang = enemy_task_rot_ptr(obj);
            ray.scale = &ea_ray_scl;
            ray.dur = sched[1] - sched[0];
            attack_ray_create(obj, &ray);
        }
        if (sched[0] >= 0 && now <= sched[1]) {
            gfx = enemy_task_gfx_ptr(obj);
            gfx->frame = (now - sched[0]) * 32.0f / (sched[1] - sched[0]);
        }
    }
}

/* steer(cast) */
/* 0x00afbdd0 */
void ea_init(void *obj, EaWork *w, int place, int *kind)
{
    EaVars *typ = &ea_types[kind[0]];
    Vec3 p;
    Vec3 v;
    int ang[3];
    float sc[2][2];
    float x, y, z, spd;
    int *stair;
    int i, n;

    w->life = typ->p.life_time;
    w->type = typ;
    w->state = typ->p.stairs_time > 0;
    w->frames = 0;
    enemy_task_clear_flags(obj, 64);
    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &v);
    spd = vec3_length(&v);
    w->speed = spd;
    if (!(fabsf(spd) < FLT_EPSILON)) {
        vec3_normalize(&w->dir, &v);
        do {
            ang[1] = RAD_TO_ANG(atan2f(-v.x, -v.z));
            ang[0] = RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
        } while (0);
        ang[2] = 0;
    } else {
        v3_set(&w->dir, 0.0f, 0.0f, 0.0f);
        i3_set(ang, 0, 0, 0);
    }
    enemy_task_set_rot(obj, ang);
    i3_copy(w->base_ang, ang);
    v3_copy(&w->from, &p);
    i3_copy(w->from_ang, ang);
    enemy_task_set_scl(obj, &ea_start_scl);
    w->stair = 0;
    for (i = 0, n = 2, stair = w->step[0]; i < 2; n += 2, i++, stair += 6) {
        x = enemy_task_get_param(obj, n);
        y = enemy_task_get_param(obj, n + 1);
        z = 0.0f;
        *(float *)&stair[3] = sqrtf(x * x + y * y);
        if (fabsf(*(float *)&stair[3]) < FLT_EPSILON) {
            *(float *)&stair[2] = *(float *)&stair[1] = *(float *)&stair[0] = z;
            stair[4] = 0;
        } else {
            int sang = RAD_TO_ANG(atan2f(-x, y));

            stair[5] = 1;
            stair[4] = sang;
            if (sang > ANG_90) {
                stair[5] = -1;
                stair[4] = sang - ANG_180;
            } else if (sang < -ANG_90) {
                stair[5] = -1;
                stair[4] = sang + ANG_180;
            }
            x /= *(float *)&stair[3];
            y /= *(float *)&stair[3];
            ROT_X(ang[0], sc[0], y, z);
            ROT_Y(ang[1], sc[1], x, z);
            *(float *)&stair[0] = x;
            *(float *)&stair[1] = y;
            *(float *)&stair[2] = z;
        }
    }
    w->roll_to = 0;
    w->roll_from = 0;
    w->tail_roll = 0;
    w->ray_mask = enemy_task_get_param(obj, 0) + 0.5f;
    {
        int level = (int)(enemy_task_get_param(obj, 0) * 1000.0f + 0.5f) % 1000;

        w->ray_count = 0;
        level = level > -1 ? level : 0;
        w->shot_level = level < 16 ? level : 15;
    }
    lockon_obj_init(&w->lock, obj, &w->core, 0, ea_lock_name, enemy_task_get_status_slot(obj), typ->p.hp, typ->p.same_lock, 0);
    lockon_entry(&w->lock);
    dc_object_init(&w->cover, *((struct DcNode **)pnb_get_entry(*(void **)obj, 2)), *((struct DcClip **)pnb_get_entry(*(void **)obj, 3)), 0x500);
    dc_object_set_wire(&w->cover, model_pnb_get_wire(*(void **)obj, 2));
    ((ModelPart *)enemy_task_gfx_ptr(obj))->radius = w->cover.radius = 45.0f;
    ea_schedule_rays(obj, w);
}

/* 0x00afc2f0 */
void ea_release(void *obj, EaWork *w)
{
    dc_object_release(&w->cover);
    lockon_remove(&w->lock);
}

/* 0x00afc320 */
void ea_fire_shot(void *obj, EaWork *w)
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
    e_shot_fire(obj, &w->muzzle, &dir, w->shot_level);
}

/* 0x00afc418 */
void ea_pose_model(void *obj, EaWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 p;
    int ang[4];
    Vec3 scl;

    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    enemy_task_get_scl(obj, &scl);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &p;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    ea_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
    anim_start((void *)gfx->clip, gfx->frame);
    ea_collect_mounts((int *)&w->core, (int *)&w->muzzle, (unsigned int *)gfx->root, 0);
    ulMatrixPop();
}

/* 0x00afc4f8 */
int ea_next_state(void *obj, EaWork *w, int n, int state)
{
    if (++w->frames > n) {
        w->state = state;
        w->frames = 1;
        enemy_task_get_pos(obj, &w->from);
        enemy_task_get_rot(obj, w->from_ang);
        return 1;
    }
    return 0;
}

/* 0x00afc568 */
int ea_state_wait(void *obj, EaWork *w)
{
    EaVars *typ = w->type;
    int n = enemy_task_get_param(obj, 1) * 60.0f / 60.0f + 1.0f;
    int frm_accel, frm_break, frm_up, total;
    float rate;
    float racc;
    float roll;
    Vec3 p;
    Vec3 v;
    int ang[3];

    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &v);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    if (w->stair == 0) {
        w->tail_roll += typ->p.tail_roll_ang / FPS;
    } else {
    frm_accel = typ->p.accel_sec * FPS + 1.0f;
    frm_break = typ->p.break_sec * FPS + 1.0f;
    frm_up = typ->p.up_sec * FPS + 1.0f;
    total = n + frm_accel + frm_break;
    rate = safe_powf((float)(frm_accel + w->frames) / total, typ->p.roll_back_pow);
    rate = MIN(0.0f <= rate ? rate : 0.0f, 1.0f);
    i3_copy(ang, w->base_ang);
    ang[2] += (int)(w->roll_from * (1.0f - rate));
    enemy_task_set_rot(obj, ang);
    total = n + frm_accel + frm_break + frm_up;
    racc = typ->p.tail_roll_accel * (1.0f - (float)(frm_accel + w->frames) / total);
    roll = typ->p.tail_roll_ang / FPS;
    w->tail_roll += racc <= 1.0f ? roll : roll * racc;
    w->frames++;
    }
    return 0;
}

/* 0x00afc810 */
int ea_state_turn(void *obj, EaWork *w)
{
    EaVars *typ = w->type;
    int n = enemy_task_get_param(obj, 1) * 60.0f / 60.0f + 1.0f;
    Vec3 p;
    Vec3 v;
    int ang[3];

    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &v);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    if (w->stair == 0) {
        int frm_break, frm_up;

        frm_break = typ->p.break_sec * FPS + 1.0f;
        frm_up = typ->p.up_sec * FPS + 1.0f;
        w->tail_roll += typ->p.tail_roll_ang / FPS * (1.0f - (float)w->frames / (n + frm_break + frm_up));
    } else {
        int frm_accel, frm_break, frm_up, total;
        float rate;

        frm_accel = typ->p.accel_sec * FPS + 1.0f;
        frm_break = typ->p.break_sec * FPS + 1.0f;
        frm_up = typ->p.up_sec * FPS + 1.0f;
        total = n + frm_accel + frm_break;
        rate = safe_powf((float)(frm_accel + w->frames) / total, typ->p.roll_back_pow);
        i3_copy(ang, w->base_ang);
        ang[2] += (int)((w->roll_to - w->roll_from) * rate + w->roll_from);
        enemy_task_set_rot(obj, ang);
        total = n + frm_accel + frm_break + frm_up;
        w->tail_roll += typ->p.tail_roll_ang * typ->p.tail_roll_accel / FPS * (1.0f - (float)(frm_accel + w->frames) / total);
    }
    ea_next_state(obj, w, n, 2);
    return 0;
}

/* 0x00afcad8 */
void ea_ray_update(void *obj, EaWork *w, int n)
{
}

/* 0x00afcae0 */
int ea_state_break(void *obj, EaWork *w)
{
    EaVars *typ = w->type;
    int on = w->ray_mask % 4 & (w->stair + 1) % 3;
    int frm_break = typ->p.break_sec * FPS + 1.0f;
    float rate = w->frames / (float)frm_break;
    float prev = (w->frames - 1) / (float)frm_break;
    int frm_accel = typ->p.accel_sec * FPS + 1.0f;
    int n = enemy_task_get_param(obj, 1) * 60.0f / 60.0f + 1.0f;
    int frm_up = typ->p.up_sec * FPS + 1.0f;
    float spd = w->speed * FPS;
    float acc, step, ease;
    int *stair;
    int rz, rx;
    Vec3 p;
    Vec3 dv;
    int ang[3];
    int aim[3];

    if (fabsf(typ->p.break_sec) < FLT_EPSILON)
        acc = 0.0f;
    else
        acc = spd * (typ->p.break_rate - 1.0f) / typ->p.break_sec;
    spd += acc * typ->p.break_sec * prev;
    step = spd * (1.0f / FPS) + acc * 0.5f * (1.0f / FPS) * (1.0f / FPS);
    v3_mul(&dv, &w->dir, step);
    enemy_task_get_pos(obj, &p);
    v3_add(&p, &dv);
    enemy_task_set_pos(obj, &p);
    if (on)
        ea_ray_update(obj, w, w->frames);
    {
        int j = w->stair % 2;

        stair = w->step[j];
    }
    ease = 1.0f - safe_powf(1.0f - rate, typ->p.break_ang_pow);
    rx = typ->p.break_ang * ease * stair[5];
    ulMatrixPush();
    matrix_unit_set(0);
    if (w->stair == 0) {
        rz = stair[4] * ease;
        ea_rotate_yxz(w->from_ang);
    } else {
        rz = (w->roll_to - w->roll_from) * safe_powf((float)(frm_accel + n + w->frames) / (frm_accel + n + frm_break), typ->p.roll_back_pow) + w->roll_from;
        ea_rotate_yxz(w->base_ang);
    }
    vu0_matrix_rotate_z(ulMatrixCur, rz);
    vu0_matrix_rotate_x(ulMatrixCur, rx);
    model_camera_angles(ang);
    ulMatrixPop();
    if (on) {
        e_shot_initial_angles(aim, &p, 0, w->shot_level);
        i3_copy(w->aim_ang, ang);
        yuki_lerp3(ang, ang, aim, rate);
    }
    enemy_task_set_rot(obj, ang);
    if (w->stair == 0)
        w->tail_roll += typ->p.tail_roll_ang / FPS * (1.0f - (float)(n + w->frames) / (frm_break + n + frm_up));
    else {
        int total = frm_break + n + (frm_up + frm_accel);

        w->tail_roll += typ->p.tail_roll_ang * typ->p.tail_roll_accel / FPS * (1.0f - (float)(n + frm_accel + w->frames) / total);
    }
    ea_next_state(obj, w, frm_break, on ? 3 : 5);
    return 0;
}

/* 0x00afd000 */
int ea_state_ray(void *obj, EaWork *w)
{
    EaVars *typ = w->type;
    int n = typ->p.ray_add_sec * FPS + 1.0f;

    ea_ray_update(obj, w, w->frames + typ->p.break_sec * FPS + 1.0f);
    if (ea_next_state(obj, w, n, 4)) {
        w->ray_count = 0;
        return 1;
    }
    return 0;
}

/* 0x00afd0a8 */
int ea_state_attack_end(void *obj, EaWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    EaVars *typ = w->type;
    int ang[3];
    float frm;

    frm = gfx->frame;
    frm += gfx->speed * typ->p.atck_motion_spd_rate;
    if (frm < gfx->frames) {
        gfx->frame = frm;
        yuki_lerp3(ang, w->from_ang, w->aim_ang, (frm - 32.0f) / (gfx->frames - 32.0f));
        enemy_task_set_rot(obj, ang);
    } else {
        gfx->frame = 0.0f;
        enemy_task_set_rot(obj, w->aim_ang);
        ea_next_state(obj, w, 0, 5);
    }
    return 0;
}

/* 0x00afd170 */
void ea_dash_roll(void *obj, EaWork *w, float *stair, int n, float rate)
{
    EaVars *typ = w->type;
    int ang[3];
    float sc[3];
    int rx;

    ulSinCos(rate * ANG_QUARTER, &sc[0], &sc[1]);
    ulSinCos(sc[0] * ANG_TURN - ANG_QUARTER, &sc[2], &sc[1]);
    rx = typ->p.break_ang * (1.0f - (sc[0] + (sc[2] + 1.0f) * 0.5f * (1.0f - rate))) * ((int *)stair)[5];
    ulMatrixPush();
    matrix_unit_set(0);
    ea_rotate_yxz(w->base_ang);
    vu0_matrix_rotate_z(ulMatrixCur, ((int *)stair)[4]);
    vu0_matrix_rotate_x(ulMatrixCur, rx);
    model_camera_angles(ang);
    ulMatrixPop();
    enemy_task_set_rot(obj, ang);
    if (w->stair == 0) {
        int frm_wait = enemy_task_get_param(obj, 1) * 60.0f / 60.0f + 1.0f;
        int frm_break = typ->p.break_sec * FPS + 1.0f;

        {
            int total = n + frm_wait + frm_break;

            w->tail_roll += typ->p.tail_roll_ang / FPS * (1.0f - (float)(frm_wait + frm_break + w->frames) / total);
        }
    } else {
        int frm_accel = typ->p.accel_sec * FPS + 1.0f;
        int frm_wait = enemy_task_get_param(obj, 1) * 60.0f / 60.0f + 1.0f;
        int frm_break = typ->p.break_sec * FPS + 1.0f;

        {
            int elapsed = frm_accel + w->frames;
            int pre = n + frm_break;
            int total = pre + (frm_wait + frm_accel);

            w->tail_roll += typ->p.tail_roll_ang * typ->p.tail_roll_accel / FPS * (1.0f - (float)(elapsed + (frm_break + frm_wait)) / total);
        }
    }
}

/* 0x00afd450 */
int ea_state_dash(void *obj, EaWork *w)
{
    float *stair = (float *)w->step[w->stair % 2];
    EaVars *typ;
    float len = stair[3];
    int n;
    float rate, prev;
    float ease, ease_prev, step;
    Vec3 p;
    Vec3 v;
    float sc[3];

    typ = w->type;
    if (fabsf(len) < FLT_EPSILON)
        n = 1;
    else
        n = len / (typ->p.up_spd * FRAME_SEC);
    rate = (float)w->frames / n;
    prev = (float)(w->frames - 1) / n;
    enemy_task_get_pos(obj, &p);
    v3_mul(&v, &w->dir, w->speed * typ->p.break_rate);
    v3_add(&p, &v);
    ulSinCos(rate * ANG_QUARTER, &sc[0], &sc[1]);
    ulSinCos(sc[0] * ANG_TURN - ANG_QUARTER, &sc[2], &sc[1]);
    ease = sc[0] + (sc[2] + 1.0f) * 0.5f * safe_powf(1.0f - rate, typ->p.up_pow) * typ->p.up_over_dis_rate;
    ulSinCos(prev * ANG_QUARTER, &sc[0], &sc[1]);
    ulSinCos(sc[0] * ANG_TURN - ANG_QUARTER, &sc[2], &sc[1]);
    ease_prev = sc[0] + (sc[2] + 1.0f) * 0.5f * safe_powf(1.0f - prev, typ->p.up_pow) * typ->p.up_over_dis_rate;
    step = (ease - ease_prev) * len;
    v3_mul(&v, (Vec3 *)stair, step);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    ea_dash_roll(obj, w, stair, n, rate);
    if (ea_next_state(obj, w, n, 6)) {
        w->roll_from = ((int *)stair)[4];
        w->roll_to = w->step[(w->stair + 1) % 2][4];
    }
    return 0;
}

/* 0x00afd7c0 */
int ea_state_brake(void *obj, EaWork *w)
{
    EaVars *typ = w->type;
    int n = typ->p.accel_sec * FPS + 1.0f;
    float prev = (float)(w->frames - 1) / n;
    int frm_wait = enemy_task_get_param(obj, 1) * 60.0f / 60.0f + 1.0f;
    int frm_break = typ->p.break_sec * FPS + 1.0f;
    float spd = w->speed * FPS;
    float acc;
    float step;
    float rate;
    int roll;
    Vec3 p;
    Vec3 v;
    int ang[3];

    if (fabsf(typ->p.accel_sec) < FLT_EPSILON)
        acc = 0.0f;
    else
        acc = spd * (1.0f - typ->p.break_rate) / typ->p.accel_sec;
    spd *= typ->p.break_rate;
    spd += acc * typ->p.accel_sec * prev;
    step = spd * FRAME_SEC + acc * 0.5f * FRAME_SEC * FRAME_SEC;
    v3_mul(&v, &w->dir, step);
    enemy_task_get_pos(obj, &p);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    rate = safe_powf((float)w->frames / (frm_wait + frm_break + n), typ->p.roll_back_pow);
    if (w->stair + 1 < typ->p.stairs_time)
        roll = (w->roll_to - w->roll_from) * rate + w->roll_from;
    else
        roll = w->roll_from * (1.0f - rate);
    i3_copy(ang, w->base_ang);
    ang[2] += roll;
    enemy_task_set_rot(obj, ang);
    {
        int frm_up = typ->p.up_sec * FPS + 1.0f;

        w->tail_roll += typ->p.tail_roll_ang / FPS * typ->p.tail_roll_accel * (1.0f - (float)w->frames / (frm_wait + (n + (frm_up + frm_break))));
    }
    if (ea_next_state(obj, w, n, 1)) {
        if (++w->stair >= typ->p.stairs_time)
            w->state = 0;
    }
    return 0;
}

/* 0x00afdb18 */
void ea_update(void *obj, EaWork *w)
{
    int (*fn)(void *obj, EaWork *w) = ea_states[w->state];
    int fire = 0;

    if (fn)
        fire = fn(obj, w);
    ea_attack_rays(obj, w);
    ea_pose_model(obj, w);
    if (fire)
        ea_fire_shot(obj, w);
}

/* 0x00afdbb0 */
void ea_exec_appear(void *obj, EaWork *w)
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
        ea_update(obj, w);
        if (w->life > 0 && --w->life <= 0) {
            w->lock.flags &= ~0x100;
            enemy_task_set_state(obj, 2);
        }
        enemy_task_appear_scale(obj, w);
        break;
    }
}

/* 0x00afdc80 */
void ea_exec_normal(void *obj, EaWork *w)
{
    LockObj *lock = &w->lock;

    if (enemy_task_get_phase(obj) != 1)
        return;
    ea_update(obj, w);
    if (w->life > 0 && --w->life <= 0) {
        lock->flags &= ~LOCK_LOCKABLE;
        enemy_task_set_state(obj, 2);
    }
}

/* 0x00afdd00 */
void ea_exec_leave(void *obj, EaWork *w)
{
    if (enemy_task_get_phase(obj) != 1) {
        w->lock.flags &= ~0x100;
        enemy_task_vanish_scale(obj, w);
        return;
    }
    ea_update(obj, w);
    enemy_task_vanish_scale(obj, w);
}

/* 0x00afdd78 */
void ea_exec_destroyed(void *obj, EaWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 0) {
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 4;
        desc.models = stage_models;
        desc.lock = &w->lock;
        desc.kind = 0;
        desc.unk_50 = 0;
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        v3_copy(&desc.v, &w->hitv);
        enemy_shatter_create(obj, &desc, 0, 0);
        FINISH_IF(obj);
    }
}

/* 0x00afde48 */
void ea_exec(void *obj, EaWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (ea_exec_table[i])
        ea_exec_table[i](obj, w);
}

/* steer(cast) */
/* 0x00afdea0 */
void ea_post_exec(void *obj, EaWork *w)
{
    LockObj *lock = &w->lock;
    Vec3 p;

    if (!enemy_status_get_ticks())
        ea_pose_model(obj, w);
    ea_ray_scl.z = ea_ray_scl.y = ea_ray_scl.x = w->type->p.ray_scl;
    if (lock->flags & LOCK_HIT)
        v3_copy(&w->hitv, &lock->imp.v);
    if (lock->life <= 0 && ((unsigned int)enemy_task_get_pstate(obj)) < 2) {
        int mult = lockon_hits_max_shot_cnt(lock);
        EaVars *typ = w->type;

        enemy_task_get_pos(obj, &p);
        enemy_status_score_popup(enemy_task_get_status_slot(obj), &p, typ->p.score, mult);
        enemy_task_set_state(obj, 3);
    }
}

/* 0x00afdfa0 */
void ea_draw_nodes(unsigned int *node, int angle)
{
    do {
        ulMatrixPush();
        anim_apply_node(node);
        if (node[0] & 0x2000000)
            vu0_matrix_rotate_z(ulMatrixCur, angle);
        if (node[1])
            nj_cnk_vu1_draw((void *)node[1]);
        if (!(node[0] & 0x10))
            ea_draw_nodes((unsigned int *)node[11], angle);
        ulMatrixPop();
        node = (unsigned int *)node[12];
    } while (node);
}

/* 0x00afe048 */
void ea_disp_body(void *obj, EaWork *w)
{
    Vec3 p;
    int ang[4];
    Vec3 scl;
    ModelPart *gfx;
    EaVars *typ;

    gfx = enemy_task_gfx_ptr(obj);
    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    enemy_task_get_scl(obj, &scl);
    if (view_sphere_visible(&p, gfx->radius)) {
        ulGxSetTextureList(((void **)obj)[1]);
        do {
            ulMatrixPush();
        } while (0);
        typ = w->type;
        vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
        ea_rotate_yxz(ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
        anim_start((void *)gfx->clip, gfx->frame);
        ea_draw_nodes((unsigned int *)gfx->root, w->tail_roll);
        if (typ->p.wire) {
            draw_set_color(yuki_beat_color(typ->p.wire_col, typ->p.wire_col2, typ->p.wire_beat, typ->p.wire_linear, 1.0f, typ->p.wire_near,
                                          typ->p.wire_far, typ->p.wire_near_rate, typ->p.wire_far_rate));
            draw_set_blend(4, 5);
            model_object_draw((struct DcObjectTreeView *)&w->cover, gfx->frame);
        }
        ulMatrixPop();
    }
}

/* 0x00afe198 */
void ea_disp(void *obj, EaWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (ea_disp_table[i])
        ea_disp_table[i](obj, w);
}

/* steer(cast) */
/* 0x00afe1f0 */
int ea_create(void *parent, Placement *place)
{
    ObjDesc desc;
    ObjFuncs funcs = ea_funcs;
    int kind;
    int i;
    EaVars *typ;

    i = ((float (*)(float))float_frac)(place->param1) * 1000.0f + 0.5f;
    if (i < 0)
        i = 0;
    if ((unsigned int)i > 2)
        i = 2;
    kind = i;
    typ = &ea_types[kind];
    nj_fill8(&desc, 0, sizeof desc);
    desc.kind = &kind;
    desc.name = ea_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->p.hp;
    desc.same_lock = typ->p.same_lock;
    desc.flags = 18;
    desc.model_no2 = 1;
    desc.score = typ->p.score;
    desc.hp_reduce = typ->p.hp_reduce;
    desc.crash_r = typ->p.crash_r;
    desc.crash_damage = typ->p.crash_damage;
    desc.time = typ->p.life_time + 1;
    desc.size = 372;
    desc.dc_flags = 0;
    desc.model_no = 0;
    return enemy_task_task_create(parent, &desc);
}

/* 0x00afe378 */
void ea_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
