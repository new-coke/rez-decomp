/*
 * Archiver, the item carrier: its core flies in, springs to a point beside the player and holds
 * there, ring spinning. Shot down, it breaks and changes the player's form; left alone, it leaves.
 */

#include "types_common.h"

#include "types_overlays.h"
#include "types_gw.h"

#include "debug.h"
#include "approach.h"
#include "camera.h"
#include "change_symbol.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "map_manager.h"
#include "math_util.h"
#include "model.h"
#include "nj_system.h"
#include "player.h"
#include "player_camera.h"
#include "pnb.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "task.h"
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

static inline void i3_copy(int *dst, int *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

extern int ulMatrixPush(void);
extern struct GwVarNode *gw_varedit_root;

extern void *item_models;
extern void *item_textures;
extern char archiver_lock_name[];
extern char archiver_vardef[];
extern GwVarDef archiver_vardefs[];
extern struct TaskDesc archiver_desc;
extern float archiver_rate;
extern float archiver_regist;
extern float archiver_dist2;
extern float archiver_lspd;
extern float archiver_ah;
extern float archiver_arate;
extern float archiver_aregist;
extern float archiver_aangz;
extern float archiver_aangx;
extern float archiver_awayspd;
extern float archiver_awayfrm;
extern int archiver_stop1;
extern int archiver_stop2;
extern int archiver_vars_group;
void archiver_change_symbol(ArchiverWork *w);
void archiver_shatter(ArchiverWork *w);
void archiver_calc_target(ArchiverWork *w);
void archiver_calc_velocity(ArchiverWork *w);
int archiver_node_cb(unsigned int *node, int idx, ArchiverWork *w);
void archiver_models_init(ArchiverWork *w);
void archiver_start_motion(ArchiverWork *w);
void archiver_models_release(ArchiverWork *w);
void archiver_turn_to_player(ArchiverWork *w);
void archiver_turn_to_dir(ArchiverWork *w, Vec3 *dir);
void archiver_spring(ArchiverWork *w);
void archiver_update(ArchiverWork *w);
void archiver_init(ArchiverWork *w, ArchiverArg *prm);
void archiver_release(ArchiverWork *w);
void archiver_exec(ArchiverWork *w);
void archiver_disp(ArchiverWork *w);
void *archiver_create(void *parent, void *prm);
int archiver_marker_draw(void **w, ArchiverArg *prm);
int archiver_vars_init(void);
int archiver_vars_release(void);
void archiver_rotate_yxz(int *ang);

/* 0x00b0ba80 stg01, 0x00b16d10 stg02, 0x00b0fe68 stg03, 0x00b166e0 stg04, 0x00af8560 stg20, 0x00b1f3f0 stg22 */
void archiver_change_symbol(ArchiverWork *w)
{
    float pos[4];
    CsArg cs;

    ulMatrixGetTranslation(w->mtx, pos);
    ulMatrixApplyPoint(camera_get_inv_mtx(), pos, pos);
    cs.pos.x = pos[0];
    cs.pos.y = pos[1];
    cs.pos.z = pos[2];
    cs.scale = 0.9f;
    cs.ang[0] = 0;
    cs.ang[1] = 0;
    cs.ang[2] = 0;
    cs.scl[0] = 0;
    cs.scl[1] = 0;
    cs.scl[2] = 0;
    cs.skip = 0;
    change_symbol_create(&cs);
}

/* 0x00b0bb08 stg01, 0x00b16d98 stg02, 0x00b0fef0 stg03, 0x00b16768 stg04, 0x00af85e8 stg20, 0x00b1f478 stg22 */
void archiver_shatter(ArchiverWork *w)
{
    SpawnDesc desc;

    nj_fill8(&desc, 0, sizeof desc);
    desc.time = w->body.frame;
    v3_copy(&desc.pos, &w->pos);
    v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
    desc.kind = 0x40000;
    desc.type = 13;
    desc.motion = w->motion;
    desc.models = item_models;
    desc.lock = &w->lock;
    i3_copy(desc.ang, w->ang);
    v3_copy(&desc.v, &w->lock.imp.v);
    enemy_shatter_create(w, &desc, 0, 0);
    desc.type = 16;
    desc.kind = 0;
    i3_copy(desc.ang, w->ang2);
    enemy_shatter_create(w, &desc, 0, 0);
}

/* Player position plus the player's rotated axes scaled by side, up and current distance. */
/* 0x00b0bc28 stg01, 0x00b16eb8 stg02, 0x00b10010 stg03, 0x00b16888 stg04, 0x00af8708 stg20, 0x00b1f598 stg22 */
void archiver_calc_target(ArchiverWork *w)
{
    Vec3 tgt, ax_side, ax_up, ax_fwd;
    int ang[3];
    Vec3 pl;
    float sc[9][2];
    float x, y, z;
    float dst;

    player_camera_get_angles(ang);
    x = 1.0f;
    y = 0.0f;
    z = 0.0f;
    ROT_Z(ang[2], sc[0], x, y);
    ROT_X(ang[0], sc[1], y, z);
    ROT_Y(ang[1], sc[2], x, z);
    v3_set(&ax_side, x, y, z);
    y = 1.0f;
    x = 0.0f;
    z = 0.0f;
    ROT_Z(ang[2], sc[3], x, y);
    ROT_X(ang[0], sc[4], y, z);
    ROT_Y(ang[1], sc[5], x, z);
    v3_set(&ax_up, x, y, z);
    x = 0.0f;
    y = 0.0f;
    z = -1.0f;
    ROT_Z(ang[2], sc[6], x, y);
    ROT_X(ang[0], sc[7], y, z);
    ROT_Y(ang[1], sc[8], x, z);
    v3_set(&ax_fwd, x, y, z);
    player_get_pos(&pl);
    dst = ulVec3Distance(&w->pos, &pl);
    tgt.x = ax_fwd.x * dst;
    tgt.y = ax_fwd.y * dst;
    tgt.z = ax_fwd.z * dst;
    v3_add(&tgt, &pl);
    w->tgt.x = tgt.x + ax_side.x * w->side + ax_up.x * w->up;
    w->tgt.y = tgt.y + ax_side.y * w->side + ax_up.y * w->up;
    w->tgt.z = tgt.z + ax_side.z * w->side + ax_up.z * w->up;
}

/* Along the player's view axis, pointing away from its direction. */
/* 0x00b0bef8 stg01, 0x00b17188 stg02, 0x00b102e0 stg03, 0x00b16b58 stg04, 0x00af89d8 stg20, 0x00b1f868 stg22 */
void archiver_calc_velocity(ArchiverWork *w)
{
    int ang[3];
    Vec3 v;
    float sc[3][2];
    float x, y, z;
    float spd;

    player_camera_get_angles(ang);
    x = 0.0f;
    y = 0.0f;
    z = -1.0f;
    ROT_Z(ang[2], sc[0], x, y);
    ROT_X(ang[0], sc[1], y, z);
    ROT_Y(ang[1], sc[2], x, z);
    v.x = x;
    v.y = y;
    v.z = z;
    if (ulVec3Dot(&v, &w->dir) >= 0.0f) {
        v.x = -v.x;
        v.y = -v.y;
        v.z = -v.z;
    }
    spd = archiver_awayspd;
    v3_mul(&w->vel, &v, spd);
}

/* 0x00b0c048 stg01, 0x00b172d8 stg02, 0x00b10430 stg03, 0x00b16ca8 stg04, 0x00af8b28 stg20, 0x00b1f9b8 stg22 */
int archiver_node_cb(unsigned int *node, int idx, ArchiverWork *w)
{
    if (*node & 0x8000000)
        vu0_matrix_copy(w->mtx, ulMatrixCur);
    return 1;
}

/* 0x00b0c080 stg01, 0x00b17310 stg02, 0x00b10468 stg03, 0x00b16ce0 stg04, 0x00af8b60 stg20, 0x00b1f9f0 stg22 */
/* steer(cast) */
void archiver_models_init(ArchiverWork *w)
{
    int root;

    w->motion = 14;
    root = *((int *(*)(void *, unsigned int))pnb_get_entry)(w->models, 13);
    dc_object_init(&w->body, root, *((int *(*)(void *, unsigned int))pnb_get_entry)(w->models, w->motion), 0x1000);
    dc_object_rebuild(&w->body);
    w->body.radius = 16.0f;
    dc_object_init(&w->ring, *((int *(*)(void *, unsigned int))pnb_get_entry)(w->models, 16), 0, 0x1000);
    dc_object_rebuild(&w->ring);
    dc_object_set_callback(&w->ring, archiver_node_cb, w);
}

/* 0x00b0c148 stg01, 0x00b173d8 stg02, 0x00b10530 stg03, 0x00b16da8 stg04, 0x00af8c28 stg20, 0x00b1fab8 stg22 */
/* steer(cast) */
void archiver_start_motion(ArchiverWork *w)
{
    int clip;

    w->motion = 15;
    clip = *((int *(*)(void *, unsigned int))pnb_get_entry)(w->models, 15);
    dc_object_set_clip(&w->body, clip);
    dc_object_clear_flags(&w->body, 0x1000);
}

/* 0x00b0c198 stg01, 0x00b17428 stg02, 0x00b10580 stg03, 0x00b16df8 stg04, 0x00af8c78 stg20, 0x00b1fb08 stg22 */
void archiver_models_release(ArchiverWork *w)
{
    dc_object_release(&w->body);
    dc_object_release(&w->ring);
}

/* 0x00b0c1c8 stg01, 0x00b17458 stg02, 0x00b105b0 stg03, 0x00b16e28 stg04, 0x00af8ca8 stg20, 0x00b1fb38 stg22 */
void archiver_turn_to_player(ArchiverWork *w)
{
    Vec3 pl;
    Vec3 dir;
    int aim[3];

    player_get_pos(&pl);
    v3_sub(&dir, &pl, &w->pos);
    do {
        aim[1] = (int)RAD_TO_ANG(atan2f(-dir.x, -dir.z));
        aim[0] = (int)RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
    } while (0);
    aim[2] = 0;
    w->ang[0] = ang_approach(aim[0], w->ang[0], archiver_lspd);
    w->ang[1] = ang_approach(aim[1], w->ang[1], archiver_lspd);
    w->ang[2] = ang_approach(aim[2], w->ang[2], archiver_lspd);
}

/* 0x00b0c310 stg01, 0x00b175a0 stg02, 0x00b106f8 stg03, 0x00b16f70 stg04, 0x00af8df0 stg20, 0x00b1fc80 stg22 */
void archiver_turn_to_dir(ArchiverWork *w, Vec3 *dir)
{
    int aim[3];

    do {
        aim[1] = (int)RAD_TO_ANG(atan2f(-dir->x, -dir->z));
        aim[0] = (int)RAD_TO_ANG(atan2f(dir->y, sqrtf(dir->x * dir->x + dir->z * dir->z)));
    } while (0);
    aim[2] = 0;
    w->ang[0] = ang_approach(aim[0], w->ang[0], archiver_lspd);
    w->ang[1] = ang_approach(aim[1], w->ang[1], archiver_lspd);
    w->ang[2] = ang_approach(aim[2], w->ang[2], archiver_lspd);
}

/* Spring follows the core; tilt by the lag, at most 60 degrees. */
/* 0x00b0c428 stg01, 0x00b176b8 stg02, 0x00b10810 stg03, 0x00b17088 stg04, 0x00af8f08 stg20, 0x00b1fd98 stg22 */
void archiver_spring(ArchiverWork *w)
{
    Vec3 tgt;
    int tilt;

    v3_copy(&tgt, &w->pos);
    tgt.y -= archiver_ah;
    spring3_step(archiver_arate, archiver_aregist, &tgt, &w->spos, &w->svel, &w->sacc);
    tilt = (int)DEG_TO_ANG((w->pos.x - w->spos.x) * archiver_aangz);
    w->ang2[2] = MIN(MAX(tilt, -10922), 10922);
    tilt = (int)DEG_TO_ANG((w->pos.z - w->spos.z) * archiver_aangx);
    w->ang2[0] = MIN(MAX(tilt, -10922), 10922);
}

/*
 * States: 0 waits, 1 aims at the player, 2 springs beside the player, 3 holds until its motion
 * loops, 4 turns and speeds away, 5 flies off and ends.
 */
/* 0x00b0c528 stg01, 0x00b177b8 stg02, 0x00b10910 stg03, 0x00b17188 stg04, 0x00af9008 stg20, 0x00b1fe98 stg22 */
void archiver_update(ArchiverWork *w)
{
    dc_object_advance_time(&w->body);
    dc_object_advance_time(&w->ring);
    switch (w->state) {
    case 0:
        if (w->delay == 0.0f) {
            archiver_calc_target(w);
            w->state = 1;
            w->timer = archiver_stop1;
        } else if (w->delay > 0.0f) {
            w->delay -= 1.0f;
        }
        archiver_turn_to_dir(w, &w->dir);
        break;
    case 1:
        if (--w->timer <= 0)
            w->state = 2;
        archiver_turn_to_player(w);
        break;
    case 2:
        spring3_step_vel(&w->tgt, &w->pos, &w->dir, &w->acc, archiver_rate, archiver_regist);
        vec3_clamp_length(w->maxspd, &w->dir, &w->dir);
        if (vec3_distance_squared(&w->tgt, &w->pos) <= archiver_dist2) {
            w->state = 3;
            w->timer = archiver_stop2;
        }
        archiver_turn_to_player(w);
        break;
    case 3:
        if (--w->timer <= 0 && (w->body.flags2 & 1)) {
            archiver_start_motion(w);
            w->state = 4;
        }
        archiver_turn_to_dir(w, &w->vel);
        break;
    case 4:
        vec3_approach(&w->vel, &w->dir, -0.003f);
        if (w->body.frame >= archiver_awayfrm)
            w->state = 5;
        archiver_turn_to_dir(w, &w->vel);
        break;
    case 5:
        vec3_approach(&w->vel, &w->dir, 0.03f);
        w->life -= 1.0f;
        if (w->life <= 0.0f) {
            enemy_status_event(w->fx, 4, &w->pos);
            w->fx_op = 0;
            FINISH_IF(w);
            w->state = 6;
        }
        archiver_turn_to_dir(w, &w->vel);
        break;
    }
    v3_add(&w->pos, &w->dir);
    v3_scale(&w->dir, w->drag);
    archiver_spring(w);
}

/* 0x00b0c848 stg01, 0x00b17ad8 stg02, 0x00b10c30 stg03, 0x00b174a8 stg04, 0x00af9328 stg20, 0x00b201b8 stg22 */
void archiver_init(ArchiverWork *w, ArchiverArg *prm)
{
    v3_copy(&w->pos, &prm->pos);
    v3_copy(&w->dir, &prm->dir);
    v3_copy(&w->spos, &prm->pos);
    w->spos.y -= archiver_ah;
    i3_copy(w->ang, prm->ang);
    w->side = prm->side;
    w->up = prm->up;
    w->drag = prm->drag;
    w->delay = prm->delay;
    w->maxspd = prm->maxspd;
    w->life = prm->life;
    archiver_calc_velocity(w);
    do {
        void *m_ = item_models, *t_ = item_textures;
        w->models = m_;
        w->textures = (int)t_;
    } while (0);
    w->fx = enemy_status_ent_create(prm, w);
    enemy_status_event(w->fx, 2, &w->pos);
    w->fx_op = 3;
    archiver_models_init(w);
    lockon_obj_init(&w->lock, w, &w->pos, 0, archiver_lock_name, w->fx, 1, 1, 8);
    lockon_entry(&w->lock);
}

/* 0x00b0c9c0 stg01, 0x00b17c50 stg02, 0x00b10da8 stg03, 0x00b17620 stg04, 0x00af94a0 stg20, 0x00b20330 stg22 */
void archiver_release(ArchiverWork *w)
{
    lockon_remove(&w->lock);
    archiver_models_release(w);
    enemy_status_event(w->fx, w->fx_op, &w->pos);
}

/* 0x00b0ca00 stg01, 0x00b17c90 stg02, 0x00b10de8 stg03, 0x00b17660 stg04, 0x00af94e0 stg20, 0x00b20370 stg22 */
void archiver_exec(ArchiverWork *w)
{
    Vec3 ofs;
    int n;

    player_get_vec(&ofs);
    v3_add(&w->pos, &ofs);
    v3_add(&w->tgt, &ofs);
    v3_add(&w->spos, &ofs);
    n = enemy_status_get_ticks();
    if (n > 0) {
        do {
            archiver_update(w);
            n--;
        } while (n);
    }
    if ((w->lock.flags & LOCK_HIT) && !w->lock.life) {
        enemy_status_score_popup(w->fx, &w->pos, 20, lockon_hits_max_shot_cnt(&w->lock));
        archiver_shatter(w);
        archiver_change_symbol(w);
        enemy_status_event(w->fx, 3, &w->pos);
        w->fx_op = 0;
        TASK_OF(w)->disp = 0;
        FINISH_IF(w);
    }
}

/* 0x00b0cb68 stg01, 0x00b17df8 stg02, 0x00b10f50 stg03, 0x00b177c8 stg04, 0x00af9648 stg20, 0x00b204d8 stg22 */
void archiver_disp(ArchiverWork *w)
{
    Vec3 *pos = &w->pos;

    if (view_sphere_visible(pos, 10.0f)) {
        ulMatrixPush();
        ulGxSetTextureList((void *)w->textures);
        do {
            Vec3 *p_ = pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        ulMatrixPush();
        archiver_rotate_yxz(w->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        dc_object_draw(&w->body);
        ulMatrixPop();
        ulMatrixPush();
        archiver_rotate_yxz(w->ang2);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        dc_object_draw(&w->ring);
        ulMatrixPop();
        ulMatrixLoad(w->mtx);
        change_symbol_model_draw();
        ulMatrixPop();
    }
}

/* 0x00b0cc58 stg01, 0x00b17ee8 stg02, 0x00b11040 stg03, 0x00b178b8 stg04, 0x00af9738 stg20, 0x00b205c8 stg22 */
void *archiver_create(void *parent, void *prm)
{
    return task_create_arg(parent, &archiver_desc, 496, prm);
}

/* 0x00b0cc80 stg01, 0x00b17f10 stg02, 0x00b11068 stg03, 0x00b178e0 stg04, 0x00af9760 stg20, 0x00b205f0 stg22 */
int archiver_marker_draw(void **w, ArchiverArg *prm)
{
    Vec3 *pos = &prm->pos;

    if (view_sphere_visible(pos, 10.0f)) {
        ulGxSetTextureList(item_textures);
        do {
        ulMatrixPush();
    } while (0);
        vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
        archiver_rotate_yxz(prm->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        ulMatrixPop();
    }
    return 0;
}

/* 0x00b0cd18 stg01, 0x00b17fa8 stg02, 0x00b11100 stg03, 0x00b17978 stg04, 0x00af97f8 stg20, 0x00b20688 stg22 */
/* steer(cast) */
int archiver_vars_init(void)
{
    int group = enemy_vars_group_acquire();

    archiver_vars_group = ((int (*)(struct GwVarNode **, int, char *))gw_varedit_add)(&gw_varedit_root, group, archiver_vardef);
    gw_varedit_add_table(&gw_varedit_root, archiver_vars_group, archiver_vardefs);
    return 0;
}

/* 0x00b0cd78 stg01, 0x00b18008 stg02, 0x00b11160 stg03, 0x00b179d8 stg04, 0x00af9858 stg20, 0x00b206e8 stg22 */
int archiver_vars_release(void)
{
    enemy_vars_group_release();
    return 0;
}

/* 0x00b0cd98 stg01, 0x00b18028 stg02, 0x00b11180 stg03, 0x00b179f8 stg04, 0x00af9878 stg20, 0x00b20708 stg22 */
void archiver_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
