/*
 * Chaser, Area 4 enemy: closes in in eased moves, falls back, turns and fires (muzzle flashes),
 * then rushes away. Hits knock it back and break pieces off.
 */

#include "stg04.h"
#include "mathdef.h"
#include "anim.h"
#include "billboard.h"
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
#include "player.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_chaser.h"
#include "../common/en_break.h"
#include "../common/en_kawasemi_flash.h"
#include "../common/en_perfectwall.h"
#include "../common/enb_hunter.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"
#include "../common/util.h"

extern ObjStateFn en_chaser_exec_table[];
extern ParamRec en_chaser_params;
extern VarDef en_chaser_vardefs[21];

/* en_chaser_params, 21 words */
typedef struct EnChaserRec {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    int score;                  /* 0x0c */
    float spd_swing;            /* 0x10: the wobble's largest time step */
    float rx_swing;             /* 0x14: its amplitudes */
    float ry_swing;             /* 0x18 */
    float rz_swing;             /* 0x1c */
    float frm_attack_motion;    /* 0x20: the attack animation (seconds) */
    float spd_move_near;        /* 0x24: approach: time per 1000 units */
    float pow_move_near;        /* 0x28: safe_powf divisors of the eased move and turn */
    float pow_rotate_near;      /* 0x2c */
    float spd_move_far;         /* 0x30: retreat: time per 1000 units */
    float pow_move_far;         /* 0x34 */
    float pow_rotate_far;       /* 0x38 */
    float spd_escape_max;       /* 0x3c: rush speed (per second) */
    float acc_escape;           /* 0x40: its acceleration (per second squared) */
    float dst_away;             /* 0x44: util_player_outside's radius */
    float sca_shot;             /* 0x48: knock-back after shooting */
    float sca_laser;            /* 0x4c: knock-back when hit */
    float frm_stop_swing;       /* 0x50: seconds without wobble after a knock */
    unsigned int col_wire;      /* 0x54 */
    int test;                   /* 0x58: prints the object's state */
} EnChaserRec;

#define EN_CHASER_REC ((EnChaserRec *)&en_chaser_params)

/* yaw into aoff[1], pitch into aoff[0]; 65536 a turn */
#define EN_CHASER_AIM_DIR(w, d) do {         (w)->aoff[1] = (int)RAD_TO_ANG(atan2f(-(d).x, -(d).z));         (w)->aoff[0] = (int)RAD_TO_ANG(atan2f((d).y, sqrtf((d).x * (d).x + (d).z * (d).z)));     } while (0)

/* next state: phase n from its first step */
#define EN_CHASER_SET_NEXT(w, n) do { (w)->next_step = 0; (w)->next = (n); } while (0)

/* 204 bytes beyond the object */
typedef enum { CHASER_STEP_START } ChaserStep;

typedef enum { CHASER_NSTEP_NONE = -1 } ChaserNextStep;

typedef struct EnChaserWork {
    Vec3 *pos;
    Vec3 *vel;                  /* 0x04 */
    int *ang;                   /* 0x08 */
    Vec3 *scale;                /* 0x0c */
    int phase;                  /* 0x10: index into en_chaser_states */
    int next;                   /* 0x14: the phase for the next frame */
    ChaserStep step;            /* 0x18: 0 starts the phase */
    ChaserNextStep next_step;   /* 0x1c */
    Vec3 dir;                   /* 0x20: the move of the phase */
    Vec3 off;                   /* 0x2c: wobble offset */
    float t;                    /* 0x38: wobble time */
    float dt;                   /* 0x3c: wobble step */
    Vec3 knock;                 /* 0x40: knock-back velocity */
    Vec3 moff;                  /* 0x4c: the eased part of the move */
    int aoff[3];                /* 0x58: the eased part of the turn */
    void *model;                /* 0x64: the wire model */
    float speed;                /* 0x68: rush speed */
    char pad_6c[4];
    float alpha;                /* 0x70 */
    LockObj lock;                  /* 0x74 */
    int flags;                  /* 0xb4: 1, 2 attack done; 4 fading; 0x400 << i: piece i broken */
    float cnt;                  /* 0xb8: frames */
    float cnt_end;              /* 0xbc */
    int shots;                  /* 0xc0 */
    float wob_wait;             /* 0xc4 */
    float fade;                 /* 0xc8: fade-in frames */
} EnChaserWork;

typedef struct EnChaserCbArg {
    EnChaserWork *work;
    int i;
} EnChaserCbArg;

extern void (*en_chaser_states[])(void *o, EnChaserWork *w);
extern void (*en_chaser_disp_table[])(void *o, EnChaserWork *w);
extern int en_chaser_part_bits[3];
extern int en_chaser_part_break_types[3];
extern int en_chaser_part_models[3];
extern char en_chaser_name[];
extern char en_chaser_lock_name[];
extern char str_en_chaser_pos[];
extern char str_en_chaser_vec[];
extern char str_en_chaser_ang[];
extern char str_en_chaser_frm[];
extern char str_en_chaser_cnt[];
extern char str_en_chaser_mod[];
extern unsigned int *en_chaser_body_nodes[3];
extern unsigned int *en_chaser_wire_nodes[3];
extern int en_chaser_part_model[3];
extern int en_chaser_wire_model;
extern int en_chaser_col_motion;
extern int en_chaser_part_motion[3];
extern int en_chaser_wire_cache;
extern float en_chaser_fade_color[4];

/* 0x00b00a20 */
void en_chaser_vars_register(void)
{
    VarDef hdr;
    VarDef *def;
    int *val;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&hdr, 0, sizeof hdr);
    hdr.name = en_chaser_params.name;
    hdr.fmt = en_chaser_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&hdr);
    def = en_chaser_vardefs;
    val = en_chaser_params.v;
    for (i = 0; i < 21; i++) {
        def->ptr = val;
        val++;
        def++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_chaser_vardefs);
}

/* 0x00b00ad0 */
void en_chaser_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b00ad8 */
int en_chaser_marker_draw(int obj, void *place)
{
    int *mdl;

    ulGxSetTextureList(stage_textures);
    mdl = pnb_get_entry(stage_models, 48);
    return obj_draw_at(obj, place, (void *)*mdl);
}

/* 0x00b00b30 */
int en_chaser_vars_init(void)
{
    en_chaser_vars_register();
    return 0;
}

/* 0x00b00b50 */
int en_chaser_vars_release(void)
{
    en_chaser_vars_unregister();
    return 0;
}

/* 0x00b00b70 */
void en_chaser_calc_shot_points(void *obj, EnChaserWork *w, Vec3 *pts)
{
    Vec3 ofs;
    float xy = 0.0f;
    int i;

    enemy_task_spawn(obj, 0x1000000, 4, pts, 0, 0);
    util_rotate_xyz(enemy_task_rot_ptr(obj), xy, xy, -2.0f, &ofs);
    for (i = 4; i != 0; i--) {
        v3_add(pts, &ofs);
        pts++;
    }
}

/* 0x00b00c30 */
void en_chaser_break_part(void *obj, EnChaserWork *w, int part)
{
    SpawnDesc desc;
    float mtx[3][16];
    ModelPart *gfx;
    Vec3 *pos;
    int bit = en_chaser_part_bits[part];

    w->flags |= 1024 << bit;
    nj_fill8(&desc, 0, sizeof desc);
    desc.kind = 0x80000;
    desc.models = stage_models;
    desc.type = en_chaser_part_break_types[part];
    desc.v.x = 0;
    desc.v.y = 0;
    desc.v.z = 0;
    gfx = enemy_task_gfx_ptr(obj);
    anim_start((void *)gfx->clip, gfx->frame);
    node_tree_draw_matching_at(gfx->root, 0x4000000, bit + 1, (int)mtx, 1);
    do {
        ulMatrixPush();
    } while (0);
    matrix_unit_set(0);
    pos = enemy_task_pos_ptr(obj);
    vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
    en_chaser_rotate_yxz(enemy_task_rot_ptr(obj));
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, mtx[bit]);
    enemy_shatter_create(obj, &desc, 0, 0);
    ulMatrixPop();
}

/* 0x00b00d88 */
void en_chaser_hit_check(void *obj, void *work)
{
    EnChaserWork *w = work;
    LockObj *lock = &w->lock;
    EnChaserRec *rec;
    int hits;
    int i;
    float xy;

    if (lock->flags & LOCK_HIT) {
        rec = EN_CHASER_REC;
        hits = lock->life;
        xy = 0.0f;
        util_rotate_xyz(enemy_task_rot_ptr(obj), xy, xy, rec->sca_laser, &w->knock);
        w->wob_wait = rec->frm_stop_swing * FPS;
        if (hits <= 0) {
            enemy_status_score_popup(enemy_task_get_status_slot(obj), enemy_task_pos_ptr(obj), rec->score, lockon_hits_max_shot_cnt(lock));
            lock->flags &= ~LOCK_LOCKABLE;
            enemy_task_set_state(obj, 3);
        } else {
            if (hits == rec->hp - 1) {
                for (i = 4 - rec->hp; --i >= 0;)
                    en_chaser_break_part(obj, w, i);
            }
            if (hits < 4)
                en_chaser_break_part(obj, w, 3 - hits);
            else
                en_break_spawn(obj, enemy_task_pos_ptr(obj), enemy_task_rot_ptr(obj), 0);
        }
    }
}

/* 0x00b00f80 */
void en_chaser_swing(void *obj, EnChaserWork *w)
{
    Vec3 pos;
    float t;

    w->wob_wait -= 1.0f;
    if (w->wob_wait < 0.0f) {
        w->dt = MIN(w->dt + 2.7777778e-06f, EN_CHASER_REC->spd_swing);
        t = w->t += w->dt;
        enemy_task_get_pos(obj, &pos);
        v3_sub(&pos, &pos, &w->off);
        v3_set(&w->off, sinf(t * 3.0f) * EN_CHASER_REC->rx_swing, cosf(t * 5.0f) * EN_CHASER_REC->ry_swing,
               sinf(t * 4.0f) * EN_CHASER_REC->rz_swing);
        v3_add(&pos, &w->off);
        enemy_task_set_pos(obj, &pos);
    }
}

/* 0x00b01110 */
void en_chaser_face_player(void *obj, EnChaserWork *w)
{
    Vec3 pl;
    int ang[3];
    Vec3 v;

    player_get_pos(&pl);
    enemy_task_get_param_vec(obj, &v);
    v3_add(&v, &pl);
    v3_sub(&v, &v, enemy_task_pos_ptr(obj));
    do {
        ang[1] = (int)RAD_TO_ANG(atan2f(-v.x, -v.z));
        ang[0] = (int)RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
    } while (0);
    ang[2] = 0;
    enemy_task_set_rot(obj, ang);
}

/* 0x00b01260 */
void en_chaser_fire_flashes(void *obj, EnChaserWork *w)
{
    Vec3 pts[4];

    en_chaser_calc_shot_points(obj, w, pts);
    if (!(w->flags & 0x1000))
        en_kawasemi_flash_create(obj, &pts[1], 1);
    en_kawasemi_flash_create(obj, &pts[0], 1);
    en_kawasemi_flash_create(obj, &pts[2], 1);
    en_kawasemi_flash_create(obj, &pts[3], 1);
}

/* 0x00b012e0 */
void en_chaser_fire_shots(void *obj, EnChaserWork *w)
{
    Vec3 pts[4];
    int ang[3];

    enemy_task_spawn(obj, 0x1000000, 4, pts, 0, 0);
    enemy_task_get_rot(obj, ang);
    if (!(w->flags & 0x1000))
        enb_hunter_create(obj, (int)&pts[1], (int)ang, 1);
    enb_hunter_create(obj, (int)&pts[0], (int)ang, 1);
    enb_hunter_create(obj, (int)&pts[2], (int)ang, 1);
    enb_hunter_create(obj, (int)&pts[3], (int)ang, 1);
    util_rotate_xyz(ang, 0.0f, 0.0f, EN_CHASER_REC->sca_shot, &w->knock);
    w->wob_wait = EN_CHASER_REC->frm_stop_swing * FPS;
}

/* 0x00b013d0 */
void en_chaser_toggle_parts(void *obj, EnChaserWork *w)
{
    if (w->flags & 0x400) {
        util_node_toggle(en_chaser_body_nodes[0]);
        util_node_toggle(en_chaser_wire_nodes[0]);
    }
    if (w->flags & 0x800) {
        util_node_toggle(en_chaser_body_nodes[1]);
        util_node_toggle(en_chaser_wire_nodes[1]);
    }
    if (!(w->flags & 0x1000))
        return;
    util_node_toggle(en_chaser_body_nodes[2]);
    util_node_toggle(en_chaser_wire_nodes[2]);
}

/* 0x00b01468 */
float en_chaser_forward_dist(Vec3 *pos)
{
    Vec3 pl;
    Vec3 dir;
    Vec3 fwd;
    float dis;

    player_get_pos(&pl);
    v3_sub(&dir, pos, &pl);
    if (v3_is_zero(&dir))
        return 0.0f;
    vec3_normalize(&dir, &dir);
    v3_set(&fwd, 0.0f, 0.0f, 1.0f);
    util_dir_to_player_frame(&fwd, &fwd);
    dis = ulVec3Distance(&pl, pos);
    return dis * ulVec3Dot(&dir, &fwd);
}

/* 0x00b01588 */
float en_chaser_set_move(void *obj, EnChaserWork *w, float dis)
{
    Vec3 v;

    dis -= en_chaser_forward_dist(enemy_task_pos_ptr(obj));
    v.x = 0;
    v.y = 0;
    v.z = dis;
    util_dir_to_player_frame(&v, &w->dir);
    return fabsf(dis);
}

/* 0x00b015e0 */
void en_chaser_state_wait(void *obj, EnChaserWork *w)
{
    unsigned int step = w->step;

    switch (step) {
    case 0:
        w->flags |= 4;
        w->cnt = 1.0f;
        w->cnt_end = enemy_task_get_param(obj, 0);
        step = ++w->step;
        w->next_step++;
    case 1:
        if (w->flags & 4) {
            w->alpha = w->fade / 30.0f;
            w->fade += 1.0f;
            if (30.0f < w->fade)
                w->flags &= ~4;
        }
        w->cnt += 1.0f;
        if (w->cnt_end < w->cnt) {
            w->next_step = 0;
            w->next = 1;
        }
        if (w->phase != w->next || step != w->next_step)
            w->step = step + 1;
        break;
    case 2:
        break;
    }
}

/* steer(cast) */
/* 0x00b016f8 */
void en_chaser_state_approach(void *obj, EnChaserWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 pos;
    int ang[3];
    Vec3 aim;
    float rot, mov, rate, end;
    float frm;

    switch (w->step) {
    case 0:
        frm = en_chaser_set_move(obj, w, ((float (*)(float))float_frac)(enemy_task_get_param(obj, 2)) * 1000.0f) /
            (EN_CHASER_REC->spd_move_near * FRAME_SEC);
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 49)));
        PART_MOTION(gfx)->speed = PART_MOTION(gfx)->frames / (30.0f < frm ? 30.0f : frm);
        PART_MOTION(gfx)->frame = 0.0f;
        w->model = *(void **)((int *)pnb_get_entry(stage_models, 52));
        v3_set(&w->moff, 0.0f, 0.0f, 0.0f);
        i3_set(w->aoff, 0, 0, 0);
        w->cnt = 1.0f;
        w->cnt_end = frm;
        w->step++;
        w->next_step++;
    case 1:
        rate = w->cnt / w->cnt_end;
        end = w->cnt_end;
        dc_object_advance_time(gfx);
        enemy_task_get_pos(obj, &pos);
        enemy_task_get_rot(obj, ang);
        v3_sub(&pos, &pos, &w->moff);
        mov = safe_powf(ease_sine_in_out(rate), EN_CHASER_REC->pow_move_near);
        v3_mul(&w->moff, &w->dir, mov);
        i3_subfrom(ang, w->aoff);
        v3_add(&pos, &w->moff);
        player_get_pos(&aim);
        v3_add(&aim, enemy_task_param_vec_ptr(obj));
        v3_sub(&aim, &aim, &pos);
        EN_CHASER_AIM_DIR(w, aim);
        rot = safe_powf(ease_sine_in_out(rate), EN_CHASER_REC->pow_rotate_near);
        i3_scale(w->aoff, rot);
        i3_add(ang, w->aoff);
        enemy_task_set_pos(obj, &pos);
        enemy_task_set_rot(obj, ang);
        if (w->flags & 4) {
            w->alpha = w->fade / 30.0f;
            w->fade += 1.0f;
            if (30.0f < w->fade)
                w->flags &= ~4;
        }
        w->cnt += 1.0f;
        if (end < w->cnt) {
            if ((int)(((float (*)(float))float_frac)(enemy_task_get_param(obj, 1)) * 1000.0f + 0.5f)) {
                EN_CHASER_SET_NEXT(w, 3);
            } else {
                w->next_step = 0;
                w->next = 4;
            }
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* steer(cast) */
/* 0x00b01c18 */
void en_chaser_state_retreat(void *obj, EnChaserWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 pos;
    int ang[3];
    float rot, mov, rate, end;

    switch (w->step) {
    case 0:
        rate = en_chaser_set_move(obj, w, (float)(int)enemy_task_get_param(obj, 2)) / (EN_CHASER_REC->spd_move_far * FRAME_SEC);
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 49)));
        gfx->frame = 0;
        w->model = *(void **)((int *)pnb_get_entry(stage_models, 52));
        v3_set(&w->moff, 0.0f, 0.0f, 0.0f);
        i3_set(w->aoff, 0, 0, 0);
        w->cnt = 1.0f;
        w->cnt_end = rate;
        w->step++;
        w->next_step++;
    case 1:
        rate = w->cnt / w->cnt_end;
        end = w->cnt_end;
        enemy_task_get_pos(obj, &pos);
        enemy_task_get_rot(obj, ang);
        v3_sub(&pos, &pos, &w->moff);
        mov = safe_powf(ease_sine_in_out(rate), EN_CHASER_REC->pow_move_far);
        i3_subfrom(ang, w->aoff);
        v3_mul(&w->moff, &w->dir, mov);
        v3_add(&pos, &w->moff);
        player_get_frame_ang(w->aoff);
        i3_subfrom(w->aoff, ang);
        do {
            w->aoff[0] = (w->aoff[0] & 0x8000) ? w->aoff[0] | ~0xffff : w->aoff[0] & 0xffff;
            w->aoff[1] = (w->aoff[1] & 0x8000) ? w->aoff[1] | ~0xffff : w->aoff[1] & 0xffff;
            w->aoff[2] = (w->aoff[2] & 0x8000) ? w->aoff[2] | ~0xffff : w->aoff[2] & 0xffff;
        } while (0);
        rot = safe_powf(ease_sine_in_out(rate), EN_CHASER_REC->pow_rotate_far);
        i3_scale(w->aoff, rot);
        i3_add(ang, w->aoff);
        enemy_task_set_pos(obj, &pos);
        enemy_task_set_rot(obj, ang);
        w->cnt += 1.0f;
        if (end < w->cnt) {
            w->cnt = 1.0f;
            w->cnt_end = (int)enemy_task_get_param(obj, 1);
            do { w->next_step = w->step + 1; } while (0);
        }
        if (w->phase == w->next && w->step == w->next_step)
            break;
        w->step++;
    case 2:
        w->cnt += 1.0f;
        if (w->cnt_end < w->cnt) {
            do {
                w->next_step = 0;
                w->next = 1;
            } while (0);
        }
        break;
    }
}

/* steer(cast) */
/* 0x00b02068 */
void en_chaser_state_attack(void *obj, EnChaserWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    float sec;
    float frm;
    int shots;

    switch (w->step) {
    case 0:
        sec = FPS;
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 50)));
        PART_MOTION(gfx)->speed = gfx->frames / (EN_CHASER_REC->frm_attack_motion * sec);
        PART_MOTION(gfx)->frame = 0.0f;
        w->model = (void *)*((int *)pnb_get_entry(stage_models, 53));
        w->cnt = 1.0f;
        w->step++;
        w->next_step++;
        w->cnt_end = EN_CHASER_REC->frm_attack_motion * sec;
    case 1:
        en_chaser_face_player(obj, w);
        dc_object_advance_time(gfx);
        frm = gfx->frame;
        if (!(w->flags & 1) && 30.0f <= frm) {
            w->flags |= 1;
            en_chaser_fire_flashes(obj, w);
        }
        if (30.0f <= frm) {
            if (!(w->flags & 2)) {
                w->flags |= 2;
                en_chaser_fire_shots(obj, w);
            }
        } else
            w->dt = EN_CHASER_REC->spd_swing * (1.0f - frm / 30.0f);
        if (gfx->flags2 & 1) {
            shots = ++w->shots;
            if (shots < (int)(((float (*)(float))float_frac)(enemy_task_get_param(obj, 1)) * 1000.0f + 0.5f)) {
                do {
                    w->next_step = 0;
                    w->next = 2;
                } while (0);
            } else {
                do {
                    w->next_step = 0;
                    w->next = 4;
                } while (0);
            }
        }
        if (w->phase == w->next && w->step == w->next_step)
            break;
        w->step++;
    case 2:
        w->flags &= ~3;
        break;
    }
}

/* steer(cast) */
/* 0x00b022c8 */
void en_chaser_state_rush(void *obj, EnChaserWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 pos;
    Vec3 v;
    float val;

    switch (w->step) {
    case 0:
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 49)));
        PART_MOTION(gfx)->speed = gfx->frames / FPS;
        gfx->frame = 0;
        w->model = (void *)*((int *)pnb_get_entry(stage_models, 52));
        w->step++;
        w->next_step++;
    case 1:
        dc_object_advance_time(gfx);
        w->speed += EN_CHASER_REC->acc_escape * 0.00027777778f;
        {
            float spd_max = EN_CHASER_REC->spd_escape_max * FRAME_SEC;

            val = w->speed;
            if (val < spd_max)
                spd_max = val;
            v.z = w->speed = spd_max;
        }
        v.x = 0;
        v.y = 0;
        util_dir_to_player_frame(&v, &v);
        enemy_task_get_pos(obj, &pos);
        v3_add(&pos, &v);
        enemy_task_set_pos(obj, &pos);
        if (!(w->flags & 4)) {
            if (util_player_outside(&pos, EN_CHASER_REC->dst_away)) {
                w->alpha = 1.0f;
                w->cnt = 1.0f;
                w->cnt_end = 30.0f;
                w->flags |= 4;
            }
        } else {
            w->alpha = 1.0f - w->cnt / w->cnt_end;
            w->cnt += 1.0f;
            val = w->cnt_end;
            if (val < w->cnt)
                enemy_task_set_state(obj, 2);
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b02500 */
void en_chaser_exec_normal(void *obj, EnChaserWork *w)
{
    Vec3 pos;

    do {
        w->phase = w->next;
        w->step = w->next_step;
    } while (0);
    if (enemy_task_get_phase(obj) == 1) {
        enemy_task_get_pos(obj, &pos);
        v3_scale(&w->knock, 0.97f);
        v3_add(&pos, &w->knock);
        enemy_task_set_pos(obj, &pos);
        en_chaser_swing(obj, w);
        if (en_chaser_states[w->phase])
            en_chaser_states[w->phase](obj, w);
    }
}

/* 0x00b02600 */
void en_chaser_exec_destroyed(void *obj, EnChaserWork *w)
{
    SpawnDesc desc;
    int mode = enemy_task_get_phase(obj);

    if (mode != 1) {
        if (mode == 0) {
            nj_fill8(&desc, 0, sizeof desc);
            desc.kind = 0x40000;
            desc.models = stage_models;
            desc.type = 48;
            if (w->phase == 3)
                desc.motion = 50;
            else
                desc.motion = 49;
            desc.time = ((ModelPart *)enemy_task_gfx_ptr(obj))->frame;
            desc.v.x = 0;
            desc.v.y = 0;
            desc.v.z = 0;
            enemy_task_get_pos(obj, &desc.pos);
            enemy_task_get_rot(obj, desc.ang);
            v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
            enemy_shatter_create(obj, &desc, 0, 0);
        }
    } else
        enemy_task_next_phase(obj);
}

/* 0x00b02700 */
void en_chaser_exec(void *obj, void *w)
{
    ObjStateFn *tab = en_chaser_exec_table;
    unsigned int state = enemy_task_get_pstate(obj);

    if (tab[state])
        tab[state](obj, w);
}

/* 0x00b02760 */
void en_chaser_pre_exec(void *obj, void *w)
{
    en_chaser_hit_check(obj, w);
}

/* 0x00b02768 */
void en_chaser_post_exec(void)
{
}

/* 0x00b02770 */
int en_chaser_node_cb(unsigned int *node, int idx, EnChaserCbArg *cb)
{
    int part;

    if (node[0] & 0x4000000) {
        part = cb->i++;
        if (cb->work->flags & (1024 << part)) {
            draw_push_blend();
            draw_set_blend(4, 5);
            model_draw_kind_list(en_chaser_part_model[part], en_chaser_part_motion[part]);
            draw_pop_blend();
            return 0;
        }
    }
    return 1;
}

/* steer(cast) */
/* 0x00b02818 */
void en_chaser_disp_fade(void *obj, EnChaserWork *w)
{
    Vec3 *pos = enemy_task_pos_ptr(obj);
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    EnChaserCbArg cb;
    Vec3 v;
    float lw;
    float alpha, prev_lw;
    int blend;
    unsigned int col;

    if (!view_sphere_visible(pos, gfx->radius))
        return;
    lw = 1.0f;
    blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    ulGxSetTextureList(stage_textures);
    draw_push_strip_flags();
    draw_push_override_color();
    ulMatrixPush();
    alpha = w->alpha;
    en_chaser_fade_color[0] = alpha - lw;
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    draw_set_override_color(en_chaser_fade_color);
    do {
        Vec3 *p_ = pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_chaser_rotate_yxz(enemy_task_rot_ptr(obj));
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    en_chaser_toggle_parts(obj, w);
    cb.work = w;
    cb.i = 0;
    prev_lw = line_width_set(lw);
    draw_set_color((EN_CHASER_REC->col_wire & 0xffffff) | ((int)((float)color_get_a(EN_CHASER_REC->col_wire) * alpha) + 1) >> 1 << 24);
    dc_object_set_callback(gfx, en_chaser_node_cb, &cb);
    dc_object_draw(gfx);
    dc_object_set_callback(gfx, 0, 0);
    line_width_set(prev_lw);
    col = motion_sample_rgba((void *)en_chaser_col_motion,
                               (float)(unsigned int)(((int *)en_chaser_col_motion)[1] - 1) * beat_clock_get_phase(),
                               100.0f, 400.0f, 1.0f, 0.2f);
    draw_set_color((col & 0xffffff) | ((int)((float)color_get_a(col) * alpha) + 1) >> 1 << 24);
    model_node_walk_list_anim(en_chaser_wire_model, (int)w->model, gfx->frame, en_chaser_wire_cache);
    en_chaser_toggle_parts(obj, w);
    v.x = 0;
    v.y = 0;
    v.z = 0;
    draw_set_blend(4, 1);
    billboard_draw(1, ((int)(alpha * 255.0f) + 1) >> 1 << 24 | 0x808080, &v, 1.7f, 1.7f);
    ulMatrixPop();
    draw_pop_override_color();
    draw_pop_strip_flags();
    draw_set_override_flags(blend);
}

/* 0x00b02b40 */
void en_chaser_disp_body(void *obj, EnChaserWork *w)
{
    Vec3 *pos;
    ModelPart *gfx;
    EnChaserCbArg cb;
    Vec3 v;
    float prev_lw;

    if (w->flags & 4) {
        en_chaser_disp_fade(obj, w);
        return;
    }
    pos = enemy_task_pos_ptr(obj);
    gfx = enemy_task_gfx_ptr(obj);
    if (view_sphere_visible(pos, gfx->radius)) {
        ulGxSetTextureList(stage_textures);
        ulMatrixPush();
        do {
            Vec3 *p_ = pos;

            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        en_chaser_rotate_yxz(enemy_task_rot_ptr(obj));
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        en_chaser_toggle_parts(obj, w);
        cb.work = w;
        cb.i = 0;
        prev_lw = line_width_set(1.0f);
        draw_set_color(EN_CHASER_REC->col_wire);
        dc_object_set_callback(gfx, en_chaser_node_cb, &cb);
        dc_object_draw(gfx);
        dc_object_set_callback(gfx, 0, 0);
        line_width_set(prev_lw);
        draw_set_color(motion_sample_rgba((void *)en_chaser_col_motion,
                                           (float)(unsigned int)(((int *)en_chaser_col_motion)[1] - 1) *
                                               beat_clock_get_phase(),
                                           100.0f, 400.0f, 1.0f, 0.2f));
        model_node_walk_list_anim(en_chaser_wire_model, (int)w->model, gfx->frame, en_chaser_wire_cache);
        en_chaser_toggle_parts(obj, w);
        v.x = 0;
        v.y = 0;
        v.z = 0;
        draw_set_blend(4, 1);
        billboard_draw(1, COL_WHITE, &v, 1.7f, 1.7f);
        ulMatrixPop();
    }
}

/* 0x00b02d70 */
void en_chaser_disp(void *obj, EnChaserWork *w)
{
    void (**tbl)(void *obj, EnChaserWork *w) = en_chaser_disp_table;
    unsigned int state;
    Vec3 pos;
    Vec3 pl;
    Vec3 v;
    int ang[3];
    ModelPart *gfx;

    state = enemy_task_get_pstate(obj);
    if (tbl[state])
        tbl[state](obj, w);
    if (debug_flags_test(16) && EN_CHASER_REC->test) {
        debug_set_origin(10, 10);
        enemy_task_get_pos(obj, &pos);
        player_get_pos(&pl);
        v3_sub(&pos, &pos, &pl);
        debug_printf(str_en_chaser_pos, (int)pos.x, (int)pos.y, (int)pos.z);
        debug_newline();
        v3_copy(&v, &w->dir);
        debug_printf(str_en_chaser_vec, v.x, v.y, v.z);
        debug_newline();
        enemy_task_get_rot(obj, ang);
        debug_printf(str_en_chaser_ang, (int)(ang[0] * DEG_PER_ANG), (int)(ang[1] * DEG_PER_ANG),
                     (int)(ang[2] % 65536 * DEG_PER_ANG));
        debug_newline();
        gfx = enemy_task_gfx_ptr(obj);
        debug_printf(str_en_chaser_frm, (int)gfx->frame, (int)gfx->frames);
        debug_newline();
        debug_printf(str_en_chaser_cnt, (int)w->cnt, (int)w->cnt_end);
        debug_newline();
        debug_printf(str_en_chaser_mod, w->phase);
        debug_newline();
    }
}

/* steer(cast) */
/* 0x00b02fc8 */
void en_chaser_init(void *obj, EnChaserWork *w)
{
    Vec3 v;
    float dis;

    w->pos = enemy_task_pos_ptr(obj);
    w->vel = enemy_task_param_vec_ptr(obj);
    w->ang = enemy_task_rot_ptr(obj);
    w->scale = enemy_task_scl_ptr(obj);
    enemy_task_clear_flags(obj, 64);
    w->dt = EN_CHASER_REC->spd_swing;
    dis = ((float (*)(float))float_frac)(enemy_task_get_param(obj, 2));
    v3_set(&v, 0.0f, 0.0f, dis * 1000.0f - en_chaser_forward_dist(enemy_task_pos_ptr(obj)));
    util_dir_to_player_frame(&v, &w->dir);
    lockon_obj_init(&w->lock, obj, enemy_task_pos_ptr(obj), 0, en_chaser_lock_name, enemy_task_get_status_slot(obj), EN_CHASER_REC->hp,
                  EN_CHASER_REC->hp, 0);
    lockon_entry(&w->lock);
    enemy_task_set_appear(obj, 0, 0);
    enemy_task_set_vanish(obj, 0, 0);
    w->model = (void *)*((int *)pnb_get_entry(stage_models, 52));
}

/* 0x00b03138 */
void en_chaser_release(void *obj, char *w)
{
    lockon_remove((LockObj *)(w + 116));
}

/* 0x00b03140 */
int en_chaser_create(void *parent, void *place)
{
    ObjDesc desc;
    ObjFuncs funcs;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&funcs, 0, sizeof funcs);
    funcs.fn[0] = en_chaser_init;
    funcs.fn[1] = en_chaser_release;
    funcs.fn[2] = en_chaser_exec;
    funcs.fn[3] = en_chaser_disp;
    funcs.fn[4] = en_chaser_pre_exec;
    funcs.fn[5] = en_chaser_post_exec;
    desc.name = en_chaser_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = 48;
    desc.model_no2 = 49;
    desc.hp = EN_CHASER_REC->hp;
    desc.score = EN_CHASER_REC->score;
    desc.size = 204;
    desc.time = 0.0f;
    desc.wob_amp = 0;
    desc.wob_spd = 0;
    return enemy_task_task_create(parent, &desc);
}

/* steer(cast) */
/* 0x00b03270 */
void en_chaser_nodes_init(void)
{
    int i;
    int mdl_no;
    int bit;
    int *wire_mdl = &en_chaser_wire_model;
    int *wire_cache = &en_chaser_wire_cache;
    int *col_mot = &en_chaser_col_motion;

    node_tree_collect_to((void *)*((int *)pnb_get_entry(stage_models, 48)), 0x4000000, (struct NodeTreeNode * *)en_chaser_body_nodes);
    *wire_mdl = *((int *)pnb_get_entry(stage_models, 51));
    *wire_cache = model_pnb_get_wire(stage_models, 51);
    *col_mot = *((int *)pnb_get_entry(stage_models, 54));
    node_tree_collect_to((void *)*wire_mdl, 0x4000000, (struct NodeTreeNode * *)en_chaser_wire_nodes);
    for (i = 2; i >= 0; i--) {
        mdl_no = en_chaser_part_models[i];
        bit = en_chaser_part_bits[i];
        en_chaser_part_model[bit] = *((int *)pnb_get_entry(stage_models, mdl_no));
        en_chaser_part_motion[bit] = model_pnb_get_wire(stage_models, mdl_no);
    }
}

/* 0x00b033c0 */
void en_chaser_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
