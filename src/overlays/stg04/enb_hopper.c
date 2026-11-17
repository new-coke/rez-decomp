/* Hopper shot, Area 4: homing, slows near the player; can be shot. */

#include "stg04.h"
#include "mathdef.h"
#include "color.h"
#include "debug.h"
#include "enemy_status.h"
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
#include "render.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "enb_hopper.h"
#include "../common/air_mng.h"
#include "../common/en_backfire.h"
#include "../common/en_perfectwall.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"
#include "../common/util.h"

extern ParamRec enb_hopper_params;
extern VarDef enb_hopper_vardefs[13];
extern int enb_hopper_init_arg;
extern struct TaskDesc enb_hopper_desc;

/* enb_hopper_params, 13 words */
typedef struct EnbHopperRec {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    int score;                  /* 0x0c */
    float clip_ang;             /* 0x10 */
    float clip_z;               /* 0x14 */
    float dst_far;              /* 0x18 */
    float dst_near;             /* 0x1c */
    float spd_far;              /* 0x20 */
    float spd_near;             /* 0x24 */
    float collision_r;          /* 0x28 */
    float frm_motion;           /* 0x2c */
    unsigned int col_wire;      /* 0x30 */
    int test;                   /* 0x34 */
    float test_float;           /* 0x38 */
} EnbHopperRec;

#define ENB_HOPPER_REC ((EnbHopperRec *)&enb_hopper_params)
extern Vec3 enb_hopper_points[2];
extern char enb_hopper_lock_name[];
extern char str_enb_hopper_pos[];
extern char str_enb_hopper_frm[];
extern char str_enb_hopper_col[];
extern void (*enb_hopper_states[])(EnbHopperWork *w);

/* 0x00b13250 */
void enb_hopper_vars_register(void)
{
    VarDef hdr;
    VarDef *def;
    int *prm;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&hdr, 0, sizeof hdr);
    hdr.name = enb_hopper_params.name;
    hdr.fmt = enb_hopper_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&hdr);
    def = enb_hopper_vardefs;
    prm = enb_hopper_params.v;
    for (i = 0; i < 13; i++) {
        def->ptr = prm;
        prm++;
        def++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)enb_hopper_vardefs);
}

/* 0x00b13300 */
void enb_hopper_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b13308 */
int enb_hopper_vars_init(int *fx_id)
{
    enb_hopper_vars_register();
    enb_hopper_init_arg = *fx_id;
    return 0;
}

/* 0x00b13340 */
int enb_hopper_vars_release(void)
{
    enb_hopper_vars_unregister();
    return 0;
}

/* 0x00b13360 */
void enb_hopper_calc_points(EnbHopperWork *w)
{
    Vec3 pts[2];

    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &w->fx_pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    enb_hopper_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    matrix_apply_points(enb_hopper_points, pts, 2);
    ulMatrixPop();
    v3_copy(&w->lpos, &pts[0]);
    v3_copy(&w->tpos, &pts[1]);
}

/* 0x00b13418 */
void enb_hopper_hit_check(EnbHopperWork *w)
{
    LockObj *lk = &w->lock;

    if (lk->flags & LOCK_HIT && lk->life <= 0) {
        lk->flags &= ~LOCK_LOCKABLE;
        enemy_status_score_popup(w->fx, &w->fx_pos, ENB_HOPPER_REC->score, lockon_hits_max_shot_cnt(lk));
        w->fx_op = 3;
        do {
            w->next = 1;
            w->next_step = 0;
        } while (0);
    }
}

/* 0x00b134a0 */
void enb_hopper_crash_check(EnbHopperWork *w)
{
    int hit = player_sphere_test(&w->fx_pos, ENB_HOPPER_REC->collision_r);

    if (hit == 1) {
        player_status_add_damage(1.0f);
        w->fx_op = 5;
        do {
            w->next = hit;
            w->next_step = 0;
        } while (0);
    }
}

/* 0x00b13508 */
void enb_hopper_wire_color(EnbHopperWork *w)
{
    Vec3 pl;
    float dist_sq, a;

    player_get_pos(&pl);
    dist_sq = vec3_distance_squared(&pl, &w->fx_pos);
    if (dist_sq < 0.0f)
        a = 1.0f;
    else if (40000.0f < dist_sq)
        a = 0.2f;
    else {
        a = (sqrtf(dist_sq) - 0.0f) / 200.0f;
        a = a * -0.8f + 1.0f;
    }
    w->col = (ENB_HOPPER_REC->col_wire & 0xffffff) | (((int)(color_get_a(ENB_HOPPER_REC->col_wire) * a) + 1) >> 1 << 24);
}

/* 0x00b13648 */
void enb_hopper_state_approach(EnbHopperWork *w)
{
    Vec3 pl;
    Vec3 vel;
    float len, spd;

    switch (w->step) {
    case 0:
        w->step++;
        w->next_step++;
    case 1:
        player_get_pos(&pl);
        v3_sub(&vel, &pl, &w->fx_pos);
        len = vec3_length(&vel);
        if (ENB_HOPPER_REC->dst_far < len)
            spd = ENB_HOPPER_REC->spd_far * FRAME_SEC;
        else if (len < ENB_HOPPER_REC->dst_near)
            spd = ENB_HOPPER_REC->spd_near * FRAME_SEC;
        else
            spd = ENB_HOPPER_REC->spd_near * FRAME_SEC +
                (ENB_HOPPER_REC->spd_far * FRAME_SEC - ENB_HOPPER_REC->spd_near * FRAME_SEC) *
                    ease_sine_in_out((len - ENB_HOPPER_REC->dst_near) / (ENB_HOPPER_REC->dst_far - ENB_HOPPER_REC->dst_near));
        vec3_scale_to_length(spd, &vel, &vel);
        v3_add(&w->fx_pos, &vel);
        do {
            w->ang[1] = (int)RAD_TO_ANG(atan2f(-vel.x, -vel.z));
            w->ang[0] = (int)RAD_TO_ANG(atan2f(vel.y, sqrtf(vel.x * vel.x + vel.z * vel.z)));
        } while (0);
        if (w->state != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b13898 */
void enb_hopper_state_break(EnbHopperWork *w)
{
    SpawnDesc desc;
    ShatterParams prm;

    switch (w->step) {
    case 0:
        nj_fill8(&desc, 0, sizeof desc);
        nj_fill8(&prm, 0, sizeof prm);
        desc.v.x = 0;
        desc.v.y = 0;
        desc.v.z = 0;
        v3_copy(&desc.pos, &w->fx_pos);
        desc.type = 133;
        desc.kind = 0;
        desc.models = stage_models;
        i3_copy(desc.ang, w->ang);
        v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
        prm.Gravity = 10.0f;
        prm.PowVec = 0.9f;
        prm.PowAng = 6.7f;
        prm.RtoVecDec = 0.94f;
        prm.RtoAngDec = 0.98f;
        prm.RotSpdPlayer = 1.0f;
        prm.TimeAlphaStart = 1.0f;
        prm.TimeAlphaEnd = 80.0f;
        prm.pcol_wire = RGBA(0x68, 0x1e, 0, 0x80);
        prm.force_wire = 0;
        prm.b_acc = 0;
        prm.frm_acc = 0;
        enemy_shatter_create(w, &desc, (float *)&prm, 0);
        w->step++;
        w->next_step++;
    case 1:
        FINISH_IF(w);
        if (w->state != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b13a50 */
void enb_hopper_exec(EnbHopperWork *w)
{
    int n = enemy_status_get_ticks();
    Vec3 ofs;
    void (*fn)(EnbHopperWork *);

    player_get_frame_vel(&ofs);
    v3_add(&w->fx_pos, &ofs);
    enb_hopper_hit_check(w);
    do {
        w->state = w->next;
        w->step = w->next_step;
    } while (0);
    while (n--) {
        do {
            w->state = w->next;
            w->step = w->next_step;
        } while (0);
        if (util_ahead_of_player(&w->fx_pos, ENB_HOPPER_REC->clip_z)) {
            FINISH_IF(w);
            return;
        }
        enb_hopper_crash_check(w);
        fn = enb_hopper_states[w->state];
        if (fn)
            fn(w);
        dc_object_advance_time(&w->body);
    }
    enb_hopper_calc_points(w);
    enb_hopper_wire_color(w);
}

/* steer(cast) */
/* 0x00b13b98 */
void enb_hopper_disp(EnbHopperWork *w)
{
    ModelPart *body = &w->body;
    Vec3 *p = &w->fx_pos;
    float rgba[4];
    int blend, dl;
    unsigned int col;
    Vec3 pos;

    v3_set(&w->scale, ENB_HOPPER_REC->test_float, ENB_HOPPER_REC->test_float, ENB_HOPPER_REC->test_float);
    if (view_sphere_visible(p, body->radius)) {
        ulGxSetTextureList(stage_textures);
        ulMatrixPush();
        do {
            Vec3 *p_ = p;

            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        enb_hopper_rotate_yxz(w->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        dc_object_draw(body);
        draw_set_blend(4, 5);
        draw_set_color(w->col);
        model_object_draw(&w->outline, body->frame);
        motion_sample_color((void *)w->wire, (float)(unsigned int)(((int *)w->wire)[1] - 1) * beat_clock_get_phase(),
                             100.0f, 400.0f, 1.0f,
                             0.2f, rgba);
        blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_SET);
        draw_push_strip_flags();
        draw_push_override_color();
        dl = RenderSetCurrentDisplayList(6);
        ulGxSetTextureList(stage_textures);
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 3072);
        draw_set_override_color(rgba);
        w->glow.frame = body->frame;
        dc_object_draw(&w->glow);
        RenderSetCurrentDisplayList(dl);
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(blend);
        ulMatrixPop();
    }
    if (debug_flags_test(16) && ENB_HOPPER_REC->test) {
        debug_set_origin(10, 20);
        v3_copy(&pos, &w->fx_pos);
        debug_printf(str_enb_hopper_pos, (int)pos.x, (int)pos.y, (int)pos.z);
        debug_newline();
        debug_printf(str_enb_hopper_frm, (int)body->frame, (int)body->frames);
        debug_newline();
        col = w->col;
        debug_set_color(col);
        debug_printf(str_enb_hopper_col, color_get_a(col), col & 0xff, col >> 8 & 0xff, col >> 16 & 0xff);
        debug_set_color(RGBA(0x60, 0x60, 0x60, 0x80));
        debug_newline();
    }
}

/* steer(cast) */
/* 0x00b13e50 */
void enb_hopper_init(EnbHopperWork *w, Vec3 *pos, void *place)
{
    int *ang = place;
    Vec3 ofs;
    BackFireLink lnk;
    TrailArg trail;
    int *rot = w->ang;
    LockObj *lk = &w->lock;
    int old_tex;

    player_get_frame_vel(&ofs);
    v3_sub(&w->fx_pos, pos, &ofs);
    i3_copy(rot, ang);
    util_rotate_xyz(ang, 0.0f, 0.0f, -1.0f, &w->dir);
    dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(stage_models, 131)), *((struct DcClip **)pnb_get_entry(stage_models, 132)), 4096);
    dc_object_init(&w->outline, *((struct DcNode **)pnb_get_entry(stage_models, 129)), *((struct DcClip **)pnb_get_entry(stage_models, 130)), 4096);
    dc_object_init(&w->glow, *((struct DcNode **)pnb_get_entry(stage_models, 134)), *((struct DcClip **)pnb_get_entry(stage_models, 135)), 4096);
    dc_object_rebuild(&w->body);
    dc_object_rebuild(&w->glow);
    dc_object_set_wire(&w->outline, model_pnb_get_wire(stage_models, 129));
    PART_MOTION(&w->body)->speed = w->body.frames / (ENB_HOPPER_REC->frm_motion * FPS);
    w->wire = *((int *)pnb_get_entry(stage_models, 136));
    w->fx = enemy_status_ent_alloc(enb_hopper_init_arg, -1, w);
    w->fx_op = 3;
    enemy_status_event(w->fx, 2, &w->fx_pos);
    lockon_obj_init(lk, w, &w->fx_pos, 0, enb_hopper_lock_name, w->fx, ENB_HOPPER_REC->hp, ENB_HOPPER_REC->hp, 0);
    lockon_entry(lk);
    nj_fill8(&lnk, 0, 52);
    lnk.gfx = 0;
    lnk.obj_scale = &w->scale;
    lnk.pos = &w->lpos;
    lnk.ang = rot;
    old_tex = en_backfire_set_texture(7);
    en_backfire_create(w, &lnk, 1);
    en_backfire_set_texture(old_tex);
    v3_set(&w->scale, ENB_HOPPER_REC->test_float, ENB_HOPPER_REC->test_float, ENB_HOPPER_REC->test_float);
    nj_fill8(&trail, 0, 40);
    trail.body = 0;
    trail.pos = &w->tpos;
    trail.tail_seg = 1;
    trail.ang = rot;
    trail.frames = 60;
    trail.tail_tex = 3;
    trail.tail_mask = 0x1000000;
    trail.one = 1;
    trail.tail_width = 1.0f;
    trail.tail_skip = 10;
    air_mng_create(w, &trail);
}

/* 0x00b14180 */
void enb_hopper_release(EnbHopperWork *w)
{
    enemy_status_event(w->fx, w->fx_op, &w->fx_pos);
    lockon_remove(&w->lock);
    dc_object_release(&w->body);
    dc_object_release(&w->glow);
    dc_object_release(&w->outline);
}

/* 0x00b141d0 */
EnbHopperWork *enb_hopper_create(void *parent, Vec3 *pos, void *place)
{
    EnbHopperWork *w = task_create_sibling_inherit(parent, &enb_hopper_desc, 460);

    enb_hopper_init(w, pos, place);
    return w;
}

/* 0x00b14230 */
void enb_hopper_models_init(void)
{
    model_pnb_get_wire(stage_models, 129);
}

/* 0x00b14240 */
void enb_hopper_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
