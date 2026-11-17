/*
 * Khufu XL (stg01): a large enemy with a swinging ring. Moves in, brakes with its exhaust beams
 * dimming, charges and fires an E_Shot volley, backs off and speeds away.
 */

#include "types_common.h"
#include "task.h"

#include "types_overlays.h"

#include "debug.h"
#include "anim.h"
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
#include "player.h"
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

void e_shot_initial_angles(int *ang, Vec3 *pos, Vec3 *dir, int level);

void e_shot_fire(void *par, Vec3 *pos, Vec3 *dir, int level);
extern void *stage_textures;
extern void *stage_models;

extern ObjFuncs khufu_xl_funcs;
extern char khufu_xl_name[];
extern void (*khufu_xl_disp_table[])(void *o, KhufuXLWork *w);
extern void (*khufu_xl_exec_table[])(void *o, KhufuXLWork *w);
extern void (*khufu_xl_states[])(void *o, KhufuXLWork *w);
float khufu_xl_apply_ring_motion(float scl, void *obj, KhufuXLWork *w);
int khufu_xl_next_state(void *obj, KhufuXLWork *w, int n, int state);
void khufu_xl_shot_charge(void *obj, KhufuXLWork *w);
float khufu_xl_part_motion_scaled(float scl, ModelPart *p, Vec3 *pos, int *ang);
extern char khufu_xl_lock_name[];
extern int khufu_xl_init_arg;
extern KhufuXLVars khufu_xl_types[1];
extern VarDef khufu_xl_vardef;
extern VarDef khufu_xl_vardefs[54];
void khufu_xl_vars_register(void);
void khufu_xl_vars_unregister(void);
int khufu_xl_vars_release(void);
void khufu_xl_rotate_yxz(int *ang);

/* model list entry n as stg22 numbers it */
#if defined(OVERLAY_STG22)
#define KHUFU_XL_MODEL(n) ((n) - 3)
#else
#define KHUFU_XL_MODEL(n) (n)
#endif

/* 0x00afcce0 stg01, 0x00afa5d0 stg22 */
void khufu_xl_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&khufu_xl_vardef);
    for (i = 0; i < 1; i++) {
        def = khufu_xl_vardefs;
        val = (int *)&khufu_xl_types[i].p;
        khufu_xl_vardef.name = khufu_xl_types[i].name;
        khufu_xl_vardef.fmt = khufu_xl_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&khufu_xl_vardef);
        for (j = 0; j < 54; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)khufu_xl_vardefs);
    }
}

/* 0x00afcdf8 stg01, 0x00afa6e8 stg22 */
void khufu_xl_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00afce00 stg01, 0x00afa6f0 stg22 */
int khufu_xl_get_init_arg(void)
{
    return khufu_xl_init_arg;
}

/* 0x00afce10 stg01, 0x00afa700 stg22 */
int khufu_xl_vars_init(int *prm)
{
    khufu_xl_init_arg = *prm;
    khufu_xl_vars_register();
    return 0;
}

/* 0x00afce38 stg01, 0x00afa728 stg22 */
int khufu_xl_vars_release(void)
{
    khufu_xl_vars_unregister();
    return 0;
}

/* 0x00afce58 stg01, 0x00afa748 stg22 */
int khufu_xl_marker_draw(int obj, void *place)
{
    int *mdl;

    ulGxSetTextureList(stage_textures);
    mdl = pnb_get_entry(stage_models, KHUFU_XL_MODEL(7));
    return obj_draw_at(obj, place, (void *)*mdl);
}

/* 0x00afceb0 stg01, 0x00afa7a0 stg22 */
int khufu_xl_node_cb(unsigned int *node, int idx, KhufuXLWork *w)
{
    if (*node & 0x2000000)
        ulMatrixGetTranslation((float *)ulMatrixCur, &w->tip.x);
    if (*node & 0x4000000)
        ulMatrixGetTranslation((float *)ulMatrixCur, &w->pos.x);
    return 0;
}

/* 0x00afcf20 stg01, 0x00afa810 stg22 */
void khufu_xl_pose_model(void *obj, KhufuXLWork *w)
{
    Vec3 p;
    int ang[4];
    void *gfx = enemy_task_gfx_ptr(obj);

    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    dc_object_set_callback(gfx, khufu_xl_node_cb, w);
    do {
        ulMatrixPush();
    } while (0);
    matrix_unit_set(0);
    vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
    khufu_xl_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(gfx);
    ulMatrixPop();
    dc_object_set_callback(gfx, 0, w);
}

/* 0x00afcff0 stg01, 0x00afa8e0 stg22 */
float khufu_xl_part_motion_value(ModelPart *p)
{
    void *out[3];
    float pos[4], ang[4], val[4];

    out[0] = pos;
    out[1] = ang;
    out[2] = val;
    anim_eval_hierarchy(p->root, p->clip, out, p->frame);
    return val[0];
}

/* 0x00afd038 stg01, 0x00afa928 stg22 */
#if defined(OVERLAY_STG22)
/* steer(cast) */
void khufu_xl_init(void *obj, KhufuXLWork *w, int prm, int *kind)
{
    Vec3 v;
    int ang[3];
    BackFireLink lnk;
    float sc[2][2];
    KhufuXLVars *typ;
    float x, y, z, brk_end;
    int n;

    typ = &khufu_xl_types[*kind];
    w->type = typ;
    dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(*(void **)obj, 5)),
                  *((struct DcClip **)pnb_get_entry(stage_models, kind[2] == -1 ? 7 : 11)), 0x500);
    dc_object_set_wire(&w->body, model_pnb_get_wire(*(void **)obj, 5));
    ((ModelPart *)enemy_task_gfx_ptr(obj))->radius = w->body.radius = 83.0f;
    dc_object_init(&w->ring, *((struct DcNode **)pnb_get_entry(*(void **)obj, 19)), *((struct DcClip **)pnb_get_entry(stage_models, 21)),
                  0x1000);
    lockon_obj_init(&w->lock, obj, &w->pos, 0, khufu_xl_lock_name, enemy_task_get_status_slot(obj), typ->p.hp,
                  typ->p.same_lock, 0);
    lockon_entry(&w->lock);
    w->lock.flags &= ~LOCK_LOCKABLE;
    enemy_task_clear_flags(obj, 64);
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
    brk_end = (typ->p.break_sec + typ->p.attack_ray_sec_dif - typ->p.attack_ray_sec) * FPS;
    n = kind[2];
    w->fire = 0;
    w->flag = n == -1;
    w->shot_set = n;
    w->end = brk_end + ((int)fabsf(enemy_task_get_param(obj, 0)) < 2 ? 1.0f
                  : (int)fabsf(enemy_task_get_param(obj, 0)));
    enemy_task_set_appear(obj, 2, 30);
    enemy_task_set_vanish(obj, 2, 60);
    w->alpha = 0;
    w->life = kind[1];
    heap_bzero(&lnk, sizeof(lnk));
    lnk.gfx = enemy_task_gfx_ptr(obj);
    lnk.pos = enemy_task_pos_ptr(obj);
    lnk.ang = enemy_task_rot_ptr(obj);
    lnk.obj_scale = enemy_task_scl_ptr(obj);
    lnk.scale = &w->backfire_scl;
    lnk.alpha = &w->alpha;
    lnk.flags = 0x1000000;
    lnk.use_pose = 0;
    en_backfire_create(obj, &lnk, 0);
    w->backfire_scl = typ->p.backfire_scl * khufu_xl_part_motion_value(&w->ring);
    v3_set(&w->sway, 0.0f, 0.0f, 0.0f);
    i3_set(w->turn, 0, 0, 0);
}
#else
/* steer(cast) */
void khufu_xl_init(void *obj, KhufuXLWork *w, int prm, int *kind)
{
    Vec3 v;
    int ang[3];
    BackFireLink lnk;
    float sc[2][2];
    KhufuXLVars *typ;
    float x, y, z, brk_end;
    int n;

    typ = &khufu_xl_types[*kind];
    w->type = typ;
    dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(*(void **)obj, 8)),
                  *((struct DcClip **)pnb_get_entry(stage_models, kind[2] == -1 ? 10 : 14)), 0x500);
    dc_object_set_wire(&w->body, model_pnb_get_wire(*(void **)obj, 8));
    ((ModelPart *)enemy_task_gfx_ptr(obj))->radius = w->body.radius = 83.0f;
    dc_object_init(&w->ring, *((struct DcNode **)pnb_get_entry(*(void **)obj, 22)), *((struct DcClip **)pnb_get_entry(stage_models, 23)),
                  0x1000);
    lockon_obj_init(&w->lock, obj, &w->pos, 0, khufu_xl_lock_name, enemy_task_get_status_slot(obj), typ->p.hp,
                  typ->p.same_lock, 0);
    lockon_entry(&w->lock);
    w->lock.flags &= ~LOCK_LOCKABLE;
    enemy_task_clear_flags(obj, 64);
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
    brk_end = (typ->p.break_sec + typ->p.attack_ray_sec_dif - typ->p.attack_ray_sec) * FPS;
    n = kind[2];
    w->fire = 0;
    w->flag = n == -1;
    w->shot_set = n;
    w->end = brk_end + ((int)fabsf(enemy_task_get_param(obj, 0)) < 2 ? 1.0f
                  : (int)fabsf(enemy_task_get_param(obj, 0)));
    enemy_task_set_appear(obj, 2, 30);
    enemy_task_set_vanish(obj, 2, 60);
    w->alpha = 0;
    w->life = kind[1];
    heap_bzero(&lnk, sizeof(lnk));
    lnk.gfx = enemy_task_gfx_ptr(obj);
    lnk.pos = enemy_task_pos_ptr(obj);
    lnk.ang = enemy_task_rot_ptr(obj);
    lnk.obj_scale = enemy_task_scl_ptr(obj);
    lnk.scale = &w->backfire_scl;
    lnk.alpha = &w->alpha;
    lnk.flags = 0x1000000;
    lnk.use_pose = 0;
    en_backfire_create(obj, &lnk, 0);
    w->backfire_scl = typ->p.backfire_scl * khufu_xl_part_motion_value(&w->ring);
    v3_set(&w->sway, 0.0f, 0.0f, 0.0f);
    i3_set(w->turn, 0, 0, 0);
}
#endif

/* 0x00afd510 stg01, 0x00afae00 stg22 */
void khufu_xl_release(void *obj, KhufuXLWork *w)
{
    dc_object_release(&w->ring);
    dc_object_release(&w->body);
    lockon_remove(&w->lock);
}

/* 0x00afd548 stg01, 0x00afae38 stg22 */
int khufu_xl_next_state(void *obj, KhufuXLWork *w, int n, int state)
{
    if (++w->frames > n) {
        w->state = state;
        w->frames = 1;
        enemy_task_get_rot(obj, w->ang0);
        return 1;
    }
    return 0;
}

/* 0x00afd590 stg01, 0x00afae80 stg22 */
/* steer(cast) */
void khufu_xl_shot_charge(void *obj, KhufuXLWork *w)
{
    AttackRayArg ray;

    if (!w->flag && --w->end <= 0) {
        heap_bzero(&ray, 16);
        ray.dur = w->type->p.attack_ray_sec * FPS + (w->end < 0 ? 0 : w->end);
        ray.pos = &w->tip;
        ray.scale = &w->scale;
        ((void *(*)(void *, AttackRayArg *))attack_ray_create)(obj, &ray);
        w->flag = 1;
    }
}

/* 0x00afd638 stg01, 0x00afaf28 stg22 */
void khufu_xl_state_move(void *obj, KhufuXLWork *w)
{
    int n;
    Vec3 p, v;

    n = (int)fabsf(enemy_task_get_param(obj, 0)) < 2 ? 1 : (int)fabsf(enemy_task_get_param(obj, 0));
    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, w->speed);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    khufu_xl_shot_charge(obj, w);
    if (khufu_xl_next_state(obj, w, n, 1))
        enemy_task_get_rot(obj, w->turn);
}

/* 0x00afd750 stg01, 0x00afb040 stg22 */
float khufu_xl_part_motion_scaled(float scl, ModelPart *p, Vec3 *pos, int *ang)
{
    void *out[3];
    float val[4];

    out[0] = pos;
    out[1] = ang;
    out[2] = val;
    anim_eval_hierarchy(p->root, p->clip, out, p->frame);
    v3_scale(pos, scl);
    ang[0] = ang[0] * scl;
    ang[1] = ang[1] * scl;
    ang[2] = ang[2] * scl;
    return val[0];
}

/* 0x00afd818 stg01, 0x00afb108 stg22 */
float khufu_xl_apply_ring_motion(float scl, void *obj, KhufuXLWork *w)
{
    Vec3 ofs;
    Vec3 p;
    int rang[4];
    int ang[4];
    float sc[3][2];
    int *base = w->turn;

    dc_object_advance_time(&w->ring);
    scl = khufu_xl_part_motion_scaled(scl, &w->ring, &ofs, rang);
    ROT_Z(base[2], sc[0], ofs.x, ofs.y);
    ROT_X(base[0], sc[1], ofs.y, ofs.z);
    ROT_Y(base[1], sc[2], ofs.x, ofs.z);
    enemy_task_get_pos(obj, &p);
    v3_sub(&p, &p, &w->sway);
    v3_add(&p, &ofs);
    enemy_task_set_pos(obj, &p);
    v3_copy(&w->sway, &ofs);
    ulMatrixPush();
    matrix_unit_set(0);
    khufu_xl_rotate_yxz(base);
    khufu_xl_rotate_yxz(rang);
    model_camera_angles(ang);
    ulMatrixPop();
    enemy_task_set_rot(obj, ang);
    return scl;
}

/* 0x00afda40 stg01, 0x00afb330 stg22 */
#if defined(OVERLAY_STG22)
/* steer(cast) */
void khufu_xl_state_break(void *obj, KhufuXLWork *w)
{
    KhufuXLVars *typ = w->type;
    Vec3 p;
    Vec3 v;
    Vec3 pl;
    Vec3 cur;
    Vec3 to_pl;
    int ang[3];
    float sc[6][2];
    ModelPart *gfx;
    float prev, rate, one, spd, bscl, x, y, z;
    float brk, dbrk, ring, dis, ddis;
    int n;

    n = typ->p.break_sec * FPS;
    rate = w->frames / (float)n;
    prev = (w->frames - 1) / (float)n;
    gfx = enemy_task_gfx_ptr(obj);
    if (w->shot_set != -1) {
        ulSinCos((int)(safe_powf(rate, typ->p.break_ang_pow) * ANG_HALF - ANG_QUARTER), &sc[0][0],
                      &sc[0][1]);
        enemy_task_get_pos(obj, &v);
        e_shot_initial_angles(w->aim, &v, 0, w->shot_set);
        yuki_lerp3(w->turn, w->ang0, w->aim, (sc[0][0] + 1.0f) * 0.5f);
    } else {
        player_get_pos(&pl);
        enemy_task_get_pos(obj, &cur);
        v3_sub(&to_pl, &pl, &cur);
        do {
            ang[1] = (int)RAD_TO_ANG(atan2f(-to_pl.x, -to_pl.z));
            ang[0] = (int)RAD_TO_ANG(atan2f(to_pl.y, sqrtf(to_pl.x * to_pl.x + to_pl.z * to_pl.z)));
        } while (0);
        ang[2] = 0;
        ulSinCos((int)(safe_powf(rate, typ->p.break_ang_pow) * ANG_HALF - ANG_QUARTER), &sc[0][0],
                      &sc[0][1]);
        yuki_lerp3(w->turn, w->ang0, ang, (sc[0][0] + 1.0f) * 0.5f);
    }
    one = 1.0f;
    gfx->frame = gfx->frames * rate;
    x = 0.0f;
    y = 0.0f;
    enemy_task_get_pos(obj, &p);
    spd = w->speed * FPS;
    spd = -spd;
    brk = spd / (typ->p.break_pow + one) * typ->p.break_sec * (safe_powf(one - rate, typ->p.break_pow + one) - one);
    dbrk = brk - spd / (typ->p.break_pow + one) * typ->p.break_sec * (safe_powf(one - prev, typ->p.break_pow + one) - one);
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, dbrk);
    v3_add(&p, &v);
    z = -1.0f;
    ROT_X(w->turn[0], sc[1], y, z);
    ROT_Y(w->turn[1], sc[2], x, z);
    v3_set(&v, x, y, z);
    ulSinCos((int)(safe_powf(rate, typ->p.break_dis_pow) * ANG_HALF - ANG_QUARTER), &sc[3][0],
             &sc[3][1]);
    dis = (sc[3][0] + one) * 0.5f;
    ulSinCos((int)(safe_powf(prev, typ->p.break_dis_pow) * ANG_HALF - ANG_QUARTER), &sc[3][0],
             &sc[3][1]);
    ddis = dis - (sc[3][0] + one) * 0.5f;
    v3_scale(&v, typ->p.break_dis * ddis);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    ulSinCos((int)(safe_powf(rate, typ->p.stop_swing_pow) * ANG_HALF - ANG_QUARTER), &sc[4][0],
             &sc[4][1]);
    sc[4][0] = (sc[4][0] + one) * 0.5f * typ->p.stop_swing_scl;
    ring = khufu_xl_apply_ring_motion(sc[4][0], obj, w);
    khufu_xl_shot_charge(obj, w);
    ulSinCos((int)(safe_powf(rate, typ->p.break_backfire_pow2) * ANG_HALF - ANG_QUARTER), &sc[5][0],
             &sc[5][1]);
    sc[5][0] = (sc[5][0] + one) * 0.5f;
    bscl = typ->p.backfire_scl * (one - sc[5][0]) + typ->p.break_backfire_scl * sc[5][0];
    ulSinCos((int)(safe_powf(rate, typ->p.break_backfire_pow) * ANG_TURN + ANG_QUARTER), &sc[5][0],
             &sc[5][1]);
    sc[5][0] = (sc[5][0] + one) * 0.5f;
    w->backfire_scl = bscl * sc[5][0] * ring;
    if (w->shot_set == -1) {
        if (khufu_xl_next_state(obj, w, n, 3)) {
            dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 14)));
            gfx->frame = 0.0f;
            dc_object_set_clip(&w->body, *((struct DcClip **)pnb_get_entry(stage_models, 15)));
        }
    } else if (khufu_xl_next_state(obj, w, n, 2)) {
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 8)));
        gfx->frame = 0.0f;
        dc_object_set_clip(&w->body, *((struct DcClip **)pnb_get_entry(stage_models, 9)));
        w->fire = 1;
    }
}
#else
/* steer(cast) */
void khufu_xl_state_break(void *obj, KhufuXLWork *w)
{
    KhufuXLVars *typ = w->type;
    Vec3 p;
    Vec3 v;
    Vec3 pl;
    Vec3 cur;
    Vec3 to_pl;
    int ang[3];
    float sc[6][2];
    ModelPart *gfx;
    float prev, rate, one, spd, bscl, x, y, z;
    float brk, dbrk, ring, ddis;
    int n;

    n = typ->p.break_sec * FPS;
    rate = w->frames / (float)n;
    prev = (w->frames - 1) / (float)n;
    gfx = enemy_task_gfx_ptr(obj);
    if (w->shot_set != -1) {
        ulSinCos((int)(safe_powf(rate, typ->p.break_ang_pow) * ANG_HALF - ANG_QUARTER), &sc[0][0],
                      &sc[0][1]);
        enemy_task_get_pos(obj, &v);
        e_shot_initial_angles(w->aim, &v, 0, w->shot_set);
        yuki_lerp3(w->turn, w->ang0, w->aim, (sc[0][0] + 1.0f) * 0.5f);
    } else {
        player_get_pos(&pl);
        enemy_task_get_pos(obj, &cur);
        v3_sub(&to_pl, &pl, &cur);
        do {
            ang[1] = (int)RAD_TO_ANG(atan2f(-to_pl.x, -to_pl.z));
            ang[0] = (int)RAD_TO_ANG(atan2f(to_pl.y, sqrtf(to_pl.x * to_pl.x + to_pl.z * to_pl.z)));
        } while (0);
        ang[2] = 0;
        ulSinCos((int)(safe_powf(rate, typ->p.break_ang_pow) * ANG_HALF - ANG_QUARTER), &sc[0][0],
                      &sc[0][1]);
        yuki_lerp3(w->turn, w->ang0, ang, (sc[0][0] + 1.0f) * 0.5f);
    }
    one = 1.0f;
    gfx->frame = gfx->frames * rate;
    x = 0.0f;
    y = 0.0f;
    enemy_task_get_pos(obj, &p);
    spd = w->speed * FPS;
    spd = -spd;
    brk = spd / (typ->p.break_pow + one) * typ->p.break_sec * (safe_powf(one - rate, typ->p.break_pow + one) - one);
    dbrk = brk - spd / (typ->p.break_pow + one) * typ->p.break_sec * (safe_powf(one - prev, typ->p.break_pow + one) - one);
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, dbrk);
    v3_add(&p, &v);
    z = -1.0f;
    ROT_X(w->turn[0], sc[1], y, z);
    ROT_Y(w->turn[1], sc[2], x, z);
    v3_set(&v, x, y, z);
    ulSinCos((int)(safe_powf(rate, typ->p.break_dis_pow) * ANG_HALF - ANG_QUARTER), &sc[3][0],
             &sc[3][1]);
    ddis = (sc[3][0] + one) * 0.5f;
    ulSinCos((int)(safe_powf(prev, typ->p.break_dis_pow) * ANG_HALF - ANG_QUARTER), &sc[3][0],
             &sc[3][1]);
    ddis -= (sc[3][0] + one) * 0.5f;
    v3_scale(&v, typ->p.break_dis * ddis);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    ulSinCos((int)(safe_powf(rate, typ->p.stop_swing_pow) * ANG_HALF - ANG_QUARTER), &sc[4][0],
             &sc[4][1]);
    sc[4][0] = (sc[4][0] + one) * 0.5f * typ->p.stop_swing_scl;
    ring = khufu_xl_apply_ring_motion(sc[4][0], obj, w);
    khufu_xl_shot_charge(obj, w);
    ulSinCos((int)(safe_powf(rate, typ->p.break_backfire_pow2) * ANG_HALF - ANG_QUARTER), &sc[5][0],
             &sc[5][1]);
    sc[5][0] = (sc[5][0] + one) * 0.5f;
    bscl = typ->p.backfire_scl * (one - sc[5][0]) + typ->p.break_backfire_scl * sc[5][0];
    ulSinCos((int)(safe_powf(rate, typ->p.break_backfire_pow) * ANG_TURN + ANG_QUARTER), &sc[5][0],
             &sc[5][1]);
    sc[5][0] = (sc[5][0] + one) * 0.5f;
    w->backfire_scl = bscl * sc[5][0] * ring;
    if (w->shot_set == -1) {
        if (khufu_xl_next_state(obj, w, n, 3)) {
            dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 17)));
            gfx->frame = 0.0f;
            dc_object_set_clip(&w->body, *((struct DcClip **)pnb_get_entry(stage_models, 18)));
        }
    } else if (khufu_xl_next_state(obj, w, n, 2)) {
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 11)));
        gfx->frame = 0.0f;
        dc_object_set_clip(&w->body, *((struct DcClip **)pnb_get_entry(stage_models, 12)));
        w->fire = 1;
    }
}
#endif

/* 0x00afe130 stg01, 0x00afba20 stg22 */
/* steer(cast) */
void khufu_xl_state_shot(void *obj, KhufuXLWork *w)
{
    KhufuXLVars *typ = w->type;
    float sc[2];
    ModelPart *gfx;
    float rate, ring, bf_rate;
    int n, bf_end;

    n = typ->p.shot_sec * FPS;
    rate = w->frames / (float)n;
    gfx = enemy_task_gfx_ptr(obj);
    ring = khufu_xl_apply_ring_motion(typ->p.stop_swing_scl, obj, w);
    gfx->frame = gfx->frames * rate;
    bf_end = (int)fabsf(enemy_task_get_param(obj, 1)) < 2 ? 1 : (int)fabsf(enemy_task_get_param(obj, 1));
    bf_end = MIN(typ->p.stop_backfire_sec * FPS, bf_end + n);
    bf_rate = w->frames / (float)bf_end;
    bf_rate = MIN(bf_rate >= 0.0f ? bf_rate : 0.0f, 1.0f);
    ulSinCos((int)(safe_powf(bf_rate, typ->p.stop_backfire_pow) * ANG_HALF - ANG_QUARTER), &sc[0],
             &sc[1]);
    sc[0] = (sc[0] + 1.0f) * 0.5f;
    w->backfire_scl = (typ->p.backfire_scl * sc[0] + typ->p.break_backfire_scl * (1.0f - sc[0])) * ring;
    if (khufu_xl_next_state(obj, w, n, 3)) {
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, KHUFU_XL_MODEL(15))));
        gfx->frame = 0.0f;
        dc_object_set_clip(&w->body, *((struct DcClip **)pnb_get_entry(stage_models, KHUFU_XL_MODEL(16))));
    }
}

/* 0x00afe378 stg01, 0x00afbc68 stg22 */
void khufu_xl_state_stop(void *obj, KhufuXLWork *w)
{
    KhufuXLVars *typ;
    Vec3 pl;
    Vec3 cur;
    float sc[2];
    float rate, ring, bf_rate, shot_end;
    int end, bf_end;

    if ((int)fabsf(enemy_task_get_param(obj, 1)) < 2)
        end = 1;
    else
        end = (int)fabsf(enemy_task_get_param(obj, 1));
    rate = w->frames / (float)end;
    typ = w->type;
    ring = khufu_xl_apply_ring_motion(typ->p.stop_swing_scl, obj, w);
    if (w->shot_set == -1) {
        bf_rate = w->frames / (typ->p.stop_backfire_sec * FPS);
        bf_rate = MIN(bf_rate >= 0.0f ? bf_rate : 0.0f, 1.0f);
        bf_rate = bf_rate >= rate ? bf_rate : rate;
    } else {
        shot_end = typ->p.shot_sec * FPS;
        bf_end = MIN((int)(typ->p.stop_backfire_sec * FPS), shot_end + end);
        bf_rate = (w->frames + shot_end) / (float)bf_end;
        bf_rate = MIN(bf_rate >= 0.0f ? bf_rate : 0.0f, 1.0f);
    }
    ulSinCos((int)(safe_powf(bf_rate, typ->p.stop_backfire_pow) * ANG_HALF - ANG_QUARTER), &sc[0],
             &sc[1]);
    sc[0] = (sc[0] + 1.0f) * 0.5f;
    w->backfire_scl = (typ->p.backfire_scl * sc[0] + typ->p.break_backfire_scl * (1.0f - sc[0])) * ring;
    if (khufu_xl_next_state(obj, w, end, 4)) {
        float ox, oy, oz;

        i3_copy(w->ang0, w->turn);
        ox = enemy_task_get_param(obj, 3);
        oy = enemy_task_get_param(obj, 4);
        oz = enemy_task_get_param(obj, 5);
        enemy_task_get_pos(obj, &cur);
        player_get_pos(&pl);
        pl.x += ox;
        pl.y += oy;
        pl.z += oz;
        v3_sub(&w->dir, &pl, &cur);
        vec3_normalize(&w->dir, &w->dir);
        do {
            w->dang[1] = (int)RAD_TO_ANG(atan2f(-w->dir.x, -w->dir.z));
            w->dang[0] = (int)RAD_TO_ANG(atan2f(w->dir.y, sqrtf(w->dir.x * w->dir.x
                                                            + w->dir.z * w->dir.z)));
        } while (0);
        w->dang[2] = 0;
    }
}

/* 0x00afe750 stg01, 0x00afc040 stg22 */
void khufu_xl_state_back(void *obj, KhufuXLWork *w)
{
    KhufuXLVars *typ = w->type;
    Vec3 p;
    Vec3 v;
    float sc[6][2];
    ModelPart *gfx;
    float half_turn, quarter, one, half, rate, prev, dis, x, y, z;
    float acrv, crv;
    int n;

    n = typ->p.back_sec * FPS;
    rate = w->frames / (float)n;
    prev = (w->frames - 1) / (float)n;
    half_turn = ANG_HALF;
    quarter = ANG_QUARTER;
    one = 1.0f;
    half = 0.5f;
    x = y = 0.0f;
    ulSinCos((int)(safe_powf(rate, typ->p.accel_ang_pow) * half_turn - quarter), &sc[0][0], &sc[0][1]);
    acrv = (sc[0][0] + one) * half;
    ulSinCos((int)(safe_powf(rate, typ->p.accel_ang_pow2) * ANG_TURN - quarter), &sc[0][0], &sc[0][1]);
    yuki_lerp3(w->turn, w->ang0, w->dang, acrv + (sc[0][0] + one) * half * typ->p.accel_ang_over_rate);
    ulSinCos((int)(safe_powf(rate, typ->p.back_pitch_pow) * half_turn - quarter), &sc[0][0], &sc[0][1]);
    ulMatrixPush();
    matrix_unit_set(0);
    khufu_xl_rotate_yxz(w->turn);
    vu0_matrix_rotate_x(ulMatrixCur, (int)(typ->p.back_pitch * (sc[0][0] + one) * half));
    model_camera_angles(w->turn);
    ulMatrixPop();
    z = 1.0f;
    ROT_X(w->turn[0], sc[1], y, z);
    ROT_Y(w->turn[1], sc[2], x, z);
    v3_set(&v, x, y, z);
    enemy_task_get_pos(obj, &p);
    ulSinCos((int)(safe_powf(rate, typ->p.back_pow) * half_turn - quarter), &sc[3][0], &sc[3][1]);
    crv = (sc[3][0] + one) * half;
    ulSinCos((int)(safe_powf(prev, typ->p.back_pow) * half_turn - quarter), &sc[3][0], &sc[3][1]);
    dis = (crv - (sc[3][0] + one) * half) * typ->p.back_dis;
    v3_scale(&v, dis);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    ulSinCos((int)(safe_powf(rate, typ->p.stop_swing_pow) * half_turn + quarter), &sc[4][0], &sc[4][1]);
    sc[4][0] = (sc[4][0] + one) * half * typ->p.stop_swing_scl;
    khufu_xl_apply_ring_motion(sc[4][0], obj, w);
    gfx = enemy_task_gfx_ptr(obj);
    gfx->frame = rate * 20.0f;
    ulSinCos((int)(safe_powf(rate, typ->p.back_backfire_pow) * half_turn - quarter), &sc[5][0], &sc[5][1]);
    sc[5][0] = (sc[5][0] + one) * half;
    w->backfire_scl = typ->p.backfire_scl * (one - sc[5][0]) + sc[5][0] * typ->p.back_backfire_scl;
    w->backfire_scl = w->backfire_scl * khufu_xl_part_motion_value(&w->ring);
    if (khufu_xl_next_state(obj, w, n, 5)) {
        gfx = enemy_task_gfx_ptr(obj);
        gfx->speed *= w->shot_set == -1 ? typ->p.accel_motion_spd_den : typ->p.accel_motion_spd_den2;
    }
}

/* 0x00afebf0 stg01, 0x00afc4e0 stg22 */
void khufu_xl_state_accel(void *obj, KhufuXLWork *w)
{
    KhufuXLVars *typ = w->type;
    Vec3 p;
    Vec3 v;
    int ang[4];
    int nang[4];
    float sc[4][2];
    float fps, prev, rate, spd, dis, x, y, z;
    int n;

    fps = FPS;
    n = typ->p.accel_sec * fps;
    rate = w->frames / (float)n;
    prev = (w->frames - 1) / (float)n;
    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    x = 0.0f;
    y = 0.0f;
    z = -1.0f;
    ROT_X(ang[0], sc[0], y, z);
    ROT_Y(ang[1], sc[1], x, z);
    v3_set(&v, x, y, z);
    spd = w->speed * fps;
    dis = spd / (typ->p.accel_pow + 1.0f) * typ->p.accel_sec * (typ->p.accel_spd_over_rate + 1.0f)
        * safe_powf(rate, typ->p.accel_pow + 1.0f);
    dis -= spd / (typ->p.accel_pow + 1.0f) * typ->p.accel_sec * (typ->p.accel_spd_over_rate + 1.0f)
         * safe_powf(prev, typ->p.accel_pow + 1.0f);
    v3_scale(&v, dis);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    ulSinCos((int)(safe_powf(rate, typ->p.accel_pitch_pow) * ANG_HALF - ANG_QUARTER), &sc[2][0],
             &sc[2][1]);
    yuki_lerp3(nang, w->ang0, w->dang, (sc[2][0] + 1.0f) * 0.5f);
    enemy_task_set_rot(obj, nang);
    dc_object_advance_time(enemy_task_gfx_ptr(obj));
    ulSinCos((int)(safe_powf(rate, typ->p.accel_backfire_pow) * ANG_HALF - ANG_QUARTER), &sc[3][0],
             &sc[3][1]);
    sc[3][0] = (sc[3][0] + 1.0f) * 0.5f;
    w->backfire_scl = typ->p.accel_backfire_scl * (1.0f - sc[3][0]) + sc[3][0] * typ->p.backfire_scl;
    w->backfire_scl = w->backfire_scl * khufu_xl_part_motion_value(&w->ring);
    khufu_xl_next_state(obj, w, n, 6);
}

/* 0x00afef28 stg01, 0x00afc818 stg22 */
void khufu_xl_state_away(void *obj, KhufuXLWork *w)
{
    Vec3 p;
    Vec3 step;
    float sspd, espd, dis;
    int n;

    sspd = w->speed * FPS;
    n = w->type->p.away_sec * FPS;
    espd = w->type->p.accel_spd_over_rate * sspd + sspd;
    dis = ((sspd - espd) * (w->frames / (float)n) * 0.5f + espd) * (w->frames / FPS)
        - ((sspd - espd) * ((w->frames - 1) / (float)n) * 0.5f + espd) * ((w->frames - 1) / FPS);
    enemy_task_get_pos(obj, &p);
    v3_mul(&step, &w->dir, dis);
    v3_add(&p, &step);
    enemy_task_set_pos(obj, &p);
    dc_object_advance_time(enemy_task_gfx_ptr(obj));
    khufu_xl_next_state(obj, w, n, 7);
}

/* 0x00aff098 stg01, 0x00afc988 stg22 */
void khufu_xl_state_cruise(void *obj, KhufuXLWork *w)
{
    Vec3 p;
    Vec3 step;
    ModelPart *gfx;
    KhufuXLVars *typ;

    enemy_task_get_pos(obj, &p);
    v3_mul(&step, &w->dir, w->speed);
    v3_add(&p, &step);
    enemy_task_set_pos(obj, &p);
    gfx = enemy_task_gfx_ptr(obj);
    typ = w->type;
    dc_object_advance_time(gfx);
    gfx->speed *= w->shot_set == -1 ? typ->p.accel_motion_spd_den : typ->p.accel_motion_spd_den2;
}

/* 0x00aff170 stg01, 0x00afca60 stg22 */
void khufu_xl_update(void *obj, KhufuXLWork *w)
{
    int ang[4];
    Vec3 dir;
    float sc[2][2];
    float x, y, z;

    if (khufu_xl_states[w->state])
        khufu_xl_states[w->state](obj, w);
    khufu_xl_pose_model(obj, w);
    if (w->fire) {
        enemy_task_get_rot(obj, ang);
        x = 0.0f;
        y = 0.0f;
        z = -1.0f;
        ROT_X(ang[0], sc[0], y, z);
        ROT_Y(ang[1], sc[1], x, z);
        v3_set(&dir, x, y, z);
        e_shot_fire(obj, &w->tip, &dir, w->shot_set);
        w->fire = 0;
    }
}

/* 0x00aff288 stg01, 0x00afcb78 stg22 */
void khufu_xl_exec_appear(void *obj, KhufuXLWork *w)
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
        khufu_xl_update(obj, w);
    }
}

/* 0x00aff328 stg01, 0x00afcc18 stg22 */
void khufu_xl_exec_normal(void *obj, KhufuXLWork *w)
{
    float frm;

    if (enemy_task_get_phase(obj) != 1)
        return;
    khufu_xl_update(obj, w);
    frm = enemy_task_get_time(obj);
    if (w->life < frm)
        enemy_task_set_state(obj, 2);
}

/* 0x00aff398 stg01, 0x00afcc88 stg22 */
void khufu_xl_exec_leave(void *obj, KhufuXLWork *w)
{
    do { enemy_task_vanish_scale(obj, w); } while (0);
    switch (enemy_task_get_phase(obj)) {
    case 0:
        w->lock.flags &= ~LOCK_LOCKABLE;
        break;
    case 2:
        break;
    default:
        khufu_xl_update(obj, w);
    }
}

/* 0x00aff410 stg01, 0x00afcd00 stg22 */
void khufu_xl_exec_destroyed(void *obj, KhufuXLWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 0) {
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = KHUFU_XL_MODEL(21);
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

/* 0x00aff4d0 stg01, 0x00afcdc0 stg22 */
void khufu_xl_pre_exec(void)
{
}

/* 0x00aff4d8 stg01, 0x00afcdc8 stg22 */
void khufu_xl_exec(void *obj, KhufuXLWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (khufu_xl_exec_table[i])
        khufu_xl_exec_table[i](obj, w);
}

/* 0x00aff530 stg01, 0x00afce20 stg22 */
/* steer(cast) */
void khufu_xl_post_exec(void *obj, KhufuXLWork *w)
{
    KhufuXLVars *typ = w->type;
    LockObj *lock = &w->lock;
    Vec3 p;
    float alpha;

    enemy_task_get_scl(obj, &w->scale);
    v3_scale(&w->scale, typ->p.attack_ray_scl);
    switch (((unsigned int)enemy_task_get_pstate(obj))) {
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
    if (!enemy_status_get_ticks())
        khufu_xl_pose_model(obj, w);
    if (lock->flags & LOCK_HIT) {
        v3_copy(&w->hitv, &lock->imp.v);
        if (lock->life <= 0 && ((unsigned int)enemy_task_get_pstate(obj)) < 2) {
            int k = lockon_hits_max_shot_cnt(lock);

            enemy_task_get_pos(obj, &p);
            enemy_status_score_popup(enemy_task_get_status_slot(obj), &p, typ->p.score, k);
            enemy_task_set_state(obj, 3);
        }
    }
}

/* 0x00aff758 stg01, 0x00afd048 stg22 */
/* steer(cast) */
void khufu_xl_disp_body(void *obj, KhufuXLWork *w)
{
    Vec3 p;
    int ang[4];
    float rgba[4];
    ModelPart *gfx;
    KhufuXLVars *typ;
    int prev;

    gfx = enemy_task_gfx_ptr(obj);
    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    if (view_sphere_visible(&p, gfx->radius)) {
        prev = 0;
        typ = w->type;
        if (w->alpha < 1.0f) {
            prev = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
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
        vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
        khufu_xl_rotate_yxz(ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
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
            draw_set_override_flags(prev);
        }
    }
}

/* 0x00aff948 stg01, 0x00afd238 stg22 */
void khufu_xl_disp(void *obj, KhufuXLWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (khufu_xl_disp_table[i])
        khufu_xl_disp_table[i](obj, w);
}

/* 0x00aff9a0 stg01, 0x00afd290 stg22 */
/* steer(cast) */
int khufu_xl_create(void *parent, Placement *place)
{
    int kind[3];
    ObjDesc desc;
    ObjFuncs funcs = khufu_xl_funcs;
    KhufuXLVars *typ;
    int n;
    float ms = 1000.0f, half = 0.5f;

    ((float (*)(float))float_frac)(place->param0);
    typ = khufu_xl_types;
    kind[0] = 0;
    kind[1] = (int)fabsf(place->param0) + (int)fabsf(place->param1) + (int)place->param2
             + (typ->p.break_sec + typ->p.accel_sec + typ->p.away_sec) * FPS;
    if (place->param1 < 0.0f) {
        kind[2] = -1;
    } else {
        n = ((float (*)(float))float_frac)(place->param1) * ms + half;
        n = n < 0 ? 0 : n;
        kind[2] = n > 7 ? 7 : n;
    }
    nj_fill8(&desc, 0, sizeof desc);
    desc.name = khufu_xl_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.kind = kind;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->p.hp;
    desc.same_lock = typ->p.same_lock;
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = KHUFU_XL_MODEL(7);
    desc.model_no2 = kind[2] == -1 ? KHUFU_XL_MODEL(9) : KHUFU_XL_MODEL(13);
    desc.score = typ->p.score;
    desc.hp_reduce = typ->p.hp_reduce;
    desc.crash_r = typ->p.crash_r;
    desc.crash_damage = typ->p.crash_damage;
    desc.time = kind[1];
    desc.size = 412;
    return enemy_task_task_create(parent, &desc);
}

/* 0x00affbb8 stg01, 0x00afd4a8 stg22 */
void khufu_xl_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
