/*
 * Garuda's nest, Area 2 enemy: hangs by the path, pulsing to the bar; in rounds opens and launches
 * four Garudas. Hits push it back and spin it.
 */

#include "stg02.h"
#include "mathdef.h"
#include "anim.h"
#include "color.h"
#include "debug.h"
#include "debugdraw.h"
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
#include "sound_beat.h"
#include "sound_midise.h"
#include "system.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "en_garuda_nest.h"
#include "en_garuda.h"
#include "en_shiva_break.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"
#include "../common/util.h"

extern int en_garuda_nest_init_arg;
extern ParamRec en_garuda_nest_params;
extern VarDef en_garuda_nest_vardefs[12];
extern int en_garuda_nest_spawn_mtx;
extern int en_garuda_nest_glow_mtx;

typedef struct EnGarudaNestWork {
    Vec3 *pos;                  /* 0x00: the object's vectors (enemy_task_pos_ptr ...) */
    Vec3 *param_vec;                  /* 0x04 */
    int *ang;                   /* 0x08 */
    Vec3 *scale;                /* 0x0c */
    int phase;                  /* 0x10: index into en_garuda_nest_states */
    int next;                   /* 0x14 */
    unsigned int step;          /* 0x18 */
    int next_step;              /* 0x1c */
    ModelPart body;                /* 0x20 */
    ModelPart glow;                /* 0x7c: its frame follows the object's model frame */
    int clip;
    int *anim;                  /* 0xdc */
    Vec3 vec;                  /* 0xe0 */
    Vec3 wob;                   /* 0xec: the wobble offset (en_garuda_nest_swing) */
    float wt;                   /* 0xf8 */
    int rate[3];                /* 0xfc: added to the angles each frame */
    Vec3 dir;                   /* 0x108: velocity, damped by 0.97 */
    int spin[3];                /* 0x114: damped by 0.99 */
    LockObj lock;                  /* 0x120 */
    int flags;                  /* 0x160 */
    int atk_cnt;                    /* 0x164: selects placement parameter 0 or 1 for the wait */
    int gen_cnt;
    float t;                    /* 0x16c: frames */
    float t_max;                /* 0x170 */
} EnGarudaNestWork;

typedef struct EnGarudaNestPlace {
    char pad_00[16];
    int ang[3];                 /* 0x10 */
    char pad_1c[24];
    Vec3 off;                   /* 0x34: from the player */
} EnGarudaNestPlace;

extern void (*en_garuda_nest_states[])(void *o, EnGarudaNestWork *w);

/* en_garuda_nest_params: name, label, values */
typedef struct EnGarudaNestRec {
    char *name;
    char *label;                /* 0x04 */
    int he;                     /* 0x08 */
    int score;                  /* 0x0c */
    float spd_motion, frm_appear, frm_away;        /* 0x10 */
    float wob_amp;              /* 0x1c */
    float dist;                 /* 0x20 */
    float recoil;               /* 0x24: the push back when hit */
    float spin_k;               /* 0x28: the spin when hit (times rate) */
    float spin_rand;            /* 0x2c: its random spread */
    float speed;                /* 0x30: per second */
    int debug;                  /* 0x34: shows the debug lines (with flag 16) */
} EnGarudaNestRec;

#define EN_GARUDA_NEST_REC ((EnGarudaNestRec *)&en_garuda_nest_params)
extern float en_garuda_nest_glow_col[4];
extern void (*en_garuda_nest_exec_table[])(void *o, EnGarudaNestWork *w);
extern char en_garuda_nest_name[];
extern char en_garuda_nest_lock_name[];
extern void (*en_garuda_nest_disp_table[])(void *o, EnGarudaNestWork *w);

/* 0x00b01780 */
void en_garuda_nest_vars_register(void)
{
    VarDef hdr;
    VarDef *def;
    int *val;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&hdr, 0, sizeof hdr);
    hdr.name = en_garuda_nest_params.name;
    hdr.fmt = en_garuda_nest_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&hdr);
    def = en_garuda_nest_vardefs;
    val = en_garuda_nest_params.v;
    for (i = 0; i < 12; i++) {
        def->ptr = val;
        val++;
        def++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_garuda_nest_vardefs);
}

/* 0x00b01830 */
void en_garuda_nest_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b01838 */
int en_garuda_nest_marker_draw(int obj, EnGarudaNestPlace *place)
{
    Vec3 pos;
    Vec3 off;
    float x, y, z;
    int *mdl;

    ulGxSetTextureList(stage_textures);
    x = place->off.x;
    y = place->off.y;
    z = place->off.z;
    player_get_pos(&pos);
    v3_set(&off, x, y, z);
    v3_add(&pos, &off);
    do {
        ulMatrixPush();
    } while (0);
    vu0_matrix_translate(ulMatrixCur, pos.x, pos.y, pos.z);
    en_garuda_nest_rotate_yxz(place->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    debug_draw_box(RGBA(0x80, 0, 0, 0x80), 20.0f);
    ulMatrixPop();
    mdl = pnb_get_entry(stage_models, 14);
    return obj_draw_at(obj, place, (void *)*mdl);
}

/* 0x00b01940 */
int en_garuda_nest_vars_init(int *id)
{
    en_garuda_nest_vars_register();
    en_garuda_nest_init_arg = *id;
    return 0;
}

/* 0x00b01978 */
int en_garuda_nest_vars_release(void)
{
    en_garuda_nest_vars_unregister();
    return 0;
}

/* 0x00b01998 */
void en_garuda_nest_swing(void *obj, EnGarudaNestWork *w)
{
    Vec3 pos;
    Vec3 mv;
    float t, r, x, y, z;

    w->wt += 0.005f;
    t = w->wt;
    r = EN_GARUDA_NEST_REC->wob_amp;
    enemy_task_get_pos(obj, &pos);
    v3_neg(&mv, &w->wob);
    x = sinf(t * 3.0f) * r;
    y = cosf(t * 5.0f) * r;
    z = sinf(t * 4.0f) * r;
    v3_set(&w->wob, x, y, z);
    v3_add(&mv, &w->wob);
    v3_add(&pos, &mv);
    enemy_task_set_pos(obj, &pos);
}

/* 0x00b01ae8 */
void en_garuda_nest_pose_model(void *obj, void *wp)
{
    EnGarudaNestWork *w = wp;
    Vec3 scl;

    ulMatrixPush();
    matrix_unit_set(0);
    anim_start(w->anim, (float)(unsigned int)(w->anim[1] - 1) * beat_clock_get_bar());
    anim_apply_node((unsigned int *)system_pnb_entry_get(0));
    yuki_matrix_get_scale(&scl);
    enemy_task_set_scl(obj, &scl);
    ulMatrixPop();
}

/* 0x00b01b98 */
float en_garuda_nest_calc_glow(void *obj, EnGarudaNestWork *w)
{
    float t = w->glow.frame;

    if (t < 6.0f)
        t = ease_sine_in_out(t / 6.0f);
    else if (t < 15.0f)
        t = 1.0f;
    else
        t = ease_sine_in_out_rev((t - 15.0f) / 24.0f);
    return t;
}

/* 0x00b01c28 */
void en_garuda_nest_hit_check(void *obj, void *wp)
{
    EnGarudaNestWork *w = wp;
    LockObj *lock = &w->lock;
    Vec3 push;
    Vec3 pos;
    int hits;
    float spin_k, spin_rnd;

    if (!lock->owner || *(int *)((char *)lock->owner - 36) & 1)
        return;
    if ((lock->flags & (LOCK_HIT | LOCK_LOCKABLE)) != (LOCK_HIT | LOCK_LOCKABLE))
        return;
    hits = lock->life;
    v3_set(&push, 0.0f, 0.0f, -EN_GARUDA_NEST_REC->recoil);
    util_dir_to_player_frame(&push.x, &w->dir.x);
    spin_k = EN_GARUDA_NEST_REC->spin_k;
    spin_rnd = EN_GARUDA_NEST_REC->spin_rand;
    i3_mul(w->spin, w->rate, spin_k);
    w->spin[0] = w->spin[0] + w->spin[0] * (RAND_FLOAT_A() * (spin_rnd + spin_rnd) - spin_rnd);
    w->spin[1] = w->spin[1] + w->spin[1] * (RAND_FLOAT_A() * (spin_rnd + spin_rnd) - spin_rnd);
    en_shiva_break_spawn(obj, enemy_task_pos_ptr(obj), enemy_task_rot_ptr(obj), 8);
    if (hits <= 0) {
        lock->flags &= ~LOCK_LOCKABLE;
        enemy_status_score_popup(enemy_task_get_status_slot(obj), enemy_task_pos_ptr(obj), EN_GARUDA_NEST_REC->score, lockon_hits_max_shot_cnt(lock));
        enemy_task_set_state(obj, 3);
    }
    enemy_task_get_pos(obj, &pos);
    ulMatrixApplyPoint((float *)ulMatrixCur, &pos.x, &pos.x);
    project_point(&pos, &pos);
    if (!hits)
        SoundRhythmSe_EnemyPos(en_garuda_nest_init_arg, 1, &pos.x);
    else if (!((lock->life_max - hits) & 1))
        SoundRhythmSe_EnemyPos(en_garuda_nest_init_arg, 0, &pos.x);
    else
        SoundRhythmSe_EnemyPos(en_garuda_nest_init_arg, 2, &pos.x);
}

/* steer(cast) */
/* 0x00b01ec0 */
void en_garuda_nest_calc_spawn_point(void *obj, EnGarudaNestWork *w, Vec3 *pos, int *ang)
{
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = enemy_task_pos_ptr(obj);
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_garuda_nest_rotate_yxz(enemy_task_rot_ptr(obj));
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_scale(ulMatrixCur, enemy_task_scl_ptr(obj)->x, enemy_task_scl_ptr(obj)->y, enemy_task_scl_ptr(obj)->z);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, (float *)&en_garuda_nest_spawn_mtx);
    ulMatrixGetTranslation((float *)ulMatrixCur, &pos->x);
    ((void (*)(int *))model_camera_angles)(ang);
    util_rotate_xyz(ang, 0.0f, 0.0f, -EN_GARUDA_NEST_REC->dist, &w->dir);
    ulMatrixPop();
}

/* steer(cast) */
/* 0x00b01fe0 */
void en_garuda_nest_spawn_garuda(void *obj, EnGarudaNestWork *w)
{
    EnGarudaArg place;
    Task *parent;

    en_garuda_nest_calc_spawn_point(obj, w, &place.pos, place.ang);
    place.unk_01 = 0;
    place.link = enemy_task_get_link(obj);
    place.pattern = (int)(((float (*)(float))float_frac)(enemy_task_get_param(obj, 1)) * 1000.0f + 0.5f);
    place.shot = 1.0f;
    place.vec.x = w->vec.x;
    place.vec.y = w->vec.y;
    place.vec.z = w->vec.z;
    parent = TASK_OF(obj)->parent;
    en_garuda_create(parent ? parent + 1 : 0, &place);
    en_garuda_se_play();
}

/* steer(cast) */
/* 0x00b020a8 */
void en_garuda_nest_state_wait(void *obj, EnGarudaNestWork *w)
{
    int phase;

    switch (w->step) {
    case 0:
        w->t = 1.0f;
        if (!w->atk_cnt)
            w->t_max = (int)enemy_task_get_param(obj, 1);
        else
            w->t_max = (int)enemy_task_get_param(obj, 0);
        w->step++;
        w->next_step++;
    case 1:
        w->t += 1.0f;
        if (w->t_max < w->t) {
            do { phase = (int)(((float (*)(float))float_frac)(enemy_task_get_param(obj, 0)) * 1000.0f + 0.5f); } while (0);
            if (phase > 0)
                phase = 2;
            else
                phase = 1;
            do {
                do { w->next_step = 0; } while (0);
                w->next = phase;
            } while (0);
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* steer(cast) */
/* 0x00b021e0 */
void en_garuda_nest_state_launch(void *obj, EnGarudaNestWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    float frm;
    int n, cnt;

    switch (w->step) {
    case 0:
        w->flags |= 2;
        dc_object_set_clip(gfx, w->clip);
        gfx->frame = 0.0f;
        w->gen_cnt = 0;
        w->t = 1.0f;
        w->t_max = enemy_task_get_param(obj, 2);
        w->step++;
        w->next_step++;
    case 1:
        if (w->flags & 4) {
            w->t += 1.0f;
            if (w->t_max < w->t) {
                dc_object_set_clip(gfx, w->clip);
                w->t = 1.0f;
                w->flags &= ~4;
            }
        } else {
            dc_object_advance_time(gfx);
            frm = gfx->frame;
            if (!(w->flags & 1) && 1.0f <= frm) {
                en_garuda_nest_spawn_garuda(obj, w);
                w->flags |= 1;
                w->gen_cnt++;
            }
            if (gfx->flags2 & 1) {
                gfx->speed = -gfx->speed;
                w->flags ^= 2;
            }
            if (fabsf(frm) < FLT_EPSILON) {
                gfx->frame = 0.0f;
                w->flags &= ~1;
                if (w->gen_cnt >= 4) {
                    cnt = ++w->atk_cnt;
                    n = (int)(((float (*)(float))float_frac)(enemy_task_get_param(obj, 0)) * 1000.0f + 0.5f);
                    if (cnt >= n) {
                        do {
                            w->next_step = 0;
                            w->next = 1;
                        } while (0);
                    } else {
                        w->next = 0;
                        w->next_step = 0;
                    }
                } else {
                    dc_object_set_clip(gfx, 0);
                    w->flags |= 4;
                }
            }
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        else
            break;
    case 2:
        dc_object_set_clip(gfx, 0);
        w->flags &= ~2;
        break;
    }
}

/* 0x00b02470 */
void en_garuda_nest_exec_normal(void *obj, EnGarudaNestWork *w)
{
    do {
        w->phase = w->next;
        w->step = w->next_step;
    } while (0);
    if (enemy_task_get_phase(obj) == 1) {
        if (en_garuda_nest_states[w->phase])
            en_garuda_nest_states[w->phase](obj, w);
        if (util_ahead_of_player(enemy_task_pos_ptr(obj), 20.0f))
            enemy_task_set_state(obj, 2);
    }
}

/* 0x00b02518 */
void en_garuda_nest_exec_destroyed(void *obj, EnGarudaNestWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 1) {
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 24;
        desc.models = stage_models;
        desc.kind = 0;
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        v3_set(&desc.v, 0.0f, 0.0f, 0.0f);
        enemy_shatter_create(obj, &desc, 0, 0);
        enemy_task_next_phase(obj);
    }
}

/* 0x00b025c0 */
void en_garuda_nest_exec(void *obj, EnGarudaNestWork *w)
{
    Vec3 pos;
    int ang[3];
    Vec3 vel;
    void (**tbl)(void *obj, EnGarudaNestWork *w);
    unsigned int state;

    if (enemy_task_get_phase(obj) == 1) {
        enemy_task_get_pos(obj, &pos);
        enemy_task_get_rot(obj, ang);
        v3_scale(&w->dir, 0.97f);
        i3_scale(w->spin, 0.99f);
        v3_add(&pos, &w->dir);
        i3_add(ang, w->spin);
        v3_set(&vel, 0.0f, 0.0f, EN_GARUDA_NEST_REC->speed * FRAME_SEC);
        util_dir_to_player_frame(&vel.x, &vel.x);
        v3_add(&pos, &vel);
        i3_add(ang, w->rate);
        enemy_task_set_pos(obj, &pos);
        enemy_task_set_rot(obj, ang);
        en_garuda_nest_swing(obj, w);
    }
    tbl = en_garuda_nest_exec_table;
    state = enemy_task_get_pstate(obj);
    if (tbl[state])
        tbl[state](obj, w);
}

/* 0x00b027e8 */
void en_garuda_nest_pre_exec(void *obj, void *w)
{
    en_garuda_nest_pose_model(obj, w);
    en_garuda_nest_hit_check(obj, w);
}

/* 0x00b02820 */
void en_garuda_nest_post_exec(void *obj, float *w)
{
    w[46] = ((float *)enemy_task_gfx_ptr(obj))[15];
}

/* steer(cast) */
/* 0x00b02850 */
void en_garuda_nest_disp_glow(void *obj, EnGarudaNestWork *w)
{
    int blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);

    draw_push_strip_flags();
    draw_push_override_color();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    en_garuda_nest_glow_col[0] = ease_sine_in_out_2x_neg(beat_clock_get_bar());
    draw_set_override_color(en_garuda_nest_glow_col);
    dc_object_draw(&w->body);
    if (w->flags & 2) {
        en_garuda_nest_glow_col[0] = en_garuda_nest_calc_glow(obj, w) - 1.0f;
        draw_set_override_color(en_garuda_nest_glow_col);
        vu0_matrix_mul(ulMatrixCur, ulMatrixCur, (float *)&en_garuda_nest_glow_mtx);
        dc_object_draw(&w->glow);
    }
    draw_pop_override_color();
    draw_pop_strip_flags();
    draw_set_override_flags(blend);
}

/* 0x00b02950 */
void en_garuda_nest_disp_body(void *obj, EnGarudaNestWork *w)
{
    Vec3 *pos = enemy_task_pos_ptr(obj);
    ModelPart *gfx = enemy_task_gfx_ptr(obj);

    if (!view_sphere_visible(pos, gfx->radius))
        return;
    ulGxSetTextureList(stage_textures);
    ulMatrixPush();
    do {
        Vec3 *p_ = pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_garuda_nest_rotate_yxz(enemy_task_rot_ptr(obj));
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_scale(ulMatrixCur, enemy_task_scl_ptr(obj)->x, enemy_task_scl_ptr(obj)->y, enemy_task_scl_ptr(obj)->z);
    dc_object_draw(gfx);
    en_garuda_nest_disp_glow(obj, w);
    ulMatrixPop();
}

/* steer(cast) */
/* 0x00b02a70 */
void en_garuda_nest_disp(void *obj, EnGarudaNestWork *w)
{
    void (**tbl)(void *obj, EnGarudaNestWork *w) = en_garuda_nest_disp_table;
    unsigned int state;
    Vec3 pos;
    Vec3 vel;
    int ang[3];
    Vec3 scl;
    ModelPart *gfx;

    state = enemy_task_get_pstate(obj);
    if (tbl[state])
        tbl[state](obj, w);
    if (debug_flags_test(16) && EN_GARUDA_NEST_REC->debug) {
        debug_set_origin(10, 10);
        enemy_task_get_pos(obj, &pos);
        debug_printf("P:%d,%d,%d", (int)pos.x, (int)pos.y, (int)pos.z);
        debug_newline();
        enemy_task_get_param_vec(obj, &vel);
        debug_printf("VEC:%f,%f,%f", vel.x, vel.y, vel.z);
        debug_newline();
        enemy_task_get_rot(obj, ang);
        debug_printf("A:%d,%d,%d", (int)(ang[0] * DEG_PER_ANG), (int)(ang[1] * DEG_PER_ANG),
                     (int)(ang[2] % 65536 * DEG_PER_ANG));
        debug_newline();
        enemy_task_get_scl(obj, &scl);
        debug_printf("SCL:%f,%f,%f", scl.x, scl.y, scl.z);
        debug_newline();
        gfx = enemy_task_gfx_ptr(obj);
        debug_printf("FRM:%d/%d", (int)gfx->frame, (int)gfx->frames);
        debug_newline();
        debug_printf("MOD:%d", w->phase);
        debug_newline();
        debug_printf("C_G:%d/%d", w->gen_cnt, 4);
        debug_newline();
        debug_printf("C_A:%d/%d", w->atk_cnt, (int)(((float (*)(float))float_frac)(enemy_task_get_param(obj, 0)) * 1000.0f + 0.5f));
        debug_newline();
        debug_printf("SHW:%d", (w->flags >> 1) & 1);
        debug_newline();
    }
}

/* steer(cast) */
/* 0x00b02d30 */
void en_garuda_nest_init(void *obj, EnGarudaNestWork *w)
{
    Vec3 vec;
    ModelPart *body = &w->body;
    ModelPart *glow = &w->glow;
    LockObj *lock = &w->lock;
    float fps = FPS;
    float x, y;
    int *mdl;

    w->pos = enemy_task_pos_ptr(obj);
    w->param_vec = enemy_task_param_vec_ptr(obj);
    w->ang = enemy_task_rot_ptr(obj);
    w->scale = enemy_task_scl_ptr(obj);
    x = enemy_task_get_param(obj, 3);
    y = enemy_task_get_param(obj, 4);
    v3_set(&vec, x, y, enemy_task_get_param(obj, 5));
    util_dir_to_player_frame(&vec.x, &w->vec.x);
    enemy_task_clear_flags(obj, 64);
    lockon_obj_init(lock, w, enemy_task_pos_ptr(obj), 0, en_garuda_nest_lock_name, enemy_task_get_status_slot(obj), EN_GARUDA_NEST_REC->he,
                  EN_GARUDA_NEST_REC->he, 0);
    lockon_entry(lock);
    w->lock.flags |= LOCK_SILENT;
    ((ModelPart *)enemy_task_gfx_ptr(obj))->speed = EN_GARUDA_NEST_REC->spd_motion * FRAME_SEC;
    {
        struct { int v[3]; } *rate = (void *)w->rate;

        rate->v[0] = 121;
        rate->v[1] = 364;
        rate->v[2] = 0;
    }
    dc_object_init(body, *((struct DcNode **)pnb_get_entry(stage_models, 22)), 0, 0);
    mdl = ((int *)pnb_get_entry(stage_models, 9));
    dc_object_init(glow, *mdl, *((struct DcClip **)pnb_get_entry(stage_models, 10)), 0);
    dc_object_rebuild(body);
    dc_object_rebuild(glow);
    w->clip = *((int *)pnb_get_entry(stage_models, 15));
    w->anim = (int *)*((int *)pnb_get_entry(stage_models, 17));
    enemy_task_set_appear(obj, 2, (int)(EN_GARUDA_NEST_REC->frm_appear * fps));
    enemy_task_set_vanish(obj, 2, (int)(EN_GARUDA_NEST_REC->frm_away * fps));
}

/* 0x00b02f90 */
void en_garuda_nest_release(void *obj, char *w)
{
    lockon_remove((LockObj *)(w + 0x120));
    dc_object_release((struct MapPart *)(w + 0x20));
    dc_object_release((struct MapPart *)(w + 0x7c));
}

/* 0x00b02fc8 */
int en_garuda_nest_create(void *parent, void *place)
{
    ObjDesc desc;
    ObjFuncs funcs;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&funcs, 0, sizeof funcs);
    funcs.fn[0] = en_garuda_nest_init;
    funcs.fn[1] = en_garuda_nest_release;
    funcs.fn[2] = en_garuda_nest_exec;
    funcs.fn[3] = en_garuda_nest_disp;
    funcs.fn[4] = en_garuda_nest_pre_exec;
    funcs.fn[5] = en_garuda_nest_post_exec;
    desc.name = en_garuda_nest_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = 14;
    desc.model_no2 = -1;
    desc.hp = EN_GARUDA_NEST_REC->he;
    desc.score = EN_GARUDA_NEST_REC->score;
    desc.size = 372;
    desc.time = 0.0f;
    desc.wob_amp = 0;
    desc.wob_spd = 0;
    return enemy_task_task_create(parent, &desc);
}

/* steer(cast) */
/* 0x00b030f8 */
void en_garuda_nest_nodes_init(void)
{
    int mdl = *((int *)pnb_get_entry(stage_models, 14));

    node_tree_draw_matching_at(mdl, 0x2000000, 1, (int)&en_garuda_nest_spawn_mtx, 0);
    node_tree_draw_matching_at(mdl, 0x1000000, 1, (int)&en_garuda_nest_glow_mtx, 0);
}

/* 0x00b03160 */
void en_garuda_nest_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
