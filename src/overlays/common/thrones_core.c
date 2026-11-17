/*
 * Thrones' cores: four lockable cores launched along their vectors with a sway; they slow, back
 * off, then speed away. The manager reports the group destroyed when all are shot, or escaped
 * once the rest got away.
 */

#include "types_common.h"

#include "types_overlays.h"
#include "debug.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "heap.h"
#include "lockon.h"
#include "model.h"
#include "nj_system.h"
#include "player.h"
#include "player_status.h"
#include "sound_midise.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "thrones_core.h"
#include "air_mng.h"
#include "en_backfire.h"
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

extern int ulMatrixPush(void);
extern struct GwVarNode *gw_varedit_root;

extern void *stage_textures;
extern void *stage_models;

extern int thrones_core_init_arg;
extern void (*thrones_core_exec_table[])(void *o, struct ThronesCoreWork *w);
extern void (*thrones_core_disp_table[])(void *o, struct ThronesCoreWork *w);
extern int thrones_core_manager_init_arg;
extern ThronesCoreSet thrones_core_types[1];
extern VarDef thrones_core_vardef;
extern VarDef thrones_core_vardefs[31];
extern void (*thrones_core_states[])(void *o, struct ThronesCoreWork *w);

extern Vec3 thrones_core_init_scale;
extern char thrones_core_label[];
extern char thrones_core_name[];
extern char thrones_core_score_name[];
extern ObjFuncs thrones_core_funcs;
extern struct TaskDesc thrones_core_manager_desc;

/* model list entry n as stg22 numbers it */
#if defined(OVERLAY_STG22)
#define THRONES_CORE_MODEL(n) ((n) + 65)
#else
#define THRONES_CORE_MODEL(n) (n)
#endif

#if defined(OVERLAY_STG22)
typedef struct ThronesCoreDestroyedWork {
    char pad_00[112];
    int *count;                 /* 0x70: counted up when it breaks */
} ThronesCoreDestroyedWork;
#endif

/* 0x00b18f50 stg05, 0x00b13bb0 stg22 */
void thrones_core_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&thrones_core_vardef);
    for (i = 0; i < 1; i++) {
        def = thrones_core_vardefs;
        val = thrones_core_types[i].v;
        thrones_core_vardef.name = thrones_core_types[i].name;
        thrones_core_vardef.fmt = thrones_core_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&thrones_core_vardef);
        for (j = 0; j < 31; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)thrones_core_vardefs);
    }
}

/* 0x00b19068 stg05, 0x00b13cc8 stg22 */
void thrones_core_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b19070 stg05, 0x00b13cd0 stg22 */
int thrones_core_get_init_arg(void)
{
    return thrones_core_init_arg;
}

/* 0x00b19080 stg05, 0x00b13ce0 stg22 */
int thrones_core_vars_init(int *prm)
{
    thrones_core_init_arg = *prm;
    thrones_core_vars_register();
    return 0;
}

/* 0x00b190a8 stg05, 0x00b13d08 stg22 */
int thrones_core_vars_release(void)
{
    thrones_core_vars_unregister();
    return 0;
}

/* 0x00b190c8 stg05, 0x00b13d28 stg22 */
void thrones_core_init(void *obj, ThronesCoreWork *w, int prm, ThronesCoreKind *k)
{
    ThronesCoreArg *arg = k->arg;

    w->type = (ThronesCoreRec *)&thrones_core_types[k->idx];
    enemy_task_clear_flags(obj, 64);
    do {
        void *m_ = stage_models, *t_ = stage_textures;
        ((void **)obj)[0] = m_;
        ((void **)obj)[1] = t_;
    } while (0);
    w->arg = *k->arg;
    enemy_task_set_scl(obj, &thrones_core_init_scale);
    w->state = 0;
    w->count = 1;
    w->time = arg->time;
    w->unk_3c = 0;
    w->roll = 0;
    lockon_obj_set_name(enemy_task_lock_ptr(obj), thrones_core_label);
    ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_SILENT;
    heap_bzero(w->link, 16);
    w->lost = 0;
    w->downed = 0;
    w->killed = 0;
    w->escaped = 0;
}

/* 0x00b191d8 stg05, 0x00b13e38 stg22 */
void thrones_core_release(void)
{
}

/* 0x00b191e0 stg05, 0x00b13e40 stg22 */
int thrones_core_next_state(void *obj, ThronesCoreWork *w, int n, int state)
{
    if (++w->count > n) {
        w->state = state;
        w->count = 1;
        return 1;
    }
    return 0;
}

/* 0x00b19210 stg05, 0x00b13e70 stg22 */
void thrones_core_state_launch(void *obj, ThronesCoreWork *w)
{
    ThronesCoreRec *typ = w->type;
    Vec3 npos;
    Vec3 p;
    Vec3 step;
    int ang[3];
    float sc[2];
    float rate, prev, acc, sway, psway, spd;
    float dt;
    int n;

    n = typ->shot_sec * FPS;
    rate = w->count / (float)n;
    prev = (w->count - 1) / (float)n;
    enemy_task_get_pos(obj, &p);
    acc = -(typ->shot_spd0 - typ->shot_spd1) / typ->shot_sec;
    dt = typ->shot_sec * rate;
    spd = typ->shot_spd0 + acc * dt;
    dt = FRAME_SEC;
    spd *= dt;
    spd += acc * 0.5f * dt * dt;
    enemy_task_get_param_vec(obj, &step);
    v3_scale(&step, spd);
    v3_sum(&npos, &p, &step);
    ulSinCos(rate * ANG_HALF, &sc[0], &sc[1]);
    sway = safe_powf(sc[0], typ->sway_pow);
    ulSinCos(typ->sway_spd * rate, &sc[0], &sc[1]);
    sway *= sc[0];
    ulSinCos(prev * ANG_HALF, &sc[0], &sc[1]);
    psway = safe_powf(sc[0], typ->sway_pow);
    ulSinCos(typ->sway_spd * prev, &sc[0], &sc[1]);
    psway *= sc[0];
    v3_mul(&step, &w->move, typ->sway * (sway - psway));
    v3_add(&npos, &step);
    enemy_task_set_pos(obj, &npos);
    enemy_task_get_rot(obj, ang);
    yuki_draw_aimed(&npos, &p, ang);
    enemy_task_set_rot(obj, ang);
    if (thrones_core_next_state(obj, w, n, 2)) {
        v3_sub(&w->move, &npos, &p);
        air_mng_stop(w->tail);
    }
}

/* 0x00b19530 stg05, 0x00b14190 stg22 */
#if defined(OVERLAY_STG22)
void thrones_core_state_back(void *obj, ThronesCoreWork *w)
{
    ThronesCoreRec *typ = w->type;
    Vec3 npos;
    Vec3 p;
    Vec3 step;
    TrailArg trail;
    float sc[2][2];
    ModelPart *gfx;
    float rate, prev, dis;
    int n;

    n = typ->back_sec * FPS;
    rate = w->count / (float)n;
    prev = (w->count - 1) / (float)n;
    ulSinCos(safe_powf(rate, typ->back_pow) * ANG_HALF - ANG_QUARTER, &sc[0][0], &sc[0][1]);
    dis = (sc[0][0] + 1.0f) * 0.5f;
    ulSinCos(safe_powf(prev, typ->back_pow) * ANG_HALF - ANG_QUARTER, &sc[0][0], &sc[0][1]);
    dis -= (sc[0][0] + 1.0f) * 0.5f;
    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &step);
    v3_scale(&step, -typ->back_dist * dis);
    v3_sum(&npos, &p, &step);
    v3_add(&npos, &w->move);
    enemy_task_set_pos(obj, &npos);
    ulSinCos(safe_powf(rate, typ->fire_pow2) * ANG_HALF, &sc[1][0], &sc[1][1]);
    w->fire = (sc[1][0] + safe_powf(rate, typ->fire_pow)) * typ->fire_size;
    gfx = enemy_task_gfx_ptr(obj);
    gfx->frame = gfx->frames * rate;
    w->roll = w->roll + typ->roll_spd / FPS * rate;
    if (thrones_core_next_state(obj, w, n, 3)) {
        w->speed = typ->shot_spd1;
        vec3_normalize(&w->move, &w->move);
        enemy_task_set_param_vec(obj, &w->move);
        heap_bzero(&trail, 40);
        trail.body = 0;
        trail.pos = enemy_task_pos_ptr(obj);
        trail.ang = enemy_task_rot_ptr(obj);
        trail.tail_seg = 1;
        trail.frames = w->type->tail_sec * FPS;
        trail.tail_tex = 0;
        trail.tail_width = w->type->tail_width;
        trail.one = 1;
        trail.tail_skip = typ->tail_skip;
        air_mng_create(obj, &trail);
    }
}
#else
void thrones_core_state_back(void *obj, ThronesCoreWork *w)
{
    ThronesCoreRec *typ = w->type;
    Vec3 npos;
    Vec3 p;
    Vec3 step;
    TrailArg trail;
    float sc[2][2];
    ModelPart *gfx;
    float rate, prev, dis;
    int n;

    n = typ->back_sec * FPS;
    rate = w->count / (float)n;
    prev = (w->count - 1) / (float)n;
    ulSinCos(safe_powf(rate, typ->back_pow) * ANG_HALF - ANG_QUARTER, &sc[0][0], &sc[0][1]);
    dis = (sc[0][0] + 1.0f) * 0.5f;
    ulSinCos(safe_powf(prev, typ->back_pow) * ANG_HALF - ANG_QUARTER, &sc[0][0], &sc[0][1]);
    dis -= (sc[0][0] + 1.0f) * 0.5f;
    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &step);
    v3_scale(&step, -typ->back_dist * dis);
    v3_sum(&npos, &p, &step);
    v3_add(&npos, &w->move);
    enemy_task_set_pos(obj, &npos);
    ulSinCos(safe_powf(rate, typ->fire_pow2) * ANG_HALF, &sc[1][0], &sc[1][1]);
    w->fire = (sc[1][0] + safe_powf(rate, typ->fire_pow)) * typ->fire_size;
    gfx = enemy_task_gfx_ptr(obj);
    gfx->frame = gfx->frames * rate;
    w->roll = w->roll + typ->roll_spd / FPS * rate;
    if (thrones_core_next_state(obj, w, n, 3)) {
        w->speed = typ->shot_spd1;
        vec3_normalize(&w->move, &w->move);
        enemy_task_set_param_vec(obj, &w->move);
        heap_bzero(&trail, 40);
        trail.body = 0;
        trail.pos = enemy_task_pos_ptr(obj);
        trail.ang = enemy_task_rot_ptr(obj);
        trail.tail_seg = 1;
        trail.frames = w->type->tail_sec * FPS;
        trail.tail_tex = 1;
        trail.tail_width = w->type->tail_width;
        trail.one = 1;
        trail.tail_skip = typ->tail_skip;
        air_mng_create(obj, &trail);
    }
}
#endif

/* 0x00b19870 stg05, 0x00b144d0 stg22 */
void thrones_core_state_accel(void *obj, ThronesCoreWork *w)
{
    ThronesCoreRec *typ = w->type;
    Vec3 npos;
    Vec3 p;
    Vec3 step;
    float sc[2];
    float rate, spd;
    int n;

    n = typ->accel_sec * FPS;
    rate = w->count / (float)n;
    enemy_task_get_pos(obj, &p);
    ulSinCos(safe_powf(rate, typ->accel_pow) * ANG_HALF - ANG_QUARTER, &sc[0], &sc[1]);
    sc[0] = (sc[0] + 1.0f) * 0.5f;
    spd = w->speed * (1.0f - sc[0]) + typ->accel_spd * sc[0];
    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &step);
    v3_scale(&step, spd * FRAME_SEC);
    v3_sum(&npos, &p, &step);
    enemy_task_set_pos(obj, &npos);
    w->roll = w->roll + typ->roll_spd / FPS;
    thrones_core_next_state(obj, w, n, 4);
}

/* 0x00b19a30 stg05, 0x00b14690 stg22 */
void thrones_core_state_finish(void *obj, ThronesCoreWork *w)
{
    ThronesCoreRec *typ = w->type;
    Vec3 npos;
    Vec3 p;
    Vec3 step;
    float sc[2];
    float rate, spd;
    int n;

    n = typ->finish_sec * FPS;
    rate = w->count / (float)n;
    enemy_task_get_pos(obj, &p);
    ulSinCos(safe_powf(rate, typ->finish_pow) * ANG_HALF - ANG_QUARTER, &sc[0], &sc[1]);
    sc[0] = (sc[0] + 1.0f) * 0.5f;
    spd = typ->accel_spd * (1.0f - sc[0]) + typ->finish_spd * sc[0];
    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &step);
    v3_scale(&step, spd * FRAME_SEC);
    v3_sum(&npos, &p, &step);
    enemy_task_set_pos(obj, &npos);
    w->roll = w->roll + typ->roll_spd / FPS;
    thrones_core_next_state(obj, w, n, 5);
}

/* 0x00b19bf0 stg05, 0x00b14850 stg22 */
/* steer(cast) */
void thrones_core_state_away(void *obj, ThronesCoreWork *w)
{
    ThronesCoreRec *typ;
    Vec3 npos;
    Vec3 p;
    Vec3 step;
    float spd;

    typ = w->type;
    enemy_task_get_pos(obj, &p);
    spd = typ->finish_spd;
    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &step);
    v3_scale(&step, spd * FRAME_SEC);
    v3_sum(&npos, &p, &step);
    enemy_task_set_pos(obj, &npos);
    w->roll = w->roll + typ->roll_spd / FPS;
    if (typ->away_sec * FPS < w->count && ((unsigned int (*)(void *))enemy_task_get_pstate)(obj) < 2)
        enemy_task_set_state(obj, 2);
    w->count++;
}

/* 0x00b19d40 stg05, 0x00b149a0 stg22 */
void thrones_core_update(void *obj, ThronesCoreWork *w)
{
    void (*fn)(void *obj, ThronesCoreWork *w) = thrones_core_states[w->state];

    if (fn)
        fn(obj, w);
}

/* 0x00b19d80 stg05, 0x00b149e0 stg22 */
void thrones_core_exec_appear(void *obj, ThronesCoreWork *w)
{
    enemy_task_appear_scale(obj, w);
    switch (enemy_task_get_phase(obj)) {
    case 0:
        ((LockObj *)enemy_task_lock_ptr(obj))->flags &= ~LOCK_LOCKABLE;
        break;
    case 2:
        ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_LOCKABLE;
        break;
    default:
        thrones_core_update(obj, w);
    }
}

/* 0x00b19e10 stg05, 0x00b14a70 stg22 */
#if defined(OVERLAY_STG22)
void thrones_core_exec_normal(void *obj, ThronesCoreWork *w)
{
    ThronesCoreRec *typ = w->type;
    int ang[3];
    Vec3 v;
    BackFireLink lnk;
    TrailArg trail;
    float dir_sx, dir_cx, dir_sy, dir_cy;
    float x, y;
    float sway_sx, sway_cx, sway_sy, sway_cy;
    float z;

    if (enemy_task_get_phase(obj) != 1)
        return;
    thrones_core_update(obj, w);
    if (w->time > 0 && --w->time <= 0) {
        w->state = 1;
        x = 0.0f;
        y = 0.0f;
        z = -1.0f;
        ((LockObj *)enemy_task_lock_ptr(obj))->life = 1;
        enemy_task_get_rot(obj, ang);
        do {
            float old_y = y, old_z = z;

            ulSinCos(ang[0], &dir_sx, &dir_cx);
            y = old_y * dir_cx - old_z * dir_sx;
            z = old_y * dir_sx + old_z * dir_cx;
        } while (0);
        do {
            float old_x = x, old_z = z;

            ulSinCos(ang[1], &dir_sy, &dir_cy);
            x = old_x * dir_cy + old_z * dir_sy;
            z = -old_x * dir_sy + old_z * dir_cy;
        } while (0);
        v3_set(&v, x, y, z);
        enemy_task_set_param_vec(obj, &v);
        ulSinCos(DEG_TO_ANG(RAND_FLOAT_A() * 360.0f), &x, &y);
        z = 0.0f;
        do {
            float old_y = y, old_z = z;

            ulSinCos(ang[0], &sway_sx, &sway_cx);
            y = old_y * sway_cx - old_z * sway_sx;
            z = old_y * sway_sx + old_z * sway_cx;
        } while (0);
        do {
            float old_x = x, old_z = z;

            ulSinCos(ang[1], &sway_sy, &sway_cy);
            x = old_x * sway_cy + old_z * sway_sy;
            z = -old_x * sway_sy + old_z * sway_cy;
        } while (0);
        {
            float t_ = y;

            v3_set(&w->move, x, t_, z);
        }
        heap_bzero(&lnk, 52);
        lnk.gfx = enemy_task_gfx_ptr(obj);
        lnk.pos = enemy_task_pos_ptr(obj);
        lnk.ang = enemy_task_rot_ptr(obj);
        lnk.obj_scale = enemy_task_scl_ptr(obj);
        lnk.scale = &w->fire;
        lnk.flags = 0x1000000;
        lnk.use_pose = 1;
        en_backfire_create(obj, &lnk, 0);
        w->fire = 0.0f;
        heap_bzero(&trail, 40);
        trail.body = 0;
        trail.pos = enemy_task_pos_ptr(obj);
        trail.ang = enemy_task_rot_ptr(obj);
        trail.tail_seg = 1;
        trail.frames = w->type->tail_sec * FPS;
        trail.tail_tex = 0;
        trail.tail_width = w->type->tail_width;
        trail.one = 1;
        trail.tail_skip = typ->tail_skip;
        w->tail = air_mng_create(obj, &trail);
        SoundRhythmSe_Enemy(thrones_core_init_arg, 6);
    }
}
#else
void thrones_core_exec_normal(void *obj, ThronesCoreWork *w)
{
    ThronesCoreRec *typ = w->type;
    int ang[3];
    Vec3 v;
    BackFireLink lnk;
    TrailArg trail;
    float dir_sx, dir_cx, dir_sy, dir_cy;
    float x, y;
    float sway_sx, sway_cx, sway_sy, sway_cy;
    float z;

    if (enemy_task_get_phase(obj) != 1)
        return;
    thrones_core_update(obj, w);
    if (w->time > 0 && --w->time <= 0) {
        w->state = 1;
        x = 0.0f;
        y = 0.0f;
        z = -1.0f;
        ((LockObj *)enemy_task_lock_ptr(obj))->life = 1;
        enemy_task_get_rot(obj, ang);
        do {
            float old_y = y, old_z = z;

            ulSinCos(ang[0], &dir_sx, &dir_cx);
            y = old_y * dir_cx - old_z * dir_sx;
            z = old_y * dir_sx + old_z * dir_cx;
        } while (0);
        do {
            float old_x = x, old_z = z;

            ulSinCos(ang[1], &dir_sy, &dir_cy);
            x = old_x * dir_cy + old_z * dir_sy;
            z = -old_x * dir_sy + old_z * dir_cy;
        } while (0);
        v3_set(&v, x, y, z);
        enemy_task_set_param_vec(obj, &v);
        ulSinCos(DEG_TO_ANG(RAND_FLOAT_A() * 360.0f), &x, &y);
        z = 0.0f;
        do {
            float old_y = y, old_z = z;

            ulSinCos(ang[0], &sway_sx, &sway_cx);
            y = old_y * sway_cx - old_z * sway_sx;
            z = old_y * sway_sx + old_z * sway_cx;
        } while (0);
        do {
            float old_x = x, old_z = z;

            ulSinCos(ang[1], &sway_sy, &sway_cy);
            x = old_x * sway_cy + old_z * sway_sy;
            z = -old_x * sway_sy + old_z * sway_cy;
        } while (0);
        {
            float t_ = y;

            v3_set(&w->move, x, t_, z);
        }
        heap_bzero(&lnk, 52);
        lnk.gfx = enemy_task_gfx_ptr(obj);
        lnk.pos = enemy_task_pos_ptr(obj);
        lnk.ang = enemy_task_rot_ptr(obj);
        lnk.obj_scale = enemy_task_scl_ptr(obj);
        lnk.scale = &w->fire;
        lnk.flags = 0x1000000;
        lnk.use_pose = 1;
        en_backfire_create(obj, &lnk, 0);
        w->fire = 0.0f;
        heap_bzero(&trail, 40);
        trail.body = 0;
        trail.pos = enemy_task_pos_ptr(obj);
        trail.ang = enemy_task_rot_ptr(obj);
        trail.tail_seg = 1;
        trail.frames = w->type->tail_sec * FPS;
        trail.tail_tex = 1;
        trail.tail_width = w->type->tail_width;
        trail.one = 1;
        trail.tail_skip = typ->tail_skip;
        w->tail = air_mng_create(obj, &trail);
        SoundRhythmSe_Enemy(thrones_core_init_arg, 6);
    }
}
#endif

/* 0x00b1a130 stg05, 0x00b14d90 stg22 */
void thrones_core_exec_leave(void *obj, ThronesCoreWork *w)
{
    enemy_task_vanish_scale(obj, w);
    switch (enemy_task_get_phase(obj)) {
    case 0:
        if (w->escaped)
            (*w->escaped)++;
        ((LockObj *)enemy_task_lock_ptr(obj))->flags &= ~LOCK_LOCKABLE;
        break;
    case 2:
        break;
    default:
        thrones_core_update(obj, w);
    }
}

/* 0x00b1a1c8 stg05, 0x00b14e28 stg22 */
#if defined(OVERLAY_STG22)
void thrones_core_exec_destroyed(void *obj, ThronesCoreDestroyedWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 0) {
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 72;
        desc.models = stage_models;
        desc.kind = 0;
        desc.lock = enemy_task_lock_ptr(obj);
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        enemy_task_get_param_vec(obj, &desc.v);
        en_shatter_spawn(obj, &desc, 2);
        if (w->count)
            (*w->count)++;
        FINISH_IF(obj);
    }
}
#else
void thrones_core_exec_destroyed(void *obj, ThronesCoreWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 0) {
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 118;
        desc.models = stage_models;
        desc.kind = 0;
        desc.lock = enemy_task_lock_ptr(obj);
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        enemy_task_get_param_vec(obj, &desc.v);
        en_shatter_spawn(obj, &desc, 2);
        if (w->killed)
            (*w->killed)++;
        FINISH_IF(obj);
    }
}
#endif

/* 0x00b1a2a0 stg05, 0x00b14f00 stg22 */
void thrones_core_pre_exec(void *obj, ThronesCoreWork *w)
{
    Vec3 p, org;

    if (w->state) {
        enemy_task_get_pos(obj, &p);
        player_get_frame_vel(&org);
        p.x -= org.x;
        p.y -= org.y;
        p.z -= org.z;
        player_get_vec(&org);
        p.x += org.x;
        p.y += org.y;
        p.z += org.z;
        enemy_task_set_pos(obj, &p);
    }
}

/* 0x00b1a358 stg05, 0x00b14fb8 stg22 */
void thrones_core_exec(void *obj, ThronesCoreWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (thrones_core_exec_table[i])
        thrones_core_exec_table[i](obj, w);
}

/* 0x00b1a3b0 stg05, 0x00b15010 stg22 */
/* steer(cast) */
void thrones_core_post_exec(void *obj, ThronesCoreWork *w)
{
    SpawnDesc desc;
    Vec3 scr;
    Vec3 scr2;
    Vec3 scr3;
    LockObj *lock = enemy_task_lock_ptr(obj);
    ThronesCoreWork *oth;
    int hits, i;

    if (w->state == 0) {
        enemy_task_set_pos(obj, w->arg.pos);
        enemy_task_set_rot(obj, w->arg.ang);
    }
    if (lock->flags & LOCK_HIT) {
        hits = lock->life;
        v3_copy(&w->hitv, &lock->imp.v);
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = THRONES_CORE_MODEL(105);
        desc.models = stage_models;
        desc.kind = 0;
        desc.lock = enemy_task_lock_ptr(obj);
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        v3_copy(&desc.v, &w->hitv);
        en_shatter_spawn(obj, &desc, 2);
        switch (hits) {
        case 0:
            if (((unsigned int (*)(void *))enemy_task_get_pstate)(obj) < 2)
                enemy_task_set_state(obj, 3);
            if (w->state <= 0) {
                if (w->lost != 3) {
                    enemy_task_get_pos(obj, &scr);
                    ulMatrixApplyPoint((float *)ulMatrixCur, &scr.x, &scr.x);
                    project_point(&scr, &scr);
                    SoundRhythmSe_EnemyPos(thrones_core_init_arg, 1, &scr.x);
                }
            } else {
                enemy_task_get_pos(obj, &scr);
                ulMatrixApplyPoint((float *)ulMatrixCur, &scr.x, &scr.x);
                project_point(&scr, &scr);
                SoundRhythmSe_EnemyPos(thrones_core_init_arg, 4, &scr.x);
                if (w->downed == 3)
                    player_status_add_score_event(thrones_core_score_name, w->type->bonus_score);
                {
                    int j;

                    for (j = 0; j < 4; j++)
                        if (w->link[j] && w->link[j] != obj)
                            ((ThronesCoreWork *)enemy_task_work_ptr(w->link[j]))->downed++;
                }
            }
            for (i = 0; i < 4; i++) {
                if (w->link[i] && w->link[i] != obj) {
                    oth = enemy_task_work_ptr(w->link[i]);
                    oth->link[i] = 0;
                    oth->lost++;
                }
            }
            break;
        case 1:
            enemy_task_get_pos(obj, &scr);
            ulMatrixApplyPoint((float *)ulMatrixCur, &scr.x, &scr.x);
            project_point(&scr, &scr);
            SoundRhythmSe_EnemyPos(thrones_core_init_arg, 3, &scr.x);
            break;
        case 2:
            enemy_task_get_pos(obj, &scr2);
            ulMatrixApplyPoint((float *)ulMatrixCur, &scr2.x, &scr2.x);
            project_point(&scr2, &scr2);
            SoundRhythmSe_EnemyPos(thrones_core_init_arg, 2, &scr2.x);
            break;
        case 3:
            enemy_task_get_pos(obj, &scr3);
            ulMatrixApplyPoint((float *)ulMatrixCur, &scr3.x, &scr3.x);
            project_point(&scr3, &scr3);
            SoundRhythmSe_EnemyPos(thrones_core_init_arg, 0, &scr3.x);
            break;
        }
    }
    {
        int j;

        for (j = 0; j < 4; j++)
            if (w->link[j] && (TASK_OF(w->link[j])->flags & TASK_F_END))
                w->link[j] = 0;
    }
}

/* 0x00b1a7b8 stg05, 0x00b15418 stg22 */
void thrones_core_disp_body(void *obj, ThronesCoreWork *w)
{
    Vec3 p;
    int ang[3];
    Vec3 scl;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);

    enemy_task_get_pos(obj, &p);
    if (view_sphere_visible(&p, gfx->radius)) {
        enemy_task_get_rot(obj, ang);
        enemy_task_get_scl(obj, &scl);
        ulGxSetTextureList(((void **)obj)[1]);
        do {
            ulMatrixPush();
        } while (0);
        vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
        thrones_core_rotate_yxz(ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_rotate_z(ulMatrixCur, w->roll);
        vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
        dc_object_draw(gfx);
        ulMatrixPop();
    }
}

/* 0x00b1a8a0 stg05, 0x00b15500 stg22 */
void thrones_core_disp(void *obj, ThronesCoreWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (thrones_core_disp_table[i])
        thrones_core_disp_table[i](obj, w);
}

/* 0x00b1a8f8 stg05, 0x00b15558 stg22 */
#if defined(OVERLAY_STG22)
int thrones_core_create(void *owner, ThronesCoreArg *core_prm)
{
    ThronesCoreKind kind;
    ObjDesc desc;
    ObjFuncs funcs = thrones_core_funcs;
    ShotArg prm;
    ThronesCoreRec *typ;

    kind.idx = 0;
    kind.arg = core_prm;
    typ = (ThronesCoreRec *)&thrones_core_types[kind.idx];
    nj_fill8(&prm, 0, sizeof prm);
    prm.kind = thrones_core_init_arg;
    prm.fx_arg = -1;
    prm.pos = *core_prm->pos;
    *(ThronesCoreAng *)prm.ang = *(ThronesCoreAng *)core_prm->ang;
    prm.vel.x = 0;
    prm.vel.y = 0;
    prm.vel.z = 0;
    nj_fill8(&desc, 0, sizeof desc);
    desc.name = thrones_core_name;
    desc.funcs = &funcs;
    desc.arg = &prm;
    desc.kind = &kind.idx;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->hp;
    desc.same_lock = typ->same_lock;
    desc.flags = 16;
    desc.dc_flags = 0;
    desc.model_no = 72;
    desc.model_no2 = 73;
    desc.score = typ->score;
    desc.hp_reduce = typ->hp_recover;
    desc.crash_r = typ->crash_r;
    desc.crash_damage = typ->crash_damage;
    desc.time = core_prm->time;
    desc.size = 120;
    return enemy_task_task_create(TASK_OF(owner)->parent ? TASK_OF(owner)->parent + 1 : 0, &desc);
}
#else
int thrones_core_create(void *owner, ThronesCoreArg *core_prm)
{
    ThronesCoreKind kind;
    ObjDesc desc;
    ObjFuncs funcs = thrones_core_funcs;
    ShotArg prm;
    ThronesCoreRec *typ;

    kind.idx = 0;
    kind.arg = core_prm;
    typ = (ThronesCoreRec *)&thrones_core_types[kind.idx];
    nj_fill8(&prm, 0, sizeof prm);
    prm.kind = thrones_core_init_arg;
    prm.fx_arg = -1;
    prm.pos = *core_prm->pos;
    *(ThronesCoreAng *)prm.ang = *(ThronesCoreAng *)core_prm->ang;
    prm.vel.x = 0;
    prm.vel.y = 0;
    prm.vel.z = 0;
    nj_fill8(&desc, 0, sizeof desc);
    desc.name = thrones_core_name;
    desc.funcs = &funcs;
    desc.arg = &prm;
    desc.kind = &kind.idx;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->hp;
    desc.same_lock = typ->same_lock;
    desc.flags = 16;
    desc.dc_flags = 0;
    desc.model_no = 118;
    desc.model_no2 = 119;
    desc.score = typ->score;
    desc.hp_reduce = typ->hp_recover;
    desc.crash_r = typ->crash_r;
    desc.crash_damage = typ->crash_damage;
    desc.time = core_prm->time;
    desc.size = 120;
    return enemy_task_task_create(TASK_OF(owner)->parent ? TASK_OF(owner)->parent + 1 : 0, &desc);
}
#endif

/* 0x00b1aab8 stg05, 0x00b15718 stg22 */
void *thrones_core_get_lock(void *obj)
{
    return enemy_task_lock_ptr(obj);
}

/* 0x00b1aad8 stg05, 0x00b15738 stg22 */
void thrones_core_set_siblings(void *obj, int core0, int core1, int core2, int core3)
{
    int *sub = enemy_task_work_ptr(obj);

    sub[22] = core0;
    sub[23] = core1;
    sub[24] = core2;
    sub[25] = core3;
}

/* 0x00b1ab30 stg05, 0x00b15790 stg22 */
void thrones_core_set_counters(void *obj, int *killed, int *escaped)
{
    ThronesCoreWork *w = enemy_task_work_ptr(obj);

    w->killed = killed;
    w->escaped = escaped;
}

/* 0x00b1ab68 stg05, 0x00b157c8 stg22 */
void thrones_core_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}

/* 0x00b1abb8 stg05, 0x00b15818 stg22 */
int thrones_core_manager_get_init_arg(void)
{
    return thrones_core_manager_init_arg;
}

/* 0x00b1abc8 stg05, 0x00b15828 stg22 */
int thrones_core_manager_vars_init(int *prm)
{
    thrones_core_manager_init_arg = *prm;
    return 0;
}

/* 0x00b1abe0 stg05, 0x00b15840 stg22 */
int thrones_core_manager_vars_release(void)
{
    return 0;
}

/* 0x00b1abe8 stg05, 0x00b15848 stg22 */
void thrones_core_manager_init(ThronesCoreManagerWork *w, ThronesCoreManagerArg *prm)
{
    int i;

    for (i = 0; i < 4; i++) {
        w->objs[i] = prm->objs[i];
        thrones_core_set_counters(prm->objs[i], &w->killed, &w->escaped);
    }
    w->escaped = 0;
    w->killed = 0;
    w->fx = enemy_status_ent_alloc(thrones_core_manager_init_arg, prm->fx, w);
    enemy_task_get_pos(w->objs[0], &w->pos);
    enemy_status_event(w->fx, 2, &w->pos);
    w->op = 3;
}

/* 0x00b1acd0 stg05, 0x00b15930 stg22 */
void thrones_core_manager_release(ThronesCoreManagerWork *w)
{
    int i;

    for (i = 0; i < 4; i++) {
        if (w->objs[i]) {
            enemy_task_get_pos(w->objs[i], &w->pos);
            break;
        }
    }
    enemy_status_event(w->fx, w->op, &w->pos);
}

/* 0x00b1ad38 stg05, 0x00b15998 stg22 */
void thrones_core_manager_fire(ThronesCoreManagerWork *w, int op)
{
    int i;

    for (i = 0; i < 4; i++) {
        if (w->objs[i]) {
            enemy_task_get_pos(w->objs[i], &w->pos);
            break;
        }
    }
    enemy_status_event(w->fx, op, &w->pos);
    w->op = 0;
    FINISH_IF(w);
}

/* 0x00b1adc0 stg05, 0x00b15a20 stg22 */
void thrones_core_manager_exec(ThronesCoreManagerWork *w)
{
    int i;

    if (w->killed >= 4)
        thrones_core_manager_fire(w, 3);
    else if (w->escaped + w->killed >= 4)
        thrones_core_manager_fire(w, 4);
    for (i = 3; i >= 0; i--) {
        if (w->objs[i]) {
            if (TASK_OF(w->objs[i])->flags & TASK_F_END)
                w->objs[i] = 0;
            else
                enemy_task_get_pos(w->objs[i], &w->pos);
        }
    }
}

/* 0x00b1ae70 stg05, 0x00b15ad0 stg22 */
void *thrones_core_manager_create(void *parent, void *core0, void *core1, void *core2, void *core3, int fx)
{
    ThronesCoreManagerArg arg;

    arg.objs[0] = core0;
    arg.objs[1] = core1;
    arg.objs[2] = core2;
    arg.objs[3] = core3;
    arg.fx = fx;
    return task_create_sibling_inherit_arg(parent, &thrones_core_manager_desc, 44, &arg);
}
