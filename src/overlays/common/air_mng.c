/*
 * Air trail: a ribbon that follows a shot or enemy (or its model's marked nodes), drawn as a
 * fading textured strip; it narrows and fades out when the owner dies or the trail stops.
 */

#include "types_common.h"

#include "types_overlays.h"

#include "debug.h"
#include "node_draw_state.h"
#include "player.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "air_mng.h"
#include "ribbon.h"
#include "system.h"

static inline void v3_copy(Vec3 *dst, Vec3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

static inline void v3_sub(Vec3 *dst, Vec3 *lhs, Vec3 *rhs)
{
    dst->x = lhs->x - rhs->x;
    dst->y = lhs->y - rhs->y;
    dst->z = lhs->z - rhs->z;
}

extern int ulMatrixPush(void);

extern struct TaskDesc air_mng_class;

/* 0x00b11b18 stg01, 0x00b1cda8 stg02, 0x00b15f00 stg03, 0x00b1c778 stg04, 0x00b286e0 stg05, 0x00b25488 stg22 */
void air_mng_init(AirMngWork *w, AirArg *air)
{
    TrailArg *tail = air->t;

    w->owner = air->owner;
    w->t = *tail;
    w->mem = yuki_alloc(tail->frames * 24 * tail->tail_seg);
    ribbon_init(&w->rib, w->mem, tail->frames, tail->tail_seg, tail->tail_skip);
    w->life = w->frames = tail->frames;
    w->fade = 1.0f;
    w->width = 1.0f;
    w->has_last = 0;
    w->stop = 0;
    task_set_dlist(w, 6);
}

/* 0x00b11c00 stg01, 0x00b1ce90 stg02, 0x00b15fe8 stg03, 0x00b1c860 stg04, 0x00b287c8 stg05, 0x00b25570 stg22 */
void air_mng_release(AirMngWork *w)
{
    yuki_free(w->mem);
    w->mem = 0;
}

/* 0x00b11c30 stg01, 0x00b1cec0 stg02, 0x00b16018 stg03, 0x00b1c890 stg04, 0x00b287f8 stg05, 0x00b255a0 stg22 */
void air_mng_exec(AirMngWork *w)
{
    Vec3 rel;
    TrailArg *tail = &w->t;
    Vec3 *pos;
    float x, y, z;

    if (tail->one) {
        player_get_pos(&rel);
        v3_sub(&rel, tail->pos, &rel);
    } else
        v3_copy(&rel, tail->pos);
    if (!w->owner || (TASK_OF(w->owner)->flags & TASK_F_END) || w->stop) {
        w->owner = 0;
        if (--w->life <= 0) {
            FINISH_IF(w);
            return;
        }
        w->fade = safe_powf((float)w->life / w->frames, 10.0f);
        w->width *= 0.9f;
        ribbon_age(&w->rib);
        return;
    }
    pos = tail->pos;
    z = pos->z;
    do { x = pos->x; } while (0);
    y = pos->y;
    if (w->has_last && x == w->last.x && y == w->last.y && z == w->last.z) {
        ribbon_age(&w->rib);
        return;
    }
    w->has_last = 1;
    w->last.x = x;
    w->last.y = y;
    w->last.z = z;
    if (!tail->body)
        ribbon_add_points(&w->rib, &rel, tail->ang, tail->tail_width);
    else
        ribbon_add_model(&w->rib, &rel, tail->ang, tail->tail_mask, tail->body->root, tail->body->clip, tail->body->frame);
}

/* 0x00b11e28 stg01, 0x00b1d0b8 stg02, 0x00b16210 stg03, 0x00b1ca88 stg04, 0x00b289f0 stg05, 0x00b25798 stg22 */
void air_mng_disp(AirMngWork *w)
{
    Vec3 pl;
    TrailArg *tail = &w->t;
    int old_fc = draw_set_fc_mode(0x2000);
    int old_tst = draw_set_test_mode(0x18000);

    draw_set_blend(4, 1);
    ulMatrixPush();
    ulGxSetTextureList(w->tex);
    if (tail->one) {
        player_get_pos(&pl);
        do {
            Vec3 *p_ = &pl;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
    }
    ribbon_draw(&w->rib, tail->tail_tex, w->fade, w->width);
    ulMatrixPop();
    draw_set_fc_mode(old_fc);
    draw_set_test_mode(old_tst);
}

/* 0x00b11ef8 stg01, 0x00b1d188 stg02, 0x00b162e0 stg03, 0x00b1cb58 stg04, 0x00b28ac0 stg05, 0x00b25868 stg22 */
void *air_mng_create(EnbNormalWork *owner, TrailArg *tail)
{
    AirArg air;

    air.t = tail;
    air.owner = owner;
    return task_create_sibling_inherit_arg(owner, &air_mng_class, 120, &air);
}

/* 0x00b11f30 stg01, 0x00b1d1c0 stg02, 0x00b16318 stg03, 0x00b1cb90 stg04, 0x00b28af8 stg05, 0x00b258a0 stg22 */
void air_mng_stop(AirMngWork *w)
{
    w->stop = 1;
}
