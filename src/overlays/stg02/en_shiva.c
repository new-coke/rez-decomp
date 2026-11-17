/*
 * Shiva, Area 2 enemy: core, shields, hatches and doors, each a lock-on target. Slides side to
 * side, fires missiles from its hatches, then escapes; all parts shot scores a bonus. The small
 * flag makes en_shiva_mini instead.
 */

#include "stg02.h"
#include "mathdef.h"
#include "camera.h"
#include "color.h"
#include "debug.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "game.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_shiva.h"
#include "en_shiva_break.h"
#include "en_shiva_flash.h"
#include "en_shiva_mini.h"
#include "enb_shiva.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"
#include "../common/util.h"

extern int en_shiva_init_arg;
extern void (*en_shiva_exec_table[])(void *o, void *w);
extern void (*en_shiva_disp_table[])(void *o, void *w);
extern VarDef en_shiva_vardef;
extern VarDef en_shiva_vardefs[24];

/* 2148 bytes */
typedef struct EnShivaWork {
    Vec3 *pos;                  /* 0x00: the object's vectors (enemy_task_pos_ptr ...) */
    Vec3 *param_vec;                  /* 0x04 */
    int *ang;                   /* 0x08 */
    Vec3 *scale;                /* 0x0c */
    void *gfx;                  /* 0x10 */
    int phase;                  /* 0x14: index into en_shiva_states */
    int next;                   /* 0x18 */
    unsigned int step;          /* 0x1c */
    int next_step;              /* 0x20 */
    ModelPart part[6];             /* 0x24 */
    int clip;                   /* 0x24c */
    Vec3 vel;                   /* 0x250: the placement's movement vector, damped each frame */
    Vec3 drift;                 /* 0x25c */
    Vec3 kick;                  /* 0x268 */
    Vec3 push;                  /* 0x274 */
    float fwd;                  /* 0x280: the slide offset, along player_get_frame_accel's vector */
    float fwd_tgt;              /* 0x284 */
    float fwd_spd;              /* 0x288 */
    Vec3 cpos;                  /* 0x28c: the core's position */
    int cang[3];                /* 0x298 */
    LockObj lock_a[10];            /* 0x2a4 */
    Vec3 pts[10];               /* 0x524 */
    LockObj lock_b[8];             /* 0x59c */
    Vec3 pts2[8];               /* 0x79c */
    LockObj lock_c;                /* 0x7fc */
    Vec3 top;                   /* 0x83c */
    float spd;                  /* 0x848 */
    float spd_tgt;              /* 0x84c */
    int flags;                  /* 0x850 */
    int done;                   /* 0x854: parts already spawned in phase 6 */
    float t;                    /* 0x858: frames */
    float t_max;                /* 0x85c */
    int cnt;                    /* 0x860 */
} EnShivaWork;

typedef struct EnShivaPlace {
    char pad_00[60];
    float small;                /* 0x3c: non-zero creates the small shiva */
} EnShivaPlace;

extern char en_shiva_name[];
extern int *en_shiva_hatch_nodes[8];
extern float en_shiva_core_glow_color[4];
extern float en_shiva_hatch_frms[8];
extern float en_shiva_lock_mtx[][16];
extern float en_shiva_fire_delays[8];
extern char en_shiva_lock_pts[];
extern char en_shiva_door_pts_closed[];
extern char en_shiva_door_pts_open[];

/* one per en_shiva_level */
typedef struct EnShivaSets { EnShivaRec r[2]; } EnShivaSets;

#define EN_SHIVA_SET(i) (((EnShivaSets *)en_shiva_sets)->r[i])
extern void (*en_shiva_states[])(void *o, EnShivaWork *w);
extern char en_shiva_core_ofs[];
extern char en_shiva_top_ofs[];
extern char en_shiva_core_lock_name[];
extern char en_shiva_shield_lock_name[];
extern char en_shiva_container_lock_name[];
extern char en_shiva_hatch_lock_name[];
extern char en_shiva_door_lock_name[];
extern char en_shiva_score_name[];

/* 0x00b07b88 */
void en_shiva_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *prm;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&en_shiva_vardef);
    for (i = 0; i < 2; i++) {
        def = en_shiva_vardefs;
        prm = en_shiva_sets[i].v;
        en_shiva_vardef.name = en_shiva_sets[i].name;
        en_shiva_vardef.fmt = en_shiva_sets[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&en_shiva_vardef);
        for (j = 0; j < 24; j++) {
            def->ptr = prm;
            prm++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_shiva_vardefs);
    }
    en_shiva_mini_vars_register();
}

/* 0x00b07ca0 */
void en_shiva_vars_unregister(void)
{
    en_shiva_mini_vars_unregister();
    enemy_vars_group_release();
}

/* 0x00b07cc0 */
int en_shiva_marker_draw(int obj, void *w)
{
    int *ent;

    ulGxSetTextureList(stage_textures);
    ent = pnb_get_entry(stage_models, 46);
    return obj_draw_at(obj, w, (void *)*ent);
}

/* 0x00b07d18 */
int en_shiva_vars_init(int *prm)
{
    en_shiva_vars_register();
    en_shiva_init_arg = *prm;
    return 0;
}

/* 0x00b07d50 */
int en_shiva_vars_release(void)
{
    en_shiva_vars_unregister();
    return 0;
}

/* 0x00b07d70 */
void en_shiva_slide(void *obj, EnShivaWork *w)
{
    Vec3 p, pl;
    float tgt = w->fwd_tgt;
    float cur = w->fwd;
    float spd, max, rem, a, dst, one;
    Vec3 *pos;

    if (tgt - cur > 0.0f) {
        w->fwd_spd += EN_SHIVA_SET(en_shiva_level).acc_slide * 0.00027777778f;
        spd = w->fwd_spd;
        max = EN_SHIVA_SET(en_shiva_level).spd_slide_max * FRAME_SEC;
        { float t = spd < max ? spd : max; w->fwd_spd = t; spd = t; }
    } else {
        w->fwd_spd -= EN_SHIVA_SET(en_shiva_level).acc_slide * 0.00027777778f;
        spd = w->fwd_spd;
        max = -(EN_SHIVA_SET(en_shiva_level).spd_slide_max * FRAME_SEC);
        { float t = max <= spd ? spd : max; w->fwd_spd = t; spd = t; }
    }
    w->fwd = cur + spd;
    rem = tgt - (cur + spd);
    one = 1.0f;
    if (fabsf(rem) < one) {
        enemy_task_get_pos(obj, &p);
        player_get_pos(&pl);
        if (fabsf(p.z - pl.z) < EN_SHIVA_SET(en_shiva_level).dst_slide_div)
            dst = EN_SHIVA_SET(en_shiva_level).dst_slide_max1 * 0.5f * (RAND_FLOAT_A() + one);
        else
            dst = EN_SHIVA_SET(en_shiva_level).dst_slide_max2 * 0.5f * (RAND_FLOAT_A() + one);
        w->fwd_tgt = dst;
        if (rem > 0.0f)
            w->fwd_tgt = -dst;
    }
    player_get_frame_accel(&p);
    vec3_scale_to_length(spd, &p.x, &p.x);
    pos = enemy_task_pos_ptr(obj);
    v3_add(pos, &p);
}

/* 0x00b08038 */
void en_shiva_calc_points(void *obj, char *work)
{
    EnShivaWork *w = (EnShivaWork *)work;
    Vec3 pos;
    int ang[3];
    Vec3 to_pl;
    int flags = w->flags;
    int *gfx;
    int pitch;
    Vec3 *cpos;

    enemy_task_get_pos(obj, &pos);
    enemy_task_get_rot(obj, ang);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_shiva_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    matrix_apply_points((float *)en_shiva_lock_pts, w->pts, 10);
    if (flags & 8) {
        matrix_apply_points((float *)en_shiva_door_pts_closed, w->pts2, 8);
    } else if (flags & 0x10) {
        matrix_apply_points((float *)en_shiva_door_pts_open, w->pts2, 8);
    } else {
        gfx = enemy_task_gfx_ptr(obj);
        util_node_collect_pos_at((void *)gfx[1], (void *)gfx[2], *(float *)&gfx[15], 0x20000000, 8, (int)w->pts2);
    }
    cpos = &w->cpos;
    ulMatrixApplyPoint((float *)ulMatrixCur, (float *)en_shiva_core_ofs, (float *)cpos);
    player_get_pos(&pos);
    do {
        float t = w->cpos.x;

        to_pl.x = pos.x - t;
        to_pl.y = pos.y - cpos->y;
        to_pl.z = pos.z - cpos->z;
    } while (0);
    w->cang[1] = (int)RAD_TO_ANG(atan2f(-to_pl.x, -to_pl.z));
    pitch = (int)RAD_TO_ANG(atan2f(to_pl.y, sqrtf(to_pl.x * to_pl.x + to_pl.z * to_pl.z)));
    w->cang[0] = ang[0];
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &w->cpos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_shiva_rotate_yxz(w->cang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    ulMatrixApplyPoint((float *)ulMatrixCur, (float *)en_shiva_top_ofs, (float *)&w->top);
    ulMatrixPop();
}

/* 0x00b08268 */
void en_shiva_part_shatter(void *obj, EnShivaWork *w, int i)
{
    SpawnDesc desc;

    nj_fill8(&desc, 0, sizeof desc);
    desc.kind = 0x80000;
    desc.models = stage_models;
    desc.type = 52;
    desc.v.y = desc.inherit = 1.0f;
    *(int *)&desc.v.x = 0;
    *(int *)&desc.v.z = 0;
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = enemy_task_pos_ptr(obj);
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_shiva_rotate_yxz(enemy_task_rot_ptr(obj));
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, en_shiva_lock_mtx[i]);
    enemy_shatter_create(obj, &desc, 0, 0);
    ulMatrixPop();
}

/* 0x00b08360 */
void en_shiva_part_debris(void *obj, EnShivaWork *w, int i)
{
    SpawnDesc desc;
    ShatterParams prm;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&prm, 0, sizeof prm);
    desc.kind = 0x80000;
    desc.models = stage_models;
    desc.type = 52;
    prm.Gravity = 10.0f;
    prm.PowVec = 0.9f;
    prm.PowAng = 6.7f;
    prm.RtoVecDec = 0.94f;
    prm.RtoAngDec = 0.98f;
    prm.TimeAlphaStart = prm.RotSpdPlayer = desc.v.y = desc.inherit = 1.0f;
    prm.TimeAlphaEnd = 80.0f;
    prm.pcol_wire = RGBA(0x68, 0x1e, 0, 0x80);
    *(int *)&desc.v.x = 0;
    *(int *)&desc.v.z = 0;
    prm.force_wire = 0;
    prm.b_acc = 0;
    *(int *)&prm.frm_acc = 0;
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = enemy_task_pos_ptr(obj);
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_shiva_rotate_yxz(enemy_task_rot_ptr(obj));
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, en_shiva_lock_mtx[i]);
    enemy_shatter_create(obj, &desc, (float *)&prm, 0);
    ulMatrixPop();
}

/* 0x00b084f0 */
void en_shiva_fire_missile(void *obj, EnShivaWork *w, int i)
{
    Vec3 pos;
    int ang[3];

    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = enemy_task_pos_ptr(obj);
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_shiva_rotate_yxz(enemy_task_rot_ptr(obj));
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, en_shiva_lock_mtx[i]);
    ulMatrixGetTranslation((float *)ulMatrixCur, &pos.x);
    model_camera_angles(ang);
    ulMatrixPop();
    enb_shiva_create(obj, (int)&pos, (int)ang);
    en_shiva_flash_create(obj, &w->pts[i]);
    v3_set(&w->kick, 0.0f, -EN_SHIVA_SET(en_shiva_level).pow_shot, 0.0f);
}

/* 0x00b08610 */
void en_shiva_hit_check(void *obj, void *work)
{
    EnShivaWork *w = work;
    Vec3 pos;
    Vec3 pl;
    LockObj *lock;
    int i, n, j;
    float range;

    for (i = 8; i < 10; i++) {
        int hits;

        lock = &w->lock_a[i];
        if (!lock->owner || *(int *)((char *)lock->owner - 36) & 1)
            continue;
        if (!(lock->flags & LOCK_LOCKABLE))
            continue;
        if (!(lock->flags & LOCK_HIT))
            continue;
        hits = lock->life;
        v3_set(&w->push, 0.0f, -EN_SHIVA_SET(en_shiva_level).pow_laser, 0.0f);
        en_shiva_break_spawn(obj, &w->pts[i], enemy_task_rot_ptr(obj), 8);
        if (hits <= 0) {
            lock->flags &= ~LOCK_LOCKABLE;
            if (i == 9) {
                enemy_status_score_popup(enemy_task_get_status_slot(obj), &w->pts[9], EN_SHIVA_SET(en_shiva_level).score_tail, lockon_hits_max_shot_cnt(lock));
                w->flags |= 4;
                en_shiva_break_spawn(obj, enemy_task_pos_ptr(obj), enemy_task_rot_ptr(obj), 6);
            } else {
                enemy_status_score_popup(enemy_task_get_status_slot(obj), &w->pts[i], EN_SHIVA_SET(en_shiva_level).score_front, lockon_hits_max_shot_cnt(lock));
                w->flags |= 2;
                en_shiva_break_spawn(obj, enemy_task_pos_ptr(obj), enemy_task_rot_ptr(obj), 3);
            }
        }
        enemy_task_get_pos(obj, &pos);
        ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&pos, (float *)&pos);
        project_point(&pos, &pos);
        if (!hits) {
            if (i == 9)
                SoundRhythmSe_EnemyPos(en_shiva_init_arg, 7, (float *)&pos);
            else
                SoundRhythmSe_EnemyPos(en_shiva_init_arg, 5, (float *)&pos);
        } else {
            if (i == 9)
                SoundRhythmSe_EnemyPos(en_shiva_init_arg, 6, (float *)&pos);
            else
                SoundRhythmSe_EnemyPos(en_shiva_init_arg, 4, (float *)&pos);
        }
    }
    for (i = 0; i < 8; i++) {
        int hits;

        lock = &w->lock_a[i];
        if (!lock->owner || *(int *)((char *)lock->owner - 36) & 1)
            continue;
        if (!(lock->flags & LOCK_LOCKABLE))
            continue;
        if (!(lock->flags & LOCK_HIT))
            continue;
        hits = lock->life;
        v3_set(&w->push, 0.0f, -EN_SHIVA_SET(en_shiva_level).pow_laser, 0.0f);
        if (hits <= 0) {
            lock->flags &= ~LOCK_LOCKABLE;
            enemy_status_score_popup(enemy_task_get_status_slot(obj), &w->pts[i], EN_SHIVA_SET(en_shiva_level).score_tray, lockon_hits_max_shot_cnt(lock));
            w->flags |= 0x200000 << i;
        }
        en_shiva_part_debris(obj, w, i);
        enemy_task_get_pos(obj, &pos);
        ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&pos, (float *)&pos);
        project_point(&pos, &pos);
        if (!hits)
            SoundRhythmSe_EnemyPos(en_shiva_init_arg, 3, (float *)&pos);
        else
            SoundRhythmSe_EnemyPos(en_shiva_init_arg, 2, (float *)&pos);
    }
    for (i = 0; i < 8; i++) {
        int hits;

        lock = &w->lock_b[i];
        if (!lock->owner || *(int *)((char *)lock->owner - 36) & 1)
            continue;
        if (!(lock->flags & LOCK_LOCKABLE))
            continue;
        if (!(lock->flags & LOCK_HIT))
            continue;
        hits = lock->life;
        v3_set(&w->push, 0.0f, -EN_SHIVA_SET(en_shiva_level).pow_laser, 0.0f);
        if (hits <= 0) {
            lock->flags &= ~LOCK_LOCKABLE;
            enemy_status_score_popup(enemy_task_get_status_slot(obj), &w->pts[i], EN_SHIVA_SET(en_shiva_level).score_tray, lockon_hits_max_shot_cnt(lock));
            en_shiva_part_shatter(obj, w, i);
            w->flags |= 0x2000 << i;
            if (!(w->flags & (32 << i))) {
                lockon_entry(&w->lock_a[i]);
                w->flags |= 32 << i;
            }
        } else {
            en_shiva_part_debris(obj, w, i);
        }
        enemy_task_get_pos(obj, &pos);
        ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&pos, (float *)&pos);
        project_point(&pos, &pos);
        if (!hits)
            SoundRhythmSe_EnemyPos(en_shiva_init_arg, 8, (float *)&pos);
    }
    lock = &w->lock_c;
    if (!lock->owner || *(int *)((char *)lock->owner - 36) & 1)
        return;
    if ((lock->flags & (LOCK_HIT | LOCK_LOCKABLE)) != (LOCK_HIT | LOCK_LOCKABLE))
        return;
    v3_set(&w->push, 0.0f, -EN_SHIVA_SET(en_shiva_level).pow_laser, 0.0f);
    en_shiva_break_spawn(obj, &w->cpos, w->cang, 8);
    if (lock->life > 0)
        return;
    n = 0;
    if (!(w->flags & 4)) {
        w->lock_a[9].flags &= ~LOCK_LOCKABLE;
        n = 1;
    }
    if (!(w->flags & 2)) {
        w->lock_a[8].flags &= ~LOCK_LOCKABLE;
        n++;
    }
    for (j = 7; j >= 0; j--) {
        if (!(w->flags & (0x200000 << j))) {
            (&w->lock_a[j])->flags &= ~LOCK_LOCKABLE;
            n++;
        }
    }
    for (j = 7; j >= 0; j--) {
        if (!(w->flags & (0x2000 << j))) {
            (&w->lock_b[j])->flags &= ~LOCK_LOCKABLE;
            n++;
        }
    }
    enemy_status_score_popup(enemy_task_get_status_slot(obj), &w->top, EN_SHIVA_SET(en_shiva_level).score_core, lockon_hits_max_shot_cnt(lock));
    if (!(n + ((n & 1) >> 1))) /* steer(zero) */
        player_status_add_score_event(en_shiva_score_name, EN_SHIVA_SET(en_shiva_level).score_bonus);
    enemy_task_get_pos(obj, &pos);
    player_get_pos(&pl);
    range = EN_SHIVA_SET(en_shiva_level).dst_clip_explode2;
    if (vec3_distance_squared(&pos, &pl) < range * range) {
        if (!(w->flags & 2))
            en_shiva_break_spawn(obj, enemy_task_pos_ptr(obj), enemy_task_rot_ptr(obj), 3);
        if (!(w->flags & 4))
            en_shiva_break_spawn(obj, enemy_task_pos_ptr(obj), enemy_task_rot_ptr(obj), 6);
    }
    w->flags |= 7;
    enemy_task_set_state(obj, 3);
}

/* 0x00b08e40 */
void en_shiva_state_move_in(void *obj, EnShivaWork *w)
{
    Vec3 pos;
    Vec3 v;
    float frm;
    int phase;

    switch (w->step) {
    case 0:
        w->t = 1.0f;
        frm = enemy_task_get_param(obj, 0);
        w->t_max = frm - vec3_length(enemy_task_param_vec_ptr(obj)) / 0.029999971f;
        w->step++;
        w->next_step++;
    case 1:
        enemy_task_get_pos(obj, &pos);
        enemy_task_get_param_vec(obj, &v);
        v3_add(&pos, &v);
        enemy_task_set_pos(obj, &pos);
        w->t += 1.0f;
        if (w->t_max < w->t) {
            if (fabsf(enemy_task_get_param(obj, 2)) < FLT_EPSILON)
                phase = 3;
            else if (!(fabsf(enemy_task_get_param(obj, 4)) < FLT_EPSILON))
                phase = 2;
            else
                phase = 4;
            do {
                do { w->next_step = 0; } while (0);
                w->next = phase;
            } while (0);
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        else
            break;
    case 2:
        enemy_task_get_param_vec(obj, &w->vel);
        break;
    }
}

/* steer(cast) */
/* 0x00b09000 */
void en_shiva_state_escape(void *obj, EnShivaWork *w)
{
    Vec3 vel;
    Vec3 pos;
    Vec3 dir;
    Vec3 eye;
    float len, spd, tgt, nspd, clip;

    switch (w->step) {
    case 0:
        enemy_task_get_param_vec(obj, &vel);
        len = vec3_length(&vel);
        if ((int)(((float (*)(float))float_frac)(enemy_task_get_param(obj, 3)) * 1000.0f + 0.5f)) {
            vel.x = 0.0f;
            vel.z = 0.0f;
            vel.y = -len;
            enemy_task_set_param_vec(obj, &vel);
        } else {
            vel.x = 0.0f;
            vel.y = 0.0f;
            vel.z = -len;
            enemy_task_set_param_vec(obj, &vel);
        }
        *(int *)&w->spd = 0;
        w->spd_tgt = len + (int)enemy_task_get_param(obj, 3);
        w->step++;
        w->next_step++;
    case 1:
        spd = w->spd;
        tgt = w->spd_tgt;
        w->spd = spd < tgt ? ((nspd = spd + EN_SHIVA_SET(en_shiva_level).acc_escape * 0.00027777778f) < tgt ? nspd : tgt)
                       : (tgt <= (nspd = spd - EN_SHIVA_SET(en_shiva_level).acc_escape * 0.00027777778f) ? nspd : tgt);
        enemy_task_get_pos(obj, &pos);
        enemy_task_get_param_vec(obj, &dir);
        vec3_scale_to_length(w->spd, &dir.x, &dir.x);
        v3_add(&pos, &dir);
        enemy_task_set_pos(obj, &pos);
        clip = EN_SHIVA_SET(en_shiva_level).dst_clip;
        camera_get_eye(&eye);
        if (vec3_distance_squared(&pos, &eye) > clip * clip)
            enemy_task_set_state(obj, 2);
        break;
    case 2:
    default:
        return;
    }
    if (w->phase != w->next || w->step != w->next_step)
        w->step++;
}

/* 0x00b092b8 */
void en_shiva_state_wait_open(void *obj, EnShivaWork *w)
{
    float t;

    switch (w->step) {
    case 0:
        w->step++;
        w->t = 1.0f;
        w->next_step++;
    case 1:
        t = w->t + 1.0f;
        w->t = t;
        if (enemy_task_get_param(obj, 4) < t)
            do {
                w->next_step = 0;
                w->next = 4;
            } while (0);
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b09378 */
void en_shiva_state_wait_escape(void *obj, EnShivaWork *w)
{
    float t;

    switch (w->step) {
    case 0:
        w->step++;
        w->t = 1.0f;
        w->next_step++;
    case 1:
        t = w->t + 1.0f;
        w->t = t;
        if (enemy_task_get_param(obj, 1) < t)
            do {
                w->next = 1;
                w->next_step = 0;
            } while (0);
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b09440 */
void en_shiva_state_open(void *obj, EnShivaWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    float frm;
    int i;

    switch (w->step) {
    case 0:
        w->flags &= ~8;
        dc_object_set_clip(gfx, w->clip);
        gfx->speed = gfx->frames / 120.0f;
        w->step++;
        w->next_step++;
    case 1:
        dc_object_advance_time(gfx);
        frm = gfx->frame;
        for (i = 7; i >= 0; i--) {
            if (!(w->flags & (32 << i)) && en_shiva_hatch_frms[i] <= frm) {
                w->flags |= 32 << i;
                lockon_entry(&w->lock_a[i]);
            }
        }
        if (gfx->flags2 & 1) {
            w->flags |= 0x10;
            do {
                w->next = 6;
                w->next_step = 0;
            } while (0);
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b095c0 */
void en_shiva_state_close(void *obj, EnShivaWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);

    switch (w->step) {
    case 0:
        w->step++;
        w->flags &= ~0x10;
        w->next_step++;
        gfx->speed = -gfx->frames / 120.0f;
    case 1:
        dc_object_advance_time(gfx);
        if (gfx->flags2 & 1) {
            dc_object_set_clip(gfx, 0);
            w->flags |= 8;
            do {
                w->next = 1;
                w->next_step = 0;
            } while (0);
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b096b8 */
void en_shiva_state_fire(void *obj, EnShivaWork *w)
{
    int all;
    int i;
    int phase;
    float cnt;

    switch (w->step) {
    case 0:
        w->step++;
        w->t = 1.0f;
        w->next_step++;
        w->done = 0;
    case 1:
        all = 1;
        for (i = 7; i >= 0; i--) {
            if (w->flags & (0x200000 << i))
                continue;
            if ((w->done >> i) & 1)
                continue;
            all = 0;
            if (en_shiva_fire_delays[i] * *(float *)&en_shiva_sets[en_shiva_level].v[11] < w->t) {
                w->done |= 1 << i;
                en_shiva_fire_missile(obj, w, i);
            }
        }
        if (all) {
            cnt = ++w->cnt;
            if (!(enemy_task_get_param(obj, 2) <= cnt))
                do { phase = 7; } while (0);
            else
                phase = 5;
            do {
                do { w->next_step = 0; } while (0);
                w->next = phase;
            } while (0);
        }
        w->t += 1.0f;
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b09890 */
void en_shiva_state_wait_fire(void *obj, EnShivaWork *w)
{
    float t;

    switch (w->step) {
    case 0:
        w->step++;
        w->t = 1.0f;
        w->next_step++;
    case 1:
        t = w->t + 1.0f;
        w->t = t;
        if (enemy_task_get_param(obj, 1) < t) {
            if ((w->flags & 0x1fe00000) == 0x1fe00000) {
                do { w->next_step = 0; w->next = 5; } while (0);
            } else {
                do { w->next_step = 0; w->next = 6; } while (0);
            }
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b09968 */
void en_shiva_exec_normal(void *obj, EnShivaWork *w)
{
    Vec3 pos;
    Vec3 step;
    float len, rate;

    do {
        w->phase = w->next;
        w->step = w->next_step;
    } while (0);
    if (enemy_task_get_phase(obj) == 1) {
        en_shiva_slide(obj, w);
        enemy_task_get_pos(obj, &pos);
        v3_scale(&w->vel, 0.97f);
        v3_add(&pos, &w->vel);
        v3_scale(&w->kick, 0.9f);
        v3_add(&pos, &w->kick);
        v3_scale(&w->push, 0.9f);
        v3_add(&pos, &w->push);
        v3_add(&w->drift, &w->kick);
        len = vec3_length(&w->drift);
        {
        if (EN_SHIVA_SET(en_shiva_level).spd_ine_back * FRAME_SEC < len) {
            rate = EN_SHIVA_SET(en_shiva_level).spd_ine_back * FRAME_SEC / len;
            v3_mul(&step, &w->drift, rate);
            v3_sub(&w->drift, &w->drift, &step);
            v3_sub(&pos, &pos, &step);
        } else {
            v3_sub(&pos, &pos, &w->drift);
            v3_set(&w->drift, 0.0f, 0.0f, 0.0f);
        }
        }
        enemy_task_set_pos(obj, &pos);
        if (en_shiva_states[w->phase])
            en_shiva_states[w->phase](obj, w);
    }
}

/* 0x00b09c70 */
void en_shiva_exec_destroyed(void *obj, EnShivaWork *w)
{
    Vec3 opos;
    Vec3 pl;
    SpawnDesc desc;
    float clip;
    int i;
    Vec3 *pos;
    int *ang;
#ifdef VERSION_US
    Vec3 rel, org;
#endif

    switch ((unsigned int)enemy_task_get_phase(obj)) {
    case 0:
        enemy_task_get_pos(obj, &opos);
        player_get_pos(&pl);
        clip = EN_SHIVA_SET(en_shiva_level).dst_clip_explode2;
        if (clip * clip < vec3_distance_squared(&opos, &pl)) {
            nj_fill8(&desc, 0, sizeof desc);
            desc.type = 108;
            desc.models = stage_models;
            desc.kind = 0;
            enemy_task_get_pos(obj, &desc.pos);
            enemy_task_get_rot(obj, desc.ang);
            enemy_task_get_scl(obj, &desc.scale);
            enemy_task_get_hit_imp(obj, &desc.v);
            enemy_shatter_create(obj, &desc, 0, 0);
        } else {
            clip = EN_SHIVA_SET(en_shiva_level).dst_clip_explode;
            if (vec3_distance_squared(&opos, &pl) < clip * clip) {
                for (i = 7; i >= 0; i--)
                    if (!(w->flags & (0x200000 << i)))
                        en_shiva_part_debris(obj, w, i);
            }
#ifdef VERSION_US
            /* relative to the camera origin */
            ang = enemy_task_rot_ptr(obj);
            enemy_task_get_pos(obj, &rel);
            player_get_frame_vel(&org);
            v3_sub(&rel, &rel, &org);
            en_shiva_break_spawn(obj, &rel, ang, 0);
            en_shiva_break_spawn(obj, &rel, ang, 2);
            en_shiva_break_spawn(obj, &rel, ang, 5);
#else
            pos = enemy_task_pos_ptr(obj);
            ang = enemy_task_rot_ptr(obj);
            en_shiva_break_spawn(obj, pos, ang, 0);
            en_shiva_break_spawn(obj, pos, ang, 2);
            en_shiva_break_spawn(obj, pos, ang, 5);
#endif
            en_shiva_break_spawn(obj, &w->cpos, w->cang, 4);
        }
        break;
    case 1:
        enemy_task_next_phase(obj);
        break;
    case 2:
        break;
    }
}

/* 0x00b09f00 */
void en_shiva_exec(void *obj, void *w)
{
    void (**tab)(void *obj, void *w) = en_shiva_exec_table;
    unsigned int i = enemy_task_get_pstate(obj);

    if (tab[i])
        tab[i](obj, w);
}

/* 0x00b09f60 */
void en_shiva_pre_exec(void *obj, void *w)
{
    en_shiva_hit_check(obj, w);
}

/* 0x00b09f68 */
void en_shiva_post_exec(void *obj, char *wp)
{
    EnShivaWork *w = (EnShivaWork *)wp;
    float bar;
    ModelPart *part;

    en_shiva_calc_points(obj, wp);
    bar = beat_clock_get_bar();
    part = &w->part[1];
    part->frame = part->frames * bar;
    part = &w->part[4];
    part->frame = part->frames * bar;
    part = &w->part[5];
    part->frame = part->frames * bar;
}

/* 0x00b09fc8 */
void en_shiva_toggle_parts(void *obj, EnShivaWork *w)
{
    int i;

    for (i = 7; i >= 0; i--) {
        if (w->flags & (0x200000 << i))
            util_node_toggle(en_shiva_hatch_nodes[i]);
        else if (w->flags & (0x2000 << i))
            util_node_toggle((int *)en_shiva_hatch_nodes[i][11]);
    }
}

/* 0x00b0a070 */
void en_shiva_toggle_glow(void *obj, EnShivaWork *w)
{
    int flags = w->flags;
    int *node = (int *)((int *)w->part[0].root)[11];

    if (flags & 4)
        util_node_toggle(node);
    node = (int *)node[12];
    if (flags & 1)
        util_node_toggle(node);
    node = (int *)node[12];
    if (!(flags & 2))
        return;
    util_node_toggle(node);
}

/* steer(cast) */
/* 0x00b0a0e8 */
void en_shiva_disp_glow(void *obj, EnShivaWork *w)
{
    float col[4];
    int blend;

    col[1] = col[2] = col[3] = 0.0f;
    blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    draw_push_strip_flags();
    draw_push_override_color();
    ulMatrixPush();
    col[0] = ease_sine_in_out_2x_neg(beat_clock_get_bar());
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    draw_set_override_color(col);
    en_shiva_toggle_glow(obj, w);
    dc_object_draw(&w->part[0]);
    en_shiva_toggle_glow(obj, w);
    ulMatrixPop();
    draw_pop_override_color();
    draw_pop_strip_flags();
    draw_set_override_flags(blend);
}

/* steer(cast) */
/* 0x00b0a1b8 */
void en_shiva_disp_body(void *obj, EnShivaWork *w)
{
    Vec3 pos;
    int ang[3];
    Vec3 scl;
    int flags;
    int blend;

    enemy_task_get_pos(obj, &pos);
    enemy_task_get_rot(obj, ang);
    enemy_task_get_scl(obj, &scl);
    if (view_sphere_visible(&pos, ((ModelPart *)enemy_task_gfx_ptr(obj))->radius)) {
        flags = w->flags;
        ulGxSetTextureList(stage_textures);
        en_shiva_toggle_parts(obj, w);
        ulMatrixPush();
        ulMatrixPush();
        do {
            Vec3 *p_ = &pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        en_shiva_rotate_yxz(ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
        if (!(flags & 1)) {
            dc_object_draw(enemy_task_gfx_ptr(obj));
            dc_object_draw(&w->part[1]);
        }
        if (!(flags & 2))
            dc_object_draw(&w->part[2]);
        if (!(flags & 4))
            dc_object_draw(&w->part[5]);
        en_shiva_disp_glow(obj, w);
        ulMatrixPop();
        if (!(flags & 1)) {
            do {
                Vec3 *p_ = &w->cpos;
                vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
            } while (0);
            en_shiva_rotate_yxz(w->cang);
            vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
            dc_object_draw(&w->part[3]);
            blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
            draw_push_strip_flags();
            draw_push_override_color();
            ulMatrixPush();
            en_shiva_core_glow_color[0] = ease_sine_in_out_2x_neg(beat_clock_get_bar());
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
            draw_set_override_color(en_shiva_core_glow_color);
            dc_object_draw(&w->part[4]);
            ulMatrixPop();
            draw_pop_override_color();
            draw_pop_strip_flags();
            draw_set_override_flags(blend);
        }
        ulMatrixPop();
        en_shiva_toggle_parts(obj, w);
    }
}

/* 0x00b0a3d0 */
void en_shiva_disp(void *obj, void *w)
{
    void (**tab)(void *obj, void *w) = en_shiva_disp_table;
    unsigned int i = enemy_task_get_pstate(obj);

    if (tab[i])
        tab[i](obj, w);
}

/* steer(cast) */
/* 0x00b0a430 */
void en_shiva_init(void *obj, EnShivaWork *w)
{
    int fx_slot, i;

    w->pos = enemy_task_pos_ptr(obj);
    w->param_vec = enemy_task_param_vec_ptr(obj);
    w->ang = enemy_task_rot_ptr(obj);
    w->scale = enemy_task_scl_ptr(obj);
    w->gfx = enemy_task_gfx_ptr(obj);
    enemy_task_clear_flags(obj, 64);
    fx_slot = enemy_task_get_status_slot(obj);
    for (i = 0; i < 10; i++) {
        lockon_obj_init(&w->lock_a[i], obj, &w->pts[i], 0, en_shiva_hatch_lock_name, fx_slot, EN_SHIVA_SET(en_shiva_level).hp_tray, EN_SHIVA_SET(en_shiva_level).hp_tray, 0);
        (&w->lock_a[i])->flags |= LOCK_SILENT;
    }
    lockon_obj_init(&w->lock_a[8], obj, &w->pts[8], 0, en_shiva_shield_lock_name, fx_slot, EN_SHIVA_SET(en_shiva_level).hp_front, EN_SHIVA_SET(en_shiva_level).hp_front, 0);
    lockon_entry(&w->lock_a[8]);
    w->lock_a[8].flags |= LOCK_SILENT;
    lockon_obj_init(&w->lock_a[9], obj, &w->pts[9], 0, en_shiva_container_lock_name, fx_slot, EN_SHIVA_SET(en_shiva_level).hp_tail, EN_SHIVA_SET(en_shiva_level).hp_tail, 0);
    lockon_entry(&w->lock_a[9]);
    w->lock_a[9].flags |= LOCK_SILENT;
    for (i = 7; i >= 0; i--) {
        lockon_obj_init(&w->lock_b[i], obj, &w->pts2[i], 0, en_shiva_door_lock_name, fx_slot, EN_SHIVA_SET(en_shiva_level).hp_door, EN_SHIVA_SET(en_shiva_level).hp_door, 0);
        lockon_entry(&w->lock_b[i]);
        (&w->lock_b[i])->flags |= LOCK_SILENT;
    }
    lockon_obj_init(&w->lock_c, obj, &w->top, 0, en_shiva_core_lock_name, fx_slot, EN_SHIVA_SET(en_shiva_level).hp_core, EN_SHIVA_SET(en_shiva_level).hp_core, 0);
    lockon_entry(&w->lock_c);
    dc_object_init(&w->part[0], *((struct DcNode **)pnb_get_entry(stage_models, 40)), 0, 0);
    dc_object_init(&w->part[1], *((struct DcNode **)pnb_get_entry(stage_models, 41)), *((struct DcClip **)pnb_get_entry(stage_models, 42)), 0x1000);
    dc_object_init(&w->part[4], *((struct DcNode **)pnb_get_entry(stage_models, 44)), *((struct DcClip **)pnb_get_entry(stage_models, 45)), 0x1000);
    dc_object_init(&w->part[2], *((struct DcNode **)pnb_get_entry(stage_models, 43)), 0, 0);
    dc_object_init(&w->part[3], *((struct DcNode **)pnb_get_entry(stage_models, 50)), 0, 0);
    dc_object_init(&w->part[5], *((struct DcNode **)pnb_get_entry(stage_models, 48)), *((struct DcClip **)pnb_get_entry(stage_models, 49)), 0x1000);
    dc_object_rebuild(&w->part[0]);
    dc_object_rebuild(&w->part[1]);
    dc_object_rebuild(&w->part[4]);
    dc_object_rebuild(&w->part[2]);
    dc_object_rebuild(&w->part[3]);
    dc_object_rebuild(&w->part[5]);
    w->clip = *((int *)pnb_get_entry(stage_models, 47));
    enemy_task_set_appear(obj, 0, 0);
    enemy_task_set_vanish(obj, 0, 0);
    w->flags |= 8;
}

/* 0x00b0a8a8 */
void en_shiva_release(void *obj, EnShivaWork *w)
{
    int i;

    for (i = 9; i >= 0; i--)
        do { lockon_remove(&w->lock_a[i]); } while (0);
    for (i = 7; i >= 0; i--)
        do { lockon_remove(&w->lock_b[i]); } while (0);
    lockon_remove(&w->lock_c);
    dc_object_release(&w->part[0]);
    dc_object_release(&w->part[1]);
    dc_object_release(&w->part[2]);
    dc_object_release(&w->part[3]);
    dc_object_release(&w->part[4]);
    dc_object_release(&w->part[5]);
}

/* 0x00b0a998 */
int en_shiva_create(void *parent, EnShivaPlace *place)
{
    ObjDesc desc;
    ObjFuncs funcs;

    if (!(fabsf(place->small) < FLT_EPSILON))
        return en_shiva_mini_create(parent, place);
    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&funcs, 0, sizeof funcs);
    funcs.fn[0] = en_shiva_init;
    funcs.fn[1] = en_shiva_release;
    funcs.fn[2] = en_shiva_exec;
    funcs.fn[3] = en_shiva_disp;
    funcs.fn[4] = en_shiva_pre_exec;
    funcs.fn[5] = en_shiva_post_exec;
    desc.name = en_shiva_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = 46;
    desc.model_no2 = -1;
    desc.size = 2148;
    desc.time = 0.0f;
    desc.wob_amp = 0;
    desc.wob_spd = 0;
    return enemy_task_task_create(parent, &desc);
}

/* steer(cast) */
/* 0x00b0aad0 */
void en_shiva_nodes_init(void)
{
    int mdl = *((int *)pnb_get_entry(stage_models, 46));
    unsigned int *clip = (unsigned int *)*((int *)pnb_get_entry(stage_models, 47));

    ulMatrixPush();
    matrix_unit_set(0);
    util_node_collect_pos_at((void *)mdl, 0, 0.0f, 0xb000000, 10, (int)en_shiva_lock_pts);
    util_node_collect_pos_at((void *)mdl, clip, 0.0f, 0x20000000, 8, (int)en_shiva_door_pts_closed);
    util_node_collect_pos_at((void *)mdl, clip, (float)(clip[1] - 1), 0x20000000, 8, (int)en_shiva_door_pts_open);
    ulMatrixPop();
    node_tree_draw_matching_at(mdl, 0xb000000, 10, (int)en_shiva_lock_mtx, 0);
    node_tree_collect_to((void *)mdl, 0x8000000, (struct NodeTreeNode * *)en_shiva_hatch_nodes);
    en_shiva_level = game_type_get() == 3;
}

/* 0x00b0ac10 */
void en_shiva_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
