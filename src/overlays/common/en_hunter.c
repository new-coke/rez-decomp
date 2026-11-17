/*
 * Hunter (stg04): rises into view, fades in and floats swinging, then fires an enb_hunter shot
 * from each gun point with a muzzle flash; charges, then escapes on a growing exhaust flame. Its
 * parts can be shot off one by one.
 */

#include "types_common.h"
#include "task.h"

#include "types_overlays.h"

#define PART_MOTION(p) ((ModelPart *)(p))

#include "debug.h"
#include "anim.h"
#include "color.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "nj_system.h"
#include "node_tree.h"
#include "player.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "ul_vu0.h"
#include "billboard.h"
#include "enemy_task.h"
#include "model.h"
#include "node_draw_state.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "en_hunter.h"
#include "en_backfire.h"
#include "en_break.h"
#include "en_kawasemi_flash.h"
#include "en_perfectwall.h"
#include "enb_hunter.h"
#include "enemy_shatter.h"
#include "enemy_vars.h"
#include "s_sin.h"
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

static inline void v3_scale(Vec3 *v, float k)
{
    v->x *= k;
    v->y *= k;
    v->z *= k;
}

static inline int v3_is_zero(Vec3 *v)
{
    int z = 0;

    if (fabsf(v->x) < 0.01f && fabsf(v->y) < 0.01f && fabsf(v->z) < 0.01f)
        z = 1;
    return z;
}

extern int ulMatrixPush(void);
extern struct GwVarNode *gw_varedit_root;

extern void *stage_textures;
extern void *stage_models;

extern void debug_set_color(unsigned int c);

static inline void i3_scale(int *v, float k)
{
    v[0] *= k;
    v[1] *= k;
    v[2] *= k;
}

static inline void i3_add(int *dst, int *src)
{
    dst[0] += src[0];
    dst[1] += src[1];
    dst[2] += src[2];
}

extern ObjStateFn en_hunter_exec_table[];
extern ParamRec en_hunter_params;
extern VarDef en_hunter_vardefs[20];

#define EN_HUNTER_REC ((EnHunterRec *)&en_hunter_params)

typedef struct EnHunterCbArg {
    EnHunterWork *work;
    int i;
} EnHunterCbArg;

extern void (*en_hunter_states[])(void *o, EnHunterWork *w);
extern void (*en_hunter_disp_table[])(void *o, EnHunterWork *w);
extern int en_hunter_part_bits[4];
extern int en_hunter_part_break_types[4];
extern int en_hunter_part_models[4];
extern char en_hunter_name[];
extern char en_hunter_lock_name[];
extern char str_en_hunter_pos[];
extern char str_en_hunter_vec[];
extern char str_en_hunter_ang[];
extern char str_en_hunter_frm[];
extern char str_en_hunter_cnt[];
extern char str_en_hunter_mod[];
extern char str_en_hunter_blr[];
extern char str_en_hunter_col[];
extern char str_en_hunter_fir[];
extern unsigned int *en_hunter_body_nodes[4];
extern unsigned int *en_hunter_core_nodes[4];
extern int en_hunter_part_model[4];
extern int en_hunter_core_model;
extern int en_hunter_col_motion;
extern int en_hunter_part_motion[4];
extern int en_hunter_core_cache;
extern float en_hunter_fade_color[4];

/* model list entry n as stg22 numbers it */
#if defined(OVERLAY_STG22)
#define EN_HUNTER_MODEL(n) ((n) + 101)
#else
#define EN_HUNTER_MODEL(n) (n)
#endif

#if defined(OVERLAY_STG22)
typedef struct EnHunterNodeCtx {
    EnHunterWork *work;
    int i;                    /* 0x04: the part, counted up per tagged node */
} EnHunterNodeCtx;
#endif

/* 0x00b058b0 stg04, 0x00b01ed0 stg22 */
void en_hunter_vars_register(void)
{
    VarDef hdr;
    VarDef *def;
    int *val;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&hdr, 0, sizeof hdr);
    hdr.name = en_hunter_params.name;
    hdr.fmt = en_hunter_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&hdr);
    def = en_hunter_vardefs;
    val = en_hunter_params.v;
    for (i = 0; i < 20; i++) {
        def->ptr = val;
        val++;
        def++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_hunter_vardefs);
}

/* 0x00b05960 stg04, 0x00b01f80 stg22 */
void en_hunter_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b05968 stg04, 0x00b01f88 stg22 */
int en_hunter_marker_draw(int obj, void *place)
{
    int *mdl;

    ulGxSetTextureList(stage_textures);
    mdl = pnb_get_entry(stage_models, EN_HUNTER_MODEL(19));
    return obj_draw_at(obj, place, (void *)*mdl);
}

/* 0x00b059c0 stg04, 0x00b01fe0 stg22 */
int en_hunter_vars_init(void)
{
    en_hunter_vars_register();
    return 0;
}

/* 0x00b059e0 stg04, 0x00b02000 stg22 */
int en_hunter_vars_release(void)
{
    en_hunter_vars_unregister();
    return 0;
}

/* 0x00b05a00 stg04, 0x00b02020 stg22 */
void en_hunter_calc_tip(void *obj, EnHunterWork *w)
{
    float xy;

    if (w->skip & 0x20) {
        xy = 0.0f;
        util_rotate_xyz(enemy_task_rot_ptr(obj), xy, xy, -7.0f, &w->muzzle);
        v3_add(&w->muzzle, enemy_task_pos_ptr(obj));
    }
    w->skip |= 1;
}

/* 0x00b05ab8 stg04, 0x00b020d8 stg22 */
void en_hunter_calc_spawn_points(void *obj, EnHunterWork *w)
{
    Vec3 pts[8];
    Vec3 ofs;
    float xy;
    int i;

    if (w->skip & 8) {
        enemy_task_spawn(obj, 0x1000000, 8, pts, 0, 0);
        xy = 0.0f;
        util_rotate_xyz(enemy_task_rot_ptr(obj), xy, xy, -2.0f, &ofs);
        for (i = 7; i >= 0; i--) {
            v3_copy(&w->pts[i], &pts[i]);
            v3_add(&w->pts[i], &ofs);
        }
    }
    w->skip |= 2;
}

/* 0x00b05b98 stg04, 0x00b021b8 stg22 */
void en_hunter_calc_points(void *obj, EnHunterWork *w)
{
    if (!(w->skip & 1))
        en_hunter_calc_tip(obj, w);
    if (!(w->skip & 2))
        en_hunter_calc_spawn_points(obj, w);
    w->skip &= ~3;
}

/* 0x00b05c08 stg04, 0x00b02228 stg22 */
void en_hunter_break_part(void *obj, EnHunterWork *w, int part)
{
    SpawnDesc desc;
    float mtx[4][16];
    ModelPart *gfx;
    int bit = en_hunter_part_bits[part];

    w->skip |= 128 << bit;
    nj_fill8(&desc, 0, sizeof desc);
    desc.kind = 0x80000;
    desc.models = stage_models;
    desc.type = en_hunter_part_break_types[part];
    desc.v.x = 0;
    desc.v.y = 0;
    desc.v.z = 0;
    gfx = enemy_task_gfx_ptr(obj);
    anim_start((void *)gfx->clip, gfx->frame);
    node_tree_draw_matching_at(gfx->root, 0x4000000, bit + 1, (int)mtx, 1);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = enemy_task_pos_ptr(obj);

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_hunter_rotate_yxz(enemy_task_rot_ptr(obj));
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, mtx[bit]);
    enemy_shatter_create(obj, &desc, 0, 0);
    ulMatrixPop();
}

/* 0x00b05d60 stg04, 0x00b02380 stg22 */
void en_hunter_hit_check(void *obj, EnHunterWork *w)
{
    LockObj *lock = &w->lock;
    EnHunterRec *rec;
    int hits;
    int i;
    float xy;

    if (lock->flags & LOCK_HIT) {
        hits = lock->life;
        xy = 0.0f;
        rec = EN_HUNTER_REC;
        util_rotate_xyz(enemy_task_rot_ptr(obj), xy, xy, rec->pow_laser, &w->knock);
        if (hits <= 0) {
            enemy_status_score_popup(enemy_task_get_status_slot(obj), enemy_task_pos_ptr(obj), rec->score, lockon_hits_max_shot_cnt(lock));
            lock->flags &= ~LOCK_LOCKABLE;
            enemy_task_set_state(obj, 3);
        } else {
            if (hits == rec->hp - 1) {
                for (i = 5 - rec->hp; --i >= 0;)
                    en_hunter_break_part(obj, w, i);
            }
            if (hits < 5)
                en_hunter_break_part(obj, w, 4 - hits);
            else
                en_break_spawn(obj, enemy_task_pos_ptr(obj), enemy_task_rot_ptr(obj), 0);
            w->wait = EN_HUNTER_REC->frm_stop_motion * FPS;
        }
    }
}

/* 0x00b05f18 stg04, 0x00b02538 stg22 */
void en_hunter_swing(void *obj, EnHunterWork *w)
{
    Vec3 pos;
    float t, r;

    t = w->t += 0.005f;
    r = EN_HUNTER_REC->r_swing;
    enemy_task_get_pos(obj, &pos);
    v3_sub(&pos, &pos, &w->off);
    v3_set(&w->off, sinf(t * 3.0f) * r, cosf(t * 5.0f) * r, sinf(t * 4.0f) * r);
    v3_add(&pos, &w->off);
    enemy_task_set_pos(obj, &pos);
}

/* 0x00b06050 stg04, 0x00b02670 stg22 */
void en_hunter_fire_flashes(void *obj, EnHunterWork *w)
{
    Vec3 *pt = &w->pts[0];

    en_hunter_calc_spawn_points(obj, w);
    if (!(w->skip & 0x80))
        en_kawasemi_flash_create(obj, pt, 0);
    if (!(w->skip & 0x100))
        en_kawasemi_flash_create(obj, &w->pts[1], 0);
    if (!(w->skip & 0x200))
        en_kawasemi_flash_create(obj, &w->pts[3], 0);
    if (!(w->skip & 0x400)) {
        en_kawasemi_flash_create(obj, &w->pts[4], 0);
        en_kawasemi_flash_create(obj, &w->pts[5], 0);
    }
    en_kawasemi_flash_create(obj, &w->pts[2], 0);
    en_kawasemi_flash_create(obj, &w->pts[6], 0);
    w->flash = en_kawasemi_flash_create(obj, &w->pts[7], 0);
}

/* 0x00b06150 stg04, 0x00b02770 stg22 */
void en_hunter_fire_shots(void *obj, EnHunterWork *w)
{
    Vec3 pts[8];
    int ang[3];

    enemy_task_spawn(obj, 0x1000000, 8, pts, 0, 0);
    enemy_task_get_rot(obj, ang);
    if (!(w->skip & 0x80))
        enb_hunter_create(obj, (int)&pts[0], (int)ang, 0);
    if (!(w->skip & 0x100))
        enb_hunter_create(obj, (int)&pts[1], (int)ang, 0);
    if (!(w->skip & 0x200))
        enb_hunter_create(obj, (int)&pts[3], (int)ang, 0);
    if (!(w->skip & 0x400)) {
        enb_hunter_create(obj, (int)&pts[4], (int)ang, 0);
        enb_hunter_create(obj, (int)&pts[5], (int)ang, 0);
    }
    enb_hunter_create(obj, (int)&pts[2], (int)ang, 0);
    enb_hunter_create(obj, (int)&pts[6], (int)ang, 0);
    enb_hunter_create(obj, (int)&pts[7], (int)ang, 0);
    util_rotate_xyz(ang, 0.0f, 0.0f, EN_HUNTER_REC->pow_shot, &w->knock);
}

/* 0x00b062a8 stg04, 0x00b028c8 stg22 */
void en_hunter_toggle_parts(void *obj, EnHunterWork *w)
{
    if (w->skip & 0x80) {
        util_node_toggle(en_hunter_body_nodes[0]);
        util_node_toggle(en_hunter_core_nodes[0]);
    } else if (w->skip & 0x100) {
        util_node_toggle(en_hunter_body_nodes[1]);
        util_node_toggle(en_hunter_core_nodes[1]);
    }
    if (w->skip & 0x200) {
        util_node_toggle(en_hunter_body_nodes[2]);
        util_node_toggle(en_hunter_core_nodes[2]);
    }
    if (!(w->skip & 0x400))
        return;
    util_node_toggle(en_hunter_body_nodes[3]);
    util_node_toggle(en_hunter_core_nodes[3]);
}

/* 0x00b06368 stg04, 0x00b02988 stg22 */
void en_hunter_state_rise(void *obj, EnHunterWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 pos;
    Vec3 pl;
    Vec3 v;
    Vec3 cur;
    Vec3 vel;
    float cnt_start, hgt, cnt, alpha, dt;

    switch (w->step) {
    case 0:
        enemy_task_get_pos(obj, &pos);
        cnt_start = 1.0f;
        player_get_frame_pos(&pl);
        hgt = pl.y - pos.y + enemy_task_get_param(obj, 3);
        v3_set(&v, 0, hgt * 0.029999971f / (enemy_task_get_param(obj, 2) * 0.029999971f + cnt_start), 0);
        enemy_task_set_param_vec(obj, &v);
        gfx->speed = gfx->frames / enemy_task_get_param(obj, 2);
        w->skip |= 0x800;
        w->cnt = cnt_start;
        w->cnt_end = enemy_task_get_param(obj, 2);
        w->step++;
        w->next_step++;
    case 1:
        dt = 1.0f;
        enemy_task_get_pos(obj, &cur);
        enemy_task_get_param_vec(obj, &vel);
        v3_add(&cur, &vel);
        enemy_task_set_pos(obj, &cur);
        w->wait -= dt;
        if (w->wait < 0.0f)
            dc_object_advance_time(gfx);
        if (w->skip & 0x800) {
            cnt = w->cnt;
            w->cnt = cnt + dt;
            if (30.0f < w->cnt_end)
                alpha = cnt / 30.0f;
            else
                alpha = cnt / w->cnt_end;
            w->alpha = alpha;
            if (1.0f <= alpha) {
                w->alpha = 1.0f;
                w->skip &= ~0x800;
            }
        }
        if (gfx->flags2 & 1) {
            enemy_task_get_param_vec(obj, &w->knock);
            if (0.0f < enemy_task_get_param(obj, 1)) {
                w->next_step = 0;
                w->next = 2;
            } else {
                switch (w->type) {
                case 1:
                    do {
                        w->next_step = 0;
                        w->next = 4;
                    } while (0);
                    break;
                case 2:
                    w->next_step = 0;
                    w->next = 6;
                    break;
                }
            }
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b06640 stg04, 0x00b02c60 stg22 */
/* steer(cast) */
void en_hunter_state_fade_in(void *obj, EnHunterWork *w)
{
    ModelPart *gfx;
    unsigned int step = w->step;
    float cnt, alpha;

    switch (step) {
    case 0:
        gfx = enemy_task_gfx_ptr(obj);
        dc_object_set_clip(gfx, *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, EN_HUNTER_MODEL(22)));
        *(int *)&gfx->frame = 0; /* steer(type): stored as an int zero, as the original */
        w->wire = *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, EN_HUNTER_MODEL(27));
        w->skip |= 0x800;
        w->cnt = 1.0f;
        w->cnt_end = enemy_task_get_param(obj, 1);
        w->step++;
        w->next_step++;
    case 1:
        if (w->skip & 0x800) {
            cnt = w->cnt;
            w->cnt = cnt + 1.0f;
            if (30.0f < w->cnt_end)
                alpha = cnt / 30.0f;
            else
                alpha = cnt / w->cnt_end;
            w->alpha = alpha;
            if (1.0f <= alpha) {
                w->alpha = 1.0f;
                w->skip &= ~0x800;
            }
        }
        w->cnt += 1.0f;
        if (w->cnt_end < w->cnt) {
            if (0.0f < enemy_task_get_param(obj, 1)) {
                do {
                    w->next_step = 0;
                    w->next = 3;
                } while (0);
            } else
                w->next_step = w->step + 1;
        }
        step = w->step;
        if (w->phase != w->next || step != w->next_step)
            w->step = step + 1;
        break;
    case 2:
        break;
    }
}

/* 0x00b06818 stg04, 0x00b02e38 stg22 */
/* steer(cast) */
void en_hunter_state_float(void *obj, EnHunterWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    float dt, start;

    switch (w->step) {
    case 0:
        dc_object_set_clip(gfx, *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, EN_HUNTER_MODEL(23)));
        *(int *)&gfx->frame = 0; /* steer(type): stored as an int zero, as the original */
        gfx->speed = gfx->frames / (EN_HUNTER_REC->frm_float_motion * FPS);
        dc_object_set_flags(gfx, 4096);
        w->wire = *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, EN_HUNTER_MODEL(28));
        w->cnt = 1.0f;
        w->cnt_end = enemy_task_get_param(obj, 1);
        w->step++;
        w->next_step++;
    case 1:
        dt = 1.0f;
        start = 0.0f;
        w->wait -= dt;
        if (w->wait < start)
            dc_object_advance_time(gfx);
        w->cnt += dt;
        if (w->cnt_end < w->cnt && (gfx->flags2 & 1)) {
            gfx->frame = start;
            w->next = 3;
            w->next_step = 0;
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b069b0 stg04, 0x00b02fd0 stg22 */
/* steer(cast) */
void en_hunter_state_attack(void *obj, EnHunterWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    float sec, frm;

    switch (w->step) {
    case 0:
        sec = FPS;
        dc_object_set_clip(gfx, *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, EN_HUNTER_MODEL(22)));
        gfx->speed = gfx->frames / (EN_HUNTER_REC->frm_attack_motion * sec);
        gfx->frame = 0.0f;
        dc_object_clear_flags(gfx, 4096);
        w->wire = *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, EN_HUNTER_MODEL(27));
        w->cnt = 1.0f;
        w->step++;
        w->next_step++;
        w->cnt_end = EN_HUNTER_REC->frm_attack_motion * sec;
    case 1:
        w->wait -= 1.0f;
        if (w->wait < 0.0f)
            dc_object_advance_time(gfx);
        frm = gfx->frame;
        if (!(w->skip & 8) && 11.0f <= frm) {
            w->skip |= 8;
            en_hunter_fire_flashes(obj, w);
        }
        if (!(w->skip & 0x10) && 18.0f <= frm) {
            w->skip |= 0x10;
            en_hunter_fire_shots(obj, w);
        }
        if (gfx->flags2 & 1) {
            switch (w->type) {
            case 1:
                do {
                    w->next_step = 0;
                    w->next = 4;
                } while (0);
                break;
            case 2:
                do {
                    w->next_step = 0;
                    w->next = 6;
                } while (0);
                break;
            case 3:
                do {
                    w->next_step = 0;
                    w->next = 7;
                } while (0);
                break;
            }
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b06bd8 stg04, 0x00b031f8 stg22 */
float en_hunter_ease_curve(float t)
{
    t = MIN(t >= 0.0f ? t : 0.0f, 1.0f);
    if (t < 0.15f)
        t = ease_sine_in_out(t / 0.15f) * 0.8f;
    else
        t = (t - 0.15f) * 0.2f / 0.85f + 0.8f;
    return t;
}

/* 0x00b06ca8 stg04, 0x00b032c8 stg22 */
/* steer(cast) */
void en_hunter_state_beam(void *obj, EnHunterWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    BackFireLink lnk;
    Vec3 pos;
    Vec3 v;
    float frm, scl;
    double sway;

    switch (w->step) {
    case 0:
        v3_set(&w->drift, 0.0f, 0.0f, 0.0f);
        dc_object_set_clip(gfx, *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, EN_HUNTER_MODEL(21)));
        ((struct { int w[20]; } *)gfx)->w[15] = 0; /* steer(alias): an int zero stored as a member of the part, as the original */
        PART_MOTION(gfx)->speed = gfx->frames / (EN_HUNTER_REC->frm_escape_motion * FPS);
        w->wire = *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, EN_HUNTER_MODEL(26));
        w->step++;
        w->next_step++;
    case 1:
        w->wait -= 1.0f;
        if (w->wait < 0.0f)
            dc_object_advance_time(gfx);
        frm = gfx->frame;
        if (!(w->skip & 0x20) && 36.0f <= frm) {
            nj_fill8(&lnk, 0, sizeof lnk);
            lnk.gfx = 0;
            lnk.pos = &w->muzzle;
            lnk.ang = enemy_task_rot_ptr(obj);
            lnk.obj_scale = &w->beam;
            en_backfire_create(obj, &lnk, 1);
            w->skip |= 0x20;
            v3_set(&w->beam, EN_HUNTER_REC->sca_fire_min, EN_HUNTER_REC->sca_fire_min, EN_HUNTER_REC->sca_fire_min);
        }
        if (36.0f <= frm && frm < 108.0f) {
            scl = en_hunter_ease_curve((frm - 36.0f) / 72.0f);
            scl = EN_HUNTER_REC->sca_fire_min + (EN_HUNTER_REC->sca_fire_max - EN_HUNTER_REC->sca_fire_min) * scl;
            v3_set(&w->beam, scl, scl, scl);
        }
        if (!(w->skip & 0x40) && 108.0f <= frm)
            w->skip |= 0x40;
        enemy_task_get_pos(obj, &pos);
        player_get_frame_accel(&v);
        if (w->skip & 0x40) {
            w->speed += EN_HUNTER_REC->acc_escape1 * 0.00027777778f;
            w->speed = MIN(w->speed, EN_HUNTER_REC->spd_escape1_max * FRAME_SEC);
            vec3_scale_to_length(w->speed, &v, &v);
            v3_add(&pos, &v);
        } else {
            v3_sub(&pos, &pos, &w->drift);
            sway = -EN_HUNTER_REC->dst_escape_anticipate;
            ((int (*)(Vec3 *, float, Vec3 *))vec3_scale_to_length)(&v, (float)(sway * sin(frm * 1.5707964f / 108.0f)), &w->drift);
            v3_add(&pos, &w->drift);
        }
        enemy_task_set_pos(obj, &pos);
        if (gfx->flags2 & 1) {
            do {
                w->next_step = 0;
                w->next = 5;
            } while (0);
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b070d0 stg04, 0x00b036f0 stg22 */
void en_hunter_state_charge(void *obj, EnHunterWork *w)
{
    Vec3 pos;
    Vec3 v;
    float scl, cnt;

    switch (w->step) {
    case 0:
        w->step++;
        w->next_step++;
    case 1:
        w->speed += EN_HUNTER_REC->acc_escape1 * 0.00027777778f;
        w->speed = MIN(w->speed, EN_HUNTER_REC->spd_escape1_max * FRAME_SEC);
        enemy_task_get_pos(obj, &pos);
        player_get_frame_accel(&v);
        if (v3_is_zero(&v)) {
            v3_set(&v, 0, 0, -1.0f);
            util_dir_to_player_frame(&v, &v);
        }
        vec3_scale_to_length(w->speed, &v, &v);
        v3_add(&pos, &v);
        enemy_task_set_pos(obj, &pos);
        scl = w->beam.x * 0.97f;
        scl = EN_HUNTER_REC->sca_fire_min <= scl ? scl : EN_HUNTER_REC->sca_fire_min;
        v3_set(&w->beam, scl, scl, scl);
        if (util_eye_outside(&pos)) {
            w->cnt = 1.0f;
            w->cnt_end = 30.0f;
            w->skip |= 0x800;
            do {
                w->next_step = w->step + 1;
            } while (0);
        }
        if (w->phase == w->next && w->step == w->next_step)
            break;
        w->step++;
    case 2:
        cnt = w->cnt;
        w->cnt = cnt + 1.0f;
        w->alpha = 1.0f - cnt / w->cnt_end;
        if (w->alpha <= 0.0f)
            enemy_task_set_state(obj, 2);
        break;
    }
}

/* 0x00b07360 stg04, 0x00b03980 stg22 */
void en_hunter_state_retreat(void *obj, EnHunterWork *w)
{
    Vec3 pos;
    int ang[3];
    Vec3 v;

    switch (w->step) {
    case 0:
        w->step++;
        w->next_step++;
        w->aknock[2] = -w->spin;
    case 1:
        w->speed += EN_HUNTER_REC->acc_escape2 * 0.00027777778f;
        w->speed = MIN(w->speed, EN_HUNTER_REC->spd_escape2_max * FRAME_SEC);
        enemy_task_get_pos(obj, &pos);
        player_get_frame_accel(&v);
        if (v3_is_zero(&v)) {
            v3_set(&v, 0.0f, 0.0f, -1.0f);
            util_dir_to_player_frame(&v, &v);
        }
        vec3_scale_to_length(w->speed, &v, &v);
        v3_sub(&pos, &pos, &v);
        enemy_task_set_pos(obj, &pos);
        enemy_task_get_rot(obj, ang);
        ang[2] += w->spin;
        enemy_task_set_rot(obj, ang);
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b07550 stg04, 0x00b03b70 stg22 */
void en_hunter_exec_normal(void *obj, EnHunterWork *w)
{
    Vec3 *pos;
    int *ang;
    Vec3 cur;
    int rot[3];
    Vec3 v;

    do {
        w->phase = w->next;
        w->step = w->next_step;
    } while (0);
    if (enemy_task_get_phase(obj) == 1) {
        v3_scale(&w->knock, 0.97f);
        pos = enemy_task_pos_ptr(obj);
        v3_add(pos, &w->knock);
        i3_scale(w->aknock, 0.95f);
        ang = enemy_task_rot_ptr(obj);
        i3_add(ang, w->aknock);
        en_hunter_swing(obj, w);
        if (w->type == 3) {
            enemy_task_get_rot(obj, rot);
            rot[2] += w->spin;
            enemy_task_set_rot(obj, rot);
            enemy_task_get_pos(obj, &cur);
            enemy_task_get_param_vec(obj, &v);
            v3_add(&cur, &v);
            enemy_task_set_pos(obj, &cur);
        }
        if (en_hunter_states[w->phase])
            en_hunter_states[w->phase](obj, w);
    }
}

/* 0x00b07760 stg04, 0x00b03d80 stg22 */
void en_hunter_exec_destroyed(void *obj, EnHunterWork *w)
{
    SpawnDesc desc;
    int mode = enemy_task_get_phase(obj);

    if (mode != 1) {
        if (mode == 0) {
            nj_fill8(&desc, 0, sizeof desc);
            desc.kind = 0x40000;
            desc.models = stage_models;
            desc.type = EN_HUNTER_MODEL(19);
            if (w->phase < 3)
                desc.motion = EN_HUNTER_MODEL(20);
            else if (w->phase < 4)
                desc.motion = EN_HUNTER_MODEL(22);
            else
                desc.motion = EN_HUNTER_MODEL(21);
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

/* 0x00b07870 stg04, 0x00b03e90 stg22 */
void en_hunter_exec(void *obj, void *w)
{
    ObjStateFn *tab = en_hunter_exec_table;
    unsigned int state = enemy_task_get_pstate(obj);

    if (tab[state])
        tab[state](obj, w);
}

/* 0x00b078d0 stg04, 0x00b03ef0 stg22 */
void en_hunter_pre_exec(void *obj, EnHunterWork *w)
{
    if ((w->skip & 8) && (!w->flash || (TASK_OF(w->flash)->flags & TASK_F_END)))
        w->skip &= ~8;
    en_hunter_hit_check(obj, w);
    switch (w->type) {
    case 2:
    case 3:
        if (util_ahead_of_player(enemy_task_pos_ptr(obj), EN_HUNTER_REC->dst_away_z))
            enemy_task_set_state(obj, 2);
        break;
    }
}

/* 0x00b07988 stg04, 0x00b03fa8 stg22 */
void en_hunter_post_exec(void *obj, EnHunterWork *w)
{
    en_hunter_calc_points(obj, w);
}

/* 0x00b07990 stg04, 0x00b03fb0 stg22 */
int en_hunter_node_cb(unsigned int *node, int idx, EnHunterCbArg *cb)
{
    int part;

    if (node[0] & 0x4000000) {
        part = cb->i++;
        if (cb->work->skip & (128 << part)) {
            draw_push_blend();
            draw_set_blend(4, 5);
            model_draw_kind_list(en_hunter_part_model[part], en_hunter_part_motion[part]);
            draw_pop_blend();
            return 0;
        }
    }
    return 1;
}

/* 0x00b07a38 stg04, 0x00b04058 stg22 */
#if defined(OVERLAY_STG22)
/* steer(cast) */
void en_hunter_disp_fade(void *obj, EnHunterWork *w)
{
    Vec3 *pos = enemy_task_pos_ptr(obj);
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    EnHunterNodeCtx cb;
    Vec3 v;
    float lw;
    float prev_lw;
    int blend;
    unsigned int col;

    if (!view_sphere_visible(pos, gfx->radius))
        return;
    blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    ulGxSetTextureList(stage_textures);
    draw_push_strip_flags();
    draw_push_override_color();
    ulMatrixPush();
    do {
        Vec3 *p_ = pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_hunter_rotate_yxz(enemy_task_rot_ptr(obj));
    lw = 1.0f;
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    en_hunter_toggle_parts(obj, w);
    en_hunter_fade_color[0] = w->alpha - lw;
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    draw_set_override_color(en_hunter_fade_color);
    cb.work = w;
    cb.i = 0;
    prev_lw = line_width_set(lw);
    draw_set_color((EN_HUNTER_REC->col_wire & 0xffffff) |
                  ((int)((float)color_get_a(EN_HUNTER_REC->col_wire) * w->alpha) + 1) >> 1 << 24);
    dc_object_set_callback(gfx, en_hunter_node_cb, &cb);
    dc_object_draw(gfx);
    dc_object_set_callback(gfx, 0, 0);
    line_width_set(prev_lw);
    if (w->type != 1 && w->phase >= 4)
        col = RGBA(0x4f, 0, 0, 0x15);
    else
        col = motion_sample_rgba((void *)en_hunter_col_motion,
                                   (float)(unsigned int)(((int *)en_hunter_col_motion)[1] - 1) * beat_clock_get_phase(),
                                   100.0f, 400.0f, 1.0f, 0.2f);
    draw_set_color((col & 0xffffff) | ((int)((float)color_get_a(col) * w->alpha) + 1) >> 1 << 24);
    model_node_walk_list_anim(en_hunter_core_model, w->wire, gfx->frame, en_hunter_core_cache);
    en_hunter_toggle_parts(obj, w);
    v.x = 0;
    v.y = 0;
    v.z = 0;
    draw_set_blend(4, 1);
    billboard_draw(3, COL_WHITE, &v, 1.7f, 1.7f);
    ulMatrixPop();
    draw_pop_override_color();
    draw_pop_strip_flags();
    draw_set_override_flags(blend);
}
#else
/* steer(cast) */
void en_hunter_disp_fade(void *obj, EnHunterWork *w)
{
    Vec3 *pos = enemy_task_pos_ptr(obj);
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    EnHunterCbArg cb;
    Vec3 v;
    float lw;
    float prev_lw;
    int blend;
    unsigned int col;

    if (!view_sphere_visible(pos, gfx->radius))
        return;
    blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    ulGxSetTextureList(stage_textures);
    draw_push_strip_flags();
    draw_push_override_color();
    ulMatrixPush();
    do {
        Vec3 *p_ = pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_hunter_rotate_yxz(enemy_task_rot_ptr(obj));
    lw = 1.0f;
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    en_hunter_toggle_parts(obj, w);
    en_hunter_fade_color[0] = w->alpha - lw;
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    draw_set_override_color(en_hunter_fade_color);
    cb.work = w;
    cb.i = 0;
    prev_lw = line_width_set(lw);
    draw_set_color((EN_HUNTER_REC->col_wire & 0xffffff) |
                  ((int)((float)color_get_a(EN_HUNTER_REC->col_wire) * w->alpha) + 1) >> 1 << 24);
    dc_object_set_callback(gfx, en_hunter_node_cb, &cb);
    dc_object_draw(gfx);
    dc_object_set_callback(gfx, 0, 0);
    line_width_set(prev_lw);
    if (w->type != 1 && w->phase >= 4)
        col = RGBA(0x4f, 0, 0, 0x15);
    else
        col = motion_sample_rgba((void *)en_hunter_col_motion,
                                   (float)(unsigned int)(((int *)en_hunter_col_motion)[1] - 1) * beat_clock_get_phase(),
                                   100.0f, 400.0f, 1.0f, 0.2f);
    draw_set_color((col & 0xffffff) | ((int)((float)color_get_a(col) * w->alpha) + 1) >> 1 << 24);
    model_node_walk_list_anim(en_hunter_core_model, w->wire, gfx->frame, en_hunter_core_cache);
    en_hunter_toggle_parts(obj, w);
    v.x = 0;
    v.y = 0;
    v.z = 0;
    draw_set_blend(4, 1);
    billboard_draw(1, COL_WHITE, &v, 1.7f, 1.7f);
    ulMatrixPop();
    draw_pop_override_color();
    draw_pop_strip_flags();
    draw_set_override_flags(blend);
}
#endif

/* 0x00b07d50 stg04, 0x00b04370 stg22 */
#if defined(OVERLAY_STG22)
void en_hunter_disp_body(void *obj, EnHunterWork *w)
{
    Vec3 *pos;
    ModelPart *gfx;
    EnHunterNodeCtx cb;
    Vec3 v;
    float prev_lw;

    if (w->skip & 0x800) {
        en_hunter_disp_fade(obj, w);
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
        en_hunter_rotate_yxz(enemy_task_rot_ptr(obj));
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        en_hunter_toggle_parts(obj, w);
        cb.work = w;
        cb.i = 0;
        prev_lw = line_width_set(1.0f);
        draw_set_color(EN_HUNTER_REC->col_wire);
        dc_object_set_callback(gfx, en_hunter_node_cb, &cb);
        dc_object_draw(gfx);
        dc_object_set_callback(gfx, 0, 0);
        line_width_set(prev_lw);
        if (w->type != 1 && w->phase >= 4)
            draw_set_color(RGBA(0x4f, 0, 0, 0x15));
        else
            draw_set_color(motion_sample_rgba((void *)en_hunter_col_motion,
                                               (float)(unsigned int)(((int *)en_hunter_col_motion)[1] - 1) *
                                                   beat_clock_get_phase(),
                                               100.0f, 400.0f, 1.0f, 0.2f));
        model_node_walk_list_anim(en_hunter_core_model, w->wire, gfx->frame, en_hunter_core_cache);
        en_hunter_toggle_parts(obj, w);
        v.x = 0;
        v.y = 0;
        v.z = 0;
        draw_set_blend(4, 1);
        billboard_draw(3, COL_WHITE, &v, 1.7f, 1.7f);
        ulMatrixPop();
    }
}
#else
void en_hunter_disp_body(void *obj, EnHunterWork *w)
{
    Vec3 *pos;
    ModelPart *gfx;
    EnHunterCbArg cb;
    Vec3 v;
    float prev_lw;

    if (w->skip & 0x800) {
        en_hunter_disp_fade(obj, w);
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
        en_hunter_rotate_yxz(enemy_task_rot_ptr(obj));
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        en_hunter_toggle_parts(obj, w);
        cb.work = w;
        cb.i = 0;
        prev_lw = line_width_set(1.0f);
        draw_set_color(EN_HUNTER_REC->col_wire);
        dc_object_set_callback(gfx, en_hunter_node_cb, &cb);
        dc_object_draw(gfx);
        dc_object_set_callback(gfx, 0, 0);
        line_width_set(prev_lw);
        if (w->type != 1 && w->phase >= 4)
            draw_set_color(RGBA(0x4f, 0, 0, 0x15));
        else
            draw_set_color(motion_sample_rgba((void *)en_hunter_col_motion,
                                               (float)(unsigned int)(((int *)en_hunter_col_motion)[1] - 1) *
                                                   beat_clock_get_phase(),
                                               100.0f, 400.0f, 1.0f, 0.2f));
        model_node_walk_list_anim(en_hunter_core_model, w->wire, gfx->frame, en_hunter_core_cache);
        en_hunter_toggle_parts(obj, w);
        v.x = 0;
        v.y = 0;
        v.z = 0;
        draw_set_blend(4, 1);
        billboard_draw(1, COL_WHITE, &v, 1.7f, 1.7f);
        ulMatrixPop();
    }
}
#endif

/* 0x00b07fa8 stg04, 0x00b045c8 stg22 */
/* steer(cast) */
void en_hunter_disp(void *obj, EnHunterWork *w)
{
    void (**tbl)(void *obj, EnHunterWork *w) = en_hunter_disp_table;
    unsigned int state;
    Vec3 pos;
    Vec3 v;
    int ang[3];
    Vec3 off;
    ModelPart *gfx;
    unsigned int col;

    state = enemy_task_get_pstate(obj);
    if (tbl[state])
        tbl[state](obj, w);
    if (debug_flags_test(16) && EN_HUNTER_REC->test) {
        debug_set_origin(10, 10);
        enemy_task_get_pos(obj, &pos);
        debug_printf(str_en_hunter_pos, (int)pos.x, (int)pos.y, (int)pos.z);
        debug_newline();
        enemy_task_get_param_vec(obj, &v);
        debug_printf(str_en_hunter_vec, v.x, v.y, v.z);
        debug_newline();
        enemy_task_get_rot(obj, ang);
        debug_printf(str_en_hunter_ang, (int)(ang[0] * DEG_PER_ANG), (int)(ang[1] * DEG_PER_ANG),
                     (int)(ang[2] % 65536 * DEG_PER_ANG));
        debug_newline();
        gfx = enemy_task_gfx_ptr(obj);
        debug_printf(str_en_hunter_frm, (int)gfx->frame, (int)gfx->frames);
        debug_newline();
        debug_printf(str_en_hunter_cnt, (int)w->cnt, (int)w->cnt_end);
        debug_newline();
        debug_printf(str_en_hunter_mod, w->phase);
        debug_newline();
        v3_copy(&off, &w->off);
        debug_printf(str_en_hunter_blr, off.x, off.y, off.z);
        debug_newline();
        col = motion_sample_rgba((void *)en_hunter_col_motion,
                                 (float)(unsigned int)(((int *)en_hunter_col_motion)[1] - 1) * beat_clock_get_phase(), 0.0f,
                                 0.0f, 0.0f, 0.0f);
        debug_set_color(col);
        debug_printf(str_en_hunter_col, color_get_a(col), ((unsigned int (*)(int))color_get_r)(col), color_get_g(col), color_get_b(col));
        debug_set_color(RGBA(0x60, 0x60, 0x60, 0x80));
        debug_newline();
        debug_printf(str_en_hunter_fir, w->beam.x);
        debug_newline();
    }
}

/* 0x00b08318 stg04, 0x00b04938 stg22 */
/* steer(cast) */
void en_hunter_init(void *obj, EnHunterWork *w)
{
    int type;

    w->pos = enemy_task_pos_ptr(obj);
    w->vel = enemy_task_param_vec_ptr(obj);
    w->ang = enemy_task_rot_ptr(obj);
    w->scale = enemy_task_scl_ptr(obj);
    type = (int)enemy_task_get_param(obj, 0);
    w->type = MIN(3, MAX(1, type));
    if (w->type == 1)
        enemy_task_clear_flags(obj, 64);
    w->spin = (int)DEG_TO_ANG(enemy_task_get_param(obj, 4) * FRAME_SEC);
    lockon_obj_init(&w->lock, obj, enemy_task_pos_ptr(obj), 0, en_hunter_lock_name, enemy_task_get_status_slot(obj), EN_HUNTER_REC->hp,
                  EN_HUNTER_REC->hp, 0);
    lockon_entry(&w->lock);
    enemy_task_set_appear(obj, 0, 0);
    enemy_task_set_vanish(obj, 0, 0);
    switch (w->type) {
    case 1:
    case 2:
        do {
            w->next = 0;
            w->next_step = 0;
        } while (0);
        break;
    case 3:
        do {
            w->next = 1;
            w->next_step = 0;
        } while (0);
        break;
    }
    w->wire = *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, EN_HUNTER_MODEL(25));
}

/* 0x00b084c0 stg04, 0x00b04ae0 stg22 */
void en_hunter_release(void *obj, char *w)
{
    lockon_remove((LockObj *)(w + 232));
}

/* 0x00b084c8 stg04, 0x00b04ae8 stg22 */
int en_hunter_create(void *parent, void *place)
{
    ObjDesc desc;
    ObjFuncs funcs;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&funcs, 0, sizeof funcs);
    funcs.fn[0] = en_hunter_init;
    funcs.fn[1] = en_hunter_release;
    funcs.fn[2] = en_hunter_exec;
    funcs.fn[3] = en_hunter_disp;
    funcs.fn[4] = en_hunter_pre_exec;
    funcs.fn[5] = en_hunter_post_exec;
    desc.name = en_hunter_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = EN_HUNTER_MODEL(19);
    desc.model_no2 = EN_HUNTER_MODEL(20);
    desc.hp = EN_HUNTER_REC->hp;
    desc.score = EN_HUNTER_REC->score;
    desc.crash_r = EN_HUNTER_REC->collision_r;
    desc.size = 312;
    desc.time = 0.0f;
    desc.wob_amp = 0;
    desc.wob_spd = 0;
    return enemy_task_task_create(parent, &desc);
}

/* 0x00b08600 stg04, 0x00b04c20 stg22 */
/* steer(cast) */
void en_hunter_nodes_init(void)
{
    int i;
    int mdl_no;
    int bit;
    int *core_mdl = &en_hunter_core_model;
    int *core_cache = &en_hunter_core_cache;
    int *col_mot = &en_hunter_col_motion;

    node_tree_collect_to((void *)*((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, EN_HUNTER_MODEL(19)), 0x4000000, (struct NodeTreeNode * *)en_hunter_body_nodes);
    *core_mdl = *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, EN_HUNTER_MODEL(24));
    *core_cache = model_pnb_get_wire(stage_models, EN_HUNTER_MODEL(24));
    *col_mot = *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, EN_HUNTER_MODEL(29));
    node_tree_collect_to((void *)*core_mdl, 0x4000000, (struct NodeTreeNode * *)en_hunter_core_nodes);
    for (i = 3; i >= 0; i--) {
        mdl_no = en_hunter_part_models[i];
        bit = en_hunter_part_bits[i];
        en_hunter_part_model[bit] = *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, mdl_no);
        en_hunter_part_motion[bit] = model_pnb_get_wire(stage_models, mdl_no);
    }
}

/* 0x00b08750 stg04, 0x00b04d70 stg22 */
void en_hunter_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
