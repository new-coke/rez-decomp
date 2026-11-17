/*
 * Dominions shooter head, Area 5: rises out of a hole with a short chain, follows the path, waves
 * the chain, throws a bomb (dominions_bomb), aims, fires and retracts.
 */

#include "stg05.h"
#include "mathdef.h"
#include "anim.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "nj_system.h"
#include "node_tree.h"
#include "player.h"
#include "sound_midise.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "dominions_head_shooter.h"
#include "dominions_body_and_tail.h"
#include "dominions_bomb.h"
#include "dominions_head_carrier.h"
#include "../common/en_shatter.h"
#include "../common/enemy_vars.h"

extern int dominions_head_shooter_init_arg;
extern void (*dominions_head_shooter_exec_table[])(void *o, TypeEnemyWork *w);
extern void (*dominions_head_shooter_disp_table[])(void *o, TypeEnemyWork *w);

/* group name, label, 14 values */
typedef struct DominionsHeadShooterSet {
    char *name;
    char *label;                /* 0x04 */
    int v[14];                  /* 0x08 */
} DominionsHeadShooterSet;

extern DominionsHeadShooterSet dominions_head_shooter_types[1];
extern VarDef dominions_head_shooter_vardef;
extern VarDef dominions_head_shooter_vardefs[14];
extern char dominions_head_shooter_lock_name[];
extern char dominions_head_shooter_name[];
extern char dominions_head_shooter_funcs[];
extern int (*dominions_head_shooter_states[])(void *o, DominionsHeadShooterWork *w);

/* ObjDesc.kind points to it */
typedef struct DominionsHeadShooterKind {
    int idx;
    int life;                   /* 0x04: frames of all its states */
} DominionsHeadShooterKind;

/* 0x00b085f8 */
void dominions_head_shooter_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&dominions_head_shooter_vardef);
    for (i = 0; i < 1; i++) {
        def = dominions_head_shooter_vardefs;
        val = dominions_head_shooter_types[i].v;
        dominions_head_shooter_vardef.name = dominions_head_shooter_types[i].name;
        dominions_head_shooter_vardef.fmt = dominions_head_shooter_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&dominions_head_shooter_vardef);
        for (j = 0; j < 14; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)dominions_head_shooter_vardefs);
    }
}

/* 0x00b08710 */
void dominions_head_shooter_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b08718 */
int dominions_head_shooter_vars_init(int *prm)
{
    dominions_head_shooter_init_arg = *prm;
    dominions_head_shooter_vars_register();
    return 0;
}

/* 0x00b08740 */
int dominions_head_shooter_vars_release(void)
{
    dominions_head_shooter_vars_unregister();
    return 0;
}

/* steer(cast) */
/* 0x00b08760 */
int dominions_head_shooter_walk_nodes(Vec3 **pt_out, int **ap, unsigned int *node, int n)
{
    do {
        ulMatrixPush();
        anim_apply_node(node);
        if (*node & 0x1000000) {
            n--;
            ulMatrixGetTranslation((float *)ulMatrixCur, &(*pt_out)->x);
            (*pt_out)++;
            ulMatrixPush();
            vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
            ((void (*)(int *))model_camera_angles)(*ap);
            *ap += 3;
            ulMatrixPop();
            if (n <= 0) {
                ulMatrixPop();
                return 0;
            }
        }
        if (!(*node & 0x10)) {
            n = dominions_head_shooter_walk_nodes(pt_out, ap, (unsigned int *)node[11], n);
            if (n <= 0) {
                ulMatrixPop();
                return 0;
            }
        }
        ulMatrixPop();
        node = (unsigned int *)node[12];
    } while (node);
    return n;
}

/* 0x00b08878 */
void dominions_head_shooter_init(void *obj, DominionsHeadShooterWork *w, void *place, DominionsHeadShooterKind *k)
{
    Vec3 pts[6];
    int angs[6][3];
    Vec3 p;
    int ang[3];
    DominionsLinkArg prm;
    Vec3 *pt_out;
    int *ang_out;
    DominionsHeadShooterSet *typ = &dominions_head_shooter_types[k->idx];
    ModelPart *gfx;

    w->type = (float *)typ;
    enemy_task_clear_flags(obj, 64);
    lockon_obj_init(&w->lock, obj, &w->lpos, 0, dominions_head_shooter_lock_name, enemy_task_get_status_slot(obj), typ->v[0], typ->v[1], 128);
    lockon_entry(&w->lock);
    w->state = 0;
    w->lock.flags &= ~LOCK_LOCKABLE;
    w->count = 1;
    gfx = enemy_task_gfx_ptr(obj);
    pt_out = pts;
    ang_out = angs[0];
    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    anim_start((void *)gfx->clip, 0.0f);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &p;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    dominions_head_shooter_rotate_yxz(ang);
    dominions_head_shooter_walk_nodes(&pt_out, &ang_out, (unsigned int *)gfx->root, 6);
    ulMatrixPop();
    v3_copy(&w->pos, &p);
    i3_copy(w->ang, ang);
    gfx->radius = 180.0f;
    prm.pos = enemy_task_pos_ptr(obj);
    prm.ang = enemy_task_rot_ptr(obj);
    prm.pos2 = prm.pos3 = enemy_task_pos_ptr(obj);
    prm.ang2 = prm.ang3 = w->ang;
    w->child = (void *)dominions_body_and_tail_create(obj, &prm, 6, k->life, 0, 0, 0, 0, 0, 1, 1);
    dominions_body_and_tail_chain_place(w->child, &pts[5], angs[5]);
    player_get_pos(&w->tgt);
    v3_copy(&w->start, &w->tgt);
    enemy_task_set_status_pos(obj, &w->lpos);
}

/* 0x00b08ad8 */
void dominions_head_shooter_release(void *obj, DominionsHeadShooterWork *w)
{
    lockon_remove(&w->lock);
    if (enemy_task_get_pstate(obj) != 3)
        dominions_body_and_tail_chain_end(w->child);
}

/* steer(cast) */
/* 0x00b08b30 */
void dominions_head_shooter_chain_wave(void *obj, DominionsHeadShooterWork *w, float rate)
{
    float blk[6];
    float sc[2];
    float *p;

    heap_bzero(blk, 24);
    rate = 1.0f - ((float (*)(float))float_frac)(rate * 3.0f);
    p = &blk[(int)(rate * 5.0f)];
    ulSinCos((int)(((float (*)(float))float_frac)(rate * 5.0f) * ANG_HALF), &sc[0], &sc[1]);
    *p = sc[0];
    dominions_body_and_tail_chain_set_rate(w->child, blk);
}

/* 0x00b08be0 */
int dominions_head_shooter_next_state(void *obj, DominionsHeadShooterWork *w, int n, int state)
{
    if (++w->count > n) {
        w->state = state;
        w->count = 1;
        return 1;
    }
    return 0;
}

/* 0x00b08c10 */
int dominions_head_shooter_play_motion(void *obj, DominionsHeadShooterWork *w, int n, int state, float frm_start, float frm_end)
{
    Vec3 pts[6];
    Vec3 cur;
    int angs[6][3];
    int ang[3];
    Vec3 to_tgt;
    float sy, cy, sx, cx, sz, cz;
    Vec3 *pt_out;
    int *ang_out;
    float rate = (float)w->count / n;
    ModelPart *gfx;
    float x, y, z, frm;
    int yaw;
    int *wa = w->ang;

    ang_out = angs[0];
    pt_out = pts;
    gfx = enemy_task_gfx_ptr(obj);
    enemy_task_get_pos(obj, &cur);
    v3_sub(&to_tgt, &w->tgt, &cur);
    do {
        Vec3 *v_ = &to_tgt;
        x = v_->x;
        z = v_->z;
        y = v_->y;
    } while (0);
    ROTS_Y(-wa[1], sy, cy, x, z);
    ROTS_X(-wa[0], sx, cx, y, z);
    ROTS_Z(-wa[2], sz, cz, x, y);
    yaw = -(int)RAD_TO_ANG(atan2f(x, -z));
    frm = (frm_end - frm_start) * rate + frm_start;
    gfx->frame = frm;
    anim_start((void *)gfx->clip, frm);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &cur;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    dominions_head_shooter_rotate_yxz(wa);
    vu0_matrix_rotate_y(ulMatrixCur, yaw);
    model_camera_angles(ang);
    enemy_task_set_rot(obj, ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dominions_head_shooter_walk_nodes(&pt_out, &ang_out, (unsigned int *)gfx->root, 6);
    ulMatrixPop();
    dominions_body_and_tail_chain_place(w->child, &pts[5], angs[5]);
    return dominions_head_shooter_next_state(obj, w, n, state);
}

/* 0x00b08ec0 */
int dominions_head_shooter_state_rise(void *obj, DominionsHeadShooterWork *w)
{
    dominions_head_shooter_play_motion(obj, w, w->type[8] * FPS, 1, 0.0f, 15.0f);
    return 0;
}

/* 0x00b08f08 */
int dominions_head_shooter_state_follow(void *obj, DominionsHeadShooterWork *w)
{
    float *typ = w->type;
    int n = typ[9] * FPS;
    float rate = (float)w->count / n;
    Vec3 *tgt = &w->tgt;
    Vec3 *start = &w->start;

    player_pass_pos_a(tgt, typ[15] * FPS * rate);
    tgt->x = tgt->x * rate + start->x * (1.0f - rate);
    tgt->y = tgt->y * rate + start->y * (1.0f - rate);
    tgt->z = tgt->z * rate + start->z * (1.0f - rate);
    dominions_head_shooter_play_motion(obj, w, n, 2, 15.0f, 34.0f);
    return 0;
}

/* 0x00b09028 */
int dominions_head_shooter_state_wave(void *obj, DominionsHeadShooterWork *w)
{
    float *typ = w->type;

    dominions_head_shooter_play_motion(obj, w, typ[10] * FPS, 3, 34.0f, 50.0f);
    dominions_head_shooter_chain_wave(obj, w, (float)w->count / ((typ[10] + typ[11]) * FPS));
    return 0;
}

/* 0x00b090d0 */
int dominions_head_shooter_state_wave_end(void *obj, DominionsHeadShooterWork *w)
{
    float blk[6];
    float *typ = w->type;
    int n = typ[11] * FPS;

    dominions_head_shooter_chain_wave(obj, w, (typ[10] * FPS + w->count) / ((typ[10] + typ[11]) * FPS));
    if (dominions_head_shooter_play_motion(obj, w, n, 4, 50.0f, 56.0f)) {
        heap_bzero(blk, 24);
        dominions_body_and_tail_chain_set_rate(w->child, blk);
        return 1;
    }
    return 0;
}

/* 0x00b09198 */
int dominions_head_shooter_state_aim(void *obj, DominionsHeadShooterWork *w)
{
    dominions_head_shooter_play_motion(obj, w, w->type[12] * FPS, 5, 56.0f, 63.0f);
    return 0;
}

/* 0x00b091e8 */
int dominions_head_shooter_state_fire(void *obj, DominionsHeadShooterWork *w)
{
    dominions_head_shooter_play_motion(obj, w, w->type[13] * FPS, 6, 63.0f, 69.0f);
    return 0;
}

/* 0x00b09238 */
int dominions_head_shooter_state_retract(void *obj, DominionsHeadShooterWork *w)
{
    int n = w->type[14] * FPS;

    if (dominions_head_shooter_play_motion(obj, w, n, 6, 69.0f, ((ModelPart *)enemy_task_gfx_ptr(obj))->frames))
        enemy_task_set_state(obj, 2);
    return 0;
}

/* 0x00b092c0 */
void dominions_head_shooter_update(void *obj, TypeEnemyWork *work)
{
    DominionsHeadShooterWork *w = (DominionsHeadShooterWork *)work;
    int (*fn)(void *obj, DominionsHeadShooterWork *w) = dominions_head_shooter_states[w->state];
    Vec3 p;
    int ang[3];
    Vec3 dir;
    float sc[2][2];
    float x, y, z;
    int fire = 0;

    if (fn)
        fire = fn(obj, w);
    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    yuki_model_positions_at(&w->lpos, &p, ang, enemy_task_gfx_ptr(obj), 0x10000000, 1);
    if (fire) {
        yuki_model_positions_at(&p, &p, ang, enemy_task_gfx_ptr(obj), 0x4000000, 1);
        y = 0.0f;
        x = y;
        z = -1.0f;
        ROT_X(ang[0], sc[0], y, z);
        ROT_Y(ang[1], sc[1], x, z);
        dir.x = x;
        dir.y = y;
        dir.z = z;
        dominions_bomb_create(obj, &p, &dir);
    }
}

/* 0x00b09440 */
void dominions_head_shooter_exec_appear(void *obj, TypeEnemyWork *w)
{
    switch (enemy_task_get_phase(obj)) {
    case 0:
        enemy_task_set_state(obj, 0);
        break;
    case 2:
        break;
    default:
        dominions_head_shooter_update(obj, w);
    }
}

/* 0x00b094b8 */
void dominions_head_shooter_exec_normal(void *obj, TypeEnemyWork *w)
{
    if (enemy_task_get_phase(obj) == 1)
        dominions_head_shooter_update(obj, w);
}

/* 0x00b09508 */
void dominions_head_shooter_exec_leave(void *w)
{
    FINISH_IF(w);
}

/* 0x00b09528 */
void dominions_head_shooter_exec_destroyed(void *obj, DominionsHeadShooterWork *w)
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
        en_shatter_spawn(obj, &desc, 6);
        dominions_body_and_tail_chain_destroy(w->child, 3, 3);
        FINISH_IF(obj);
    }
}

/* 0x00b09608 */
void dominions_head_shooter_pre_exec(void *obj)
{
    Vec3 p, org;

    player_get_frame_vel(&org);
    enemy_task_get_pos(obj, &p);
    p.x -= org.x;
    p.y -= org.y;
    p.z -= org.z;
    enemy_task_set_pos(obj, &p);
}

/* 0x00b09678 */
void dominions_head_shooter_exec(void *obj, TypeEnemyWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (dominions_head_shooter_exec_table[i])
        dominions_head_shooter_exec_table[i](obj, w);
}

/* steer(cast) */
/* 0x00b096d0 */
void dominions_head_shooter_post_exec(void *obj, DominionsHeadShooterWork *w)
{
    Vec3 p;
    int ang[3];
    Vec3 ofs;
    SpawnDesc desc;
    Vec3 vpos;
    float sc[3][2];
    LockObj *lock = &w->lock;
    float x, y, z;
    float *typ;
    int n;

    if (!enemy_status_get_ticks()) {
        enemy_task_get_pos(obj, &p);
        enemy_task_get_rot(obj, ang);
        yuki_model_positions_at(&w->lpos, &p, ang, enemy_task_gfx_ptr(obj), 0x10000000, 1);
    }
    v3_sub(&ofs, &w->lpos, &w->pos);
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
        w->lock.flags &= ~LOCK_LOCKABLE;
    else
        w->lock.flags |= LOCK_LOCKABLE;
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
        enemy_task_get_pos(obj, &vpos);
        ulMatrixApplyPoint((float *)ulMatrixCur, &vpos.x, &vpos.x);
        project_point(&vpos, &vpos);
        switch (lock->life) {
        case 0:
            SoundRhythmSe_EnemyPos(dominions_head_shooter_init_arg, 1, &vpos.x);
            break;
        case 1:
            SoundRhythmSe_EnemyPos(dominions_head_shooter_init_arg, 7, &vpos.x);
            break;
        case 2:
            SoundRhythmSe_EnemyPos(dominions_head_shooter_init_arg, 6, &vpos.x);
            break;
        case 3:
            SoundRhythmSe_EnemyPos(dominions_head_shooter_init_arg, 5, &vpos.x);
            break;
        case 4:
            SoundRhythmSe_EnemyPos(dominions_head_shooter_init_arg, 4, &vpos.x);
            break;
        case 5:
            SoundRhythmSe_EnemyPos(dominions_head_shooter_init_arg, 3, &vpos.x);
            break;
        case 6:
            SoundRhythmSe_EnemyPos(dominions_head_shooter_init_arg, 2, &vpos.x);
            break;
        case 7:
            SoundRhythmSe_EnemyPos(dominions_head_shooter_init_arg, 0, &vpos.x);
            break;
        }
    }
    if (lock->life <= 0 && ((unsigned int)enemy_task_get_pstate(obj)) < 2) {
        n = lockon_hits_max_shot_cnt(lock);
        typ = w->type;
        enemy_status_score_popup(enemy_task_get_status_slot(obj), &w->lpos, ((int *)typ)[4], n);
        enemy_task_set_state(obj, 3);
    }
}

/* 0x00b09a90 */
void dominions_head_shooter_disp(void *obj, TypeEnemyWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (dominions_head_shooter_disp_table[i])
        dominions_head_shooter_disp_table[i](obj, w);
}

/* 0x00b09ae8 */
int dominions_head_shooter_create(void *parent, EnDominionsHoleArg *prm, int *timer)
{
    DominionsHeadShooterKind k;
    ObjDesc desc;
    ObjFuncs funcs = *(ObjFuncs *)dominions_head_shooter_funcs;
    float *typ = (float *)dominions_head_shooter_types;

    k.idx = 0;
    k.life = (typ[8] + typ[9] + typ[10] + typ[11] + typ[12] + typ[13] + typ[14]) * FPS;
    *timer = k.life;
    dominions_head_shooter_init_arg = dominions_head_carrier_get_init_arg();
    ((ShotArg *)prm)->kind = dominions_head_shooter_init_arg;
    nj_fill8(&desc, 0, sizeof desc);
    desc.name = dominions_head_shooter_name;
    desc.funcs = &funcs;
    desc.arg = prm;
    desc.kind = &k.idx;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = ((int *)typ)[2];
    desc.same_lock = ((int *)typ)[3];
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = 21;
    desc.model_no2 = 22;
    desc.score = ((int *)typ)[4];
    desc.hp_reduce = typ[5];
    desc.crash_r = typ[6];
    desc.crash_damage = typ[7];
    desc.time = k.life;
    desc.size = 152;
    return enemy_task_task_create(parent, &desc);
}

/* 0x00b09c90 */
void dominions_head_shooter_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
