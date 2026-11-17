/*
 * Vishnu, Area 2 enemy: deploys six hands on strings, drops them in random order to hang and fire
 * lasers, crab-walks sideways, then escapes. Its core is its own lock-on point.
 */

#include "stg02.h"
#include "mathdef.h"
#include "debug.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_vishnu.h"
#include "en_shiva_break.h"
#include "en_vishnu_hand.h"
#include "en_vishnu_string.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"
#include "../common/util.h"

extern int en_vishnu_init_arg;
extern void (*en_vishnu_exec_table[])(void *o, void *w);
extern ParamRec en_vishnu_params;
extern VarDef en_vishnu_vardefs[20];

/* as en_vishnu_get_hand_pos and en_vishnu_count_hands read it */
typedef struct EnVishnu {
    char pad_00[492];
    Vec3 hpos[6];                 /* 0x1ec */
    char pad_234[92];
    unsigned int flags;          /* 0x290 */
} EnVishnu;

extern int en_vishnu_hand_mtx;
extern int en_vishnu_lock_mtx;

typedef enum { VI_STEP_START } EnVishnuStep;

typedef enum { VI_NSTEP_NONE = -1 } EnVishnuNextStep;

typedef struct EnVishnuWork {
    Vec3 *pos;                  /* 0x00: the object's vectors (enemy_task_pos_ptr ...) */
    Vec3 *param_vec;                  /* 0x04 */
    int *ang;                   /* 0x08 */
    Vec3 *scale;                /* 0x0c */
    int phase;                  /* 0x10: index into en_vishnu_states */
    int next;                   /* 0x14 */
    EnVishnuStep step;                /* 0x18 */
    EnVishnuNextStep next_step;       /* 0x1c */
    ModelPart body;                /* 0x20 */
    ModelPart glow;                /* 0x7c */
    Vec3 vel;                   /* 0xd8: damped by 0.97 each frame */
    Vec3 mov;                   /* 0xe4 */
    Vec3 off;                   /* 0xf0: the eased part of a move */
    int rot[3];                 /* 0xfc */
    int roff[3];                /* 0x108: the eased part of a turn */
    LockObj lock;                  /* 0x114 */
    Vec3 lpos;                  /* 0x154: the lock-on point */
    void *hand[6];              /* 0x160: en_vishnu_hand works */
    void *string[6];            /* 0x178: en_vishnu_string works */
    ModelPart core;                /* 0x190 */
    Vec3 hpos[6];               /* 0x1ec: the hands' start points */
    int hang[6][3];             /* 0x234: and angles */
    int perm;                   /* 0x27c: a random order of 0..5, three bits each */
    Vec3 wob;                   /* 0x280: the wobble offset (en_vishnu_swing) */
    float wt;                   /* 0x28c */
    int flags;                  /* 0x290 */
    float cnt;                  /* 0x294: frames */
    float cnt_end;              /* 0x298 */
} EnVishnuWork;

/* en_vishnu_params: name, label, values */
typedef struct EnVishnuRec {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    int score;                  /* 0x0c */
    float spd_move;             /* 0x10 */
    float pow_move;             /* 0x14 */
    float frm_motion_escape;    /* 0x18 */
    float spd_escape;           /* 0x1c */
    float frm_escape_anticipate; /* 0x20 */
    float pow_escape_move;      /* 0x24 */
    float frm_deploy_hand;      /* 0x28 */
    float wob_spd;              /* 0x2c */
    float wob_amp;              /* 0x30 */
    float sca_laser_power;      /* 0x34 */
    float spd_hand;             /* 0x38 */
    float frm_last;             /* 0x3c */
    float frm_crab_walk;        /* 0x40 */
    float spd_crab_walk;        /* 0x44 */
    float pow_crab_walk;        /* 0x48 */
    float frm_open_interval;                  /* 0x4c */
    float frm_open_after;                  /* 0x50 */
    int debug;                  /* 0x54: shows the debug lines (with flag 16) */
} EnVishnuRec;

#define EN_VISHNU_REC ((EnVishnuRec *)&en_vishnu_params)
extern void (*en_vishnu_states[])(void *o, EnVishnuWork *w);
extern char en_vishnu_name[];
extern void (*en_vishnu_disp_table[])(void *o, EnVishnuWork *w);
extern float en_vishnu_glow_color[4];
extern char en_vishnu_lock_name[];
extern int en_vishnu_hand_frms[6];

/* 0x00b0d150 */
void en_vishnu_vars_register(void)
{
    VarDef hdr;
    VarDef *def;
    int *val;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&hdr, 0, sizeof hdr);
    hdr.name = en_vishnu_params.name;
    hdr.fmt = en_vishnu_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&hdr);
    def = en_vishnu_vardefs;
    val = en_vishnu_params.v;
    for (i = 0; i < 20; i++) {
        def->ptr = val;
        val++;
        def++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_vishnu_vardefs);
    en_vishnu_hand_vars_register();
    en_vishnu_string_vars_register();
}

/* 0x00b0d210 */
void en_vishnu_vars_unregister(void)
{
    en_vishnu_string_vars_unregister();
    en_vishnu_hand_vars_unregister();
    enemy_vars_group_release();
}

/* 0x00b0d238 */
int en_vishnu_marker_draw(int obj, void *w)
{
    int *ent;

    ulGxSetTextureList(stage_textures);
    ent = pnb_get_entry(stage_models, 98);
    return obj_draw_at(obj, w, (void *)*ent);
}

/* 0x00b0d290 */
int en_vishnu_vars_init(int *prm)
{
    en_vishnu_vars_register();
    en_vishnu_init_arg = *prm;
    return 0;
}

/* 0x00b0d2c8 */
int en_vishnu_vars_release(void)
{
    en_vishnu_vars_unregister();
    return 0;
}

/* 0x00b0d2e8 */
void en_vishnu_place_vec(int *ang, Vec3 *out)
{
    int rel_ang[3];

    util_ang_from_player_frame(ang, rel_ang);
    {
        int x, y, z;
        do { x = rel_ang[0]; y = rel_ang[1]; z = rel_ang[2]; } while (0);
        out->x = x * DEG_PER_ANG;
        out->y = y * DEG_PER_ANG;
        out->z = z * DEG_PER_ANG;
    }
    util_dir_to_player_frame(&out->x, &out->x);
}

/* 0x00b0d360 */
void en_vishnu_shuffle_hands(void *obj, EnVishnuWork *w)
{
    int order[6];
    int i, j, swap;
    int perm = 0;

    for (i = 5; i >= 0; i--)
        order[i] = i;
    for (i = 5; i >= 0; i--) {
        j = (int)(RAND_FLOAT_A() * 6.0f);
        swap = order[i];
        order[i] = order[j];
        order[j] = swap;
    }
    for (i = 5; i >= 0; i--)
        perm |= order[i] << (i * 3);
    w->perm = perm;
}

/* 0x00b0d468 */
float en_vishnu_hand_open_frm(void *obj, EnVishnuWork *w, int i)
{
    float base = enemy_task_get_param(obj, 0);
    float flash_intv = enemy_task_get_param(obj, 1);
    float intv = EN_VISHNU_REC->frm_open_interval * FPS;
    float ofs;

    if (fabsf(flash_intv - (int)flash_intv) < FLT_EPSILON)
        ofs = intv * i;
    else
        ofs = intv * ((w->perm >> (i * 3)) & 7);
    return base + ofs;
}

/* 0x00b0d538 */
float en_vishnu_hand_flash_frm(void *obj, EnVishnuWork *w, int i)
{
    float flash_intv = enemy_task_get_param(obj, 1);
    float after = EN_VISHNU_REC->frm_open_after * FPS;
    float ofs;

    if (fabsf(flash_intv - (int)flash_intv) < FLT_EPSILON)
        ofs = flash_intv * i;
    else
        ofs = flash_intv * ((w->perm >> (i * 3)) & 7);
    return ofs + after;
}

/* 0x00b0d5e8 */
void en_vishnu_calc_hand_points(void *obj, int *work)
{
    EnVishnuWork *w = (EnVishnuWork *)work;
    float mtx[16];
    Vec3 *p;
    int i;
    float (*mtxp)[16];
    Vec3 *hpos;
    int (*hang)[3];
    int bit;

    ulMatrixPush();
    matrix_unit_set(0);
    do {
        p = enemy_task_pos_ptr(obj);
        vu0_matrix_translate(ulMatrixCur, p->x, p->y, p->z);
    } while (0);
    en_vishnu_rotate_yxz(enemy_task_rot_ptr(obj));
    bit = 1024;
    hpos = &w->hpos[5];
    hang = &w->hang[5];
    i = 6;
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_copy(mtx, ulMatrixCur);
    mtxp = &((float (*)[16])&en_vishnu_hand_mtx)[5];
    for (; i != 0; i--) {
        vu0_matrix_mul(ulMatrixCur, ulMatrixCur, *mtxp--);
        ulMatrixGetTranslation((float *)ulMatrixCur, &hpos->x);
        hpos--;
        if (!(w->flags & (bit << i)))
            model_camera_angles(*hang--);
        else
            hang--;
        ulMatrixLoad(mtx);
    }
    ulMatrixPop();
    w->flags |= 1;
}

/* 0x00b0d720 */
void en_vishnu_calc_lock_point(void *obj, int *work)
{
    EnVishnuWork *w = (EnVishnuWork *)work;

    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = enemy_task_pos_ptr(obj);
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_vishnu_rotate_yxz(enemy_task_rot_ptr(obj));
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, (float *)&en_vishnu_lock_mtx);
    ulMatrixGetTranslation((float *)ulMatrixCur, &w->lpos.x);
    ulMatrixPop();
    w->flags |= 2;
}

/* 0x00b0d7d0 */
void en_vishnu_calc_points(void *obj, void *work)
{
    int *w = work;

    if (!(w[164] & 1))
        en_vishnu_calc_hand_points(obj, w);
    if (!(w[164] & 2))
        en_vishnu_calc_lock_point(obj, w);
    w[164] &= ~3;
}

/* 0x00b0d840 */
void en_vishnu_swing(void *obj, EnVishnuWork *w)
{
    Vec3 pos;
    float t, amp;

    t = w->wt += EN_VISHNU_REC->wob_spd;
    amp = EN_VISHNU_REC->wob_amp;
    enemy_task_get_pos(obj, &pos);
    v3_sub(&pos, &pos, &w->wob);
    v3_set(&w->wob, sinf(t * 3.0f) * amp, cosf(t * 5.0f) * amp, sinf(t * 4.0f) * amp);
    v3_add(&pos, &w->wob);
    enemy_task_set_pos(obj, &pos);
}

/* 0x00b0d970 */
void en_vishnu_hit_check(void *obj, int *work)
{
    EnVishnuWork *w = (EnVishnuWork *)work;
    Vec3 spos;
    LockObj *lock = &w->lock;
    int hits;
    int se;

    if (!(lock->flags & LOCK_HIT))
        return;
    hits = lock->life;
    vec3_scale_to_length(EN_VISHNU_REC->sca_laser_power, &lock->imp.v.x, &w->vel.x);
    if (hits <= 0) {
        enemy_status_score_popup(enemy_task_get_status_slot(obj), enemy_task_pos_ptr(obj), EN_VISHNU_REC->score, lockon_hits_max_shot_cnt(lock));
        lock->flags &= ~LOCK_LOCKABLE;
        enemy_task_set_state(obj, 3);
    } else {
        en_shiva_break_spawn(obj, enemy_task_pos_ptr(obj), enemy_task_rot_ptr(obj), 8);
    }
    enemy_task_get_pos(obj, &spos);
    ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&spos, (float *)&spos);
    project_point(&spos, &spos);
    if (!hits)
        SoundRhythmSe_EnemyPos(en_vishnu_init_arg, 1, (float *)&spos);
    else if ((lock->life_max - hits) % 2 == 1)
        SoundRhythmSe_EnemyPos(en_vishnu_init_arg, 0, (float *)&spos);
    else
        SoundRhythmSe_EnemyPos(en_vishnu_init_arg, 2, (float *)&spos);
}

/* 0x00b0daf8 */
void en_vishnu_deploy_hand(void *obj, EnVishnuWork *w, int i, Vec3 *ofs)
{
    Vec3 vel;
    float open_frm, flash_frm;

    en_vishnu_calc_hand_points(obj, (int *)w);
    util_rotate_xyz(w->hang[i], 0.0f, EN_VISHNU_REC->spd_hand * FRAME_SEC, 0.0f, &vel);
    v3_add(&vel, ofs);
    open_frm = en_vishnu_hand_open_frm(obj, w, i);
    flash_frm = en_vishnu_hand_flash_frm(obj, w, i);
    w->hand[i] = en_vishnu_hand_create(obj, i, &w->hpos[i], w->hang[i], (int)&vel, open_frm, flash_frm);
    w->string[i] = en_vishnu_string_create(obj, (int)w->hand[i], i, ofs);
    w->flags |= 1024 << i;
}

/* steer(cast) */
/* 0x00b0dc50 */
void en_vishnu_state_deploy(void *obj, EnVishnuWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 tgt;
    Vec3 dir;
    Vec3 pos;
    Vec3 mov;
    Vec3 step;
    float len, frm, min_frm, cnt, end, rate, prog, cur_frm;
    int i;

    switch (w->step) {
    case 0:
        player_get_frame_pos(&tgt);
        v3_add(&tgt, enemy_task_param_vec_ptr(obj));
        v3_sub(&dir, &tgt, enemy_task_pos_ptr(obj));
        enemy_task_set_param_vec(obj, &dir);
        len = vec3_length(&dir);
        frm = len / (EN_VISHNU_REC->spd_move * FRAME_SEC);
        gfx->speed = gfx->frames / (EN_VISHNU_REC->frm_deploy_hand * FPS);
        v3_set(&w->off, 0.0f, 0.0f, 0.0f);
        w->cnt = 1.0f;
        min_frm = EN_VISHNU_REC->frm_deploy_hand * FPS;
        len = (int)(min_frm <= frm ? frm : min_frm);
        w->cnt_end = len;
        w->step++;
        w->next_step++;
    case 1:
        cnt = w->cnt;
        end = w->cnt_end;
        enemy_task_get_pos(obj, &pos);
        prog = cnt / end;
        enemy_task_get_param_vec(obj, &mov);
        v3_neg(&step, &w->off);
        v3_sub(&pos, &pos, &w->off);
        rate = safe_powf(ease_sine_in_out(prog), EN_VISHNU_REC->pow_move);
        v3_mul(&w->off, &mov, rate);
        v3_add(&pos, &w->off);
        v3_add(&step, &w->off);
        enemy_task_set_pos(obj, &pos);
        if (end - cnt < EN_VISHNU_REC->frm_deploy_hand * FPS) {
            dc_object_advance_time(gfx);
            cur_frm = gfx->frame;
            i = 6;
            while (--i >= 0) {
                if (w->flags & (1024 << i))
                    break;
                if (en_vishnu_hand_frms[i] <= cur_frm) {
                    en_vishnu_deploy_hand(obj, w, i, &step);
                    break;
                }
            }
        }
        w->cnt += 1.0f;
        if (end < w->cnt) {
            w->cnt = 1.0f;
            w->cnt_end = enemy_task_get_param(obj, 0) + enemy_task_get_param(obj, 1) * 5.0f + EN_VISHNU_REC->frm_last * FPS;
            dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 103)));
            *(int *)&gfx->frame = 0;
            gfx->speed = gfx->frames / 30.0f;
            dc_object_set_flags(gfx, 4096);
            w->next_step = w->step + 1;
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        else
            break;
    case 2:
        dc_object_advance_time(gfx);
        w->cnt += 1.0f;
        if (w->cnt_end < w->cnt && (gfx->flags2 & 1)) {
            do {
                w->next_step = 0;
                w->next = 1;
            } while (0);
        }
        break;
    }
}

/* steer(cast) */
/* 0x00b0e120 */
void en_vishnu_state_drop_hands(void *obj, EnVishnuWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    int i;

    switch (w->step) {
    case 0:
        for (i = 5; i >= 0; i--) {
            if ((w->flags & (1024 << i)) && !(w->flags & (0x10000 << i))) {
                en_vishnu_hand_detach(w->hand[i]);
                en_vishnu_string_detach(w->string[i]);
            }
        }
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 101)));
        *(int *)&gfx->frame = 0;
        gfx->speed = 0.75f;
        dc_object_clear_flags(gfx, 4096);
        v3_set(&w->off, 0.0f, 0.0f, 0.0f);
        w->cnt = 1.0f;
        w->cnt_end = 60.0f;
        w->step++;
        w->next_step++;
    case 1:
        dc_object_advance_time(gfx);
        if (45.0f <= gfx->frame) {
            gfx->frame = 45.0f;
            do {
                w->next = 2;
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

/* 0x00b0e2b8 */
void en_vishnu_state_crab_walk(void *obj, EnVishnuWork *w)
{
    Vec3 v;
    Vec3 pos;
    int ang[3];
    float end, cnt, rate, zero;

    switch (w->step) {
    case 0:
        enemy_task_get_param_vec(obj, &v);
        util_dir_from_player_frame(&v, &v);
        if (0.0f < v.x) {
            w->rot[2] = 0x10000;
            v.x = EN_VISHNU_REC->spd_crab_walk * FRAME_SEC * (EN_VISHNU_REC->frm_crab_walk * FPS);
        } else {
            w->rot[2] = -0x10000;
            v.x = -(EN_VISHNU_REC->spd_crab_walk * FRAME_SEC) * (EN_VISHNU_REC->frm_crab_walk * FPS);
        }
        zero = 0.0f;
        v.z = zero;
        v.y = zero;
        util_dir_to_player_frame((float *)&v, (float *)&w->mov);
        v3_set(&w->off, zero, zero, zero);
        w->cnt = 1.0f;
        w->cnt_end = EN_VISHNU_REC->frm_crab_walk * FPS;
        w->rot[0] = 0;
        w->rot[1] = 0;
        i3_set(w->roff, 0, 0, 0);
        w->step++;
        w->next_step++;
    case 1:
        end = w->cnt_end;
        cnt = w->cnt;
        enemy_task_get_pos(obj, &pos);
        enemy_task_get_rot(obj, ang);
        rate = safe_powf(ease_sine_in_out(cnt / end), EN_VISHNU_REC->pow_crab_walk);
        v3_sub(&pos, &pos, &w->off);
        i3_sub(ang, w->roff);
        v3_mul(&w->off, &w->mov, rate);
        v3_add(&pos, &w->off);
        i3_mul(w->roff, w->rot, rate);
        i3_add(ang, w->roff);
        enemy_task_set_pos(obj, &pos);
        enemy_task_set_rot(obj, ang);
        w->cnt += 1.0f;
        if (end < w->cnt) {
            w->cnt = 1.0f;
            w->cnt_end = 30.0f;
            do { w->next_step = w->step + 1; } while (0);
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        else
            break;
    case 2:
        w->cnt += 1.0f;
        if (w->cnt_end < w->cnt) {
            do {
                w->next_step = 0;
                w->next = 3;
            } while (0);
        }
        break;
    }
}

/* steer(cast) */
/* 0x00b0e6a0 */
void en_vishnu_state_escape(void *obj, EnVishnuWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 v;
    Vec3 pos;
    Vec3 dir;
    float end, rate;

    switch (w->step) {
    case 0:
        util_dir_from_player_frame(enemy_task_param_vec_ptr(obj), &v);
        v.y = -v.y;
        v.z = -v.z;
        util_dir_to_player_frame((float *)&v, (float *)&v);
        enemy_task_set_param_vec(obj, &v);
        v3_set(&w->off, 0.0f, 0.0f, 0.0f);
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 102)));
        *(int *)&gfx->frame = 0;
        gfx->speed = gfx->frames / (EN_VISHNU_REC->frm_escape_anticipate * FPS);
        w->cnt = 1.0f;
        w->cnt_end = vec3_length(&v) / (EN_VISHNU_REC->spd_escape * FRAME_SEC);
        w->step++;
        w->next_step++;
    case 1:
        dc_object_advance_time(gfx);
        if (gfx->flags2 & 1) {
            end = w->cnt_end;
            enemy_task_get_pos(obj, &pos);
            enemy_task_get_param_vec(obj, &dir);
            v3_sub(&pos, &pos, &w->off);
            rate = safe_powf(ease_sine_in_out(w->cnt / end), EN_VISHNU_REC->pow_escape_move);
            v3_mul(&w->off, &dir, rate);
            v3_add(&pos, &w->off);
            enemy_task_set_pos(obj, &pos);
            w->cnt += 1.0f;
            if (end < w->cnt)
                enemy_task_set_state(obj, 2);
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b0e938 */
void en_vishnu_exec_normal(void *obj, EnVishnuWork *w)
{
    Vec3 pos;

    do {
        w->phase = w->next;
        w->step = w->next_step;
    } while (0);
    if (enemy_task_get_phase(obj) == 1) {
        enemy_task_get_pos(obj, &pos);
        v3_scale(&w->vel, 0.97f);
        v3_add(&pos, &w->vel);
        enemy_task_set_pos(obj, &pos);
        en_vishnu_swing(obj, w);
        if (en_vishnu_states[w->phase])
            en_vishnu_states[w->phase](obj, w);
    }
}

/* 0x00b0ea38 */
void en_vishnu_exec_destroyed(void *obj, void *w)
{
    SpawnDesc desc;

    switch ((unsigned int)enemy_task_get_phase(obj)) {
    case 0:
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 99;
        desc.models = stage_models;
        desc.kind = 0;
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
        v3_set(&desc.v, 0.0f, 0.0f, 0.0f);
        enemy_shatter_create(obj, &desc, 0, 0);
        break;
    case 1:
        enemy_task_next_phase(obj);
        break;
    case 2:
        break;
    }
}

/* 0x00b0eb00 */
void en_vishnu_exec(void *obj, void *w)
{
    void (**tab)(void *obj, void *w) = en_vishnu_exec_table;
    unsigned int i = enemy_task_get_pstate(obj);

    if (tab[i])
        tab[i](obj, w);
}

/* 0x00b0eb60 */
void en_vishnu_pre_exec(void *obj, int *w)
{
    do {
        if (w[4] == 0 && (w[164] & 0x3f0000) == 0x3f0000) {
            w[7] = 0;
            w[5] = 1;
        }
    } while (0);
    en_vishnu_hit_check(obj, w);
}

/* 0x00b0eb90 */
void en_vishnu_post_exec(void *obj, void *w)
{
    en_vishnu_calc_points(obj, w);
}

/* steer(cast) */
/* 0x00b0eb98 */
void en_vishnu_disp_glow(void *obj, EnVishnuWork *w)
{
    ModelPart *body = &w->body;
    int blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    float alpha;

    draw_push_strip_flags();
    draw_push_override_color();
    alpha = ease_sine_in_out_2x_neg(beat_clock_get_bar());
    en_vishnu_glow_color[0] = alpha;
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    draw_set_override_color(en_vishnu_glow_color);
    dc_object_draw(body);
    draw_set_color(((int)((alpha + 1.0f) * 255.0f) + 1 >> 1) << 24 | 0x808000);
    model_object_draw(&w->glow, body->frame);
    draw_pop_override_color();
    draw_pop_strip_flags();
    draw_set_override_flags(blend);
}

/* 0x00b0ec90 */
void en_vishnu_disp_body(void *obj, EnVishnuWork *w)
{
    float mtx[16];
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 *p = enemy_task_pos_ptr(obj);
    int *ang;
    int i;

    if (!view_sphere_visible(p, gfx->radius))
        return;
    ulGxSetTextureList(stage_textures);
    ang = enemy_task_rot_ptr(obj);
    ulMatrixPush();
    do {
        Vec3 *p_ = p;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_vishnu_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(gfx);
    en_vishnu_disp_glow(obj, w);
    ulMatrixPop();
    vu0_matrix_copy(mtx, ulMatrixCur);
    p = &w->hpos[6];
    ang = w->hang[6];
    gfx = &w->core;
    for (i = 5; i >= 0; i--) {
        if (w->flags & (1024 << i))
            continue;
        p--;
        do {
            Vec3 *p_ = p;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        ang -= 3;
        en_vishnu_rotate_yxz(ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        dc_object_draw(gfx);
        ulMatrixLoad(mtx);
    }
}

/* 0x00b0ee08 */
void en_vishnu_disp(void *obj, EnVishnuWork *w)
{
    void (**tbl)(void *obj, EnVishnuWork *w) = en_vishnu_disp_table;
    unsigned int i;
    float deg;
    Vec3 pos;
    Vec3 vel;
    int ang[3];
    Vec3 mov;
    int rot[3];
    ModelPart *gfx;

    i = enemy_task_get_pstate(obj);
    if (tbl[i])
        tbl[i](obj, w);
    if (debug_flags_test(16) && EN_VISHNU_REC->debug) {
        debug_set_origin(10, 10);
        deg = DEG_PER_ANG;
        enemy_task_get_pos(obj, &pos);
        debug_printf("P:%d,%d,%d", (int)pos.x, (int)pos.y, (int)pos.z);
        debug_newline();
        enemy_task_get_param_vec(obj, &vel);
        debug_printf("V:%f,%f,%f", vel.x, vel.y, vel.z);
        debug_newline();
        enemy_task_get_rot(obj, ang);
        debug_printf("A:%d,%d,%d", (int)(ang[0] * deg), (int)(ang[1] * deg), (int)(ang[2] % 65536 * deg));
        debug_newline();
        gfx = enemy_task_gfx_ptr(obj);
        debug_printf("F:%f/%f", gfx->frame, gfx->frames);
        debug_newline();
        debug_printf("CNT:%d/%d", (int)w->cnt, (int)w->cnt_end);
        debug_newline();
        v3_copy(&mov, &w->mov);
        debug_printf("MOV:%f,%f,%f", mov.x, mov.y, mov.z);
        debug_newline();
        i3_copy(rot, w->rot);
        debug_printf("ROT:%d,%d,%d", (int)(rot[0] * deg), (int)(rot[1] * deg), (int)(rot[2] % 65536 * deg));
        debug_newline();
    }
}

/* steer(cast) */
/* 0x00b0f100 */
void en_vishnu_init(void *obj, EnVishnuWork *w, char *place)
{
    Vec3 vel;
    int ang[3];
    int *ent;

    w->pos = enemy_task_pos_ptr(obj);
    w->param_vec = enemy_task_param_vec_ptr(obj);
    w->ang = enemy_task_rot_ptr(obj);
    w->scale = enemy_task_scl_ptr(obj);
    en_vishnu_place_vec((int *)(place + 16), &vel);
    enemy_task_set_param_vec(obj, &vel);
    i3_set(ang, 0, ANG_180, 0);
    enemy_task_set_rot(obj, ang);
    en_vishnu_shuffle_hands(obj, w);
    enemy_task_clear_flags(obj, 64);
    lockon_obj_init(&w->lock, obj, &w->lpos, 0, en_vishnu_lock_name, enemy_task_get_status_slot(obj), EN_VISHNU_REC->hp, EN_VISHNU_REC->hp, 0);
    lockon_entry(&w->lock);
    w->lock.flags |= LOCK_SILENT;
    dc_object_init(&w->core, *((struct DcNode **)pnb_get_entry(stage_models, 85)), 0, 0);
    ent = ((int *)pnb_get_entry(stage_models, 91));
    dc_object_init(&w->body, *ent, *((struct DcClip **)pnb_get_entry(stage_models, 92)), 0);
    ent = ((int *)pnb_get_entry(stage_models, 93));
    dc_object_init(&w->glow, *ent, *((struct DcClip **)pnb_get_entry(stage_models, 94)), 0);
    dc_object_rebuild(&w->core);
    dc_object_rebuild(&w->body);
    dc_object_set_wire(&w->glow, model_pnb_get_wire(stage_models, 93));
    enemy_task_set_appear(obj, 0, 0);
    enemy_task_set_vanish(obj, 0, 0);
}

/* 0x00b0f300 */
void en_vishnu_release(void *obj, char *w)
{
    dc_object_release((struct MapPart *)(w + 0x7c));
    dc_object_release((struct MapPart *)(w + 0x20));
    dc_object_release((struct MapPart *)(w + 0x190));
    lockon_remove((LockObj *)(w + 0x114));
}

/* 0x00b0f340 */
int en_vishnu_create(void *parent, void *place)
{
    ObjDesc desc;
    ObjFuncs funcs;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&funcs, 0, sizeof funcs);
    funcs.fn[0] = en_vishnu_init;
    funcs.fn[1] = en_vishnu_release;
    funcs.fn[2] = en_vishnu_exec;
    funcs.fn[3] = en_vishnu_disp;
    funcs.fn[4] = en_vishnu_pre_exec;
    funcs.fn[5] = en_vishnu_post_exec;
    desc.name = en_vishnu_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = 98;
    desc.model_no2 = 100;
    desc.hp = EN_VISHNU_REC->hp;
    desc.score = EN_VISHNU_REC->score;
    desc.size = 668;
    desc.time = 0.0f;
    desc.wob_amp = 0;
    desc.wob_spd = 0;
    return enemy_task_task_create(parent, &desc);
}

/* steer(cast) */
/* 0x00b0f470 */
void en_vishnu_nodes_init(void)
{
    int mdl = *((int *)pnb_get_entry(stage_models, 98));

    node_tree_draw_matching_at(mdl, 0x4000000, 6, (int)&en_vishnu_hand_mtx, 0);
    node_tree_draw_matching_at(mdl, 0x80000000, 1, (int)&en_vishnu_lock_mtx, 0);
    en_vishnu_hand_nodes_init();
}

/* 0x00b0f4d8 */
void en_vishnu_get_hand_pos(void *obj, int i, Vec3 *out)
{
    EnVishnu *view = enemy_task_work_ptr(obj);

    v3_copy(out, &view->hpos[i]);
}

/* 0x00b0f530 */
void en_vishnu_hand_gone(void *obj, int idx)
{
    int *view = enemy_task_work_ptr(obj);

    view[164] |= 0x10000 << idx;
}

/* 0x00b0f568 */
int en_vishnu_count_hands(void *obj)
{
    EnVishnu *view = enemy_task_work_ptr(obj);
    int n = 0;
    int i;

    for (i = 0; i < 6; i++)
        if (!(view->flags & (0x10000 << i)))
            n++;
    return n;
}

/* 0x00b0f5b8 */
void en_vishnu_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
