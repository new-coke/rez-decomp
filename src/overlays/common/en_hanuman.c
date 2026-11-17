/*
 * Hanuman (stg02): flies to a point near the player, shrinking from its approach size, stops and
 * plays its motion, drifts, winds up, then escapes until out of range. Carries a shield.
 */

#include "types_common.h"
#include "task.h"
#include "color.h"

#include "types_overlays.h"

#include "debug.h"
#include "approach.h"
#include "camera.h"
#include "debugdraw.h"
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
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_hanuman.h"
#include "en_backfire.h"
#include "en_hanuman_shield.h"
#include "enemy_shatter.h"
#include "enemy_vars.h"
#include "s_cos.h"
#include "s_sin.h"
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

/* sc receives the sine and cosine */
#define ROT_Z(ang, sc, x, y) \
    do { \
        float rot_u = x, rot_v = y; \
        ulSinCos(ang, &sc[0], &sc[1]); \
        x = rot_u * sc[1] - rot_v * sc[0]; \
        y = rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

#define ROT_X(ang, sc, y, z) \
    do { \
        float rot_u = y, rot_v = z; \
        ulSinCos(ang, &sc[0], &sc[1]); \
        y = rot_u * sc[1] - rot_v * sc[0]; \
        z = rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

#define ROT_Y(ang, sc, x, z) \
    do { \
        float rot_u = x, rot_v = z; \
        ulSinCos(ang, &sc[0], &sc[1]); \
        x = rot_u * sc[1] + rot_v * sc[0]; \
        z = -rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

static inline void v3_sub(Vec3 *dst, Vec3 *lhs, Vec3 *rhs)
{
    dst->x = lhs->x - rhs->x;
    dst->y = lhs->y - rhs->y;
    dst->z = lhs->z - rhs->z;
}

static inline void v3_sum(Vec3 *dst, Vec3 *lhs, Vec3 *rhs)
{
    dst->x = lhs->x + rhs->x;
    dst->y = lhs->y + rhs->y;
    dst->z = lhs->z + rhs->z;
}

static inline void v3_scale(Vec3 *v, float k)
{
    v->x *= k;
    v->y *= k;
    v->z *= k;
}

static inline void v3_mul(Vec3 *dst, Vec3 *src, float scale)
{
    dst->x = src->x * scale;
    dst->y = src->y * scale;
    dst->z = src->z * scale;
}

static inline int v3_is_zero(Vec3 *v)
{
    int z = 0;

    if (fabsf(v->x) < 0.01f && fabsf(v->y) < 0.01f && fabsf(v->z) < 0.01f)
        z = 1;
    return z;
}

static inline void i3_set(int *dst, int x, int y, int z)
{
    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
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

static inline void i3_add(int *dst, int *src)
{
    dst[0] += src[0];
    dst[1] += src[1];
    dst[2] += src[2];
}

static inline void i3_mul(int *dst, int *src, float scale)
{
    dst[0] = src[0] * scale;
    dst[1] = src[1] * scale;
    dst[2] = src[2] * scale;
}

static inline void i3_sub(int *dst, int *src)
{
    dst[0] -= src[0];
    dst[1] -= src[1];
    dst[2] -= src[2];
}

extern void (*en_hanuman_exec_table[])(void *o, void *w);
extern void (*en_hanuman_disp_table[])(void *o, void *w);
extern ParamRec en_hanuman_params;
extern VarDef en_hanuman_vardefs[17];

#define EN_HANUMAN_REC ((EnHanumanRec *)&en_hanuman_params)
extern void (*en_hanuman_states[])(void *o, EnHanumanWork *w);

extern char en_hanuman_name[];

extern char en_hanuman_lock_name[];

static inline void i3_diff(int *dst, int *lhs, int *rhs)
{
    dst[0] = lhs[0] - rhs[0];
    dst[1] = lhs[1] - rhs[1];
    dst[2] = lhs[2] - rhs[2];
}

/* model list entry n as stg22 numbers it */
#if defined(OVERLAY_STG22)
#define EN_HANUMAN_MODEL(n) ((n) + 81)
#else
#define EN_HANUMAN_MODEL(n) (n)
#endif

/* 0x00b031b0 stg02, 0x00afd4f8 stg22 */
void en_hanuman_vars_register(void)
{
    VarDef grp_def;
    VarDef *def;
    int *val;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&grp_def, 0, sizeof grp_def);
    grp_def.name = en_hanuman_params.name;
    grp_def.fmt = en_hanuman_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&grp_def);
    def = en_hanuman_vardefs;
    val = en_hanuman_params.v;
    for (i = 0; i < 17; i++) {
        def->ptr = val;
        val++;
        def++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_hanuman_vardefs);
}

/* 0x00b03260 stg02, 0x00afd5a8 stg22 */
void en_hanuman_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b03268 stg02, 0x00afd5b0 stg22 */
#if defined(OVERLAY_STG22)
int en_hanuman_marker_draw(int obj, void *place)
{
    EnHanumanArg *prm = place;
    Vec3 p;
    Vec3 v;
    int *mdl;

    ulGxSetTextureList(stage_textures);
    v3_set(&v, prm->mark[0] * DEG_PER_ANG, prm->mark[1] * DEG_PER_ANG, prm->mark[2] * DEG_PER_ANG);
    if (!v3_is_zero(&v)) {
        player_get_pos(&p);
        v3_add(&p, &v);
        do {
            ulMatrixPush();
        } while (0);
        vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
        debug_draw_box(RGBA(0x80, 0, 0, 0x80), 20.0f);
        ulMatrixPop();
    }
    v3_set(&v, prm->mark2.x, prm->mark2.y, prm->mark2.z);
    if (!v3_is_zero(&v)) {
        player_get_pos(&p);
        v3_add(&p, &v);
        do {
            ulMatrixPush();
        } while (0);
        vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        debug_draw_box(RGBA(0, 0x80, 0, 0x80), 20.0f);
        ulMatrixPop();
    }
    v3_set(&v, prm->mark3.x, prm->mark3.y, prm->mark3.z);
    if (!v3_is_zero(&v)) {
        player_get_pos(&p);
        v3_add(&p, &v);
        do {
            ulMatrixPush();
        } while (0);
        vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        debug_draw_box(RGBA(0, 0, 0x80, 0x80), 20.0f);
        ulMatrixPop();
    }
    mdl = pnb_get_entry(stage_models, 106);
    return obj_draw_at(obj, place, (void *)*mdl);
}
#else
int en_hanuman_marker_draw(int obj, EnHanumanArg *prm)
{
    Vec3 p;
    Vec3 v;
    int *mdl;

    ulGxSetTextureList(stage_textures);
    v.x = prm->mark[0] * DEG_PER_ANG;
    v.y = prm->mark[1] * DEG_PER_ANG;
    v.z = prm->mark[2] * DEG_PER_ANG;
    if (!v3_is_zero(&v)) {
        player_get_pos(&p);
        v3_add(&p, &v);
        do {
            ulMatrixPush();
        } while (0);
        vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
        debug_draw_box(RGBA(0x80, 0, 0, 0x80), 20.0f);
        ulMatrixPop();
    }
    v.x = prm->mark2.x;
    v.y = prm->mark2.y;
    v.z = prm->mark2.z;
    if (!v3_is_zero(&v)) {
        player_get_pos(&p);
        v3_add(&p, &v);
        do {
            ulMatrixPush();
        } while (0);
        vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        debug_draw_box(RGBA(0, 0x80, 0, 0x80), 20.0f);
        ulMatrixPop();
    }
    v.x = prm->mark3.x;
    v.y = prm->mark3.y;
    v.z = prm->mark3.z;
    if (!v3_is_zero(&v)) {
        player_get_pos(&p);
        v3_add(&p, &v);
        do {
            ulMatrixPush();
        } while (0);
        vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        debug_draw_box(RGBA(0, 0, 0x80, 0x80), 20.0f);
        ulMatrixPop();
    }
    mdl = pnb_get_entry(stage_models, 25);
    return obj_draw_at(obj, prm, (void *)*mdl);
}
#endif

/* 0x00b03578 stg02, 0x00afd8c0 stg22 */
int en_hanuman_vars_init(void)
{
    en_hanuman_vars_register();
    return 0;
}

/* 0x00b03598 stg02, 0x00afd8e0 stg22 */
int en_hanuman_vars_release(void)
{
    en_hanuman_vars_unregister();
    return 0;
}

/* 0x00b035b8 stg02, 0x00afd900 stg22 */
void en_hanuman_update_target(void *obj, EnHanumanWork *w)
{
    Vec3 p, pl;

    enemy_task_get_pos(obj, &p);
    player_get_frame_pos(&pl);
    v3_sub(&w->rel, &p, &pl);
    if (w->sel == w->cur)
        return;
    switch (w->sel) {
    case 0:
        v3_sub(&w->to, enemy_task_param_vec_ptr(obj), &w->rel);
        break;
    case 1:
        v3_sub(&w->to, &w->tgt[0], &w->rel);
        break;
    default:
        v3_sub(&w->to, &w->tgt[w->sel - 1], &w->rel);
        break;
    }
}

/* 0x00b036e0 stg02, 0x00afda28 stg22 */
void en_hanuman_spawn_shield(void *obj, EnHanumanWork *w)
{
    EnHanumanShieldArg prm;
    Vec3 org;
    float sc[3][2];
    float x, y, z;
    int ax, ay, az;
    int *ang;
    Vec3 *p;
    Task *parent;

    nj_fill8(&prm, 0, sizeof prm);
    x = y = 0.0f;
    ang = prm.ang;
    enemy_task_get_rot(obj, ang);
    z = EN_HANUMAN_REC->shield_z;
    do {
        ax = ang[0];
    } while (0);
    az = ang[2];
    do {
        ay = ang[1];
    } while (0);
    ROT_Z(az, sc[0], x, y);
    ROT_X(ax, sc[1], y, z);
    ROT_Y(ay + ANG_180, sc[2], x, z);
    prm.pos.x = x;
    prm.pos.y = y;
    prm.pos.z = z;
    p = enemy_task_pos_ptr(obj);
    v3_add(&prm.pos, p);
    player_get_frame_vel(&org);
    v3_sub(&prm.pos, &prm.pos, &org);
    parent = TASK_OF(obj)->parent;
    en_hanuman_shield_create(parent ? parent + 1 : 0, &prm, obj, w->dir_set);
}

/* 0x00b03898 stg02, 0x00afdbe0 stg22 */
void en_hanuman_get_spawn_points(void *obj, void *work)
{
    EnHanumanWork *w = work;
    Vec3 p[2];

    enemy_task_spawn(obj, 0x41000000, 2, p, 0, 0);
    v3_copy(&w->vec, &p[0]);
    v3_copy(&w->bf_pos, &p[1]);
}

/* 0x00b03908 stg02, 0x00afdc50 stg22 */
#if defined(OVERLAY_STG22)
/* steer(cast) */
void en_hanuman_state_move(void *obj, EnHanumanWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 prev;
    Vec3 step;
    Vec3 base;
    Vec3 ofs;
    int nang[3];
    Vec3 *pos;
    float rate, scl, one;

    switch (w->step) {
    case 0:
        en_hanuman_update_target(obj, w);
        w->frame = 1.0f;
        w->frames = (int)(vec3_length(&w->to) / (EN_HANUMAN_REC->spd_move * FRAME_SEC));
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 109)));
        gfx->speed = 0.5833333f;
        gfx->frame = 2.0f;
        w->step++;
        w->next_step++;
    case 1:
        rate = w->frame / w->frames;
        enemy_task_get_pos(obj, &prev);
        if (w->frame <= 12.0f)
            dc_object_advance_time(gfx);
        player_get_pos(&base);
        v3_add(&base, &w->rel);
        v3_mul(&ofs, &w->to, rate);
        pos = enemy_task_pos_ptr(obj);
        v3_sum(pos, &base, &ofs);
        pos = enemy_task_pos_ptr(obj);
        v3_sub(&step, pos, &prev);
        do {
            nang[1] = (int)RAD_TO_ANG(atan2f(-step.x, -step.z));
            nang[0] = (int)RAD_TO_ANG(atan2f(step.y, sqrtf(step.x * step.x + step.z * step.z)));
        } while (0);
        nang[2] = 0;
        enemy_task_set_rot(obj, nang);
        scl = 8.0 - sin(rate * 1.5707964f) * 7.0;
        v3_set(&w->scl, scl, scl, scl);
        one = 1.0f;
        w->frame += one;
        if (w->frames < w->frame) {
            v3_copy(&w->last, &step);
            v3_set(&w->scl, one, one, one);
            do {
                w->next = 1;
                w->next_step = 0;
            } while (0);
        }
        break;
    case 2:
    default:
        return;
    }
    if (w->phase != w->next || w->step != w->next_step)
        w->step++;
}
#else
/* steer(cast) */
void en_hanuman_state_move(void *obj, EnHanumanWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 prev;
    Vec3 step;
    Vec3 base;
    Vec3 ofs;
    int nang[3];
    Vec3 *pos;
    float rate, scl, one;

    switch (w->step) {
    case 0:
        en_hanuman_update_target(obj, w);
        w->frame = 1.0f;
        w->frames = (int)(vec3_length(&w->to) / (EN_HANUMAN_REC->spd_move * FRAME_SEC));
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 28)));
        gfx->speed = 0.5833333f;
        gfx->frame = 2.0f;
        w->step++;
        w->next_step++;
    case 1:
        rate = w->frame / w->frames;
        enemy_task_get_pos(obj, &prev);
        if (w->frame <= 12.0f)
            dc_object_advance_time(gfx);
        player_get_pos(&base);
        v3_add(&base, &w->rel);
        v3_mul(&ofs, &w->to, rate);
        pos = enemy_task_pos_ptr(obj);
        v3_sum(pos, &base, &ofs);
        pos = enemy_task_pos_ptr(obj);
        v3_sub(&step, pos, &prev);
        do {
            nang[1] = (int)RAD_TO_ANG(atan2f(-step.x, -step.z));
            nang[0] = (int)RAD_TO_ANG(atan2f(step.y, sqrtf(step.x * step.x + step.z * step.z)));
        } while (0);
        nang[2] = 0;
        enemy_task_set_rot(obj, nang);
        scl = 8.0 - sin(rate * 1.5707964f) * 7.0;
        v3_set(&w->scl, scl, scl, scl);
        one = 1.0f;
        w->frame += one;
        if (w->frames < w->frame) {
            v3_copy(&w->last, &step);
            v3_set(&w->scl, one, one, one);
            do {
                w->next = 1;
                w->next_step = 0;
            } while (0);
        }
        break;
    case 2:
    default:
        return;
    }
    if (w->phase != w->next || w->step != w->next_step)
        w->step++;
}
#endif

/* 0x00b03cb8 stg02, 0x00afe000 stg22 */
float en_hanuman_ease_envelope(float t)
{
    if (t < 0.2f)
        t = ease_sine_in_out(t / 0.2f);
    else if (0.7f < t)
        t = ease_sine_in_out_rev((t - 0.7f) / 0.3f);
    else
        t = 1.0f;
    return t;
}

/* 0x00b03d50 stg02, 0x00afe098 stg22 */
void en_hanuman_state_stop(void *obj, EnHanumanWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    int dang[3];
    int cur[3];
    Vec3 p;
    int nang[3];
    int ofs[3];
    Vec3 step;
    int pl_ang[3];
    Vec3 v;
    int bounce_ang[3];
    int pl_ang2[3];
    float sc[3][2];
    float rate, arate, env, scl, x, y, z, one;

    switch (w->step) {
    case 0:
        do {
            i3_set(dang, 0, ANG_180, 0);
            enemy_task_get_rot(obj, cur);
        } while (0);
        i3_sub(dang, cur);
        dang[0] = dang[0] & 0x8000 ? dang[0] | ~0xffff : dang[0] & 0xffff;
        dang[1] = dang[1] & 0x8000 ? dang[1] | ~0xffff : dang[1] & 0xffff;
        dang[2] = dang[2] & 0x8000 ? dang[2] | ~0xffff : dang[2] & 0xffff;
        do { } while (0);
        if (dang[1] > 0)
            i3_set(w->atgt, 0, 0, (int)DEG_TO_ANG(EN_HANUMAN_REC->ang_rotate_z));
        else
            i3_set(w->atgt, 0, 0, -(int)DEG_TO_ANG(EN_HANUMAN_REC->ang_rotate_z));
        i3_set(w->aoff, 0, 0, 0);
        i3_copy(w->atot, dang);
        w->frame = 1.0f;
        w->frames = model_ivec3_length(dang) / (int)DEG_TO_ANG(EN_HANUMAN_REC->spd_ang_rotate * FRAME_SEC);
        gfx->speed = (gfx->frames - 9.0f) / (12.0f < w->frames ? 12.0f : w->frames);
        w->step++;
        w->next_step++;
        v3_set(&w->moved, 0.0f, 0.0f, 0.0f);
    case 1:
        do {
            rate = w->frame / w->frames;
            one = 1.0f;
            enemy_task_get_pos(obj, &p);
        } while (0);
        enemy_task_get_rot(obj, nang);
        dc_object_advance_time(gfx);
        i3_sub(nang, w->aoff);
        arate = safe_powf(ease_sine_in_out(rate), EN_HANUMAN_REC->pow_stop_ang);
        i3_mul(ofs, w->atot, arate);
        i3_copy(w->aoff, ofs);
        env = en_hanuman_ease_envelope(rate);
        i3_mul(ofs, w->atgt, env);
        i3_add(w->aoff, ofs);
        i3_add(nang, w->aoff);
        {
            float srate = safe_powf(ease_sine_in_out(one - rate), EN_HANUMAN_REC->pow_stop);
            v3_mul(&step, &w->last, srate);
        }
        v3_add(&p, &step);
        v3_add(&w->moved, &step);
        enemy_task_set_pos(obj, &p);
        enemy_task_set_rot(obj, nang);
        if (0.8f < rate)
            v3_set(&w->scl, 8.0f, 8.0f, 8.0f);
        w->frame += one;
        if (w->frames < w->frame) {
            v3_copy(&w->vel, &step);
            w->frame = one;
            w->frames = (int)(EN_HANUMAN_REC->frm_bounce * FPS);
            do { } while (0);
            *(int *)&gfx->frame = 0;
            do { gfx->speed = gfx->frames / w->frames; } while (0);
            w->next_step = w->step + 1;
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        else
            break;
    case 2:
      {
        float bounce_rate, frame_step, turn;
        bounce_rate = w->frame / w->frames;
        x = 0.0f;
        y = 0.0f;
        z = -1.0f;
        player_get_frame_ang(pl_ang);
        ROT_Z(pl_ang[2], sc[0], x, y);
        ROT_X(pl_ang[0], sc[1], y, z);
        ROT_Y(pl_ang[1], sc[2], x, z);
        v3_set(&v, x, y, z);
        frame_step = 1.0f;
        {
            float *vp = &v.x;

            vec3_scale_to_length(-(EN_HANUMAN_REC->spd_bounce * FRAME_SEC)
                                     * ease_sine_in_out(safe_powf(bounce_rate, EN_HANUMAN_REC->pow_bounce)),
                                 vp, vp);
        }
        v3_add(enemy_task_pos_ptr(obj), &v);
        dc_object_advance_time(gfx);
        v3_add(&w->moved, &v);
        turn = frame_step / (w->frames - w->frame + frame_step);
        enemy_task_get_rot(obj, bounce_ang);
        player_get_rot(pl_ang2);
        pl_ang2[1] += ANG_180;
        ang3_approach(pl_ang2, bounce_ang, turn);
        enemy_task_set_rot(obj, bounce_ang);
        scl = 8.0f - bounce_rate * 7.0f;
        v3_set(&w->scl, scl, scl, scl);
        w->frame += frame_step;
        if (w->frames < w->frame) {
            w->next_step = 0;
            w->next = 2;
        }
        break;
      }
    }
}

/* 0x00b04578 stg02, 0x00afe8c0 stg22 */
#if defined(OVERLAY_STG22)
/* steer(cast) */
void en_hanuman_state_motion(void *obj, EnHanumanWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    float fps;

    switch (w->step) {
    case 0:
        if (w->dir_set)
            dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 108)));
        else
            dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 107)));
        fps = FPS;
        *(int *)&gfx->frame = 0; /* steer(type): stored as an int zero, as the original */
        gfx->speed = gfx->frames / (EN_HANUMAN_REC->frm_motion * fps);
        en_hanuman_spawn_shield(obj, w);
        w->frame = 1.0f;
        w->step++;
        w->next_step++;
        w->frames = (int)(EN_HANUMAN_REC->frm_motion * fps);
    case 1:
        dc_object_advance_time(gfx);
        w->frame += 1.0f;
        if (w->frames < w->frame)
            do {
                w->next_step = 0;
                w->next = 3;
            } while (0);
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}
#else
/* steer(cast) */
void en_hanuman_state_motion(void *obj, EnHanumanWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    float fps;

    switch (w->step) {
    case 0:
        if (w->dir_set)
            dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 27)));
        else
            dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 26)));
        fps = FPS;
        *(int *)&gfx->frame = 0;
        gfx->speed = gfx->frames / (EN_HANUMAN_REC->frm_motion * fps);
        en_hanuman_spawn_shield(obj, w);
        w->frame = 1.0f;
        w->step++;
        w->next_step++;
        w->frames = (int)(EN_HANUMAN_REC->frm_motion * fps);
    case 1:
        dc_object_advance_time(gfx);
        w->frame += 1.0f;
        if (w->frames < w->frame)
            do {
                w->next_step = 0;
                w->next = 3;
            } while (0);
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}
#endif

/* 0x00b04708 stg02, 0x00afea50 stg22 */
#if defined(OVERLAY_STG22)
/* steer(cast) */
void en_hanuman_state_drift(void *obj, EnHanumanWork *w)
{
    switch (w->step) {
    case 0:
        dc_object_set_clip(enemy_task_gfx_ptr(obj), *((struct DcClip **)pnb_get_entry(stage_models, 109)));
        ((ModelPart *)enemy_task_gfx_ptr(obj))->frame = 0.0f;
        w->frame = 1.0f;
        w->step++;
        w->next_step++;
        w->frames = (int)(EN_HANUMAN_REC->frm_drift * FPS);
    case 1:
        w->frame += 1.0f;
        if (w->frames < w->frame)
            do {
                w->next_step = 0;
                w->next = 4;
            } while (0);
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}
#else
/* steer(cast) */
void en_hanuman_state_drift(void *obj, EnHanumanWork *w)
{
    switch (w->step) {
    case 0:
        dc_object_set_clip(enemy_task_gfx_ptr(obj), *((struct DcClip **)pnb_get_entry(stage_models, 28)));
        ((ModelPart *)enemy_task_gfx_ptr(obj))->frame = 0.0f;
        w->frame = 1.0f;
        w->step++;
        w->next_step++;
        w->frames = (int)(EN_HANUMAN_REC->frm_drift * FPS);
    case 1:
        w->frame += 1.0f;
        if (w->frames < w->frame)
            do {
                w->next_step = 0;
                w->next = 4;
            } while (0);
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}
#endif

/* 0x00b04828 stg02, 0x00afeb70 stg22 */
void en_hanuman_state_anticipate(void *obj, EnHanumanWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    int dang[3];
    int cur[3];
    int nang[3];
    int aim[3];
    Vec3 v;
    int ang[3];
    float sc[3][2];
    float rate, env, scl, x, y, z, one;

    switch (w->step) {
    case 0:
        do {
            w->sel++;
            en_hanuman_update_target(obj, w);
        } while (0);
        dang[1] = (int)RAD_TO_ANG(atan2f(-w->to.x, -w->to.z));
        dang[0] = (int)RAD_TO_ANG(atan2f(w->to.y, sqrtf(w->to.x * w->to.x + w->to.z * w->to.z)));
        do { dang[2] = 0; } while (0);
        enemy_task_get_rot(obj, cur);
        i3_sub(dang, cur);
        dang[0] = dang[0] & 0x8000 ? dang[0] | ~0xffff : dang[0] & 0xffff;
        dang[1] = dang[1] & 0x8000 ? dang[1] | ~0xffff : dang[1] & 0xffff;
        dang[2] = dang[2] & 0x8000 ? dang[2] | ~0xffff : dang[2] & 0xffff;
        do { } while (0);
        if (dang[1] > 0)
            i3_set(w->atgt, 0, 0, (int)DEG_TO_ANG(EN_HANUMAN_REC->ang_rotate_z));
        else
            i3_set(w->atgt, 0, 0, -(int)DEG_TO_ANG(EN_HANUMAN_REC->ang_rotate_z));
        i3_set(w->aoff, 0, 0, 0);
        w->frame = 1.0f;
        w->frames = model_ivec3_length(dang) / (int)DEG_TO_ANG(EN_HANUMAN_REC->spd_ang_rotate * FRAME_SEC);
        w->step++;
        w->next_step++;
    case 1:
        rate = w->frame / w->frames;
        do {
            aim[1] = (int)RAD_TO_ANG(atan2f(-w->to.x, -w->to.z));
            aim[0] = (int)RAD_TO_ANG(atan2f(w->to.y, sqrtf(w->to.x * w->to.x + w->to.z * w->to.z)));
        } while (0);
        aim[2] = 0;
        one = 1.0f;
        enemy_task_get_rot(obj, nang);
        ang3_approach(aim, nang, one / (w->frames - w->frame + one));
        enemy_task_set_rot(obj, nang);
        i3_sub(nang, w->aoff);
        env = en_hanuman_ease_envelope(rate);
        w->frame += one;
        i3_mul(w->aoff, w->atgt, env);
        i3_add(nang, w->aoff);
        if (w->frames < w->frame) {
            w->frame = one;
            w->frames = (int)(EN_HANUMAN_REC->frm_anticipate * FPS);
            gfx->speed = 2.0f / w->frames;
            do { w->next_step = w->step + 1; } while (0);
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        else
            break;
    case 2:
      {
        float anticipate_rate, frame_step;
        anticipate_rate = w->frame / w->frames;
        frame_step = 1.0f;
        dc_object_advance_time(gfx); scl = anticipate_rate * 7.0f + frame_step; v3_set(&w->scl, scl, scl, scl); { double spd = EN_HANUMAN_REC->spd_anticipate * FRAME_SEC; z = spd * cos(anticipate_rate * 1.5707964f); } x = 0.0f; y = 0.0f;
        enemy_task_get_rot(obj, ang);
        ROT_Z(ang[2], sc[0], x, y);
        ROT_X(ang[0], sc[1], y, z);
        ROT_Y(ang[1], sc[2], x, z);
        v3_set(&v, x, y, z);
        v3_add(enemy_task_pos_ptr(obj), &v);
        w->frame += frame_step;
        if (w->frames < w->frame) {
            w->next_step = 0;
            if (w->sel >= w->cur)
                w->next = 5;
            else {
                do { w->next = 0; } while (0);
            }
        }
        break;
      }
    }
}

/* 0x00b04f18 stg02, 0x00aff260 stg22 */
void en_hanuman_state_escape(void *obj, EnHanumanWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 p;
    int ang[3];
    Vec3 eye;
    float clip;

    switch (w->step) {
    case 0:
        {
            float *pv = &w->to.x;

            vec3_scale_to_length(EN_HANUMAN_REC->spd_escape * FRAME_SEC, pv, pv);
        }
        gfx->speed = 0.5833333f;
        w->frame = 1.0f;
        w->step++;
        w->next_step++;
    case 1:
        if (w->frame < 12.0f)
            dc_object_advance_time(gfx);
        enemy_task_get_pos(obj, &p);
        v3_add(&p, &w->to);
        enemy_task_set_pos(obj, &p);
        do {
            ang[1] = (int)RAD_TO_ANG(atan2f(-w->to.x, -w->to.z));
            ang[0] = (int)RAD_TO_ANG(atan2f(w->to.y, sqrtf(w->to.x * w->to.x + w->to.z * w->to.z)));
        } while (0);
        ang[2] = 0;
        enemy_task_set_rot(obj, ang);
        clip = EN_HANUMAN_REC->dst_clip;
        camera_get_eye(&eye);
        if (vec3_distance_squared(&p, &eye) > clip * clip)
            enemy_task_set_state(obj, 2);
        w->frame += 1.0f;
        break;
    case 2:
    default:
        return;
    }
    if (w->phase != w->next || w->step != w->next_step)
        w->step++;
}

/* 0x00b05188 stg02, 0x00aff4d0 stg22 */
void en_hanuman_exec_normal(void *obj, EnHanumanWork *w)
{
    Vec3 p;

    do {
        w->phase = w->next;
        w->step = w->next_step;
    } while (0);
    if (enemy_task_get_phase(obj) == 1) {
        enemy_task_get_pos(obj, &p);
        v3_scale(&w->vel, 0.97f);
        v3_add(&p, &w->vel);
        enemy_task_set_pos(obj, &p);
        if (en_hanuman_states[w->phase])
            en_hanuman_states[w->phase](obj, w);
    }
}

/* 0x00b05280 stg02, 0x00aff5c8 stg22 */
void en_hanuman_exec_appear(void *obj, void *w)
{
    if (enemy_task_get_phase(obj) == 1)
        enemy_task_next_phase(obj);
}

/* 0x00b052c0 stg02, 0x00aff608 stg22 */
void en_hanuman_exec_leave(void *obj, void *w)
{
    if (enemy_task_get_phase(obj) == 1)
        enemy_task_next_phase(obj);
}

/* 0x00b05300 stg02, 0x00aff648 stg22 */
#if defined(OVERLAY_STG22)
void en_hanuman_exec_destroyed(void *obj, EnHanumanWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 1) {
        nj_fill8(&desc, 0, sizeof desc);
        desc.models = stage_models;
        desc.type = 110;
        desc.kind = 0;
        if (w->phase == 2) {
            desc.kind = 0x40000;
            if (w->dir_set)
                desc.motion = 112;
            else
                desc.motion = 111;
            desc.time = ((ModelPart *)enemy_task_gfx_ptr(obj))->frame;
        }
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        enemy_task_get_hit_imp(obj, &desc.v);
        enemy_shatter_create(obj, &desc, 0, 0);
        enemy_task_next_phase(obj);
    }
}
#else
void en_hanuman_exec_destroyed(void *obj, EnHanumanWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 1) {
        nj_fill8(&desc, 0, sizeof desc);
        desc.models = stage_models;
        desc.type = 29;
        desc.kind = 0;
        if (w->phase == 2) {
            desc.kind = 0x40000;
            if (w->dir_set)
                desc.motion = 31;
            else
                desc.motion = 30;
            desc.time = ((ModelPart *)enemy_task_gfx_ptr(obj))->frame;
        }
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        enemy_task_get_hit_imp(obj, &desc.v);
        enemy_shatter_create(obj, &desc, 0, 0);
        enemy_task_next_phase(obj);
    }
}
#endif

/* 0x00b053f0 stg02, 0x00aff738 stg22 */
void en_hanuman_exec(void *obj, void *w)
{
    void (**tbl)(void *obj, void *w) = en_hanuman_exec_table;
    unsigned int i = enemy_task_get_pstate(obj);

    if (tbl[i])
        tbl[i](obj, w);
}

/* 0x00b05450 stg02, 0x00aff798 stg22 */
void en_hanuman_hit_check(void *obj, void *work)
{
    EnHanumanWork *w = work;
    LockObj *lock = &w->lock;
    Vec3 p;

    if (lock->flags & LOCK_HIT && lock->life <= 0) {
        enemy_task_get_pos(obj, &p);
        enemy_status_score_popup(enemy_task_get_status_slot(obj), &p, EN_HANUMAN_REC->score, lockon_hits_max_shot_cnt(lock));
        lock->flags &= ~LOCK_LOCKABLE;
        enemy_task_set_state(obj, 3);
    }
}

/* 0x00b054f8 stg02, 0x00aff840 stg22 */
void en_hanuman_pre_exec(void *obj, void *w)
{
    en_hanuman_hit_check(obj, w);
}

/* 0x00b05500 stg02, 0x00aff848 stg22 */
void en_hanuman_post_exec(void *obj, void *w)
{
    en_hanuman_get_spawn_points(obj, w);
}

/* 0x00b05508 stg02, 0x00aff850 stg22 */
void en_hanuman_disp_body(void *obj, EnHanumanWork *w)
{
    Vec3 p;
    int ang[3];
    Vec3 scl;

    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    enemy_task_get_scl(obj, &scl);
    if (view_sphere_visible(&p, ((ModelPart *)enemy_task_gfx_ptr(obj))->radius)) {
        ulGxSetTextureList(stage_textures);
        do {
            ulMatrixPush();
        } while (0);
        vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
        en_hanuman_rotate_yxz(ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
        dc_object_draw(enemy_task_gfx_ptr(obj));
        ulMatrixPop();
    }
}

/* 0x00b055e0 stg02, 0x00aff928 stg22 */
void en_hanuman_disp(void *obj, void *w)
{
    void (**t)(void *obj, void *w) = en_hanuman_disp_table;
    unsigned int i = enemy_task_get_pstate(obj);

    if (t[i])
        t[i](obj, w);
}

/* 0x00b05640 stg02, 0x00aff988 stg22 */
void en_hanuman_rotate_by_player(Vec3 *v)
{
    int ang[4];
    float sc[3][2];
    float x, y, z;

    player_get_frame_ang(ang);
    do {
        x = v->x;
    } while (0);
    y = v->y;
    z = v->z;
    ROT_Z(ang[2], sc[0], x, y);
    do {
        do {
            ROT_X(ang[0], sc[1], y, z);
        } while (0);
    } while (0);
    ROT_Y(ang[1], sc[2], x, z);
    do {
        v->x = x;
    } while (0);
    v->z = z;
    v->y = y;
}

/* 0x00b05730 stg02, 0x00affa78 stg22 */
void en_hanuman_init(void *obj, EnHanumanWork *w, EnHanumanArg *prm)
{
    int dang[3];
    int pl_ang[4];
    Vec3 v;
    BackFireLink lnk;
    float x0, y0, z0, x1, y1, z1;
    LockObj *lock;

    w->pos = enemy_task_pos_ptr(obj);
    w->param_vec = enemy_task_param_vec_ptr(obj);
    w->ang = enemy_task_rot_ptr(obj);
    w->scale = enemy_task_scl_ptr(obj);
    w->dir_set = !v3_is_zero(&prm->dir);
    do {
        w->next = 0;
    } while (0);
    do {
        w->next_step = 0;
    } while (0);
    enemy_task_clear_flags(obj, 64);
    lock = &w->lock;
    lockon_obj_init(lock, obj, &w->vec, 0, en_hanuman_lock_name, enemy_task_get_status_slot(obj), EN_HANUMAN_REC->hp, EN_HANUMAN_REC->hp, 0);
    lockon_entry(lock);
    player_get_frame_ang(pl_ang);
    i3_diff(dang, prm->mark, pl_ang);
    v.x = dang[0] * DEG_PER_ANG;
    v.y = dang[1] * DEG_PER_ANG;
    v.z = dang[2] * DEG_PER_ANG;
    en_hanuman_rotate_by_player(&v);
    enemy_task_set_param_vec(obj, &v);
    x0 = enemy_task_get_param(obj, 0);
    y0 = enemy_task_get_param(obj, 1);
    z0 = enemy_task_get_param(obj, 2);
    x1 = enemy_task_get_param(obj, 3);
    y1 = enemy_task_get_param(obj, 4);
    z1 = enemy_task_get_param(obj, 5);
    w->cur = 1;
    if (!(fabsf(x0) < FLT_EPSILON && fabsf(y0) < FLT_EPSILON && fabsf(z0) < FLT_EPSILON)) {
        w->cur++;
        v3_set(&w->tgt[0], x0, y0, z0);
        en_hanuman_rotate_by_player(&w->tgt[0]);
        if (!(fabsf(x1) < FLT_EPSILON && fabsf(y1) < FLT_EPSILON && fabsf(z1) < FLT_EPSILON)) {
            w->cur++;
            v3_set(&w->tgt[1], x1, y1, z1);
            en_hanuman_rotate_by_player(&w->tgt[1]);
        }
    }
    nj_fill8(&lnk, 0, sizeof(lnk));
    lnk.gfx = 0;
    lnk.pos = &w->bf_pos;
    lnk.ang = enemy_task_rot_ptr(obj);
    lnk.obj_scale = &w->scl;
    en_backfire_create(obj, &lnk, 1);
    v3_set(&w->scl, 1.0f, 1.0f, 1.0f);
}

/* 0x00b05aa0 stg02, 0x00affde8 stg22 */
void en_hanuman_release(void *obj, char *w)
{
    lockon_remove((LockObj *)(w + 0xac));
}

/* 0x00b05aa8 stg02, 0x00affdf0 stg22 */
void en_hanuman_create(void *parent, EnHanumanArg *prm)
{
    ObjDesc desc;
    ObjFuncs funcs;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&funcs, 0, sizeof funcs);
    funcs.fn[0] = en_hanuman_init;
    funcs.fn[1] = en_hanuman_release;
    funcs.fn[2] = en_hanuman_exec;
    funcs.fn[3] = en_hanuman_disp;
    funcs.fn[4] = en_hanuman_pre_exec;
    funcs.fn[5] = en_hanuman_post_exec;
    desc.name = en_hanuman_name;
    desc.funcs = &funcs;
    desc.arg = prm;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = EN_HANUMAN_MODEL(25);
    desc.model_no2 = v3_is_zero(&prm->dir) ? EN_HANUMAN_MODEL(26) : EN_HANUMAN_MODEL(27);
    desc.hp = EN_HANUMAN_REC->hp;
    desc.score = EN_HANUMAN_REC->score;
    desc.size = 276;
    desc.time = 0.0f;
    desc.wob_amp = 0;
    desc.wob_spd = 0;
    enemy_task_task_create(parent, &desc);
}

/* 0x00b05c28 stg02, 0x00afff70 stg22 */
void en_hanuman_models_init(void)
{
}

/* 0x00b05c30 stg02, 0x00afff78 stg22 */
void en_hanuman_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
