/*
 * Common enemy bullet behind E_Shot and the other shots: a lockable model or sprite fired
 * straight, aimed or sideways, with an optional air trail; hurts on contact, scores when shot,
 * ends with its life or out of view range. Look and behaviour come from each kind's ShotDesc.
 */

#include "types_common.h"

#include "types_overlays.h"

#include "debug.h"
#include "camera.h"
#include "enemy_status.h"
#include "fog.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "player_status.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "enb_normal.h"
#include "air_mng.h"
#include "system.h"
#include "mathdef.h"

static inline void v3_copy(Vec3 *dst, Vec3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

static inline void v3_add(Vec3 *dst, Vec3 *src)
{
    dst->x += src->x;
    dst->y += src->y;
    dst->z += src->z;
}

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

extern struct TaskDesc enb_normal_class;

static inline void v3_sum(Vec3 *dst, Vec3 *lhs, Vec3 *rhs)
{
    dst->x = lhs->x + rhs->x;
    dst->y = lhs->y + rhs->y;
    dst->z = lhs->z + rhs->z;
}

/* 0x00b0eaa0 stg01, 0x00b19d30 stg02, 0x00b12e88 stg03, 0x00b19700 stg04, 0x00b2cd18 stg05, 0x00b22410 stg22 */
void enb_normal_aim_velocity(Vec3 *pos, Vec3 *dir, ShotDesc *desc, Vec3 *vel, Vec3 *aim)
{
    Vec3 v, bend;
    float sc[2];
    float len, spd_st, spd_fin, rate, inv, spd;

    v.x = -aim->x;
    v.y = -aim->y;
    v.z = -aim->z;
    len = vec3_length(&v);
    spd_st = desc->spd_start * (1.0f / FPS);
    spd_fin = spd_st * desc->spd_chg_rate;
    if (len > desc->spd_chg_dis_start) {
        spd = spd_st;
    } else if (len < desc->spd_chg_dis_finish) {
        spd = spd_fin;
    } else {
        rate = (desc->spd_chg_dis_start - len) / (desc->spd_chg_dis_start - desc->spd_chg_dis_finish);
        spd = (spd_fin - spd_st) * rate + spd_st;
    }
    rate = spd / len;
    v3_scale(&v, rate);
    v3_sub(&bend, dir, &v);
    ulSinCos((float)(desc->curve_time << 15) * (1.0f - rate), &sc[0], &sc[1]);
    inv = 1.0f / sc[0];
    vel->x = bend.x * inv;
    vel->y = bend.y * inv;
    vel->z = bend.z * inv;
}

/* 0x00b0ec30 stg01, 0x00b19ec0 stg02, 0x00b13018 stg03, 0x00b19890 stg04, 0x00b2cea8 stg05, 0x00b225a0 stg22 */
void enb_normal_side_velocity(float speed, Vec3 *pos, Vec3 *vel)
{
    Vec3 pl;
    int ang[4];
    float sc[4][2];

    player_get_pos(&pl);
    player_get_rot(ang);
    v3_sub(vel, pos, &pl);
    ROT_Y(ang[1], sc[0], vel->x, vel->z);
    ROT_X(ang[0], sc[1], vel->y, vel->z);
    vel->z = 0;
    vec3_normalize(vel, vel);
    v3_scale(vel, speed);
    ROT_X(ang[0], sc[2], vel->y, vel->z);
    ROT_Y(ang[1], sc[3], vel->x, vel->z);
}

/* 0x00b0edd8 stg01, 0x00b1a068 stg02, 0x00b131c0 stg03, 0x00b19a38 stg04, 0x00b2d050 stg05, 0x00b22748 stg22 */
void enb_normal_send_event(EnbNormalWork *w, int op)
{
    ShotHit hit;

    if (w->cb) {
        hit.level = w->level;
        hit.spin = &w->spin;
        hit.owner = w;
        hit.flip = &w->flip;
        hit.op = op;
        hit.pos = &w->pos;
        hit.model = &w->body;
        hit.ang = w->ang;
        hit.spr_ang = w->spr_ang;
        hit.lock = &w->lock;
        hit.t = w->t;
        hit.lpos = &w->lpos;
        w->cb(&hit);
    }
}

/* 0x00b0ee60 stg01, 0x00b1a0f0 stg02, 0x00b13248 stg03, 0x00b19ac0 stg04, 0x00b2d0d8 stg05, 0x00b227d0 stg22 */
void enb_normal_close_fx(EnbNormalWork *w, int op, int ev)
{
    if (w->fx_op) {
        enemy_status_event(w->fx, op, &w->pos);
        w->fx_op = 0;
    }
    enb_normal_send_event(w, ev);
}

/* 0x00b0eeb0 stg01, 0x00b1a140 stg02, 0x00b13298 stg03, 0x00b19b10 stg04, 0x00b2d128 stg05, 0x00b22820 stg22 */
int enb_normal_area_check(EnbNormalWork *w, int quiet)
{
    float r;

    if (w->clip_ang) {
        if (w->type == 0)
            r = w->body.radius;
        else {
            r = 0;
            if (w->type < 3)
                r = w->spr_w;
        }
        if (!yuki_camera_relative(&w->pos, w->clip_ang, r)) {
            if (!quiet)
                enb_normal_close_fx(w, 3, 9);
            FINISH_IF(w);
            TASK_OF(w)->exec = 0;
            TASK_OF(w)->disp = 0;
            return 0;
        }
    }
    return 1;
}

/* 0x00b0ef58 stg01, 0x00b1a1e8 stg02, 0x00b13340 stg03, 0x00b19bb8 stg04, 0x00b2d1d0 stg05, 0x00b228c8 stg22 */
void enb_normal_init(EnbNormalWork *w, ObjShotArg *shot)
{
    Vec3 lead, pvec;
    TrailArg tail;
    ShotDesc *desc = (ShotDesc *)shot->blk;
    float tfrm;
    int n;

    w->level = desc->level;
    w->cb = desc->cb;
    do { w->spin = desc->spin; } while (0);
    w->flip = desc->flip;
    w->unk_00 = 0;
    do { w->tex = desc->tex; } while (0);
    w->type = desc->type;
    w->fog = desc->fog;
    w->radius = desc->radius;
    w->has_model = 0;
    if (desc->root) {
        if (desc->clip)
            dc_object_init(&w->body, (struct DcNode *)desc->root, desc->clip, desc->mflags);
        else
            dc_object_init(&w->body, (struct DcNode *)desc->root, 0, desc->mflags);
        dc_object_rebuild(&w->body);
        w->tex2 = desc->tex;
        if (desc->radius < 0)
            w->radius = w->body.radius;
        w->has_model = 1;
    }
    w->spr_tex = desc->spr_tex;
    w->spr_ang = 0;
    w->spr_w = desc->spr_w;
    w->spr_col = desc->spr_col;
    w->spr_h = desc->spr_h;
    w->st_fc = desc->st_fc;
    w->st_a = desc->st_a;
    w->st_b = desc->st_b;
    w->spr2 = desc->spr2;
    if (desc->radius < 0)
        w->radius = w->spr_w;
    if (!enb_normal_area_check(w, 1)) {
        w->dead = 1;
        return;
    }
    w->dead = 0;
    w->frames = desc->motion_sec * FPS;
    w->t = 0;
    w->damage = desc->damage;
    w->score = desc->score;
    w->fx = enemy_status_ent_alloc(desc->kind, -1, w);
    enemy_status_event(w->fx, 2, &w->pos);
    w->fx_op = 3;
    w->curve_time = desc->curve_time;
    w->spd = desc->spd_start * (1.0f / FPS);
    w->curve_type = desc->curve_type;
    w->spd_chg_rate = desc->spd_chg_rate;
    w->spd_chg_dis_start = desc->spd_chg_dis_start;
    w->spd_chg_dis_finish = desc->spd_chg_dis_finish;
    player_get_vec(&pvec);
    v3_sub(&w->pos, shot->pos, &pvec);
    player_get_pos(&w->target);
    v3_sub(&w->target, &w->target, &pvec);
    v3_sub(&lead, &w->target, &w->pos);
    v3_scale(&lead, desc->pursuit);
    v3_add(&w->target, &lead);
    v3_sub(&w->d, &w->pos, &w->target);
    v3_copy(&w->dir, &w->d);
    w->dis = 0;
    w->len = vec3_length(&w->dir);
    if (shot->dir == 0)
        enb_normal_side_velocity(w->len * desc->curve_rate, shot->pos, &w->vel);
    else if (shot->n)
        enb_normal_aim_velocity(shot->pos, shot->dir, desc, &w->vel, &w->dir);
    else
        v3_copy(&w->vel, shot->dir);
    enb_normal_initial_angles(w->ang, shot->pos, shot->dir, shot->n, (int *)desc);
    i3_copy(w->ang2, w->ang);
    w->roll = 0;
    lockon_obj_init(&w->lock, w, &w->lpos, 0, shot->kind, w->fx, desc->hp, desc->same_lock, 0);
    lockon_entry(&w->lock);
    w->trail = desc->use_tail;
    if (w->trail) {
        tail.tail_seg = desc->tail_seg;
        tail.body = desc->root ? &w->body : 0;
        tfrm = desc->tail_sec * FPS;
        tail.ang = w->ang2;
        tail.tail_skip = desc->tail_skip;
        n = tfrm;
        tail.pos = &w->pos;
        if (tfrm <= 2.0f)
            n = 2;
        tail.frames = n;
        tail.tail_width = desc->tail_width;
        tail.tail_tex = desc->tail_tex;
        tail.tail_mask = desc->tail_mask;
        tail.one = 1;
        air_mng_create(w, &tail);
    }
    w->unk_1b4 = desc->unk_b8;
    w->mask = desc->mask;
    if (w->mask) {
        yuki_model_positions_at(&w->lpos, &w->pos, w->ang, &w->body, w->mask, 1);
    } else {
        v3_copy(&w->lpos, &w->pos);
    }
    w->started = 0;
    w->done = 0;
    w->life = desc->frames;
    w->face_target = desc->face_target;
    w->clip_ang = desc->clip_ang;
    enb_normal_send_event(w, 7);
}

/* 0x00b0f4b8 stg01, 0x00b1a748 stg02, 0x00b138a0 stg03, 0x00b1a118 stg04, 0x00b2d730 stg05, 0x00b22e28 stg22 */
void enb_normal_release(EnbNormalWork *w)
{
    if (!w->dead) {
        enb_normal_close_fx(w, w->fx_op, 8);
        lockon_remove(&w->lock);
    }
    if (w->has_model)
        dc_object_release(&w->body);
}

/* 0x00b0f510 stg01, 0x00b1a7a0 stg02, 0x00b138f8 stg03, 0x00b1a170 stg04, 0x00b2d788 stg05, 0x00b22e80 stg22 */
int enb_normal_move(EnbNormalWork *w)
{
    Vec3 line, bend, prev, v;
    float dis_st, rest, spd_fin, rate, s;
    int ret = 0;

    rest = w->len - w->dis;
    dis_st = w->spd_chg_dis_start;
    spd_fin = w->spd * w->spd_chg_rate;
    if (dis_st < rest) {
        w->cur_spd = w->spd;
        ret = w->face_target;
    } else if (rest < w->spd_chg_dis_finish)
        w->cur_spd = spd_fin;
    else {
        rate = (dis_st - rest) / (dis_st - w->spd_chg_dis_finish);
        w->cur_spd = (spd_fin - w->spd) * rate + w->spd;
    }
    w->dis += w->cur_spd;
    rate = 1.0f - w->dis / w->len;
    v3_mul(&line, &w->dir, rate);
    s = ulSinPoly((w->curve_time << 15) * rate);
    switch (w->curve_type) {
    case 1:
        s *= 1.0f - rate;
        break;
    case 2:
        s *= rate;
        break;
    }
    v3_mul(&bend, &w->vel, s);
    v3_copy(&prev, &w->d);
    v3_sum(&w->d, &line, &bend);
    v3_sum(&w->pos, &w->target, &w->d);
    if (!w->face_target)
        yuki_draw_aimed(&w->d, &prev, w->ang);
    else {
        do {
            Vec3 *d_ = &w->d;

            v.x = -d_->x;
            v.y = -d_->y;
            v.z = -d_->z;
        } while (0);
        do {
            w->ang[1] = (int)RAD_TO_ANG(atan2f(-v.x, -v.z));
            w->ang[0] = (int)RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
        } while (0);
        w->ang[2] = 0;
    }
    w->roll += w->roll_spd / FPS;
    ulMatrixPush();
    matrix_unit_set(0);
    enb_normal_rotate_yxz(w->ang);
    vu0_matrix_rotate_z(ulMatrixCur, w->roll);
    model_camera_angles(w->ang2);
    ulMatrixPop();
    if (!w->done && fabsf(w->unk_12c) < FLT_EPSILON && player_sphere_test(&w->pos, w->radius) == 1) {
        player_get_pos(&w->pos);
        player_status_add_damage(w->damage);
        enb_normal_close_fx(w, 5, 2);
        w->done = 1;
        w->started = 0;
    }
    return ret;
}

/* 0x00b0f8c0 stg01, 0x00b1ab50 stg02, 0x00b13ca8 stg03, 0x00b1a520 stg04, 0x00b2db38 stg05, 0x00b23230 stg22 */
void enb_normal_animate(EnbNormalWork *w)
{
    int n = w->frames;
    float mfrm, mlen;

    if (n <= 0) {
        w->t = 0;
        return;
    }
    if (w->type) {
        w->spr_ang += 0x10000 / n;
        w->t = (w->spr_ang & 0xffff) * (1.0f / ANG_TURN);
        return;
    }
    mlen = w->body.frames;
    mfrm = w->body.frame + mlen / n;
    while (mlen < mfrm)
        mfrm -= mlen;
    while (mfrm < 0)
        mfrm += mlen;
    w->body.frame = mfrm;
    w->t = mfrm / mlen;
}

/* 0x00b0f9b0 stg01, 0x00b1ac40 stg02, 0x00b13d98 stg03, 0x00b1a610 stg04, 0x00b2dc28 stg05, 0x00b23320 stg22 */
int enb_normal_hit_check(EnbNormalWork *w)
{
    int num;

    if (w->lock.flags & LOCK_HIT) {
        if (w->lock.life > 0) {
            enb_normal_send_event(w, 0);
            return 1;
        }
        enb_normal_close_fx(w, 3, 1);
        num = lockon_hits_max_shot_cnt(&w->lock);
        enemy_status_score_popup(w->fx, &w->pos, w->score, num);
        FINISH_IF(w);
        return 0;
    }
    return 1;
}

/* 0x00b0fa40 stg01, 0x00b1acd0 stg02, 0x00b13e28 stg03, 0x00b1a6a0 stg04, 0x00b2dcb8 stg05, 0x00b233b0 stg22 */
int enb_normal_range_check(EnbNormalWork *w)
{
    Vec3 pl;
    Vec3 *pos = &w->pos;
    float r = camera_get_far();

    player_get_pos(&pl);
    if (vec3_distance_squared(pos, &pl) > r * r) {
        enb_normal_close_fx(w, 3, 5);
        FINISH_IF(w);
        return 0;
    }
    if (!enb_normal_area_check(w, 0))
        return 0;
    if (w->mask)
        yuki_model_positions_at(&w->lpos, pos, w->ang, &w->body, w->mask, 1);
    else
        v3_copy(&w->lpos, pos);
    return 1;
}

/* 0x00b0fb28 stg01, 0x00b1adb8 stg02, 0x00b13f10 stg03, 0x00b1a788 stg04, 0x00b2dda0 stg05, 0x00b23498 stg22 */
void enb_normal_exec(EnbNormalWork *w)
{
    Vec3 pv_idle, pv_first, pv_step;
    Vec3 *pos = &w->pos;
    int n = enemy_status_get_ticks();

    enemy_status_ent_set_pos(w->fx, pos, w->life);
    if (!n) {
        player_get_vec(&pv_idle);
        v3_add(&w->pos, &pv_idle);
        v3_add(&w->target, &pv_idle);
    } else if (!w->started) {
        player_get_vec(&pv_first);
        w->started = 1;
        v3_add(&w->pos, &pv_first);
        v3_add(&w->target, &pv_first);
    } else {
        player_get_vec(&pv_step);
        v3_add(&w->target, &pv_step);
        while (n--) {
            if (!enb_normal_move(w))
                enb_normal_animate(w);
            if (!w || (TASK_OF(w)->flags & TASK_F_END))
                return;
            if (w->life > 0 && --w->life <= 0) {
                enb_normal_close_fx(w, 3, 6);
                do { FINISH(w); } while (0);
                return;
            }
        }
    }
    if (enb_normal_hit_check(w) && enb_normal_range_check(w))
        enb_normal_send_event(w, 3);
}

/* 0x00b0fd48 stg01, 0x00b1afd8 stg02, 0x00b14130 stg03, 0x00b1a9a8 stg04, 0x00b2dfc0 stg05, 0x00b236b8 stg22 */
void enb_normal_draw_model(EnbNormalWork *w)
{
    Vec3 *p = &w->pos;

    if (view_sphere_visible(p, w->body.radius)) {
        ulGxSetTextureList(w->tex2);
        do {
            ulMatrixPush();
        } while (0);
        vu0_matrix_translate(ulMatrixCur, p->x, p->y, p->z);
        enb_normal_rotate_yxz(w->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        dc_object_draw(&w->body);
        ulMatrixPop();
    }
}

/* 0x00b0fde8 stg01, 0x00b1b078 stg02, 0x00b141d0 stg03, 0x00b1aa48 stg04, 0x00b2e060 stg05, 0x00b23758 stg22 */
void enb_normal_draw_sprite(EnbNormalWork *w)
{
    int old_fc = draw_set_fc_mode(w->st_fc);

    draw_set_blend(w->st_a, w->st_b);
    ulGxSetTextureList(w->tex);
    if (!w->spr2)
        yuki_sprite_draw3d_min(w->spr_ang, w->spr_col, w->spr_tex, &w->pos, w->spr_w, w->spr_w,
                               w->spr_h, w->spr_h);
    else
        yuki_sprite2_draw3d_min(w->spr_ang, w->spr_col, w->spr_tex, &w->pos, w->spr_w, w->spr_w,
                                w->spr_h, w->spr_h);
    draw_set_fc_mode(old_fc);
}

/* 0x00b0fe88 stg01, 0x00b1b118 stg02, 0x00b14270 stg03, 0x00b1aae8 stg04, 0x00b2e100 stg05, 0x00b237f8 stg22 */
void enb_normal_disp(EnbNormalWork *w)
{
    void *fog = fog_set_enable(w->fog);

    switch (w->type) {
    case 0:
        enb_normal_draw_model(w);
        break;
    case 1:
        enb_normal_draw_sprite(w);
        break;
    }
    fog_set_enable(fog);
    enb_normal_send_event(w, 4);
    if (w->done)
        FINISH_IF(w);
}

/* 0x00b0ff28 stg01, 0x00b1b1b8 stg02, 0x00b14310 stg03, 0x00b1ab88 stg04, 0x00b2e1a0 stg05, 0x00b23898 stg22 */
void *enb_normal_create(void *parent, Vec3 *pos, Vec3 *dir, int aimed, void *kind, int *blk)
{
    ObjShotArg shot;

    shot.kind = kind;
    shot.dir = dir;
    shot.pos = pos;
    shot.blk = blk;
    shot.n = aimed;
    return task_create_sibling_inherit_arg(parent, &enb_normal_class, 440, &shot);
}

/* 0x00b0ff70 stg01, 0x00b1b200 stg02, 0x00b14358 stg03, 0x00b1abd0 stg04, 0x00b2e1e8 stg05, 0x00b238e0 stg22 */
void *enb_normal_fire(void *parent, Vec3 *pos, Vec3 *dir, int aimed, void *kind, int *blk)
{
    blk[46] = 0;
    return enb_normal_create(parent, pos, dir, aimed, kind, blk);
}

/* 0x00b0ff90 stg01, 0x00b1b220 stg02, 0x00b14378 stg03, 0x00b1abf0 stg04, 0x00b2e208 stg05, 0x00b23900 stg22 */
void *enb_normal_fire_same_lock(void *parent, Vec3 *pos, Vec3 *dir, int aimed, void *kind, int *blk)
{
    blk[35] = 1;
    blk[44] = 0;
    return enb_normal_fire(parent, pos, dir, aimed, kind, blk);
}

/* 0x00b0ffb8 stg01, 0x00b1b248 stg02, 0x00b143a0 stg03, 0x00b1ac18 stg04, 0x00b2e230 stg05, 0x00b23928 stg22 */
void *enb_normal_fire_basic(void *parent, Vec3 *pos, Vec3 *dir, int aimed, void *kind, int *blk)
{
    blk[33] = 0;
    return enb_normal_fire_same_lock(parent, pos, dir, aimed, kind, blk);
}

/* 0x00b0ffd8 stg01, 0x00b1b268 stg02, 0x00b143c0 stg03, 0x00b1ac38 stg04, 0x00b2e250 stg05, 0x00b23948 stg22 */
void enb_normal_initial_angles(int *ang, Vec3 *pos, Vec3 *dir, int aimed, int *blk)
{
    Vec3 tgt, ofs, next, vel, line, lead;
    float sc[2];
    ShotDesc *desc = (ShotDesc *)blk;
    float len, spd_st, spd_fin, rate, spd;

    player_get_pos(&tgt);
    v3_sub(&lead, &tgt, pos);
    v3_scale(&lead, desc->pursuit);
    v3_add(&tgt, &lead);
    v3_sub(&ofs, pos, &tgt);
    v3_copy(&line, &ofs);
    len = vec3_length(&ofs);
    if (dir == 0)
        enb_normal_side_velocity(len * desc->curve_rate, pos, &vel);
    else if (aimed)
        enb_normal_aim_velocity(pos, dir, desc, &vel, &line);
    else
        v3_copy(&vel, dir);
    spd_st = desc->spd_start * (1.0f / FPS);
    spd_fin = spd_st * desc->spd_chg_rate;
    if (len > desc->spd_chg_dis_start) {
        spd = spd_st;
    } else if (len < desc->spd_chg_dis_finish) {
        spd = spd_fin;
    } else {
        rate = (desc->spd_chg_dis_start - len) / (desc->spd_chg_dis_start - desc->spd_chg_dis_finish);
        spd = (spd_fin - spd_st) * rate + spd_st;
    }
    rate = 1.0f - spd / len;
    v3_scale(&line, rate);
    ulSinCos((desc->curve_time << 15) * rate, &sc[0], &sc[1]);
    switch (desc->curve_type) {
    case 2:
        sc[0] *= rate;
        break;
    case 1:
        sc[0] *= 1.0f - rate;
        break;
    }
    v3_scale(&vel, sc[0]);
    v3_sum(&ofs, &line, &vel);
    v3_sum(&next, &tgt, &ofs);
    v3_sub(&lead, &next, pos);
    do {
        ang[1] = (int)RAD_TO_ANG(atan2f(-lead.x, -lead.z));
        ang[0] = (int)RAD_TO_ANG(atan2f(lead.y, sqrtf(lead.x * lead.x + lead.z * lead.z)));
    } while (0);
    ang[2] = 0;
}

/* 0x00b10348 stg01, 0x00b1b5d8 stg02, 0x00b14730 stg03, 0x00b1afa8 stg04, 0x00b2e5c0 stg05, 0x00b23cb8 stg22 */
void enb_normal_kill(EnbNormalWork *w)
{
    enb_normal_close_fx(w, 3, 1);
    FINISH_IF(w);
}

/* 0x00b10388 stg01, 0x00b1b618 stg02, 0x00b14770 stg03, 0x00b1afe8 stg04, 0x00b2e600 stg05, 0x00b23cf8 stg22 */
void enb_normal_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
