/*
 * Dominions parts, Area 5: DominionsChild, launched by body segments (flies out, follows the
 * chain, stops to fire, leaves), and enDominionsHole, the portal releasing the head carrier and
 * chain; with a second hole the chain dives back in at the end of its pass.
 */

#include "stg05.h"
#include "mathdef.h"
#include "debug.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "game_flow_enemy.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_draw.h"
#include "nj_system.h"
#include "pass.h"
#include "player.h"
#include "pnb.h"
#include "sound_midise.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "dominions_child.h"
#include "dominions_head_carrier.h"
#include "dominions_head_shooter.h"
#include "enb_sidewinder.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"

extern int dominions_child_init_arg;
extern int en_dominions_hole_init_arg;
extern void (*dominions_child_exec_table[])(void *o, TypeEnemyWork *w);
extern void (*dominions_child_disp_table[])(void *o, TypeEnemyWork *w);

typedef struct EnDominionsHoleWork {
    void *models;               /* 0x00 */
    void *textures;             /* 0x04 */
    void *desc;                 /* 0x08 */
    ModelPart body;                /* 0x0c */
    Vec3 pos;                   /* 0x68 */
    int ang[3];                 /* 0x74 */
    Vec3 pos2;                  /* 0x80: the second hole, at the end of the pass */
    int ang2[3];                /* 0x8c */
    int sel;                    /* 0x98: pass index, -1 without a second hole */
    EnDominionsHoleArg arg;                /* 0x9c */
    float t1;                   /* 0xdc: motion time of the first hole */
    float t2;                   /* 0xe0: and of the second */
    int wait;                   /* 0xe4 */
    int timer;                  /* 0xe8: frames to the next opening */
    void *child;                /* 0xec: the DominionsHead it created */
} EnDominionsHoleWork;

extern struct TaskDesc en_dominions_hole_desc;

/* group name, label, 25 values */
typedef struct DominionsChildSet {
    char *name;
    char *label;                /* 0x04 */
    int v[25];                  /* 0x08 */
} DominionsChildSet;

extern DominionsChildSet dominions_child_types[1];
extern VarDef dominions_child_vardef;
extern VarDef dominions_child_vardefs[25];

typedef struct DominionsChildWork {
    float *type;                /* variable record */
    int count;                  /* 0x04 */
    int state;                  /* 0x08 */
    DominionsChildLaunch launch;/* 0x0c: a copy of the creation arguments */
    Vec3 hitv;                  /* 0x20: the lock-on record's vector when hit */
    int ang[3];                 /* 0x2c */
    Vec3 vel;                   /* 0x38: the per-frame move kept by state 1 */
    Vec3 pos;                   /* 0x44 */
    float t;                    /* 0x50 */
    float rate;                 /* 0x54 */
    int shot_time, shot_num;               /* 0x58: from the record */
    int unk_60;                 /* 0x60: cleared on stop; read as float_frac(unk_60) * 1000 */
    int unk_64;                 /* 0x64: cleared by dominions_child_state_stop, never read */
    int fire;                   /* 0x68 */
    int unk_6c;                 /* 0x6c: set to 0 by dominions_child_init, never read */
} DominionsChildWork;

/* pointers to position and angles */
typedef struct PosAngArg {
    char pad_00[12];
    Vec3 *pos;                  /* 0x0c */
    int *ang;                   /* 0x10 */
} PosAngArg;

/* group name, label, 1 value */
typedef struct EnDominionsHoleSet {
    char *name;
    char *label;                /* 0x04 */
    int v[1];                   /* 0x08 */
} EnDominionsHoleSet;

extern EnDominionsHoleSet en_dominions_hole_types[1];
extern VarDef en_dominions_hole_vardef;
extern VarDef en_dominions_hole_vardefs[1];
extern int dominions_child_pass_symbol;
extern char dominions_child_lock_name[];
extern int (*dominions_child_states[])(void *o, DominionsChildWork *w);

/* ObjDesc.kind points to it */
typedef struct DominionsChildKind {
    int idx;
    DominionsChildLaunch *arg;  /* 0x04: copied to the work's launch */
} DominionsChildKind;

extern char dominions_child_funcs[];
extern char dominions_child_name[];
extern int en_dominions_hole_pass_symbol;

/* 0x00b06270 */
void dominions_child_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&dominions_child_vardef);
    for (i = 0; i < 1; i++) {
        def = dominions_child_vardefs;
        val = dominions_child_types[i].v;
        dominions_child_vardef.name = dominions_child_types[i].name;
        dominions_child_vardef.fmt = dominions_child_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&dominions_child_vardef);
        for (j = 0; j < 25; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)dominions_child_vardefs);
    }
}

/* 0x00b06388 */
void dominions_child_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b06390 */
int dominions_child_get_init_arg(void)
{
    return dominions_child_init_arg;
}

/* 0x00b063a0 */
int dominions_child_vars_init(int *prm)
{
    dominions_child_init_arg = *prm;
    dominions_child_vars_register();
    return 0;
}

/* 0x00b063c8 */
int dominions_child_vars_release(void)
{
    dominions_child_vars_unregister();
    return 0;
}

/* 0x00b063e8 */
void dominions_child_eval_pass(void *obj, DominionsChildWork *w, void *out)
{
    pass_table_push(stage_sequence);
    sequence_eval(w->launch.seq, w->rate * w->t, 0, out, 0);
    pass_table_pop();
}

/* 0x00b06440 */
void dominions_child_init(void *obj, DominionsChildWork *w, void *prm, DominionsChildKind *k)
{
    int n;

    w->type = (float *)&dominions_child_types[k->idx];
    enemy_task_clear_flags(obj, 64);
    do {
        void *m_ = stage_models, *t_ = stage_textures;
        ((void **)obj)[0] = m_;
        ((void **)obj)[1] = t_;
    } while (0);
    w->state = 0;
    w->count = 0;
    lockon_obj_set_name(enemy_task_lock_ptr(obj), dominions_child_lock_name);
    ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_SILENT;
    w->launch = *k->arg;
    w->fire = 0;
    w->unk_6c = 0;
    pass_table_push(stage_sequence);
    n = pass_find_by_symbol(dominions_child_pass_symbol);
    w->launch.seq = MIN(MAX_GE(w->launch.seq, 0), 5) + n;
    w->rate = pass_table_end(w->launch.seq) - 1.0f;
    pass_table_pop();
    w->t = 0;
}

/* 0x00b06578 */
void dominions_child_release(void)
{
}

/* 0x00b06580 */
int dominions_child_next_state(void *obj, DominionsChildWork *w, int n, int state)
{
    if (++w->count > n) {
        w->state = state;
        w->count = 1;
        enemy_task_get_rot(obj, w->ang);
        return 1;
    }
    return 0;
}

/* 0x00b065c8 */
int dominions_child_state_start(void *obj, PosAngArg *prm)
{
    enemy_task_set_pos(obj, prm->pos);
    enemy_task_set_rot(obj, prm->ang);
    return 0;
}

/* 0x00b06610 */
int dominions_child_state_launch(void *obj, DominionsChildWork *w)
{
    float unit = 1.0f;
    Vec3 p;
    Vec3 v;
    int ang[3];
    Vec3 mv;
    Vec3 v32;
    float sc[2];
    float *typ = w->type;
    int n = typ[8] * FPS;
    float k = typ[9];
    float acc = k * (typ[16] - unit) / typ[8];
    float prev = (float)(w->count - 1) / n;
    float rate = (float)w->count / n;
    float mrate;
    ModelPart *gfx;

    k += acc * typ[8] * prev;
    k *= FRAME_SEC;
    k += acc * 0.5f * FRAME_SEC * FRAME_SEC;
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, k);
    enemy_task_get_pos(obj, &p);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    if (typ[10] < rate) {
        gfx = enemy_task_gfx_ptr(obj);
        mrate = (rate - typ[10]) / (typ[11] - typ[10]);
        gfx->frame = gfx->frames * MIN(MAX_GE(mrate, 0.0f), 1.0f);
    }
    if (typ[12] < rate) {
        float unit = 1.0f;
        float k;

        rate = (rate - typ[12]) / (unit - typ[12]);
        ang[0] = 0;
        ang[1] = w->ang[1];
        ulSinCos((int)(*(int *)&typ[14] * rate), &sc[0], &sc[1]);
        k = safe_powf(unit - rate, typ[15]) * sc[0];
        ang[2] = k * *(int *)&typ[13];
        yuki_lerp3(ang, ang, w->ang, k);
        enemy_task_set_rot(obj, ang);
        rate = (prev - typ[12]) / (unit - typ[12]);
        ulSinCos((int)(*(int *)&typ[14] * rate), &sc[0], &sc[1]);
        k -= safe_powf(unit - rate, typ[15]) * sc[0];
        v3_mul(&mv, &w->vel, typ[17] * k);
        v3_add(&p, &mv);
        enemy_task_set_pos(obj, &p);
    }
    if (dominions_child_next_state(obj, w, n, 2)) {
        enemy_task_get_pos(obj, &w->pos);
        k = typ[9];
        k *= typ[16];
        enemy_task_get_param_vec(obj, &v32);
        k *= FRAME_SEC;
        v3_mul(&w->vel, &v32, k);
    }
    return 0;
}

/* 0x00b069f8 */
int dominions_child_state_join(void *obj, DominionsChildWork *w)
{
    Vec3 p;
    Vec3 cur;
    int ang[3];
    float sc[2];
    float *typ = w->type;
    int n = typ[18] * FPS;
    float rate = (float)w->count / n;
    Vec3 *kept = &w->pos;

    enemy_task_get_pos(obj, &cur);
    v3_add(kept, &w->vel);
    dominions_child_eval_pass(obj, w, &p);
    ulSinCos((int)(rate * ANG_HALF - ANG_QUARTER), &sc[0], &sc[1]);
    sc[0] = (sc[0] + 1.0f) * 0.5f;
    p.x = p.x * sc[0] + kept->x * (1.0f - sc[0]);
    p.y = p.y * sc[0] + kept->y * (1.0f - sc[0]);
    p.z = p.z * sc[0] + kept->z * (1.0f - sc[0]);
    enemy_task_set_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    yuki_draw_aimed(&p, &cur, ang);
    enemy_task_set_rot(obj, ang);
    w->t += typ[20] / FPS;
    dominions_child_next_state(obj, w, n, 3);
    return 0;
}

/* 0x00b06be8 */
int dominions_child_state_follow(void *obj, DominionsChildWork *w)
{
    Vec3 p;
    Vec3 cur;
    int ang[3];
    float *typ = w->type;

    enemy_task_get_pos(obj, &cur);
    dominions_child_eval_pass(obj, w, &p);
    enemy_task_set_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    yuki_draw_aimed(&p, &cur, ang);
    enemy_task_set_rot(obj, ang);
    w->t += typ[20] / FPS;
    return 0;
}

/* 0x00b06cb0 */
int dominions_child_state_stop(void *obj, DominionsChildWork *w)
{
    Vec3 p;
    Vec3 cur;
    int ang[3];
    Vec3 pl;
    Vec3 to_pl;
    float *typ = w->type;
    int n = typ[19] * FPS;
    float dec = -typ[20] / typ[19];
    float rate = (float)w->count / n;

    w->t += (typ[20] + dec * typ[19] * rate) * FRAME_SEC + dec * FRAME_SEC * FRAME_SEC * 0.5f;
    enemy_task_get_pos(obj, &cur);
    dominions_child_eval_pass(obj, w, &p);
    enemy_task_set_pos(obj, &p);
    player_get_pos(&pl);
    v3_sub(&to_pl, &pl, &p);
    do {
        ang[1] = RAD_TO_ANG(atan2f(-to_pl.x, -to_pl.z));
        ang[0] = RAD_TO_ANG(atan2f(to_pl.y, sqrtf(to_pl.x * to_pl.x + to_pl.z * to_pl.z)));
    } while (0);
    ang[2] = 0;
    yuki_lerp3(ang, w->ang, ang, rate);
    enemy_task_set_rot(obj, ang);
    if (dominions_child_next_state(obj, w, n, 5)) {
        w->shot_time = *(int *)&typ[21];
        w->shot_num = *(int *)&typ[22];
        w->unk_64 = w->unk_60 = 0;
    }
    return 0;
}

/* 0x00b06ee0 */
int dominions_child_state_leave(void *obj, DominionsChildWork *w)
{
    Vec3 p;
    Vec3 cur;
    int ang[3];
    float *typ = w->type;
    int n = typ[19] * FPS;
    float spd = typ[20];
    float acc = spd / typ[19];
    float rate = (float)w->count / n;

    spd = acc * typ[19] * rate;
    w->t += spd * FRAME_SEC + acc * FRAME_SEC * FRAME_SEC * 0.5f;
    enemy_task_get_pos(obj, &cur);
    dominions_child_eval_pass(obj, w, &p);
    enemy_task_set_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    yuki_draw_aimed(&p, &cur, ang);
    enemy_task_set_rot(obj, ang);
    yuki_lerp3(ang, w->ang, ang, rate);
    enemy_task_set_rot(obj, ang);
    dominions_child_next_state(obj, w, n, 3);
    return 0;
}

/* 0x00b07040 */
int dominions_child_state_wait(void *obj, DominionsChildWork *w)
{
    return dominions_child_next_state(obj, w, w->type[23] * FPS, 6);
}

/* steer(cast) */
/* 0x00b07078 */
void dominions_child_update(void *obj, TypeEnemyWork *work)
{
    DominionsChildWork *w = (DominionsChildWork *)work;
    int (*fn)(void *obj, DominionsChildWork *w) = dominions_child_states[w->state];
    Vec3 p;
    Vec3 dir;
    int ang[3];
    float sc[2][2];
    float x, y, z;
    int n;

    if (fn && fn(obj, w)) {
        y = 0.0f;
        x = y;
        n = ((float (*)(float))float_frac)(w->unk_60) * 1000.0f;
        enemy_task_get_pos(obj, &p);
        enemy_task_get_rot(obj, ang);
        z = -1.0f;
        ROT_X(ang[0], sc[0], y, z);
        ROT_Y(ang[1], sc[1], x, z);
        dir.x = x;
        dir.y = y;
        dir.z = z;
        n = MAX_GE(n, 0);
        e_shot_fire(obj, &p, &dir, n > 6 ? 6 : n);
    }
}

/* 0x00b071c0 */
void dominions_child_exec_appear(void *obj, TypeEnemyWork *w)
{
    switch (enemy_task_get_phase(obj)) {
    case 0:
        ((LockObj *)enemy_task_lock_ptr(obj))->flags &= ~LOCK_LOCKABLE;
        break;
    case 2:
        break;
    default:
        dominions_child_update(obj, w);
    }
}

/* 0x00b07238 */
void dominions_child_exec_normal(void *obj, TypeEnemyWork *w)
{
    if (enemy_task_get_phase(obj) == 1)
        dominions_child_update(obj, w);
}

/* 0x00b07288 */
void dominions_child_exec_leave(void *obj, DominionsChildWork *w)
{
    Vec3 p;

    if (enemy_task_get_phase(obj) != 1) {
        ((LockObj *)enemy_task_lock_ptr(obj))->flags &= ~LOCK_LOCKABLE;
    } else {
        dominions_child_update(obj, (TypeEnemyWork *)w);
        enemy_task_get_pos(obj, &p);
        if (!view_sphere_visible(&p, ((ModelPart *)enemy_task_gfx_ptr(obj))->radius))
            FINISH_IF(obj);
    }
}

/* 0x00b07328 */
void dominions_child_exec_destroyed(void *obj, DominionsChildWork *w)
{
    SpawnDesc desc;
    Vec3 vpos;

    if (enemy_task_get_phase(obj) == 0) {
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 16;
        desc.models = stage_models;
        desc.kind = 0;
        desc.unk_50 = 0;
        desc.lock = enemy_task_lock_ptr(obj);
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        v3_copy(&desc.v, &w->hitv);
        enemy_shatter_create(obj, &desc, 0, 0);
        enemy_task_get_pos(obj, &vpos);
        ulMatrixApplyPoint((float *)ulMatrixCur, &vpos.x, &vpos.x);
        project_point(&vpos, &vpos);
        SoundRhythmSe_EnemyPos(dominions_child_init_arg, 1, &vpos.x);
        FINISH_IF(obj);
    }
}

/* 0x00b07440 */
void dominions_child_pre_exec(void *obj)
{
    Vec3 p, org;

    player_get_frame_vel(&org);
    enemy_task_get_pos(obj, &p);
    p.x -= org.x;
    p.y -= org.y;
    p.z -= org.z;
    enemy_task_set_pos(obj, &p);
}

/* 0x00b074b0 */
void dominions_child_exec(void *obj, TypeEnemyWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (dominions_child_exec_table[i])
        dominions_child_exec_table[i](obj, w);
}

/* steer(cast) */
/* 0x00b07508 */
void dominions_child_post_exec(void *obj, DominionsChildWork *w)
{
    LockObj *lock = enemy_task_lock_ptr(obj);

    if (lock->flags & LOCK_HIT)
        v3_copy(&w->hitv, &lock->imp.v);
    if (lock->life <= 0 && ((unsigned int)enemy_task_get_pstate(obj)) < 2)
        enemy_task_set_state(obj, 3);
}

/* 0x00b075a0 */
void dominions_child_disp(void *obj, TypeEnemyWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (dominions_child_disp_table[i])
        dominions_child_disp_table[i](obj, w);
}

/* 0x00b075f8 */
void *dominions_child_create(void *obj, ShotArg *sarg, DominionsChildLaunch *launch)
{
    DominionsChildKind k;
    ObjDesc desc;
    ObjFuncs funcs = *(ObjFuncs *)dominions_child_funcs;
    DominionsChildSet *typ = dominions_child_types;

    k.arg = launch;
    sarg->kind = dominions_child_init_arg;
    sarg->fx_arg = -1;
    k.idx = 0;
    nj_fill8(&desc, 0, sizeof desc);
    desc.name = dominions_child_name;
    desc.funcs = &funcs;
    desc.arg = sarg;
    desc.kind = &k.idx;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->v[0];
    desc.same_lock = typ->v[1];
    desc.flags = 16;
    desc.model_no = 16;
    desc.model_no2 = 17;
    desc.score = typ->v[2];
    desc.hp_reduce = *(float *)&typ->v[3];
    desc.crash_r = *(float *)&typ->v[4];
    desc.crash_damage = *(float *)&typ->v[5];
    desc.size = 112;
    desc.dc_flags = 0;
    desc.time = 0;
    return (void *)enemy_task_task_create(TASK_OF(obj)->parent ? TASK_OF(obj)->parent + 1 : 0, &desc);
}

/* 0x00b07750 */
void dominions_child_launch(void *obj)
{
    DominionsChildWork *w = enemy_task_work_ptr(obj);
    int *lang;
    Vec3 v;
    float sc[4][2];
    float x, y, z;

    w->state = 1;
    x = 0.0f;
    lang = w->launch.ang;
    z = x;
    y = 1.0f;
    ROT_Z(lang[2], sc[0], x, y);
    ROT_X(lang[0], sc[1], y, z);
    ROT_Y(lang[1], sc[2], x, z);
    v.x = x;
    v.y = y;
    v.z = z;
    enemy_task_set_param_vec(obj, &v);
    enemy_task_get_rot(obj, w->ang);
    y = 0.0f;
    z = y;
    x = 1.0f;
    ROT_Y(lang[1], sc[3], x, z);
    v3_set(&w->vel, x, y, z);
    ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_LOCKABLE;
    SoundRhythmSe_Enemy(dominions_child_init_arg, 2);
}

/* steer(cast) */
/* 0x00b078d0 */
void dominions_child_destroy(void *obj)
{
    if (((unsigned int)enemy_task_get_pstate(obj)) < 2)
        enemy_task_set_state(obj, 3);
}

/* 0x00b07910 */
void en_dominions_hole_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&en_dominions_hole_vardef);
    for (i = 0; i < 1; i++) {
        def = en_dominions_hole_vardefs;
        val = en_dominions_hole_types[i].v;
        en_dominions_hole_vardef.name = en_dominions_hole_types[i].name;
        en_dominions_hole_vardef.fmt = en_dominions_hole_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&en_dominions_hole_vardef);
        for (j = 0; j < 1; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_dominions_hole_vardefs);
    }
}

/* 0x00b07a28 */
void en_dominions_hole_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b07a30 */
int en_dominions_hole_vars_init(int *prm)
{
    en_dominions_hole_init_arg = *prm;
    en_dominions_hole_vars_register();
    return 0;
}

/* 0x00b07a58 */
int en_dominions_hole_vars_release(void)
{
    en_dominions_hole_vars_unregister();
    return 0;
}

/* steer(cast) */
/* 0x00b07a78 */
int en_dominions_hole_marker_draw(void *w, EnDominionsHoleArg *prm)
{
    Vec3 p;
    Vec3 *v;
    int i, n, sel;

    ulGxSetTextureList(stage_textures);
    pass_table_push(stage_sequence);
    if (prm->sel < -0.5f)
        i = -1;
    else
        i = prm->sel + 0.5f;
    if (i < 0) {
        v = &prm->pos;
        if (view_sphere_visible(v, 10.0f)) {
            ulMatrixPush();
            do {
                Vec3 *p_ = v;
                vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
            } while (0);
            en_dominions_hole_rotate_yxz(prm->ang);
            vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
            nj_object_draw_shapes(*((int *)pnb_get_entry(stage_models, 14)));
            ulMatrixPop();
        }
    } else {
        v = &prm->pos;
        n = pass_find_by_symbol(en_dominions_hole_pass_symbol);
        sel = i > -1 ? i : 0;
        sel = sel < 6 ? sel : 5;
        i = sel + n;
        if (view_sphere_visible(v, 10.0f)) {
            ulMatrixPush();
            do {
                Vec3 *p_ = v;
                vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
            } while (0);
            en_dominions_hole_rotate_yxz(prm->ang);
            vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
            nj_object_draw_shapes(*((int *)pnb_get_entry(stage_models, 14)));
            ulMatrixPop();
        }
        sequence_eval(i, pass_table_end(i) - 1.0f, 0, &p, 0);
        do {
            ulMatrixPush();
            vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
            en_dominions_hole_rotate_yxz(prm->ang);
            vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
            nj_object_draw_shapes(*((int *)pnb_get_entry(stage_models, 14)));
            ulMatrixPop();
        } while (0);
    }
    pass_table_pop();
    return 0;
}

/* 0x00b07ca8 */
void cmd_enemy_kill_all(void)
{
    flow_enemy_kill_all();
}

/* 0x00b07cb0 */
int en_dominions_hole_get_init_arg(void)
{
    return en_dominions_hole_init_arg;
}

/* steer(cast) */
/* 0x00b07cc0 */
void en_dominions_hole_init(EnDominionsHoleWork *w, EnDominionsHoleArg *prm)
{
    Vec3 org;
    Vec3 p;
    float sc[3][2];
    int *ang = prm->ang;
    float x, y, z;
    int n;

    w->desc = en_dominions_hole_types;
    w->timer = prm->time * 60.0f / 60.0f;
    do {
        void *m_ = stage_models, *t_ = stage_textures;
        w->models = m_;
        w->textures = t_;
    } while (0);
    dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(w->models, 14)), *((struct DcClip **)pnb_get_entry(w->models, 15)), 0);
    dc_object_rebuild(&w->body);
    w->body.radius = 64.0f;
    player_get_frame_vel(&org);
    v3_sum_to(&prm->pos, &org, &w->pos);
    {
        void *q_ = stage_sequence;

        i3_copy(w->ang, ang);
        pass_table_push(q_);
    }
    w->sel = prm->sel < -0.5f ? -1 : (int)(prm->sel + 0.5f);
    if (w->sel < 0) {
        v3_set(&w->pos2, 0.0f, 0.0f, 0.0f);
        i3_set(w->ang2, 0, 0, 0);
    } else {
        n = pass_find_by_symbol(en_dominions_hole_pass_symbol);
        {
            int sel = w->sel;
            sel = sel > -1 ? sel : 0;
            sel = sel < 6 ? sel : 5;
            w->sel = sel + n;
        }
        sequence_eval(w->sel, pass_table_end(w->sel) - 1.0f, 0, &p, 0);
        do {
            Vec3 *p_ = &p;
            x = p_->x;
            y = p_->y;
            z = p_->z;
        } while (0);
        ROT_Z(ang[2], sc[0], x, y);
        ROT_X(ang[0], sc[1], y, z);
        ROT_Y(ang[1], sc[2], x, z);
        v3_set(&w->pos2, x, y, z);
        i3_copy(w->ang2, ang);
        v3_add(&w->pos2, &w->pos);
    }
    pass_table_pop();
    w->arg = *prm;
    *(int *)&w->t1 = 0;
    *(int *)&w->t2 = 0;
}

/* 0x00b08068 */
void en_dominions_hole_release(EnDominionsHoleWork *w)
{
    dc_object_release(&w->body);
}

/* 0x00b08070 */
void en_dominions_hole_exec_close(EnDominionsHoleWork *w)
{
    w->wait -= enemy_status_get_ticks();
    if (w->wait < 0) {
        w->t1 -= w->body.speed * enemy_status_get_ticks();
        w->t1 = MAX_GE(w->t1, 0.0f);
        if (fabsf(w->t1) < FLT_EPSILON) {
            TASK_OF(w)->exec = 0;
            w->timer = *(float *)&((EnDominionsHoleSet *)w->desc)->v[0] * FPS;
            w->body.frame = 0.0f;
        }
    }
}

/* 0x00b08120 */
void en_dominions_hole_rewind(EnDominionsHoleWork *w)
{
    ModelPart *body = &w->body;
    float step = body->speed * enemy_status_get_ticks();

    if (!w->child) {
        w->t2 -= step;
        w->t2 = MAX_GE(w->t2, 0.0f);
        w->t1 -= step;
        w->t1 = MAX_GE(w->t1, 0.0f);
        if (fabsf(w->t1) < FLT_EPSILON && fabsf(w->t2) < FLT_EPSILON) {
            TASK_OF(w)->exec = 0;
            w->timer = *(float *)&((EnDominionsHoleSet *)w->desc)->v[0] * FPS;
            w->body.frame = 0.0f;
        }
    } else if (TASK_OF(w->child)->flags & TASK_F_END) {
        w->child = 0;
    } else {
        if (dominions_head_carrier_is_done(w->child)) {
            w->t1 -= step;
            w->t1 = MAX_GE(w->t1, 0.0f);
        }
        w->timer -= enemy_status_get_ticks();
        if (w->timer < 0) {
            w->t2 += step;
            w->t2 = MIN(w->t2, body->frames);
        }
    }
}

/* steer(cast) */
/* 0x00b082a0 */
void en_dominions_hole_exec_open(EnDominionsHoleWork *w)
{
    ModelPart *body = &w->body;
    int n = enemy_status_get_ticks();
    float u, v;
    int nu;

    while (n-- > 0)
        dc_object_advance_time(body);
    w->t1 = body->frame;
    if (fabsf(body->frames - body->frame) < FLT_EPSILON) {
        if (w->sel < 0) {
            dominions_head_shooter_create(w, &w->arg, &w->wait);
            TASK_OF(w)->exec = (void (*)(void *))en_dominions_hole_exec_close;
        } else {
            u = w->arg.param4;
            v = w->arg.param5;
            nu = ((float (*)(float))float_frac)(u) * 1000.0f + 0.5f;
            w->child = dominions_head_carrier_create(w, (ShotArg *)&w->arg, w->arg.n, w->sel, u, nu, v,
                                            ((float (*)(float))float_frac)(v) * 1000.0f + 0.5f, &w->timer, w->arg.speed);
            TASK_OF(w)->exec = (void (*)(void *))en_dominions_hole_rewind;
            w->timer = (int)(w->timer - body->frames / body->speed) - 30.0f;
        }
    }
}

/* 0x00b08450 */
void en_dominions_hole_exec(EnDominionsHoleWork *w)
{
    if (--w->timer < 0)
        TASK_OF(w)->exec = (void (*)(void *))en_dominions_hole_exec_open;
}

/* 0x00b08478 */
void en_dominions_hole_disp(EnDominionsHoleWork *w)
{
    Vec3 *p = &w->pos;

    ulGxSetTextureList(w->textures);
    if (view_sphere_visible(p, w->body.radius)) {
        ulMatrixPush();
        do {
            Vec3 *p_ = p;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        en_dominions_hole_rotate_yxz(w->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        model_object_draw(&w->body, w->t1);
        ulMatrixPop();
    }
    if (w->sel >= 0) {
        p = &w->pos2;
        if (view_sphere_visible(p, w->body.radius)) {
            ulMatrixPush();
            vu0_matrix_translate(ulMatrixCur, p->x, p->y, p->z);
            en_dominions_hole_rotate_yxz(w->ang2);
            vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
            model_object_draw(&w->body, w->t2);
            ulMatrixPop();
        }
    }
}

/* 0x00b08580 */
void *en_dominions_hole_create(void *parent, void *prm)
{
    return task_create_inherit_arg(parent, &en_dominions_hole_desc, 240, prm);
}

/* 0x00b085a8 */
void en_dominions_hole_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
