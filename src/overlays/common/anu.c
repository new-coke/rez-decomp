/*
 * Anu: a lockable enemy that flies in on its exhaust beams, brakes, fires an E_Shot volley from
 * its tip with an attack-ray flash, then speeds away; a second state set repeats the run.
 */

#include "types_common.h"
#include "task.h"

#include "types_overlays.h"

#include "debug.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "pnb.h"
#include "ps_gs.h"
#include "rez_wire_compile.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "attack_ray.h"
#include "en_backfire.h"
#include "enemy_shatter.h"
#include "enemy_vars.h"
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
        int a_ = ang; \
        float rot_u = x, rot_v = y; \
        ulSinCos(a_, &sc[0], &sc[1]); \
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
void *e_shot_fire(void *parent, Vec3 *pos, Vec3 *dir, int level);

extern float *matrix_from_axis_angle(float *m, Vec3 *a, int angle);
extern int anu_init_arg;
extern void (*anu_exec_table[])(void *o, AnuWork *w);
extern void (*anu_disp_table[])(void *o, AnuWork *w);
void anu_vars_register(void);
void anu_vars_unregister(void);
void anu_animate_shot(void *obj, AnuWork *w);
void anu_update(void *obj, AnuWork *w);
int anu_vars_init(int *prm);
int anu_vars_release(void);
int anu_node_cb(unsigned int *node, int idx, AnuWork *w);
void anu_pose_model(void *obj, AnuWork *w);
int anu_next_state(void *obj, AnuWork *w, int n, int state);
void anu_state_move(void *obj, AnuWork *w);
void anu_state_break(void *obj, AnuWork *w);
void anu_state_shot(void *obj, AnuWork *w);
void anu_state_accel(void *obj, AnuWork *w);
void anu_state_finish(void *obj, AnuWork *w);
void anu_state_cruise(void *obj, AnuWork *w);
void anu_state_move2(void *obj, AnuWork *w);
void anu_state_break2(void *obj, AnuWork *w);
void anu_state_shot2(void *obj, AnuWork *w);
void anu_state_accel2(void *obj, AnuWork *w);
void anu_state_finish2(void *obj, AnuWork *w);
void anu_exec_normal(void *obj, AnuWork *w);
void anu_exec_leave(void *obj, AnuWork *w);
void anu_exec(void *obj, AnuWork *w);
void anu_disp(void *obj, AnuWork *w);
void anu_rotate_yxz(int *ang);
extern AnuVars anu_types[1];
extern VarDef anu_vardef;
extern VarDef anu_vardefs[69];
extern void (*anu_states[])(void *o, AnuWork *w);
extern ObjFuncs anu_funcs;
extern char anu_name[];

extern char anu_lock_name[];

/*
 * Per overlay: model list entries (enemy model and its second entry, body model and motion,
 * break pieces), hit radius and the most E_Shot bullets per volley.
 */
#if defined(OVERLAY_STG02)
#define ANU_MODEL 70
#define ANU_MODEL2 68
#define ANU_BODY_MODEL 71
#define ANU_BODY_MOTION 69
#define ANU_BREAK_MODEL 67
#define ANU_RADIUS 30.0f
#define ANU_SHOTS_MAX 11
#elif defined(OVERLAY_STG03)
#define ANU_MODEL 64
#define ANU_MODEL2 65
#define ANU_BODY_MODEL 66
#define ANU_BODY_MOTION 67
#define ANU_BREAK_MODEL 68
#define ANU_RADIUS 40.0f
#define ANU_SHOTS_MAX 15
#elif defined(OVERLAY_STG22)
#define ANU_MODEL 55
#define ANU_MODEL2 56
#define ANU_BODY_MODEL 57
#define ANU_BODY_MOTION 58
#define ANU_BREAK_MODEL 59
#define ANU_RADIUS 40.0f
#define ANU_SHOTS_MAX 7
#else
#define ANU_MODEL 68
#define ANU_MODEL2 66
#define ANU_BODY_MODEL 69
#define ANU_BODY_MOTION 67
#define ANU_BREAK_MODEL 65
#define ANU_RADIUS 40.0f
#define ANU_SHOTS_MAX 7
#endif

/* 0x00af9168 stg02, 0x00af8808 stg03, 0x00af8c70 stg04, 0x00af7450 stg22 */
void anu_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *vd;
    int *fld;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&anu_vardef);
    for (i = 0; i < 1; i++) {
        vd = anu_vardefs;
        fld = (int *)&anu_types[i].p;
        anu_vardef.name = anu_types[i].name;
        anu_vardef.fmt = anu_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&anu_vardef);
        for (j = 0; j < 69; j++) {
            vd->ptr = fld;
            fld++;
            vd++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)anu_vardefs);
    }
}

/* 0x00af9280 stg02, 0x00af8920 stg03, 0x00af8d88 stg04, 0x00af7568 stg22 */
void anu_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00af9288 stg02, 0x00af8928 stg03, 0x00af8d90 stg04, 0x00af7570 stg22 */
int anu_get_init_arg(void)
{
    return anu_init_arg;
}

/* 0x00af9298 stg02, 0x00af8938 stg03, 0x00af8da0 stg04, 0x00af7580 stg22 */
int anu_vars_init(int *prm)
{
    anu_init_arg = *prm;
    anu_vars_register();
    return 0;
}

/* 0x00af92c0 stg02, 0x00af8960 stg03, 0x00af8dc8 stg04, 0x00af75a8 stg22 */
int anu_vars_release(void)
{
    anu_vars_unregister();
    return 0;
}

/* 0x00af92e0 stg02, 0x00af8980 stg03, 0x00af8de8 stg04, 0x00af75c8 stg22 */
int anu_marker_draw(int obj, void *w)
{
    int *ent;

    ulGxSetTextureList(stage_textures);
    ent = pnb_get_entry(stage_models, ANU_MODEL);
    return obj_draw_at(obj, w, (void *)*ent);
}

/* 0x00af9338 stg02, 0x00af89d8 stg03, 0x00af8e40 stg04, 0x00af7620 stg22 */
int anu_node_cb(unsigned int *node, int idx, AnuWork *w)
{
    if (*node & 0x1000000)
        ulMatrixGetTranslation((float *)ulMatrixCur, &w->tip.x);
    if (*node & 0x4000000)
        ulMatrixGetTranslation((float *)ulMatrixCur, &w->pos.x);
    return 0;
}

/* 0x00af93a8 stg02, 0x00af8a48 stg03, 0x00af8eb0 stg04, 0x00af7690 stg22 */
void anu_pose_model(void *obj, AnuWork *w)
{
    Vec3 pos;
    int ang[4];
    void *gfx = enemy_task_gfx_ptr(obj);

    enemy_task_get_pos(obj, &pos);
    enemy_task_get_rot(obj, ang);
    dc_object_set_callback(gfx, anu_node_cb, w);
    do {
        ulMatrixPush();
    } while (0);
    matrix_unit_set(0);
    vu0_matrix_translate(ulMatrixCur, pos.x, pos.y, pos.z);
    anu_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(gfx);
    ulMatrixPop();
    dc_object_set_callback(gfx, 0, 0);
}

/* 0x00af9478 stg02, 0x00af8b18 stg03, 0x00af8f80 stg04, 0x00af7760 stg22 */
/* steer(cast) */
void anu_init(void *obj, AnuWork *w, int rec, int *kind)
{
    Vec3 v;
    int ang[3];
    BackFireLink lnk;
    float sc[2][2];
    float scu[3][2];
    AnuVars *typ;
    float bdis, frm, x, y, z;
    float ux, uy, uz;
    int base;

    bdis = enemy_task_get_param(obj, 4);
    typ = &anu_types[*kind];
    w->type = typ;
    lockon_obj_init(&w->lock, obj, &w->pos, 0, anu_lock_name, enemy_task_get_status_slot(obj), typ->p.hp,
                  typ->p.same_lock, 0);
    lockon_entry(&w->lock);
    w->lock.flags &= ~LOCK_LOCKABLE;
    enemy_task_clear_flags(obj, 64);
    dc_object_init(&w->body, *((int *(*)(void *, unsigned int))pnb_get_entry)(*(void **)obj, ANU_BODY_MODEL), *((int *(*)(void *, unsigned int))pnb_get_entry)(*(void **)obj, ANU_BODY_MOTION), 0x500);
    dc_object_set_wire(&w->body, model_pnb_get_wire(*(void **)obj, ANU_BODY_MODEL));
    ((ModelPart *)enemy_task_gfx_ptr(obj))->radius = w->body.radius = ANU_RADIUS;
    enemy_task_get_param_vec(obj, &v);
    enemy_task_get_rot(obj, ang);
    if (v3_is_zero(&v)) {
        x = 0.0f;
        y = 0.0f;
        z = -1.0f;
        ROT_X(ang[0], sc[0], y, z);
        ROT_Y(ang[1], sc[1], x, z);
        w->speed = 0.0f;
        v3_set(&v, x, y, z);
    } else {
        w->speed = vec3_length(&v);
        vec3_normalize(&v, &v);
        do {
            ang[1] = (int)RAD_TO_ANG(atan2f(-v.x, -v.z));
            ang[0] = (int)RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
        } while (0);
    }
    enemy_task_set_param_vec(obj, &v);
    enemy_task_set_rot(obj, ang);
    i3_copy(w->ang, ang);
    w->roll = 0;
    frm = enemy_task_get_param(obj, 1);
    w->fire = 0;
    w->shot = (int)frm > 0;
    if (bdis > 0.0f) {
        w->state = 0;
        w->end = enemy_task_get_param(obj, 3) * 60.0f / 60.0f <= 1.0f ? 1.0f
                 : enemy_task_get_param(obj, 3) * 60.0f / 60.0f;
        base = w->end + enemy_task_get_param(obj, 4) / typ->p.break_spd * FPS;
        frm = typ->p.shot_motion_sec_dif * FPS;
        w->end = (int)(base + frm);
        w->start = (int)(w->end - typ->p.shot_motion_sec * FPS);
    } else if (!w->shot) {
        w->state = 4;
    } else {
        w->state = 6;
        base = enemy_task_get_param(obj, 3) * 60.0f / 60.0f;
        frm = (typ->p.break2_sec + typ->p.shot2_sec) * FPS;
        w->end = (int)(base + frm);
        w->start = (int)(w->end - typ->p.shot_motion_sec * FPS);
    }
    w->frames = 1;
    enemy_task_set_appear(obj, 2, 30);
    enemy_task_set_vanish(obj, 2, 60);
    ux = 0.0f;
    uy = 1.0f;
    uz = 0.0f;
    ROT_Z(ang[2], scu[0], ux, uy);
    ROT_X(ang[0], scu[1], uy, uz);
    ROT_Y(ang[1], scu[2], ux, uz);
    v3_set(&w->up, ux, uy, uz);
    v3_set(&w->sway, 0.0f, 0.0f, 0.0f);
    heap_bzero(&lnk, sizeof(lnk));
    lnk.gfx = enemy_task_gfx_ptr(obj);
    lnk.pos = enemy_task_pos_ptr(obj);
    lnk.ang = enemy_task_rot_ptr(obj);
    lnk.obj_scale = enemy_task_scl_ptr(obj);
    lnk.scale = &w->backfire_scl;
    lnk.len = typ->p.backfire_len;
    lnk.width = typ->p.backfire_width;
    lnk.len_scale = &w->backfire2_scl;
    lnk.width_scale = &w->backfire2_scl;
    lnk.alpha = &w->alpha;
    lnk.flags = 0x2000000;
    lnk.use_pose = 0;
    en_backfire_beam_create(obj, &lnk);
    w->backfire_scl = typ->p.backfire_scl;
    w->backfire2_scl = typ->p.backfire2_scl;
}

/* 0x00af9a98 stg02, 0x00af9138 stg03, 0x00af95a0 stg04, 0x00af7d80 stg22 */
void anu_release(void *obj, AnuWork *w)
{
    dc_object_release(&w->body);
    lockon_remove(&w->lock);
}

/* 0x00af9ac8 stg02, 0x00af9168 stg03, 0x00af95d0 stg04, 0x00af7db0 stg22 */
int anu_next_state(void *obj, AnuWork *w, int n, int state)
{
    if (++w->frames > n) {
        w->state = state;
        w->frames = 1;
        return 1;
    }
    return 0;
}

/* 0x00af9af8 stg02, 0x00af9198 stg03, 0x00af9600 stg04, 0x00af7de0 stg22 */
void anu_animate_shot(void *obj, AnuWork *w)
{
    float rate;
    float frm;
    ModelPart *gfx;
    AttackRayArg ray;

    rate = (enemy_task_get_time(obj) - w->start) / (w->end - w->start);
    gfx = enemy_task_gfx_ptr(obj);
    frm = gfx->frames;
    if (rate < 1.0f)
        frm *= rate;
    if (w->shot && !(frm < 0.0f)) {
        gfx->frame = frm;
        if (frm < 0.0f)
            return;
        switch (w->shot) {
        case 1:
            ray.pos = &w->tip;
            ray.ang = enemy_task_rot_ptr(obj);
            ray.scale = &w->scale;
            ray.dur = (w->end - w->start) * 29.0f / gfx->frames;
            attack_ray_create(obj, &ray);
            w->shot = 2;
            break;
        case 2:
            if (!(frm < 29.0f)) {
                w->shot = 3;
                w->fire = 1;
            }
            break;
        case 3:
            w->fire = 0;
            break;
        }
    }
}

/* 0x00af9c88 stg02, 0x00af9328 stg03, 0x00af9790 stg04, 0x00af7f70 stg22 */
void anu_state_move(void *obj, AnuWork *w)
{
    int n;
    Vec3 pos;
    Vec3 v;

    n = enemy_task_get_param(obj, 3) * 60.0f / 60.0f <= 1.0f ? 1.0f : enemy_task_get_param(obj, 3) * 60.0f / 60.0f;
    enemy_task_get_pos(obj, &pos);
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, w->speed);
    v3_add(&pos, &v);
    enemy_task_set_pos(obj, &pos);
    w->roll = w->roll - (int)DEG_TO_ANG(enemy_task_get_param(obj, 2)) / FPS;
    anu_animate_shot(obj, w);
    anu_next_state(obj, w, n, 1);
}

/* 0x00af9e08 stg02, 0x00af94a8 stg03, 0x00af9910 stg04, 0x00af80f0 stg22 */
void anu_state_break(void *obj, AnuWork *w)
{
    AnuVars *typ = w->type;
    Vec3 pos, v, ofs;
    int ang[3];
    Vec3 dir, ax;
    float mtx[16];
    int nang[4];
    float sc[5][2];
    float fps, unit, half_turn, quarter, half;
    float dur, rate, prev, rem, dis, amp, crv, x, y, z;
    int n;

    fps = FPS;
    dur = fabsf(enemy_task_get_param(obj, 4)) / typ->p.break_spd;
    unit = 1.0f;
    half_turn = ANG_HALF;
    quarter = ANG_QUARTER;
    half = 0.5f;
    x = 0.0f;
    y = 0.0f;
    n = (int)(dur * fps);
    rate = w->frames / (float)n;
    prev = (w->frames - 1) / (float)n;
    enemy_task_get_pos(obj, &pos);
    v3_sub(&pos, &pos, &w->sway);
    rem = unit - rate;
    dis = -(w->speed * fps) / (typ->p.break_pow + unit) * dur * (safe_powf(rem, typ->p.break_pow + unit) - unit);
    dis -= -(w->speed * fps) / (typ->p.break_pow + unit) * dur
         * (safe_powf(unit - prev, typ->p.break_pow + unit) - unit);
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, dis);
    v3_add(&pos, &v);
    amp = enemy_task_get_param(obj, 4) * (typ->p.break_r_over_rate + unit);
    ulSinCos((int)(safe_powf(rate, typ->p.break_r_pow) * half_turn - quarter), &sc[0][0], &sc[0][1]);
    crv = safe_powf((sc[0][0] + unit) * half, typ->p.break_r_pow2);
    ulSinCos((int)(safe_powf(prev, typ->p.break_r_pow) * half_turn - quarter), &sc[0][0], &sc[0][1]);
    crv -= safe_powf((sc[0][0] + unit) * half, typ->p.break_r_pow2);
    amp *= crv;
    v3_mul(&ofs, &w->up, amp);
    v3_add(&pos, &ofs);
    ulSinCos((int)(safe_powf(rate, typ->p.break_dis_pow) * ANG_TURN - quarter), &sc[1][0], &sc[1][1]);
    z = -safe_powf((sc[1][0] + unit) * half, typ->p.break_dis_pow2) * typ->p.break_dis;
    enemy_task_get_rot(obj, ang);
    ROT_X(ang[0], sc[2], y, z);
    ROT_Y(ang[1], sc[3], x, z);
    v3_set(&w->sway, x, y, z);
    v3_add(&pos, &w->sway);
    enemy_task_set_pos(obj, &pos);
    enemy_task_get_param_vec(obj, &dir);
    ulVec3Cross(&ax, &dir, &w->up);
    ulSinCos((int)(safe_powf(rate, typ->p.break_ang_pow) * half_turn - quarter), &sc[4][0], &sc[4][1]);
    crv = safe_powf((sc[4][0] + unit) * half, typ->p.break_ang_pow2);
    matrix_from_axis_angle(mtx, &ax, (int)((typ->p.break_ang_over_rate + unit) * half_turn * crv));
    ulMatrixPush();
    ulMatrixLoad(mtx);
    anu_rotate_yxz(w->ang);
    model_camera_angles(nang);
    ulMatrixPop();
    enemy_task_set_rot(obj, nang);
    w->roll = w->roll - (int)DEG_TO_ANG(enemy_task_get_param(obj, 2)) / fps * (unit - rate);
    if (w->shot) {
        w->backfire_scl = safe_powf((unit - rate), typ->p.break_backfire_pow) * typ->p.backfire_scl;
        w->backfire2_scl = safe_powf((unit - rate), typ->p.break_backfire2_pow) * typ->p.backfire2_scl;
    } else {
        w->backfire_scl = safe_powf((unit - rate), typ->p.break_backfire_pow) * typ->p.backfire_scl + safe_powf(rate, typ->p.break_backfire_pow2) * typ->p.break_backfire_scl;
        w->backfire2_scl = safe_powf((unit - rate), typ->p.break_backfire2_pow) * typ->p.backfire2_scl + safe_powf(rate, typ->p.break_backfire2_pow2) * typ->p.break_backfire2_scl;
    }
    anu_animate_shot(obj, w);
    if (w->shot)
        anu_next_state(obj, w, n, 5);
    else
        anu_next_state(obj, w, n, 2);
}

/* 0x00afa458 stg02, 0x00af9af8 stg03, 0x00af9f60 stg04, 0x00af8740 stg22 */
void anu_state_shot(void *obj, AnuWork *w)
{
    int n = w->type->p.shot_motion_sec_dif * FPS;
    ModelPart *gfx;

    anu_animate_shot(obj, w);
    if (anu_next_state(obj, w, n, 2)) {
        gfx = enemy_task_gfx_ptr(obj);
        gfx->frame = gfx->frames;
    }
}

/* 0x00afa4d8 stg02, 0x00af9b78 stg03, 0x00af9fe0 stg04, 0x00af87c0 stg22 */
void anu_state_accel(void *obj, AnuWork *w)
{
    AnuVars *typ = w->type;
    Vec3 pos, v, ofs;
    Vec3 dir, ax;
    float mtx[16];
    int nang[4];
    float sc[2][2];
    float fps, unit, full, quarter, half, half_turn;
    float rate, prev, rem, dis, amp, crv, acrv;
    int n;

    fps = FPS;
    unit = 1.0f;
    full = ANG_TURN;
    quarter = ANG_QUARTER;
    half = 0.5f;
    half_turn = ANG_HALF;
    n = typ->p.accel_sec * fps;
    rate = w->frames / (float)n;
    prev = (w->frames - 1) / (float)n;
    enemy_task_get_pos(obj, &pos);
    rem = unit - rate;
    dis = w->speed * fps / (typ->p.accel_pow + unit) * typ->p.accel_sec * (typ->p.accel_spd_over_rate + unit)
        * safe_powf(rate, typ->p.accel_pow + unit);
    dis -= w->speed * fps / (typ->p.accel_pow + unit) * typ->p.accel_sec * (typ->p.accel_spd_over_rate + unit)
         * safe_powf(prev, typ->p.accel_pow + unit);
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, -dis);
    v3_add(&pos, &v);
    amp = enemy_task_get_param(obj, 4) * typ->p.break_r_over_rate;
    ulSinCos((int)(safe_powf(rate, typ->p.accel_r_pow) * full - quarter), &sc[0][0], &sc[0][1]);
    crv = safe_powf((sc[0][0] + unit) * half, typ->p.accel_r_pow2) * rem;
    ulSinCos((int)(safe_powf(prev, typ->p.accel_r_pow) * full - quarter), &sc[0][0], &sc[0][1]);
    crv -= safe_powf((sc[0][0] + unit) * half, typ->p.accel_r_pow2) * rem;
    v3_mul(&ofs, &w->up, -amp * crv);
    v3_add(&pos, &ofs);
    enemy_task_set_pos(obj, &pos);
    enemy_task_get_param_vec(obj, &dir);
    ulVec3Cross(&ax, &dir, &w->up);
    ulSinCos((int)(safe_powf(rate, typ->p.accel_ang_pow) * half_turn - quarter), &sc[1][0], &sc[1][1]);
    acrv = safe_powf((sc[1][0] + unit) * half, typ->p.accel_ang_pow2);
    ulSinCos((int)(safe_powf(rate, typ->p.accel_ang_pow3) * full - quarter), &sc[1][0], &sc[1][1]);
    acrv += safe_powf((sc[1][0] + unit) * half, typ->p.accel_ang_pow4) * typ->p.accel_ang_rate;
    acrv = unit - acrv;
    matrix_from_axis_angle(mtx, &ax, (int)((typ->p.break_ang_over_rate * acrv + unit) * half_turn));
    ulMatrixPush();
    ulMatrixLoad(mtx);
    anu_rotate_yxz(w->ang);
    model_camera_angles(nang);
    ulMatrixPop();
    enemy_task_set_rot(obj, nang);
    w->roll = w->roll - (int)DEG_TO_ANG(enemy_task_get_param(obj, 2)) / fps * rate;
    if (!w->shot) {
        w->backfire_scl = (typ->p.backfire_scl - typ->p.break_backfire_scl) * safe_powf(rate, typ->p.accel_backfire_pow) + typ->p.break_backfire_scl;
        w->backfire2_scl = (typ->p.backfire2_scl - typ->p.break_backfire2_scl) * safe_powf(rate, typ->p.accel_backfire_pow2) + typ->p.break_backfire2_scl;
    } else {
        w->backfire_scl = typ->p.backfire_scl * safe_powf(rate, typ->p.accel_backfire2_pow);
        w->backfire2_scl = typ->p.backfire2_scl * safe_powf(rate, typ->p.accel_backfire2_pow2);
    }
    anu_next_state(obj, w, n, 3);
}

/* 0x00afa9f0 stg02, 0x00afa090 stg03, 0x00afa4f8 stg04, 0x00af8cd8 stg22 */
void anu_state_finish(void *obj, AnuWork *w)
{
    AnuVars *typ = w->type;
    Vec3 pos, v;
    float fps, sspd, espd, dis;
    int n;

    fps = FPS;
    sspd = w->speed * fps;
    n = typ->p.finish_sec * fps;
    espd = typ->p.accel_spd_over_rate * sspd + sspd;
    dis = ((sspd - espd) * (w->frames / (float)n) * 0.5f + espd) * (w->frames / fps)
        - ((sspd - espd) * ((w->frames - 1) / (float)n) * 0.5f + espd) * ((w->frames - 1) / fps);
    enemy_task_get_pos(obj, &pos);
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, -dis);
    v3_add(&pos, &v);
    enemy_task_set_pos(obj, &pos);
    w->roll = w->roll - (int)DEG_TO_ANG(enemy_task_get_param(obj, 2)) / fps;
    if (anu_next_state(obj, w, n, 4))
        w->speed = -w->speed;
}

/* 0x00afabc0 stg02, 0x00afa260 stg03, 0x00afa6c8 stg04, 0x00af8ea8 stg22 */
void anu_state_cruise(void *obj, AnuWork *w)
{
    Vec3 pos, v;

    enemy_task_get_pos(obj, &pos);
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, w->speed);
    v3_add(&pos, &v);
    enemy_task_set_pos(obj, &pos);
    w->roll = w->roll - (int)DEG_TO_ANG(enemy_task_get_param(obj, 2)) / FPS;
}

/* 0x00afacb0 stg02, 0x00afa350 stg03, 0x00afa7b8 stg04, 0x00af8f98 stg22 */
void anu_state_move2(void *obj, AnuWork *w)
{
    Vec3 pos, v;
    float fps, frm;

    enemy_task_get_pos(obj, &pos);
    fps = FPS;
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, w->speed);
    v3_add(&pos, &v);
    enemy_task_set_pos(obj, &pos);
    w->roll = w->roll - (int)DEG_TO_ANG(enemy_task_get_param(obj, 2)) / fps;
    anu_animate_shot(obj, w);
    frm = enemy_task_get_time(obj);
    if (frm > (int)(enemy_task_get_param(obj, 3) * fps / fps)) {
        w->state = 7;
        w->frames = 1;
    }
}

/* 0x00afae10 stg02, 0x00afa4b0 stg03, 0x00afa918 stg04, 0x00af90f8 stg22 */
void anu_state_break2(void *obj, AnuWork *w)
{
    AnuVars *typ = w->type;
    Vec3 pos, v;
    float unit, rate, prev, dis;
    int n;

    unit = 1.0f;
    n = typ->p.break2_sec * FPS;
    rate = w->frames / (float)n;
    prev = (w->frames - 1) / (float)n;
    enemy_task_get_pos(obj, &pos);
    rate = unit - rate;
    dis = -(w->speed * FPS) / (typ->p.break2_pow + unit) * typ->p.break2_sec
        * (safe_powf(rate, typ->p.break2_pow + unit) - unit);
    dis -= -(w->speed * FPS) / (typ->p.break2_pow + unit) * typ->p.break2_sec
         * (safe_powf(unit - prev, typ->p.break2_pow + unit) - unit);
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, dis);
    v3_add(&pos, &v);
    enemy_task_set_pos(obj, &pos);
    w->roll = w->roll - (int)DEG_TO_ANG(enemy_task_get_param(obj, 2)) / FPS * rate;
    w->backfire_scl = safe_powf(rate, typ->p.break2_backfire_pow) * typ->p.backfire_scl;
    w->backfire2_scl = safe_powf(rate, typ->p.break2_backfire2_pow) * typ->p.backfire2_scl;
    anu_animate_shot(obj, w);
    anu_next_state(obj, w, n, 8);
}

/* 0x00afb060 stg02, 0x00afa700 stg03, 0x00afab68 stg04, 0x00af9348 stg22 */
void anu_state_shot2(void *obj, AnuWork *w)
{
    int n = w->type->p.shot2_sec * FPS;
    ModelPart *gfx;

    anu_animate_shot(obj, w);
    if (anu_next_state(obj, w, n, 9)) {
        gfx = enemy_task_gfx_ptr(obj);
        gfx->frame = gfx->frames;
    }
}

/* 0x00afb0e0 stg02, 0x00afa780 stg03, 0x00afabe8 stg04, 0x00af93c8 stg22 */
void anu_state_accel2(void *obj, AnuWork *w)
{
    AnuVars *typ = w->type;
    Vec3 pos, v;
    float rate, prev, dis;
    int n;

    n = typ->p.accel2_sec * FPS;
    rate = w->frames / (float)n;
    prev = (w->frames - 1) / (float)n;
    enemy_task_get_pos(obj, &pos);
    dis = w->speed * FPS / (typ->p.accel2_pow + 1.0f) * typ->p.accel2_sec * (typ->p.accel2_spd_over_rate + 1.0f)
        * safe_powf(rate, typ->p.accel2_pow + 1.0f);
    dis -= w->speed * FPS / (typ->p.accel2_pow + 1.0f) * typ->p.accel2_sec * (typ->p.accel2_spd_over_rate + 1.0f)
         * safe_powf(prev, typ->p.accel2_pow + 1.0f);
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, dis);
    v3_add(&pos, &v);
    enemy_task_set_pos(obj, &pos);
    w->roll = w->roll - (int)DEG_TO_ANG(enemy_task_get_param(obj, 2)) * (typ->p.accel2_roll_spd_rate + 1.0f) / FPS
                * safe_powf(rate, typ->p.accel2_roll_pow);
    w->backfire_scl = typ->p.accel2_backfire_scl * safe_powf(rate, typ->p.accel2_backfire_pow);
    w->backfire2_scl = typ->p.accel2_backfire2_scl * safe_powf(rate, typ->p.accel2_backfire2_pow);
    anu_next_state(obj, w, n, 10);
}

/* 0x00afb340 stg02, 0x00afa9e0 stg03, 0x00afae48 stg04, 0x00af9628 stg22 */
void anu_state_finish2(void *obj, AnuWork *w)
{
    AnuVars *typ = w->type;
    Vec3 pos, v;
    float unit, sspd, espd, dis, rate;
    int n;

    unit = 1.0f;
    n = typ->p.finish2_sec * FPS;
    espd = w->speed * FPS * (typ->p.accel2_spd_over_rate + 1.0f);
    sspd = w->speed * FPS;
    rate = w->frames / (float)n;
    dis = ((sspd - espd) * rate * 0.5f + espd) * (w->frames / FPS)
        - ((sspd - espd) * ((w->frames - 1) / (float)n) * 0.5f + espd) * ((w->frames - 1) / FPS);
    enemy_task_get_pos(obj, &pos);
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, dis);
    v3_add(&pos, &v);
    enemy_task_set_pos(obj, &pos);
    w->roll = w->roll - (int)DEG_TO_ANG(enemy_task_get_param(obj, 2))
                * (typ->p.accel2_roll_spd_rate * (unit - rate) + unit) / FPS;
    anu_next_state(obj, w, n, 4);
}

/* 0x00afb530 stg02, 0x00afabd0 stg03, 0x00afb038 stg04, 0x00af9818 stg22 */
/* steer(cast) */
void anu_update(void *obj, AnuWork *w)
{
    int ang[3];
    Vec3 dir;
    float sc[2][2];
    float x, y, z;
    int n;

    if (anu_states[w->state])
        anu_states[w->state](obj, w);
    anu_pose_model(obj, w);
    if (w->fire) {
        n = ((float (*)(float))float_frac)(enemy_task_get_param(obj, 1)) * 1000.0f + 4.0f + 0.5f;
        enemy_task_get_rot(obj, ang);
        x = 0.0f;
        y = 0.0f;
        z = -1.0f;
        ROT_X(ang[0], sc[0], y, z);
        ROT_Y(ang[1], sc[1], x, z);
        v3_set(&dir, x, y, z);
        n = n < 0 ? 0 : n;
        e_shot_fire(obj, &w->tip, &dir, n > ANU_SHOTS_MAX ? ANU_SHOTS_MAX : n);
        w->fire = 0;
    }
}

/* 0x00afb6a8 stg02, 0x00afad48 stg03, 0x00afb1b0 stg04, 0x00af9990 stg22 */
void anu_exec_appear(void *obj, AnuWork *w)
{
    enemy_task_appear_scale(obj, w);
    if (w->alpha > 0.0f)
        w->lock.flags |= LOCK_LOCKABLE;
    switch (enemy_task_get_phase(obj)) {
    case 0:
        w->lock.flags &= ~LOCK_LOCKABLE;
        break;
    case 2:
        w->lock.flags |= LOCK_LOCKABLE;
        break;
    default:
        anu_update(obj, w);
    }
}

/* 0x00afb748 stg02, 0x00afade8 stg03, 0x00afb250 stg04, 0x00af9a30 stg22 */
void anu_exec_normal(void *obj, AnuWork *w)
{
    float frm;

    if (enemy_task_get_phase(obj) != 1)
        return;
    anu_update(obj, w);
    frm = enemy_task_get_time(obj);
    if (fabsf(enemy_task_get_param(obj, 0)) * 60.0f / 60.0f < frm)
        enemy_task_set_state(obj, 2);
}

/* 0x00afb7e8 stg02, 0x00afae88 stg03, 0x00afb2f0 stg04, 0x00af9ad0 stg22 */
void anu_exec_leave(void *obj, AnuWork *w)
{
    do { enemy_task_vanish_scale(obj, w); } while (0);
    switch (enemy_task_get_phase(obj)) {
    case 0:
        w->lock.flags &= ~LOCK_LOCKABLE;
        break;
    case 2:
        break;
    default:
        anu_update(obj, w);
    }
}

/* 0x00afb860 stg02, 0x00afaf00 stg03, 0x00afb368 stg04, 0x00af9b48 stg22 */
void anu_exec_destroyed(void *obj, AnuWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 0) {
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = ANU_BREAK_MODEL;
        desc.models = stage_models;
        desc.lock = &w->lock;
        desc.kind = 0;
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        enemy_task_get_param_vec(obj, &desc.v);
        enemy_shatter_create(obj, &desc, 0, 0);
        FINISH_IF(obj);
    }
}

/* 0x00afb920 stg02, 0x00afafc0 stg03, 0x00afb428 stg04, 0x00af9c08 stg22 */
void anu_pre_exec(void)
{
}

/* 0x00afb928 stg02, 0x00afafc8 stg03, 0x00afb430 stg04, 0x00af9c10 stg22 */
void anu_exec(void *obj, AnuWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (anu_exec_table[i])
        anu_exec_table[i](obj, w);
}

/* 0x00afb980 stg02, 0x00afb020 stg03, 0x00afb488 stg04, 0x00af9c68 stg22 */
/* steer(cast) */
void anu_post_exec(void *obj, AnuWork *w)
{
    AnuVars *typ = w->type;
    LockObj *lock = &w->lock;
    Vec3 pos;
    float alpha;
    int s;

    enemy_task_get_scl(obj, &w->scale);
    v3_scale(&w->scale, typ->p.attack_ray_scl);
    if (!enemy_status_get_ticks())
        anu_pose_model(obj, w);
    switch (((unsigned int (*)(void *))enemy_task_get_pstate)(obj)) {
    case 1:
        alpha = enemy_task_get_state_time(obj) / 30.0f;
        w->alpha = alpha;
        alpha = MIN(alpha >= 0.0f ? alpha : 0.0f, 1.0f);
        w->alpha = alpha;
        break;
    case 0:
        w->alpha = 1.0f;
        break;
    case 2:
        alpha = 1.0f - enemy_task_get_state_time(obj) / FPS;
        w->alpha = alpha;
        alpha = MIN(alpha >= 0.0f ? alpha : 0.0f, 1.0f);
        w->alpha = alpha;
        break;
    default:
        w->alpha = 0.0f;
        break;
    }
    if (lock->flags & LOCK_HIT) {
        v3_copy(&w->hitv, &lock->imp.v);
        if (lock->life <= 0 && ((unsigned int (*)(void *))enemy_task_get_pstate)(obj) < 2) {
            int mult = lockon_hits_max_shot_cnt(lock);

            enemy_task_get_pos(obj, &pos);
            enemy_status_score_popup(enemy_task_get_status_slot(obj), &pos, typ->p.score, mult);
            enemy_task_set_state(obj, 3);
        }
    }
}

/* 0x00afbba0 stg02, 0x00afb240 stg03, 0x00afb6a8 stg04, 0x00af9e88 stg22 */
/* steer(cast) */
void anu_disp_body(void *obj, AnuWork *w)
{
    Vec3 pos;
    int ang[4];
    Vec3 scl;
    float rgba[4];
    ModelPart *gfx;
    AnuVars *typ;
    int blend;

    gfx = enemy_task_gfx_ptr(obj);
    enemy_task_get_pos(obj, &pos);
    enemy_task_get_rot(obj, ang);
    enemy_task_get_scl(obj, &scl);
    if (view_sphere_visible(&pos, gfx->radius)) {
        blend = 0;
        typ = w->type;
        if (w->alpha < 1.0f) {
            blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
            heap_bzero(rgba, 16);
            draw_push_strip_flags();
            draw_push_override_color();
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 0x800);
            rgba[0] = w->alpha - 1.0f;
            draw_set_override_color(rgba);
            psGsSetAlphaTest(0, 1, 128, 1);
        }
        ulGxSetTextureList(((void **)obj)[1]);
        do {
            ulMatrixPush();
        } while (0);
        vu0_matrix_translate(ulMatrixCur, pos.x, pos.y, pos.z);
        anu_rotate_yxz(ang);
        vu0_matrix_rotate_z(ulMatrixCur, w->roll);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
        dc_object_draw(gfx);
        if (typ->p.wire) {
            draw_set_color(yuki_beat_color(typ->p.wire_col, typ->p.wire_col2, typ->p.wire_beat,
                                          typ->p.wire_linear, w->alpha, typ->p.wire_near,
                                          typ->p.wire_far, typ->p.wire_near_rate,
                                          typ->p.wire_far_rate));
            draw_set_blend(4, 5);
            model_object_draw(&w->body, gfx->frame);
        }
        ulMatrixPop();
        if (w->alpha < 1.0f) {
            psGsSetAlphaTest(1, 5, 128, 1);
            draw_pop_strip_flags();
            draw_pop_override_color();
            draw_set_override_flags(blend);
        }
    }
}

/* 0x00afbdb8 stg02, 0x00afb458 stg03, 0x00afb8c0 stg04, 0x00afa0a0 stg22 */
void anu_disp(void *obj, AnuWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (anu_disp_table[i])
        anu_disp_table[i](obj, w);
}

/* 0x00afbe10 stg02, 0x00afb4b0 stg03, 0x00afb918 stg04, 0x00afa0f8 stg22 */
/* steer(cast) */
int anu_create(void *parent, Placement *rec)
{
    ObjDesc desc;
    ObjFuncs funcs = anu_funcs;
    int kind;
    AnuVars *typ;

    ((float (*)(float))float_frac)(rec->param0);
    typ = anu_types;
    kind = 0;
    nj_fill8(&desc, 0, sizeof desc);
    desc.arg = rec;
    desc.kind = &kind;
    desc.name = anu_name;
    desc.funcs = &funcs;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->p.hp;
    desc.same_lock = typ->p.same_lock;
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = ANU_MODEL;
    desc.model_no2 = ANU_MODEL2;
    desc.score = typ->p.score;
    desc.hp_reduce = typ->p.hp_reduce;
    desc.crash_r = typ->p.crash_r;
    desc.crash_damage = typ->p.crash_damage;
    desc.time = fabsf(rec->param0) * 60.0f / 60.0f;
    desc.size = 288;
    return enemy_task_task_create(parent, &desc);
}

/* 0x00afbf68 stg02, 0x00afb608 stg03, 0x00afba70 stg04, 0x00afa250 stg22 */
void anu_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
