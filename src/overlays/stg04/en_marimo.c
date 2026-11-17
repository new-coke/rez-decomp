/*
 * Marimo, Area 4 enemy: base and two containers, each a lock-on target. Releases hoppers at
 * intervals, palette cycling, then escapes; broken parts fly off as en_marimo_break.
 */

#include "stg04.h"
#include "mathdef.h"
#include "anim.h"
#include "color.h"
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
#include "player_status.h"
#include "pnb.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_marimo.h"
#include "en_hopper.h"
#include "en_marimo_break.h"
#include "../common/en_break.h"
#include "../common/en_perfectwall.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"
#include "../common/util.h"
#include "task.h"

extern ParamRec en_marimo_params;
extern VarDef en_marimo_vardefs[17];
extern int en_marimo_init_arg;

/* 856 bytes beyond the object */
typedef enum { MARIMO_STEP_START } MarimoStep;

typedef enum { MARIMO_NSTEP_NONE = -1 } MarimoNextStep;

typedef struct EnMarimoWork {
    Vec3 *pos;
    Vec3 *vel;                  /* 0x04 */
    int *ang;                   /* 0x08 */
    Vec3 *scale;                /* 0x0c */
    unsigned int phase;         /* 0x10: index into en_marimo_states */
    int next;                   /* 0x14 */
    MarimoStep step;            /* 0x18 */
    MarimoNextStep next_step;   /* 0x1c */
    ModelPart part_a;              /* 0x20: models 88/89, follows the body's frame */
    ModelPart part_b;              /* 0x7c: models 76/77, follows the object's frame */
    ModelPart body;                /* 0xd8: models 85/86 */
    ModelPart cont[2];             /* 0x134: the containers (models 92/93, 94/95) */
    int pal_a;                  /* 0x1ec: model 90, part_a's palette */
    int pal_b;                  /* 0x1f0: model 78, part_b's palette */
    int pal_wire;               /* 0x1f4: model 91, the containers' palette */
    float fade;                 /* 0x1f8: colour offset while fading (-1..0) */
    float pal_pos;                 /* 0x1fc: position in pal_wire */
    float pal_frm;                 /* 0x200: its frame count */
    Vec3 off;                   /* 0x204: swing offset */
    float t;                    /* 0x210: swing time */
    Vec3 knock;                 /* 0x214 */
    Vec3 move;                  /* 0x220: the eased part of the move */
    int fire;                   /* 0x22c: parameter 0 of the last spawned hopper */
    Vec3 cpt[2];                /* 0x230: the containers' points */
    char pad_248[48];
    Vec3 bpt[1];                /* 0x278: the base point */
    LockObj lock[3];               /* 0x284: the base, the containers */
    int flags;                  /* 0x344: 1 placed, 0x10 << i container broken, 0x100 fading, 0x200 shown, 1024 << n piece broken */
    float cnt;                  /* 0x348 */
    float cnt_end;              /* 0x34c */
    int nshot;                  /* 0x350 */
    int nfire;                  /* 0x354 */
} EnMarimoWork;

extern unsigned int *en_marimo_model_nodes;
extern ObjStateFn en_marimo_exec_table[];

/* en_marimo_params, 17 words */
typedef struct EnMarimoRec {
    char *name;
    char *label;                /* 0x04 */
    int hp_vital;               /* 0x08: the base's lock */
    int hp_core;                /* 0x0c: the containers' locks */
    int score_vital;            /* 0x10 */
    int score_core;             /* 0x14 */
    int score_bonus;            /* 0x18 */
    float spd_swing;            /* 0x1c */
    float r_swing;              /* 0x20 */
    float spd_move;             /* 0x24: per second */
    float pow_move;             /* 0x28: safe_powf exponent */
    float spd_escape;           /* 0x2c */
    float pow_escape;           /* 0x30 */
    float frm_open_close;       /* 0x34: seconds */
    float dst_away_z;           /* 0x38 */
    float pow_shot;             /* 0x3c */
    float pow_laser;            /* 0x40 */
    unsigned int col_wire;      /* 0x44 */
    int test;                   /* 0x48 */
} EnMarimoRec;

#define EN_MARIMO_REC ((EnMarimoRec *)&en_marimo_params)

/* the en_marimo_break task to start on one node */
typedef struct EnMarimoCbArg {
    void *o;
    EnMarimoBreakArg *brk;            /* 0x04 */
    int node;                   /* 0x08 */
    char pad_0c[4];
    EnMarimoBreakArg arg;           /* 0x10 */
} EnMarimoCbArg;

extern void (*en_marimo_states[])(void *o, EnMarimoWork *w);
extern void (*en_marimo_disp_table[])(void *o, EnMarimoWork *w);
extern int en_marimo_part_slots[7];
extern int en_marimo_part_bits[7];
extern int en_marimo_part_break_types[7];
extern int en_marimo_part_models[7];
extern char en_marimo_name[];
extern char en_marimo_base_lock_name[];
extern char en_marimo_container_lock_name[];
extern Vec3 en_marimo_base_point;
extern Vec3 en_marimo_container_points[2];
extern char str_en_marimo_pos[];
extern char str_en_marimo_vec[];
extern char str_en_marimo_ang[];
extern char str_en_marimo_frm[];
extern char str_en_marimo_cnt[];
extern char str_en_marimo_mod[];
extern unsigned int *en_marimo_part_a_nodes[4];
extern unsigned int *en_marimo_body_nodes[7];
extern int en_marimo_part_model[7];
extern int en_marimo_part_motion[7];
extern int en_marimo_part_nodes[7];

/* 0x00b09550 */
void en_marimo_vars_register(void)
{
    VarDef grp_def;
    VarDef *def;
    int *val;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&grp_def, 0, sizeof grp_def);
    grp_def.name = en_marimo_params.name;
    grp_def.fmt = en_marimo_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&grp_def);
    def = en_marimo_vardefs;
    val = en_marimo_params.v;
    for (i = 0; i < 17; i++) {
        def->ptr = val;
        val++;
        def++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_marimo_vardefs);
}

/* 0x00b09600 */
void en_marimo_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b09608 */
int en_marimo_marker_draw(int obj, void *mark)
{
    int *mdl;

    ulGxSetTextureList(stage_textures);
    mdl = pnb_get_entry(stage_models, 74);
    return obj_draw_at(obj, mark, (void *)*mdl);
}

/* 0x00b09660 */
int en_marimo_vars_init(int *id)
{
    en_marimo_vars_register();
    en_marimo_init_arg = *id;
    return 0;
}

/* 0x00b09698 */
int en_marimo_vars_release(void)
{
    en_marimo_vars_unregister();
    return 0;
}

/* 0x00b096b8 */
void en_marimo_calc_lock_points(void *obj, EnMarimoWork *w)
{
    int fl = w->flags;
    ModelPart *body;

    if (enemy_task_get_pstate(obj) != 3) {
        ulMatrixPush();
        matrix_unit_set(0);
        do {
            Vec3 *p_ = enemy_task_pos_ptr(obj);

            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        en_marimo_rotate_yxz(enemy_task_rot_ptr(obj));
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        if (fl & 4)
            ulMatrixApplyPoint((float *)ulMatrixCur, &en_marimo_base_point.x, &w->bpt[0].x);
        else
            enemy_task_spawn(obj, 0x80000000, 1, w->bpt, 0, 0);
        if (fl & 2)
            matrix_apply_points(en_marimo_container_points, w->cpt, 2);
        else {
            body = &w->body;
            node_tree_collect_xforms_at(body->root, body->clip, body->frame, 0x80000000, 2, enemy_task_pos_ptr(obj), enemy_task_rot_ptr(obj),
                          enemy_task_scl_ptr(obj), w->cpt, 0, 0);
        }
        ulMatrixPop();
        w->flags |= 1;
    }
}

/* 0x00b09828 */
void en_marimo_calc_points(void *obj, EnMarimoWork *w)
{
    if (!(w->flags & 1))
        en_marimo_calc_lock_points(obj, w);
}

/* 0x00b09848 */
void en_marimo_get_shot_points(void *obj, EnMarimoWork *w, Vec3 *out)
{
    Vec3 pos;
    int ang[3];
    Vec3 scl;

    enemy_task_get_pos(obj, &pos);
    enemy_task_get_rot(obj, ang);
    enemy_task_get_scl(obj, &scl);
    node_tree_collect_xforms_at(w->body.root, w->body.clip, w->body.frame, 0x1000000, 2, &pos, ang, &scl, out, 0, 0);
}

/* 0x00b098e8 */
void en_marimo_part_shatter(void *obj, EnMarimoWork *w, int grp, int piece)
{
    SpawnDesc desc;
    ShatterParams prm;
    float mtx[7][16];
    ModelPart *body = &w->body;
    int bit = en_marimo_part_bits[grp * 3 + piece];

    w->flags |= 1024 << bit;
    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&prm, 0, sizeof prm);
    desc.kind = 0x80000;
    desc.models = stage_models;
    desc.type = en_marimo_part_break_types[grp * 3 + piece];
    prm.Gravity = 10.0f;
    prm.PowVec = 1.0f;
    prm.PowAng = 1.0f;
    prm.RtoVecDec = 0.94f;
    prm.RtoAngDec = 0.98f;
    prm.RotSpdPlayer = 1.0f;
    prm.TimeAlphaStart = 1.0f;
    prm.TimeAlphaEnd = 80.0f;
    prm.force_wire = 0;
    prm.pcol_wire = RGBA(0x34, 0x0f, 0, 0x80);
    prm.b_acc = 0;
    prm.frm_acc = 0.0f;
    anim_start((void *)body->clip, body->frame);
    node_tree_draw_matching_at(body->root, 0x4000000, bit + 1, (int)mtx, 1);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = enemy_task_pos_ptr(obj);

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_marimo_rotate_yxz(enemy_task_rot_ptr(obj));
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, mtx[bit]);
    enemy_shatter_create(obj, &desc, (float *)&prm, 0);
    ulMatrixPop();
}

/* 0x00b09ac0 */
void en_marimo_shell_shatter(void *obj, EnMarimoWork *w, int shell)
{
    SpawnDesc desc;
    float mtx[16];
    ModelPart *gfx;

    if (shell == 1) {
        en_marimo_part_shatter(obj, w, 2, 0);
        return;
    }
    w->flags |= 0x20000 << shell;
    nj_fill8(&desc, 0, sizeof desc);
    desc.kind = 0x80000;
    desc.models = stage_models;
    desc.type = 106;
    desc.v.x = 0;
    desc.v.y = 0;
    desc.v.z = 0;
    gfx = enemy_task_gfx_ptr(obj);
    anim_start((void *)gfx->clip, gfx->frame);
    node_tree_draw_matching_at(gfx->root, 0x4000000, 1, (int)mtx, 1);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = enemy_task_pos_ptr(obj);

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_marimo_rotate_yxz(enemy_task_rot_ptr(obj));
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, mtx);
    enemy_shatter_create(obj, &desc, 0, 0);
    ulMatrixPop();
}

/* 0x00b09c08 */
int en_marimo_spawn_break_cb(void *part, int node, EnMarimoCbArg *cb)
{
    Vec3 pos;
    int ang[3];

    if (node == cb->node) {
        ulMatrixGetTranslation((float *)ulMatrixCur, &pos.x);
        switch (cb->brk->kind) {
        case 0:
        case 1:
        case 3:
            ulMatrixPush();
            vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
            model_camera_angles(ang);
            ulMatrixPop();
            break;
        case 2:
            model_camera_angles(ang);
            break;
        }
        cb->brk->pos = &pos;
        cb->brk->ang = ang;
        en_marimo_break_create(cb->o, cb->brk);
    }
    return 0;
}

/* 0x00b09cc8 */
void en_marimo_spawn_break(void *obj, EnMarimoWork *w, int kind)
{
    EnMarimoCbArg cb;
    ModelPart *body = &w->body;
    LockObj *lock;

    cb.o = obj;
    cb.brk = &cb.arg;
    cb.arg.kind = kind;
    switch ((unsigned int)kind) {
    case 0:
        cb.arg.lock = 0;
        cb.node = en_marimo_part_nodes[en_marimo_part_bits[2]];
        break;
    case 1:
        cb.arg.lock = 0;
        cb.node = en_marimo_part_nodes[en_marimo_part_bits[5]];
        break;
    case 2:
        cb.arg.lock = &w->lock[1];
        cb.arg.target = &w->cpt[0];
        cb.node = en_marimo_part_nodes[en_marimo_part_bits[2]];
        break;
    case 3:
        cb.arg.lock = &w->lock[2];
        cb.arg.target = &w->cpt[1];
        cb.node = en_marimo_part_nodes[en_marimo_part_bits[5]];
        break;
    }
    lock = cb.arg.lock;
    if (lock) {
        if (!lock->owner || (TASK_OF(lock->owner)->flags & TASK_F_END)
            || !(lock->flags & LOCK_LOCKABLE))
            cb.arg.lock = 0;
    }
    cb.arg.time = w->phase < 2 ? 0.0f : w->body.frame;
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = enemy_task_pos_ptr(obj);

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_marimo_rotate_yxz(enemy_task_rot_ptr(obj));
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_set_callback(body, en_marimo_spawn_break_cb, &cb);
    dc_object_draw(body);
    dc_object_set_callback(body, 0, 0);
    ulMatrixPop();
}

/* 0x00b09e88 */
void en_marimo_hit_check_base(void *obj, EnMarimoWork *w)
{
    LockObj *lock = &w->lock[0];

    if (lock->flags & LOCK_HIT) {
        if (lock->life <= 0) {
            enemy_status_score_popup(enemy_task_get_status_slot(obj), enemy_task_pos_ptr(obj), EN_MARIMO_REC->score_vital, lockon_hits_max_shot_cnt(lock));
            if ((w->flags & 0x30) == 0x30)
                player_status_add_score_event(en_marimo_base_lock_name, EN_MARIMO_REC->score_bonus);
            lock->flags &= ~LOCK_LOCKABLE;
            w->flags |= 0x100000;
            w->flags |= 0x400000;
            en_marimo_spawn_break(obj, w, 1);
            en_marimo_spawn_break(obj, w, 0);
            en_marimo_shell_shatter(obj, w, 1);
            if (!(w->flags & 0x10000))
                en_marimo_part_shatter(obj, w, 0, 1);
            if (!(w->flags & 0x8000))
                en_marimo_part_shatter(obj, w, 1, 1);
            if (!(w->flags & 0x10)) {
                w->flags |= 0x10;
                en_marimo_spawn_break(obj, w, 2);
            }
            if (!(w->flags & 0x20)) {
                w->flags |= 0x20;
                en_marimo_spawn_break(obj, w, 3);
            }
            enemy_task_set_state(obj, 3);
        } else if (lock->life == 4)
            en_marimo_shell_shatter(obj, w, 0);
        else
            en_break_spawn(obj, enemy_task_pos_ptr(obj), enemy_task_rot_ptr(obj), 1);
    }
}

/* 0x00b0a0e8 */
void en_marimo_hit_check_container(void *obj, EnMarimoWork *w, int cont)
{
    LockObj *lock = &w->lock[cont + 1];
    int hits;
    Vec3 scr;

    if (lock->flags & LOCK_HIT) {
        hits = lock->life;
        switch (hits) {
        case 0:
            lock->flags &= ~LOCK_LOCKABLE;
            enemy_status_score_popup(enemy_task_get_status_slot(obj), &w->cpt[cont], EN_MARIMO_REC->score_core, lockon_hits_max_shot_cnt(lock));
            w->flags |= 16 << cont;
            switch (cont) {
            case 0:
                w->flags |= 1024 << en_marimo_part_bits[2];
                en_marimo_spawn_break(obj, w, 2);
                break;
            case 1:
                w->flags |= 1024 << en_marimo_part_bits[5];
                en_marimo_spawn_break(obj, w, 3);
                break;
            }
            break;
        case 1:
            en_marimo_part_shatter(obj, w, cont, 1);
        case 2:
            w->flags |= 0x100000 << (cont * 2);
            en_marimo_part_shatter(obj, w, cont, 0);
            break;
        default:
            en_break_spawn(obj, &w->cpt[cont], enemy_task_rot_ptr(obj), 1);
            break;
        }
        ulMatrixApplyPoint((float *)ulMatrixCur, &w->cpt[cont].x, &scr.x);
        project_point(&scr, &scr);
        if (!hits)
            SoundRhythmSe_EnemyPos(en_marimo_init_arg, 3, &scr.x);
        else
            SoundRhythmSe_EnemyPos(en_marimo_init_arg, 2, &scr.x);
    }
}

/* 0x00b0a328 */
void en_marimo_hit_check(void *obj, EnMarimoWork *w)
{
    if (enemy_task_get_pstate(obj) != 3) {
        en_marimo_hit_check_base(obj, w);
        en_marimo_hit_check_container(obj, w, 0);
        en_marimo_hit_check_container(obj, w, 1);
    }
}

/* 0x00b0a3a0 */
void en_marimo_swing(void *obj, EnMarimoWork *w)
{
    Vec3 pos;
    float t, r;

    t = w->t += EN_MARIMO_REC->spd_swing;
    r = EN_MARIMO_REC->r_swing;
    enemy_task_get_pos(obj, &pos);
    v3_sub(&pos, &pos, &w->off);
    v3_set(&w->off, sinf(t * 3.0f) * r, cosf(t * 5.0f) * r, sinf(t * 4.0f) * r);
    v3_add(&pos, &w->off);
    enemy_task_set_pos(obj, &pos);
}

/* 0x00b0a4d0 */
void en_marimo_palette_start(void *obj, EnMarimoWork *w)
{
    w->flags |= 8;
    w->pal_pos = 0;
    w->pal_frm = 1.0f;
}

/* 0x00b0a4f0 */
void en_marimo_palette_update(void *obj, EnMarimoWork *w)
{
    float last, frm;

    if (w->flags & 8) {
        last = (float)(unsigned int)(((int *)w->pal_wire)[1] - 1);
        frm = w->pal_frm;
        w->pal_pos = last * frm / 24.0f;
        w->pal_frm = frm + 1.0f;
        if (24.0f < w->pal_frm) {
            w->pal_pos = last;
            w->flags &= ~8;
        }
    }
}

/* 0x00b0a588 */
void en_marimo_spawn_hoppers(void *obj, EnMarimoWork *w, int fire)
{
    Vec3 pt[2];
    EnMarimoPlace place;
    Task *parent;

    nj_fill8(&place, 0, sizeof place);
    enemy_task_get_rot(obj, place.ang);
    place.unk_01 = 0;
    place.link = enemy_task_get_link(obj);
    en_marimo_get_shot_points(obj, w, pt);
    if (!(w->flags & 0x10)) {
        v3_copy(&place.pos, &pt[0]);
        if (fire)
            place.prm[0] = 1.0f;
        else
            place.prm[0] = 0.0f;
        parent = TASK_OF(obj)->parent;
        en_hopper_create(parent ? parent + 1 : 0, (EnHopperPlace *)&place);
    }
    if (!(w->flags & 0x20)) {
        v3_copy(&place.pos, &pt[1]);
        place.prm[0] = w->fire ? 1.0f : 0.0f;
        parent = TASK_OF(obj)->parent;
        en_hopper_create(parent ? parent + 1 : 0, (EnHopperPlace *)&place);
    }
    w->fire = fire;
    en_marimo_palette_start(obj, w);
}

/* 0x00b0a6c0 */
void en_marimo_state_move_in(void *obj, EnMarimoWork *w)
{
    Vec3 travel;
    Vec3 pos;
    Vec3 vel;
    float nfrm, frm, end, rate;
    int i;
    float fade_frm;

    switch (w->step) {
    case 0:
        enemy_task_get_param_vec(obj, &travel);
        nfrm = vec3_length(&travel) / (EN_MARIMO_REC->spd_move * FRAME_SEC);
        if (0.0f < nfrm) {
            for (i = 2; i >= 0; i--) {
                LockObj *lock = &w->lock[i];

                lock->flags &= ~LOCK_LOCKABLE;
            }
            w->flags |= 0x100;
        }
        v3_set(&w->move, 0.0f, 0.0f, 0.0f);
        w->cnt = 1.0f;
        w->cnt_end = nfrm;
        w->step++;
        w->next_step++;
    case 1:
        frm = w->cnt;
        end = w->cnt_end;
        enemy_task_get_pos(obj, &pos);
        rate = frm / end;
        enemy_task_get_param_vec(obj, &vel);
        if (w->flags & 0x100) {
            fade_frm = 60.0f < w->cnt_end ? 60.0f : (int)w->cnt_end;
            if (frm < fade_frm)
                w->fade = w->cnt / fade_frm - 1.0f;
            else {
                int j;

                for (j = 2; j >= 0; j--) {
                    LockObj *lock = &w->lock[j];

                    lock->flags |= LOCK_LOCKABLE;
                }
                w->flags &= ~0x100;
            }
        }
        v3_sub(&pos, &pos, &w->move);
        v3_mul(&w->move, &vel, safe_powf(ease_sine_in_out(rate), EN_MARIMO_REC->pow_move));
        v3_add(&pos, &w->move);
        enemy_task_set_pos(obj, &pos);
        w->cnt += 1.0f;
        if (end < w->cnt) {
            w->next_step = 0;
            w->next = 1;
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b0aa10 */
void en_marimo_state_ready(void *obj, EnMarimoWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    ModelPart *body = &w->body;

    switch (w->step) {
    case 0:
        body->speed = gfx->speed = gfx->frames / FPS;
        w->step++;
        w->next_step++;
    case 1:
        dc_object_advance_time(gfx);
        dc_object_advance_time(body);
        w->part_a.frame = body->frame;
        w->part_b.frame = gfx->frame;
        if ((gfx->flags2 & 1) && (body->flags2 & 1)) {
            w->flags |= 6;
            w->cnt = 1.0f;
            w->cnt_end = enemy_task_get_param(obj, 1);
            do {
                w->next_step = w->step + 1;
            } while (0);
        }
        if (w->phase == w->next && w->step == w->next_step)
            break;
        w->step++;
    case 2:
        w->cnt += 1.0f;
        if (w->cnt_end < w->cnt) {
            do {
                w->next = 2;
                w->next_step = 0;
            } while (0);
        }
        break;
    }
}

/* steer(cast) */
/* 0x00b0ab80 */
void en_marimo_state_open(void *obj, EnMarimoWork *w)
{
    ModelPart *body = &w->body;

    switch (w->step) {
    case 0:
        w->flags |= 0x200;
        dc_object_set_clip(body, *((struct DcClip **)pnb_get_entry(stage_models, 87)));
        w->step++;
        w->next_step++;
        *(void **)&body->frame = 0;
        body->speed = body->frames / (EN_MARIMO_REC->frm_open_close * FPS);
    case 1:
        dc_object_advance_time(body);
        w->cont[1].frame = w->cont[0].frame = body->frame;
        if (20.0f < body->frame) {
            body->frame = 20.0f;
            do {
                w->next = 3;
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

/* steer(cast) */
/* 0x00b0ac98 */
void en_marimo_state_shoot(void *obj, EnMarimoWork *w)
{
    float tick;
    int fire;
    int nshot;
    float due;

    switch (w->step) {
    case 0:
        w->cnt = 1.0f;
        w->nshot = 0;
        w->nfire = 0;
        w->cnt_end = (int)enemy_task_get_param(obj, 0);
        w->step++;
        w->next_step++;
    case 1:
        tick = 1.0f;
        w->cnt += tick;
        if (w->cnt_end < w->cnt) {
            nshot = ++w->nshot;
            if ((int)(((float (*)(float))float_frac)(enemy_task_get_param(obj, 0)) * 1000.0f + 0.5f) < nshot) {
                w->next_step = 0;
                w->next = 4;
            } else {
                due = w->nshot * enemy_task_get_param(obj, 3);
                fire = 0;
                if (due > w->nfire) {
                    w->nfire++;
                    fire = 1;
                }
                en_marimo_spawn_hoppers(obj, w, fire);
                w->cnt = tick;
            }
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b0ae28 */
void en_marimo_state_close(void *obj, EnMarimoWork *w)
{
    ModelPart *body;

    switch (w->step) {
    case 0:
        w->step++;
        w->next_step++;
    case 1:
        body = &w->body;
        dc_object_advance_time(body);
        w->cont[1].frame = w->cont[0].frame = body->frame;
        if (body->flags2 & 1) {
            w->flags &= ~0x200;
            w->cnt = 1.0f;
            w->cnt_end = enemy_task_get_param(obj, 2);
            do {
                w->next_step = w->step + 1;
            } while (0);
        }
        if (w->phase == w->next && w->step == w->next_step)
            break;
        w->step++;
    case 2:
        w->cnt += 1.0f;
        if (w->cnt_end < w->cnt) {
            do {
                w->next = 5;
                w->next_step = 0;
            } while (0);
        }
        break;
    }
}

/* 0x00b0af50 */
void en_marimo_state_escape(void *obj, EnMarimoWork *w)
{
    Vec3 travel;
    Vec3 pos;
    Vec3 vel;
    float nfrm, frm, end, rate, left;
    int i;

    switch (w->step) {
    case 0:
        enemy_task_get_param_vec(obj, &travel);
        travel.z = -travel.z;
        enemy_task_set_param_vec(obj, &travel);
        nfrm = vec3_length(&travel) / (EN_MARIMO_REC->spd_escape * FRAME_SEC);
        v3_set(&w->move, 0.0f, 0.0f, 0.0f);
        w->cnt = 1.0f;
        w->step++;
        w->next_step++;
        w->cnt_end = nfrm;
    case 1:
        frm = w->cnt;
        end = w->cnt_end;
        enemy_task_get_pos(obj, &pos);
        rate = frm / end;
        enemy_task_get_param_vec(obj, &vel);
        v3_sub(&pos, &pos, &w->move);
        v3_mul(&w->move, &vel, safe_powf(ease_sine_in_out(rate), EN_MARIMO_REC->pow_escape));
        v3_add(&pos, &w->move);
        enemy_task_set_pos(obj, &pos);
        left = end - frm;
        if (left < 120.0f) {
            if (!(w->flags & 0x100)) {
                for (i = 2; i >= 0; i--) {
                    LockObj *lock = &w->lock[i];

                    lock->flags &= ~LOCK_LOCKABLE;
                }
                w->flags |= 0x100;
            }
            w->fade = left / 120.0f - 1.0f;
        }
        w->cnt += 1.0f;
        if (w->cnt_end < w->cnt)
            enemy_task_set_state(obj, 2);
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b0b220 */
void en_marimo_exec_normal(void *obj, EnMarimoWork *w)
{
    Vec3 *pos;

    if (w->phase < 5 && (w->flags & 0x30) == 0x30) {
        w->next_step = 0;
        w->next = 5;
    }
    do {
        w->phase = w->next;
        w->step = w->next_step;
    } while (0);
    if (enemy_task_get_phase(obj) == 1) {
        v3_scale(&w->knock, 0.97f);
        pos = enemy_task_pos_ptr(obj);
        v3_add(pos, &w->knock);
        en_marimo_swing(obj, w);
        if (en_marimo_states[w->phase])
            en_marimo_states[w->phase](obj, w);
        en_marimo_palette_update(obj, w);
    }
}

/* 0x00b0b368 */
void en_marimo_exec_destroyed(void *obj, EnMarimoWork *w)
{
    SpawnDesc desc;
    Task *child;
    int sub = enemy_task_get_phase(obj);

    if (sub != 1) {
        if (!sub) {
            nj_fill8(&desc, 0, sizeof desc);
            desc.kind = 0x40000;
            desc.models = stage_models;
            desc.type = 74;
            desc.motion = 75;
            desc.time = ((ModelPart *)enemy_task_gfx_ptr(obj))->frame;
            desc.v.x = 0;
            desc.v.y = 0;
            desc.v.z = 0;
            enemy_task_get_pos(obj, &desc.pos);
            enemy_task_get_rot(obj, desc.ang);
            v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
            enemy_shatter_create(obj, &desc, 0, 0);
        }
    } else {
        child = (Task *)TASK_OF(obj)->child;
        if (!(child ? child + 1 : 0))
            enemy_task_next_phase(obj);
    }
}

/* 0x00b0b458 */
void en_marimo_exec(void *obj, void *w)
{
    ObjStateFn *tab = en_marimo_exec_table;
    unsigned int state = enemy_task_get_pstate(obj);

    if (tab[state])
        tab[state](obj, w);
}

/* 0x00b0b4b8 */
void en_marimo_pre_exec(void *obj, EnMarimoWork *w)
{
    w->flags &= ~1;
    en_marimo_hit_check(obj, w);
}

/* 0x00b0b4d0 */
void en_marimo_post_exec(void *obj, EnMarimoWork *w)
{
    en_marimo_calc_points(obj, w);
}

/* 0x00b0b4d8 */
void en_marimo_toggle_model_nodes(void *obj, EnMarimoWork *w)
{
    if (w->flags & 0x20000)
        util_node_toggle_unmasked(en_marimo_model_nodes, 0x4000000);
}

/* 0x00b0b500 */
void en_marimo_toggle_body_parts(void *obj, EnMarimoWork *w)
{
    int i;

    for (i = 6; i >= 0; i--) {
        if (w->flags & (1024 << i))
            util_node_toggle_unmasked(en_marimo_body_nodes[i], 0x4000000);
    }
}

/* 0x00b0b580 */
void en_marimo_toggle_part_a(void *obj, EnMarimoWork *w)
{
    int i;

    for (i = 3; i >= 0; i--) {
        if (w->flags & (0x100000 << i))
            util_node_toggle_unmasked(en_marimo_part_a_nodes[i], 0x4000000);
    }
}

/* steer(cast) */
/* 0x00b0b600 */
void en_marimo_disp_body(void *obj, EnMarimoWork *w)
{
    ModelPart *body = &w->body;
    Vec3 *pos = enemy_task_pos_ptr(obj);
    float rgba[4];
    int blend;
    int fade;
    float opaque;

    if (!view_sphere_visible(pos, body->radius))
        return;
    memset(rgba, 0, sizeof rgba);
    blend = 0;
    { void *t_ = stage_textures; fade = w->flags & 0x100; ulGxSetTextureList(t_); }
    ulMatrixPush();
    if (fade) {
        blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        draw_push_strip_flags();
        draw_push_override_color();
        rgba[0] = w->fade;
#ifdef VERSION_US
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 3072);
#else
        draw_set_strip_flags(0xffffffff, 2048);
#endif
        draw_set_override_color(rgba);
    }
#ifdef VERSION_US
    else {                              /* not fading: blend also set before the body */
        blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE);
        draw_push_strip_flags();
        draw_push_override_color();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 1024);
    }
#endif
    do {
        Vec3 *p_ = pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_marimo_rotate_yxz(enemy_task_rot_ptr(obj));
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    en_marimo_toggle_model_nodes(obj, w);
    dc_object_draw(enemy_task_gfx_ptr(obj));
    en_marimo_toggle_model_nodes(obj, w);
    en_marimo_toggle_body_parts(obj, w);
    dc_object_draw(body);
    en_marimo_toggle_body_parts(obj, w);
    if (fade) {
        draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_SET);
        motion_sample_color((void *)w->pal_b,
                             (float)(unsigned int)(((int *)w->pal_b)[1] - 1) * beat_clock_get_phase(), 0.0f, 0.0f, 0.0f,
                             0.0f, rgba);
        rgba[0] *= w->fade + 1.0f;
        draw_set_override_color(rgba);
        dc_object_draw(&w->part_b);
        motion_sample_color((void *)w->pal_a,
                             (float)(unsigned int)(((int *)w->pal_a)[1] - 1) * beat_clock_get_phase(), 0.0f, 0.0f, 0.0f,
                             0.0f, rgba);
        opaque = 1.0f;
        rgba[0] *= w->fade + opaque;
        draw_set_override_color(rgba);
        en_marimo_toggle_part_a(obj, w);
        dc_object_draw(&w->part_a);
        en_marimo_toggle_part_a(obj, w);
        if (w->flags & 0x200) {
            motion_sample_color((void *)w->pal_wire, w->pal_pos, 0.0f, 0.0f, 0.0f, 0.0f, rgba);
            rgba[0] *= w->fade + opaque;
            draw_set_override_color(rgba);
            if (!(w->flags & 0x10))
                dc_object_draw(&w->cont[0]);
            if (!(w->flags & 0x20))
                dc_object_draw(&w->cont[1]);
        }
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(blend);
    } else {
#ifdef VERSION_US
        draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_SET);
#else
        blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_SET);
        draw_push_strip_flags();
        draw_push_override_color();
#endif
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 3072);
        motion_sample_color((void *)w->pal_b,
                             (float)(unsigned int)(((int *)w->pal_b)[1] - 1) * beat_clock_get_phase(), 0.0f, 0.0f, 0.0f,
                             0.0f, rgba);
        draw_set_override_color(rgba);
        dc_object_draw(&w->part_b);
        motion_sample_color((void *)w->pal_a,
                             (float)(unsigned int)(((int *)w->pal_a)[1] - 1) * beat_clock_get_phase(), 0.0f, 0.0f, 0.0f,
                             0.0f, rgba);
        draw_set_override_color(rgba);
        en_marimo_toggle_part_a(obj, w);
        dc_object_draw(&w->part_a);
        en_marimo_toggle_part_a(obj, w);
        if (w->flags & 0x200) {
            motion_sample_color((void *)w->pal_wire, w->pal_pos, 0.0f, 0.0f, 0.0f, 0.0f, rgba);
            draw_set_override_color(rgba);
            if (!(w->flags & 0x10))
                dc_object_draw(&w->cont[0]);
            if (!(w->flags & 0x20))
                dc_object_draw(&w->cont[1]);
        }
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(blend);
    }
    ulMatrixPop();
}

/* 0x00b0baf8 */
void en_marimo_disp(void *obj, EnMarimoWork *w)
{
    void (**tbl)(void *obj, EnMarimoWork *w) = en_marimo_disp_table;
    unsigned int state;
    Vec3 pos;
    Vec3 vel;
    int ang[3];
    ModelPart *part;

    state = enemy_task_get_pstate(obj);
    if (tbl[state])
        tbl[state](obj, w);
    if (debug_flags_test(16) && EN_MARIMO_REC->test) {
        debug_set_origin(10, 10);
        enemy_task_get_pos(obj, &pos);
        debug_printf(str_en_marimo_pos, (int)pos.x, (int)pos.y, (int)pos.z);
        debug_newline();
        enemy_task_get_param_vec(obj, &vel);
        debug_printf(str_en_marimo_vec, vel.x, vel.y, vel.z);
        debug_newline();
        enemy_task_get_rot(obj, ang);
        debug_printf(str_en_marimo_ang, (int)(ang[0] * DEG_PER_ANG), (int)(ang[1] * DEG_PER_ANG),
                     (int)(ang[2] % 65536 * DEG_PER_ANG));
        debug_newline();
        part = enemy_task_gfx_ptr(obj);
        debug_printf(str_en_marimo_frm, (int)part->frame, (int)part->frames);
        debug_newline();
        part = &w->body;
        debug_printf(str_en_marimo_frm, (int)part->frame, (int)part->frames);
        debug_newline();
        debug_printf(str_en_marimo_cnt, (int)w->cnt, (int)w->cnt_end);
        debug_newline();
        debug_printf(str_en_marimo_mod, w->phase);
        debug_newline();
    }
}

/* steer(cast) */
/* 0x00b0bd40 */
void en_marimo_init(void *obj, EnMarimoWork *w)
{
    int i;

    w->pos = enemy_task_pos_ptr(obj);
    w->vel = enemy_task_param_vec_ptr(obj);
    w->ang = enemy_task_rot_ptr(obj);
    w->scale = enemy_task_scl_ptr(obj);
    enemy_task_clear_flags(obj, 64);
    dc_object_init(&w->part_a, *((struct DcNode **)pnb_get_entry(stage_models, 88)), *((struct DcClip **)pnb_get_entry(stage_models, 89)), 0);
    dc_object_init(&w->part_b, *((struct DcNode **)pnb_get_entry(stage_models, 76)), *((struct DcClip **)pnb_get_entry(stage_models, 77)), 0);
    dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(stage_models, 85)), *((struct DcClip **)pnb_get_entry(stage_models, 86)), 0);
    dc_object_init(&w->cont[0], *((struct DcNode **)pnb_get_entry(stage_models, 92)), *((struct DcClip **)pnb_get_entry(stage_models, 93)), 0);
    dc_object_init(&w->cont[1], *((struct DcNode **)pnb_get_entry(stage_models, 94)), *((struct DcClip **)pnb_get_entry(stage_models, 95)), 0);
    dc_object_rebuild(&w->part_a);
    dc_object_rebuild(&w->part_b);
    dc_object_rebuild(&w->body);
    dc_object_rebuild(&w->cont[0]);
    dc_object_rebuild(&w->cont[1]);
    w->pal_a = *((int *)pnb_get_entry(stage_models, 90));
    w->pal_b = *((int *)pnb_get_entry(stage_models, 78));
    w->pal_wire = *((int *)pnb_get_entry(stage_models, 91));
    for (i = 0; i >= 0; i--) {
        if (EN_MARIMO_REC->test)
            lockon_obj_init(&w->lock[0], obj, &w->bpt[i], 0, en_marimo_base_lock_name, enemy_task_get_status_slot(obj), EN_MARIMO_REC->hp_vital, 1,
                          0);
        else
            lockon_obj_init(&w->lock[0], obj, &w->bpt[i], 0, en_marimo_base_lock_name, enemy_task_get_status_slot(obj), EN_MARIMO_REC->hp_vital,
                          EN_MARIMO_REC->hp_vital, 0);
    }
    for (i = 1; i >= 0; i--) {
        if (EN_MARIMO_REC->test)
            lockon_obj_init(&w->lock[i + 1], obj, &w->cpt[i], 0, en_marimo_container_lock_name, enemy_task_get_status_slot(obj), EN_MARIMO_REC->hp_core,
                          1, 128);
        else
            lockon_obj_init(&w->lock[i + 1], obj, &w->cpt[i], 0, en_marimo_container_lock_name, enemy_task_get_status_slot(obj), EN_MARIMO_REC->hp_core,
                          EN_MARIMO_REC->hp_core, 128);
    }
    for (i = 3; --i >= 0;)
        lockon_entry(&w->lock[i]);
    enemy_task_set_appear(obj, 0, 0);
    enemy_task_set_vanish(obj, 0, 0);
}

/* 0x00b0c0d8 */
void en_marimo_release(void *obj, EnMarimoWork *w)
{
    int i;

    dc_object_release(&w->part_a);
    dc_object_release(&w->part_b);
    dc_object_release(&w->body);
    dc_object_release(&w->cont[0]);
    dc_object_release(&w->cont[1]);
    for (i = 3; --i >= 0;)
        lockon_remove(&w->lock[i]);
}

/* 0x00b0c150 */
int en_marimo_create(void *parent, void *place)
{
    ObjDesc desc;
    ObjFuncs funcs;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&funcs, 0, sizeof funcs);
    funcs.fn[0] = en_marimo_init;
    funcs.fn[1] = en_marimo_release;
    funcs.fn[2] = en_marimo_exec;
    funcs.fn[3] = en_marimo_disp;
    funcs.fn[4] = en_marimo_pre_exec;
    funcs.fn[5] = en_marimo_post_exec;
    desc.name = en_marimo_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = 74;
    desc.model_no2 = 75;
    desc.time = 0;
    desc.wob_amp = 0;
    desc.wob_spd = 0;
    desc.size = 856;
    return enemy_task_task_create(parent, &desc);
}

/* steer(cast) */
/* 0x00b0c258 */
void en_marimo_nodes_init(void)
{
    int i;
    int slot;
    int mdl;
    int base = *((int *)pnb_get_entry(stage_models, 74));
    int body = *((int *)pnb_get_entry(stage_models, 85));

    node_tree_collect_to((void *)*((int *)pnb_get_entry(stage_models, 88)), 0x4000000, (struct NodeTreeNode * *)en_marimo_part_a_nodes);
    node_tree_collect_to((void *)base, 0x4000000, (struct NodeTreeNode * *)&en_marimo_model_nodes);
    node_tree_collect_to((void *)body, 0x4000000, (struct NodeTreeNode * *)en_marimo_body_nodes);
    node_tree_collect_counted((void *)body, 0x4000000, 7, en_marimo_part_nodes);
    for (i = 6; i >= 0; i--) {
        slot = en_marimo_part_slots[i];
        mdl = en_marimo_part_models[slot];
        en_marimo_part_model[i] = *((int *)pnb_get_entry(stage_models, mdl));
        en_marimo_part_motion[i] = model_pnb_get_wire(stage_models, mdl);
    }
}

/* 0x00b0c3a8 */
void en_marimo_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
