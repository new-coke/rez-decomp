/*
 * Dominions body and tail, Area 5: segments that each steer after the one ahead, tail at the end.
 * Segments are lock-on targets whose launchers open in turn to send out DominionsChild craft;
 * the chain blows up segment by segment when the head dies.
 */

#include "stg05.h"
#include "mathdef.h"
#include "approach.h"
#include "debug.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "fog.h"
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
#include "rez_wire_compile.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "dominions_body_and_tail.h"
#include "dominions_child.h"
#include "../common/en_shatter.h"
#include "../common/enemy_vars.h"

extern int dominions_body_and_tail_init_arg;
extern void (*dominions_body_and_tail_exec_table[])(void *o, TypeEnemyWork *w);
extern void (*dominions_body_and_tail_disp_table[])(void *o, TypeEnemyWork *w);
extern DominionsBodyAndTailSet dominions_body_and_tail_types[2];
extern VarDef dominions_body_and_tail_vardef;
extern VarDef dominions_body_and_tail_vardefs[11];

typedef struct DominionsBodyAndTailWork DominionsBodyAndTailWork;

/* 40 bytes; ObjDesc.kind points to it */
typedef struct DominionsBodyAndTailKind {
    int idx;
    DominionsLinkArg *link;           /* 0x04 */
    int n;                      /* 0x08: bodies still to create after this one */
    int life;                   /* 0x0c */
    int i;                      /* 0x10 */
    int tail;                   /* 0x14 */
    int launch_start, child_seq, child_param, launch_wait;     /* 0x18 */
} DominionsBodyAndTailKind;

extern char dominions_body_name[];
extern char dominions_tail_name[];

/* 448 bytes */
struct DominionsBodyAndTailWork {
    DominionsBodyAndTailSet *type;
    DominionsLinkArg link;            /* 0x04: from the creation record */
    char pad_1c[4];
    void *sub;                  /* 0x20: its DominionsChild launcher, ended with it */
    void *next;                 /* 0x24: the next body of the chain */
    LockObj lock;                  /* 0x28 */
    Vec3 hitv;                  /* 0x68: the lock-on record's vector when hit */
    Vec3 fore;                  /* 0x74: 5 ahead along its angles */
    Vec3 back;                  /* 0x80: 5 behind */
    Vec3 start;                 /* 0x8c: the leader's last position */
    int launch_time;                    /* 0x98: set by dominions_body_and_tail_chain_destroy */
    int life;                   /* 0x9c: frames left */
    float blend;                /* 0xa0: weight of the leader's direction */
    void *unk_a4;               /* 0xa4: set to 0 by dominions_body_and_tail_init, never read */
    int unk_a8;                 /* 0xa8: set to 0 by dominions_body_and_tail_init, never read */
    ModelPart body;                /* 0xac */
    int child_seq;                    /* 0x108 */
    int child_param;                    /* 0x10c */
    int mode;                   /* 0x110: launcher state; -1 when destroyed */
    int wait;                   /* 0x114: frames before the launcher opens */
    int count;                  /* 0x118 */
    ShotArg arg;                /* 0x11c: the creation argument */
    int last;                   /* 0x15c: the last body of the chain */
    float glow;                 /* 0x160: draws body2 and scales up when not 0 */
    ModelPart body2;               /* 0x164 */
};

extern char dominions_body_lock_name[];
extern char dominions_tail_lock_name[];

/* 0x00b029d8 */
void dominions_body_and_tail_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&dominions_body_and_tail_vardef);
    for (i = 0; i < 2; i++) {
        def = dominions_body_and_tail_vardefs;
        val = dominions_body_and_tail_types[i].v;
        dominions_body_and_tail_vardef.name = dominions_body_and_tail_types[i].name;
        dominions_body_and_tail_vardef.fmt = dominions_body_and_tail_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&dominions_body_and_tail_vardef);
        for (j = 0; j < 11; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)dominions_body_and_tail_vardefs);
    }
}

/* 0x00b02af0 */
void dominions_body_and_tail_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b02af8 */
int dominions_body_and_tail_vars_init(int *prm)
{
    dominions_body_and_tail_init_arg = *prm;
    dominions_body_and_tail_vars_register();
    return 0;
}

/* 0x00b02b20 */
int dominions_body_and_tail_vars_release(void)
{
    dominions_body_and_tail_vars_unregister();
    return 0;
}

/* 0x00b02b40 */
void dominions_body_and_tail_forward(int *ang, Vec3 *out, float len)
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

/* 0x00b02c00 */
void dominions_body_and_tail_calc_tips(void *obj, void *work)
{
    DominionsBodyAndTailWork *w = work;
    Vec3 p;
    Vec3 ofs;
    int ang[3];

    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    dominions_body_and_tail_forward(ang, &ofs, 5.0f);
    v3_sum(&w->fore, &p, &ofs);
    v3_sub(&w->back, &p, &ofs);
}

/* steer(cast) */
/* 0x00b02cc8 */
void dominions_body_and_tail_init(void *obj, DominionsBodyAndTailWork *w, ShotArg *place, DominionsBodyAndTailKind *k)
{
    Vec3 p;
    Vec3 ofs;
    DominionsLinkArg prm;
    DominionsLinkArg *lnk = k->link;
    DominionsBodyAndTailSet *typ;
    ModelPart *gfx;

    do {
        void *m_ = stage_models, *t_ = stage_textures;
        ((void **)obj)[0] = m_;
        ((void **)obj)[1] = t_;
    } while (0);
    enemy_task_clear_flags(obj, 64);
    typ = &dominions_body_and_tail_types[k->idx];
    w->type = typ;
    w->link = *lnk;
    w->last = k->n < 2 && !k->tail;
    lockon_obj_init(&w->lock, obj, enemy_task_pos_ptr(obj), 0, w->last ? dominions_tail_lock_name : dominions_body_lock_name, enemy_task_get_status_slot(obj),
                  typ->v[0], typ->v[1], 128);
    lockon_entry(&w->lock);
    w->lock.flags &= ~LOCK_LOCKABLE;
    dominions_body_and_tail_forward(lnk->ang, &ofs, !k->i ? 15.0f : 10.0f);
    v3_sum(&p, lnk->pos, &ofs);
    enemy_task_set_pos(obj, &p);
    enemy_task_set_rot(obj, lnk->ang);
    dominions_body_and_tail_calc_tips(obj, w);
    if (w->last) {
        dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(((void **)obj)[0], 13)), 0, 1280);
        dc_object_set_wire(&w->body, model_pnb_get_wire(((void **)obj)[0], 13));
    } else {
        dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(((void **)obj)[0], 9)), 0, 1280);
        dc_object_set_wire(&w->body, model_pnb_get_wire(((void **)obj)[0], 9));
        gfx = enemy_task_gfx_ptr(obj);
        gfx->radius = w->body.radius = 27.0f;
    }
    w->life = k->life;
    *(int *)&w->blend = 0;
    w->unk_a4 = 0;
    w->next = 0;
    if (--k->n > 0) {
        prm.pos = &w->fore;
        prm.ang = enemy_task_rot_ptr(obj);
        prm.pos2 = lnk->pos2;
        prm.ang2 = lnk->ang2;
        prm.pos3 = lnk->pos3;
        prm.ang3 = lnk->ang3;
        w->next = (void *)dominions_body_and_tail_create(obj, &prm, k->n, k->life, k->i + 1, k->launch_start, k->child_seq, k->child_param, k->launch_wait, k->tail,
                                               k->idx);
    }
    w->unk_a8 = 0;
    w->count = 0;
    v3_copy(&w->start, lnk->pos);
    w->sub = 0;
    w->arg = *place;
    *(int *)&w->glow = 0;
    dc_object_init(&w->body2, *((struct DcNode **)pnb_get_entry(stage_models, 8)), 0, 0);
    dc_object_rebuild(&w->body2);
    if (k->launch_start > 0 && k->i < typ->v[10]) {
        float step = *(float *)&typ->v[9] * FPS;

        w->mode = k->launch_start + step * k->i;
        w->wait = k->launch_wait - step * k->i;
    } else {
        w->wait = 0;
        w->mode = -1;
    }
    w->child_seq = k->child_seq;
    w->child_param = k->child_param;
}

/* 0x00b03160 */
void dominions_body_and_tail_release(void *obj, DominionsBodyAndTailWork *w)
{
    dc_object_release(&w->body2);
    dc_object_release(&w->body);
    lockon_remove(&w->lock);
    if (w->sub)
        FINISH(w->sub);
}

/* 0x00b031b0 */
void dominions_body_and_tail_aim(void *obj, int *w)
{
    int ang[3];
    Vec3 p;
    int *lnk = &w[1];

    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    yuki_draw_aimed((Vec3 *)lnk[0], &p, ang);
    ang[2] = ang_approach(((int *)lnk[1])[2], ang[2], 0.99f);
    enemy_task_set_rot(obj, ang);
}

/* 0x00b03248 */
void dominions_body_and_tail_lock_side_test(void *obj, void *work)
{
    DominionsBodyAndTailWork *w = work;
    Vec3 p;
    Vec3 ofs;
    float sc[3][2];
    Vec3 *lpos;
    int *lang;
    float x, y, z;
    float da;

    enemy_task_get_pos(obj, &p);
    da = vec3_distance_squared(w->link.pos2, &p);
    if (da < vec3_distance_squared(w->link.pos3, &p)) {
        lpos = w->link.pos2;
        lang = w->link.ang2;
    } else {
        lpos = w->link.pos3;
        lang = w->link.ang3;
    }
    v3_sub(&ofs, &p, lpos);
    do {
        Vec3 *v_ = &ofs;
        x = v_->x;
        z = v_->z;
        y = v_->y;
    } while (0);
    ROT_Y(-lang[1], sc[0], x, z);
    ROT_X(-lang[0], sc[1], y, z);
    ROT_Z(-lang[2], sc[2], x, y);
    if (y < 0.0f)
        w->lock.flags &= ~LOCK_LOCKABLE;
    else
        w->lock.flags |= LOCK_LOCKABLE;
}

/* 0x00b033c0 */
void dominions_body_and_tail_launcher_update(void *obj, DominionsBodyAndTailWork *w)
{
    DominionsChildLaunch launch;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    float end = gfx->frames;
    float frm_step = gfx->speed;
    float frm = gfx->frame;

    switch (w->mode) {
    case 0:
        frm -= frm_step;
        gfx->frame = 0.0f <= frm ? frm : 0.0f;
        break;
    case 1:
        if (++w->count > 60.0f)
            w->mode = 0;
        break;
    case 2:
        frm += frm_step;
        frm = MIN(frm, end);
        gfx->frame = frm;
        if (fabsf(frm - end) < FLT_EPSILON) {
            w->mode = 1;
            dominions_child_launch(w->sub);
            w->sub = 0;
        }
        break;
    default:
        if (--w->mode == 2) {
            launch.pos = enemy_task_pos_ptr(obj);
            launch.ang = enemy_task_rot_ptr(obj);
            launch.seq = w->child_seq;
            launch.param = w->child_param;
            launch.wait = w->wait;
            w->sub = dominions_child_create(obj, &w->arg, &launch);
        }
        break;
    }
}

/* 0x00b03528 */
void dominions_body_and_tail_follow_dir(void *obj, void *work)
{
    DominionsBodyAndTailWork *w = work;
    Vec3 p;
    Vec3 dir;
    int ang[3];
    Vec3 back;
    Vec3 fwd;
    DominionsLinkArg *lnk = &w->link;

    v3_sub(&dir, lnk->pos, &w->start);
    enemy_task_get_rot(obj, ang);
    if (v3_is_zero(&dir)) {
        dominions_body_and_tail_forward(ang, &dir, 5.0f);
    } else {
        vec3_normalize(&dir, &dir);
        v3_mul(&back, &dir, -5.0f);
        dominions_body_and_tail_forward(ang, &fwd, 5.0f);
        dir.x = back.x * w->blend + fwd.x * (1.0f - w->blend);
        dir.y = back.y * w->blend + fwd.y * (1.0f - w->blend);
        dir.z = back.z * w->blend + fwd.z * (1.0f - w->blend);
        vec3_normalize(&dir, &dir);
        v3_scale(&dir, 5.0f);
    }
    v3_copy(&w->start, lnk->pos);
    v3_sum(&p, lnk->pos, &dir);
    enemy_task_set_pos(obj, &p);
    enemy_task_set_param_vec(obj, &dir);
    dominions_body_and_tail_aim(obj, (int *)w);
    dominions_body_and_tail_calc_tips(obj, w);
    dominions_body_and_tail_lock_side_test(obj, w);
    if (w->mode >= 0)
        dominions_body_and_tail_launcher_update(obj, w);
}

/* 0x00b037b0 */
void dominions_body_and_tail_exec_appear(void *obj, DominionsBodyAndTailWork *w)
{
    int n = enemy_task_get_phase(obj);

    if (!n || n == 2)
        return;
    if (--w->life <= 0)
        enemy_task_set_state(obj, 2);
}

/* 0x00b03810 */
void dominions_body_and_tail_exec_normal(void *obj, DominionsBodyAndTailWork *w)
{
    LockObj *lock = &w->lock;

    if (enemy_task_get_phase(obj) != 1)
        return;
    if (--w->life <= 0) {
        lock->flags &= ~LOCK_LOCKABLE;
        enemy_task_set_state(obj, 2);
    }
}

/* 0x00b03880 */
int dominions_body_and_tail_exec_leave(void *obj)
{
    return enemy_task_get_phase(obj);
}

/* 0x00b038a0 */
void dominions_body_and_tail_exec_destroyed(void *obj, DominionsBodyAndTailWork *w)
{
    Vec3 p;
    Vec3 v;
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 1) {
        enemy_task_get_param_vec(obj, &v);
        if (!v3_is_zero(&v)) {
            enemy_task_get_pos(obj, &p);
            vec3_normalize(&v, &v);
            v3_scale(&v, *(float *)&w->type->v[8] * (1.0f / FPS));
            v3_add(&p, &v);
            enemy_task_set_pos(obj, &p);
        }
        if (enemy_task_get_state_time(obj) > w->launch_time) {
            nj_fill8(&desc, 0, sizeof desc);
            desc.models = stage_models;
            desc.type = w->last ? 12 : 7;
            desc.lock = &w->lock;
            desc.kind = 0;
            desc.unk_50 = 0;
            enemy_task_get_pos(obj, &desc.pos);
            enemy_task_get_rot(obj, desc.ang);
            enemy_task_get_param_vec(obj, &desc.v);
            enemy_task_get_scl(obj, &desc.scale);
            en_shatter_spawn(obj, &desc, 6);
            FINISH_IF(obj);
        }
    }
}

/* 0x00b03a80 */
void dominions_body_and_tail_exec(void *obj, TypeEnemyWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (dominions_body_and_tail_exec_table[i])
        dominions_body_and_tail_exec_table[i](obj, w);
}

/* 0x00b03ad8 */
void dominions_body_and_tail_pre_exec(void *obj)
{
    Vec3 p, org;

    enemy_task_get_pos(obj, &p);
    player_get_frame_vel(&org);
    p.x -= org.x;
    p.y -= org.y;
    p.z -= org.z;
    enemy_task_set_pos(obj, &p);
}

/* 0x00b03b48 */
void dominions_body_and_tail_post_exec(void *obj, DominionsBodyAndTailWork *w)
{
    SpawnDesc desc;
    Vec3 p;
    Vec3 vpos;
    LockObj *lock = &w->lock;
    DominionsBodyAndTailSet *typ;
    int n;

    if (lock->flags & LOCK_HIT) {
        v3_copy(&w->hitv, &lock->imp.v);
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 105;
        desc.models = stage_models;
        desc.kind = 0;
        desc.unk_50 = 0;
        desc.lock = lock;
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        v3_copy(&desc.v, &w->hitv);
        en_shatter_spawn(obj, &desc, 6);
        if (lock->life <= 0) {
            n = lockon_hits_max_shot_cnt(lock);
            typ = w->type;
            enemy_task_get_pos(obj, &p);
            enemy_status_score_popup(enemy_task_get_status_slot(obj), &p, typ->v[2], n);
            w->mode = -1;
            if (w->sub) {
                dominions_child_destroy(w->sub);
                w->sub = 0;
            }
        }
        enemy_task_get_pos(obj, &vpos);
        ulMatrixApplyPoint((float *)ulMatrixCur, &vpos.x, &vpos.x);
        project_point(&vpos, &vpos);
        switch (lock->life) {
        case 0:
            SoundRhythmSe_EnemyPos(dominions_body_and_tail_init_arg, 1, &vpos.x);
            break;
        case 1:
            SoundRhythmSe_EnemyPos(dominions_body_and_tail_init_arg, 7, &vpos.x);
            break;
        case 2:
            SoundRhythmSe_EnemyPos(dominions_body_and_tail_init_arg, 6, &vpos.x);
            break;
        case 3:
            SoundRhythmSe_EnemyPos(dominions_body_and_tail_init_arg, 5, &vpos.x);
            break;
        case 4:
            SoundRhythmSe_EnemyPos(dominions_body_and_tail_init_arg, 4, &vpos.x);
            break;
        case 5:
            SoundRhythmSe_EnemyPos(dominions_body_and_tail_init_arg, 3, &vpos.x);
            break;
        case 6:
            SoundRhythmSe_EnemyPos(dominions_body_and_tail_init_arg, 2, &vpos.x);
            break;
        case 7:
            SoundRhythmSe_EnemyPos(dominions_body_and_tail_init_arg, 0, &vpos.x);
            break;
        }
    }
}

/* 0x00b03dd8 */
void dominions_body_and_tail_disp_body(void *obj, DominionsBodyAndTailWork *w)
{
    Vec3 p;
    int ang[3];
    Vec3 scl;
    float col_add[4];
    float k = w->glow * 0.1f + 1.0f;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    void *fog;
    int n;

    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    enemy_task_get_scl(obj, &scl);
    if (view_sphere_visible(&p, gfx->radius)) {
        ulGxSetTextureList(((void **)obj)[1]);
        do {
            ulMatrixPush();
        } while (0);
        n = w->lock.life;
        vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
        dominions_body_and_tail_rotate_yxz(ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
        vu0_matrix_scale(ulMatrixCur, k, k, k);
        if (n <= 0) {
            fog = fog_set_enable(0);
            draw_set_color(w->type->v[7]);
            draw_set_blend(4, 1);
            model_object_draw(&w->body, gfx->frame);
            fog_set_enable(fog);
        } else {
            dc_object_draw(gfx);
        }
        if (!(fabsf(w->glow) < FLT_EPSILON)) {
            n = draw_set_override_flags(16);
            heap_bzero(col_add, 16);
            col_add[2] = 0;
            col_add[1] = col_add[0] = 1.0f;
            col_add[3] = 0;
            draw_push_override_color();
            draw_set_override_color(col_add);
            dc_object_draw(&w->body2);
            draw_pop_override_color();
            draw_set_override_flags(n);
        }
        ulMatrixPop();
    }
}

/* 0x00b03fb8 */
void dominions_body_and_tail_disp(void *obj, TypeEnemyWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (dominions_body_and_tail_disp_table[i])
        dominions_body_and_tail_disp_table[i](obj, w);
}

/* 0x00b04010 */
int dominions_body_and_tail_create(void *obj, DominionsLinkArg *prm, int n, int life, int i, int mode, int k1c, int launch_b, int wait, int tail, int sel)
{
    DominionsBodyAndTailKind k;
    ObjDesc desc;
    ObjFuncs f = { { dominions_body_and_tail_init, dominions_body_and_tail_release, dominions_body_and_tail_exec,
                     dominions_body_and_tail_disp, dominions_body_and_tail_pre_exec, dominions_body_and_tail_post_exec } };
    ShotArg sarg;
    DominionsBodyAndTailSet *typ;

    k.idx = MAX(sel, 0) != 0;
    typ = &dominions_body_and_tail_types[k.idx];
    k.link = prm;
    k.n = n;
    k.life = life;
    k.i = i;
    k.tail = tail;
    k.launch_wait = wait;
    k.launch_start = i < typ->v[10] ? mode : 0;
    k.child_seq = k1c;
    k.child_param = launch_b;
    nj_fill8(&sarg, 0, sizeof sarg);
    sarg.kind = dominions_body_and_tail_init_arg;
    sarg.fx_arg = -1;
    sarg.pos = *prm->pos;
    *(ThronesCoreAng *)sarg.ang = *(ThronesCoreAng *)prm->ang;
    nj_fill8(&desc, 0, sizeof desc);
    desc.kind = &k.idx;
    desc.name = n >= 2 || tail ? dominions_body_name : dominions_tail_name;
    desc.funcs = &f;
    desc.arg = &sarg;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->v[0];
    desc.same_lock = typ->v[1];
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = n >= 2 || tail ? 5 : 11;
    desc.model_no2 = n >= 2 || tail ? 6 : -1;
    desc.score = typ->v[2];
    desc.hp_reduce = *(float *)&typ->v[3];
    desc.crash_r = n >= 2 || tail ? *(float *)&typ->v[4] : *(float *)&typ->v[5];
    desc.crash_damage = *(float *)&typ->v[6];
    desc.time = life;
    desc.size = 448;
    return enemy_task_task_create(TASK_OF(obj)->parent ? TASK_OF(obj)->parent + 1 : 0, &desc);
}

/* steer(cast) */
/* 0x00b04270 */
void dominions_body_and_tail_chain_destroy(void *obj, int v, int step)
{
    int *sub = enemy_task_work_ptr(obj);

    if (((unsigned int)enemy_task_get_pstate(obj)) < 2)
        enemy_task_set_state(obj, 3);
    sub[38] = v;
    if (sub[9])
        dominions_body_and_tail_chain_destroy((void *)sub[9], v + step, step);
}

/* 0x00b042f0 */
void dominions_body_and_tail_chain_end(void *obj)
{
    int *sub = enemy_task_work_ptr(obj);

    FINISH_IF(obj);
    if (sub[9])
        dominions_body_and_tail_chain_end((void *)sub[9]);
}

/* 0x00b04340 */
void dominions_body_and_tail_chain_set_pair(void *obj, float *blend, float *ang_rate)
{
    if (obj) {
        float *sub = enemy_task_work_ptr(obj);

        sub[40] = *blend;
        sub[41] = *ang_rate;
        if (((void **)sub)[9])
            dominions_body_and_tail_chain_set_pair(((void **)sub)[9], blend, ang_rate);
    }
}

/* 0x00b04398 */
void dominions_body_and_tail_chain_update(void *obj)
{
    void **sub;

loop:
    if (obj) {
        sub = enemy_task_work_ptr(obj);
        dominions_body_and_tail_follow_dir(obj, sub);
        obj = sub[9];
        goto loop;
    }
}

/* 0x00b043e8 */
void dominions_body_and_tail_chain_last_pose(void *obj, Vec3 *pos, int *ang)
{
    void *n;

loop:
    if (obj) {
        n = ((void **)enemy_task_work_ptr(obj))[9];
        if (n) {
            obj = n;
            goto loop;
        }
        if (pos)
            enemy_task_get_pos(obj, pos);
        if (ang)
            enemy_task_get_rot(obj, ang);
    }
}

/* 0x00b04478 */
void dominions_body_and_tail_chain_place(void *obj, Vec3 *pos, int *ang)
{
    if (obj) {
        void **sub = enemy_task_work_ptr(obj);

        enemy_task_set_pos(obj, pos);
        enemy_task_set_rot(obj, ang);
        dominions_body_and_tail_calc_tips(obj, sub);
        dominions_body_and_tail_lock_side_test(obj, sub);
        if (sub[9])
            dominions_body_and_tail_chain_place(sub[9], pos - 1, ang - 3);
    }
}

/* 0x00b04518 */
void dominions_body_and_tail_chain_set_rate(void *obj, float *v)
{
    if (obj) {
        float *sub = enemy_task_work_ptr(obj);

        sub[88] = *v;
        if (((void **)sub)[9])
            dominions_body_and_tail_chain_set_rate(((void **)sub)[9], v + 1);
    }
}

/* 0x00b04560 */
void dominions_body_and_tail_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
