/*
 * Tiamat's body and tail (stg03): lockable segments behind the head, each creating the next up to
 * the placement's length; each steers after the one ahead, swings when hit, breaks when shot.
 */

#include "types_common.h"
#include "task.h"

#include "types_overlays.h"

#include "mathdef.h"
#include "debug.h"
#include "anim.h"
#include "approach.h"
#include "color.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "model.h"
#include "nj_matrix.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "player.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "tiamat_body_and_tail.h"
#include "enemy_shatter.h"
#include "enemy_vars.h"
#include "system.h"

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

extern int ulMatrixPush(void);
extern struct GwVarNode *gw_varedit_root;

#define MAX_GE(a, b) ((a) >= (b) ? (a) : (b))

extern void *stage_textures;
extern void *stage_models;

extern int tiamat_body_and_tail_init_arg;
extern void (*tiamat_body_and_tail_exec_table[])(void *o, TiamatBodyAndTailWork *w);
extern void (*tiamat_body_and_tail_disp_table[])(void *o, TiamatBodyAndTailWork *w);
extern TiamatBodyAndTailVars tiamat_body_and_tail_types[1];
extern VarDef tiamat_body_and_tail_vardef;
extern VarDef tiamat_body_and_tail_vardefs[19];

extern ObjFuncs tiamat_body_and_tail_funcs;
extern char tiamat_body_and_tail_body_name[];
extern char tiamat_body_and_tail_tail_name[];
extern char tiamat_body_and_tail_body_lock_name[];
extern char tiamat_body_and_tail_tail_lock_name[];
extern Vec3 tiamat_body_and_tail_start_scl;

/* model list entry n as stg22 numbers it */
#if defined(OVERLAY_STG22)
#define TIAMAT_BODY_AND_TAIL_MODEL(n) ((n) + 10)
#else
#define TIAMAT_BODY_AND_TAIL_MODEL(n) (n)
#endif

/* 0x00b0ac60 stg03, 0x00b18628 stg22 */
void tiamat_body_and_tail_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *prm;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&tiamat_body_and_tail_vardef);
    for (i = 0; i < 1; i++) {
        def = tiamat_body_and_tail_vardefs;
        prm = (int *)&tiamat_body_and_tail_types[i].p;
        tiamat_body_and_tail_vardef.name = tiamat_body_and_tail_types[i].name;
        tiamat_body_and_tail_vardef.fmt = tiamat_body_and_tail_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&tiamat_body_and_tail_vardef);
        for (j = 0; j < 19; j++) {
            def->ptr = prm;
            prm++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)tiamat_body_and_tail_vardefs);
    }
}

/* 0x00b0ad78 stg03, 0x00b18740 stg22 */
void tiamat_body_and_tail_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b0ad80 stg03, 0x00b18748 stg22 */
int tiamat_body_and_tail_vars_init(int *fx_id)
{
    tiamat_body_and_tail_init_arg = *fx_id;
    tiamat_body_and_tail_vars_register();
    return 0;
}

/* 0x00b0ada8 stg03, 0x00b18770 stg22 */
int tiamat_body_and_tail_vars_release(void)
{
    tiamat_body_and_tail_vars_unregister();
    return 0;
}

/* 0x00b0adc8 stg03, 0x00b18790 stg22 */
void tiamat_body_and_tail_tip_pos(void *obj, TiamatBodyAndTailWork *w)
{
    Vec3 p;
    Vec3 ofs;
    int ang[3];
    float sc[2][2];
    float x, y, z;

    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    x = 0.0f;
    y = 0.0f;
    z = 5.0f;
    ROT_X(ang[0], sc[0], y, z);
    ROT_Y(ang[1], sc[1], x, z);
    v3_set(&ofs, x, y, z);
    v3_sum(&w->tip, &p, &ofs);
}

/* 0x00b0aed0 stg03, 0x00b18898 stg22 */
/* steer(cast) */
void tiamat_body_and_tail_init(void *obj, TiamatBodyAndTailWork *w, char *flag, int *kind)
{
    int *prm = (int *)kind[1];
    TiamatBodyAndTailVars *typ;
    Vec3 *pos;
    int fx;
    Vec3 p;
    Vec3 ofs;
    int lnk[4];
    float sc[2][2];
    float x, y, z;

    enemy_task_clear_flags(obj, 64);
    typ = &tiamat_body_and_tail_types[kind[0]];
    w->type = typ;
    w->link = *(TiamatChainLink *)prm;
    pos = enemy_task_pos_ptr(obj);
    fx = enemy_task_get_status_slot(obj);
    lockon_obj_init(&w->lock, obj, pos, 0, kind[2] >= 2 ? tiamat_body_and_tail_body_lock_name : tiamat_body_and_tail_tail_lock_name, fx,
                  prm[3] ? typ->p.hp : typ->p.hp_short, typ->p.same_lock, 128);
    lockon_entry(&w->lock);
    if (kind[2] >= 2) {
        dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(*(void **)obj, TIAMAT_BODY_AND_TAIL_MODEL(29))), *((struct DcClip **)pnb_get_entry(*(void **)obj, TIAMAT_BODY_AND_TAIL_MODEL(30))), 0x500);
        dc_object_set_wire(&w->body, model_pnb_get_wire(*(void **)obj, TIAMAT_BODY_AND_TAIL_MODEL(29)));
    } else {
        dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(*(void **)obj, TIAMAT_BODY_AND_TAIL_MODEL(33))), *((struct DcClip **)pnb_get_entry(*(void **)obj, TIAMAT_BODY_AND_TAIL_MODEL(34))), 0x500);
        dc_object_set_wire(&w->body, model_pnb_get_wire(*(void **)obj, TIAMAT_BODY_AND_TAIL_MODEL(33)));
    }
    dc_object_init(&w->ring, *((struct DcNode **)pnb_get_entry(*(void **)obj, TIAMAT_BODY_AND_TAIL_MODEL(36))), 0, 0);
    dc_object_rebuild(&w->ring);
    enemy_task_set_scl(obj, &tiamat_body_and_tail_start_scl);
    v3_copy(&w->anchor, (Vec3 *)prm[0]);
    x = 0.0f;
    y = 0.0f;
    z = 5.0f;
    ROT_X(((int *)prm[1])[0], sc[0], y, z);
    ROT_Y(((int *)prm[1])[1], sc[1], x, z);
    v3_set(&ofs, x, y, z);
    v3_sum(&p, (Vec3 *)prm[0], &ofs);
    enemy_task_set_pos(obj, &p);
    enemy_task_set_rot(obj, (int *)prm[1]);
    tiamat_body_and_tail_tip_pos(obj, w);
    w->life = kind[3];
    w->steering = 0;
    w->swing_count = 0;
    w->phase = *(float *)&kind[5] + *(float *)&kind[6];
    w->swing_phase = 0;
    w->pull = typ->p.rate;
    w->kind = kind[2];
    v3_set(&w->swing_dir, 0.0f, 0.0f, 0.0f);
    v3_set(&w->swing_base, 0.0f, 0.0f, 0.0f);
    w->big = prm[3];
    {
        Vec3 *v_ = &w->swing;

        ((union { float f; int i; } *)&v_->x)->f = 0.0f; /* steer(type): store scheduled late */
        v_->y = 0.0f;
        v_->z = 0.0f;
    }
    {
        int n_ = kind[2];

        *(int *)&w->head = kind[4]; /* steer(cast) */
        w->swing_rate = typ->p.damage_swing_rate;
        w->last = 0;
        kind[2] = --n_;
    }
    if (kind[2] > 0) {
        lnk[0] = (int)&w->tip;
        *(void **)&lnk[1] = enemy_task_rot_ptr(obj);
        lnk[2] = prm[2];
        lnk[3] = w->big;
        w->last = (void *)tiamat_body_and_tail_create(obj, (int)lnk, kind[2], kind[3], flag, w->phase, *(float *)&kind[6]);
    }
}

/* 0x00b0b2d0 stg03, 0x00b18c98 stg22 */
void tiamat_body_and_tail_release(void *obj, TiamatBodyAndTailWork *w)
{
    dc_object_release(&w->body);
    dc_object_release(&w->ring);
    lockon_remove(&w->lock);
}

/* 0x00b0b308 stg03, 0x00b18cd0 stg22 */
void tiamat_body_and_tail_aim(void *obj, TiamatBodyAndTailWork *w)
{
    int ang[3];
    Vec3 p;
    TiamatChainLink *lnk = &w->link;

    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    yuki_draw_aimed(lnk->pos, &p, ang);
    ang[2] = ang_approach(lnk->ang[2], ang[2], 0.99f);
    enemy_task_set_rot(obj, ang);
}

/* 0x00b0b3a0 stg03, 0x00b18d68 stg22 */
void tiamat_body_and_tail_forward(void *obj, int w, Vec3 *out)
{
    int ang[3];
    float sc[2][2];
    float x, y, z;

    enemy_task_get_rot(obj, ang);
    x = 0.0f;
    y = 0.0f;
    z = 5.0f;
    ROT_X(ang[0], sc[0], y, z);
    ROT_Y(ang[1], sc[1], x, z);
    v3_set(out, x, y, z);
}

/* 0x00b0b460 stg03, 0x00b18e28 stg22 */
void tiamat_body_and_tail_steer(void *obj, TiamatBodyAndTailWork *w, Vec3 *v)
{
    float rate = w->steer_rate;
    float unit = 1.0f;
    Vec3 *tgt = &w->steer_to;

    v->x = v->x * (unit - rate) + tgt->x * rate;
    v->y = v->y * (unit - rate) + tgt->y * rate;
    v->z = v->z * (unit - rate) + tgt->z * rate;
    vec3_normalize(v, v);
    v3_scale(v, 5.0f);
    rate = MIN(MAX_GE(rate + unit / w->type->p.pose_sec, 0.0f), 1.0f);
    w->steer_rate = rate;
}

/* 0x00b0b580 stg03, 0x00b18f48 stg22 */
void tiamat_body_and_tail_swing_offset(void *obj, TiamatBodyAndTailWork *w, Vec3 *out)
{
    TiamatBodyAndTailVars *typ = w->type;
    Vec3 base;
    Vec3 swing;
    float sc[2];

    v3_mul(&base, &w->swing_base, w->swing_count / (typ->p.damage_swing_sec * FPS + 1.0f));
    ulSinCos(w->swing_phase, &sc[0], &sc[1]);
    v3_mul(&swing, &w->swing, sc[0] * w->swing_count / (typ->p.damage_swing_sec * FPS + 1.0f));
    v3_sum(out, &swing, &base);
}

/* 0x00b0b6a8 stg03, 0x00b19070 stg22 */
void tiamat_body_and_tail_swing_dir(void *obj, TiamatBodyAndTailWork *w)
{
    Vec3 v;
    int ang[3];
    float sc[3][2];
    float x, y, z;

    tiamat_body_and_tail_swing_offset(obj, w, &v);
    do {
        x = v.x;
        y = v.y;
        z = v.z;
    } while (0);
    enemy_task_get_rot(obj, ang);
    ROT_Z(ang[2], sc[0], x, y);
    ROT_X(ang[0], sc[1], y, z);
    ROT_Y(ang[1], sc[2], x, z);
    v3_set(&w->swing_dir, x, y, z);
}

/* 0x00b0b7b0 stg03, 0x00b19178 stg22 */
void tiamat_body_and_tail_swing_start(void *obj, TiamatBodyAndTailWork *w)
{
    TiamatBodyAndTailVars *typ = w->type;
    Vec3 dir;
    float sc[2];

    if (w->swing_count <= 0 || typ->p.damage_swing_add != 0) {
        tiamat_body_and_tail_swing_offset(obj, w, &w->swing_base);
        ulSinCos(DEG_TO_ANG(RAND_FLOAT_A() * 360.0f), &sc[0], &sc[1]);
        v3_set(&dir, sc[1], sc[0], 0.0f);
        v3_mul(&w->swing, &dir, 5.0f);
        w->swing_phase = 0;
        w->swing_count = typ->p.damage_swing_sec * FPS + 1.0f;
        tiamat_body_and_tail_swing_dir(obj, w);
    }
}

/* 0x00b0b8d8 stg03, 0x00b192a0 stg22 */
void tiamat_body_and_tail_damage_swing(void *obj, TiamatBodyAndTailWork *w)
{
    int v = w->swing_count - 1;

    w->swing_count = v < 0 ? 0 : v;
    w->swing_phase = w->swing_phase + w->type->p.damage_swing_ang / FPS;
    if (w->swing_count > 0) {
        w->pull = w->type->p.damage_swing_rate2;
        tiamat_body_and_tail_chain_set_rate(w->last, (float *)&w->pull);
    } else {
        w->pull = w->type->p.rate;
        tiamat_body_and_tail_chain_set_rate(w->last, 0);
    }
}

/* 0x00b0b958 stg03, 0x00b19320 stg22 */
void tiamat_body_and_tail_follow(void *obj, TiamatBodyAndTailWork *w)
{
    Vec3 *off;
    Vec3 **tgt = &w->link.pos;
    Vec3 p;
    Vec3 pl;
    Vec3 dv;
    Vec3 pull;
    Vec3 fwd;
    float rate;
    int near;

    player_get_frame_pos(&pl);
    near = 0;
    v3_add(&w->anchor, &pl);
    v3_sub(&dv, *tgt, &w->anchor);
    if (fabsf(dv.x) < 0.01f && fabsf(dv.y) < 0.01f && fabsf(dv.z) < 0.01f)
        near = 1;
    if (near) {
        tiamat_body_and_tail_forward(obj, (int)w, &dv);
    } else {
        vec3_normalize(&dv, &dv);
        v3_mul(&pull, &dv, -5.0f);
        tiamat_body_and_tail_forward(obj, (int)w, &fwd);
        dv.x = pull.x * w->pull + fwd.x * (1.0f - w->pull);
        dv.y = pull.y * w->pull + fwd.y * (1.0f - w->pull);
        dv.z = pull.z * w->pull + fwd.z * (1.0f - w->pull);
        vec3_normalize(&dv, &dv);
        v3_mul(&dv, &dv, 5.0f);
    }
    if (w->steering)
        tiamat_body_and_tail_steer(obj, w, &dv);
    if (w->swing_count > 0) {
        rate = w->swing_rate;
        tiamat_body_and_tail_swing_dir(obj, w);
        dv.x = dv.x * (1.0f - rate) + w->swing_dir.x * rate;
        dv.y = dv.y * (1.0f - rate) + w->swing_dir.y * rate;
        dv.z = dv.z * (1.0f - rate) + w->swing_dir.z * rate;
        vec3_normalize(&dv, &dv);
        v3_mul(&dv, &dv, 5.0f);
        tiamat_body_and_tail_damage_swing(obj, w);
    }
    v3_sum(&p, *tgt, &dv);
    enemy_task_set_pos(obj, &p);
    enemy_task_set_param_vec(obj, &dv);
    off = &w->anchor;
    v3_sub(off, *tgt, &pl);
    tiamat_body_and_tail_aim(obj, w);
    tiamat_body_and_tail_tip_pos(obj, w);
}

/* 0x00b0bcb8 stg03, 0x00b19680 stg22 */
void tiamat_body_and_tail_exec_appear(void *obj, TiamatBodyAndTailWork *w)
{
    switch (enemy_task_get_phase(obj)) {
    case 0:
        enemy_task_appear_scale(obj, w);
        break;
    case 2:
        enemy_task_appear_scale(obj, w);
        w->lock.flags |= 0x100;
        break;
    default:
        enemy_task_appear_scale(obj, w);
        if (w->life > 0 && --w->life <= 0) {
            w->lock.flags &= ~0x100;
            enemy_task_set_state(obj, 2);
        }
        if (w->head == 0 || (TASK_OF(w->head)->flags & TASK_F_END))
            FINISH_IF(obj);
        break;
    }
}

/* 0x00b0bda8 stg03, 0x00b19770 stg22 */
void tiamat_body_and_tail_exec_normal(void *obj, TiamatBodyAndTailWork *w)
{
    LockObj *lock = &w->lock;

    if (enemy_task_get_phase(obj) != 1)
        return;
    if (w->life > 0 && --w->life <= 0) {
        lock->flags &= ~LOCK_LOCKABLE;
        enemy_task_set_state(obj, 2);
    }
    if (w->head == 0 || (TASK_OF(w->head)->flags & TASK_F_END))
        FINISH_IF(obj);
}

/* 0x00b0be50 stg03, 0x00b19818 stg22 */
void tiamat_body_and_tail_exec_leave(void *obj, TiamatBodyAndTailWork *w)
{
    if (enemy_task_get_phase(obj) != 1) {
        enemy_task_vanish_scale(obj, w);
        return;
    }
    enemy_task_vanish_scale(obj, w);
    if (w->head == 0 || (TASK_OF(w->head)->flags & TASK_F_END))
        FINISH_IF(obj);
}

/* 0x00b0bed8 stg03, 0x00b198a0 stg22 */
void tiamat_body_and_tail_exec_destroyed(void *obj, TiamatBodyAndTailWork *w)
{
    Vec3 p;
    Vec3 v;
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 1) {
        enemy_task_get_param_vec(obj, &v);
        if (!v3_is_zero(&v)) {
            enemy_task_get_pos(obj, &p);
            vec3_normalize(&v, &v);
            v3_scale(&v, w->type->p.die_vec * (1.0f / FPS));
            v3_add(&p, &v);
            enemy_task_set_pos(obj, &p);
        }
        if (enemy_task_get_state_time(obj) > w->fall) {
            nj_fill8(&desc, 0, sizeof desc);
            desc.kind = 0;
            desc.models = stage_models;
            desc.type = w->last ? TIAMAT_BODY_AND_TAIL_MODEL(31) : TIAMAT_BODY_AND_TAIL_MODEL(35);
            desc.lock = &w->lock;
            enemy_task_get_pos(obj, &desc.pos);
            enemy_task_get_rot(obj, desc.ang);
            enemy_task_get_param_vec(obj, &desc.v);
            enemy_task_get_scl(obj, &desc.scale);
            enemy_shatter_create(obj, &desc, 0, 0);
            FINISH_IF(obj);
        }
    }
}

/* 0x00b0c0b8 stg03, 0x00b19a80 stg22 */
void tiamat_body_and_tail_exec(void *obj, TiamatBodyAndTailWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (tiamat_body_and_tail_exec_table[i])
        tiamat_body_and_tail_exec_table[i](obj, w);
}

/* 0x00b0c110 stg03, 0x00b19ad8 stg22 */
void tiamat_body_and_tail_pre_exec(void *obj)
{
    Vec3 p, pl_vel;

    if (enemy_status_get_ticks() > 0 && enemy_task_get_pstate(obj) != 3) {
        enemy_task_get_pos(obj, &p);
        player_get_frame_vel(&pl_vel);
        p.x -= pl_vel.x;
        p.y -= pl_vel.y;
        p.z -= pl_vel.z;
        enemy_task_set_pos(obj, &p);
    }
}

/* 0x00b0c1a0 stg03, 0x00b19b68 stg22 */
#if defined(OVERLAY_STG22)
void tiamat_body_and_tail_post_exec(void *obj, TiamatBodyAndTailWork *w)
{
    LockObj *lock = &w->lock;
    SpawnDesc desc;
    Vec3 p;
    Vec3 pos;
    TiamatBodyAndTailVars *typ;
    int mult, hits;

    if (!enemy_status_get_ticks())
        tiamat_body_and_tail_tip_pos(obj, w);
    if (lock->flags & LOCK_HIT && enemy_task_get_pstate(obj) != 3) {
        v3_copy(&w->hitv, &lock->imp.v);
        tiamat_body_and_tail_swing_start(obj, w);
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 48;
        desc.models = stage_models;
        desc.kind = 0;
        desc.lock = lock;
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        v3_copy(&desc.v, &w->hitv);
        enemy_shatter_create(obj, &desc, 0, 0);
        enemy_task_get_pos(obj, &p);
        ulMatrixApplyPoint((float *)ulMatrixCur, &p.x, &p.x);
        project_point(&p, &p);
        hits = lock->life;
        if (!hits)
            SoundRhythmSe_EnemyPos(tiamat_body_and_tail_init_arg, 1, &p.x);
        else if (hits & 1)
            SoundRhythmSe_EnemyPos(tiamat_body_and_tail_init_arg, 2, &p.x);
        else
            SoundRhythmSe_EnemyPos(tiamat_body_and_tail_init_arg, 0, &p.x);
        if (lock->life <= 0) {
            mult = lockon_hits_max_shot_cnt(lock);
            typ = w->type;
            enemy_task_get_pos(obj, &pos);
            enemy_status_score_popup(enemy_task_get_status_slot(obj), &pos, w->big ? typ->p.score : typ->p.score_short, mult);
            (*w->link.hits)++;
        }
    }
}
#else
void tiamat_body_and_tail_post_exec(void *obj, TiamatBodyAndTailWork *w)
{
    LockObj *lock = &w->lock;
    SpawnDesc desc;
    Vec3 p;
    Vec3 pos;
    int hits;

    if (!enemy_status_get_ticks())
        tiamat_body_and_tail_tip_pos(obj, w);
    if ((lock->flags & LOCK_HIT) && enemy_task_get_pstate(obj) != 3) {
        v3_copy(&w->hitv, &lock->imp.v);
        tiamat_body_and_tail_swing_start(obj, w);
        nj_fill8(&desc, 0, sizeof desc);
        desc.kind = 0;
        desc.models = stage_models;
        desc.type = 38;
        desc.lock = lock;
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        v3_copy(&desc.v, &w->hitv);
        enemy_shatter_create(obj, &desc, 0, 0);
        enemy_task_get_pos(obj, &p);
        ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&p, (float *)&p);
        project_point(&p, &p);
        hits = lock->life;
        if (!hits)
            SoundRhythmSe_EnemyPos(tiamat_body_and_tail_init_arg, 1, (float *)&p);
        else if (hits & 1)
            SoundRhythmSe_EnemyPos(tiamat_body_and_tail_init_arg, 2, (float *)&p);
        else
            SoundRhythmSe_EnemyPos(tiamat_body_and_tail_init_arg, 0, (float *)&p);
        if (lock->life <= 0) {
            int mult = lockon_hits_max_shot_cnt(lock);
            TiamatBodyAndTailVars *typ = w->type;

            enemy_task_get_pos(obj, &pos);
            enemy_status_score_popup(enemy_task_get_status_slot(obj), &pos, w->big ? typ->p.score : typ->p.score_short, mult);
            (*w->link.hits)++;
        }
    }
}
#endif

/* 0x00b0c3b8 stg03, 0x00b19d80 stg22 */
int tiamat_body_and_tail_collect_node_matrices(float **out, unsigned int *node, int idx)
{
    do {
        ulMatrixPush();
        anim_apply_node(node);
        if (node[0] & 0x4000000) {
            njGetMatrix(*out);
            idx++;
            *out += 16;
        }
        if (!(node[0] & 0x10))
            idx = tiamat_body_and_tail_collect_node_matrices(out, (unsigned int *)node[11], idx);
        ulMatrixPop();
        node = (unsigned int *)node[12];
    } while (node);
    return idx;
}

/* 0x00b0c468 stg03, 0x00b19e30 stg22 */
void tiamat_body_and_tail_draw_at_nodes(ModelPart *mdl, ModelPart *src, float zscl)
{
    float buf[4][16];
    float (*p)[16];
    int n;

    p = buf;
    anim_start((void *)src->clip, src->frame);
    n = tiamat_body_and_tail_collect_node_matrices((float **)&p, (unsigned int *)src->root, 0);
    p = buf;
    while (n--) {
        ulMatrixPush();
        ulMatrixLoad(*p++);
        vu0_matrix_scale(ulMatrixCur, 1.0f, 1.0f, zscl);
        dc_object_draw(mdl);
        ulMatrixPop();
    }
}

/* 0x00b0c530 stg03, 0x00b19ef8 stg22 */
/* steer(cast) */
void tiamat_body_and_tail_disp_body(void *obj, TiamatBodyAndTailWork *w)
{
    Vec3 p;
    int ang[4];
    Vec3 scl;
    float body_rgba[4];
    float rgba[4];
    ModelPart *gfx;
    int hits;
    int blend;
    float beat;

    gfx = enemy_task_gfx_ptr(obj);
    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    enemy_task_get_scl(obj, &scl);
    if (!view_sphere_visible(&p, gfx->radius))
        return;
    ulGxSetTextureList(((void **)obj)[1]);
    ulMatrixPush();
    hits = w->lock.life;
    beat = beat_clock_get_phase() - w->phase;
    do {
        Vec3 *p_ = &p;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    tiamat_body_and_tail_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    while (beat < 0.0f)
        beat += 1.0f;
    while (1.0f < beat)
        beat -= 1.0f;
    if (hits <= 0) {
        unsigned int wcol = w->type->p.wire_col;
        int b = ((color_get_b(wcol) + 1) >> 1 & 0xff) << 16;
        int gr = (((int)color_get_g(wcol) + 1) >> 1 & 0xff) << 8;
        int r = ((int)((unsigned int (*)(int))color_get_r)(wcol) + 1) >> 1 & 0xff;
        unsigned int col;

        if (color_get_a(wcol) * scl.x > 254.0f)
            col = 0x7f000000 | b | gr | r;
        else
            col = ((int)(color_get_a(wcol) * scl.x) + 1) >> 1 << 24 | b | gr | r;
        draw_set_color(col);
        draw_set_blend(4, 1);
        model_object_draw((struct DcObjectTreeView *)&w->body, gfx->frame);
    } else if (w->big == 0) {
        dc_object_draw(gfx);
    } else {
        int prev = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);

        draw_push_strip_flags();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 0x800);
        draw_push_override_color();
        body_rgba[0] = scl.x - 1.0f;
        body_rgba[1] = body_rgba[2] = body_rgba[3] = 0.0f;
        draw_set_override_color(body_rgba);
        dc_object_draw(gfx);
        draw_pop_strip_flags();
        draw_pop_override_color();
        draw_set_override_flags(prev);
    }
    blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    heap_bzero((int *)rgba, 16);
    draw_push_strip_flags();
    draw_push_override_color();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 0x800);
    rgba[0] = scl.x - 1.0f;
    draw_set_override_color(rgba);
    tiamat_body_and_tail_draw_at_nodes(&w->ring, gfx, beat);
    draw_pop_strip_flags();
    draw_pop_override_color();
    draw_set_override_flags(blend);
    ulMatrixPop();
}

/* 0x00b0c890 stg03, 0x00b1a258 stg22 */
void tiamat_body_and_tail_disp(void *obj, TiamatBodyAndTailWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (tiamat_body_and_tail_disp_table[i])
        tiamat_body_and_tail_disp_table[i](obj, w);
}

/* 0x00b0c8e8 stg03, 0x00b1a2b0 stg22 */
int tiamat_body_and_tail_create(void *head, int lnk, int seg, int frames, char *flag, float phase, float phase_step)
{
    TiamatBodyAndTailKind kind;
    ObjDesc desc;
    ObjFuncs funcs = tiamat_body_and_tail_funcs;
    TiamatBodyAndTailVars *typ = tiamat_body_and_tail_types;
    int tail;

    kind.a = (void *)lnk;
    kind.phase = phase;
    kind.phase_step = phase_step;
    *flag = tiamat_body_and_tail_init_arg;
    kind.n = seg;
    kind.idx = 0;
    kind.time = frames;
    kind.head = head;
    nj_fill8(&desc, 0, sizeof desc);
    tail = seg < 2;
    desc.kind = &kind.idx;
    desc.name = !tail ? tiamat_body_and_tail_body_name : tiamat_body_and_tail_tail_name;
    desc.funcs = &funcs;
    desc.arg = flag;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->p.hp;
    desc.same_lock = typ->p.same_lock;
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = !tail ? TIAMAT_BODY_AND_TAIL_MODEL(28) : TIAMAT_BODY_AND_TAIL_MODEL(32);
    desc.model_no2 = !tail ? TIAMAT_BODY_AND_TAIL_MODEL(30) : TIAMAT_BODY_AND_TAIL_MODEL(34);
    desc.score = typ->p.score;
    desc.hp_reduce = typ->p.hp_reduce;
    desc.crash_r = !tail ? typ->p.crash_r_body : typ->p.crash_r_tail;
    desc.crash_damage = typ->p.crash_damage;
    desc.time = frames;
    desc.size = 408;
    return enemy_task_task_create(TASK_OF(head)->parent ? TASK_OF(head)->parent + 1 : 0, &desc);
}

/* 0x00b0ca98 stg03, 0x00b1a460 stg22 */
void tiamat_body_and_tail_chain_destroy(void *obj, int delay, int step)
{
    TiamatBodyAndTailWork *sub = enemy_task_work_ptr(obj);

    if ((unsigned int)enemy_task_get_pstate(obj) < 2)
        enemy_task_set_state(obj, 3);
    sub->fall = delay;
    sub->lock.flags &= ~0x80;
    if (sub->last)
        tiamat_body_and_tail_chain_destroy(sub->last, delay + step, step);
}

/* 0x00b0cb28 stg03, 0x00b1a4f0 stg22 */
void tiamat_body_and_tail_chain_set_points(void *obj, Vec3 *points, int count)
{
    TiamatBodyAndTailWork *sub = enemy_task_work_ptr(obj);

    if (!points) {
        sub->steering = 0;
        sub->steer_rate = 0;
        if (sub->last)
            tiamat_body_and_tail_chain_set_points(sub->last, 0, 0);
    } else {
        sub->steering = 1;
        sub->steer_to = *points;
        if (sub->last) {
            if (count >= 2)
                tiamat_body_and_tail_chain_set_points(sub->last, points + 1, count - 1);
            else
                tiamat_body_and_tail_chain_set_points(sub->last, 0, 0);
        }
    }
}

/* 0x00b0cbf0 stg03, 0x00b1a5b8 stg22 */
int tiamat_body_and_tail_chain_count_from(void *obj, int n)
{
    TiamatBodyAndTailWork *sub = enemy_task_work_ptr(obj);

    if (fabsf(sub->steer_rate - 1.0f) < FLT_EPSILON && sub->last)
        return tiamat_body_and_tail_chain_count_from(sub->last, n + 1);
    return n;
}

/* 0x00b0cc58 stg03, 0x00b1a620 stg22 */
int tiamat_body_and_tail_chain_count(void *w)
{
    return tiamat_body_and_tail_chain_count_from(w, 0);
}

/* 0x00b0cc78 stg03, 0x00b1a640 stg22 */
void tiamat_body_and_tail_chain_set_rate(void *obj, float *rate)
{
    TiamatBodyAndTailWork *sub;

    if (obj) {
        sub = enemy_task_work_ptr(obj);
        sub->pull = rate ? *rate : sub->type->p.rate;
        if (sub->last)
            tiamat_body_and_tail_chain_set_rate(sub->last, rate);
    }
}

/* 0x00b0ccd0 stg03, 0x00b1a698 stg22 */
void tiamat_body_and_tail_chain_swing_start(void *obj)
{
    if (obj)
        tiamat_body_and_tail_swing_start(obj, enemy_task_work_ptr(obj));
}

/* 0x00b0cd18 stg03, 0x00b1a6e0 stg22 */
void tiamat_body_and_tail_chain_set_swing_rate(void *obj, float *rate)
{
    TiamatBodyAndTailWork *sub;

    if (obj) {
        sub = enemy_task_work_ptr(obj);
        sub->swing_rate = rate ? *rate : sub->type->p.damage_swing_rate;
        if (sub->last)
            tiamat_body_and_tail_chain_set_swing_rate(sub->last, rate);
    }
}

/* 0x00b0cd70 stg03, 0x00b1a738 stg22 */
void tiamat_body_and_tail_chain_update(void *obj)
{
    TiamatBodyAndTailWork *sub;

    if (obj) {
        sub = enemy_task_work_ptr(obj);
        tiamat_body_and_tail_follow(obj, sub);
        tiamat_body_and_tail_chain_update(sub->last);
    }
}

/* 0x00b0cdc0 stg03, 0x00b1a788 stg22 */
void tiamat_body_and_tail_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
