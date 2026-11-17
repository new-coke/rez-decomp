/*
 * Lesser Thrones (a Thrones placed without a wait): follows its path without launching cores.
 * Its lockable parts shatter one by one, each with a rhythm sound; with all four lost it falls.
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
#include "player.h"
#include "pnb.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "thrones_child.h"
#include "../stg22/e_shot.h"
#include "en_shatter.h"
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

extern int thrones_child_init_arg;
extern void (*thrones_child_exec_table[])(void *o, struct ThronesChildWork *w);
extern void (*thrones_child_disp_table[])(void *o, struct ThronesChildWork *w);
extern ThronesChildSet thrones_child_types[1];
extern VarDef thrones_child_vardef;
extern VarDef thrones_child_vardefs[12];
extern void (*thrones_child_states[])(void *o, ThronesChildWork *w);

/* advanced by thrones_child_node_cb while posing */
typedef struct ThronesChildCursor {
    ThronesChildNode *node;
    ThronesChildPart *part;              /* 0x04 */
} ThronesChildCursor;

extern Vec3 thrones_child_init_scale;
extern int thrones_child_node_break_types[];
extern char thrones_child_lock_name[];
extern char thrones_child_name[];
extern ObjFuncs thrones_child_funcs;

/* model list entry n as stg22 numbers it */
#if defined(OVERLAY_STG22)
#define THRONES_CHILD_MODEL(n) ((n) - 46)
#else
#define THRONES_CHILD_MODEL(n) (n)
#endif

#if defined(OVERLAY_STG22)
#endif

/* 0x00b174e0 stg05, 0x00b12170 stg22 */
void thrones_child_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&thrones_child_vardef);
    for (i = 0; i < 1; i++) {
        def = thrones_child_vardefs;
        val = thrones_child_types[i].v;
        thrones_child_vardef.name = thrones_child_types[i].name;
        thrones_child_vardef.fmt = thrones_child_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&thrones_child_vardef);
        for (j = 0; j < 12; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)thrones_child_vardefs);
    }
}

/* 0x00b175f8 stg05, 0x00b12288 stg22 */
void thrones_child_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b17600 stg05, 0x00b12290 stg22 */
int thrones_child_get_init_arg(void)
{
    return thrones_child_init_arg;
}

/* 0x00b17610 stg05, 0x00b122a0 stg22 */
int thrones_child_vars_init(int *prm)
{
    thrones_child_init_arg = *prm;
    thrones_child_vars_register();
    return 0;
}

/* 0x00b17638 stg05, 0x00b122c8 stg22 */
int thrones_child_vars_release(void)
{
    thrones_child_vars_unregister();
    return 0;
}

/* 0x00b17658 stg05, 0x00b122e8 stg22 */
int thrones_child_marker_draw(int obj, void *place)
{
    int *mdl;

    ulGxSetTextureList(stage_textures);
    mdl = pnb_get_entry(stage_models, THRONES_CHILD_MODEL(148));
    return obj_draw_at(obj, place, (void *)*mdl);
}

/* 0x00b176b0 stg05, 0x00b12340 stg22 */
int thrones_child_node_cb(unsigned int *n, int idx, ThronesChildCursor *cur)
{
    ThronesChildNode *p;

    if (*n & 0x2000000) {
        p = cur->node;
        ulMatrixGetTranslation((float *)ulMatrixCur, &p->pos.x);
        ulMatrixPush();
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        model_camera_angles(p->ang);
        ulMatrixPop();
        cur->node++;
    }
    if (*n & 0x1000000) {
        ulMatrixGetTranslation((float *)ulMatrixCur, &cur->part->pos.x);
        cur->part++;
    }
    return 0;
}

/* 0x00b17770 stg05, 0x00b12400 stg22 */
void thrones_child_pose_model(void *obj, ThronesChildWork *w)
{
    ThronesChildCursor cur;
    ModelPart *blk = enemy_task_gfx_ptr(obj);

    cur.node = w->nodes;
    cur.part = w->part;
    dc_object_set_callback(blk, thrones_child_node_cb, &cur);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &w->pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    thrones_child_rotate_yxz(w->ang);
    dc_object_draw(blk);
    ulMatrixPop();
    dc_object_set_callback(blk, 0, 0);
}

/* 0x00b17818 stg05, 0x00b124a8 stg22 */
/* steer(cast) */
void thrones_child_follow_path_circle(void *obj, ThronesChildWork *w)
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
    v3_scale(&v, w->pos.y);
    x += v.x;
    y += v.y;
    player_pass_pos_b(&w->pos, time);
    w->pos.x += x;
    w->pos.y += y;
    w->pos.z += z;
    i3_set(w->ang, 0, 0, (int)(atan2f(-x, y) * full / tau) + ANG_180);
    ulMatrixPush();
    matrix_unit_set(0);
    thrones_child_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    model_camera_angles(w->ang);
    ulMatrixPop();
    enemy_task_get_pos(obj, &p);
    w->pos.x = w->pos.x * w->blend + p.x * (1.0f - w->blend);
    w->pos.y = w->pos.y * w->blend + p.y * (1.0f - w->blend);
    w->pos.z = w->pos.z * w->blend + p.z * (1.0f - w->blend);
}

/* 0x00b17ac0 stg05, 0x00b12750 stg22 */
int thrones_child_root_node_cb(unsigned int *n, int idx, ThronesChildWork *w)
{
    ulMatrixGetTranslation((float *)ulMatrixCur, &w->pos.x);
    model_camera_angles(w->ang);
    return 0;
}

/* 0x00b17b00 stg05, 0x00b12790 stg22 */
#if defined(OVERLAY_STG22)
/* steer(cast) */
void thrones_child_follow_path(void *obj, ThronesChildWork *w)
{
    Vec3 p;
    int ang[3];
    Vec3 cur;
    float sc[3][2];
    float time, x, y, z;
    Vec3 *lpos;

    time = ((float (*)(float))float_frac)(enemy_task_get_param(obj, 0)) * 1000.0f * 60.0f / 60.0f;
    dc_object_set_callback(&w->body, thrones_child_root_node_cb, w);
    ulMatrixPush();
    matrix_unit_set(0);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(&w->body);
    ulMatrixPop();
    dc_object_set_callback(&w->body, 0, 0);
    if (use_circle_path()) {
        thrones_child_follow_path_circle(obj, w);
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
    lpos = &w->pos;
    y = lpos->y;
    z = lpos->z;
    do { x = lpos->x; } while (0);
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
void thrones_child_follow_path(void *obj, ThronesChildWork *w)
{
    Vec3 p;
    int ang[3];
    Vec3 cur;
    float sc[3][2];
    float time, x, y, z;
    Vec3 *v;
    int *lpos;

    time = ((float (*)(float))float_frac)(enemy_task_get_param(obj, 0)) * 1000.0f * 60.0f / 60.0f;
    dc_object_set_callback(&w->body, thrones_child_root_node_cb, w);
    ulMatrixPush();
    matrix_unit_set(0);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(&w->body);
    ulMatrixPop();
    dc_object_set_callback(&w->body, 0, 0);
    if (map_get_current() == 20) {
        thrones_child_follow_path_circle(obj, w);
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
    v = &w->pos;
    y = v->y;
    z = v->z;
    do { x = v->x; } while (0);
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

/* 0x00b17e18 stg05, 0x00b12aa0 stg22 */
#if defined(OVERLAY_STG22)
/* steer(cast) */
void thrones_child_init(void *obj, ThronesChildWork *w, ThronesChildArg *prm, void *kind)
{
    int i;
    ThronesChildKind *k = kind;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    ThronesChildPart *part = w->part;
    ThronesChildRec *typ;
    int var;

    typ = (ThronesChildRec *)&thrones_child_types[k->idx];
    w->type = typ;
    enemy_task_clear_flags(obj, 64);
    do {
        void *m_ = stage_models, *t_ = stage_textures;
        ((void **)obj)[0] = m_;
        ((void **)obj)[1] = t_;
    } while (0);
    gfx->radius = 100.0f;
    w->blend = 0;
    var = ((float (*)(float))float_frac)(prm->motion) * 1000.0f + 0.5f;
    if (var < 0)
        var = 0;
    if (var > 0)
        var = 0;
    dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(stage_models, var + 19)), *((struct DcClip **)pnb_get_entry(stage_models, var + 20)), 0);
    w->body.frame = w->body.frames * RAND_FLOAT_A();
    gfx->frame = gfx->frames * RAND_FLOAT_A();
    for (i = 0; i < 4; i++, part++) {
        lockon_obj_init(&part->lock, obj, &part->pos, 0, thrones_child_lock_name, enemy_task_get_status_slot(obj), typ->hp, typ->same_lock, 128);
        lockon_entry(&part->lock);
        part->lock.flags &= ~LOCK_LOCKABLE;
    }
    enemy_task_set_scl(obj, &thrones_child_init_scale);
    w->state = 0;
    w->count = 1;
    w->life = k->life;
    enemy_task_set_status_pos(obj, &w->pos);
    w->lost = 0;
}
#else
/* steer(cast) */
void thrones_child_init(void *obj, ThronesChildWork *w, ThronesChildArg *prm, void *kind)
{
    int i;
    ThronesChildKind *k = kind;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    ThronesChildPart *part = w->part;
    ThronesChildRec *typ;
    int var;

    typ = (ThronesChildRec *)&thrones_child_types[k->idx];
    w->type = typ;
    enemy_task_clear_flags(obj, 64);
    do {
        void *m_ = stage_models, *t_ = stage_textures;
        ((void **)obj)[0] = m_;
        ((void **)obj)[1] = t_;
    } while (0);
    gfx->radius = 100.0f;
    w->blend = 0;
    var = ((float (*)(float))float_frac)(prm->motion) * 1000.0f + 0.5f;
    if (var < 0)
        var = 0;
    if (var > 0)
        var = 0;
    dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(stage_models, var + 47)), *((struct DcClip **)pnb_get_entry(stage_models, var + 48)), 0);
    w->body.frame = w->body.frames * RAND_FLOAT_A();
    gfx->frame = gfx->frames * RAND_FLOAT_A();
    for (i = 0; i < 4; i++, part++) {
        lockon_obj_init(&part->lock, obj, &part->pos, 0, thrones_child_lock_name, enemy_task_get_status_slot(obj), typ->hp, typ->same_lock, 128);
        lockon_entry(&part->lock);
        part->lock.flags &= ~LOCK_LOCKABLE;
    }
    enemy_task_set_scl(obj, &thrones_child_init_scale);
    w->state = 0;
    w->count = 1;
    w->life = k->life;
    enemy_task_set_status_pos(obj, &w->pos);
    w->lost = 0;
}
#endif

/* 0x00b18068 stg05, 0x00b12cf0 stg22 */
void thrones_child_release(void *obj, ThronesChildWork *w)
{
    ThronesChildPart *part = w->part;
    int i;

    for (i = 0; i < 4; i++, part++)
        lockon_remove(&part->lock);
    dc_object_release(&w->body);
}

/* 0x00b180b8 stg05, 0x00b12d40 stg22 */
int thrones_child_next_state(void *obj, ThronesChildWork *w, int n, int state)
{
    if (++w->count > n) {
        w->state = state;
        w->count = 1;
        return 1;
    }
    return 0;
}

/* 0x00b180e8 stg05, 0x00b12d70 stg22 */
void thrones_child_state_move(void *obj, ThronesChildWork *w)
{
    Vec3 p;
    Vec3 v;
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
    thrones_child_follow_path(obj, w);
    thrones_child_next_state(obj, w, n, 1);
}

/* 0x00b181d8 stg05, 0x00b12e60 stg22 */
void thrones_child_state_animate(void *obj, ThronesChildWork *w)
{
    dc_object_advance_time(enemy_task_gfx_ptr(obj));
    thrones_child_follow_path(obj, w);
}

/* 0x00b18218 stg05, 0x00b12ea0 stg22 */
void thrones_child_update(void *obj, ThronesChildWork *w)
{
    void (*fn)(void *obj, ThronesChildWork *w) = thrones_child_states[w->state];

    if (fn)
        fn(obj, w);
    thrones_child_pose_model(obj, w);
}

/* 0x00b18270 stg05, 0x00b12ef8 stg22 */
#if defined(OVERLAY_STG22)
void thrones_child_state_die(void *obj, ThronesChildWork *w)
{
    ThronesChildRec *typ = w->type;
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
    thrones_child_follow_path(obj, w);
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
    thrones_child_pose_model(obj, w);
    if (w->count++ > n)
        FINISH_IF(obj);
}
#else
void thrones_child_state_die(void *obj, ThronesChildWork *w)
{
    ThronesChildRec *typ = w->type;
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
    thrones_child_follow_path(obj, w);
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
    thrones_child_pose_model(obj, w);
    if (w->count++ > n)
        FINISH_IF(obj);
}
#endif

/* 0x00b18588 stg05, 0x00b131e8 stg22 */
void thrones_child_exec_appear(void *obj, ThronesChildWork *w)
{
    ThronesChildPart *part = w->part;
    int i;

    enemy_task_appear_scale(obj, w);
    switch (enemy_task_get_phase(obj)) {
    case 0:
        return;
    case 2:
        for (i = 0; i < 4; i++, part++)
            part->lock.flags |= LOCK_LOCKABLE;
        return;
    }
    thrones_child_update(obj, w);
}

/* 0x00b18628 stg05, 0x00b13288 stg22 */
void thrones_child_exec_normal(void *obj, ThronesChildWork *w)
{
    if (enemy_task_get_phase(obj) != 1)
        return;
    thrones_child_update(obj, w);
    if (w->life > 0 && --w->life <= 0)
        enemy_task_set_state(obj, 2);
}

/* 0x00b18690 stg05, 0x00b132f0 stg22 */
void thrones_child_exec_leave(void *obj, ThronesChildWork *w)
{
    ThronesChildPart *part = w->part;
    int i;

    switch (enemy_task_get_phase(obj)) {
    case 0:
        for (i = 0; i < 4; i++, part++)
            part->lock.flags &= ~LOCK_LOCKABLE;
        break;
    case 2:
        break;
    default:
        thrones_child_state_die(obj, w);
    }
}

/* 0x00b18728 stg05, 0x00b13388 stg22 */
void thrones_child_exec_destroyed(void *obj, ThronesChildWork *w)
{
    SpawnDesc desc;
    ThronesChildNode *node;
    int i;

    if (enemy_task_get_phase(obj) == 0) {
        thrones_child_follow_path(obj, w);
        thrones_child_pose_model(obj, w);
        for (i = 0, node = w->nodes; i < 4; i++, node++) {
            int *ang = node->ang;
            nj_fill8(&desc, 0, sizeof desc);
            desc.type = thrones_child_node_break_types[i];
            desc.models = stage_models;
            desc.kind = 0;
            desc.lock = enemy_task_lock_ptr(obj);
            v3_copy(&desc.pos, &node->pos);
            desc.ang[0] = ang[0];
            desc.ang[1] = ang[1];
            desc.ang[2] = ang[2];
            enemy_task_get_scl(obj, &desc.scale);
            v3_copy(&desc.v, &w->hitv);
            en_shatter_spawn(obj, &desc, 2);
        }
        FINISH_IF(obj);
    }
}

/* 0x00b18888 stg05, 0x00b134e8 stg22 */
void thrones_child_pre_exec(void)
{
}

/* 0x00b18890 stg05, 0x00b134f0 stg22 */
void thrones_child_exec(void *obj, ThronesChildWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (thrones_child_exec_table[i])
        thrones_child_exec_table[i](obj, w);
}

/* 0x00b188e8 stg05, 0x00b13548 stg22 */
#if defined(OVERLAY_STG22)
/* steer(cast) */
void thrones_child_post_exec(void *obj, ThronesChildWork *w)
{
    SpawnDesc desc;
    Vec3 scr;
    Vec3 scr2;
    int dead = 0;
    int mult = 0;
    ThronesChildRec *typ = w->type;
    ThronesChildPart *hit = 0;
    ThronesChildPart *part = w->part;
    int i;

    if (!enemy_status_get_ticks()) {
        thrones_child_follow_path(obj, w);
        thrones_child_pose_model(obj, w);
    }
    for (i = 0; i < 4; i++, part++) {
        if (part->lock.flags & LOCK_HIT) {
            nj_fill8(&desc, 0, sizeof desc);
            desc.type = 170;
            desc.models = stage_models;
            desc.kind = 0;
            desc.lock = enemy_task_lock_ptr(obj);
            v3_copy(&desc.pos, &part->pos);
            enemy_task_get_rot(obj, desc.ang);
            enemy_task_get_scl(obj, &desc.scale);
            v3_copy(&desc.v, &w->hitv);
            en_shatter_spawn(obj, &desc, 2);
            if (part->lock.life <= 0) {
                mult = lockon_hits_max_shot_cnt(&part->lock);
                hit = part;
                w->lost++;
            }
            if (w->lost < 4) {
                ulMatrixApplyPoint((float *)ulMatrixCur, &part->pos.x, &scr.x);
                project_point(&scr, &scr);
                SoundRhythmSe_EnemyPos(thrones_child_init_arg, 0, &scr.x);
            } else {
                ulMatrixApplyPoint((float *)ulMatrixCur, &part->pos.x, &scr2.x);
                project_point(&scr2, &scr2);
                SoundRhythmSe_EnemyPos(thrones_child_init_arg, 1, &scr2.x);
            }
        }
        if (part->lock.life < 1)
            dead++;
    }
    if (dead >= 4 && ((unsigned int)enemy_task_get_pstate(obj)) < 2) {
        enemy_task_set_state(obj, 3);
        if (mult > 0)
            enemy_status_score_popup(enemy_task_get_status_slot(obj), &hit->pos, typ->score, mult);
    }
}
#else
/* steer(cast) */
void thrones_child_post_exec(void *obj, ThronesChildWork *w)
{
    SpawnDesc desc;
    Vec3 scr;
    Vec3 scr2;
    int dead = 0;
    int mult = 0;
    ThronesChildRec *typ = w->type;
    ThronesChildPart *hit = 0;
    ThronesChildPart *part = w->part;
    int i;

    if (!enemy_status_get_ticks()) {
        thrones_child_follow_path(obj, w);
        thrones_child_pose_model(obj, w);
    }
    for (i = 0; i < 4; i++, part++) {
        if (part->lock.flags & LOCK_HIT) {
            nj_fill8(&desc, 0, sizeof desc);
            desc.type = 105;
            desc.models = stage_models;
            desc.kind = 0;
            desc.lock = enemy_task_lock_ptr(obj);
            v3_copy(&desc.pos, &part->pos);
            enemy_task_get_rot(obj, desc.ang);
            enemy_task_get_scl(obj, &desc.scale);
            v3_copy(&desc.v, &w->hitv);
            en_shatter_spawn(obj, &desc, 2);
            if (part->lock.life <= 0) {
                mult = lockon_hits_max_shot_cnt(&part->lock);
                hit = part;
                w->lost++;
            }
            if (w->lost < 4) {
                ulMatrixApplyPoint((float *)ulMatrixCur, &part->pos.x, &scr.x);
                project_point(&scr, &scr);
                SoundRhythmSe_EnemyPos(thrones_child_init_arg, 0, &scr.x);
            } else {
                ulMatrixApplyPoint((float *)ulMatrixCur, &part->pos.x, &scr2.x);
                project_point(&scr2, &scr2);
                SoundRhythmSe_EnemyPos(thrones_child_init_arg, 1, &scr2.x);
            }
        }
        if (part->lock.life < 1)
            dead++;
    }
    if (dead >= 4 && ((unsigned int)enemy_task_get_pstate(obj)) < 2) {
        enemy_task_set_state(obj, 3);
        if (mult > 0)
            enemy_status_score_popup(enemy_task_get_status_slot(obj), &hit->pos, typ->score, mult);
    }
}
#endif

/* 0x00b18b40 stg05, 0x00b137a0 stg22 */
void thrones_child_disp_body(void *obj, ThronesChildWork *w)
{
    Vec3 scl;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);

    enemy_task_get_scl(obj, &scl);
    if (view_sphere_visible(&w->pos, gfx->radius)) {
        ulGxSetTextureList(((void **)obj)[1]);
        ulMatrixPush();
        do {
            Vec3 *pos = &w->pos;
            vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
        } while (0);
        thrones_child_rotate_yxz(w->ang);
        vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
        dc_object_draw(gfx);
        ulMatrixPop();
    }
}

/* 0x00b18bf8 stg05, 0x00b13858 stg22 */
void thrones_child_disp_die(void *obj, ThronesChildWork *w)
{
    Vec3 scl;
    float col_add[4];
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    int prev;

    enemy_task_get_scl(obj, &scl);
    if (view_sphere_visible(&w->pos, gfx->radius)) {
        prev = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        heap_bzero(col_add, 16);
        col_add[0] = -(float)w->count / (w->type->die_sec * FPS);
        draw_push_strip_flags();
        draw_push_override_color();
        draw_set_strip_flags(0xfffffff, 2048);
        draw_set_override_color(col_add);
        ulGxSetTextureList(((void **)obj)[1]);
        ulMatrixPush();
        do {
            Vec3 *p_ = &w->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        thrones_child_rotate_yxz(w->ang);
        vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
        dc_object_draw(gfx);
        ulMatrixPop();
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(prev);
    }
}

/* 0x00b18d48 stg05, 0x00b139a8 stg22 */
void thrones_child_disp(void *obj, ThronesChildWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (thrones_child_disp_table[i])
        thrones_child_disp_table[i](obj, w);
}

/* 0x00b18da0 stg05, 0x00b13a00 stg22 */
int thrones_child_create(void *parent, ThronesChildArg *prm)
{
    ThronesChildKind kind;
    ObjDesc desc;
    ObjFuncs funcs = thrones_child_funcs;
    ThronesChildRec *typ;

    kind.idx = 0;
    kind.life = (int)prm->t0 + (int)prm->t1;
    typ = (ThronesChildRec *)&thrones_child_types[kind.idx];
    nj_fill8(&desc, 0, sizeof desc);
    desc.name = thrones_child_name;
    desc.funcs = &funcs;
    desc.arg = prm;
    desc.kind = &kind.idx;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.flags = 18;
    desc.dc_flags = 4096;
    desc.model_no = THRONES_CHILD_MODEL(148);
    desc.model_no2 = THRONES_CHILD_MODEL(149);
    desc.hp = 0;
    desc.same_lock = 0;
    desc.score = typ->score;
    desc.hp_reduce = typ->hp_recover;
    desc.crash_r = typ->crash_r;
    desc.crash_damage = typ->crash_damage;
    desc.time = kind.life;
    desc.size = 552;
    return enemy_task_task_create(parent, &desc);
}

/* 0x00b18f00 stg05, 0x00b13b60 stg22 */
void thrones_child_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
