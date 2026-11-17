/*
 * Small Shiva, Area 2 enemy: one lock-on core; slides side to side firing, drifts when hit,
 * escapes when its time is up or too far from the camera.
 */

#include "stg02.h"
#include "mathdef.h"
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
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "pnb.h"
#include "sound_beat.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_shiva_mini.h"
#include "en_shiva_break.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"

extern void (*en_shiva_mini_exec_table[])(void *o, void *w);
extern void (*en_shiva_mini_disp_table[])(void *o, void *w);
extern ParamRec en_shiva_mini_params;
extern VarDef en_shiva_mini_vardefs[12];
extern int en_shiva_mini_core_mtx;

typedef struct EnShivaMiniWork {
    Vec3 *pos;                  /* 0x00: the object's vectors (enemy_task_pos_ptr ...) */
    Vec3 *param_vec;                  /* 0x04 */
    int *ang;                   /* 0x08 */
    Vec3 *scale;                /* 0x0c */
    void *gfx;                  /* 0x10 */
    int phase;                  /* 0x14: index into en_shiva_mini_states */
    int next;                   /* 0x18 */
    unsigned int step;          /* 0x1c */
    int next_step;              /* 0x20 */
    ModelPart glow;                /* 0x24 */
    ModelPart core;                /* 0x80 */
    Vec3 vel;                   /* 0xdc: the placement's movement vector */
    Vec3 drift;                 /* 0xe8: worked off at drift_spd per second */
    Vec3 push;                  /* 0xf4: set when hit */
    float sway;                 /* 0x100: sideways offset */
    float sway_tgt;             /* 0x104 */
    float sway_spd;             /* 0x108 */
    Vec3 cpos;                  /* 0x10c: the core's position */
    int cang[3];                /* 0x118 */
    LockObj lock;                  /* 0x124 */
    float spd;                  /* 0x164: escape speed */
    float spd_tgt;              /* 0x168 */
    int flags;                  /* 0x16c */
    float t;                    /* 0x170: frames */
    float t_max;                /* 0x174 */
} EnShivaMiniWork;

/* en_shiva_mini_params: name, label, values */
typedef struct EnShivaMiniRec {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    int score;                  /* 0x0c */
    float frm_shot_interval;    /* 0x10 */
    float acc_escape;           /* 0x14 */
    float spd_slide_max;        /* 0x18 */
    float acc_slide;            /* 0x1c */
    float dst_slide_div;        /* 0x20 */
    float dst_slide_max1;       /* 0x24 */
    float dst_slide_max2;       /* 0x28 */
    float drift_spd;            /* 0x2c: per second */
    float push;                 /* 0x30: the push when hit */
    float far;                  /* 0x34: leaves beyond this distance from the eye */
} EnShivaMiniRec;

#define EN_SHIVA_MINI_REC ((EnShivaMiniRec *)&en_shiva_mini_params)
extern float en_shiva_mini_glow_color[4];
extern void (*en_shiva_mini_states[])(void *o, EnShivaMiniWork *w);
extern char en_shiva_mini_name[];
extern char en_shiva_mini_lock_name[];

/* 0x00b0bd78 */
void en_shiva_mini_vars_register(void)
{
    VarDef hdr;
    VarDef *def;
    int *val;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&hdr, 0, sizeof hdr);
    hdr.name = en_shiva_mini_params.name;
    hdr.fmt = en_shiva_mini_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&hdr);
    def = en_shiva_mini_vardefs;
    val = en_shiva_mini_params.v;
    for (i = 0; i < 12; i++) {
        def->ptr = val;
        val++;
        def++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_shiva_mini_vardefs);
}

/* 0x00b0be28 */
void en_shiva_mini_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b0be30 */
void en_shiva_mini_slide(void *obj, EnShivaMiniWork *w)
{
    Vec3 pos;
    Vec3 pl;
    float gap, spd, lim, gap_left, dst, a, one;

    gap = w->sway_tgt - w->sway;
    if (0.0f < gap) {
        w->sway_spd += EN_SHIVA_MINI_REC->acc_slide * 0.00027777778f;
        spd = w->sway_spd;
        lim = EN_SHIVA_MINI_REC->spd_slide_max * FRAME_SEC;
        { float t = spd < lim ? spd : lim; w->sway_spd = t; spd = t; }
    } else {
        w->sway_spd -= EN_SHIVA_MINI_REC->acc_slide * 0.00027777778f;
        spd = w->sway_spd;
        lim = -(EN_SHIVA_MINI_REC->spd_slide_max * FRAME_SEC);
        { float t = lim <= spd ? spd : lim; w->sway_spd = t; spd = t; }
    }
    w->sway = w->sway + spd;
    one = 1.0f;
    gap_left = w->sway_tgt - w->sway;
    if (fabsf(gap_left) < one) {
        enemy_task_get_pos(obj, &pos);
        player_get_pos(&pl);
        if (fabsf(pos.z - pl.z) < EN_SHIVA_MINI_REC->dst_slide_div)
            dst = EN_SHIVA_MINI_REC->dst_slide_max1 * 0.5f * (RAND_FLOAT_A() + one);
        else
            dst = EN_SHIVA_MINI_REC->dst_slide_max2 * 0.5f * (RAND_FLOAT_A() + one);
        w->sway_tgt = dst;
        if (0.0f < gap_left)
            w->sway_tgt = -dst;
    }
    player_get_frame_accel(&pos);
    vec3_scale_to_length(spd, &pos.x, &pos.x);
    v3_add(enemy_task_pos_ptr(obj), &pos);
}

/* steer(cast) */
/* 0x00b0c080 */
void en_shiva_mini_calc_core(void *obj, int *work)
{
    EnShivaMiniWork *w = (EnShivaMiniWork *)work;
    Vec3 pl;
    int ang[3];
    Vec3 to_pl;
    Vec3 *cpos;

    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = enemy_task_pos_ptr(obj);
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    {
        int *oang = ((int *)enemy_task_rot_ptr(obj));

        cpos = &w->cpos;
        en_shiva_mini_rotate_yxz(oang);
    }
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, (float *)&en_shiva_mini_core_mtx);
    ulMatrixGetTranslation((float *)ulMatrixCur, &cpos->x);
    ulMatrixPop();
    player_get_pos(&pl);
    v3_sub(&to_pl, &pl, cpos);
    do {
        ang[1] = (int)RAD_TO_ANG(atan2f(-to_pl.x, -to_pl.z));
        ang[0] = (int)RAD_TO_ANG(atan2f(to_pl.y, sqrtf(to_pl.x * to_pl.x + to_pl.z * to_pl.z)));
    } while (0);
    i3_set(w->cang, ((int *)enemy_task_rot_ptr(obj))[0], ang[1], 0);
    w->flags |= 1;
}

/* 0x00b0c238 */
void en_shiva_mini_calc_points(void *obj, int *w)
{
    if (!(w[91] & 1))
        en_shiva_mini_calc_core(obj, w);
    w[91] &= ~1;
}

/* 0x00b0c280 */
void en_shiva_mini_hit_check(void *obj, void *work)
{
    EnShivaMiniWork *w = work;
    LockObj *lock = &w->lock;

    if (!lock->owner || *(int *)((char *)lock->owner - 36) & 1)
        return;
    if ((lock->flags & (LOCK_HIT | LOCK_LOCKABLE)) != (LOCK_HIT | LOCK_LOCKABLE))
        return;
    {
        Vec3 *p = &w->push;

        float t_ = -EN_SHIVA_MINI_REC->push;
        ((union { int i; float f; } *)&p->x)->i = 0;
        ((union { int i; float f; } *)&p->z)->i = 0;
        p->y = t_;
    }
    if (lock->life <= 0) {
        lock->flags &= ~LOCK_LOCKABLE;
        enemy_status_score_popup(enemy_task_get_status_slot(obj), &w->cpos, EN_SHIVA_MINI_REC->score, lockon_hits_max_shot_cnt(lock));
        enemy_task_set_state(obj, 3);
    }
    en_shiva_break_spawn(obj, &w->cpos, w->cang, 8);
}

/* 0x00b0c3a8 */
void en_shiva_mini_state_move_in(void *obj, EnShivaMiniWork *w)
{
    Vec3 pos;
    Vec3 vel;
    float frm;

    switch (w->step) {
    case 0:
        w->t = 1.0f;
        frm = enemy_task_get_param(obj, 0);
        w->t_max = frm - vec3_length(enemy_task_param_vec_ptr(obj)) / 0.029999971f;
        w->step++;
        w->next_step++;
    case 1:
        enemy_task_get_pos(obj, &pos);
        enemy_task_get_param_vec(obj, &vel);
        v3_add(&pos, &vel);
        enemy_task_set_pos(obj, &pos);
        w->t += 1.0f;
        if (w->t_max < w->t)
            do {
                w->next_step = 0;
                w->next = 2;
            } while (0);
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
/* 0x00b0c520 */
void en_shiva_mini_state_escape(void *obj, EnShivaMiniWork *w)
{
    Vec3 vel;
    Vec3 pos;
    Vec3 dir;
    Vec3 eye;
    float len, spd, tgt, nspd, clip, cspd;

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
        do { do { do { spd = w->spd; } while (0); } while (0); } while (0);
        do { do { tgt = w->spd_tgt; } while (0); } while (0);
        if (spd < tgt) {
            nspd = spd + EN_SHIVA_MINI_REC->acc_escape * 0.00027777778f;
            cspd = nspd < tgt ? nspd : tgt;
            w->spd = cspd;
        } else {
            nspd = spd - EN_SHIVA_MINI_REC->acc_escape * 0.00027777778f;
            cspd = tgt <= nspd ? nspd : tgt;
            w->spd = cspd;
        }
        enemy_task_get_pos(obj, &pos);
        enemy_task_get_param_vec(obj, &dir);
        vec3_scale_to_length(w->spd, &dir.x, &dir.x);
        v3_add(&pos, &dir);
        enemy_task_set_pos(obj, &pos);
        clip = EN_SHIVA_MINI_REC->far;
        camera_get_eye(&eye);
        if (vec3_distance_squared(&pos, &eye) > clip * clip)
            enemy_task_set_state(obj, 2);
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b0c798 */
void en_shiva_mini_state_wait(void *obj, EnShivaMiniWork *w)
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

/* 0x00b0c860 */
void en_shiva_mini_exec_normal(void *obj, EnShivaMiniWork *w)
{
    Vec3 pos;
    Vec3 step;
    float len, rate;

    do {
        w->phase = w->next;
        w->step = w->next_step;
    } while (0);
    if (enemy_task_get_phase(obj) == 1) {
        en_shiva_mini_slide(obj, w);
        enemy_task_get_pos(obj, &pos);
        v3_scale(&w->vel, 0.97f);
        v3_add(&pos, &w->vel);
        v3_scale(&w->push, 0.9f);
        v3_add(&pos, &w->push);
        len = vec3_length(&w->drift);
        if (EN_SHIVA_MINI_REC->drift_spd * FRAME_SEC < len) {
            rate = EN_SHIVA_MINI_REC->drift_spd * FRAME_SEC / len;
            v3_mul(&step, &w->drift, rate);
            v3_sub(&w->drift, &w->drift, &step);
            v3_sub(&pos, &pos, &step);
        } else {
            v3_sub(&pos, &pos, &w->drift);
            v3_set(&w->drift, 0.0f, 0.0f, 0.0f);
        }
        enemy_task_set_pos(obj, &pos);
        if (en_shiva_mini_states[w->phase])
            en_shiva_mini_states[w->phase](obj, w);
    }
}

/* 0x00b0cab0 */
void en_shiva_mini_exec_destroyed(void *obj, EnShivaMiniWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 1) {
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 108;
        desc.models = stage_models;
        desc.kind = 0;
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        enemy_task_get_hit_imp(obj, &desc.v);
        enemy_shatter_create(obj, &desc, 0, 0);
        enemy_task_next_phase(obj);
    }
}

/* 0x00b0cb58 */
void en_shiva_mini_exec(void *obj, void *w)
{
    void (**tab)(void *obj, void *w) = en_shiva_mini_exec_table;
    unsigned int i = enemy_task_get_pstate(obj);

    if (tab[i])
        tab[i](obj, w);
}

/* 0x00b0cbb8 */
void en_shiva_mini_pre_exec(void *obj, void *w)
{
    en_shiva_mini_hit_check(obj, w);
}

/* 0x00b0cbc0 */
void en_shiva_mini_post_exec(void *obj, int *w)
{
    float *gfx;

    en_shiva_mini_calc_points(obj, w);
    gfx = enemy_task_gfx_ptr(obj);
    gfx[15] = gfx[16] * beat_clock_get_bar();
}

/* steer(cast) */
/* 0x00b0cc00 */
void en_shiva_mini_disp_glow(void *obj, EnShivaMiniWork *w)
{
    int blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);

    draw_push_strip_flags();
    draw_push_override_color();
    ulMatrixPush();
    en_shiva_mini_glow_color[0] = ease_sine_in_out_2x_neg(beat_clock_get_bar());
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    draw_set_override_color(en_shiva_mini_glow_color);
    dc_object_draw(&w->glow);
    ulMatrixPop();
    draw_pop_override_color();
    draw_pop_strip_flags();
    draw_set_override_flags(blend);
}

/* 0x00b0cca8 */
void en_shiva_mini_disp_body(void *obj, EnShivaMiniWork *w)
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
    en_shiva_mini_rotate_yxz(enemy_task_rot_ptr(obj));
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(gfx);
    en_shiva_mini_disp_glow(obj, w);
    ulMatrixPop();
    ulMatrixPush();
    do {
        Vec3 *p_ = &w->cpos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_shiva_mini_rotate_yxz(w->cang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(&w->core);
    ulMatrixPop();
}

/* 0x00b0cdd0 */
void en_shiva_mini_disp(void *obj, void *w)
{
    void (**tab)(void *obj, void *w) = en_shiva_mini_disp_table;
    unsigned int i = enemy_task_get_pstate(obj);

    if (tab[i])
        tab[i](obj, w);
}

/* steer(cast) */
/* 0x00b0ce30 */
void en_shiva_mini_init(void *obj, EnShivaMiniWork *w)
{
    ModelPart *glow = &w->glow;
    LockObj *lock = &w->lock;

    w->pos = enemy_task_pos_ptr(obj);
    w->param_vec = enemy_task_param_vec_ptr(obj);
    w->ang = enemy_task_rot_ptr(obj);
    w->scale = enemy_task_scl_ptr(obj);
    w->gfx = enemy_task_gfx_ptr(obj);
    enemy_task_clear_flags(obj, 64);
    lockon_obj_init(lock, obj, &w->cpos, 0, en_shiva_mini_lock_name, enemy_task_get_status_slot(obj), EN_SHIVA_MINI_REC->hp, EN_SHIVA_MINI_REC->hp, 0);
    lockon_entry(lock);
    dc_object_init(glow, *((struct DcNode **)pnb_get_entry(stage_models, 104)), 0, 0);
    dc_object_init(&w->core, *((struct DcNode **)pnb_get_entry(stage_models, 105)), 0, 0);
    dc_object_rebuild(glow);
    dc_object_rebuild(&w->core);
    enemy_task_set_appear(obj, 0, 0);
    enemy_task_set_vanish(obj, 0, 0);
}

/* 0x00b0cf80 */
void en_shiva_mini_release(void *obj, char *w)
{
    lockon_remove((LockObj *)(w + 0x124));
    dc_object_release((struct MapPart *)(w + 0x24));
    dc_object_release((struct MapPart *)(w + 0x80));
}

/* 0x00b0cfb8 */
int en_shiva_mini_create(void *parent, void *place)
{
    ObjDesc desc;
    ObjFuncs funcs;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&funcs, 0, sizeof funcs);
    funcs.fn[0] = en_shiva_mini_init;
    funcs.fn[1] = en_shiva_mini_release;
    funcs.fn[2] = en_shiva_mini_exec;
    funcs.fn[3] = en_shiva_mini_disp;
    funcs.fn[4] = en_shiva_mini_pre_exec;
    funcs.fn[5] = en_shiva_mini_post_exec;
    desc.name = en_shiva_mini_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = 106;
    desc.model_no2 = 107;
    desc.size = 376;
    desc.time = 0.0f;
    desc.wob_amp = 0;
    desc.wob_spd = 0;
    return enemy_task_task_create(parent, &desc);
}

/* steer(cast) */
/* 0x00b0d0c0 */
void en_shiva_mini_nodes_init(void)
{
    node_tree_draw_matching_at(*((int *)pnb_get_entry(stage_models, 106)), 0x2000000, 1, (int)&en_shiva_mini_core_mtx, 0);
}

/* 0x00b0d100 */
void en_shiva_mini_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
