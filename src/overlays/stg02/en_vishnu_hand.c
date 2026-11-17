/*
 * Vishnu's hands, Area 2: swing on strings; wait, open, charge and fire a laser (enb_vishnu),
 * recoiling. Fall away tumbling once Vishnu is gone. Each is a lock-on target.
 */

#include "stg02.h"
#include "mathdef.h"
#include "approach.h"
#include "camera.h"
#include "debug.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_system.h"
#include "node_tree.h"
#include "player.h"
#include "pnb.h"
#include "sound_midise.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_vishnu_hand.h"
#include "en_shiva_break.h"
#include "en_vishnu.h"
#include "en_vishnu_flash.h"
#include "en_vishnu_string.h"
#include "enb_vishnu.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"

extern int en_vishnu_hand_init_arg;
extern ParamRec en_vishnu_hand_params;
extern VarDef en_vishnu_hand_vardefs[15];

/* en_vishnu_hand_params: name, label, values */
typedef struct EnVishnuHandRec {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    int score;                  /* 0x0c */
    int score2;                    /* 0x10 */
    float frm_open;             /* 0x14 */
    float spd_swing;            /* 0x18 */
    float r_swing;              /* 0x1c */
    float rate_complement_ang;  /* 0x20 */
    float sca_laser_power;      /* 0x24 */
    float throw_spd;            /* 0x28 */
    float gravity;              /* 0x2c */
    float spd_fall_move;        /* 0x30 */
    float ang_fall_max;         /* 0x34 */
    float ang_fall_min;         /* 0x38 */
    float clip_r;               /* 0x3c: no draw beyond this distance from the camera */
    int debug;                  /* 0x40: shows the debug lines (with flag 16) */
} EnVishnuHandRec;

#define EN_VISHNU_HAND_REC ((EnVishnuHandRec *)&en_vishnu_hand_params)
extern struct TaskDesc en_vishnu_hand_desc;
extern int en_vishnu_hand_lock_mtx;
extern int en_vishnu_hand_flash_mtx;
extern int en_vishnu_hand_string_mtx;
extern void (*en_vishnu_hand_states[])(EnVishnuHandWork *w);
extern char en_vishnu_hand_lock_name[];

/* 0x00b0f7d0 */
void en_vishnu_hand_vars_register(void)
{
    VarDef hdr;
    VarDef *def;
    int *val;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&hdr, 0, sizeof hdr);
    hdr.name = en_vishnu_hand_params.name;
    hdr.fmt = en_vishnu_hand_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&hdr);
    def = en_vishnu_hand_vardefs;
    val = en_vishnu_hand_params.v;
    for (i = 0; i < 15; i++) {
        def->ptr = val;
        val++;
        def++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_vishnu_hand_vardefs);
}

/* 0x00b0f880 */
void en_vishnu_hand_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b0f888 */
int en_vishnu_hand_vars_init(int *prm)
{
    en_vishnu_hand_init_arg = *prm;
    return 0;
}

/* 0x00b0f8a0 */
void en_vishnu_hand_calc_lock_pos(EnVishnuHandWork *w)
{
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &w->pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_vishnu_hand_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, (float *)&en_vishnu_hand_lock_mtx);
    ulMatrixGetTranslation((float *)ulMatrixCur, &w->lock_pos.x);
    ulMatrixPop();
    w->flags |= 1;
}

/* 0x00b0f938 */
void en_vishnu_hand_calc_flash_pos(EnVishnuHandWork *w)
{
    if (w->flags & 0x400) {
        ulMatrixPush();
        matrix_unit_set(0);
        do {
            Vec3 *p_ = &w->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        en_vishnu_hand_rotate_yxz(w->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_mul(ulMatrixCur, ulMatrixCur, (float *)&en_vishnu_hand_flash_mtx);
        ulMatrixGetTranslation((float *)ulMatrixCur, &w->flash_pos.x);
        ulMatrixPop();
    }
    w->flags |= 2;
}

/* 0x00b0f9e0 */
void en_vishnu_hand_calc_string_pos(EnVishnuHandWork *w)
{
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &w->pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_vishnu_hand_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, (float *)&en_vishnu_hand_string_mtx);
    ulMatrixGetTranslation((float *)ulMatrixCur, &w->string_pos.x);
    ulMatrixPop();
    w->flags |= 4;
}

/* 0x00b0fa78 */
void en_vishnu_hand_calc_all(EnVishnuHandWork *w)
{
    float mtx[16];

    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &w->pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_vishnu_hand_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_copy(mtx, ulMatrixCur);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, (float *)&en_vishnu_hand_lock_mtx);
    ulMatrixGetTranslation((float *)ulMatrixCur, &w->lock_pos.x);
    if (w->flags & 0x400) {
        ulMatrixLoad(mtx);
        vu0_matrix_mul(ulMatrixCur, ulMatrixCur, (float *)&en_vishnu_hand_flash_mtx);
        ulMatrixGetTranslation((float *)ulMatrixCur, &w->flash_pos.x);
    }
    ulMatrixLoad(mtx);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, (float *)&en_vishnu_hand_string_mtx);
    ulMatrixGetTranslation((float *)ulMatrixCur, &w->string_pos.x);
    ulMatrixPop();
}

/* 0x00b0fb80 */
void en_vishnu_hand_calc_points(EnVishnuHandWork *w)
{
    int flags = w->flags;

    if (flags & 7) {
        if (!(flags & 1))
            en_vishnu_hand_calc_lock_pos(w);
        if (!(flags & 2))
            en_vishnu_hand_calc_flash_pos(w);
        if (!(flags & 4))
            en_vishnu_hand_calc_string_pos(w);
        w->flags &= ~7;
    } else
        en_vishnu_hand_calc_all(w);
}

/* 0x00b0fc20 */
void en_vishnu_hand_swing(EnVishnuHandWork *w)
{
    Vec3 *p = &w->pos;
    Vec3 *off = &w->wob;
    float t, amp;

    t = w->wt += EN_VISHNU_HAND_REC->spd_swing;
    amp = EN_VISHNU_HAND_REC->r_swing;
    v3_sub(p, p, off);
    v3_set(off, sinf(w->freq[0] * t) * amp, cosf(w->freq[1] * t) * amp, sinf(w->freq[2] * t) * amp);
    v3_add(p, off);
}

/* 0x00b0fd30 */
void en_vishnu_hand_shatter(EnVishnuHandWork *w)
{
    SpawnDesc desc;

    nj_fill8(&desc, 0, sizeof desc);
    desc.type = 85;
    desc.models = stage_models;
    desc.kind = 0;
    v3_copy(&desc.pos, &w->pos);
    i3_copy(desc.ang, w->ang);
    v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
    v3_set(&desc.v, 0.0f, 0.0f, 0.0f);
    enemy_shatter_create(w, &desc, 0, 0);
}

/* 0x00b0fde8 */
void en_vishnu_hand_hit_check(EnVishnuHandWork *w)
{
    Vec3 spos;
    LockObj *lock = &w->lock;
    int hits;

    if (!(lock->flags & LOCK_HIT))
        return;
    hits = lock->life;
    vec3_scale_to_length(EN_VISHNU_HAND_REC->sca_laser_power, &lock->imp.v.x, &w->vel.x);
    if (hits <= 0) {
        w->ev = 3;
        enemy_status_score_popup(w->slot, &w->pos, w->score, lockon_hits_max_shot_cnt(lock));
        lock->flags &= ~LOCK_LOCKABLE;
        en_vishnu_hand_shatter(w);
        FINISH_IF(w);
    } else {
        en_shiva_break_spawn(w, &w->pos, w->ang, 8);
    }
    ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&w->popup_pos, (float *)&spos);
    project_point(&spos, &spos);
    if (!hits) {
        if (w->owner) {
            if (en_vishnu_count_hands(w->owner) > 0)
                SoundRhythmSe_EnemyPos(en_vishnu_hand_init_arg, 1, (float *)&spos);
            else
                SoundRhythmSe_EnemyPos(en_vishnu_hand_init_arg, 2, (float *)&spos);
        } else
            SoundRhythmSe_EnemyPos(en_vishnu_hand_init_arg, 1, (float *)&spos);
    } else {
        SoundRhythmSe_EnemyPos(en_vishnu_hand_init_arg, 0, (float *)&spos);
    }
}

/* 0x00b0ff30 */
int en_vishnu_hand_is_far(EnVishnuHandWork *w)
{
    Vec3 eye;
    float clip;

    camera_get_eye(&eye);
    clip = EN_VISHNU_HAND_REC->clip_r;
    if (vec3_distance_squared(&w->pos, &eye) > clip * clip)
        return 1;
    return 0;
}

/* 0x00b0ff90 */
void en_vishnu_hand_tether(EnVishnuHandWork *w)
{
    Vec3 anchor;
    Vec3 ofs;
    float len, rate, lim;

    if (w->owner) {
        en_vishnu_get_hand_pos(w->owner, w->idx, &anchor);
        v3_sub(&ofs, &anchor, &w->pos);
        len = vec3_length(&ofs);
        lim = en_vishnu_string_max_len();
        if (lim < len) {
            rate = len / lim - 1.0f;
            ofs.x *= rate;
            ofs.y *= rate;
            ofs.z *= rate;
            v3_add(&w->pos, &ofs);
            v3_mul(&w->vel, &ofs, en_vishnu_string_get_rebound());
        }
    }
}

/* 0x00b100b8 */
void en_vishnu_hand_aim(EnVishnuHandWork *w)
{
    Vec3 pl;
    int ang[3];
    Vec3 v;

    player_get_pos(&pl);
    v3_sub(&v, &pl, &w->pos);
    do {
        ang[1] = (int)RAD_TO_ANG(atan2f(-v.x, -v.z));
        ang[0] = (int)RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
    } while (0);
    ang[2] = 0;
    ang3_approach(ang, w->ang, EN_VISHNU_HAND_REC->rate_complement_ang);
}

/* 0x00b101d0 */
void en_vishnu_hand_fire(EnVishnuHandWork *w)
{
    Vec3 pl;
    Vec3 dir;

    player_get_pos(&pl);
    v3_sub(&dir, &pl, &w->pos);
    vec3_scale_to_length(-EN_VISHNU_HAND_REC->throw_spd, &dir.x, &w->vel.x);
    w->fx = enb_vishnu_create(w);
    w->flags |= 0x800;
}

/* 0x00b10260 */
void en_vishnu_hand_state_wait(EnVishnuHandWork *w)
{
    switch (w->step) {
    case 0:
        w->cnt = 1.0f;
        w->cnt_end = w->open_frm;
        w->next_step++;
        w->step++;
    case 1:
        w->cnt += 1.0f;
        if (w->cnt_end < w->cnt)
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

/* steer(cast) */
/* 0x00b102f8 */
void en_vishnu_hand_state_open(EnVishnuHandWork *w)
{
    ModelPart *body;

    switch (w->step) {
    case 0:
        dc_object_set_clip(body = &w->body, *((struct DcClip **)pnb_get_entry(stage_models, 86)));
        w->body.speed = w->body.frames / (EN_VISHNU_HAND_REC->frm_open * FPS);
        SoundRhythmSe_Enemy(en_vishnu_hand_init_arg, 3);
        w->step++;
        w->next_step++;
    case 1:
        dc_object_advance_time(&w->body);
        if (w->body.flags2 & 1) {
            do {
                w->next_step = 0;
                w->next = 2;
            } while (0);
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b103f0 */
void en_vishnu_hand_state_charge(EnVishnuHandWork *w)
{
    switch (w->step) {
    case 0:
        w->cnt = 1.0f;
        w->cnt_end = w->flash_frm;
        w->step++;
        w->next_step++;
    case 1:
        w->cnt += 1.0f;
        if (w->cnt_end < w->cnt) {
            w->circle = en_vishnu_flash_create(w, &w->flash_pos);
            w->flags |= 0x400;
            do { w->next_step = w->step + 1; } while (0);
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b104c8 */
void en_vishnu_hand_state_fall(EnVishnuHandWork *w)
{
    int spin_x, spin_y;

    switch (w->step) {
    case 0:
        w->lock.life = 1;
        FINISH_IF(w->circle);
        {
            float *rel = &w->rel.x;

            vec3_scale_to_length(EN_VISHNU_HAND_REC->spd_fall_move * FRAME_SEC, rel, rel);
        }
        spin_x = RAND_FLOAT_A() * (float)((int)DEG_TO_ANG(EN_VISHNU_HAND_REC->ang_fall_max) - (int)DEG_TO_ANG(EN_VISHNU_HAND_REC->ang_fall_min)) + (float)(int)DEG_TO_ANG(EN_VISHNU_HAND_REC->ang_fall_min);
        spin_y = RAND_FLOAT_A() * (float)((int)DEG_TO_ANG(EN_VISHNU_HAND_REC->ang_fall_max) - (int)DEG_TO_ANG(EN_VISHNU_HAND_REC->ang_fall_min)) + (float)(int)DEG_TO_ANG(EN_VISHNU_HAND_REC->ang_fall_min);
        i3_set(w->spin, spin_x, spin_y, RAND_FLOAT_A() * (float)((int)DEG_TO_ANG(EN_VISHNU_HAND_REC->ang_fall_max) - (int)DEG_TO_ANG(EN_VISHNU_HAND_REC->ang_fall_min)) + (float)(int)DEG_TO_ANG(EN_VISHNU_HAND_REC->ang_fall_min));
        w->step++;
        w->next_step++;
    case 1:
        v3_scale(&w->rel, 0.99f);
        i3_scale(w->spin, 0.99f);
        v3_add(&w->pos, &w->rel);
        {
            float grav = EN_VISHNU_HAND_REC->gravity;

            w->t += 1.0f;
            w->pos.y += grav * 0.00027777778f * (0.5f - w->t);
        }
        i3_add(w->ang, w->spin);
        if (en_vishnu_hand_is_far(w) && w)
            FINISH(w);
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b10858 */
void en_vishnu_hand_exec(EnVishnuHandWork *w)
{
    Vec3 pl_vel;
    int n = enemy_status_get_ticks();

    player_get_frame_vel(&pl_vel);
    v3_add(&w->pos, &pl_vel);
    if (w->owner && (TASK_OF(w->owner)->flags & TASK_F_END)) {
        en_vishnu_hand_detach(w);
        w->owner = 0;
        w->score = EN_VISHNU_HAND_REC->score;
    }
    if ((w->flags & 0x400) && w->circle && (TASK_OF(w->circle)->flags & TASK_F_END)) {
        en_vishnu_hand_fire(w);
        w->circle = 0;
        w->flags &= ~0x400;
    }
    if (w->flags & 0x800) {
        if (!w->fx || (TASK_OF(w->fx)->flags & TASK_F_END))
            w->fx = 0;
        else if (!w->owner) {
            enb_vishnu_detach(w->fx);
            w->fx = 0;
        }
    }
    en_vishnu_hand_hit_check(w);
    do {
        w->phase = w->next;
        w->step = w->next_step;
    } while (0);
    while (--n >= 0) {
        do {
            w->phase = w->next;
            w->step = w->next_step;
        } while (0);
        v3_scale(&w->vel, 0.97f);
        v3_add(&w->pos, &w->vel);
        if (w->phase != 3) {
            en_vishnu_hand_swing(w);
            en_vishnu_hand_tether(w);
            en_vishnu_hand_aim(w);
        }
        if (en_vishnu_hand_states[w->phase])
            en_vishnu_hand_states[w->phase](w);
    }
    en_vishnu_hand_calc_points(w);
}

/* 0x00b10a90 */
void en_vishnu_hand_disp(EnVishnuHandWork *w)
{
    Vec3 *p = &w->pos;
    Vec3 v;
    int ang[3];
    ModelPart *gfx;

    if (view_sphere_visible(p, w->body.radius)) {
        ulGxSetTextureList(stage_textures);
        ulMatrixPush();
        do {
            Vec3 *p_ = p;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        en_vishnu_hand_rotate_yxz(w->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        dc_object_draw(&w->body);
        ulMatrixPop();
    }
    if (debug_flags_test(16) && EN_VISHNU_HAND_REC->debug) {
        debug_set_origin(10, 20);
        v3_copy(&v, p);
        debug_printf("POS:%d,%d,%d", (int)v.x, (int)v.y, (int)v.z);
        debug_newline();
        i3_copy(ang, w->ang);
        debug_printf("ANG:%d,%d,%d", (int)(ang[0] * DEG_PER_ANG), (int)(ang[1] * DEG_PER_ANG),
                     (int)(ang[2] % 65536 * DEG_PER_ANG));
        debug_newline();
        gfx = &w->body;
        debug_printf("FRM:%d/%d", (int)gfx->frame, (int)gfx->frames);
        debug_newline();
        debug_printf("CNT:%d/%d", (int)w->cnt, (int)w->cnt_end);
        debug_newline();
    }
}

/* steer(cast) */
/* 0x00b10c80 */
void en_vishnu_hand_init(EnVishnuHandWork *w, int idx, Vec3 *pos, int *ang, int vel, float open_frm, float flash_frm)
{
    Vec3 pl_vel;
    LockObj *lock;

    player_get_frame_vel(&pl_vel);
    v3_sub(&w->pos, pos, &pl_vel);
    i3_copy(w->ang, ang);
    w->idx = idx;
    w->score = EN_VISHNU_HAND_REC->score;
    v3_copy(&w->vel, (Vec3 *)vel);
    w->open_frm = open_frm;
    w->flash_frm = flash_frm;
    w->freq[0] = RAND_FLOAT_A() * 4.0f + 2.0f;
    w->freq[1] = RAND_FLOAT_A() * 4.0f + 2.0f;
    w->freq[2] = RAND_FLOAT_A() * 4.0f + 2.0f;
    w->slot = enemy_status_ent_alloc(en_vishnu_hand_init_arg, -1, w);
    enemy_status_event(w->slot, 2, &w->pos);
    w->ev = 4;
    dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(stage_models, 85)), 0, 0);
    dc_object_rebuild(&w->body);
    lock = &w->lock;
    lockon_obj_init(lock, w, &w->lock_pos, 0, en_vishnu_hand_lock_name, w->slot, EN_VISHNU_HAND_REC->hp, EN_VISHNU_HAND_REC->hp, 128);
    lockon_entry(lock);
}

/* 0x00b10ed8 */
void en_vishnu_hand_release(EnVishnuHandWork *w)
{
    enemy_status_event(w->slot, w->ev, &w->pos);
    lockon_remove(&w->lock);
    dc_object_release(&w->body);
    if (!w->owner || *(int *)((char *)w->owner - 36) & 1)
        return;
    en_vishnu_hand_gone(w->owner, w->idx);
}

/* 0x00b10f48 */
EnVishnuHandWork *en_vishnu_hand_create(void *obj, int idx, Vec3 *pos, int *ang, int vel, float open_frm, float flash_frm)
{
    EnVishnuHandWork *w = task_create_sibling_inherit(obj, &en_vishnu_hand_desc, 368);

    w->owner = obj;
    en_vishnu_hand_init(w, idx, pos, ang, vel, open_frm, flash_frm);
    return w;
}

/* steer(cast) */
/* 0x00b10ff8 */
void en_vishnu_hand_nodes_init(void)
{
    int mdl = *((int *)pnb_get_entry(stage_models, 85));

    node_tree_draw_matching_at(mdl, 0x2000000, 1, (int)&en_vishnu_hand_lock_mtx, 0);
    node_tree_draw_matching_at(mdl, 0x2000000, 1, (int)&en_vishnu_hand_flash_mtx, 0);
    node_tree_draw_matching_at(mdl, 0x1000000, 1, (int)&en_vishnu_hand_string_mtx, 0);
}

/* 0x00b11078 */
void en_vishnu_hand_get_muzzle(EnVishnuHandWork *w, Vec3 *pos, int *ang)
{
    v3_copy(pos, &w->lock_pos);
    i3_copy(ang, w->ang);
}

/* 0x00b110b8 */
void en_vishnu_hand_get_string_pos(char *w, Vec3 *out)
{
    Vec3 *src = (Vec3 *)(w + 0x124);

    out->x = src->x;
    out->y = src->y;
    out->z = src->z;
}

/* 0x00b110d8 */
void en_vishnu_hand_destroy(EnVishnuHandWork *w)
{
    w->ev = 3;
    enemy_status_score_popup(w->slot, &w->popup_pos, w->score, lockon_hits_max_shot_cnt(&w->lock));
    w->lock.flags &= ~LOCK_LOCKABLE;
    en_vishnu_hand_shatter(w);
    if (w->owner) {
        if (en_vishnu_count_hands(w->owner) > 0)
            SoundRhythmSe_EnemyPos(en_vishnu_hand_init_arg, 1, &w->pos.x);
        else
            SoundRhythmSe_EnemyPos(en_vishnu_hand_init_arg, 2, &w->pos.x);
    }
    FINISH_IF(w);
}

/* 0x00b11190 */
void en_vishnu_hand_detach(EnVishnuHandWork *w)
{
    Vec3 opos;

    enemy_task_get_pos(w->owner, &opos);
    v3_sub(&w->rel, &w->pos, &opos);
    w->score = EN_VISHNU_HAND_REC->score2;
    do { w->next = 3; } while (0);
    w->next_step = 0;
    w->owner = 0;
}

/* 0x00b11210 */
void en_vishnu_hand_finish(void *w)
{
    FINISH_IF(w);
}

/* 0x00b11230 */
void en_vishnu_hand_move_dir(EnVishnuHandWork *w, Vec3 *dir)
{
    v3_add(&w->pos, dir);
    do {
        w->ang[1] = (int)RAD_TO_ANG(atan2f(-dir->x, -dir->z));
        w->ang[0] = (int)RAD_TO_ANG(atan2f(dir->y, sqrtf(dir->x * dir->x + dir->z * dir->z)));
    } while (0);
}

/* 0x00b11340 */
int en_vishnu_hand_count_others(int *w)
{
    int cnt = 0;

    if (w[38])
        cnt = en_vishnu_count_hands((void *)w[38]) - 1;
    return cnt;
}

/* 0x00b11370 */
void en_vishnu_hand_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
