/*
 * Ramses (stg01): rises out of the ground (cut by plane_cut, rings from ramses_circle), halo
 * facing the camera. Like Khufu XL it moves, brakes, charges, fires an E_Shot volley, backs off
 * and speeds away; at the end of its time it sinks back.
 */

#include "types_common.h"
#include "task.h"

#include "types_overlays.h"

#include "debug.h"
#include "anim.h"
#include "camera.h"
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
#include "rez_wire_compile.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "attack_ray.h"
#include "enemy_shatter.h"
#include "enemy_vars.h"
#include "plane_cut.h"
#include "ramses_circle.h"
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

/* ROT_* as plain blocks, without the do-while */
#define ROTB_Z(ang, sc, x, y)     {         float rot_u = x, rot_v = y;         ulSinCos(ang, &sc[0], &sc[1]);         x = rot_u * sc[1] - rot_v * sc[0];         y = rot_u * sc[0] + rot_v * sc[1];     }

#define ROTB_X(ang, sc, y, z)     {         float rot_u = y, rot_v = z;         ulSinCos(ang, &sc[0], &sc[1]);         y = rot_u * sc[1] - rot_v * sc[0];         z = rot_u * sc[0] + rot_v * sc[1];     }
#define ROTB_Y(ang, sc, x, z)     {         float rot_u = x, rot_v = z;         ulSinCos(ang, &sc[0], &sc[1]);         x = rot_u * sc[1] + rot_v * sc[0];         z = -rot_u * sc[0] + rot_v * sc[1];     }

extern int ramses_init_arg;
extern RamsesVars ramses_types[1];
extern VarDef ramses_vardef;
extern VarDef ramses_vardefs[49];
extern int *ramses_body_save;
extern void *ramses_halo_save;
void ramses_vars_register(void);
void ramses_vars_unregister(void);
void ramses_rotate_yxz(int *ang);
extern char ramses_lock_name[];
extern char ramses_name[];
extern ObjFuncs ramses_funcs;
int ramses_node_cb(unsigned int *node, int idx, RamsesWork *w);
int ramses_node_cb_first(unsigned int *node, int idx, RamsesWork *w);
void ramses_pose_model(void *obj, RamsesWork *w);
float ramses_part_motion_value(ModelPart *p);
void ramses_init(void *obj, RamsesWork *w, int prm, int *kind);
void ramses_release(void *obj, RamsesWork *w);
int ramses_next_state(void *obj, RamsesWork *w, int n, int state);
void ramses_shot_charge(void *obj, RamsesWork *w);
int ramses_create(void *parent, Placement *place);
void ramses_place_pose2(void *obj, RamsesWork *w);
void ramses_state_move(void *obj, RamsesWork *w);
float ramses_part_motion_scaled(float scl, ModelPart *p, Vec3 *pos, int *ang);
float ramses_apply_ring_motion(float scl, void *obj, RamsesWork *w);
extern void (*ramses_states[])();
extern void (*ramses_exec_table[])(void *o, RamsesWork *w);
extern void (*ramses_disp_table[])(void *o, RamsesWork *w);
void ramses_state_cruise(void *obj, RamsesWork *w);
void ramses_update(void *obj, RamsesWork *w);
void ramses_exec_appear(void *obj, RamsesWork *w);
void ramses_exec_normal(void *obj, RamsesWork *w);
void ramses_exec_leave(void *obj, RamsesWork *w);
void ramses_exec_destroyed(void *obj, RamsesWork *w);
void ramses_pre_exec(void *obj, RamsesWork *w);
void ramses_exec(void *obj, RamsesWork *w);
void ramses_disp(void *obj, RamsesWork *w);
extern Vec3 ramses_body_scale;
void ramses_post_exec(void *obj, RamsesWork *w);
void ramses_disp_ground(void *obj, RamsesWork *w);
void ramses_disp_body(void *obj, RamsesWork *w);
void ramses_state_break(void *obj, RamsesWork *w);
void ramses_state_shot(void *obj, RamsesWork *w);
void ramses_state_stop(void *obj, RamsesWork *w);
void ramses_state_back(void *obj, RamsesWork *w);
void ramses_state_accel(void *obj, RamsesWork *w);
void ramses_state_away(void *obj, RamsesWork *w);

/* dst = lhs - rhs */
static inline void v3_sub_from(Vec3 *rhs, Vec3 *lhs, Vec3 *dst)
{
    dst->x = lhs->x - rhs->x;
    dst->y = lhs->y - rhs->y;
    dst->z = lhs->z - rhs->z;
}

/* model list entry n as stg22 numbers it */
#if defined(OVERLAY_STG22)
#define RAMSES_MODEL(n) ((n) - 2)
#else
#define RAMSES_MODEL(n) (n)
#endif

/* 0x00b01db8 stg01, 0x00b09218 stg22 */
void ramses_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&ramses_vardef);
    for (i = 0; i < 1; i++) {
        def = ramses_vardefs;
        val = (int *)&ramses_types[i].p;
        ramses_vardef.name = ramses_types[i].name;
        ramses_vardef.fmt = ramses_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&ramses_vardef);
        for (j = 0; j < 49; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)ramses_vardefs);
    }
}

/* 0x00b01ed0 stg01, 0x00b09330 stg22 */
void ramses_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b01ed8 stg01, 0x00b09338 stg22 */
int ramses_get_init_arg(void)
{
    return ramses_init_arg;
}

/* 0x00b01ee8 stg01, 0x00b09348 stg22 */
int ramses_vars_init(int *prm)
{
    ramses_init_arg = *prm;
    ramses_vars_register();
    ramses_halo_save = 0;
    ramses_body_save = 0;
    return 0;
}

/* 0x00b01f20 stg01, 0x00b09380 stg22 */
int ramses_vars_release(void)
{
    if (ramses_body_save) {
        yuki_save_free(ramses_body_save);
        ramses_body_save = 0;
        yuki_save_free(ramses_halo_save);
        ramses_halo_save = 0;
    }
    ramses_vars_unregister();
    return 0;
}

/* 0x00b01f78 stg01, 0x00b093d8 stg22 */
int ramses_marker_draw(int obj, void *place)
{
    int *mdl;

    ulGxSetTextureList(stage_textures);
    mdl = pnb_get_entry(stage_models, RAMSES_MODEL(24));
    return obj_draw_at(obj, place, (void *)*mdl);
}

/* 0x00b01fd0 stg01, 0x00b09430 stg22 */
int ramses_node_cb(unsigned int *node, int idx, RamsesWork *w)
{
    if (*node & 0x2000000)
        ulMatrixGetTranslation((float *)ulMatrixCur, &w->tip.x);
    if (*node & 0x2000000)
        ulMatrixGetTranslation((float *)ulMatrixCur, &w->pos.x);
    if (*node & 0x1000000)
        ulMatrixGetTranslation((float *)ulMatrixCur, &w->halo_pos.x);
    return 0;
}

/* 0x00b02068 stg01, 0x00b094c8 stg22 */
int ramses_node_cb_first(unsigned int *node, int idx, RamsesWork *w)
{
    if (*node & 0x1000000)
        ulMatrixGetTranslation((float *)ulMatrixCur, &w->wing_pos.x);
    return 0;
}

/* 0x00b020a0 stg01, 0x00b09500 stg22 */
void ramses_pose_model(void *obj, RamsesWork *w)
{
    void *gfx = enemy_task_gfx_ptr(obj);

    dc_object_set_callback(gfx, ramses_node_cb_first, w);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &w->local_pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    ramses_rotate_yxz(w->local_ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(gfx);
    ulMatrixPop();
    dc_object_set_callback(gfx, 0, w);
    dc_object_set_callback(gfx, ramses_node_cb, w);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &w->pos2;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    ramses_rotate_yxz(w->ang2);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(gfx);
    ulMatrixPop();
    dc_object_set_callback(gfx, 0, w);
}

/* 0x00b021b8 stg01, 0x00b09618 stg22 */
float ramses_part_motion_value(ModelPart *p)
{
    void *out[3];
    float pos[4], ang[4], val[4];

    out[0] = pos;
    out[1] = ang;
    out[2] = val;
    anim_eval_hierarchy(p->root, p->clip, out, p->frame);
    return val[0];
}

/* 0x00b02200 stg01, 0x00b09660 stg22 */
#if defined(OVERLAY_STG22)
/* steer(cast) */
void ramses_init(void *obj, RamsesWork *w, int prm, int *kind)
{
    Vec3 v;
    RamsesVars *typ;
    float brk_end;

    typ = &ramses_types[*kind];
    w->type = typ;
    if (!ramses_body_save) {
        ramses_body_save = ((void *(*)(void *))yuki_save_create)((void *)*((int *(*)(void *, unsigned int))pnb_get_entry)(*(void **)obj, 22));
        ramses_halo_save = ((void *(*)(void *))yuki_save_create)((void *)*((int *(*)(void *, unsigned int))pnb_get_entry)(*(void **)obj, 24));
    }
    dc_object_init(&w->body, *((int *(*)(void *, unsigned int))pnb_get_entry)(*(void **)obj, 23),
                  *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, kind[2] == -1 ? 27 : 29), 0x500);
    dc_object_set_wire(&w->body, model_pnb_get_wire(*(void **)obj, 23));
    dc_object_init(&w->ring, *((int *(*)(void *, unsigned int))pnb_get_entry)(*(void **)obj, 19), *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, 21),
                  0x1000);
    dc_object_init(&w->wing, *((int *(*)(void *, unsigned int))pnb_get_entry)(*(void **)obj, 24), *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, 25),
                  0x1000);
    w->radius0 = ((ModelPart *)enemy_task_gfx_ptr(obj))->radius;
    w->wing_radius0 = w->wing.radius;
    w->wing.radius = ((ModelPart *)enemy_task_gfx_ptr(obj))->radius = w->body.radius = 120.0f;
    lockon_obj_init(&w->lock, obj, &w->pos, 0, ramses_lock_name, enemy_task_get_status_slot(obj), typ->p.hp,
                  typ->p.same_lock, 0);
    lockon_entry(&w->lock);
    enemy_task_set_status_pos(obj, &w->pos2);
    enemy_task_clear_flags(obj, 64);
    v3_set(&w->local_pos, 0.0f, 0.0f, 0.0f);
    i3_set(w->local_ang, 0, 0, 0);
    enemy_task_get_param_vec(obj, &v);
    w->speed = vec3_length(&v);
    v3_set(&w->dir, 0.0f, 1.0f, 0.0f);
    w->ground_y = w->radius0;
    w->shot_set = kind[2];
    w->rise = 1.0f;
    w->shot = 0;
    enemy_task_set_appear(obj, 2, 30);
    enemy_task_set_vanish(obj, 2, 60);
    v3_set(&w->sway, 0.0f, 0.0f, 0.0f);
    w->halo_scale = typ->p.backfire_scl;
    brk_end = (typ->p.break_sec + typ->p.attack_ray_sec_dif - typ->p.attack_ray_sec) * FPS;
    w->end_time = kind[1];
    i3_set(w->turn, 0, 0, 0);
    w->flag = w->shot_set == -1;
    w->end = brk_end + ((int)fabsf(enemy_task_get_param(obj, 0)) < 2 ? 1.0f
                  : (int)fabsf(enemy_task_get_param(obj, 0)));
}
#else
/* steer(cast) */
void ramses_init(void *obj, RamsesWork *w, int prm, int *kind)
{
    Vec3 v;
    RamsesVars *typ;
    float brk_end;

    typ = &ramses_types[*kind];
    w->type = typ;
    if (!ramses_body_save) {
        ramses_body_save = ((void *(*)(void *))yuki_save_create)((void *)*((int *(*)(void *, unsigned int))pnb_get_entry)(*(void **)obj, 24));
        ramses_halo_save = ((void *(*)(void *))yuki_save_create)((void *)*((int *(*)(void *, unsigned int))pnb_get_entry)(*(void **)obj, 26));
    }
    dc_object_init(&w->body, *((int *(*)(void *, unsigned int))pnb_get_entry)(*(void **)obj, 25),
                  *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, kind[2] == -1 ? 29 : 31), 0x500);
    dc_object_set_wire(&w->body, model_pnb_get_wire(*(void **)obj, 25));
    dc_object_init(&w->ring, *((int *(*)(void *, unsigned int))pnb_get_entry)(*(void **)obj, 22), *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, 23),
                  0x1000);
    dc_object_init(&w->wing, *((int *(*)(void *, unsigned int))pnb_get_entry)(*(void **)obj, 26), *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, 27),
                  0x1000);
    w->radius0 = ((ModelPart *)enemy_task_gfx_ptr(obj))->radius;
    w->wing_radius0 = w->wing.radius;
    w->wing.radius = ((ModelPart *)enemy_task_gfx_ptr(obj))->radius = w->body.radius = 120.0f;
    lockon_obj_init(&w->lock, obj, &w->pos, 0, ramses_lock_name, enemy_task_get_status_slot(obj), typ->p.hp,
                  typ->p.same_lock, 0);
    lockon_entry(&w->lock);
    enemy_task_set_status_pos(obj, &w->pos2);
    enemy_task_clear_flags(obj, 64);
    v3_set(&w->local_pos, 0.0f, 0.0f, 0.0f);
    i3_set(w->local_ang, 0, 0, 0);
    enemy_task_get_param_vec(obj, &v);
    w->speed = vec3_length(&v);
    v3_set(&w->dir, 0.0f, 1.0f, 0.0f);
    w->ground_y = w->radius0;
    w->shot_set = kind[2];
    w->rise = 1.0f;
    w->shot = 0;
    enemy_task_set_appear(obj, 2, 30);
    enemy_task_set_vanish(obj, 2, 60);
    v3_set(&w->sway, 0.0f, 0.0f, 0.0f);
    w->halo_scale = typ->p.backfire_scl;
    brk_end = (typ->p.break_sec + typ->p.attack_ray_sec_dif - typ->p.attack_ray_sec) * FPS;
    w->end_time = kind[1];
    i3_set(w->turn, 0, 0, 0);
    w->flag = w->shot_set == -1;
    w->end = brk_end + ((int)fabsf(enemy_task_get_param(obj, 0)) < 2 ? 1.0f
                  : (int)fabsf(enemy_task_get_param(obj, 0)));
}
#endif

/* 0x00b02570 stg01, 0x00b099d0 stg22 */
void ramses_release(void *obj, RamsesWork *w)
{
    dc_object_release(&w->wing);
    dc_object_release(&w->ring);
    dc_object_release(&w->body);
    lockon_remove(&w->lock);
}

/* 0x00b025b0 stg01, 0x00b09a10 stg22 */
int ramses_next_state(void *obj, RamsesWork *w, int n, int state)
{
    if (++w->frames > n) {
        w->state = state;
        w->frames = 1;
        i3_copy(w->ang0, w->local_ang);
        return 1;
    }
    return 0;
}

/* 0x00b02608 stg01, 0x00b09a68 stg22 */
/* steer(cast) */
void ramses_shot_charge(void *obj, RamsesWork *w)
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

/* 0x00b026b0 stg01, 0x00b09b10 stg22 */
void ramses_place_pose2(void *obj, RamsesWork *w)
{
    Vec3 p;
    int ang[4];
    float sc[3][2];
    float x, y, z;
    Vec3 *lpos;

    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    lpos = &w->local_pos;
    do {
        z = lpos->z;
        x = lpos->x;
        y = lpos->y;
    } while (0);
    ROT_Z(ang[2], sc[0], x, y);
    ROT_X(ang[0], sc[1], y, z);
    ROT_Y(ang[1], sc[2], x, z);
    v3_set(&w->pos2, x, y, z);
    v3_add(&w->pos2, &p);
    ulMatrixPush();
    matrix_unit_set(0);
    ramses_rotate_yxz(ang);
    ramses_rotate_yxz(w->local_ang);
    model_camera_angles(w->ang2);
    ulMatrixPop();
}

/* 0x00b02828 stg01, 0x00b09c88 stg22 */
void ramses_state_move(void *obj, RamsesWork *w)
{
    Vec3 v;
    int n;

    n = (int)fabsf(enemy_task_get_param(obj, 0)) < 2 ? 1 : (int)fabsf(enemy_task_get_param(obj, 0));
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, w->speed);
    v3_add(&w->local_pos, &v);
    ramses_shot_charge(obj, w);
    if (ramses_next_state(obj, w, n, 1))
        i3_copy(w->turn, w->local_ang);
}

/* 0x00b02940 stg01, 0x00b09da0 stg22 */
float ramses_part_motion_scaled(float scl, ModelPart *p, Vec3 *pos, int *ang)
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

/* 0x00b02a08 stg01, 0x00b09e68 stg22 */
float ramses_apply_ring_motion(float scl, void *obj, RamsesWork *w)
{
    Vec3 ofs;
    int rang[4];
    float sc[3][2];
    int *base = w->turn;

    dc_object_advance_time(&w->ring);
    scl = ramses_part_motion_scaled(scl, &w->ring, &ofs, rang);
    ROT_Z(base[2], sc[0], ofs.x, ofs.y);
    ROT_X(base[0], sc[1], ofs.y, ofs.z);
    ROT_Y(base[1], sc[2], ofs.x, ofs.z);
    v3_sub_from(&w->sway, &w->local_pos, &w->local_pos);
    v3_add(&w->local_pos, &ofs);
    v3_copy(&w->sway, &ofs);
    ulMatrixPush();
    matrix_unit_set(0);
    ramses_rotate_yxz(base);
    ramses_rotate_yxz(rang);
    model_camera_angles(w->local_ang);
    ulMatrixPop();
    return scl;
}

/* 0x00b02c08 stg01, 0x00b0a068 stg22 */
/* steer(cast) */
void ramses_state_break(void *obj, RamsesWork *w)
{
    union {
        int a[4];
        Vec3 v;
    } u;
    Vec3 aim_dir;
    Vec3 pl;
    Vec3 to_pl;
    int ang[4];
    int aim[4];
    float sc[14][2];
    RamsesVars *typ = w->type;
    ModelPart *gfx;
    float prev, rate, one, half_turn, quarter, half, spd, bscl, dis, x, y, z;
    float ring, dbrk;
    int n;

    n = typ->p.break_sec * FPS;
    rate = w->frames / (float)n;
    prev = (w->frames - 1) / (float)n;
    gfx = enemy_task_gfx_ptr(obj);
    if (w->shot_set != -1) {
        float x, y, z;

        x = 0.0f;
        y = 0.0f;
        ulSinCos((int)(safe_powf(rate, typ->p.break_ang_pow) * ANG_HALF - ANG_QUARTER), &sc[0][0],
                      &sc[0][1]);
        e_shot_initial_angles(w->aim, &w->pos2, 0, w->shot_set);
        z = -1.0f;
        ROT_X(w->aim[0], sc[1], y, z);
        ROT_Y(w->aim[1], sc[2], x, z);
        enemy_task_get_rot(obj, u.a);
        ROT_Y(-u.a[1], sc[3], x, z);
        ROT_X(-u.a[0], sc[4], y, z);
        ROT_Z(-u.a[2], sc[5], x, y);
        v3_set(&aim_dir, x, y, z);
        do {
            w->aim[1] = (int)RAD_TO_ANG(atan2f(-aim_dir.x, -aim_dir.z));
            w->aim[0] = (int)RAD_TO_ANG(atan2f(aim_dir.y, sqrtf(aim_dir.x * aim_dir.x + aim_dir.z * aim_dir.z)));
        } while (0);
        w->aim[2] = 0;
        yuki_lerp3(w->turn, w->ang0, w->aim, (sc[0][0] + 1.0f) * 0.5f);
    } else {
        float x, y, z;

        player_get_pos(&pl);
        enemy_task_get_rot(obj, ang);
        v3_sub(&to_pl, &pl, &w->pos2);
        do {
            Vec3 *v_ = &to_pl;

            x = v_->x;
            y = v_->y;
            z = v_->z;
        } while (0);
        ROT_Y(-ang[1], sc[6], x, z);
        ROT_X(-ang[0], sc[7], y, z);
        ROT_Z(-ang[2], sc[8], x, y);
        v3_set(&to_pl, x, y, z);
        do {
            aim[1] = (int)RAD_TO_ANG(atan2f(-to_pl.x, -to_pl.z));
            aim[0] = (int)RAD_TO_ANG(atan2f(to_pl.y, sqrtf(to_pl.x * to_pl.x + to_pl.z * to_pl.z)));
        } while (0);
        aim[2] = 0;
        ulSinCos((int)(safe_powf(rate, typ->p.break_ang_pow) * ANG_HALF - ANG_QUARTER), &sc[0][0],
                      &sc[0][1]);
        yuki_lerp3(w->turn, w->ang0, aim, (sc[0][0] + 1.0f) * 0.5f);
    }
    if (w->shot_set != -1)
        gfx->frame = 57.0f * rate;
    else
        gfx->frame = gfx->frames * rate;
    y = 1.0f;
    x = 0.0f;
    spd = w->speed * FPS;
    spd = -spd;
    dbrk = spd / (typ->p.break_pow + y) * typ->p.break_sec * (safe_powf(y - rate, typ->p.break_pow + y) - y);
    dbrk -= spd / (typ->p.break_pow + y) * typ->p.break_sec * (safe_powf(y - prev, typ->p.break_pow + y) - y);
    v3_mul(&u.v, &w->dir, dbrk);
    v3_add(&w->local_pos, &u.v);
    z = 0.0f;
    ROT_X(w->turn[0], sc[9], y, z);
    ROT_Y(w->turn[1], sc[10], x, z);
    v3_set(&u.v, x, y, z);
    half_turn = ANG_HALF;
    quarter = ANG_QUARTER;
    one = 1.0f;
    half = 0.5f;
    ulSinCos((int)(safe_powf(rate, typ->p.break_dis_pow) * half_turn - quarter), &sc[11][0],
                  &sc[11][1]);
    dis = (sc[11][0] + one) * half;
    ulSinCos((int)(safe_powf(prev, typ->p.break_dis_pow) * half_turn - quarter), &sc[11][0],
                  &sc[11][1]);
    dis -= (sc[11][0] + one) * half;
    v3_scale(&u.v, typ->p.break_dis * dis);
    v3_add(&w->local_pos, &u.v);
    ulSinCos((int)(safe_powf(rate, typ->p.stop_swing_pow) * half_turn - quarter), &sc[12][0],
                  &sc[12][1]);
    sc[12][0] = (sc[12][0] + one) * half * typ->p.stop_swing_scl;
    ring = ramses_apply_ring_motion(sc[12][0], obj, w);
    ramses_shot_charge(obj, w);
    ulSinCos((int)(safe_powf(rate, typ->p.break_backfire_pow2) * half_turn - quarter),
                  &sc[13][0], &sc[13][1]);
    sc[13][0] = (sc[13][0] + one) * half;
    bscl = typ->p.backfire_scl * (one - sc[13][0]) + typ->p.break_backfire_scl * sc[13][0];
    ulSinCos((int)(safe_powf(rate, typ->p.break_backfire_pow) * ANG_TURN + quarter),
                  &sc[13][0], &sc[13][1]);
    sc[13][0] = (sc[13][0] + one) * half;
    w->halo_scale = bscl * sc[13][0] * ring;
    if (w->shot_set == -1) {
        if (ramses_next_state(obj, w, n, 3)) {
            dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, RAMSES_MODEL(34))));
            gfx->frame = 0.0f;
            dc_object_set_clip(&w->body, *((struct DcClip **)pnb_get_entry(stage_models, RAMSES_MODEL(35))));
        }
    } else if (ramses_next_state(obj, w, n, 2)) {
        w->shot = 1;
    }
}

/* 0x00b03528 stg01, 0x00b0a988 stg22 */
/* steer(cast) */
void ramses_state_shot(void *obj, RamsesWork *w)
{
    RamsesVars *typ = w->type;
    float sc[2];
    ModelPart *gfx;
    float rate, ring, bf_rate;
    int n, bf_end;

    n = typ->p.shot_sec * FPS;
    rate = w->frames / (float)n;
    gfx = enemy_task_gfx_ptr(obj);
    ring = ramses_apply_ring_motion(typ->p.stop_swing_scl, obj, w);
    gfx->frame = (gfx->frames - 57.0f) * rate + 57.0f;
    bf_end = (int)fabsf(enemy_task_get_param(obj, 1)) < 2 ? 1 : (int)fabsf(enemy_task_get_param(obj, 1));
    bf_end = MIN(typ->p.stop_backfire_sec * FPS, bf_end + n);
    bf_rate = w->frames / (float)bf_end;
    bf_rate = MIN(bf_rate >= 0.0f ? bf_rate : 0.0f, 1.0f);
    ulSinCos((int)(safe_powf(bf_rate, typ->p.stop_backfire_pow) * ANG_HALF - ANG_QUARTER), &sc[0],
             &sc[1]);
    sc[0] = (sc[0] + 1.0f) * 0.5f;
    w->halo_scale = (typ->p.backfire_scl * sc[0] + typ->p.break_backfire_scl * (1.0f - sc[0])) * ring;
    if (ramses_next_state(obj, w, n, 3)) {
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, RAMSES_MODEL(32))));
        gfx->frame = 0.0f;
        dc_object_set_clip(&w->body, *((struct DcClip **)pnb_get_entry(stage_models, RAMSES_MODEL(33))));
    }
}

/* 0x00b03780 stg01, 0x00b0abe0 stg22 */
void ramses_state_stop(void *obj, RamsesWork *w)
{
    RamsesVars *typ;
    float sc[2];
    float rate, ring, bf_rate, shot_end;
    int end, bf_end;

    if ((int)fabsf(enemy_task_get_param(obj, 1)) < 2)
        end = 1;
    else
        end = (int)fabsf(enemy_task_get_param(obj, 1));
    rate = w->frames / (float)end;
    typ = w->type;
    ring = ramses_apply_ring_motion(typ->p.stop_swing_scl, obj, w);
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
    w->halo_scale = (typ->p.backfire_scl * sc[0] + typ->p.break_backfire_scl * (1.0f - sc[0])) * ring;
    if (ramses_next_state(obj, w, end, 4))
        i3_copy(w->ang0, w->turn);
}

/* 0x00b03a18 stg01, 0x00b0ae78 stg22 */
void ramses_state_back(void *obj, RamsesWork *w)
{
    int zero_ang[4];
    Vec3 v;
    float sc[6][2];
    RamsesVars *typ = w->type;
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
    x = 0.0f;
    ulSinCos((int)(safe_powf(rate, typ->p.accel_ang_pow) * half_turn - quarter), &sc[0][0],
                  &sc[0][1]);
    acrv = (sc[0][0] + one) * half;
    ulSinCos((int)(safe_powf(rate, typ->p.accel_ang_pow2) * ANG_TURN - quarter), &sc[0][0],
                  &sc[0][1]);
    i3_set(zero_ang, 0, 0, 0);
    yuki_lerp3(w->turn, w->ang0, zero_ang,
               acrv + (sc[0][0] + one) * half * typ->p.accel_ang_over_rate);
    z = 0.0f;
    y = -1.0f;
    ROT_X(w->turn[0], sc[1], y, z);
    ROT_Y(w->turn[1], sc[2], x, z);
    v3_set(&v, x, y, z);
    ulSinCos((int)(safe_powf(rate, typ->p.back_pow) * half_turn - quarter), &sc[3][0], &sc[3][1]);
    crv = (sc[3][0] + one) * half;
    ulSinCos((int)(safe_powf(prev, typ->p.back_pow) * half_turn - quarter), &sc[3][0], &sc[3][1]);
    dis = (crv - (sc[3][0] + one) * half) * typ->p.back_dis;
    v3_scale(&v, dis);
    v3_add(&w->local_pos, &v);
    ulSinCos((int)(safe_powf(rate, typ->p.stop_swing_pow) * half_turn + quarter), &sc[4][0],
                  &sc[4][1]);
    sc[4][0] = (sc[4][0] + one) * half * typ->p.stop_swing_scl;
    ramses_apply_ring_motion(sc[4][0], obj, w);
    gfx = enemy_task_gfx_ptr(obj);
    gfx->frame = rate * 49.0f;
    ulSinCos((int)(safe_powf(rate, typ->p.back_backfire_pow) * half_turn - quarter), &sc[5][0],
                  &sc[5][1]);
    sc[5][0] = (sc[5][0] + one) * half;
    w->halo_scale = typ->p.backfire_scl * (one - sc[5][0]) + sc[5][0] * typ->p.back_backfire_scl;
    w->halo_scale = w->halo_scale * ramses_part_motion_value(&w->ring);
    ramses_next_state(obj, w, n, 5);
}

/* 0x00b03de8 stg01, 0x00b0b248 stg22 */
void ramses_state_accel(void *obj, RamsesWork *w)
{
    Vec3 step;
    float sc[2];
    RamsesVars *typ = w->type;
    float rate, prev, spd, dis;
    int n;

    n = typ->p.accel_sec * FPS;
    rate = w->frames / (float)n;
    prev = (w->frames - 1) / (float)n;
    spd = w->speed * FPS;
    dis = spd / (typ->p.accel_pow + 1.0f) * typ->p.accel_sec * (typ->p.accel_spd_over_rate + 1.0f)
        * safe_powf(rate, typ->p.accel_pow + 1.0f);
    dis -= spd / (typ->p.accel_pow + 1.0f) * typ->p.accel_sec * (typ->p.accel_spd_over_rate + 1.0f)
         * safe_powf(prev, typ->p.accel_pow + 1.0f);
    v3_mul(&step, &w->dir, dis);
    v3_add(&w->local_pos, &step);
    dc_object_advance_time(enemy_task_gfx_ptr(obj));
    ulSinCos((int)(safe_powf(rate, typ->p.accel_backfire_pow) * ANG_HALF - ANG_QUARTER), &sc[0],
             &sc[1]);
    sc[0] = (sc[0] + 1.0f) * 0.5f;
    w->halo_scale = typ->p.accel_backfire_scl * (1.0f - sc[0]) + sc[0] * typ->p.backfire_scl;
    w->halo_scale = w->halo_scale * ramses_part_motion_value(&w->ring);
    ramses_next_state(obj, w, n, 6);
}

/* 0x00b04018 stg01, 0x00b0b478 stg22 */
void ramses_state_away(void *obj, RamsesWork *w)
{
    Vec3 step;
    float sspd, espd, dis;
    int n;

    sspd = w->speed * FPS;
    n = w->type->p.away_sec * FPS;
    espd = w->type->p.accel_spd_over_rate * sspd + sspd;
    dis = ((sspd - espd) * (w->frames / (float)n) * 0.5f + espd) * (w->frames / FPS)
        - ((sspd - espd) * ((w->frames - 1) / (float)n) * 0.5f + espd) * ((w->frames - 1) / FPS);
    v3_mul(&step, &w->dir, dis);
    v3_add(&w->local_pos, &step);
    dc_object_advance_time(enemy_task_gfx_ptr(obj));
    ramses_next_state(obj, w, n, 7);
}

/* 0x00b04170 stg01, 0x00b0b5d0 stg22 */
void ramses_state_cruise(void *obj, RamsesWork *w)
{
    Vec3 step;

    v3_mul(&step, &w->dir, w->speed);
    v3_add(&w->local_pos, &step);
    dc_object_advance_time(enemy_task_gfx_ptr(obj));
}

/* 0x00b041e8 stg01, 0x00b0b648 stg22 */
void ramses_update(void *obj, RamsesWork *w)
{
    Vec3 dir;
    float sc[2][2];
    float x, y, z;

    if (ramses_states[w->state])
        ramses_states[w->state]();  /* the handler receives obj and w as passed in here */
    dc_object_advance_time(&w->wing);
    ramses_place_pose2(obj, w);
    ramses_pose_model(obj, w);
    if (w->shot) {
        x = 0.0f;
        y = 0.0f;
        z = -1.0f;
        ROT_X(w->ang2[0], sc[0], y, z);
        ROT_Y(w->ang2[1], sc[1], x, z);
        v3_set(&dir, x, y, z);
        e_shot_fire(obj, &w->tip, &dir, w->shot_set);
        w->shot = 0;
    }
}

/* 0x00b04308 stg01, 0x00b0b768 stg22 */
void ramses_exec_appear(void *obj, RamsesWork *w)
{
    RingArg circ;
    Vec3 p;
    Vec3 ofs;
    int ang[4];
    float sc[3][2];
    float rad = w->radius0;
    float rise, x, y, z;
    int st;

    st = enemy_task_get_phase(obj);
    if (!st)
        return;
    if (st == 2) {
        w->lock.flags |= LOCK_LOCKABLE;
        return;
    }
    ramses_update(obj, w);
    rise = w->local_pos.y / (rad + rad);
    w->rise = rise;
    if (rise < 0.0f) {
        w->rise = 0.0f;
    } else if (1.0f < rise) {
        enemy_task_set_state(obj, 0);
        w->rise = 1.0f;
        dc_object_rebuild(enemy_task_gfx_ptr(obj));
        dc_object_rebuild(&w->wing);
    }
    if (w->count++ % 9 == 0 && 0.1f < w->rise && w->rise < 0.5f) {
        heap_bzero(&circ, 8);
        enemy_task_get_pos(obj, &p);
        enemy_task_get_rot(obj, ang);
        x = 0.0f;
        y = rad;
        z = 0.0f;
        ROT_Z(ang[2], sc[0], x, y);
        ROT_X(ang[0], sc[1], y, z);
        ROT_Y(ang[1], sc[2], x, z);
        v3_set(&ofs, x, y, z);
        v3_add(&p, &ofs);
        circ.pos = &p;
        circ.ang = ang;
        ramses_circle_create(obj, &circ);
    }
}

/* 0x00b04560 stg01, 0x00b0b9c0 stg22 */
void ramses_exec_normal(void *obj, RamsesWork *w)
{
    float frm;

    if (enemy_task_get_phase(obj) != 1)
        return;
    ramses_update(obj, w);
    frm = enemy_task_get_time(obj);
    if (w->end_time < frm) {
        enemy_task_set_state(obj, 2);
        dc_object_free_handles(enemy_task_gfx_ptr(obj));
        dc_object_free_handles(&w->wing);
        w->ground_y = w->local_pos.y + w->radius0;
    }
}

/* 0x00b04600 stg01, 0x00b0ba60 stg22 */
void ramses_exec_leave(void *obj, RamsesWork *w)
{
    RingArg circ;
    Vec3 p;
    Vec3 ofs;
    int ang[4];
    float sc[3][2];
    float rad = w->radius0;
    float rise, x, y, z;
    int st;

    st = enemy_task_get_phase(obj);
    if (!st) {
        y = w->ground_y;
        z = w->local_pos.z;
        x = w->local_pos.x;
        heap_bzero(&circ, 8);
        enemy_task_get_pos(obj, &p);
        enemy_task_get_rot(obj, ang);
        ROTB_Z(ang[2], sc[0], x, y);
        ROTB_X(ang[0], sc[1], y, z);
        ROTB_Y(ang[1], sc[2], x, z);
        v3_set(&ofs, x, y, z);
        v3_add(&p, &ofs);
        circ.pos = &p;
        circ.ang = ang;
        ramses_circle_create(obj, &circ);
        w->lock.flags &= ~LOCK_LOCKABLE;
        return;
    }
    if (st == 2)
        return;
    ramses_update(obj, w);
    rise = (w->ground_y - w->local_pos.y + rad) / (rad + rad);
    w->rise = rise;
    rise = rise < 1.0f ? rise : 1.0f;
    w->rise = rise;
    if (1.0f < rise) {
        w->rise = 1.0f;
    } else if (rise < 0.0f) {
        FINISH_IF(obj);
        w->rise = 0.0f;
    }
}

/* 0x00b04828 stg01, 0x00b0bc88 stg22 */
void ramses_exec_destroyed(void *obj, RamsesWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 0) {
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = RAMSES_MODEL(36);
        desc.models = stage_models;
        desc.lock = &w->lock;
        desc.kind = 0;
        ramses_place_pose2(obj, w);
        v3_copy(&desc.pos, &w->pos2);
        i3_copy(desc.ang, w->ang2);
        enemy_task_get_scl(obj, &desc.scale);
        enemy_task_get_param_vec(obj, &desc.v);
        enemy_shatter_create(obj, &desc, 0, 0);
        FINISH_IF(obj);
    }
}

/* 0x00b04910 stg01, 0x00b0bd70 stg22 */
void ramses_pre_exec(void *obj, RamsesWork *w)
{
    Vec3 org;

    player_get_frame_vel(&org);
    v3_add(&w->pos2, &org);
}

/* 0x00b04970 stg01, 0x00b0bdd0 stg22 */
void ramses_exec(void *obj, RamsesWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (ramses_exec_table[i])
        ramses_exec_table[i](obj, w);
}

/* 0x00b049c8 stg01, 0x00b0be28 stg22 */
/* steer(cast) */
void ramses_post_exec(void *obj, RamsesWork *w)
{
    RamsesVars *typ = w->type;
    LockObj *lock = &w->lock;

    enemy_task_get_scl(obj, &w->scale);
    v3_scale(&w->scale, typ->p.attack_ray_scl);
    if (!enemy_status_get_ticks()) {
        ramses_place_pose2(obj, w);
        ramses_pose_model(obj, w);
    }
    if (lock->flags & LOCK_HIT) {
        v3_copy(&w->hitv, &lock->imp.v);
        if (lock->life <= 0 && ((unsigned int)enemy_task_get_pstate(obj)) < 2) {
            int k = lockon_hits_max_shot_cnt(lock);

            enemy_status_score_popup(enemy_task_get_status_slot(obj), &w->pos2, typ->p.score, k);
            enemy_task_set_state(obj, 3);
        }
    }
}

/* 0x00b04b08 stg01, 0x00b0bf68 stg22 */
/* steer(cast) */
void ramses_disp_ground(void *obj, RamsesWork *w)
{
    Vec3 pos;
    Vec3 eye;
    int ang[4];
    Vec3 scl;
    Vec3 to_eye;
    float sc[3][2];
    ModelPart *gfx;
    int prev, ry;

    gfx = enemy_task_gfx_ptr(obj);
    if (view_sphere_visible(&w->pos2, gfx->radius)) {
        scl = ramses_body_scale;
        prev = draw_set_override_flags(DRAW_STRIP_OVERRIDE);
        draw_push_strip_flags();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xfffffeff, 0x800);
        ulGxSetTextureList(((void **)obj)[1]);
        enemy_task_get_pos(obj, &pos);
        enemy_task_get_rot(obj, ang);
        plane_cut_draw_part(gfx, ramses_body_save, 0x80000000, &w->local_pos, w->local_ang, &scl, 0x8000, &pos,
                             ang, enemy_task_get_pstate(obj) == 1, w->ground_y);
        camera_get_eye(&eye);
        v3_sub(&to_eye, &eye, &w->halo_pos);
        ROT_Y(-w->ang2[1], sc[0], to_eye.x, to_eye.z);
        ROT_X(-w->ang2[0], sc[1], to_eye.y, to_eye.z);
        ROT_Z(-w->ang2[2], sc[2], to_eye.x, to_eye.y);
        v3_set(&scl, w->halo_scale, w->halo_scale, 1.0f);
        ry = (int)RAD_TO_ANG(atan2f(to_eye.x, to_eye.z));
        plane_cut_draw_part(&w->wing, ramses_halo_save, 0x80000000, &w->wing_pos, w->local_ang, &scl, ry,
                             &pos, ang, enemy_task_get_pstate(obj) == 1, w->ground_y);
        draw_pop_strip_flags();
        draw_set_override_flags(prev);
    }
}

/* 0x00b04de8 stg01, 0x00b0c248 stg22 */
void ramses_disp_body(void *obj, RamsesWork *w)
{
    Vec3 eye;
    Vec3 to_eye;
    int ang[4];
    float sc[3][2];
    ModelPart *gfx;
    RamsesVars *typ;
    Vec3 *p;
    int ry;

    gfx = enemy_task_gfx_ptr(obj);
    if (view_sphere_visible(&w->pos2, gfx->radius)) {
        typ = w->type;
        ulGxSetTextureList(((void **)obj)[1]);
        ulMatrixPush();
        do {
            p = &w->pos2;
            vu0_matrix_translate(ulMatrixCur, p->x, p->y, p->z);
        } while (0);
        ramses_rotate_yxz(w->ang2);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        dc_object_draw(gfx);
        if (typ->p.wire) {
            draw_set_color(yuki_beat_color(typ->p.wire_col, typ->p.wire_col2, typ->p.wire_beat,
                                          typ->p.wire_linear, w->rise, typ->p.wire_near,
                                          typ->p.wire_far, typ->p.wire_near_rate,
                                          typ->p.wire_far_rate));
            draw_set_blend(4, 5);
            model_object_draw(&w->body, gfx->frame);
        }
        ulMatrixPop();
        p = &w->halo_pos;
        camera_get_eye(&eye);
        v3_sub(&to_eye, &eye, p);
        ROT_Y(-w->ang2[1], sc[0], to_eye.x, to_eye.z);
        ROT_X(-w->ang2[0], sc[1], to_eye.y, to_eye.z);
        ROT_Z(-w->ang2[2], sc[2], to_eye.x, to_eye.y);
        i3_copy(ang, w->ang2);
        ry = (int)RAD_TO_ANG(atan2f(to_eye.x, to_eye.z));
        do { ulMatrixPush(); } while (0);
        vu0_matrix_translate(ulMatrixCur, p->x, p->y, p->z);
        ramses_rotate_yxz(ang);
        vu0_matrix_rotate_y(ulMatrixCur, ry);
        vu0_matrix_scale(ulMatrixCur, w->halo_scale, w->halo_scale, 1.0f);
        dc_object_draw(&w->wing);
        ulMatrixPop();
    }
}

/* 0x00b050b8 stg01, 0x00b0c518 stg22 */
void ramses_disp(void *obj, RamsesWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (ramses_disp_table[i])
        ramses_disp_table[i](obj, w);
}

/* 0x00b05110 stg01, 0x00b0c570 stg22 */
#if defined(OVERLAY_STG22)
/* steer(cast) */
int ramses_create(void *parent, Placement *place)
{
    int kind[3];
    ObjDesc desc;
    ObjFuncs funcs = ramses_funcs;
    RamsesVars *typ;
    int n;
    float ms = 1000.0f, half = 0.5f;

    ((float (*)(float))float_frac)(place->param0);
    typ = ramses_types;
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
    desc.name = ramses_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.kind = kind;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->p.hp;
    desc.same_lock = typ->p.same_lock;
    desc.flags = 2;
    desc.dc_flags = 0;
    desc.model_no = 22;
    desc.model_no2 = kind[2] == -1 ? 26 : 28;
    desc.score = typ->p.score;
    desc.hp_reduce = typ->p.hp_reduce;
    desc.crash_r = typ->p.crash_r;
    desc.crash_damage = typ->p.crash_damage;
    desc.time = kind[1];
    desc.size = 580;
    return enemy_task_task_create(parent, &desc);
}
#else
/* steer(cast) */
int ramses_create(void *parent, Placement *place)
{
    int kind[3];
    ObjDesc desc;
    ObjFuncs funcs = ramses_funcs;
    RamsesVars *typ;
    int n;
    float ms = 1000.0f, half = 0.5f;

    ((float (*)(float))float_frac)(place->param0);
    typ = ramses_types;
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
    desc.name = ramses_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.kind = kind;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->p.hp;
    desc.same_lock = typ->p.same_lock;
    desc.flags = 2;
    desc.dc_flags = 0;
    desc.model_no = 24;
    desc.model_no2 = kind[2] == -1 ? 28 : 30;
    desc.score = typ->p.score;
    desc.hp_reduce = typ->p.hp_reduce;
    desc.crash_r = typ->p.crash_r;
    desc.crash_damage = typ->p.crash_damage;
    desc.time = kind[1];
    desc.size = 580;
    return enemy_task_task_create(parent, &desc);
}
#endif

/* 0x00b05328 stg01, 0x00b0c788 stg22 */
void ramses_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
