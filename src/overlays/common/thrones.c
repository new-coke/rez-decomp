/*
 * Thrones (stg05): an armoured enemy on a path, ring segments turning in staggered pairs to aim
 * on the beat; launches four cores (thrones_core). Killed, it sheds its armour and falls away
 * spinning. A Thrones placed without a wait is made as a thrones_child.
 */

#include "types_common.h"

#include "types_overlays.h"
#include "debug.h"
#include "approach.h"
#include "camera.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "math_util.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_midise.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "../stg22/e_shot.h"
#include "en_shatter.h"
#include "en_thrones_armor_die.h"
#include "enemy_vars.h"
#include "thrones_child.h"
#include "thrones_core.h"
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

/* ROT_* with the sine and cosine in two scalars */
#define ROTS_Z(ang, s, c, x, y)     do {         float rot_u = x, rot_v = y;         ulSinCos(ang, &s, &c);         x = rot_u * c - rot_v * s;         y = rot_u * s + rot_v * c;     } while (0)

#define ROTS_X(ang, s, c, y, z)     do {         float rot_u = y, rot_v = z;         ulSinCos(ang, &s, &c);         y = rot_u * c - rot_v * s;         z = rot_u * s + rot_v * c;     } while (0)
#define ROTS_Y(ang, s, c, x, z)     do {         float rot_u = x, rot_v = z;         ulSinCos(ang, &s, &c);         x = rot_u * c + rot_v * s;         z = -rot_u * s + rot_v * c;     } while (0)

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

static inline void i3_add(int *dst, int *src)
{
    dst[0] += src[0];
    dst[1] += src[1];
    dst[2] += src[2];
}

extern int ulMatrixPush(void);
extern struct GwVarNode *gw_varedit_root;

#define MAX_GE(a, b) ((a) >= (b) ? (a) : (b))

extern void *stage_textures;
extern void *stage_models;

int map_get_current(void);

extern float *matrix_from_axis_angle(float *m, Vec3 *a, int angle);
extern int thrones_init_arg;
extern void (*thrones_disp_table[])(void *o, TypeEnemyWork *w);
void thrones_vars_register(void);
void thrones_vars_unregister(void);
int thrones_vars_init(int *prm);
int thrones_vars_release(void);
int thrones_next_state(void *obj, TypeEnemyWork *w, int n, int state);
void thrones_disp(void *obj, TypeEnemyWork *w);
void thrones_rotate_yxz(int *ang);
extern struct TaskDesc en_armor_disp_desc;

/* model position and angles filled by thrones_root_node_cb */
typedef struct ThronesNodeCam {
    char pad_00[28];
    Vec3 pos;                   /* 0x1c */
    int ang[3];                 /* 0x28 */
} ThronesNodeCam;

typedef struct ThronesPoseCur {
    ThronesArm *arm;
    ThronesSlot *slot;
    ThronesRing *ring;
    ThronesWork *w;
} ThronesPoseCur;

extern void (*thrones_states[])(void *o, TypeEnemyWork *w);
void thrones_pose_model(void *obj, TypeEnemyWork *w);
void en_armor_disp_disp(void *w);
extern ThronesSet thrones_types[1];
extern VarDef thrones_vardef;
extern VarDef thrones_vardefs[30];
extern void (*thrones_exec_table[])(void *o, TypeEnemyWork *w);
float thrones_swing_value(ThronesSwing *swing);
void thrones_follow_path(void *obj, ThronesChildWork *w);
int thrones_root_node_cb(unsigned int *n, int idx, ThronesNodeCam *w);

extern int thrones_piece_break_types[];
extern char thrones_name[];
extern char thrones_funcs[];
extern int thrones_piece_models[];
extern int thrones_piece_wire_models[];
extern int thrones_seg_models[][4];
extern Vec3 thrones_init_scale;

/* model list entry n as stg22 numbers it */
#if defined(OVERLAY_STG22)
#define THRONES_MODEL(n) ((n) - 46)
#else
#define THRONES_MODEL(n) (n)
#endif

#if defined(OVERLAY_STG22)
#endif

/* 0x00b13128 stg05, 0x00b0dde8 stg22 */
void thrones_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&thrones_vardef);
    for (i = 0; i < 1; i++) {
        def = thrones_vardefs;
        val = thrones_types[i].v;
        thrones_vardef.name = thrones_types[i].name;
        thrones_vardef.fmt = thrones_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&thrones_vardef);
        for (j = 0; j < 30; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)thrones_vardefs);
    }
}

/* 0x00b13240 stg05, 0x00b0df00 stg22 */
void thrones_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b13248 stg05, 0x00b0df08 stg22 */
int thrones_get_init_arg(void)
{
    return thrones_init_arg;
}

/* 0x00b13258 stg05, 0x00b0df18 stg22 */
int thrones_vars_init(int *prm)
{
    thrones_init_arg = *prm;
    thrones_vars_register();
    return 0;
}

/* 0x00b13280 stg05, 0x00b0df40 stg22 */
int thrones_vars_release(void)
{
    thrones_vars_unregister();
    return 0;
}

/* 0x00b132a0 stg05, 0x00b0df60 stg22 */
int thrones_marker_draw(int obj, float *place)
{
    int *mdl;

    ulGxSetTextureList(stage_textures);
    if ((int)place[15] <= 0)
        return thrones_child_marker_draw(obj, place);
    mdl = pnb_get_entry(stage_models, THRONES_MODEL(120));
    return obj_draw_at(obj, place, (void *)*mdl);
}

/* 0x00b13320 stg05, 0x00b0dfe0 stg22 */
int thrones_node_cb(unsigned int *node, int idx, ThronesPoseCur *cur)
{
    if (node[0] & 0x1000000) {
        ThronesArm *arm = cur->arm;

        ulMatrixGetTranslation((float *)ulMatrixCur, (float *)&arm->pos);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        model_camera_angles(arm->ang);
        vu0_matrix_rotate_y(ulMatrixCur, -ANG_180);
        cur->arm++;
    }
    if (node[0] & 0x40000000) {
        ThronesSlot *slot = cur->slot;

        ulMatrixGetTranslation((float *)ulMatrixCur, (float *)&slot->pos);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        model_camera_angles(slot->ang);
        vu0_matrix_rotate_y(ulMatrixCur, -ANG_180);
        cur->slot++;
    }
    if (node[0] & 0x80000000) {
        ThronesRing *ring = cur->ring;

        ulMatrixGetTranslation((float *)ulMatrixCur, (float *)&ring->pos);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        model_camera_angles(ring->cur);
        vu0_matrix_rotate_y(ulMatrixCur, -ANG_180);
        cur->ring++;
    }
    return 0;
}

/* 0x00b13450 stg05, 0x00b0e110 stg22 */
void thrones_pose_model(void *obj, TypeEnemyWork *work)
{
    ThronesWork *w = (ThronesWork *)work;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    ThronesPoseCur cur;
    Vec3 *p;

    cur.arm = w->arm;
    cur.slot = w->x;
    cur.ring = w->y;
    cur.w = w;
    dc_object_set_callback(gfx, thrones_node_cb, &cur);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        p = &w->pos;
        vu0_matrix_translate(ulMatrixCur, p->x, p->y, p->z);
    } while (0);
    thrones_rotate_yxz(w->ang);
    dc_object_draw(gfx);
    ulMatrixPop();
    dc_object_set_callback(gfx, 0, 0);
}

/* 0x00b13508 stg05, 0x00b0e1c8 stg22 */
void thrones_swing_clear(void *p)
{
    heap_bzero(p, 28);
}

/* 0x00b13510 stg05, 0x00b0e1d0 stg22 */
void thrones_swing_release(ThronesSwing *p)
{
}

/* 0x00b13518 stg05, 0x00b0e1d8 stg22 */
float thrones_swing_value(ThronesSwing *swing)
{
    float sc[2];
    float rate, val;

    rate = MIN(MAX_GE((float)swing->cnt / (float)swing->total, 0.0f), 1.0f);
    val = swing->val * rate;
    ulSinCos((int)(swing->ang * (1.0f - rate)), &sc[0], &sc[1]);
    return val + -rate * sc[0] - swing->base;
}

/* 0x00b135f0 stg05, 0x00b0e2b0 stg22 */
void thrones_count_down(int *n)
{
    int v = *n - 1;

    if (v < 0)
        v = 0;
    *n = v;
}

/* 0x00b13610 stg05, 0x00b0e2d0 stg22 */
void thrones_swing_reset(ThronesSwing *swing, int n, float rate, float scale, int ang)
{
    float val = thrones_swing_value(swing);

    swing->ang = ang;
    swing->cnt = swing->total = n;
    swing->a = rate;
    swing->b = scale;
    swing->val = val;
}

/* 0x00b13680 stg05, 0x00b0e340 stg22 */
/* steer(cast) */
void thrones_follow_path_circle(void *obj, ThronesChildWork *w)
{
    Vec3 p;
    Vec3 v;
    float sc[2];
    float time, x, y, z, rad;
    float full, tau, px;

    time = ((float (*)(float))float_frac)(enemy_task_get_param(obj, 0)) * 1000.0f * 60.0f / 60.0f;
    x = enemy_task_get_param(obj, 2);
    y = enemy_task_get_param(obj, 3);
    z = enemy_task_get_param(obj, 4);
    rad = sqrtf(x * x + y * y);
    tau = TWO_PI;
    px = w->pos.x;
    full = ANG_TURN;
    {
        int ang = px * full / (rad * tau);
        ROT_Z(ang, sc, x, y);
    }
    v.x = x;
    v.y = y;
    v.z = 0;
    vec3_normalize(&v, &v);
    v3_scale(&v, w->pos.y - thrones_swing_value(&((ThronesWork *)w)->swing));
    x += v.x;
    y += v.y;
    player_pass_pos_b(&w->pos, time);
    w->pos.x += x;
    w->pos.y += y;
    w->pos.z += z;
    i3_set(w->ang, 0, 0, (int)(atan2f(-x, y) * full / tau) + ANG_180);
    ulMatrixPush();
    matrix_unit_set(0);
    thrones_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    model_camera_angles(w->ang);
    ulMatrixPop();
    enemy_task_get_pos(obj, &p);
    w->pos.x = w->pos.x * w->blend + p.x * (1.0f - w->blend);
    w->pos.y = w->pos.y * w->blend + p.y * (1.0f - w->blend);
    w->pos.z = w->pos.z * w->blend + p.z * (1.0f - w->blend);
}

/* 0x00b13930 stg05, 0x00b0e5f0 stg22 */
int thrones_root_node_cb(unsigned int *n, int idx, ThronesNodeCam *w)
{
    ulMatrixGetTranslation((float *)ulMatrixCur, &w->pos.x);
    model_camera_angles(w->ang);
    return 0;
}

/* 0x00b13970 stg05, 0x00b0e630 stg22 */
#if defined(OVERLAY_STG22)
/* steer(cast) */
void thrones_follow_path(void *obj, ThronesChildWork *w)
{
    Vec3 p;
    int ang[3];
    Vec3 cur;
    float sc[3][2];
    float time, x, y, z;
    Vec3 *lpos;

    time = ((float (*)(float))float_frac)(enemy_task_get_param(obj, 0)) * 1000.0f * 60.0f / 60.0f;
    dc_object_set_callback(&w->body, thrones_root_node_cb, w);
    ulMatrixPush();
    matrix_unit_set(0);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(&w->body);
    ulMatrixPop();
    dc_object_set_callback(&w->body, 0, 0);
    if (use_circle_path()) {
        thrones_follow_path_circle(obj, w);
        return;
    }
    if (fabsf(w->blend) < FLT_EPSILON) {
        enemy_task_get_pos(obj, &p);
    } else {
        player_pass_pos_a(&p, time);
        p.x += enemy_task_get_param(obj, 2);
        p.y += enemy_task_get_param(obj, 3);
        p.z += enemy_task_get_param(obj, 4);
        if (w->blend < 1.0f) {
            enemy_task_get_pos(obj, &cur);
            p.x = p.x * w->blend + cur.x * (1.0f - w->blend);
            p.y = p.y * w->blend + cur.y * (1.0f - w->blend);
            p.z = p.z * w->blend + cur.z * (1.0f - w->blend);
        }
    }
    player_pass_rot_a(ang, time);
    p.y -= thrones_swing_value(&((ThronesWork *)w)->swing);
    do { } while (0);
    lpos = &w->pos;
    y = lpos->y;
    z = lpos->z;
    x = lpos->x;
    ROT_Z(ang[2], sc[0], x, y);
    ROT_X(ang[0], sc[1], y, z);
    ROT_Y(ang[1], sc[2], x, z);
    lpos->x = x;
    lpos->y = y;
    lpos->z = z;
    v3_add(&w->pos, &p);
    i3_add(w->ang, ang);
}
#else
/* steer(cast) */
void thrones_follow_path(void *obj, ThronesChildWork *w)
{
    Vec3 p;
    int ang[3];
    Vec3 cur;
    float sc[3][2];
    float time, x, y, z;
    Vec3 *v;
    int *lpos;

    time = ((float (*)(float))float_frac)(enemy_task_get_param(obj, 0)) * 1000.0f * 60.0f / 60.0f;
    dc_object_set_callback(&w->body, thrones_root_node_cb, w);
    ulMatrixPush();
    matrix_unit_set(0);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(&w->body);
    ulMatrixPop();
    dc_object_set_callback(&w->body, 0, 0);
    if (map_get_current() == 20) {
        thrones_follow_path_circle(obj, w);
        return;
    }
    if (fabsf(w->blend) < FLT_EPSILON) {
        enemy_task_get_pos(obj, &p);
    } else {
        player_pass_pos_a(&p, time);
        p.x += enemy_task_get_param(obj, 2);
        p.y += enemy_task_get_param(obj, 3);
        p.z += enemy_task_get_param(obj, 4);
        if (w->blend < 1.0f) {
            enemy_task_get_pos(obj, &cur);
            p.x = p.x * w->blend + cur.x * (1.0f - w->blend);
            p.y = p.y * w->blend + cur.y * (1.0f - w->blend);
            p.z = p.z * w->blend + cur.z * (1.0f - w->blend);
        }
    }
    player_pass_rot_a(ang, time);
    p.y -= thrones_swing_value(&((ThronesWork *)w)->swing);
    do { } while (0);
    v = &w->pos;
    y = v->y;
    z = v->z;
    x = v->x;
    ROT_Z(ang[2], sc[0], x, y);
    ROT_X(ang[0], sc[1], y, z);
    ROT_Y(ang[1], sc[2], x, z);
    v->x = x;
    v->y = y;
    v->z = z;
    v3_add(&w->pos, &p);
    i3_add(w->ang, ang);
}
#endif

/* 0x00b13c98 stg05, 0x00b0e950 stg22 */
void en_armor_disp_disp(void *wp)
{
    Vec3 scl;
    Vec3 eye;
    void *obj = ((EnArmorDispWork *)wp)->obj;
    ThronesWork *w = enemy_task_work_ptr(obj);
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    ThronesRec *typ;
    ThronesArm *arm;
    ThronesSlot *x;
    int i, step;

    enemy_task_get_scl(obj, &scl);
    if (view_sphere_visible(&w->pos, gfx->radius)) {
        step = 1;
        typ = w->type;
        ulGxSetTextureList(((void **)obj)[1]);
        arm = w->arm;
        x = w->x;
        camera_get_eye(&eye);
        if (vec3_distance_squared(&eye, &w->arm[0].pos) < vec3_distance_squared(&eye, &w->arm[3].pos)) {
            arm = &w->arm[3];
            x = &w->x[3];
            step = -1;
        }
        for (i = 3; i != -1; arm += step, x += step, i--) {
            if (x->live) {
                ulMatrixPush();
                do {
                    Vec3 *p_ = &arm->pos;
                    vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
                } while (0);
                thrones_rotate_yxz(arm->ang);
                vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
                vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
                dc_object_draw(&arm->m);
                if (typ->wire) {
                    draw_set_blend(4, 5);
                    draw_set_color(yuki_beat_color(typ->wire_col, typ->wire_col2, typ->wire_beat, typ->wire_linear, 1.0f,
                                                   typ->wire_near, typ->wire_far, typ->wire_near_rate, typ->wire_far_rate));
                    model_object_draw(&arm->wire, arm->wire.frame);
                }
                ulMatrixPop();
            }
        }
    }
}

/* 0x00b13e78 stg05, 0x00b0eb30 stg22 */
void en_armor_disp_init(EnArmorDispWork *w, void *obj)
{
    EnArmorDispArg ent;

    w->obj = obj;
    heap_bzero(&ent, 20);
    ent.owner = w;
    ent.work = w;
    ent.fn = en_armor_disp_disp;
    ent.pos = (char *)enemy_task_work_ptr(obj) + 28;
    en_sort_add(&ent);
}

/* 0x00b13ee0 stg05, 0x00b0eb98 stg22 */
void *en_armor_disp_create(void *w)
{
    return task_create_sibling_inherit_arg(w, &en_armor_disp_desc, 12, w);
}

/* 0x00b13f08 stg05, 0x00b0ebc0 stg22 */
#if defined(OVERLAY_STG22)
/* steer(cast) */
void thrones_init(void *obj, ThronesWork *w, ShotArg *prm, int *k)
{
    ThronesCoreArg cprm;
    ThronesArm *piece = w->arm;
    ThronesRing *seg = w->y;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    ThronesSlot *slot = w->x;
    ThronesRec *typ;
    int n = 0;
    unsigned int i;

    w->type = typ = (ThronesRec *)&thrones_types[*k];
    enemy_task_clear_flags(obj, 64);
    do {
        void *m_ = stage_models, *t_ = stage_textures;
        ((void **)obj)[0] = m_;
        ((void **)obj)[1] = t_;
    } while (0);
    gfx->radius = 150.0f;
    w->blend = 0;
    {
        int kind = ((float (*)(float))float_frac)(*(float *)&prm->params[3]) * 1000.0f + 0.5f;
        ModelPart *body = &w->body;
        int *mdl, *mot;

        kind = kind > -1 ? kind : 0;
        kind = kind < 1 ? kind : 0;
        mdl = ((int *)pnb_get_entry(stage_models, kind + 19));
        mot = ((int *)pnb_get_entry(stage_models, kind + 20));
        dc_object_init(body, *mdl, *mot, 0);
    }
    ((PartAnim *)gfx)->frame = ((PartAnim *)gfx)->frame_end * RAND_FLOAT_A();
    ((PartAnim *)&w->body)->frame = ((PartAnim *)&w->body)->frame_end * RAND_FLOAT_A();
    {
        int *mdl = ((int *)pnb_get_entry(stage_models, 104));
        int *mot = ((int *)pnb_get_entry(stage_models, 105));

        dc_object_init(&w->halo, *mdl, *mot, 0);
    }
    dc_object_rebuild(&w->halo);
    w->halo.radius = 25.0f;
    {
        ModelPart *gfx = enemy_task_gfx_ptr(obj);

        ((PartAnim *)gfx)->frame = ((PartAnim *)gfx)->frame_end * RAND_FLOAT_A();
    }
    n = k[1];
    for (i = 0; i < 4; i++, slot++) {
        slot->wait = n;
        cprm.pos = &slot->pos;
        cprm.time = n;
        cprm.ang = slot->ang;
        slot->obj = (void *)thrones_core_create(obj, &cprm);
        slot->live = 1;
        n += typ->release_dif_sec * FPS;
    }
    slot = w->x;
    for (i = 0; i < 4; i++)
        thrones_core_set_siblings(slot[i].obj, (int)slot[0].obj, (int)slot[1].obj, (int)slot[2].obj, (int)slot[3].obj);
    thrones_core_manager_create(obj, slot[0].obj, slot[1].obj, slot[2].obj, slot[3].obj, prm->fx_arg);
    for (i = 0; i < 6; i++, seg++) {
        int *mdl = ((int *)pnb_get_entry(stage_models, thrones_seg_models[i][0]));
        int *mot = ((int *)pnb_get_entry(stage_models, thrones_seg_models[i][1]));

        dc_object_init(&seg->m, *mdl, *mot, 0);
        dc_object_rebuild(&seg->m);
        seg->state = 0;
        seg->m.radius = 25.0f;
    }
    dc_object_init(&w->core, *((struct DcNode **)pnb_get_entry(stage_models, 97)), 0, 0);
    dc_object_rebuild(&w->core);
    for (i = 0; i < 4; i++, piece++) {
        dc_object_init(&piece->m, *((struct DcNode **)pnb_get_entry(stage_models, thrones_piece_models[i])), 0, 0);
        dc_object_rebuild(&piece->m);
        dc_object_init(&piece->wire, *((struct DcNode **)pnb_get_entry(stage_models, thrones_piece_wire_models[i])), 0, 0);
        dc_object_set_wire(&piece->wire, model_pnb_get_wire(((void **)obj)[0], thrones_piece_wire_models[i]));
    }
    w->task = en_armor_disp_create(obj);
    enemy_task_set_scl(obj, &thrones_init_scale);
    w->state = 0;
    w->count = 1;
    w->life = k[1];
    thrones_swing_clear(&w->swing);
    {
        int kind = ((float (*)(float))float_frac)(enemy_task_get_param(obj, 1)) * 10.0f + 0.5f;

        kind = kind > -1 ? kind : 0;
        w->level = kind < 4 ? kind : 3;
    }
    enemy_task_set_status_pos(obj, &w->pos);
}
#else
/* steer(cast) */
void thrones_init(void *obj, ThronesWork *w, ShotArg *prm, int *k)
{
    ThronesCoreArg cprm;
    ThronesArm *piece = w->arm;
    ThronesRing *seg = w->y;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    ThronesSlot *slot = w->x;
    ThronesRec *typ;
    int n = 0;
    unsigned int i;

    w->type = typ = (ThronesRec *)&thrones_types[*k];
    enemy_task_clear_flags(obj, 64);
    do {
        void *m_ = stage_models, *t_ = stage_textures;
        ((void **)obj)[0] = m_;
        ((void **)obj)[1] = t_;
    } while (0);
    gfx->radius = 150.0f;
    w->blend = 0;
    {
        int kind = ((float (*)(float))float_frac)(*(float *)&prm->params[3]) * 1000.0f + 0.5f;
        ModelPart *body = &w->body;
        int *mdl, *mot;

        kind = kind > -1 ? kind : 0;
        kind = kind < 1 ? kind : 0;
        mdl = ((int *)pnb_get_entry(stage_models, kind + 47));
        mot = ((int *)pnb_get_entry(stage_models, kind + 48));
        dc_object_init(body, *mdl, *mot, 0);
    }
    ((PartAnim *)gfx)->frame = ((PartAnim *)gfx)->frame_end * RAND_FLOAT_A();
    ((PartAnim *)&w->body)->frame = ((PartAnim *)&w->body)->frame_end * RAND_FLOAT_A();
    {
        int *mdl = ((int *)pnb_get_entry(stage_models, 150));
        int *mot = ((int *)pnb_get_entry(stage_models, 151));

        dc_object_init(&w->halo, *mdl, *mot, 0);
    }
    dc_object_rebuild(&w->halo);
    w->halo.radius = 25.0f;
    {
        ModelPart *gfx = enemy_task_gfx_ptr(obj);

        ((PartAnim *)gfx)->frame = ((PartAnim *)gfx)->frame_end * RAND_FLOAT_A();
    }
    n = k[1];
    for (i = 0; i < 4; i++, slot++) {
        slot->wait = n;
        cprm.pos = &slot->pos;
        cprm.time = n;
        cprm.ang = slot->ang;
        slot->obj = (void *)thrones_core_create(obj, &cprm);
        slot->live = 1;
        n += typ->release_dif_sec * FPS;
    }
    slot = w->x;
    for (i = 0; i < 4; i++)
        thrones_core_set_siblings(slot[i].obj, (int)slot[0].obj, (int)slot[1].obj, (int)slot[2].obj, (int)slot[3].obj);
    thrones_core_manager_create(obj, slot[0].obj, slot[1].obj, slot[2].obj, slot[3].obj, prm->fx_arg);
    for (i = 0; i < 6; i++, seg++) {
        int *mdl = ((int *)pnb_get_entry(stage_models, thrones_seg_models[i][0]));
        int *mot = ((int *)pnb_get_entry(stage_models, thrones_seg_models[i][1]));

        dc_object_init(&seg->m, *mdl, *mot, 0);
        dc_object_rebuild(&seg->m);
        seg->state = 0;
        seg->m.radius = 25.0f;
    }
    dc_object_init(&w->core, *((struct DcNode **)pnb_get_entry(stage_models, 143)), 0, 0);
    dc_object_rebuild(&w->core);
    for (i = 0; i < 4; i++, piece++) {
        dc_object_init(&piece->m, *((struct DcNode **)pnb_get_entry(stage_models, thrones_piece_models[i])), 0, 0);
        dc_object_rebuild(&piece->m);
        dc_object_init(&piece->wire, *((struct DcNode **)pnb_get_entry(stage_models, thrones_piece_wire_models[i])), 0, 0);
        dc_object_set_wire(&piece->wire, model_pnb_get_wire(((void **)obj)[0], thrones_piece_wire_models[i]));
    }
    w->task = en_armor_disp_create(obj);
    enemy_task_set_scl(obj, &thrones_init_scale);
    w->state = 0;
    w->count = 1;
    w->life = k[1];
    thrones_swing_clear(&w->swing);
    {
        int kind = ((float (*)(float))float_frac)(enemy_task_get_param(obj, 1)) * 10.0f + 0.5f;

        kind = kind > -1 ? kind : 0;
        w->level = kind < 4 ? kind : 3;
    }
    enemy_task_set_status_pos(obj, &w->pos);
}
#endif

/* 0x00b14470 stg05, 0x00b0f128 stg22 */
void thrones_release(void *obj, ThronesWork *w)
{
    ThronesArm *arm;
    unsigned int i;
    ThronesRing *ring = w->y;
    ModelPart *part = &ring->m;

    dc_object_release(&w->body);
    for (i = 0, arm = w->arm, ring++; i < 4; i++, arm++) {
        dc_object_release(&arm->m);
        dc_object_release(&arm->wire);
    }
    i = 0;
    FINISH_IF(w->task);
    for (;;) {
        dc_object_release(part);
        i++;
        if (i >= 6)
            break;
        part = &(ring++)->m;
    }
    dc_object_release(&w->halo);
    dc_object_release(&w->core);
    thrones_swing_release(&w->swing);
}

/* 0x00b14560 stg05, 0x00b0f218 stg22 */
int thrones_next_state(void *obj, TypeEnemyWork *w, int n, int state)
{
    if (++w->frames > n) {
        w->state = state;
        w->frames = 1;
        return 1;
    }
    return 0;
}

/* 0x00b14590 stg05, 0x00b0f248 stg22 */
/* steer(cast) */
void thrones_state_enter(void *obj, ThronesChildWork *w)
{
    Vec3 p;
    Vec3 v;
    ThronesPartAngRec *rec;
    unsigned int i;
    float rate;
    int n;

    n = enemy_task_get_param(obj, 0);
    rate = w->count / (float)n;
    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &v);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    dc_object_advance_time(enemy_task_gfx_ptr(obj));
    w->blend = rate;
    thrones_follow_path(obj, w);
    for (i = 0, rec = (ThronesPartAngRec *)((ThronesWork *)w)->y[0].ang; i < 6; i++, rec++)
        rec->old[0] = rec->cur[0], rec->old[1] = rec->cur[1], rec->old[2] = rec->cur[2];
    thrones_next_state(obj, (TypeEnemyWork *)w, n, 1);
}

/* 0x00b146b0 stg05, 0x00b0f368 stg22 */
/* steer(cast) */
void thrones_state_shot_count(void *obj, TypeEnemyWork *w)
{
    int n = enemy_task_get_param(obj, 1);
    ThronesPartAngRec *rec;
    unsigned int i;

    dc_object_advance_time(enemy_task_gfx_ptr(obj));
    thrones_follow_path(obj, (ThronesChildWork *)w);
    for (i = 0, rec = (ThronesPartAngRec *)((ThronesWork *)w)->y[0].ang; i < 6; i++, rec++)
        rec->old[0] = rec->cur[0], rec->old[1] = rec->cur[1], rec->old[2] = rec->cur[2];
    thrones_next_state(obj, w, n, 2);
}

/* 0x00b14750 stg05, 0x00b0f408 stg22 */
/* steer(cast) */
void thrones_state_motion(void *obj, ThronesWork *w)
{
    Vec3 pl;
    Vec3 aim;
    Vec3 d;
    Vec3 cur;
    int aim_ang[2];
    Vec3 ax;
    float mtx[16];
    float sc_0_0, sc_0_1, sc_1_0, sc_1_1, sc_2_0, sc_2_1, sc_3_0, sc_3_1, sc_4_0, sc_4_1;
    ThronesRec *typ = w->type;
    int dif = typ->motion_dif_sec * FPS;
    int mot = typ->motion_sec * FPS;
    int wait = typ->motion_wait_sec * FPS;
    int n = mot + dif * 2 + wait;
    int beat;
    ThronesRing *seg;
    float rate, frac, x, ry, z;
    int i, j;

    dc_object_advance_time(enemy_task_gfx_ptr(obj));
    thrones_follow_path(obj, (ThronesChildWork *)w);
    beat = (int)(((float (*)(float))float_frac)(enemy_task_get_param(obj, 1)) * 1000.0f + 0.5f) % 10;
    player_get_pos(&pl);
    for (i = 0; i < 3u; i++) {
        if (w->count < dif * i)
            continue;
        seg = &w->y[i * 2];
        frac = (w->count - dif * i) / (float)mot;
        rate = MIN((0.0f <= frac ? frac : 0.0f), 1.0f);
        for (j = 0; j < 2; j++, seg++) {
            x = 0;
            ry = 0;
            z = -1.0f;
            if (!w->level)
                break;
            if (w->level == 2) {
                if (!j)
                    continue;
            } else if (w->level == 1 && j == 1)
                continue;
            ROTS_Z(seg->cur[2], sc_0_0, sc_0_1, x, ry);
            ROTS_X(seg->cur[0], sc_1_0, sc_1_1, ry, z);
            ROTS_Y(seg->cur[1], sc_2_0, sc_2_1, x, z);
            cur.x = x;
            cur.y = ry;
            cur.z = z;
            cr_needle_initial_angles(aim_ang, &seg->pos, 0, beat);
            x = 0;
            ry = 0;
            z = -1.0f;
            ROTS_X(aim_ang[0], sc_3_0, sc_3_1, ry, z);
            ROTS_Y(aim_ang[1], sc_4_0, sc_4_1, x, z);
            aim.x = x;
            aim.y = ry;
            aim.z = z;
            if (ulVec3Dot(&cur, &aim) < 0.0f) {
                ulVec3Cross(&ax, &cur, &aim);
                matrix_from_axis_angle(mtx, &ax, ANG_90);
                ulMatrixPush();
                ulMatrixLoad(mtx);
                ulMatrixApplyVector((float *)ulMatrixCur, &cur.x, &aim.x);
                ulMatrixPop();
            }
            v3_copy(&d, &cur);
            vec3_approach(&aim, &d, rate);
            do {
                seg->ang[1] = RAD_TO_ANG(atan2f(-d.x, -d.z));
                seg->ang[0] = RAD_TO_ANG(atan2f(d.y, sqrtf(d.x * d.x + d.z * d.z)));
            } while (0);
            seg->ang[2] = 0;
            seg->m.frame = seg->m.frames * rate;
            switch (seg->state) {
            case 0:
                seg->scale = 0;
                seg->state = 1;
                break;
            case 1:
                frac = (w->count - dif * i - mot) / (float)wait;
                seg->scale = frac;
                {
                    float s = MIN((0.0f <= frac ? frac : 0.0f), 1.0f);

                    seg->scale = s;
                }
                if (w->count >= dif * i + mot + wait)
                    seg->state = 2;
                break;
            }
        }
    }
    thrones_next_state(obj, (TypeEnemyWork *)w, n, 3);
}

/* 0x00b14d10 stg05, 0x00b0f9c8 stg22 */
void thrones_state_tackle_count(void *obj, TypeEnemyWork *w)
{
    int n = enemy_task_get_param(obj, 5);

    dc_object_advance_time(enemy_task_gfx_ptr(obj));
    thrones_follow_path(obj, (ThronesChildWork *)w);
    thrones_next_state(obj, w, n, 3);
}

/* 0x00b14d80 stg05, 0x00b0fa38 stg22 */
void thrones_update(void *obj, TypeEnemyWork *w)
{
    void (*fn)(void *obj, TypeEnemyWork *w) = thrones_states[w->state];

    if (fn)
        fn(obj, w);
    thrones_pose_model(obj, w);
}

/* 0x00b14dd8 stg05, 0x00b0fa90 stg22 */
#if defined(OVERLAY_STG22)
void thrones_state_die(void *obj, ThronesChildWork *w)
{
    ThronesRec *typ = (ThronesRec *)w->type;
    Vec3 dir;
    Vec3 step;
    ModelPart *gfx;
    float rate, fall;
    int n;

    n = typ->die_sec * FPS;
    rate = MIN(MAX_GE(w->count / (float)n, 0.0f), 1.0f);
    gfx = enemy_task_gfx_ptr(obj);
    gfx->speed *= typ->motion_density;
    dc_object_advance_time(gfx);
    w->body.speed *= typ->motion_density;
    thrones_follow_path(obj, w);
    do {
        v3_set(&dir, 0.0f, -1.0f, 0.0f);
    } while (0);
    player_get_frame_accel(&step);
    if (!v3_is_zero(&step)) {
        vec3_normalize(&step, &step);
        v3_sub(&dir, &dir, &step);
    }
    w->ang[0] = w->ang[0] + typ->die_rot_x / FPS * rate;
    fall = typ->die_accel * 0.5f * ((w->count / FPS) * (w->count / FPS) - ((w->count - 1) / FPS) * ((w->count - 1) / FPS));
    v3_mul(&step, &dir, fall);
    v3_add(&w->pos, &step);
    w->ang[2] = w->ang[2] + typ->die_rot_z / FPS * rate;
    thrones_pose_model(obj, (TypeEnemyWork *)w);
    if (w->count++ > n)
        FINISH_IF(obj);
}
#else
void thrones_state_die(void *obj, ThronesChildWork *w)
{
    ThronesRec *typ = (ThronesRec *)w->type;
    Vec3 dir;
    Vec3 step;
    ModelPart *gfx;
    float rate, fall;
    int n;

    n = typ->die_sec * FPS;
    rate = MIN(MAX_GE(w->count / (float)n, 0.0f), 1.0f);
    gfx = enemy_task_gfx_ptr(obj);
    gfx->speed *= typ->motion_density;
    dc_object_advance_time(gfx);
    w->body.speed *= typ->motion_density;
    thrones_follow_path(obj, w);
    do {
        if (map_get_current() == 20)
            v3_set(&dir, 0.0f, 0.0f, 1.0f);
        else
            v3_set(&dir, 0.0f, -1.0f, 0.0f);
    } while (0);
    player_get_frame_accel(&step);
    if (!v3_is_zero(&step)) {
        vec3_normalize(&step, &step);
        v3_sub(&dir, &dir, &step);
    }
    w->ang[0] = w->ang[0] + typ->die_rot_x / FPS * rate;
    fall = typ->die_accel * 0.5f * ((w->count / FPS) * (w->count / FPS) - ((w->count - 1) / FPS) * ((w->count - 1) / FPS));
    v3_mul(&step, &dir, fall);
    v3_add(&w->pos, &step);
    w->ang[2] = w->ang[2] + typ->die_rot_z / FPS * rate;
    thrones_pose_model(obj, (TypeEnemyWork *)w);
    if (w->count++ > n)
        FINISH_IF(obj);
}
#endif

/* 0x00b150f0 stg05, 0x00b0fd80 stg22 */
void thrones_exec_appear(void *obj, ObjLifeWork *w)
{
    enemy_task_appear_scale(obj, w);
    switch (enemy_task_get_phase(obj)) {
    case 0:
        return;
    case 2:
        enemy_task_appear_scale(obj, w);
        return;
    }
    thrones_update(obj, (TypeEnemyWork *)w);
    if (w->life > 0 && --w->life <= 0)
        enemy_task_set_state(obj, 2);
}

/* 0x00b15180 stg05, 0x00b0fe10 stg22 */
void thrones_exec_normal(void *obj, ObjLifeWork *w)
{
    if (enemy_task_get_phase(obj) != 1)
        return;
    thrones_update(obj, (TypeEnemyWork *)w);
    if (w->life > 0 && --w->life <= 0)
        enemy_task_set_state(obj, 2);
}

/* 0x00b151e8 stg05, 0x00b0fe78 stg22 */
void thrones_exec_leave(void *obj, ThronesWork *w)
{
    ThronesRec *typ;
    ThronesSlot *x = w->x;
    EnThronesArmorDieArg prm;
    int i, n;

    typ = w->type;
    n = enemy_task_get_phase(obj);
    if (!n) {
        w->count = 0;
        return;
    }
    if (n == 2)
        return;
    for (i = 0; i < 4U; i++, x++) {
        if (w->count < typ->release_dif_sec * FPS * i)
            break;
        if (x->live) {
            x->live = 0;
            prm.pos = &x->pos;
            prm.ang = x->ang;
            prm.i = i;
            prm.color = yuki_beat_color(typ->wire_col, typ->wire_col2, typ->wire_beat, typ->wire_linear, 1.0f,
                                      typ->wire_near, typ->wire_far, typ->wire_near_rate, typ->wire_far_rate);
            en_thrones_armor_die2_create(obj, &prm);
        }
    }
    thrones_state_die(obj, (ThronesChildWork *)w);
}

/* 0x00b15338 stg05, 0x00b0ffc8 stg22 */
void thrones_exec_destroyed(void *obj, ThronesWork *w)
{
    SpawnDesc desc;
    Vec3 scr;
    unsigned int i;

    if (enemy_task_get_phase(obj))
        return;
    thrones_follow_path(obj, (ThronesChildWork *)w);
    thrones_pose_model(obj, (TypeEnemyWork *)w);
    nj_fill8(&desc, 0, sizeof desc);
    desc.kind = 0;
    desc.models = stage_models;
    enemy_task_get_scl(obj, &desc.scale);
    for (i = 0; i < 4; i++) {
        ThronesArm *piece = &w->arm[i];

        desc.type = thrones_piece_break_types[i];
        v3_copy(&desc.pos, &piece->pos);
        i3_copy(desc.ang, piece->ang);
        en_shatter_spawn(obj, &desc, 2);
    }
    ulMatrixApplyPoint((float *)ulMatrixCur, &w->pos.x, &scr.x);
    project_point(&scr, &scr);
    SoundRhythmSe_EnemyPos(thrones_core_get_init_arg(), 5, &scr.x);
    FINISH_IF(obj);
}

/* 0x00b15498 stg05, 0x00b10128 stg22 */
void thrones_pre_exec(void)
{
}

/* 0x00b154a0 stg05, 0x00b10130 stg22 */
void thrones_exec(void *obj, TypeEnemyWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);
    unsigned int k;
    ThronesSlot *slot;

    if (thrones_exec_table[i])
        thrones_exec_table[i](obj, w);
    thrones_pose_model(obj, w);
    for (k = 0, slot = ((ThronesWork *)w)->x; k < 4; k++, slot++)
        slot->wait--;
}

/* 0x00b15530 stg05, 0x00b101c0 stg22 */
/* steer(cast) */
void thrones_post_exec(void *obj, ThronesWork *w)
{
    unsigned int gone = 0;
    ThronesRec *typ = w->type;
    ThronesSlot *slot = w->x;
    ThronesRing *seg = w->y;
    ThronesRing *pair;
    int i, j;
    int set;
    float dx, dy, dz;

    thrones_count_down(&w->swing.cnt);
    if (!enemy_status_get_ticks()) {
        thrones_follow_path(obj, (ThronesChildWork *)w);
        thrones_pose_model(obj, (TypeEnemyWork *)w);
    }
    for (i = 0; i < sizeof w->x / sizeof w->x[0]; i++, slot++) {
        if (slot->wait <= 0)
            continue;
        if (!slot->obj) {
            gone++;
            continue;
        }
        if (TASK_OF(slot->obj)->flags & TASK_F_END) {
            slot->obj = 0;
            continue;
        }
        if (!slot->live)
            continue;
        if (typ->armor_hp < ((LockObj *)thrones_core_get_lock(slot->obj))->life)
            continue;
        {
        EnThronesArmorDieArg die;

        slot->live = 0;
        die.pos = &slot->pos;
        die.ang = slot->ang;
        die.i = i;
        die.color = yuki_beat_color(typ->wire_col, typ->wire_col2, typ->wire_beat, typ->wire_linear, 1.0f, typ->wire_near,
                                   typ->wire_far, typ->wire_near_rate, typ->wire_far_rate);
        en_thrones_armor_die_create(obj, &die);
        }
        if (i > 0) {
            int n = (i - 1) * 2;

            pair = &w->y[n];
            for (j = 0; j < 2; j++, n++, pair++) {
                if (pair->state < 3) {
                    SpawnDesc desc;

                    nj_fill8(&desc, 0, sizeof desc);
                    desc.kind = 0;
                    desc.models = stage_models;
                    desc.lock = enemy_task_lock_ptr(obj);
                    if (0.0f < pair->m.frame)
                        desc.type = thrones_seg_models[n][3];
                    else
                        desc.type = thrones_seg_models[n][2];
                    desc.time = pair->m.frame;
                    v3_copy(&desc.pos, &pair->pos);
                    i3_copy(desc.ang, pair->ang);
                    enemy_task_get_scl(obj, &desc.scale);
                    v3_copy(&desc.v, &w->hitv);
                    en_shatter_spawn(obj, &desc, 2);
                    pair->state = 3;
                }
            }
        }
    }
    if (gone >= 4 && !enemy_task_get_pstate(obj))
        enemy_task_set_state(obj, 3);
    for (i = 0; i < sizeof w->y / sizeof w->y[0]; i++, seg++) {
        if (seg->state == 2) {
            Vec3 dir;
            float sc_0_0, sc_0_1, sc_1_0, sc_1_1;

            set = (int)(((float (*)(float))float_frac)(enemy_task_get_param(obj, 1)) * 1000.0f + 0.5f);
            set %= 10;
            set = set > -1 ? set : 0;
            set = set < 3 ? set : 2;
            dx = 0;
            dy = 0;
            dz = -1.0f;
            ROTS_X(seg->ang[0], sc_0_0, sc_0_1, dy, dz);
            ROTS_Y(seg->ang[1], sc_1_0, sc_1_1, dx, dz);
            dir.x = dx;
            dir.y = dy;
            dir.z = dz;
            cr_needle_fire(obj, &seg->pos, &dir, set);
            seg->state = 3;
            thrones_swing_reset(&w->swing, typ->blow_back_sec * FPS, typ->blow_back_rate, typ->blow_back_scl,
                                 typ->blow_back_spd);
        }
    }
}

/* 0x00b159e8 stg05, 0x00b10678 stg22 */
void thrones_disp_body(void *obj, ThronesWork *w)
{
    Vec3 scl;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    ThronesRing *seg;
    unsigned int i;

    enemy_task_get_scl(obj, &scl);
    if (view_sphere_visible(&w->pos, gfx->radius)) {
        seg = w->y;
        ulGxSetTextureList(((void **)obj)[1]);
        ulMatrixPush();
        do {
            Vec3 *p_ = &w->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        thrones_rotate_yxz(w->ang);
        vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
        dc_object_draw(gfx);
        ulMatrixPop();
        ulMatrixPush();
        do {
            Vec3 *p_ = &w->arm[3].pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        thrones_rotate_yxz(w->arm[3].ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
        dc_object_draw(&w->core);
        ulMatrixPop();
        for (i = 0; i < 6; i++, seg++) {
            if (seg->state < 2) {
                ulMatrixPush();
                do {
                    Vec3 *p_ = &seg->pos;
                    vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
                } while (0);
                thrones_rotate_yxz(seg->ang);
                vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
                vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
                dc_object_draw(&seg->m);
                if (seg->state == 1) {
                    ulMatrixUnitRotation((volatile float *)ulMatrixCur);
                    model_object_draw(&w->halo, w->halo.frames * seg->scale);
                }
                ulMatrixPop();
            }
        }
    }
}

/* 0x00b15bc0 stg05, 0x00b10850 stg22 */
/* steer(cast) */
void thrones_disp_die(void *obj, ThronesWork *w)
{
    Vec3 scl;
    float col_add[4];
    int prev;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    ThronesRec *typ;
    ThronesArm *arm, *piece;
    ThronesSlot *slot;
    ThronesRing *seg;
    unsigned int i;
    float rate;

    enemy_task_get_scl(obj, &scl);
    if (view_sphere_visible(&w->pos, gfx->radius)) {
        arm = w->arm;
        prev = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        slot = w->x;
        seg = w->y;
        typ = w->type;
        heap_bzero(col_add, 16);
        rate = 1.0f - w->count / (typ->die_sec * FPS);
        rate = 0.0f <= rate ? (rate < 1.0f ? (0.0f <= rate ? rate : 0.0f) : 1.0f) : 0.0f;
        col_add[0] = rate - 1.0f;
        draw_push_strip_flags();
        draw_push_override_color();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
        draw_set_override_color(col_add);
        ulGxSetTextureList(((void **)obj)[1]);
        ulMatrixPush();
        do {
            Vec3 *p_ = &w->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        thrones_rotate_yxz(w->ang);
        vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
        dc_object_draw(gfx);
        ulMatrixPop();
        ulMatrixPush();
        do {
            Vec3 *p_ = &w->arm[3].pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        thrones_rotate_yxz(w->arm[3].ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
        dc_object_draw(&w->core);
        ulMatrixPop();
        for (i = 0; i < 6; i++, seg++) {
            if (seg->state < 2) {
                ulMatrixPush();
                do {
                    Vec3 *p_ = &seg->pos;
                    vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
                } while (0);
                thrones_rotate_yxz(seg->ang);
                vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
                vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
                dc_object_draw(&seg->m);
                ulMatrixPop();
            }
        }
        for (i = 0, piece = arm; i < 4; i++, piece++, slot++) {
            if (slot->live) {
                ulMatrixPush();
                do {
                    Vec3 *p_ = &piece->pos;
                    vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
                } while (0);
                thrones_rotate_yxz(piece->ang);
                vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
                vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
                dc_object_draw(&piece->m);
                if (typ->wire) {
                    draw_set_blend(4, 5);
                    draw_set_color(yuki_beat_color(typ->wire_col, typ->wire_col2, typ->wire_beat, typ->wire_linear, rate,
                                                   typ->wire_near, typ->wire_far, typ->wire_near_rate, typ->wire_far_rate));
                    model_object_draw(&piece->wire, piece->wire.frame);
                }
                ulMatrixPop();
            }
        }
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(prev);
    }
}

/* 0x00b15f40 stg05, 0x00b10bd0 stg22 */
void thrones_disp(void *obj, TypeEnemyWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (thrones_disp_table[i])
        thrones_disp_table[i](obj, w);
}

/* 0x00b15f98 stg05, 0x00b10c28 stg22 */
void thrones_create(void *obj, ShotArg *prm)
{
    int k[2];
    ObjDesc desc;
    ObjFuncs funcs = *(ObjFuncs *)thrones_funcs;
    ThronesRec *typ;
    int wait = *(float *)&prm->params[3];
    int move = prm->move;
    int delay = prm->delay;

    if (wait <= 0) {
        thrones_child_create(obj, (ThronesChildArg *)prm);
        return;
    }
    typ = (ThronesRec *)thrones_types;
    k[0] = 0;
    k[1] = (int)(move + delay + wait + (typ->motion_sec + typ->motion_wait_sec + typ->motion_dif_sec * 2.0f) * FPS)
           + typ->core_shot_wait_sec * FPS;
    nj_fill8(&desc, 0, sizeof desc);
    desc.name = thrones_name;
    desc.funcs = &funcs;
    desc.arg = prm;
    desc.kind = k;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.flags = 10;
    desc.dc_flags = 4096;
    desc.model_no = THRONES_MODEL(120);
    desc.model_no2 = THRONES_MODEL(121);
    desc.hp = 0;
    desc.same_lock = 0;
    desc.score = 0;
    desc.hp_reduce = 0;
    desc.crash_r = typ->crash_r;
    desc.crash_damage = typ->crash_damage;
    desc.time = k[1];
    desc.size = 2184;
    enemy_task_task_create(obj, &desc);
}

/* 0x00b16168 stg05, 0x00b10df8 stg22 */
void thrones_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
