/*
 * Ishtar's arms, Area 3, two per core: trail the core firing attack rays; once released (or the
 * core is shot) they turn and dive at the player, shattering on contact or out of view.
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
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "ishtar_arm.h"
#include "e_shot.h"
#include "../common/attack_ray.h"
#include "../common/en_backfire.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"

/* Named by the 'ishtar_arm' VarDefs. */
typedef struct IshtarArmParams {
    int hp;
    int same_lock;              /* 0x04 */
    int score;                  /* 0x08 */
    float hp_reduce;            /* 0x0c */
    float crash_r;              /* 0x10 */
    float crash_damage;         /* 0x14 */
    float bullet_spd;           /* 0x18 */
    float hokan_rate;           /* 0x1c */
    float hist_sec;             /* 0x20 */
    float blow_sec;             /* 0x24 */
    float blow_spd;             /* 0x28 */
    float blow_spd_rate;        /* 0x2c */
    float blow_down_dis;        /* 0x30 */
    int blow_ang_spd;           /* 0x34 */
    float break_sec;            /* 0x38 */
    float break_ang_pow;        /* 0x3c */
    float break_accel_dis;      /* 0x40 */
    float break_backfire_scl;   /* 0x44 */
    float break_backfire_scl2;  /* 0x48 */
    float accel;                /* 0x4c */
    float accel_spd_max;        /* 0x50 */
    int tail_roll_spd;          /* 0x54 */
    int tail_roll_spd_tackle;   /* 0x58 */
    float motion_spd;           /* 0x5c */
    float motion_spd_shot;      /* 0x60 */
    float attack_ray_size;      /* 0x64 */
    int tackle_roll_spd;        /* 0x68 */
    int tackle_clip_ang;        /* 0x6c */
} IshtarArmParams;

typedef struct IshtarArmVars {
    char *name;
    char *label;                /* 0x04 */
    IshtarArmParams p;          /* 0x08 */
} IshtarArmVars;

/* ishtar_arm_create's argument, from the core. */
typedef struct IshtarArmArg {
    Vec3 *pos;
    int *ang;                   /* 0x04 */
    int side;                   /* 0x08: 0 left, 1 right */
    int delay;                  /* 0x0c: frames to the ray */
    int level;                  /* 0x10: shot level */
    int life;                   /* 0x14 */
} IshtarArmArg;

struct IshtarArmWork {
    IshtarArmVars *type;
    int frames;                 /* 0x04: frames in the state */
    int life;                   /* 0x08: frames left; 0 = no limit */
    Vec3 core;                  /* 0x0c: lock-on point */
    Vec3 muzzle;                /* 0x18 */
    LockObj lock;               /* 0x24 */
    Vec3 hitv;                  /* 0x64: the lock-on record's vector when hit */
    ModelPart body;             /* 0x70 */
    int state;                  /* 0xcc */
    IshtarArmArg arg;           /* 0xd0 */
    Vec3 *trail;                /* 0xe8: the core's past positions, player relative */
    int trail_len;              /* 0xec */
    int trail_at;               /* 0xf0 */
    float speed;                /* 0xf4 */
    int spin[3];                /* 0xf8: per frame while flying */
    int ang[3];                 /* 0x104 */
    float backfire_scl;         /* 0x110 */
    char pad_114[4];
    int fired;                  /* 0x118 */
    int ray_started;            /* 0x11c */
    int tail_roll;              /* 0x120 */
    Vec3 ray_scl;               /* 0x124 */
    Vec3 pos_buf;               /* 0x130: own position once detached */
    int ang_buf[3];             /* 0x13c */
};

extern int ishtar_arm_init_arg;
extern void (*ishtar_arm_exec_table[])(void *o, IshtarArmWork *w);
extern IshtarArmVars ishtar_arm_types[1];
extern VarDef ishtar_arm_vardef;
extern VarDef ishtar_arm_vardefs[28];

extern void (*ishtar_arm_disp_table[])(void *o, IshtarArmWork *w);

extern int (*ishtar_arm_states[])(void *o, IshtarArmWork *w);
extern int (*ishtar_arm_held_states[])(void *o, IshtarArmWork *w);
extern ObjFuncs ishtar_arm_funcs;
extern char ishtar_arm_name[];
extern char ishtar_arm_lock_name[];
extern Vec3 ishtar_arm_start_scl;

/* 0x00b02e28 */
void ishtar_arm_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *prm;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&ishtar_arm_vardef);
    for (i = 0; i < 1; i++) {
        def = ishtar_arm_vardefs;
        prm = (int *)&ishtar_arm_types[i].p;
        ishtar_arm_vardef.name = ishtar_arm_types[i].name;
        ishtar_arm_vardef.fmt = ishtar_arm_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&ishtar_arm_vardef);
        for (j = 0; j < 28; j++) {
            def->ptr = prm;
            prm++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)ishtar_arm_vardefs);
    }
}

/* 0x00b02f40 */
void ishtar_arm_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b02f48 */
int ishtar_arm_vars_init(int *fx_id)
{
    ishtar_arm_init_arg = *fx_id;
    ishtar_arm_vars_register();
    return 0;
}

/* 0x00b02f70 */
int ishtar_arm_vars_release(void)
{
    ishtar_arm_vars_unregister();
    return 0;
}

/* 0x00b02f90 */
int ishtar_arm_get_init_arg(void)
{
    return ishtar_arm_init_arg;
}

/* 0x00b02fa0 */
int ishtar_arm_find_node_pos(float *out, unsigned int *node)
{
    do {
        ulMatrixPush();
        anim_apply_node(node);
        if (node[0] & 0x1000000) {
            ulMatrixGetTranslation((float *)ulMatrixCur, out);
            ulMatrixPop();
            return 1;
        }
        if (!(node[0] & 0x10) && ishtar_arm_find_node_pos(out, (unsigned int *)node[11])) {
            ulMatrixPop();
            return 1;
        }
        ulMatrixPop();
        node = (unsigned int *)node[12];
    } while (node);
    return 0;
}

/* 0x00b03058 */
void ishtar_arm_tip_pos(void *obj, IshtarArmWork *w, float *out)
{
    ModelPart *gfx = &w->body;

    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = w->arg.pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    ishtar_arm_rotate_yxz(w->arg.ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    anim_start((void *)gfx->clip, gfx->frame);
    ishtar_arm_find_node_pos(out, (unsigned int *)gfx->root);
    ulMatrixPop();
}

/* steer(cast) */
/* 0x00b030f0 */
void ishtar_arm_init(void *obj, IshtarArmWork *w, int flag, int *kind)
{
    IshtarArmVars *typ = &ishtar_arm_types[kind[0]];
    ModelPart *mdl = &w->body;
    int *prm;
    ModelPart *gfx;
    Vec3 p;
    Vec3 pl;
    float frm;
    int n;
    int i;

    w->type = typ;
    lockon_obj_init(&w->lock, obj, &w->core, 0, ishtar_arm_lock_name, enemy_task_get_status_slot(obj), typ->p.hp, typ->p.same_lock, 0);
    lockon_entry(&w->lock);
    enemy_task_clear_flags(obj, 64);
    prm = (int *)kind[1];
    w->life = prm[5];
    w->state = 0;
    w->frames = 0;
    w->arg = *(IshtarArmArg *)prm;
    dc_object_init(mdl, *((struct DcNode **)pnb_get_entry(*(void **)obj, 49)), *((struct DcClip **)pnb_get_entry(*(void **)obj, !((int *)kind[1])[2] ? 50 : 51)), 0x1000);
    ishtar_arm_tip_pos(obj, w, (float *)&p);
    enemy_task_set_pos(obj, &p);
    enemy_task_set_rot(obj, (int *)((int *)kind[1])[1]);
    enemy_task_set_scl(obj, &ishtar_arm_start_scl);
    mdl->speed *= typ->p.motion_spd;
    gfx = enemy_task_gfx_ptr(obj);
    gfx->speed *= typ->p.motion_spd_shot;
    player_get_frame_pos(&pl);
    frm = typ->p.hist_sec * FPS;
    n = 1.0f <= frm ? (int)frm : 1;
    w->trail_at = 0;
    w->trail_len = n;
    w->trail = yuki_alloc(n * 12);
    for (i = 0; i < w->trail_len; i++) {
        Vec3 *src = (Vec3 *)((int *)kind[1])[0];
        Vec3 *d_ = &w->trail[i];

        d_->x = src->x - pl.x;
        d_->y = src->y - pl.y;
        d_->z = src->z - pl.z;
    }
    w->ray_started = 0;
    v3_set(&w->ray_scl, typ->p.attack_ray_size, typ->p.attack_ray_size, typ->p.attack_ray_size);
    do { w->backfire_scl = 0; } while (0);
    w->fired = 0;
    w->tail_roll = 0;
}

/* 0x00b03378 */
void ishtar_arm_release(void *obj, IshtarArmWork *w)
{
    dc_object_release(&w->body);
    yuki_free(w->trail);
    w->trail = 0;
    lockon_remove(&w->lock);
}

/* 0x00b033b0 */
void ishtar_arm_fire_shot(void *obj, IshtarArmWork *w)
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
    e_shot_fire(obj, &w->muzzle, &dir, w->arg.level);
}

/* 0x00b034a8 */
int ishtar_arm_collect_mounts(int *pos_a, int *pos_b, unsigned int *node, int cnt)
{
    do {
        ulMatrixPush();
        anim_apply_node(node);
        if (node[0] & 0x1000000) {
            ulMatrixGetTranslation((float *)ulMatrixCur, (float *)pos_a);
            if (++cnt >= 2) {
                ulMatrixPop();
                return cnt;
            }
        }
        if (node[0] & 0x2000000) {
            ulMatrixGetTranslation((float *)ulMatrixCur, (float *)pos_b);
            if (++cnt >= 2) {
                ulMatrixPop();
                return cnt;
            }
        }
        if (!(node[0] & 0x10)) {
            cnt = ishtar_arm_collect_mounts(pos_a, pos_b, (unsigned int *)node[11], cnt);
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

/* 0x00b035e0 */
void ishtar_arm_pose_model(void *obj, IshtarArmWork *w)
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
    ishtar_arm_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
    anim_start((void *)gfx->clip, gfx->frame);
    ishtar_arm_collect_mounts((int *)&w->core, (int *)&w->muzzle, (unsigned int *)gfx->root, 0);
    ulMatrixPop();
}

/* 0x00b036c0 */
void ishtar_arm_shatter(void *obj, IshtarArmWork *w)
{
    SpawnDesc desc;

    nj_fill8(&desc, 0, sizeof desc);
    desc.type = 48;
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

/* 0x00b03780 */
int ishtar_arm_next_state(void *obj, IshtarArmWork *w, int n, int state)
{
    if (++w->frames > n) {
        w->state = state;
        w->frames = 1;
        enemy_task_get_rot(obj, w->ang);
        return 1;
    }
    return 0;
}

/* 0x00b037c8 */
void ishtar_arm_follow(void *obj, IshtarArmWork *w)
{
    Vec3 *pos = w->arg.pos;
    Vec3 *buf = w->trail + w->trail_at;
    IshtarArmVars *typ = w->type;
    Vec3 goal;
    Vec3 cur;
    Vec3 pl;
    Vec3 tip;
    Vec3 dtip;

    enemy_task_get_pos(obj, &cur);
    player_get_frame_pos(&pl);
    goal.x = pos->x - pl.x;
    goal.y = pos->y - pl.y;
    goal.z = pos->z - pl.z;
    goal.x = buf->x * typ->p.hokan_rate + goal.x * (1.0f - typ->p.hokan_rate);
    goal.y = buf->y * typ->p.hokan_rate + goal.y * (1.0f - typ->p.hokan_rate);
    goal.z = buf->z * typ->p.hokan_rate + goal.z * (1.0f - typ->p.hokan_rate);
    goal.x += pl.x;
    goal.y += pl.y;
    goal.z += pl.z;
    ishtar_arm_tip_pos(obj, w, (float *)&tip);
    dtip.x = tip.x - pos->x;
    dtip.y = tip.y - pos->y;
    dtip.z = tip.z - pos->z;
    goal.x += dtip.x;
    goal.y += dtip.y;
    goal.z += dtip.z;
    enemy_task_set_pos(obj, &goal);
    buf->x = pos->x - pl.x;
    buf->y = pos->y - pl.y;
    buf->z = pos->z - pl.z;
    w->trail_at = (w->trail_at + 1) % w->trail_len;
    dtip.x = goal.x - cur.x;
    dtip.y = goal.y - cur.y;
    dtip.z = goal.z - cur.z;
    enemy_task_set_param_vec(obj, &dtip);
    enemy_task_set_rot(obj, w->arg.ang);
    w->tail_roll += w->type->p.tail_roll_spd / FPS;
}

/* 0x00b03a18 */
int ishtar_arm_held_state_ray(void *obj, IshtarArmWork *w)
{
    ModelPart *gfx;
    AttackRayArg ray;

    dc_object_advance_time(&w->body);
    ishtar_arm_follow(obj, w);
    if (w->arg.delay > 0 && --w->arg.delay <= 0) {
        gfx = enemy_task_gfx_ptr(obj);
        w->state = 1;
        ray.pos = &w->muzzle;
        ray.ang = enemy_task_rot_ptr(obj);
        ray.scale = enemy_task_scl_ptr(obj);
        ray.dur = gfx->frames / gfx->speed;
        attack_ray_create(obj, &ray);
    }
    return 0;
}

/* 0x00b03ad0 */
int ishtar_arm_held_state_return(void *obj, IshtarArmWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    int done = 0;
    AttackRayArg ray;
    Vec3 p;
    int ang[3];
    int aim[3];
    float rate;

    ishtar_arm_follow(obj, w);
    dc_object_advance_time(gfx);
    if (w->ray_started == 0 && gfx->frame > 15.0f) {
        w->ray_started = 1;
        ray.pos = &w->muzzle;
        ray.ang = enemy_task_rot_ptr(obj);
        ray.scale = &w->ray_scl;
        ray.dur = 16.0f / gfx->speed;
        attack_ray_create(obj, &ray);
    }
    if (w->fired == 0 && gfx->frame > 31.0f) {
        done = 1;
        w->fired = 1;
    }
    if (w->fired == 0)
        rate = gfx->frame / 31.0f;
    else
        rate = (gfx->frames - gfx->frame) / (gfx->frames - 31.0f);
    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    e_shot_initial_angles(aim, &p, 0, w->arg.level);
    yuki_lerp3(ang, ang, aim, rate);
    enemy_task_set_rot(obj, ang);
    if (fabsf(gfx->frame - gfx->frames) < FLT_EPSILON)
        w->state = 0;
    return done;
}

/* 0x00b03ca8 */
int ishtar_arm_state_release(void *obj, IshtarArmWork *w)
{
    IshtarArmVars *typ;
    Vec3 pos;
    Vec3 tip;
    Vec3 v;
    Vec3 dv;
    float len;
    float step;
    ModelPart *gfx;

    typ = w->type;
    ishtar_arm_follow(obj, w);
    ishtar_arm_tip_pos(obj, w, (float *)&tip);
    enemy_task_get_pos(obj, &pos);
    dv.x = pos.x - tip.x;
    dv.y = pos.y - tip.y;
    dv.z = pos.z - tip.z;
    enemy_task_get_param_vec(obj, &v);
    v.x += dv.x;
    v.y += dv.y;
    v.z += dv.z;
    len = vec3_length(&v);
    w->speed = len;
    if (fabsf(len) < FLT_EPSILON) {
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
    } else {
        vec3_normalize(&v, &v);
        step = typ->p.blow_spd * FRAME_SEC;
        v.x *= step;
        v.y *= step;
        v.z *= step;
    }
    enemy_task_set_param_vec(obj, &v);
    w->spin[0] = RAND_FLOAT_A() * typ->p.blow_ang_spd / FPS;
    w->spin[1] = RAND_FLOAT_A() * typ->p.blow_ang_spd / FPS;
    w->spin[2] = RAND_FLOAT_A() * typ->p.blow_ang_spd / FPS;
    gfx = enemy_task_gfx_ptr(obj);
    w->frames = 0;
    gfx->frame = 0.0f;
    ishtar_arm_next_state(obj, w, 0, 3);
    return 0;
}

/* 0x00b03ef8 */
int ishtar_arm_state_fly(void *obj, IshtarArmWork *w)
{
    IshtarArmVars *typ = w->type;
    int n = typ->p.blow_sec * FPS + 1.0f;
    float rate = (float)w->frames / n;
    float prev = (float)(w->frames - 1) / n;
    float rem;
    float spd;
    float acc;
    float step;
    Vec3 p;
    Vec3 v;
    BackFireLink lnk;
    float sin_rate, cos_rate, sin_prev, cos_prev;

    enemy_task_get_param_vec(obj, &v);
    spd = w->speed * FPS;
    if (!(fabsf(spd) < FLT_EPSILON) && !(fabsf(typ->p.blow_sec) < FLT_EPSILON)) {
        acc = spd * (typ->p.blow_spd_rate - 1.0f) / typ->p.blow_sec;
        spd += acc * typ->p.blow_sec * prev;
        step = spd * FRAME_SEC + acc * 0.5f * FRAME_SEC * FRAME_SEC;
        v.x *= step;
        v.y *= step;
        v.z *= step;
    }
    ulSinCos(rate * ANG_QUARTER, &sin_rate, &cos_rate);
    ulSinCos(prev * ANG_QUARTER, &sin_prev, &cos_prev);
    v.y -= typ->p.blow_down_dis * (sin_rate - sin_prev);
    enemy_task_get_pos(obj, &p);
    p.x += v.x;
    p.y += v.y;
    p.z += v.z;
    enemy_task_set_pos(obj, &p);
    ulMatrixPush();
    matrix_unit_set(0);
    rem = 1.0f - rate;
    vu0_matrix_rotate_y(ulMatrixCur, w->spin[1] * rem);
    vu0_matrix_rotate_x(ulMatrixCur, w->spin[0] * rem);
    vu0_matrix_rotate_z(ulMatrixCur, w->spin[2] * rem);
    ishtar_arm_rotate_yxz(w->ang);
    model_camera_angles(w->ang);
    ulMatrixPop();
    enemy_task_set_rot(obj, w->ang);
    w->tail_roll += w->type->p.tail_roll_spd / FPS * rem;
    if (ishtar_arm_next_state(obj, w, n, 4)) {
        heap_bzero((int *)&lnk, sizeof lnk);
        lnk.gfx = enemy_task_gfx_ptr(obj);
        lnk.pos = enemy_task_pos_ptr(obj);
        lnk.ang = enemy_task_rot_ptr(obj);
        lnk.obj_scale = enemy_task_scl_ptr(obj);
        lnk.scale = (float *)&w->backfire_scl;
        lnk.flags = 0x4000000;
        lnk.use_pose = 1;
        en_backfire_create(obj, &lnk, 0);
        enemy_task_get_param_vec(obj, &v);
        w->speed *= typ->p.blow_spd_rate;
        {
            float scl = typ->p.blow_spd_rate;

            v.x *= scl;
            v.y *= scl;
            v.z *= scl;
        }
        enemy_task_set_param_vec(obj, &v);
    }
    return 0;
}

/* 0x00b042c8 */
int ishtar_arm_state_aim(void *obj, IshtarArmWork *w)
{
    IshtarArmVars *typ = w->type;
    int n = typ->p.break_sec * FPS + 1.0f;
    float rate = (float)w->frames / n;
    float prev = (float)(w->frames - 1) / n;
    float spd;
    float acc;
    float step;
    float ease;
    Vec3 p;
    Vec3 start;
    Vec3 v;
    int ang[3];
    Vec3 ofs;
    int aim[3];
    int old_ang[3];
    float sc[3][2];

    enemy_task_get_pos(obj, &start);
    enemy_task_get_param_vec(obj, &v);
    spd = w->speed * FPS;
    if (!(fabsf(spd) < FLT_EPSILON) && !(fabsf(typ->p.break_sec) < FLT_EPSILON)) {
        acc = -spd / typ->p.break_sec;
        spd += acc * typ->p.break_sec * prev;
        step = spd * FRAME_SEC + acc * 0.5f * FRAME_SEC * FRAME_SEC;
        v.x *= step;
        v.y *= step;
        v.z *= step;
        v3_set(&ofs, 0.0f, 0.0f, -typ->p.break_accel_dis * rate);
        enemy_task_get_rot(obj, ang);
        ROT_X(ang[0], sc[0], ofs.y, ofs.z);
        ROT_Y(ang[1], sc[1], ofs.x, ofs.z);
        v.x += ofs.x;
        v.y += ofs.y;
        v.z += ofs.z;
        enemy_task_get_pos(obj, &p);
        p.x += v.x;
        p.y += v.y;
        p.z += v.z;
        enemy_task_set_pos(obj, &p);
    }
    ulSinCos(rate * ANG_HALF, &sc[2][0], &sc[2][1]);
    w->backfire_scl = (rate + sc[2][0] * typ->p.break_backfire_scl) * typ->p.break_backfire_scl2;
    w->tail_roll -= w->type->p.tail_roll_spd_tackle / FPS * rate;
    ease = safe_powf(rate, typ->p.break_ang_pow);
    player_get_pos(&ofs);
    v.x = ofs.x - p.x;
    v.y = ofs.y - p.y;
    v.z = ofs.z - p.z;
    do {
        aim[1] = (int)RAD_TO_ANG(atan2f(-v.x, -v.z));
        aim[0] = (int)RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
    } while (0);
    aim[2] = 0;
    enemy_task_get_rot(obj, old_ang);
    yuki_lerp3(aim, old_ang, aim, ease);
    enemy_task_set_rot(obj, aim);
    if (ishtar_arm_next_state(obj, w, n, 5)) {
        v.x = p.x - start.x;
        v.y = p.y - start.y;
        v.z = p.z - start.z;
        w->speed = vec3_length(&v);
        SoundRhythmSe_Enemy(ishtar_arm_init_arg, 2);
    }
    return 0;
}

/* 0x00b04750 */
int ishtar_arm_state_dive(void *obj, IshtarArmWork *w)
{
    IshtarArmVars *typ = w->type;
    Vec3 p;
    Vec3 v;
    int ang[3];
    int nang[3];
    float sc[2][2];
    float x, y, z;
    float spd;
    float step;
    ModelPart *gfx;

    x = 0.0f;
    y = 0.0f;
    z = -1.0f;
    enemy_task_get_rot(obj, ang);
    ROT_X(ang[0], sc[0], y, z);
    ROT_Y(ang[1], sc[1], x, z);
    v3_set(&v, x, y, z);
    spd = w->speed * FPS + (w->frames - 1) / FPS * typ->p.accel;
    if (spd > typ->p.accel_spd_max)
        spd = typ->p.accel_spd_max;
    step = spd * FRAME_SEC + typ->p.accel * 0.5f * FRAME_SEC * FRAME_SEC;
    v.x *= step;
    v.y *= step;
    v.z *= step;
    enemy_task_get_pos(obj, &p);
    p.x += v.x;
    p.y += v.y;
    p.z += v.z;
    enemy_task_set_pos(obj, &p);
    w->tail_roll -= w->type->p.tail_roll_spd_tackle / FPS;
    enemy_task_get_rot(obj, nang);
    nang[2] += typ->p.tackle_roll_spd / FPS;
    enemy_task_set_rot(obj, nang);
    gfx = enemy_task_gfx_ptr(obj);
    gfx->frame = gfx->frame + gfx->speed < 31.0f ? gfx->frame + gfx->speed : 31.0f;
    if (player_sphere_test(&p, typ->p.crash_r) == 1) {
        player_get_pos(&p);
        enemy_task_set_pos(obj, &p);
        player_status_add_damage(typ->p.crash_damage);
        enemy_task_set_end_event(obj, 5);
        ishtar_arm_shatter(obj, w);
    }
    if (!yuki_camera_relative(&p, typ->p.tackle_clip_ang, ((ModelPart *)enemy_task_gfx_ptr(obj))->radius))
        ishtar_arm_shatter(obj, w);
    w->frames++;
    return 0;
}

/* 0x00b04a30 */
void ishtar_arm_update(void *obj, IshtarArmWork *w)
{
    int (*fn)(void *obj, IshtarArmWork *w) = ishtar_arm_states[w->state];
    int fire = 0;

    if (fn) {
        fire = fn(obj, w);
        ishtar_arm_pose_model(obj, w);
    }
    if (fire)
        ishtar_arm_fire_shot(obj, w);
}

/* 0x00b04ac0 */
void ishtar_arm_exec_appear(void *obj, IshtarArmWork *w)
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
        ishtar_arm_update(obj, w);
        if (w->life > 0 && w->state < 2 && --w->life <= 0) {
            w->lock.flags &= ~0x100;
            enemy_task_set_state(obj, 2);
        }
        enemy_task_appear_scale(obj, w);
        break;
    }
}

/* 0x00b04ba0 */
void ishtar_arm_exec_normal(void *obj, IshtarArmWork *w)
{
    if (enemy_task_get_phase(obj) != 1)
        return;
    ishtar_arm_update(obj, w);
    if (w->life > 0 && w->state < 2 && --w->life <= 0) {
        w->lock.flags &= ~0x100;
        enemy_task_set_state(obj, 2);
    }
}

/* 0x00b04c28 */
void ishtar_arm_exec_leave(void *obj, IshtarArmWork *w)
{
    if (enemy_task_get_phase(obj) != 1) {
        w->lock.flags &= ~0x100;
        enemy_task_vanish_scale(obj, w);
        return;
    }
    ishtar_arm_update(obj, w);
    enemy_task_vanish_scale(obj, w);
}

/* 0x00b04ca0 */
void ishtar_arm_exec_destroyed(void *obj, IshtarArmWork *w)
{
    if (enemy_task_get_phase(obj) == 0)
        ishtar_arm_shatter(obj, w);
}

/* 0x00b04cf0 */
void ishtar_arm_exec(void *obj, IshtarArmWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (ishtar_arm_exec_table[i])
        ishtar_arm_exec_table[i](obj, w);
}

/* 0x00b04d48 */
void ishtar_arm_pre_exec(void *obj, IshtarArmWork *w)
{
    Vec3 p, pl_vel;

    if (enemy_status_get_ticks() > 0) {
        if (w->state < 2) {
            enemy_task_get_pos(obj, &p);
            player_get_frame_vel(&pl_vel);
            p.x -= pl_vel.x;
            p.y -= pl_vel.y;
            p.z -= pl_vel.z;
            enemy_task_set_pos(obj, &p);
        } else {
            Vec3 dvel;

            player_get_frame_vel(&dvel);
            v3_add(&w->pos_buf, &dvel);
        }
    }
}

/* steer(cast) */
/* 0x00b04e20 */
void ishtar_arm_post_exec(void *obj, IshtarArmWork *w)
{
    LockObj *lock = &w->lock;
    Vec3 p;

    if (!enemy_status_get_ticks())
        ishtar_arm_pose_model(obj, w);
    if (lock->flags & LOCK_HIT)
        v3_copy(&w->hitv, &lock->imp.v);
    if (lock->life <= 0 && ((unsigned int)enemy_task_get_pstate(obj)) < 2) {
        int mult = lockon_hits_max_shot_cnt(lock);
        IshtarArmVars *typ = w->type;

        enemy_task_get_pos(obj, &p);
        enemy_status_score_popup(enemy_task_get_status_slot(obj), &p, typ->p.score, mult);
        enemy_task_set_state(obj, 3);
    }
}

/* 0x00b04f00 */
void ishtar_arm_draw_nodes(unsigned int *node, int angle)
{
    do {
        ulMatrixPush();
        anim_apply_node(node);
        if (node[0] & 0x10000000)
            vu0_matrix_rotate_y(ulMatrixCur, angle);
        if (node[1])
            nj_cnk_vu1_draw((void *)node[1]);
        if (!(node[0] & 0x10))
            ishtar_arm_draw_nodes((unsigned int *)node[11], angle);
        ulMatrixPop();
        node = (unsigned int *)node[12];
    } while (node);
}

/* steer(cast) */
/* 0x00b04fa8 */
void ishtar_arm_disp_body(void *obj, IshtarArmWork *w)
{
    Vec3 p;
    int ang[4];
    Vec3 scl;
    float rgba[4];
    ModelPart *gfx;
    float unit, eps;
    int blend;

    gfx = enemy_task_gfx_ptr(obj);
    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    enemy_task_get_scl(obj, &scl);
    if (view_sphere_visible(&p, gfx->radius)) {
        unit = 1.0f;
        eps = FLT_EPSILON;
        blend = 0;
        if (!(fabsf(scl.x - unit) < eps)) {
            blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
            draw_push_strip_flags();
            draw_push_override_color();
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 0x800);
            heap_bzero(rgba, 16);
            rgba[0] = scl.x - unit;
            draw_set_override_color(rgba);
        }
        ulGxSetTextureList(((void **)obj)[1]);
        do {
            ulMatrixPush();
        } while (0);
        vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
        ishtar_arm_rotate_yxz(ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        anim_start((void *)gfx->clip, gfx->frame);
        ishtar_arm_draw_nodes((unsigned int *)gfx->root, w->tail_roll);
        ulMatrixPop();
        if (!(fabsf(scl.x - unit) < eps)) {
            draw_pop_strip_flags();
            draw_pop_override_color();
            draw_set_override_flags(blend);
        }
    }
}

/* 0x00b05150 */
void ishtar_arm_disp(void *obj, IshtarArmWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (ishtar_arm_disp_table[i])
        ishtar_arm_disp_table[i](obj, w);
    if (w->state == 6)
        FINISH_IF(obj);
}

/* 0x00b051c8 */
int ishtar_arm_create(void *parent, char *flag, int *prm)
{
    int kind[3];
    ObjDesc desc;
    ObjFuncs funcs = ishtar_arm_funcs;
    IshtarArmVars *typ = ishtar_arm_types;
    Task *ptask;

    *flag = ishtar_arm_init_arg;
    kind[1] = (int)prm;
    kind[2] = (int)parent;
    kind[0] = 0;
    nj_fill8(&desc, 0, sizeof desc);
    desc.name = ishtar_arm_name;
    desc.funcs = &funcs;
    desc.arg = flag;
    desc.kind = kind;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->p.hp;
    desc.same_lock = typ->p.same_lock;
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = 46;
    desc.model_no2 = 47;
    desc.score = typ->p.score;
    desc.hp_reduce = typ->p.hp_reduce;
    desc.crash_r = typ->p.crash_r;
    desc.crash_damage = typ->p.crash_damage;
    desc.time = prm[5];
    desc.size = 328;
    ptask = TASK_OF(parent)->parent;
    return enemy_task_task_create(ptask ? ptask + 1 : 0, &desc);
}

/* 0x00b05330 */
void ishtar_arm_detach(void *obj)
{
    IshtarArmWork *w = enemy_task_work_ptr(obj);

    w->state = 2;
    v3_copy(&w->pos_buf, w->arg.pos);
    w->arg.pos = &w->pos_buf;
    i3_copy(w->ang_buf, w->arg.ang);
    w->arg.ang = w->ang_buf;
}

/* 0x00b053a0 */
void ishtar_arm_held_update(void *obj)
{
    int *sub = enemy_task_work_ptr(obj);
    int (*fn)(void *obj, IshtarArmWork *w) = ishtar_arm_held_states[sub[51]];
    int fire = 0;

    if (fn)
        fire = fn(obj, sub);
    ishtar_arm_pose_model(obj, sub);
    if (fire)
        ishtar_arm_fire_shot(obj, sub);
}

/* 0x00b05438 */
void ishtar_arm_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
