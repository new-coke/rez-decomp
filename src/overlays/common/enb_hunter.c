/*
 * The Hunter's shot, two parameter sets: a lockable bullet flying straight from its gun point,
 * speeding up, growing and rolling; hurts on contact, breaks when shot, vanishes at the end of its
 * life or out of its area. Also the Chaser's init argument.
 */

#include "types_common.h"
#include "color.h"

#include "types_overlays.h"

#include "debug.h"
#include "billboard.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "lockon.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "render.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "enb_hunter.h"
#include "enemy_shatter.h"
#include "enemy_vars.h"
#include "util.h"
#include "system.h"
#include "mathdef.h"

static inline void v3_copy(Vec3 *dst, Vec3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

static inline void v3_set(Vec3 *v, float x, float y, float z)
{
    v->x = x;
    v->y = y;
    v->z = z;
}

static inline void v3_add(Vec3 *dst, Vec3 *src)
{
    dst->x += src->x;
    dst->y += src->y;
    dst->z += src->z;
}

static inline void v3_sub(Vec3 *dst, Vec3 *lhs, Vec3 *rhs)
{
    dst->x = lhs->x - rhs->x;
    dst->y = lhs->y - rhs->y;
    dst->z = lhs->z - rhs->z;
}

static inline void v3_mul(Vec3 *dst, Vec3 *src, float scale)
{
    dst->x = src->x * scale;
    dst->y = src->y * scale;
    dst->z = src->z * scale;
}

static inline void i3_copy(int *dst, int *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

extern int ulMatrixPush(void);
extern struct GwVarNode *gw_varedit_root;

extern void *stage_textures;
extern void *stage_models;

extern EnbHunterSet enb_hunter_sets[2];
#define ENB_HUNTER_SET(i) (((EnbHunterSets *)enb_hunter_sets)->r[i])
extern VarDef enb_hunter_vardef;
extern VarDef enb_hunter_vardefs[11];
extern int enb_chaser_init_arg;

extern int enb_hunter_init_arg;

extern int (*enb_hunter_area_checks[])(void *p, float v);

extern char *enb_hunter_names[];
extern TaskDesc enb_hunter_desc;
extern float enb_hunter_max_sizes[2];
extern int enb_hunter_body_models[2];
extern int enb_hunter_glow_models[2];
extern int enb_hunter_body_motions[2];
extern int enb_hunter_glow_motions[2];
extern char *enb_hunter_lock_names[];
extern void (*enb_hunter_states[])(EnbHunterWork *w);
extern char str_enb_hunter_pos[];
extern char str_enb_hunter_frm[];

/* 0x00b14290 stg04, 0x00b1d528 stg22 */
void enb_hunter_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *prm;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&enb_hunter_vardef);
    for (i = 0; i < 2; i++) {
        def = enb_hunter_vardefs;
        prm = enb_hunter_sets[i].v;
        enb_hunter_vardef.name = enb_hunter_sets[i].name;
        enb_hunter_vardef.fmt = enb_hunter_sets[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&enb_hunter_vardef);
        for (j = 0; j < 11; j++) {
            def->ptr = prm;
            prm++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)enb_hunter_vardefs);
    }
}

/* 0x00b143a8 stg04, 0x00b1d640 stg22 */
void enb_hunter_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b143b0 stg04, 0x00b1d648 stg22 */
int enb_hunter_vars_init(int *fx_id)
{
    enb_hunter_vars_register();
    enb_hunter_init_arg = *fx_id;
    return 0;
}

/* 0x00b143e8 stg04, 0x00b1d680 stg22 */
int enb_chaser_vars_init(int *fx_id)
{
    enb_chaser_init_arg = *fx_id;
    return 0;
}

/* 0x00b14400 stg04, 0x00b1d698 stg22 */
int enb_hunter_vars_release(void)
{
    enb_hunter_vars_unregister();
    return 0;
}

/* 0x00b14420 stg04, 0x00b1d6b8 stg22 */
void enb_hunter_hit_check(EnbHunterWork *w)
{
    LockObj *lk = &w->lock;

    if (lk->flags & LOCK_HIT && lk->life <= 0) {
        EnbHunterSet *sets = enb_hunter_sets;
        int *base = &sets->v[1];
        int *score = base + w->kind * 13;

        lk->flags &= ~LOCK_LOCKABLE;
        enemy_status_score_popup(w->fx, &w->fx_pos, *score, lockon_hits_max_shot_cnt(lk));
        w->fx_op = 3;
        do {
            w->next = 1;
            w->next_step = 0;
        } while (0);
    }
}

/* 0x00b144d0 stg04, 0x00b1d768 stg22 */
void enb_hunter_crash_check(EnbHunterWork *w)
{
    if (!(w->flags & 2)) {
        if (player_sphere_test(&w->fx_pos, ENB_HUNTER_SET(w->kind).v[7]) == 1) {
            player_status_add_damage(1.0f);
            w->fx_op = 5;
            w->flags |= 2;
        }
    }
}

/* 0x00b14558 stg04, 0x00b1d7f0 stg22 */
void enb_hunter_area_check(EnbHunterWork *w)
{
    int kind = w->kind;
    int (*fn)(void *p, float v) = enb_hunter_area_checks[kind];

    fn(&w->fx_pos, ENB_HUNTER_SET(kind).v[3]);
}

/* 0x00b145a8 stg04, 0x00b1d840 stg22 */
void enb_hunter_state_fly(EnbHunterWork *w)
{
    Vec3 v;
    float spd, lim;

    switch (w->step) {
    case 0:
        w->step++;
        w->next_step++;
        w->spd = ENB_HUNTER_SET(w->kind).v[4] * FRAME_SEC;
    case 1:
        spd = w->spd + ENB_HUNTER_SET(w->kind).v[5] * 0.00027777778f;
        lim = ENB_HUNTER_SET(w->kind).v[4] * FRAME_SEC;
        spd = MIN(spd, lim);
        {
            float sz = w->size + spd;
            float sz_max = enb_hunter_max_sizes[w->kind];

            w->spd = spd;
            lim = MIN(sz, sz_max);
            w->size = lim;
        }
        v3_mul(&v, &w->dir, spd);
        v3_add(&w->fx_pos, &v);
        w->roll += (int)DEG_TO_ANG(ENB_HUNTER_SET(w->kind).v[8] * FRAME_SEC);
        if (w->state != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b14750 stg04, 0x00b1d9e8 stg22 */
void enb_hunter_state_break(EnbHunterWork *w)
{
    SpawnDesc desc;
    ShatterParams prm;

    switch (w->step) {
    case 0:
        nj_fill8(&desc, 0, sizeof desc);
        nj_fill8(&prm, 0, sizeof prm);
        desc.models = stage_models;
        desc.type = enb_hunter_body_models[w->kind];
        desc.kind = 0x50000;
        desc.motion = enb_hunter_body_motions[w->kind];
        desc.time = w->a.frame;
        v3_copy(&desc.pos, &w->fx_pos);
        i3_copy(desc.ang, w->ang);
        v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
        v3_set(&desc.v, 0, 0, 0);
        prm.Gravity = 10.0f;
        prm.PowVec = 0.9f;
        prm.PowAng = 6.7f;
        prm.RtoVecDec = 0.94f;
        prm.RtoAngDec = 0.98f;
        prm.RotSpdPlayer = 1.0f;
        prm.TimeAlphaStart = 1.0f;
        prm.TimeAlphaEnd = 80.0f;
        prm.force_wire = 0;
        prm.pcol_wire = RGBA(0x68, 0x1e, 0, 0x80);
        prm.b_acc = 0;
        prm.frm_acc = 0;
        enemy_shatter_create(w, &desc, (float *)&prm, 0);
        w->flags |= 1;
        w->step++;
        w->next_step++;
    case 1:
        w->size -= ENB_HUNTER_SET(w->kind).v[6] * FRAME_SEC;
        if (w->size <= 0.0f) {
            FINISH_IF(w);
            break;
        }
        if (w->state != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b14998 stg04, 0x00b1dc30 stg22 */
void enb_hunter_exec(EnbHunterWork *w)
{
    int n = enemy_status_get_ticks();
    Vec3 ofs;
    void (*fn)(EnbHunterWork *);

    enb_hunter_hit_check(w);
    player_get_frame_vel(&ofs);
    v3_add(&w->fx_pos, &ofs);
    do {
        w->state = w->next;
        w->step = w->next_step;
    } while (0);
    while (n--) {
        do {
            w->state = w->next;
            w->step = w->next_step;
        } while (0);
        w->life -= 1.0f;
        if (w->life <= 0.0f || ((int (*)(EnbHunterWork *))enb_hunter_area_check)(w)) { /* steer(cast) */
            FINISH_IF(w);
            return;
        }
        enb_hunter_crash_check(w);
        fn = enb_hunter_states[w->state];
        if (fn)
            fn(w);
    }
    w->a.frame = w->ring.frame = w->ring.frames * w->size / enb_hunter_max_sizes[w->kind];
}

/* 0x00b14b20 stg04, 0x00b1ddb8 stg22 */
#if defined(OVERLAY_STG22)
void enb_hunter_disp(EnbHunterWork *w)
{
    Vec3 *p = &w->fx_pos;
    Vec3 pt;
    int dl;
    ModelPart *body;

    if (view_sphere_visible(p, w->size)) {
        ulGxSetTextureList(stage_textures);
        ulMatrixPush();
        do {
            Vec3 *p_ = p;

            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        enb_hunter_rotate_yxz(w->ang);
        vu0_matrix_rotate_z(ulMatrixCur, w->roll);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        if (!(w->flags & 1))
            dc_object_draw(&w->a);
        dl = RenderSetCurrentDisplayList(6);
        if (!(w->flags & 1)) {
            ulGxSetTextureList(stage_textures);
            v3_set(&pt, 0, 0, 0);
            draw_set_blend(4, 1);
            billboard_draw(4, COL_WHITE, &pt, 6.0f, 6.0f);
        }
        dc_object_draw(&w->ring);
        RenderSetCurrentDisplayList(dl);
        ulMatrixPop();
    }
    if (debug_flags_test(16) && enb_hunter_sets[w->kind].v[10]) {
        debug_set_origin(10, 20);
        v3_copy(&pt, &w->fx_pos);
        debug_printf(str_enb_hunter_pos, (int)pt.x, (int)pt.y, (int)pt.z);
        debug_newline();
        body = &w->a;
        debug_printf(str_enb_hunter_frm, (int)body->frame, (int)body->frames);
        debug_newline();
    }
}
#else
void enb_hunter_disp(EnbHunterWork *w)
{
    Vec3 *p = &w->fx_pos;
    Vec3 pt;
    int dl;
    ModelPart *body;

    if (view_sphere_visible(p, w->size)) {
        ulGxSetTextureList(stage_textures);
        ulMatrixPush();
        do {
            Vec3 *p_ = p;

            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        enb_hunter_rotate_yxz(w->ang);
        vu0_matrix_rotate_z(ulMatrixCur, w->roll);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        if (!(w->flags & 1))
            dc_object_draw(&w->a);
        dl = RenderSetCurrentDisplayList(6);
        if (!(w->flags & 1)) {
            ulGxSetTextureList(stage_textures);
            v3_set(&pt, 0, 0, 0);
            draw_set_blend(4, 1);
            billboard_draw(6, COL_WHITE, &pt, 6.0f, 6.0f);
        }
        dc_object_draw(&w->ring);
        RenderSetCurrentDisplayList(dl);
        ulMatrixPop();
    }
    if (debug_flags_test(16) && enb_hunter_sets[w->kind].v[10]) {
        debug_set_origin(10, 20);
        v3_copy(&pt, &w->fx_pos);
        debug_printf(str_enb_hunter_pos, (int)pt.x, (int)pt.y, (int)pt.z);
        debug_newline();
        body = &w->a;
        debug_printf(str_enb_hunter_frm, (int)body->frame, (int)body->frames);
        debug_newline();
    }
}
#endif

/* 0x00b14cf8 stg04, 0x00b1df90 stg22 */
/* steer(cast) */
void enb_hunter_init(void *task, int pos, int ang, int kind)
{
    EnbHunterWork *w = task;
    Vec3 ofs;

    player_get_frame_vel(&ofs);
    v3_sub(&w->fx_pos, (Vec3 *)pos, &ofs);
    i3_copy(w->ang, (int *)ang);
    w->kind = kind;
    w->life = ENB_HUNTER_SET(kind).v[9] * FPS;
    util_rotate_xyz((int *)ang, 0.0f, 0.0f, -1.0f, &w->dir);
    dc_object_init(&w->a, *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, enb_hunter_body_models[kind]),
                  *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, enb_hunter_body_motions[kind]), 4096);
    dc_object_init(&w->ring, *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, enb_hunter_glow_models[kind]),
                  *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, enb_hunter_glow_motions[kind]), 0);
    dc_object_rebuild(&w->a);
    dc_object_rebuild(&w->ring);
    w->fx = enemy_status_ent_alloc((&enb_hunter_init_arg)[kind], -1, w);
    w->fx_op = 3;
    enemy_status_event(w->fx, 2, &w->fx_pos);
    lockon_obj_init(&w->lock, w, &w->fx_pos, 0, enb_hunter_lock_names[kind], w->fx, enb_hunter_sets[w->kind].v[0],
                  enb_hunter_sets[w->kind].v[0], 0);
    lockon_entry(&w->lock);
}

/* 0x00b14f50 stg04, 0x00b1e1e8 stg22 */
void enb_hunter_release(EnbHunterWork *w)
{
    enemy_status_event(w->fx, w->fx_op, &w->fx_pos);
    lockon_remove(&w->lock);
    dc_object_release(&w->ring);
    dc_object_release(&w->a);
}

/* 0x00b14f98 stg04, 0x00b1e230 stg22 */
void *enb_hunter_create(void *parent, int pos, int ang, int kind)
{
    void *w;

    enb_hunter_desc.name = enb_hunter_names[kind];
    w = task_create_sibling_inherit(parent, &enb_hunter_desc, 340);
    enb_hunter_init(w, pos, ang, kind);
    return w;
}

/* 0x00b15020 stg04, 0x00b1e2b8 stg22 */
void enb_hunter_models_init(void)
{
}

/* 0x00b15028 stg04, 0x00b1e2c0 stg22 */
void enb_hunter_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
