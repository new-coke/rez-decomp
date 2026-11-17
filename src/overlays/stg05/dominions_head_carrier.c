/*
 * Dominions head, Area 5: comes out of its hole, follows a stage-sequence course relative to the
 * player leading its body and tail (dominions_body_and_tail), then coasts away. Shooting it down
 * destroys the chain.
 */

#include "stg05.h"
#include "mathdef.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "nj_system.h"
#include "node_tree.h"
#include "pass.h"
#include "player.h"
#include "sound_midise.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "dominions_head_carrier.h"
#include "dominions_body_and_tail.h"
#include "../common/en_shatter.h"
#include "../common/enemy_vars.h"

extern int dominions_head_carrier_init_arg;
extern void (*dominions_head_carrier_exec_table[])(void *o, TypeEnemyWork *w);
extern void (*dominions_head_carrier_disp_table[])(void *o, TypeEnemyWork *w);

/* group name, label, 10 values */
typedef struct DominionsHeadCarrierSet {
    char *name;
    char *label;                /* 0x04 */
    int v[10];                  /* 0x08 */
} DominionsHeadCarrierSet;

extern DominionsHeadCarrierSet dominions_head_carrier_types[1];
extern VarDef dominions_head_carrier_vardef;
extern VarDef dominions_head_carrier_vardefs[10];

typedef struct DominionsHeadCarrierWork {
    struct DominionsHeadCarrierRec *type;
    int state;                  /* 0x04: indexes dominions_head_carrier_states */
    int count;                  /* 0x08: frames of state 0 */
    int wait;                   /* 0x0c: frames of state 3 */
    int life;                   /* 0x10: frames left */
    void *child;                /* 0x14: the first body of the chain (dominions_body_and_tail) */
    Vec3 pos;                   /* 0x18: the course origin */
    int ang[3];                 /* 0x24: the course angles */
    int n;                      /* 0x30: bodies in the chain */
    Vec3 head;                  /* 0x34: 10 ahead of the object along its angles */
    Vec3 hitv;                  /* 0x40: the lock-on record's vector when hit */
    float speed;                /* 0x4c: course position per frame */
    int seq;                    /* 0x50: the course sequence (dominions_head_carrier_path_eval) */
    float t;                    /* 0x54: course position 0..1 */
    Vec3 move;                  /* 0x58: the last move, kept in state 3 */
} DominionsHeadCarrierWork;

extern void (*dominions_head_carrier_states[])(void *o, TypeEnemyWork *w);
extern Vec3 dominions_head_carrier_funcs;

/* view of dominions_head_carrier_types */
typedef struct DominionsHeadCarrierRec {
    char *name;
    char *label;                /* 0x04 */
    int hp, same_lock, score;          /* 0x08 */
    float hp_reduce, crash_r, crash_damage;        /* 0x14 */
    float bullet_spd;                  /* 0x20 */
    float back;                 /* 0x24: start offset back along the course; 0 starts in state 1 */
    float rate;                  /* 0x28: the chain's blend, falling to 0 along it */
    float ang_rate;                  /* 0x2c: its second value */
} DominionsHeadCarrierRec;

/* placement; ObjDesc.kind points to it */
typedef struct DominionsHeadCarrierKind {
    int idx;
    int life;                   /* 0x04: frames */
    int n;                      /* 0x08: bodies in the chain */
    float speed;                /* 0x0c: course position per frame */
    float dist;                 /* 0x10: start speed along the placement angles */
    int count;                  /* 0x14 */
    int wait;                   /* 0x18 */
    int seq;                    /* 0x1c: the course sequence */
    int launch_start, child_param, launch_wait, child_seq;     /* 0x20: passed on to the bodies */
} DominionsHeadCarrierKind;

extern char dominions_head_carrier_lock_name[];
extern char dominions_head_carrier_name[];

/* 0x00b045b0 */
void dominions_head_carrier_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&dominions_head_carrier_vardef);
    for (i = 0; i < 1; i++) {
        def = dominions_head_carrier_vardefs;
        val = dominions_head_carrier_types[i].v;
        dominions_head_carrier_vardef.name = dominions_head_carrier_types[i].name;
        dominions_head_carrier_vardef.fmt = dominions_head_carrier_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&dominions_head_carrier_vardef);
        for (j = 0; j < 10; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)dominions_head_carrier_vardefs);
    }
}

/* 0x00b046c8 */
void dominions_head_carrier_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b046d0 */
int dominions_head_carrier_vars_init(int *prm)
{
    dominions_head_carrier_init_arg = *prm;
    dominions_head_carrier_vars_register();
    return 0;
}

/* 0x00b046f8 */
int dominions_head_carrier_vars_release(void)
{
    dominions_head_carrier_vars_unregister();
    return 0;
}

/* 0x00b04718 */
int dominions_head_carrier_get_init_arg(void)
{
    return dominions_head_carrier_init_arg;
}

/* 0x00b04728 */
int dominions_head_carrier_tail_raised(void *obj, DominionsHeadCarrierWork *w)
{
    Vec3 p;
    float sc[2][2];
    float x, y, z;

    dominions_body_and_tail_chain_last_pose(w->child, &p, 0);
    v3_sub(&p, &p, &w->pos);
    x = p.x;
    y = p.y;
    z = p.z;
    ROT_Y(-w->ang[1], sc[0], x, z);
    ROT_X(-w->ang[0], sc[1], y, z);
    return y > 10.0f;
}

/* 0x00b04818 */
void dominions_head_carrier_path_eval(void *out, int idx, float rate)
{
    pass_table_push(stage_sequence);
    sequence_eval(idx, (pass_table_end(idx) - 1.0f) * rate, 0, out, 0);
    pass_table_pop();
}

/* 0x00b04888 */
void dominions_head_carrier_forward(int *ang, Vec3 *out, float len)
{
    float sc[2][2];
    float x, y, z;

    z = len;
    y = 0.0f;
    x = y;
    ROT_X(ang[0], sc[0], y, z);
    ROT_Y(ang[1], sc[1], x, z);
    v3_set(out, x, y, z);
}

/* 0x00b04948 */
void dominions_head_carrier_calc_tip(void *obj, DominionsHeadCarrierWork *w)
{
    Vec3 p;
    Vec3 ofs;
    int ang[3];

    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    dominions_head_carrier_forward(ang, &ofs, 10.0f);
    v3_sum(&w->head, &p, &ofs);
}

/* 0x00b049e0 */
void dominions_head_carrier_face(Vec3 *to, Vec3 *from, int *ang)
{
    Vec3 dir;
    int rot[3];
    float sc[3][2];
    float x, y, z;

    v3_sub(&dir, to, from);
    x = dir.x;
    y = dir.y;
    z = dir.z;
    ROT_Y(-ang[1], sc[0], x, z);
    ROT_X(-ang[0], sc[1], y, z);
    ROT_Z(-ang[2], sc[2], x, y);
    dir.x = x;
    dir.y = y;
    dir.z = z;
    do {
        rot[1] = RAD_TO_ANG(atan2f(-dir.x, -dir.z));
        rot[0] = RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
    } while (0);
    ulMatrixPush();
    matrix_unit_set(0);
    dominions_head_carrier_rotate_yxz(ang);
    vu0_matrix_rotate_x(ulMatrixCur, rot[0]);
    vu0_matrix_rotate_y(ulMatrixCur, rot[1]);
    model_camera_angles(ang);
    ulMatrixPop();
}

/* 0x00b04bd8 */
void dominions_head_carrier_face_move(void *obj, int work, void *p)
{
    int ang[3];

    enemy_task_get_rot(obj, ang);
    dominions_head_carrier_face(p, &dominions_head_carrier_funcs, ang);
    enemy_task_set_rot(obj, ang);
}

/* 0x00b04c30 */
void dominions_head_carrier_init(void *obj, DominionsHeadCarrierWork *w, void *place, DominionsHeadCarrierKind *k)
{
    Vec3 pt;
    int ang[3];
    Vec3 v;
    Vec3 p;
    DominionsLinkArg prm;
    float fa[32];
    float fb[32];
    float sc[6][2];
    DominionsHeadCarrierRec *typ;
    int i;

    typ = (DominionsHeadCarrierRec *)&dominions_head_carrier_types[k->idx];
    w->type = typ;
    enemy_task_clear_flags(obj, 64);
    do {
        void *m_ = stage_models, *t_ = stage_textures;
        ((void **)obj)[0] = m_;
        ((void **)obj)[1] = t_;
    } while (0);
    w->state = fabsf(typ->back) < FLT_EPSILON;
    w->count = k->count;
    w->wait = k->wait;
    lockon_obj_set_name(enemy_task_lock_ptr(obj), dominions_head_carrier_lock_name);
    ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_SILENT;
    w->life = k->life;
    enemy_task_get_pos(obj, &w->pos);
    enemy_task_get_rot(obj, w->ang);
    w->t = 0;
    w->speed = k->speed;
    w->seq = k->seq;
    {
        float x, y, z;

        x = w->t;
        y = k->dist;
        z = x;
        enemy_task_get_rot(obj, ang);
        ROT_Z(ang[2], sc[0], x, y);
        ROT_X(ang[0], sc[1], y, z);
        ROT_Y(ang[1], sc[2], x, z);
        v.x = x;
        v.y = y;
        v.z = z;
    }
    enemy_task_set_param_vec(obj, &v);
    do {
        ang[1] = RAD_TO_ANG(atan2f(-v.x, -v.z));
        ang[0] = RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
    } while (0);
    ang[2] = 0;
    enemy_task_set_rot(obj, ang);
    dominions_head_carrier_path_eval(&p, w->seq, w->t);
    {
        float x, y, z;

        do {
            Vec3 *p_ = &p;

            x = p_->x;
            y = p_->y;
            z = p_->z;
        } while (0);
        ROT_Z(w->ang[2], sc[3], x, y);
        ROT_X(w->ang[0], sc[4], y, z);
        ROT_Y(w->ang[1], sc[5], x, z);
        p.x = x;
        p.y = y;
        p.z = z;
    }
    vec3_normalize(&v, &v);
    v3_scale(&v, -typ->back);
    v3_sum(&pt, &w->pos, &v);
    v3_add(&pt, &p);
    enemy_task_set_pos(obj, &pt);
    dominions_head_carrier_calc_tip(obj, w);
    w->n = k->n;
    prm.pos = &w->head;
    prm.ang = enemy_task_rot_ptr(obj);
    prm.pos3 = &w->pos;
    prm.ang3 = w->ang;
    prm.pos2 = &w->pos;
    prm.ang2 = w->ang;
    w->child = (void *)dominions_body_and_tail_create(obj, &prm, w->n, w->life, 0, k->launch_start, k->child_seq, k->child_param, k->launch_wait, 0, 0);
    for (i = 0; i < w->n; i++) {
        fa[i] = typ->rate * (1.0f - (float)i / (float)(w->n - 1));
        fb[i] = typ->ang_rate;
    }
    dominions_body_and_tail_chain_set_pair(w->child, fa, fb);
}

/* 0x00b05150 */
void dominions_head_carrier_release(void *obj, DominionsHeadCarrierWork *w)
{
    if (enemy_task_get_pstate(obj) != 3)
        dominions_body_and_tail_chain_end(w->child);
}

/* 0x00b05190 */
int dominions_head_carrier_state_enter(void *obj, DominionsHeadCarrierWork *w)
{
    Vec3 p;
    Vec3 cur;
    Vec3 v;
    Vec3 mv;

    enemy_task_get_param_vec(obj, &v);
    enemy_task_get_pos(obj, &cur);
    v3_sum(&p, &v, &cur);
    enemy_task_set_pos(obj, &p);
    v3_sub(&mv, &p, &cur);
    dominions_head_carrier_face_move(obj, (int)w, &mv);
    dominions_head_carrier_calc_tip(obj, w);
    dominions_body_and_tail_chain_update(w->child);
    if (--w->count < 0)
        w->state = 1;
    return 0;
}

/* 0x00b05280 */
int dominions_head_carrier_state_course(void *obj, DominionsHeadCarrierWork *w)
{
    Vec3 p;
    Vec3 cur;
    Vec3 mv;
    Vec3 v;
    float fa[32];
    float fb[32];
    float sc[3][2];
    DominionsHeadCarrierRec *typ = w->type;
    float x, y, z;
    int i;

    w->t += w->speed;
    w->t = MIN(w->t, 1.0f);
    if (fabsf(w->t - 1.0f) < FLT_EPSILON)
        w->state = 3;
    dominions_head_carrier_path_eval(&p, w->seq, w->t);
    do {
        x = p.x;
        y = p.y;
        z = p.z;
    } while (0);
    ROT_Z(w->ang[2], sc[0], x, y);
    ROT_X(w->ang[0], sc[1], y, z);
    ROT_Y(w->ang[1], sc[2], x, z);
    enemy_task_get_pos(obj, &cur);
    p.x = x;
    p.y = y;
    p.z = z;
    v3_add(&p, &w->pos);
    enemy_task_set_pos(obj, &p);
    v3_sub(&mv, &p, &cur);
    dominions_head_carrier_face_move(obj, (int)w, &mv);
    v3_sub(&v, &p, &cur);
    enemy_task_set_param_vec(obj, &v);
    dominions_head_carrier_calc_tip(obj, w);
    dominions_body_and_tail_chain_update(w->child);
    if (dominions_head_carrier_tail_raised(obj, w)) {
        for (i = 0; i < w->n; i++) {
            fa[i] = typ->rate;
            fb[i] = typ->ang_rate;
        }
        dominions_body_and_tail_chain_set_pair(w->child, fa, fb);
        w->state = 2;
    }
    return 0;
}

/* 0x00b05520 */
int dominions_head_carrier_state_course_turn(void *obj, DominionsHeadCarrierWork *w)
{
    Vec3 p;
    Vec3 cur;
    Vec3 mv;
    float sc[3][2];
    float x, y, z;
    int moving = 0;

    w->t += w->speed;
    if (w->t < 1.0f) {
        moving = 1;
    } else {
        w->t = 1.0f;
        w->state = 3;
    }
    dominions_head_carrier_path_eval(&p, w->seq, w->t);
    do {
        x = p.x;
        y = p.y;
        z = p.z;
    } while (0);
    ROT_Z(w->ang[2], sc[0], x, y);
    ROT_X(w->ang[0], sc[1], y, z);
    ROT_Y(w->ang[1], sc[2], x, z);
    enemy_task_get_pos(obj, &cur);
    p.x = x;
    p.y = y;
    p.z = z;
    v3_add(&p, &w->pos);
    enemy_task_set_pos(obj, &p);
    v3_sub(&mv, &p, &cur);
    dominions_head_carrier_face_move(obj, (int)w, &mv);
    if (moving)
        v3_sub(&w->move, &p, &cur);
    dominions_head_carrier_calc_tip(obj, w);
    dominions_body_and_tail_chain_update(w->child);
    return 0;
}

/* 0x00b05738 */
int dominions_head_carrier_state_coast(void *obj, DominionsHeadCarrierWork *w)
{
    Vec3 p;
    Vec3 cur;
    Vec3 mv;

    enemy_task_get_pos(obj, &cur);
    v3_sum(&p, &w->move, &cur);
    enemy_task_set_pos(obj, &p);
    v3_sub(&mv, &p, &cur);
    dominions_head_carrier_face_move(obj, (int)w, &mv);
    dominions_head_carrier_calc_tip(obj, w);
    dominions_body_and_tail_chain_update(w->child);
    if (w->wait-- < 0)
        FINISH_IF(obj);
    return 0;
}

/* 0x00b05830 */
void dominions_head_carrier_update(void *obj, TypeEnemyWork *w)
{
    void (*fn)(void *obj, TypeEnemyWork *w) = dominions_head_carrier_states[w->state];

    if (fn)
        fn(obj, w);
}

/* 0x00b05870 */
void dominions_head_carrier_exec_appear(void *obj, DominionsHeadCarrierWork *w)
{
    if (enemy_task_get_phase(obj) == 2) {
        ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_LOCKABLE;
        return;
    }
    dominions_head_carrier_update(obj, (TypeEnemyWork *)w);
    if (--w->life <= 0)
        enemy_task_set_state(obj, 2);
}

/* 0x00b058e8 */
void dominions_head_carrier_exec_normal(void *obj, DominionsHeadCarrierWork *w)
{
    if (enemy_task_get_phase(obj) != 1)
        return;
    dominions_head_carrier_update(obj, (TypeEnemyWork *)w);
    if (--w->life <= 0)
        enemy_task_set_state(obj, 2);
}

/* 0x00b05950 */
void dominions_head_carrier_exec_leave(void *obj, TypeEnemyWork *w)
{
    if (enemy_task_get_phase(obj) == 1)
        dominions_head_carrier_update(obj, w);
}

/* 0x00b059a0 */
void dominions_head_carrier_exec_destroyed(void *obj, DominionsHeadCarrierWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 0) {
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 4;
        desc.models = stage_models;
        desc.kind = 0;
        desc.unk_50 = 0;
        desc.lock = enemy_task_lock_ptr(obj);
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        v3_copy(&desc.v, &w->hitv);
        en_shatter_spawn(obj, &desc, 6);
        dominions_body_and_tail_chain_destroy(w->child, 3, 3);
        FINISH_IF(obj);
    }
}

/* 0x00b05a80 */
void dominions_head_carrier_pre_exec(void *obj)
{
    Vec3 p, org;

    player_get_frame_vel(&org);
    enemy_task_get_pos(obj, &p);
    p.x -= org.x;
    p.y -= org.y;
    p.z -= org.z;
    enemy_task_set_pos(obj, &p);
}

/* 0x00b05af0 */
void dominions_head_carrier_exec(void *obj, TypeEnemyWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (dominions_head_carrier_exec_table[i])
        dominions_head_carrier_exec_table[i](obj, w);
}

/* steer(cast) */
/* 0x00b05b48 */
void dominions_head_carrier_post_exec(void *obj, DominionsHeadCarrierWork *w)
{
    Vec3 p;
    Vec3 ofs;
    SpawnDesc desc;
    Vec3 vpos;
    float sc[3][2];
    float x, y, z;
    LockObj *lock = enemy_task_lock_ptr(obj);

    enemy_task_get_pos(obj, &p);
    v3_sub(&ofs, &p, &w->pos);
    do {
        Vec3 *v_ = &ofs;
        x = v_->x;
        z = v_->z;
        y = v_->y;
    } while (0);
    ROT_Y(-w->ang[1], sc[0], x, z);
    ROT_X(-w->ang[0], sc[1], y, z);
    ROT_Z(-w->ang[2], sc[2], x, y);
    if (y < 0.0f)
        lock->flags &= ~LOCK_LOCKABLE;
    else
        lock->flags |= LOCK_LOCKABLE;
    if (lock->flags & LOCK_HIT) {
        v3_copy(&w->hitv, &lock->imp.v);
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 105;
        desc.models = stage_models;
        desc.kind = 0;
        desc.unk_50 = 0;
        desc.lock = enemy_task_lock_ptr(obj);
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        v3_copy(&desc.v, &w->hitv);
        en_shatter_spawn(obj, &desc, 6);
        enemy_task_get_pos(obj, &vpos);
        ulMatrixApplyPoint((float *)ulMatrixCur, &vpos.x, &vpos.x);
        project_point(&vpos, &vpos);
        switch (lock->life) {
        case 0:
            SoundRhythmSe_EnemyPos(dominions_head_carrier_init_arg, 1, &vpos.x);
            break;
        case 1:
            SoundRhythmSe_EnemyPos(dominions_head_carrier_init_arg, 7, &vpos.x);
            break;
        case 2:
            SoundRhythmSe_EnemyPos(dominions_head_carrier_init_arg, 6, &vpos.x);
            break;
        case 3:
            SoundRhythmSe_EnemyPos(dominions_head_carrier_init_arg, 5, &vpos.x);
            break;
        case 4:
            SoundRhythmSe_EnemyPos(dominions_head_carrier_init_arg, 4, &vpos.x);
            break;
        case 5:
            SoundRhythmSe_EnemyPos(dominions_head_carrier_init_arg, 3, &vpos.x);
            break;
        case 6:
            SoundRhythmSe_EnemyPos(dominions_head_carrier_init_arg, 2, &vpos.x);
            break;
        case 7:
            SoundRhythmSe_EnemyPos(dominions_head_carrier_init_arg, 0, &vpos.x);
            break;
        }
    }
    if (lock->life <= 0 && ((unsigned int)enemy_task_get_pstate(obj)) < 2)
        enemy_task_set_state(obj, 3);
}

/* 0x00b05e90 */
void dominions_head_carrier_disp(void *obj, TypeEnemyWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (dominions_head_carrier_disp_table[i])
        dominions_head_carrier_disp_table[i](obj, w);
}

/* 0x00b05ee8 */
void *dominions_head_carrier_create(void *obj, ShotArg *sarg, int n, int seq, int mode, int k2c, int launch_b, int wait, int *timer, float spd)
{
    DominionsHeadCarrierKind k;
    ObjDesc desc;
    ObjFuncs fn = { { dominions_head_carrier_init, dominions_head_carrier_release, dominions_head_carrier_exec,
                      dominions_head_carrier_disp, dominions_head_carrier_pre_exec, dominions_head_carrier_post_exec } };
    Vec3 p;
    Vec3 pt;
    DominionsHeadCarrierRec *typ;
    float speed = spd;
    float dist, fr;

    k.n = MAX(n, 2);
    k.n = k.n < 31 ? k.n : 30;
    k.idx = 0;
    k.seq = seq;
    speed = MIN(MAX_GE(speed / FPS * 0.01f, 1.0000001e-05f), 0.1f);
    typ = (DominionsHeadCarrierRec *)&dominions_head_carrier_types[k.idx];
    k.speed = speed;
    k.launch_start = mode;
    k.child_seq = k2c;
    k.child_param = launch_b;
    k.launch_wait = wait;
    dominions_head_carrier_path_eval(&p, seq, 0.0f);
    dominions_head_carrier_path_eval(&pt, seq, speed);
    k.dist = ulVec3Distance(&p, &pt);
    k.count = typ->back / k.dist;
    dominions_head_carrier_path_eval(&p, seq, 1.0f);
    dominions_head_carrier_path_eval(&pt, seq, 1.0f - speed);
    dist = ulVec3Distance(&p, &pt);
    fr = 1.0f / speed;
    *timer = k.count + fr;
    k.wait = n * 10.0f / dist + 10.0f;
    k.life = *timer + k.wait;
    sarg->kind = dominions_head_carrier_init_arg;
    nj_fill8(&desc, 0, sizeof desc);
    desc.name = dominions_head_carrier_name;
    desc.funcs = &fn;
    desc.arg = sarg;
    desc.kind = &k.idx;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->hp;
    desc.same_lock = typ->same_lock;
    desc.flags = 16;
    desc.dc_flags = 0;
    desc.model_no = 2;
    desc.model_no2 = -1;
    desc.score = typ->score;
    desc.hp_reduce = typ->hp_reduce;
    desc.crash_r = typ->crash_r;
    desc.crash_damage = typ->crash_damage;
    desc.time = k.life;
    desc.size = 100;
    return (void *)enemy_task_task_create(obj, &desc);
}

/* 0x00b061f8 */
int dominions_head_carrier_is_done(void *obj)
{
    return ((int *)enemy_task_work_ptr(obj))[1] >= 2;
}

/* 0x00b06220 */
void dominions_head_carrier_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
