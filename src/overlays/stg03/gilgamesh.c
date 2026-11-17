/*
 * Gilgamesh, Area 3 enemy (three kinds): rides a stage sequence path relative to the player,
 * spinning; stops, opens six arms and fires six attack rays, then leaves along the path's end.
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
#include "pass.h"
#include "player.h"
#include "pnb.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "sound_midise.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "e_shot.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"

typedef struct RayWork {
    void *models;
    void *textures;             /* 0x04 */
    int timer;                  /* 0x08: frames */
    ModelPart body;             /* 0x0c */
    AttackRayArg arg;           /* 0x68 */
} RayWork;

/* Named by the 'gilgamesh' VarDefs gilgamesh_vardefs. */
typedef struct GilgameshParams {
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
    float appear_dis;           /* 0x40 */
    float appear_sec;           /* 0x44 */
    float break_sec;            /* 0x48 */
    float stop_sec;             /* 0x4c */
    float accel_sec;            /* 0x50 */
    float disappear_dis;        /* 0x54 */
    float disappear_sec;        /* 0x58 */
    int swing_ang_spd;          /* 0x5c */
    float swing_dis;            /* 0x60 */
    float roll_spd;             /* 0x64 */
    float cd_roll_spd;          /* 0x68 */
    float cover_roll_sec;       /* 0x6c */
    int cover_roll_ang;         /* 0x70 */
    float cover_roll_pow;       /* 0x74 */
    int cover_roll_mergin;      /* 0x78 */
    float cover_roll_mergin_rate; /* 0x7c */
    int damage_swing_ang;       /* 0x80 */
    int damage_swing_spd;       /* 0x84 */
    float damage_swing_sec;     /* 0x88 */
} GilgameshParams;

typedef struct GilgameshVars {
    char *name;
    char *label;                /* 0x04 */
    GilgameshParams p;          /* 0x08 */
} GilgameshVars;

typedef struct GilgameshWork {
    GilgameshVars *type;
    int frames;                 /* 0x04: frames in the state */
    Vec3 hitv;                  /* 0x08: the lock-on record's vector when hit */
    int state;                  /* 0x14 */
    int life;                   /* 0x18: frames left; 0 = no limit */
    Vec3 core;                  /* 0x1c: lock-on point */
    Vec3 tip[6];                /* 0x28: ray origins */
    int tip_ang[6][3];          /* 0x70 */
    LockObj lock;               /* 0xb8 */
    ModelPart cover;            /* 0xf8: the wire cover */
    int path;                   /* 0x154 */
    Vec3 base;                  /* 0x158: start position, player relative */
    int base_ang[3];            /* 0x164 */
    Vec3 cur;                   /* 0x170: position on the path */
    Vec3 prev;                  /* 0x17c */
    Vec3 from;                  /* 0x188: position at the state's start */
    int tilt[3];                /* 0x194 */
    Vec3 dir;                   /* 0x1a0 */
    float speed;                /* 0x1ac */
    float prog;                 /* 0x1b0: 0 to 1 along the path */
    float prog_from;            /* 0x1b4 */
    int stops;                  /* 0x1b8: stops left that fire rays */
    int firing;                 /* 0x1bc */
    int rays;                   /* 0x1c0: rays created this stop */
    Vec3 swing;                 /* 0x1c4 */
    int swing_ang;              /* 0x1d0 */
    int roll;                   /* 0x1d4 */
    int cover_roll;             /* 0x1d8 */
    int cover_roll_base;        /* 0x1dc */
    int cover_count;            /* 0x1e0: frames into the cover roll */
    int damage_swing;           /* 0x1e4 */
    int damage_yaw;             /* 0x1e8 */
    int damage_swing_max;       /* 0x1ec */
    int damage_count;           /* 0x1f0: frames of damage swing left */
} GilgameshWork;

extern int gilgamesh_init_arg;
extern void (*gilgamesh_exec_table[])(void *o, GilgameshWork *w);
extern void (*gilgamesh_disp_table[])(void *o, GilgameshWork *w);
void gilgamesh_vars_register(void);
void gilgamesh_vars_unregister(void);
int gilgamesh_vars_init(int *fx_id);
int gilgamesh_vars_release(void);
void gilgamesh_exec(void *obj, GilgameshWork *w);
void gilgamesh_disp(void *obj, GilgameshWork *w);
void gilgamesh_rotate_yxz(int *ang);

extern GilgameshVars gilgamesh_types[3];
extern VarDef gilgamesh_vardef;
extern VarDef gilgamesh_vardefs[35];
extern struct TaskDesc ray_desc;
void gilgamesh_update(void *obj, GilgameshWork *w);
void gilgamesh_walk_nodes(int *out_a, int *out_b, int *out_c, int naddr);
extern int (*gilgamesh_states[])(void *o, GilgameshWork *w);
void gilgamesh_place(void *obj, GilgameshWork *w);
void gilgamesh_fire_shot(void *obj, GilgameshWork *w);
void gilgamesh_motion(void *obj, GilgameshWork *w);
int gilgamesh_next_state(void *obj, GilgameshWork *w, int n, int state);
void gilgamesh_damage_swing(void *obj, GilgameshWork *w);
extern ObjFuncs gilgamesh_funcs;
extern char gilgamesh_name[];
extern int gilgamesh_path_sym;
int gilgamesh_path_frames(float spd, GilgameshVars *types, int frm, int path, int *out);
extern char gilgamesh_lock_name[];
extern Vec3 gilgamesh_start_scl;

/* 0x00b005d0 */
void gilgamesh_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *prm;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&gilgamesh_vardef);
    for (i = 0; i < 3; i++) {
        def = gilgamesh_vardefs;
        prm = (int *)&gilgamesh_types[i].p;
        gilgamesh_vardef.name = gilgamesh_types[i].name;
        gilgamesh_vardef.fmt = gilgamesh_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&gilgamesh_vardef);
        for (j = 0; j < 35; j++) {
            def->ptr = prm;
            prm++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)gilgamesh_vardefs);
    }
}

/* 0x00b006e8 */
void gilgamesh_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b006f0 */
int gilgamesh_vars_init(int *fx_id)
{
    gilgamesh_init_arg = *fx_id;
    gilgamesh_vars_register();
    return 0;
}

/* 0x00b00718 */
int gilgamesh_vars_release(void)
{
    gilgamesh_vars_unregister();
    return 0;
}

/* 0x00b00738 */
int gilgamesh_marker_draw(int obj, void *place)
{
    int *ent;

    ulGxSetTextureList(stage_textures);
    ent = pnb_get_entry(stage_models, 18);
    return obj_draw_at(obj, place, (void *)*ent);
}

/* 0x00b00790 */
int gilgamesh_get_init_arg(void)
{
    return gilgamesh_init_arg;
}

/* steer(cast) */
/* 0x00b007a0 */
void ray_init(RayWork *w, AttackRayArg *prm)
{
    ModelPart *gfx = &w->body;
    int *root;

    root = ((int *)pnb_get_entry(w->models, 23));
    dc_object_init(gfx, *root, *((struct DcClip **)pnb_get_entry(w->models, 24)), 0);
    dc_object_rebuild(gfx);
    w->body.radius = 35.0f;
    w->arg = *prm;
    w->timer = 0;
}

/* 0x00b00848 */
void ray_release(RayWork *w)
{
    dc_object_release(&w->body);
}

/* 0x00b00850 */
void ray_exec(RayWork *w)
{
    ModelPart *gfx = &w->body;

    if (++w->timer > w->arg.dur)
        FINISH_IF(w);
    else
        gfx->frame = gfx->frames * w->timer / w->arg.dur;
}

/* 0x00b008b0 */
void ray_disp(RayWork *w)
{
    AttackRayArg *prm = &w->arg;
    ModelPart *gfx = &w->body;
    int dl;

    dl = RenderSetCurrentDisplayList(6);
    ulGxSetTextureList(w->textures);
    ulMatrixPush();
    do {
        Vec3 *p_;

        do { p_ = w->arg.pos; } while (0);

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    ulMatrixUnitRotation((float *)ulMatrixCur);
    do {
        Vec3 *s_ = prm->scale;

        vu0_matrix_scale(ulMatrixCur, s_->x, s_->y, s_->z);
    } while (0);
    anim_start((void *)gfx->clip, gfx->frame);
    dc_object_draw(gfx);
    ulMatrixPop();
    RenderSetCurrentDisplayList(dl);
}

/* 0x00b00970 */
void *ray_create(void *parent, void *prm)
{
    return task_create_inherit_arg(parent, &ray_desc, 120, prm);
}

/* steer(cast) */
/* 0x00b00998 */
void gilgamesh_walk_nodes(int *out_a, int *out_b, int *out_c, int naddr)
{
    float **pa = (float **)out_a;
    float **pb = (float **)out_b;
    int **pc = (int **)out_c;
    unsigned int *node = (unsigned int *)naddr;

    do {
        ulMatrixPush();
        anim_apply_node(node);
        if (node[0] & 0x4000000) {
            ulMatrixGetTranslation((float *)ulMatrixCur, *pa);
            *pa += 3;
        }
        if (node[0] & 0x1000000) {
            ulMatrixGetTranslation((float *)ulMatrixCur, *pb);
            ((void (*)(int *))model_camera_angles)(*pc);
            *pb += 3;
            *pc += 3;
        }
        if (!(node[0] & 0x10))
            gilgamesh_walk_nodes(out_a, out_b, out_c, node[11]);
        ulMatrixPop();
        node = (unsigned int *)node[12];
    } while (node);
}

/* 0x00b00aa0 */
void gilgamesh_pose_nodes(int out_a, int out_b, int out_c, ModelPart *gfx)
{
    int cur[3];

    cur[0] = out_a;
    cur[1] = out_b;
    cur[2] = out_c;
    anim_start((void *)gfx->clip, gfx->frame);
    gilgamesh_walk_nodes(&cur[0], &cur[1], &cur[2], gfx->root);
}

/* 0x00b00af0 */
void gilgamesh_place(void *obj, GilgameshWork *w)
{
    Vec3 p;
    Vec3 org;
    int ang[3];
    Vec3 dorg;

    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &w->swing;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    player_get_frame_pos(&org);
    do {
        Vec3 *p_ = &org;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    do {
        Vec3 *p_ = &w->base;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    gilgamesh_rotate_yxz(w->base_ang);
    do {
        Vec3 *p_ = &w->cur;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    ulMatrixGetTranslation((float *)ulMatrixCur, (float *)&p);
    ulMatrixPop();
    enemy_task_set_pos(obj, &p);
    v3_sub(&dorg, &org, &p);
    do {
        ang[1] = (int)RAD_TO_ANG(atan2f(-dorg.x, -dorg.z));
        ang[0] = (int)RAD_TO_ANG(atan2f(dorg.y, sqrtf(dorg.x * dorg.x + dorg.z * dorg.z)));
    } while (0);
    ang[2] = 0;
    ulMatrixPush();
    matrix_unit_set(0);
    gilgamesh_rotate_yxz(ang);
    gilgamesh_rotate_yxz(w->tilt);
    vu0_matrix_rotate_y(ulMatrixCur, w->damage_swing + w->damage_yaw);
    model_camera_angles(ang);
    ulMatrixPop();
    enemy_task_set_rot(obj, ang);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &p;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    gilgamesh_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_rotate_z(ulMatrixCur, -w->cover_roll);
    gilgamesh_pose_nodes((int)&w->core, (int)w->tip, (int)w->tip_ang, enemy_task_gfx_ptr(obj));
    ulMatrixPop();
}

/* 0x00b00d70 */
void gilgamesh_path_eval(void *out, int idx, float t)
{
    pass_table_push(stage_sequence);
    sequence_eval(idx, (pass_table_end(idx) - 1.0f) * t, 0, out, 0);
    pass_table_pop();
}

/* steer(cast) */
/* 0x00b00de0 */
void gilgamesh_init(void *obj, GilgameshWork *w, int place, int *kind)
{
    GilgameshVars *typ = &gilgamesh_types[kind[0]];
    ModelPart *gfx;
    Vec3 org;
    Vec3 pl_vel;
    Vec3 p;
    Vec3 v;
    int ang[3];
    float sc[3][2];
    float x, y, z;
    float spd;

    w->type = typ;
    lockon_obj_init(&w->lock, obj, &w->core, 0, gilgamesh_lock_name, enemy_task_get_status_slot(obj), typ->p.hp, typ->p.same_lock, 128);
    lockon_entry(&w->lock);
    enemy_task_clear_flags(obj, 64);
    w->life = kind[2];
    w->prog = 0;
    w->path = kind[1];
    w->state = 0;
    w->frames = 0;
    enemy_task_set_scl(obj, &gilgamesh_start_scl);
    dc_object_init(&w->cover, *((struct DcNode **)pnb_get_entry(*(void **)obj, 19)), *((struct DcClip **)pnb_get_entry(*(void **)obj, 22)), 0x500);
    dc_object_set_wire(&w->cover, model_pnb_get_wire(*(void **)obj, 19));
    gfx = enemy_task_gfx_ptr(obj);
    gfx->radius = w->cover.radius = 10.0f;
    enemy_task_get_rot(obj, ang);
    vec3_normalize(&v, (Vec3 *)&kind[3]);
    v3_copy(&w->dir, &v);
    do { Vec3 *p_ = &v; x = p_->x; y = p_->y; z = p_->z; } while (0);
    ROT_Z(ang[2], sc[0], x, y);
    ROT_X(ang[0], sc[1], y, z);
    ROT_Y(ang[1], sc[2], x, z);
    spd = typ->p.appear_dis;
    v3_set(&v, x * spd, y * spd, z * spd);
    enemy_task_get_pos(obj, &p);
    v3_sum(&w->base, &p, &v);
    player_get_frame_pos(&org);
    player_get_frame_vel(&pl_vel);
    v3_add(&w->base, &pl_vel);
    v3_sub(&w->base, &w->base, &org);
    enemy_task_get_rot(obj, w->base_ang);
    i3_set(w->tilt, 0, 0, 0);
    w->speed = vec3_length((Vec3 *)&kind[3]);
    v3_mul(&w->cur, &w->dir, -typ->p.appear_dis);
    v3_copy(&w->prev, &w->cur);
    v3_copy(&w->from, &w->cur);
    w->stops = enemy_task_get_param(obj, 4) + 0.5f;
    w->firing = 0;
    w->rays = 0;
    w->swing_ang = 0;
    w->damage_count = 0;
    w->damage_yaw = 0;
    w->damage_swing = 0;
    w->damage_swing_max = 0;
    w->roll = 0;
    w->cover_roll = w->cover_roll_base = 0;
    w->cover_count = 0;
}

/* 0x00b011d8 */
void gilgamesh_release(void *obj, GilgameshWork *w)
{
    dc_object_release(&w->cover);
    lockon_remove(&w->lock);
}

/* 0x00b01208 */
void gilgamesh_fire_shot(void *obj, GilgameshWork *w)
{
    Vec3 dir;
    float sc[2][2];
    float x, y, z;
    int i;
    int *ay = &w->tip_ang[0][1], *ax = &w->tip_ang[0][0];

    for (i = 0; i < 6; i++, ay += 3, ax += 3) {
        x = 0.0f;
        y = 0.0f;
        z = w->type->p.bullet_spd * (1.0f / FPS);
        ROT_X(*ax, sc[0], y, z);
        ROT_Y(*ay, sc[1], x, z);
        v3_set(&dir, x, y, z);
        e_shot_fire(obj, &w->tip[i], &dir, i + 9);
    }
}

/* 0x00b01368 */
void gilgamesh_damage_swing(void *obj, GilgameshWork *w)
{
    GilgameshVars *typ;
    float rate;
    float sc[2];
    int n;

    if (--w->damage_count <= 0) {
        w->damage_swing_max = w->damage_swing = w->damage_yaw = w->damage_count = 0;
    } else {
        typ = w->type;
        n = typ->p.damage_swing_sec * FPS + 1.0f;
        rate = (float)w->damage_count / n;
        ulSinCos((n - w->damage_count) * typ->p.damage_swing_spd / FPS, &sc[0], &sc[1]);
        w->damage_swing = typ->p.damage_swing_ang * sc[0] * rate;
        w->damage_yaw = w->damage_swing_max * rate;
    }
}

/* 0x00b01468 */
void gilgamesh_motion(void *obj, GilgameshWork *w)
{
    GilgameshVars *typ = w->type;
    int ang[3];
    Vec3 dv;
    float sc[3][2];
    float x, y, z;
    float len;
    float rate;
    float hold;
    int n;

    w->swing_ang += typ->p.swing_ang_spd / FPS;
    x = typ->p.swing_dis;
    y = 0.0f;
    z = 0.0f;
    enemy_task_get_rot(obj, ang);
    ROT_Z(w->swing_ang, sc[0], x, y);
    ROT_X(ang[0], sc[1], y, z);
    ROT_Y(ang[1], sc[2], x, z);
    v3_set(&w->swing, x, y, z);
    v3_sub(&dv, &w->cur, &w->prev);
    len = vec3_length(&dv);
    n = typ->p.cover_roll_sec * FPS + 1.0f;
    rate = (float)++w->cover_count / n;
    w->tilt[2] += (int)DEG_TO_ANG(len * typ->p.roll_spd);
    w->roll += typ->p.cd_roll_spd / FPS;
    if (w->cover_count >= n) {
        w->cover_count = 0;
        w->cover_roll = w->cover_roll_base = w->cover_roll_base + typ->p.cover_roll_ang;
    } else {
        hold = typ->p.cover_roll_mergin_rate;
        if (fabsf(hold) < FLT_EPSILON) {
            w->cover_roll = w->cover_roll_base + typ->p.cover_roll_ang * safe_powf(rate, typ->p.cover_roll_pow);
        } else if (!(fabsf(hold - 1.0f) < FLT_EPSILON)) {
            if (rate < 1.0f - hold)
                w->cover_roll = w->cover_roll_base + (typ->p.cover_roll_ang + typ->p.cover_roll_mergin) * safe_powf(rate / (1.0f - hold), typ->p.cover_roll_pow);
            else
                w->cover_roll = (w->cover_roll_base + typ->p.cover_roll_ang) - typ->p.cover_roll_mergin * (1.0f - rate) * hold;
        }
    }
    gilgamesh_damage_swing(obj, w);
}

/* 0x00b01788 */
int gilgamesh_next_state(void *obj, GilgameshWork *w, int n, int state)
{
    Vec3 p_end;
    Vec3 p_prev;
    Vec3 dv;
    float unit;
    float end;

    gilgamesh_motion(obj, w);
    unit = 1.0f;
    if (fabsf(w->prog - unit) < FLT_EPSILON || w->prog > unit) {
        pass_table_push(stage_sequence);
        end = pass_table_end(w->path);
        w->state = 5;
        v3_sub(&dv, &w->cur, &w->prev);
        w->speed = vec3_length(&dv);
        sequence_eval(w->path, end - unit, 0, &p_end, 0);
        sequence_eval(w->path, end - 2.0f, 0, &p_prev, 0);
        v3_sub(&dv, &p_end, &p_prev);
        vec3_normalize(&w->dir, &dv);
        w->prog_from = w->prog;
        v3_copy(&w->from, &w->cur);
        w->frames = 1;
        pass_table_pop();
        return 1;
    }
    v3_copy(&w->prev, &w->cur);
    if (++w->frames > n) {
        w->prog_from = w->prog;
        w->frames = 1;
        v3_copy(&w->from, &w->cur);
        w->state = state;
        return 1;
    }
    return 0;
}

/* 0x00b01990 */
int gilgamesh_state_appear(void *obj, GilgameshWork *w)
{
    GilgameshVars *typ = w->type;
    float v, sec, start_spd, tm, dis;
    int n;
    Vec3 dv;

    v = w->speed * FPS;
    sec = typ->p.appear_sec;
    n = sec * FPS;
    start_spd = (typ->p.appear_dis + typ->p.appear_dis - v * sec) / sec;
    tm = sec * (w->frames / (float)n);
    dis = start_spd * tm + (v - start_spd) / sec * tm * tm * 0.5f;
    v3_mul(&dv, &w->dir, dis);
    v3_sum(&w->cur, &w->from, &dv);
    gilgamesh_next_state(obj, w, n, 1);
    return 0;
}

/* 0x00b01a98 */
int gilgamesh_state_move(void *obj, GilgameshWork *w)
{
    float prog;
    int n;

    n = enemy_task_get_param(obj, 1) * 60.0f / 60.0f + 1.0f;
    w->prog += enemy_task_get_param(obj, 0) / FPS;
    prog = MIN(w->prog, 1.0f);
    w->prog = prog;
    gilgamesh_path_eval(&w->cur, w->path, prog);
    gilgamesh_next_state(obj, w, n, 2);
    return 0;
}

/* 0x00b01b78 */
int gilgamesh_state_break(void *obj, GilgameshWork *w)
{
    GilgameshVars *typ = w->type;
    float sec, v, prog;
    ModelPart *gfx;
    int n;

    sec = typ->p.break_sec;
    n = sec * FPS + 1.0f;
    sec = sec * (w->frames / (float)n);
    v = enemy_task_get_param(obj, 0);
    w->prog = w->prog_from + v * sec + -v / sec * sec * sec * 0.5f;
    prog = MIN(w->prog, 1.0f);
    w->prog = prog;
    gilgamesh_path_eval(&w->cur, w->path, prog);
    if (w->stops > 0) {
        float frm;

        gfx = enemy_task_gfx_ptr(obj);
        frm = w->frames * 0.0f / (float)n;
        gfx->frame = MIN(frm >= 0.0f ? frm : 0.0f, 0.0f);
    }
    gilgamesh_next_state(obj, w, n, 3);
    return 0;
}

/* 0x00b01cf0 */
int gilgamesh_state_stop(void *obj, GilgameshWork *w)
{
    GilgameshVars *typ = w->type;
    int done = 0;
    int n;
    int i;
    ModelPart *gfx;
    AttackRayArg ray;

    n = typ->p.stop_sec * FPS + 1.0f;
    if (w->stops > 0) {
        gfx = enemy_task_gfx_ptr(obj);
        gfx->frame = w->frames * 31.0f / (float)n + 0.0f;
        if (w->rays == 0) {
            w->rays = 1;
            for (i = 0; i < 6; i++) {
                heap_bzero(&ray, sizeof ray);
                ray.pos = &w->tip[i];
                ray.ang = w->tip_ang[i];
                ray.scale = enemy_task_scl_ptr(obj);
                ray.dur = (float)n;
                ray_create(obj, &ray);
            }
        }
    }
    if (gilgamesh_next_state(obj, w, n, 4)) {
        if (w->stops > 0) {
            w->firing = 1;
            done = 1;
        }
    }
    return done;
}

/* 0x00b01e48 */
int gilgamesh_state_accel(void *obj, GilgameshWork *w)
{
    GilgameshVars *typ = w->type;
    float sec, rate, prog;
    ModelPart *gfx;
    int n;

    n = typ->p.accel_sec * FPS + 1.0f;
    rate = w->frames / (float)n;
    sec = typ->p.break_sec * rate;
    w->prog = w->prog_from + enemy_task_get_param(obj, 0) / sec * sec * sec * 0.5f;
    prog = MIN(w->prog, 1.0f);
    w->prog = prog;
    gilgamesh_path_eval(&w->cur, w->path, prog);
    if (w->firing) {
        gfx = enemy_task_gfx_ptr(obj);
        gfx->frame = (gfx->frames - 31.0f) * rate + 31.0f;
    }
    if (gilgamesh_next_state(obj, w, n, 1)) {
        w->rays = 0;
        w->firing = 0;
        w->stops--;
    }
    return 0;
}

/* 0x00b01fa0 */
int gilgamesh_state_disappear(void *obj, GilgameshWork *w)
{
    GilgameshVars *typ = w->type;
    float v, sec, acc, tm, dis;
    int n;
    Vec3 dv;

    v = w->speed * FPS;
    n = typ->p.disappear_sec * FPS;
    tm = (float)w->frames++;
    sec = typ->p.disappear_sec;
    acc = (typ->p.disappear_dis + typ->p.disappear_dis - v * sec) / sec;
    acc = (acc - v) / sec;
    sec *= tm / n;
    dis = v * sec + acc * sec * sec * 0.5f;
    v3_mul(&dv, &w->dir, dis);
    v3_sum(&w->cur, &w->from, &dv);
    gilgamesh_motion(obj, w);
    return 0;
}

/* 0x00b020b8 */
void gilgamesh_update(void *obj, GilgameshWork *w)
{
    int (*fn)(void *obj, GilgameshWork *w) = gilgamesh_states[w->state];
    int fire = 0;

    if (fn)
        fire = fn(obj, w);
    gilgamesh_place(obj, w);
    if (fire)
        gilgamesh_fire_shot(obj, w);
}

/* 0x00b02148 */
void gilgamesh_exec_appear(void *obj, GilgameshWork *w)
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
        gilgamesh_update(obj, w);
        if (w->life > 0 && --w->life <= 0) {
            w->lock.flags &= ~0x100;
            enemy_task_set_state(obj, 2);
        }
        enemy_task_appear_scale(obj, w);
        break;
    }
}

/* 0x00b02218 */
void gilgamesh_exec_normal(void *obj, GilgameshWork *w)
{
    if (enemy_task_get_phase(obj) != 1)
        return;
    gilgamesh_update(obj, w);
    if (w->life > 0 && --w->life <= 0) {
        w->lock.flags &= ~0x100;
        enemy_task_set_state(obj, 2);
    }
}

/* 0x00b02290 */
void gilgamesh_exec_leave(void *obj, GilgameshWork *w)
{
    if (enemy_task_get_phase(obj) != 1) {
        enemy_task_vanish_scale(obj, w);
        return;
    }
    gilgamesh_update(obj, w);
    enemy_task_vanish_scale(obj, w);
}

/* 0x00b022f8 */
void gilgamesh_exec_destroyed(void *obj, GilgameshWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 0) {
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 20;
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

/* 0x00b023c8 */
void gilgamesh_exec(void *obj, GilgameshWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (gilgamesh_exec_table[i])
        gilgamesh_exec_table[i](obj, w);
}

/* steer(cast) */
/* 0x00b02420 */
void gilgamesh_post_exec(void *obj, GilgameshWork *w)
{
    LockObj *lock = &w->lock;
    Vec3 p;
    Vec3 pos;

    if (!enemy_status_get_ticks())
        gilgamesh_place(obj, w);
    if (lock->flags & LOCK_HIT) {
        v3_copy(&w->hitv, &lock->imp.v);
        w->damage_swing_max = w->damage_swing;
        w->damage_count = w->type->p.damage_swing_sec * FPS + 1.0f;
        gilgamesh_damage_swing(obj, w);
        ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&w->core, (float *)&p);
        project_point(&p, &p);
        switch (lock->life) {
        case 0:
            SoundRhythmSe_EnemyPos(gilgamesh_init_arg, 1, (float *)&p);
            break;
        case 1:
            SoundRhythmSe_EnemyPos(gilgamesh_init_arg, 2, (float *)&p);
            break;
        case 2:
            SoundRhythmSe_EnemyPos(gilgamesh_init_arg, 0, (float *)&p);
            break;
        }
    }
    if (lock->life <= 0 && ((unsigned int)enemy_task_get_pstate(obj)) < 2) {
        int mult = lockon_hits_max_shot_cnt(lock);
        GilgameshVars *typ = w->type;

        enemy_task_get_pos(obj, &pos);
        enemy_status_score_popup(enemy_task_get_status_slot(obj), &pos, typ->p.score, mult);
        enemy_task_set_state(obj, 3);
    }
}

/* 0x00b025e0 */
void gilgamesh_draw_nodes(unsigned int *node, int ry_a, int ry_b)
{
    do {
        ulMatrixPush();
        anim_apply_node(node);
        if (node[0] & 0x20000000)
            vu0_matrix_rotate_y(ulMatrixCur, ry_a);
        if (node[0] & 0x10000000)
            vu0_matrix_rotate_y(ulMatrixCur, ry_b);
        if (node[1])
            nj_cnk_vu1_draw((void *)node[1]);
        if (!(node[0] & 0x10))
            gilgamesh_draw_nodes((unsigned int *)node[11], ry_a, ry_b);
        ulMatrixPop();
        node = (unsigned int *)node[12];
    } while (node);
}

/* 0x00b026c0 */
void gilgamesh_disp_body(void *obj, GilgameshWork *w)
{
    Vec3 p;
    int ang[4];
    Vec3 scl;
    ModelPart *gfx;
    GilgameshVars *typ;

    gfx = enemy_task_gfx_ptr(obj);
    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    enemy_task_get_scl(obj, &scl);
    if (view_sphere_visible(&p, gfx->radius)) {
        ulGxSetTextureList(((void **)obj)[1]);
        ulMatrixPush();
        typ = w->type;
        do {
            Vec3 *p_ = &p;

            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        gilgamesh_rotate_yxz(ang);
        vu0_matrix_rotate_y(ulMatrixCur, w->damage_swing + ANG_180 + w->damage_yaw);
        vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
        anim_start((void *)gfx->clip, gfx->frame);
        gilgamesh_draw_nodes((unsigned int *)gfx->root, -w->roll, -w->cover_roll);
        if (typ->p.wire) {
            draw_set_color(yuki_beat_color(typ->p.wire_col, typ->p.wire_col2, typ->p.wire_beat, typ->p.wire_linear, 1.0f, typ->p.wire_near,
                                          typ->p.wire_far, typ->p.wire_near_rate, typ->p.wire_far_rate));
            draw_set_blend(4, 5);
            vu0_matrix_rotate_z(ulMatrixCur, -w->cover_roll);
            model_object_draw((struct DcObjectTreeView *)&w->cover, gfx->frame);
        }
        ulMatrixPop();
    }
}

/* 0x00b02840 */
void gilgamesh_disp(void *obj, GilgameshWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (gilgamesh_disp_table[i])
        gilgamesh_disp_table[i](obj, w);
}

/* 0x00b02898 */
int gilgamesh_path_frames(float spd, GilgameshVars *types, int frm, int path, int *out)
{
    Vec3 *seg = (Vec3 *)out;
    float sec = frm / FPS;
    float total = 0.0f;
    float pos = 0.0f;
    float next;
    float stop_pos;
    float dis;
    Vec3 p0;
    Vec3 pt;

    pass_table_push(stage_sequence);
    sequence_eval(path, 0.0f, 0, &p0, 0);
    pass_table_pop();
    gilgamesh_path_eval(&pt, path, spd / FPS);
    v3_sub(seg, &pt, &p0);
    vec3_length(seg);
    pass_table_push(stage_sequence);
    sequence_eval(path, 1.0f, 0, &pt, 0);
    pass_table_pop();
    v3_sub(seg, &pt, &p0);
    total += types->p.appear_sec;
    for (;;) {
        next = spd * sec + pos;
        if (fabsf(next - 1.0f) < FLT_EPSILON || next > 1.0f) {
            total += (1.0f - pos) / spd;
            break;
        }
        {
            float adis = types->p.break_sec;

            adis = spd * adis * 0.5f;
            dis = adis;
            stop_pos = dis + next;
        }
        total += sec;
        if (fabsf(stop_pos - 1.0f) < FLT_EPSILON || stop_pos > 1.0f) {
            total += types->p.break_sec - sqrtf(2.0f * (next - 1.0f) * types->p.break_sec / spd);
            break;
        }
        pos = stop_pos;
        total += types->p.break_sec;
        total += types->p.stop_sec;
        dis = spd * types->p.accel_sec * 0.5f;
        next = dis + pos;
        if (fabsf(next - 1.0f) < FLT_EPSILON || next > 1.0f) {
            total += sqrtf(2.0f * (1.0f - pos) * types->p.accel_sec / spd);
            break;
        }
        total += types->p.accel_sec;
        pos = next;
    }
    total += types->p.disappear_sec;
    return total * 60.0f - 60.0f;
}

/* 0x00b02b98 */
int gilgamesh_create(void *parent, Placement *place)
{
    int prm[8];
    ObjDesc desc;
    ObjFuncs funcs = gilgamesh_funcs;
    GilgameshVars *typ;
    int i;

    i = place->param2;
    if (i < 0)
        i = 0;
    if ((unsigned int)i > 2)
        i = 2;
    prm[0] = i;
    typ = &gilgamesh_types[i];
    if (fabsf(place->param0) < FLT_EPSILON || place->param0 < 0.0f)
        place->param0 = 0.1f;
    pass_table_push(stage_sequence);
    prm[1] = MIN(place->param3 >= 0.0f ? place->param3 : 0.0f, 2.0f) + pass_find_by_symbol(gilgamesh_path_sym);
    pass_table_pop();
    prm[2] = gilgamesh_path_frames(place->param0, typ, (int)place->param1, prm[1], &prm[3]);
    nj_fill8(&desc, 0, sizeof desc);
    desc.name = gilgamesh_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.kind = prm;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->p.hp;
    desc.same_lock = typ->p.same_lock;
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = 18;
    desc.model_no2 = 21;
    desc.score = typ->p.score;
    desc.hp_reduce = typ->p.hp_reduce;
    desc.crash_r = typ->p.crash_r;
    desc.crash_damage = typ->p.crash_damage;
    desc.time = prm[2];
    desc.size = 500;
    return enemy_task_task_create(parent, &desc);
}

/* 0x00b02dd8 */
void gilgamesh_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
