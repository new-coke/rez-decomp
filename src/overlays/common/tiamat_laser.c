/*
 * Tiamat's laser (stg03): after the charge flash at the mouth, a lockable beam shoots toward the
 * player, moving with them, then narrows and fades. Shot down, it scores and bends off at random
 * with a flash at the bend.
 */

#include "types_common.h"

#include "types_overlays.h"

#include "mathdef.h"
#include "debug.h"
#include "approach.h"
#include "color.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "heap.h"
#include "lens.h"
#include "libc.h"
#include "lockon.h"
#include "model.h"
#include "model_wire_break.h"
#include "node_draw_state.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "render.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "tiamat_laser.h"
#include "../stg03/e_shot.h"
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

extern struct TaskDesc tiamat_laser_attack_ray_desc;
extern int tiamat_laser_init_arg;
extern TiamatLaserVars tiamat_laser_types[1];
extern VarDef tiamat_laser_vardef;
extern VarDef tiamat_laser_vardefs[16];

extern void (*tiamat_laser_disp_table[])(void *w);
extern struct TaskDesc tiamat_laser_desc;

extern struct TaskDesc tiamat_laser_turn_eff_desc;
extern char tiamat_laser_lock_name[];

/* model list entry n as stg22 numbers it */
#if defined(OVERLAY_STG22)
#define TIAMAT_LASER_MODEL(n) ((n) - 5)
#else
#define TIAMAT_LASER_MODEL(n) (n)
#endif

/* 0x00b0ce10 stg03, 0x00b1a7d8 stg22 */
void tiamat_laser_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *prm;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&tiamat_laser_vardef);
    for (i = 0; i < 1; i++) {
        def = tiamat_laser_vardefs;
        prm = (int *)&tiamat_laser_types[i].p;
        tiamat_laser_vardef.name = tiamat_laser_types[i].name;
        tiamat_laser_vardef.fmt = tiamat_laser_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&tiamat_laser_vardef);
        for (j = 0; j < 16; j++) {
            def->ptr = prm;
            prm++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)tiamat_laser_vardefs);
    }
}

/* 0x00b0cf28 stg03, 0x00b1a8f0 stg22 */
void tiamat_laser_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b0cf30 stg03, 0x00b1a8f8 stg22 */
int tiamat_laser_vars_init(int *fx_id)
{
    tiamat_laser_init_arg = *fx_id;
    tiamat_laser_vars_register();
    return 0;
}

/* 0x00b0cf58 stg03, 0x00b1a920 stg22 */
int tiamat_laser_vars_release(void)
{
    tiamat_laser_vars_unregister();
    return 0;
}

/* 0x00b0cf78 stg03, 0x00b1a940 stg22 */
int tiamat_laser_get_init_arg(void)
{
    return tiamat_laser_init_arg;
}

/* 0x00b0cf88 stg03, 0x00b1a950 stg22 */
void tiamat_laser_init(TiamatLaserWork *w, int *prm)
{
    TiamatLaserVars *typ = &tiamat_laser_types[prm[0]];
    Vec3 p;
    float sec;

    w->state = 0;
    player_get_pos(&p);
    v3_sub(&w->tail_ofs, (Vec3 *)prm[1], &p);
    v3_copy(&w->ofs, &w->tail_ofs);
    v3_sum(&w->pos, &w->ofs, &p);
    v3_set(&w->bend, 0.0f, 0.0f, 0.0f);
    v3_set(&w->bend_ofs, 0.0f, 0.0f, 0.0f);
    if (v3_is_zero(&w->ofs)) {
        v3_set(&w->dir0, 0.0f, 0.0f, 0.0f);
    } else {
        Vec3 *v = &w->dir0;

        vec3_normalize(v, &w->ofs);
        v->x = -v->x;
        v->y = -v->y;
        v->z = -v->z;
        v3_copy(&w->dir, v);
    }
    w->fx = enemy_status_ent_alloc(tiamat_laser_init_arg, -1, (int)w);
    enemy_status_event(w->fx, 2, (float *)&w->pos);
    *(void **)&w->fx_op = (void *)3; /* steer(cast) */
    lockon_obj_init(&w->lock, w, &w->pos, 0, tiamat_laser_lock_name, w->fx, typ->p.hp, typ->p.same_lock, 0);
    lockon_entry(&w->lock);
    sec = vec3_length(&w->ofs) / (typ->p.speed * (1.0f / FPS));
    w->width = typ->p.width;
    w->width_add = typ->p.width_add;
    w->alpha = 1.0f;
    w->type = typ;
    w->state = 0;
    w->frames = 0;
    w->life = sec * FPS;
}

/* 0x00b0d220 stg03, 0x00b1abe8 stg22 */
void tiamat_laser_release(TiamatLaserWork *w)
{
    enemy_status_event(w->fx, w->fx_op, &w->pos);
    lockon_remove(&w->lock);
}

/* 0x00b0d258 stg03, 0x00b1ac20 stg22 */
void tiamat_laser_set_state(TiamatLaserWork *w, int state)
{
    w->state = state;
    w->frames = 0;
}

/* 0x00b0d268 stg03, 0x00b1ac30 stg22 */
void tiamat_laser_state_move(TiamatLaserWork *w)
{
    TiamatLaserVars *typ = w->type;
    Vec3 p;
    Vec3 dv;
    float dis;

    v3_mul(&dv, &w->dir, typ->p.speed * (1.0f / FPS));
    v3_add(&w->ofs, &dv);
    player_get_pos(&p);
    v3_sum(&w->pos, &w->ofs, &p);
    dis = ulVec3Distance(&w->ofs, &w->tail_ofs);
    dis = MIN(dis, typ->p.len);
    v3_mul(&dv, &w->dir, -dis);
    v3_sum(&w->tail_ofs, &w->ofs, &dv);
    v3_sum(&w->tail, &w->pos, &dv);
    w->width_add *= typ->p.width_add_density;
    if (player_sphere_test(&w->pos, typ->p.collision_r) == 1) {
        player_status_add_damage(typ->p.damage);
        tiamat_laser_set_state(w, 2);
    }
}

/* 0x00b0d438 stg03, 0x00b1ae00 stg22 */
void tiamat_laser_state_away(TiamatLaserWork *w)
{
    TiamatLaserVars *typ = w->type;
    Vec3 p;
    Vec3 dv;

    v3_mul(&dv, &w->dir, typ->p.speed_away * (1.0f / FPS));
    v3_add(&w->ofs, &dv);
    player_get_pos(&p);
    v3_sum(&w->pos, &w->ofs, &p);
    v3_sum(&w->bend, &w->bend_ofs, &p);
    v3_sum(&w->tail, &w->tail_ofs, &p);
    w->width *= typ->p.width_density;
    w->alpha *= typ->p.alpha_density;
    w->width_add *= typ->p.width_add_density;
    if (++w->frames > typ->p.turn_sec * FPS)
        tiamat_laser_set_state(w, 3);
}

/* 0x00b0d5f0 stg03, 0x00b1afb8 stg22 */
void tiamat_laser_state_hit(TiamatLaserWork *w)
{
    TiamatLaserVars *typ = w->type;
    Vec3 p;
    Vec3 dv;
    float dis;

    v3_mul(&dv, &w->dir, typ->p.speed * (1.0f / FPS));
    v3_add(&w->ofs, &dv);
    player_get_pos(&p);
    v3_sum(&w->pos, &w->ofs, &p);
    dis = ulVec3Distance(&w->ofs, &w->tail_ofs);
    dis = MIN(dis, typ->p.len);
    w->frames++;
    v3_mul(&dv, &w->dir, -dis);
    v3_sum(&w->tail_ofs, &w->ofs, &dv);
    v3_sum(&w->tail, &w->pos, &dv);
    w->width *= typ->p.width_density;
    w->alpha *= typ->p.alpha_density;
    w->width_add *= typ->p.width_add_density;
    if (w->frames > typ->p.away_sec * FPS)
        FINISH_IF(w);
}

/* 0x00b0d7f8 stg03, 0x00b1b1c0 stg22 */
void tiamat_laser_state_turn(TiamatLaserWork *w)
{
    TiamatLaserVars *typ = w->type;
    Vec3 p;
    Vec3 dv;

    v3_mul(&dv, &w->dir, typ->p.speed_away * (1.0f / FPS));
    v3_add(&w->ofs, &dv);
    player_get_pos(&p);
    v3_sum(&w->pos, &w->ofs, &p);
    v3_sum(&w->bend, &w->bend_ofs, &p);
    v3_sum(&w->tail, &w->tail_ofs, &p);
    w->width *= typ->p.width_density;
    w->alpha *= typ->p.alpha_density;
    w->width_add *= typ->p.width_add_density;
    if (++w->frames > typ->p.away_sec * FPS)
        FINISH_IF(w);
}

/* 0x00b0d9b8 stg03, 0x00b1b380 stg22 */
void tiamat_laser_shot_down(TiamatLaserWork *w)
{
    int ang[3];
    Vec3 p;
    float sc[3][2];
    float z;
    int mult;

    if (!(w->lock.flags & LOCK_HIT))
        return;
    if (w->lock.life > 0)
        return;
    enemy_status_event(w->fx, 3, (float *)&w->pos);
    w->fx_op = 0;
    mult = lockon_hits_max_shot_cnt(&w->lock);
    enemy_status_score_popup(w->fx, &w->pos, w->type->p.score, mult);
    z = sc[0][1] = sc[0][0] = 0.0f;
    if (!v3_is_zero(&w->dir)) {
        do {
            ang[1] = (int)RAD_TO_ANG(atan2f(-w->dir.x, -w->dir.z));
            ang[0] = (int)RAD_TO_ANG(atan2f(w->dir.y, sqrtf(w->dir.x * w->dir.x + w->dir.z * w->dir.z)));
        } while (0);
        ulSinCos(DEG_TO_ANG(RAND_FLOAT_A() * 360.0f), &sc[0][0], &sc[0][1]);
        ROT_X(ang[0], sc[1], sc[0][1], z);
        ROT_Y(ang[1], sc[2], sc[0][0], z);
        v3_set(&w->dir, sc[0][0], sc[0][1], z);
    }
    player_get_pos(&p);
    v3_copy(&w->bend_ofs, &w->ofs);
    v3_sum(&w->bend, &w->bend_ofs, &p);
    tiamat_laser_turn_eff_create(w, &w->bend);
    tiamat_laser_set_state(w, 1);
    w->lock.flags &= ~LOCK_LOCKABLE;
}

/* 0x00b0dcb0 stg03, 0x00b1b678 stg22 */
void tiamat_laser_exec(TiamatLaserWork *w)
{
    int n = enemy_status_get_ticks();
    Vec3 p;

    enemy_status_ent_set_pos(w->fx, (float *)&w->pos, w->life);
    if (!n) {
        player_get_pos(&p);
        v3_sum(&w->pos, &p, &w->ofs);
        v3_sum(&w->bend, &w->bend_ofs, &p);
        v3_sum(&w->tail, &w->tail_ofs, &p);
    } else {
        while (n--) {
            switch (w->state) {
            case 0:
                tiamat_laser_state_move(w);
                break;
            case 1:
                tiamat_laser_state_away(w);
                break;
            case 2:
                tiamat_laser_state_hit(w);
                break;
            case 3:
                tiamat_laser_state_turn(w);
                break;
            }
            if (!w || (TASK_OF(w)->flags & TASK_F_END))
                return;
            if (w->life > 0 && --w->life <= 0)
                w->state = 2;
        }
    }
    if (w->state == 0)
        tiamat_laser_shot_down(w);
}

/* 0x00b0dea8 stg03, 0x00b1b870 stg22 */
/* steer(cast) */
void tiamat_laser_draw_beam(TiamatLaserWork *w, Vec3 *pts, int n)
{
    LineStyle style;
    int dl;

    dl = RenderSetCurrentDisplayList(6);
    draw_set_blend(4, 1);
    heap_bzero(&style, sizeof style);
    {
        int a_ = color_get_a(RGBA(0x80, 0x80, 0, 0x80));
        unsigned int r_ = ((unsigned int (*)(int))color_get_r)(RGBA(0x80, 0x80, 0, 0x80));
        unsigned int g_ = color_get_g(RGBA(0x80, 0x80, 0, 0x80));
        unsigned int b_ = color_get_b(RGBA(0x80, 0x80, 0, 0x80));

        style.col[0] = ((int)(a_ * w->alpha) + 1) >> 1 << 24 | (b_ & 0xff) << 16 | (g_ & 0xff) << 8 | (r_ & 0xff);
    }
    {
        int a_ = color_get_a(RGBA(0x80, 0, 0x0f, 0x80));
        unsigned int r_ = ((unsigned int (*)(int))color_get_r)(RGBA(0x80, 0, 0x0f, 0x80));
        unsigned int g_ = color_get_g(RGBA(0x80, 0, 0x0f, 0x80));
        unsigned int b_ = color_get_b(RGBA(0x80, 0, 0x0f, 0x80));

        style.col[1] = ((int)(a_ * w->alpha) + 1) >> 1 << 24 | (b_ & 0xff) << 16 | (g_ & 0xff) << 8 | (r_ & 0xff);
    }
    {
        int a_ = color_get_a(0);
        unsigned int r_ = color_get_r(0);
        unsigned int g_ = color_get_g(0);
        unsigned int b_ = color_get_b(0);

        style.col[2] = ((int)(a_ * w->alpha) + 1) >> 1 << 24 | (b_ & 0xff) << 16 | (g_ & 0xff) << 8 | (r_ & 0xff);
    }
    style.width = w->width + w->width_add;
    style.rate_in = 0.7f;
    tiamat_laser_draw_polyline(&style, pts, n, 1);
    RenderSetCurrentDisplayList(dl);
}

/* 0x00b0e0e0 stg03, 0x00b1baa8 stg22 */
void tiamat_laser_disp_straight(TiamatLaserWork *w)
{
    Vec3 v[2];

    v3_copy(&v[0], &w->pos);
    v3_copy(&v[1], &w->tail);
    tiamat_laser_draw_beam(w, v, 1);
}

/* 0x00b0e138 stg03, 0x00b1bb00 stg22 */
void tiamat_laser_disp_bent(TiamatLaserWork *w)
{
    Vec3 v[3];

    v3_copy(&v[0], &w->pos);
    v3_copy(&v[1], &w->bend);
    v3_copy(&v[2], &w->tail);
    tiamat_laser_draw_beam(w, v, 1);
    tiamat_laser_draw_beam(w, &v[1], 1);
}

/* 0x00b0e1c8 stg03, 0x00b1bb90 stg22 */
void tiamat_laser_disp(TiamatLaserWork *w)
{
    if (tiamat_laser_disp_table[w->state])
        tiamat_laser_disp_table[w->state](w);
}

/* 0x00b0e208 stg03, 0x00b1bbd0 stg22 */
void *tiamat_laser_create(void *parent, int pos)
{
    int blk[2];

    heap_bzero(blk, 8);
    blk[1] = pos;
    blk[0] = 0;
    return task_create_sibling_inherit_arg(parent, &tiamat_laser_desc, 204, blk);
}

/* 0x00b0e260 stg03, 0x00b1bc28 stg22 */
#if defined(OVERLAY_STG22)
/* steer(cast) */
void tiamat_laser_attack_ray_init(TiamatLaserAttackRayWork *w, TiamatLaserAttackRayArg *ray)
{
    ModelPart *body = &w->body;
    ModelPart *ring = &w->ring;

    w->arg = *ray;
    w->t = 0;
    dc_object_init(body, *((struct DcNode **)pnb_get_entry(w->models, 53)), *((struct DcClip **)pnb_get_entry(w->models, 54)), 0);
    dc_object_rebuild(body);
    dc_object_init(ring, *((struct DcNode **)pnb_get_entry(w->models, 49)), *((struct DcClip **)pnb_get_entry(w->models, 50)), 0);
    dc_object_rebuild(ring);
    w->laser_state = 0;
    w->gone_frm = 0;
}
#else
/* steer(cast) */
void tiamat_laser_attack_ray_init(TiamatLaserAttackRayWork *w, TiamatLaserAttackRayArg *prm)
{
    w->arg = *prm;
    w->t = 0;
    {
    int *root;
    root = ((int *)pnb_get_entry(w->models, 58));
    dc_object_init(&w->body, *root, *((struct DcClip **)pnb_get_entry(w->models, 59)), 0);
    dc_object_rebuild(&w->body);
    }
    {
    int *root;
    root = ((int *)pnb_get_entry(w->models, 39));
    dc_object_init(&w->ring, *root, *((struct DcClip **)pnb_get_entry(w->models, 40)), 0);
    dc_object_rebuild(&w->ring);
    }
    w->laser_state = 0;
    w->gone_frm = 0;
}
#endif

/* 0x00b0e340 stg03, 0x00b1bd08 stg22 */
void tiamat_laser_attack_ray_release(TiamatLaserAttackRayWork *w)
{
    dc_object_release(&w->ring);
    dc_object_release(&w->body);
}

/* 0x00b0e370 stg03, 0x00b1bd38 stg22 */
void tiamat_laser_attack_ray_exec(TiamatLaserAttackRayWork *w)
{
    TiamatLaserAttackRayArg *prm = &w->arg;
    Vec3 pl_vel;

    if (!prm->owner) {
        player_get_frame_vel(&pl_vel);
        v3_add(&w->pos, &pl_vel);
        if (++w->gone_frm >= 30.0f)
            FINISH_IF(w);
    } else if (TASK_OF(prm->owner)->flags & TASK_F_END) {
        Vec3 *p = &w->pos;
        int *ang = w->ang;

        p->x = prm->pos->x;
        ang[0] = prm->ang[0];
        p->y = prm->pos->y;
        ang[1] = prm->ang[1];
        p->z = prm->pos->z;
        ang[2] = prm->ang[2];
        prm->pos = p;
        prm->ang = ang;
        prm->owner = 0;
    }
    w->t += enemy_status_get_ticks();
    if (w->t > prm->frames) {
        w->t = prm->frames;
        FINISH_IF(w);
    }
    if (w->laser_state == 1 && prm->owner) {
        tiamat_laser_create(w, (int)prm->pos);
        w->laser_state = 2;
    }
}

/* 0x00b0e4e8 stg03, 0x00b1beb0 stg22 */
/* steer(cast) */
void tiamat_laser_attack_ray_disp(TiamatLaserAttackRayWork *w)
{
    TiamatLaserAttackRayArg *prm = &w->arg;
    int dur = prm->frames;
    int tick = w->t * 3;
    int idx = tick / dur;
    int blend = 0;
    int dl;
    float frac;
    float scl;
    float rgba[4];
    ModelPart *rmdl;

    dl = RenderSetCurrentDisplayList(6);
    frac = (float)(tick - idx * dur) / dur;
    ulGxSetTextureList(w->textures);
    ulMatrixPush();
    do {
        Vec3 *p_ = w->arg.pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    tiamat_laser_rotate_yxz(prm->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    if (w->gone_frm > 0) {
        blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        heap_bzero((int *)rgba, 16);
        draw_push_strip_flags();
        draw_push_override_color();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 0x800);
        rgba[0] = -(float)w->gone_frm / 30.0f;
        draw_set_override_color(rgba);
    }
    if (idx < 2) {
        ModelPart *bmdl = &w->body;

        vu0_matrix_scale(ulMatrixCur, 1.0f, 1.0f, 1.0f);
        model_node_draw_tree2_anim(bmdl->root, bmdl->clip, bmdl->frames * frac);
    }
    ulMatrixUnitRotation((float *)ulMatrixCur);
    rmdl = &w->ring;
    switch (idx) {
    case 0:
        scl = frac * 0.1f;
        vu0_matrix_scale(ulMatrixCur, scl, scl, scl);
        model_object_draw(rmdl, 0.0f);
        break;
    case 1:
        scl = frac * 0.4f + 0.1f;
        vu0_matrix_scale(ulMatrixCur, scl, scl, scl);
        model_object_draw(rmdl, 0.0f);
        break;
    case 2:
        scl = frac * 0.5f + 0.5f;
        vu0_matrix_scale(ulMatrixCur, scl, scl, scl);
        model_object_draw(rmdl, rmdl->frames * frac);
        if (w->laser_state == 0)
            w->laser_state = 1;
        break;
    }
    if (w->gone_frm > 0) {
        draw_pop_strip_flags();
        draw_pop_override_color();
        draw_set_override_flags(blend);
    }
    RenderSetCurrentDisplayList(dl);
    ulMatrixPop();
}

/* 0x00b0e790 stg03, 0x00b1c158 stg22 */
void *tiamat_laser_attack_ray_create(void *parent, void *prm)
{
    return task_create_sibling_inherit_arg(parent, &tiamat_laser_attack_ray_desc, 244, prm);
}

/* 0x00b0e7b8 stg03, 0x00b1c180 stg22 */
/* steer(cast) */
void tiamat_laser_turn_eff_init(TiamatLaserTurnEffWork *w, Vec3 *pos)
{
    int *root;

    w->pos = pos;
    w->t = 0;
    root = ((int *)pnb_get_entry(w->models, TIAMAT_LASER_MODEL(58)));
    dc_object_init(&w->body, *root, *((struct DcClip **)pnb_get_entry(w->models, TIAMAT_LASER_MODEL(59))), 0);
    dc_object_rebuild(&w->body);
}

/* 0x00b0e828 stg03, 0x00b1c1f0 stg22 */
void tiamat_laser_turn_eff_release(TiamatLaserTurnEffWork *w)
{
    dc_object_release(&w->body);
}

/* 0x00b0e830 stg03, 0x00b1c1f8 stg22 */
void tiamat_laser_turn_eff_exec(TiamatLaserTurnEffWork *w)
{
    if (++w->t >= 30.0f)
        FINISH_IF(w);
}

/* 0x00b0e878 stg03, 0x00b1c240 stg22 */
void tiamat_laser_turn_eff_disp(TiamatLaserTurnEffWork *w)
{
    ModelPart *gfx;
    float rgba[4];
    float rate;
    int blend;

    ulGxSetTextureList(w->tex);
    ulMatrixPush();
    do {
        Vec3 *p_ = w->pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    gfx = &w->body;
    rate = w->t / 30.0f;
    blend = draw_set_override_flags(32);
    draw_push_override_color();
    heap_bzero(rgba, 16);
    rgba[0] = -rate;
    draw_set_override_color(rgba);
    model_node_draw_tree2_anim(gfx->root, gfx->clip, gfx->frames * (1.0f - rate));
    draw_pop_override_color();
    draw_set_override_flags(blend);
    ulMatrixPop();
}

/* 0x00b0e958 stg03, 0x00b1c320 stg22 */
void *tiamat_laser_turn_eff_create(void *parent, void *prm)
{
    return task_create_inherit_arg(parent, &tiamat_laser_turn_eff_desc, 108, prm);
}

/* 0x00b0e980 stg03, 0x00b1c348 stg22 */
void tiamat_laser_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}

/* 0x00b1c398 stg22 */
#if defined(OVERLAY_STG22)

void tiamat_laser_draw_polyline(void *ln, Vec3 *pts, int n, int capped)
{
    Vec3 buf[16];
    float prev;
    int i;

    prev = polyline_set_width_min(0.1f);
    if (n == 1) {
        for (i = 0; i < 16; i++) {
            v3_copy(&buf[i], &pts[0]);
            vec3_approach(&pts[1], &buf[i], i / 15.0f);
        }
        pts = buf;
        n = 15;
    }
    if (capped)
        draw_polyline_wide_capped(ln, pts, n);
    else
        draw_polyline_wide_nocap(ln, pts, n);
    polyline_set_width_min(prev);
}
#endif

