/*
 * Balloon payload, Area 4: a core flies at the player while 44 fragments spread around it (mode
 * 2: they slow later). All can be shot; stragglers and those out of view are dropped.
 */

#include "stg04.h"
#include "mathdef.h"
#include "debug.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_cnk_vu1.h"
#include "nj_system.h"
#include "node_tree.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "enb_balloon.h"
#include "../common/en_backfire.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"

extern int enb_balloon_init_arg2;
extern ParamRec enb_balloon_params;
extern VarDef enb_balloon_vardefs[10];
extern int enb_balloon_init_arg;
extern struct TaskDesc enb_balloon_desc;

/* enb_balloon_params, 10 words */
typedef struct EnbBalloonRec {
    char *name;
    char *label;                /* 0x04 */
    int hp_core;                /* 0x08 */
    int hp_fragment;            /* 0x0c */
    int score_core;             /* 0x10 */
    int score_fragment;         /* 0x14 */
    float clip_ang;             /* 0x18 */
    float clip_z;               /* 0x1c */
    int use_mode_b;             /* 0x20 */
    float spd_move;             /* 0x24 */
    float spd_move_min;         /* 0x28 */
    float sca_fire;             /* 0x2c */
} EnbBalloonRec;

#define ENB_BALLOON_REC ((EnbBalloonRec *)&enb_balloon_params)
extern float enb_balloon_core_matrix[16];
extern Vec3 enb_balloon_node_offsets[22];
extern int enb_balloon_node_angs[22][3];
extern void *enb_balloon_nodes[22];
extern char enb_balloon_lock_name[];
extern char enb_balloon_core_lock_name[];
extern void (*enb_balloon_states[])(EnbBalloonWork *w);

/* 0x00b114b8 */
void enb_balloon_vars_register(void)
{
    VarDef hdr;
    VarDef *def;
    int *prm;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&hdr, 0, sizeof hdr);
    hdr.name = enb_balloon_params.name;
    hdr.fmt = enb_balloon_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&hdr);
    def = enb_balloon_vardefs;
    prm = enb_balloon_params.v;
    for (i = 0; i < 10; i++) {
        def->ptr = prm;
        prm++;
        def++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)enb_balloon_vardefs);
}

/* 0x00b11568 */
void enb_balloon_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b11570 */
int enb_balloon_vars_init(int *fx_id)
{
    enb_balloon_vars_register();
    enb_balloon_init_arg = *fx_id;
    return 0;
}

/* 0x00b115a8 */
int enb_balloon_vars_init_core(int *fx_id)
{
    enb_balloon_init_arg2 = *fx_id;
    return 0;
}

/* 0x00b115c0 */
int enb_balloon_vars_release(void)
{
    enb_balloon_vars_unregister();
    return 0;
}

/* 0x00b115e0 */
void enb_balloon_calc_lock_point(EnbBalloonWork *w)
{
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &w->fx_pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    enb_balloon_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, enb_balloon_core_matrix);
    ulMatrixGetTranslation((float *)ulMatrixCur, &w->lpos.x);
    ulMatrixPop();
}

/* 0x00b11668 */
void enb_balloon_release_core(EnbBalloonWork *w)
{
    if (!(w->flags & 1)) {
        enemy_status_event(w->fx, w->fx_op, &w->fx_pos);
        lockon_remove(&w->lock);
        FINISH_IF(w->sub);
        w->flags |= 1;
    }
}

/* 0x00b116d8 */
void enb_balloon_release_fragment(EnbBalloonWork *w, int i)
{
    unsigned char *sts = w->st;
    unsigned char *pst = &sts[i];

    if (!(*pst & 1)) {
        enemy_status_event(w->fxs[i], w->fx_ops[i], &w->pts[i]);
        lockon_remove(&w->locks[i]);
        w->n--;
        *pst |= 1;
    }
}

/* 0x00b11778 */
void enb_balloon_shatter_core(EnbBalloonWork *w)
{
    SpawnDesc desc;

    nj_fill8(&desc, 0, sizeof desc);
    v3_copy(&desc.pos, &w->fx_pos);
    desc.type = 17;
    desc.kind = 0;
    desc.models = stage_models;
    i3_copy(desc.ang, w->ang);
    v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
    v3_set(&desc.v, 0, 0, 0);
    enemy_shatter_create(w, &desc, 0, 0);
}

/* 0x00b11830 */
void enb_balloon_break_fragment(EnbBalloonWork *w, int i)
{
    SpawnDesc desc;

    nj_fill8(&desc, 0, sizeof desc);
    desc.kind = 0x20000;
    desc.models = stage_models;
    desc.type = 11;
    desc.idx = i % 22 + 2;
    v3_copy(&desc.pos, &w->pts[i]);
    i3_copy(desc.ang, w->angs[i]);
    v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
    v3_set(&desc.v, 0, 0, 0);
    enemy_shatter_create(w, &desc, 0, 0);
}

/* 0x00b11928 */
void enb_balloon_hit_check(EnbBalloonWork *w)
{
    int i;

    for (i = 43; i >= 0; i--) {
        if (!(w->st[i] & 1)) {
            if (w->locks[i].flags & LOCK_HIT && w->locks[i].life <= 0) {
                LockObj *lk = &w->locks[i];

                lk->flags &= ~LOCK_LOCKABLE;
                enemy_status_score_popup(w->fxs[i], &w->pts[i], ENB_BALLOON_REC->score_fragment, lockon_hits_max_shot_cnt(lk));
                w->fx_ops[i] = 3;
                enb_balloon_break_fragment(w, i);
                enb_balloon_release_fragment(w, i);
            }
        }
    }
    if (!(w->flags & 1)) {
        LockObj *lk = &w->lock;

        if (lk->flags & LOCK_HIT && lk->life <= 0) {
            lk->flags &= ~LOCK_LOCKABLE;
            enemy_status_score_popup(w->fx, &w->fx_pos, ENB_BALLOON_REC->score_core, lockon_hits_max_shot_cnt(lk));
            w->fx_op = 3;
            enb_balloon_shatter_core(w);
            enb_balloon_release_core(w);
        }
    }
}

/* 0x00b11ad8 */
void enb_balloon_crash_check(EnbBalloonWork *w)
{
    if (!(w->flags & 1) && player_sphere_test(&w->fx_pos, 1.0f) == 1) {
        player_status_add_damage(1.0f);
        w->fx_op = 5;
        enb_balloon_shatter_core(w);
        enb_balloon_release_core(w);
    }
}

/* 0x00b11b60 */
void enb_balloon_clip_fragments(EnbBalloonWork *w)
{
    Vec3 pl;
    float dist_sq;
    int i;

    player_get_pos(&pl);
    dist_sq = ENB_BALLOON_REC->clip_z * ENB_BALLOON_REC->clip_z;
    for (i = 43; i >= 0; i--) {
        if (w->st[i] & 1)
            continue;
        if (dist_sq < vec3_distance_squared(&pl, &w->pts[i])) {
            enb_balloon_release_fragment(w, i);
            continue;
        }
        if (!yuki_camera_relative(&w->pts[i], (int)DEG_TO_ANG(ENB_BALLOON_REC->clip_ang), 1.0f)) {
            enb_balloon_break_fragment(w, i);
            enb_balloon_release_fragment(w, i);
        }
    }
}

static inline void v3_add_to(Vec3 *lhs, Vec3 *rhs, Vec3 *dst)
{
    float t = rhs->x, u = rhs->y;

    if ((((int)dst) & 1) >> 1)
        t = 0;
    dst->x = lhs->x + t;
    dst->y = lhs->y + u;
    dst->z = lhs->z + rhs->z;
}

/* 0x00b11c78 */
void enb_balloon_setup_a(EnbBalloonWork *w, Vec3 *pos, int *ang)
{
    Vec3 v;
    Vec3 pvel;
    float sc[2][2];
    float x, y, z, scl;
    Vec3 *dir;
    int i;

    ulMatrixPush();
    matrix_unit_set(0);
    enb_balloon_rotate_yxz(ang + (((int)ang & 1) >> 1) + (((int)ang & 2) >> 2) + (((int)ang & 4) >> 3)); /* steer(zero) */
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    matrix_apply_vectors(enb_balloon_node_offsets, w->dirs, 22);
    for (i = 1; i > 0; i--)
        do { nj_copy8(&w->dirs[22 * i], &w->dirs[0], 264); } while (0);
    ulMatrixPop();
    for (i = 43; i >= 0; i--) {
        v3_add_to(pos, &w->dirs[i], &w->pts[i]);
        i3_copy(w->angs[i], ang);
        dir = &w->dirs[i];
        scl = RAND_FLOAT_A() * 0.40000004f + 0.8f;
        v3_scale(dir, scl);
        z = dir->z;
        x = dir->x;
        y = dir->y;
        ROT_X((int)DEG_TO_ANG(RAND_FLOAT_A() * 90.0f + -45.0f), sc[0], y, z);
        ROT_Y((int)DEG_TO_ANG(RAND_FLOAT_A() * 90.0f + -45.0f), sc[1], x, z);
        dir->x = x;
        dir->y = y;
        dir->z = z;
        vec3_scale_to_length(ENB_BALLOON_REC->spd_move * FRAME_SEC, dir, dir);
    }
    v3_set(&v, 0, 0, 2.7f);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    enb_balloon_rotate_yxz(ang);
    ulMatrixApplyPoint((float *)ulMatrixCur, &v.x, &w->fx_pos.x);
    ulMatrixPop();
    player_get_pos(&v);
    player_get_vec(&pvel);
    v3_sub(&v, &v, &pvel);
    v3_sub(&w->vel, &v, &w->fx_pos);
    vec3_scale_to_length(ENB_BALLOON_REC->spd_move * FRAME_SEC, &w->vel, &w->vel);
    do {
        w->ang[1] = (int)RAD_TO_ANG(atan2f(-w->vel.x, -w->vel.z));
        w->ang[0] = (int)RAD_TO_ANG(atan2f(w->vel.y, sqrtf(w->vel.x * w->vel.x + w->vel.z * w->vel.z)));
    } while (0);
}

/* 0x00b12110 */
void enb_balloon_setup_b(EnbBalloonWork *w, Vec3 *pos, int *ang)
{
    Vec3 v;
    Vec3 pvel;
    float sc[2][2];
    float x, y, z, scl;
    Vec3 *dir;
    int i, j;

    ulMatrixPush();
    matrix_unit_set(0);
    enb_balloon_rotate_yxz(ang + (((int)ang & 1) >> 1) + (((int)ang & 2) >> 2)); /* steer(zero) */
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    matrix_apply_vectors(enb_balloon_node_offsets, w->dirs, 22);
    for (i = 1; i > 0; i--)
        do { nj_copy8(&w->dirs[22 * i], &w->dirs[0], 264); } while (0);
    ulMatrixPop();
    for (i = 43; i >= 0; i--) {
        v3_add_to(pos, &w->dirs[i], &w->pts[i]);
        i3_copy(w->angs[i], ang);
        dir = &w->dirs[i];
        scl = RAND_FLOAT_A() * 0.40000004f + 0.8f;
        v3_scale(dir, scl);
        z = dir->z;
        x = dir->x;
        y = dir->y;
        ROT_X((int)DEG_TO_ANG(RAND_FLOAT_A() * 90.0f + -45.0f), sc[0], y, z);
        ROT_Y((int)DEG_TO_ANG(RAND_FLOAT_A() * 90.0f + -45.0f), sc[1], x, z);
        dir->x = x;
        dir->y = y;
        dir->z = z;
        vec3_scale_to_length(ENB_BALLOON_REC->spd_move * FRAME_SEC, dir, dir);
    }
    for (j = 43; j >= 0; j--)
        w->delay[j] = RAND_FLOAT_A() * 60.0f + 60.0f;
    v3_set(&v, 0, 0, 2.7f);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    enb_balloon_rotate_yxz(ang);
    ulMatrixApplyPoint((float *)ulMatrixCur, &v.x, &w->fx_pos.x);
    ulMatrixPop();
    player_get_pos(&v);
    player_get_vec(&pvel);
    v3_sub(&v, &v, &pvel);
    v3_sub(&w->vel, &v, &w->fx_pos);
    vec3_scale_to_length(ENB_BALLOON_REC->spd_move * FRAME_SEC, &w->vel, &w->vel);
    do {
        w->ang[1] = (int)RAD_TO_ANG(atan2f(-w->vel.x, -w->vel.z));
        w->ang[0] = (int)RAD_TO_ANG(atan2f(w->vel.y, sqrtf(w->vel.x * w->vel.x + w->vel.z * w->vel.z)));
    } while (0);
}

/* 0x00b12610 */
void enb_balloon_state_a(EnbBalloonWork *w)
{
    int i;
    int step, state, next;

    switch (w->step) {
    case 0:
        w->unk_1478 = 1.0f;
        w->step++;
        w->next_step++;
    case 1:
        step = w->step;
        state = w->state;
        next = w->next;
        for (i = 43; i >= 0; i--) {
            if (w->st[i] & 1)
                continue;
            {
                Vec3 *vel = &w->dirs[i];
                Vec3 *p = &w->pts[i];

                v3_add(p, vel);
            }
            w->angs[i][2] += 1092;
        }
        {
            Vec3 *vel = &w->vel;
            Vec3 *p = &w->fx_pos;

            v3_add(p, vel);
        }
        w->ang[2] += 1092;
        if (state != next || step != w->next_step)
            w->step = step + 1;
        break;
    case 2:
        break;
    }
}

/* 0x00b12720 */
void enb_balloon_state_b(EnbBalloonWork *w)
{
    Vec3 org;
    Vec3 *dir;
    float len, spd_min;
    int i;

    switch (w->step) {
    case 0:
        w->unk_1478 = 1.0f;
        w->step++;
        w->next_step++;
    case 1:
        v3_set(&org, 0, 0, 0);
        for (i = 43; i >= 0; i--) {
            dir = &w->dirs[i];
            if (w->st[i] & 1)
                continue;
            if ((w->delay[i] -= 1.0f) < 0.0f) {
                v3_scale(dir, 0.99f);
                len = vec3_length(dir);
                spd_min = ENB_BALLOON_REC->spd_move_min * FRAME_SEC;
                if (len < spd_min)
                    v3_scale(dir, spd_min / len);
            }
            v3_add(&w->pts[i], dir);
            w->angs[i][2] = w->angs[i][2] + vec3_length(dir) * 1092.0f / 0.33333334f;
        }
        {
            Vec3 *vel = &w->vel;
            Vec3 *pos = &w->fx_pos;

            v3_add(pos, vel);
        }
        w->ang[2] += 1092;
        if (w->state != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b129c0 */
void enb_balloon_exec(EnbBalloonWork *w)
{
    int n = enemy_status_get_ticks();
    Vec3 ofs;
    Vec3 *p;
    int i;
    void (*fn)(EnbBalloonWork *);

    enb_balloon_hit_check(w);
    player_get_frame_vel(&ofs);
    for (i = 43, p = &w->pts[44]; i >= 0; i--)
        v3_add(--p, &ofs);
    player_get_vec(&ofs);
    v3_add(&w->fx_pos, &ofs);
    if (ENB_BALLOON_REC->use_mode_b) {
        player_get_accel(&ofs);
        v3_scale(&ofs, -FRAME_SEC);
        for (i = 43, p = &w->pts[44]; i >= 0; i--)
            v3_add(--p, &ofs);
    }
    if (!w->sub || (TASK_OF(w->sub)->flags & TASK_F_END))
        w->sub = 0;
    do {
        w->state = w->next;
        w->step = w->next_step;
    } while (0);
    while (n--) {
        do {
            w->state = w->next;
            w->step = w->next_step;
        } while (0);
        enb_balloon_clip_fragments(w);
        enb_balloon_crash_check(w);
        if (!w->n && (w->flags & 1)) {
            FINISH_IF(w);
            return;
        }
        fn = enb_balloon_states[w->state];
        if (fn)
            fn(w);
    }
    enb_balloon_calc_lock_point(w);
}

/* 0x00b12c08 */
void enb_balloon_disp(EnbBalloonWork *w)
{
    int i, k;

    ulGxSetTextureList(w->textures);
    for (k = 1; k >= 0; k--) {
        for (i = 21; i >= 0; i--) {
            if (!(w->st[22 * k + i] & 1) && view_sphere_visible(&w->pts[22 * k + i], 1.0f)) {
                ulMatrixPush();
                do {
                    Vec3 *p_ = &w->pts[22 * k + i];

                    vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
                } while (0);
                enb_balloon_rotate_yxz(w->angs[22 * k + i]);
                vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
                nj_cnk_vu1_draw(enb_balloon_nodes[i]);
                ulMatrixPop();
            }
        }
    }
    if (!(w->flags & 1) && view_sphere_visible(&w->fx_pos, 1.0f)) {
        ulMatrixPush();
        do {
            Vec3 *p_ = &w->fx_pos;

            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        enb_balloon_rotate_yxz(w->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        dc_object_draw(&w->part);
        ulMatrixPop();
    }
}

/* steer(cast) */
/* 0x00b12dd0 */
void enb_balloon_init(EnbBalloonWork *w, Vec3 *pos, void *place)
{
    int *ang = place;
    Vec3 bpos;
    Vec3 ofs;
    BackFireLink lnk;
    int i;
    int old_tex;

    player_get_frame_vel(&ofs);
    v3_sub(&bpos, pos, &ofs);
    dc_object_init(&w->part, *((struct DcNode **)pnb_get_entry(stage_models, 18)), 0, 0);
    dc_object_rebuild(&w->part);
    if (ENB_BALLOON_REC->use_mode_b) {
        do { w->next_step = 0; w->next = 1; } while (0);
        enb_balloon_setup_b(w, &bpos, ang);
    } else
        enb_balloon_setup_a(w, &bpos, ang);
    w->n = 44;
    for (i = 43; i >= 0; i--) {
        w->fxs[i] = enemy_status_ent_alloc(enb_balloon_init_arg, -1, w);
        w->fx_ops[i] = 3;
        enemy_status_event(w->fxs[i], 2, &w->pts[i]);
        lockon_obj_init(&w->locks[i], w, &w->pts[i], 0, enb_balloon_lock_name, w->fxs[i], ENB_BALLOON_REC->hp_fragment,
                      ENB_BALLOON_REC->hp_fragment, 0);
        lockon_entry(&w->locks[i]);
    }
    w->fx = enemy_status_ent_alloc(enb_balloon_init_arg2, -1, w);
    w->fx_op = 3;
    enemy_status_event(w->fx, 2, &w->fx_pos);
    lockon_obj_init(&w->lock, w, &w->fx_pos, 0, enb_balloon_core_lock_name, w->fx, ENB_BALLOON_REC->hp_core, ENB_BALLOON_REC->hp_core,
                  0);
    lockon_entry(&w->lock);
    nj_fill8(&lnk, 0, 52);
    lnk.obj_scale = &w->lscale;
    lnk.pos = &w->lpos;
    lnk.ang = w->ang;
    lnk.gfx = 0;
    old_tex = en_backfire_set_texture(5);
    w->sub = en_backfire_create(w, &lnk, 1);
    en_backfire_set_texture(old_tex);
    v3_set(&w->lscale, ENB_BALLOON_REC->sca_fire, ENB_BALLOON_REC->sca_fire, ENB_BALLOON_REC->sca_fire);
}

/* 0x00b13078 */
void enb_balloon_release(EnbBalloonWork *w)
{
    int i;

    for (i = 43; i >= 0; i--)
        enb_balloon_release_fragment(w, i);
    enb_balloon_release_core(w);
    dc_object_release(&w->part);
}

/* 0x00b130d8 */
EnbBalloonWork *enb_balloon_create(void *parent, Vec3 *pos, void *place)
{
    EnbBalloonWork *w = task_create_sibling_inherit(parent, &enb_balloon_desc, 5248);

    enb_balloon_init(w, pos, place);
    return w;
}

/* steer(cast) */
/* 0x00b13138 */
void enb_balloon_models_init(void)
{
    unsigned int *root = (unsigned int *)((unsigned int *)*((int *)pnb_get_entry(stage_models, 11)))[11];
    unsigned int *node;
    int i = 0;

    node = (unsigned int *)root[12];
    do {
        enb_balloon_nodes[i] = ((void **)node)[1];
        v3_copy(&enb_balloon_node_offsets[i], (Vec3 *)&node[2]);
        i3_copy(enb_balloon_node_angs[i], (int *)&node[5]);
        i++;
        node = ((unsigned int **)node)[12];
    } while (node);
    node_tree_draw_matching_at(*((int *)pnb_get_entry(stage_models, 18)), 0x40000000, 1, (int)enb_balloon_core_matrix, 0);
}

/* 0x00b13200 */
void enb_balloon_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
