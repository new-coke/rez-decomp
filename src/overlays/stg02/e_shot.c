/*
 * Enemy shots ("E_Shot"): e_shot_fire launches a shot from a position along a direction with the
 * starting angles of its level, the hit events react to a shot being hit by its kind, and the
 * shots' tuning variables are registered with the variable editor. Each stage builds its own
 * copy; this one is Area 2's, with a separate hit event for the missiles.
 */

#include "stg02.h"
#include "mathdef.h"
#include "color.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "heap.h"
#include "node_draw_state.h"
#include "pnb.h"
#include "render.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "yuki_tool.h"
#include "e_shot.h"
#include "../common/enb_normal.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"

extern int e_shot_init_arg;
extern int e_shot_vt_bullet_init_arg;
extern int e_shot_stx_missile_init_arg;
extern EShotParamSet e_shot_sets[12];
extern VarDef e_shot_vardef;
extern VarDef e_shot_vardefs[24];
extern char e_shot_name[];
extern char e_shot_vt_bullet_name[];
extern char e_shot_stx_missile_name[];

/* 0x00b14dd8 */
void e_shot_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *prm;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&e_shot_vardef);
    for (i = 0; i < 12; i++) {
        def = e_shot_vardefs;
        prm = (int *)&e_shot_sets[i].p;
        e_shot_vardef.name = e_shot_sets[i].name;
        e_shot_vardef.fmt = e_shot_sets[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&e_shot_vardef);
        for (j = 0; j < 24; j++) {
            def->ptr = prm;
            prm++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)e_shot_vardefs);
    }
}

/* 0x00b14ef0 */
void e_shot_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b14ef8 */
int e_shot_vars_init(int *fx_id)
{
    e_shot_init_arg = *fx_id;
    e_shot_vars_register();
    return 0;
}

/* 0x00b14f20 */
int e_shot_vars_release(void)
{
    e_shot_vars_unregister();
    return 0;
}

/* 0x00b14f40 */
int e_shot_vt_bullet_vars_init(int *prm)
{
    e_shot_vt_bullet_init_arg = *prm;
    return 0;
}

/* 0x00b14f58 */
int e_shot_stx_missile_vars_init(int *prm)
{
    e_shot_stx_missile_init_arg = *prm;
    return 0;
}

/* 0x00b14f70 */
void e_shot_setup_model5(ShotDesc *w, EShotParamSet *set)
{
    int *ent;

    w->fog = (void *)1;
    w->type = 0;
    ent = pnb_get_entry(stage_models, 5);
    w->tail_tex = 5;
    *(void **)&w->tail_seg = (void *)1;
    *(void **)&w->mflags = 0;
    *(void **)&w->tail_mask = (void *)0x2000000;
    w->root = (int *)*ent;
    w->tail_width = 1.0f;
    w->tail_skip = (int)*(void **)&set->p.tail_skip; /* steer(cast) */
    w->clip = 0;
    w->spin = 0;
}

/* 0x00b15000 */
void e_shot_type_event(ShotHit *hit)
{
    Vec3 spos;
    SpawnDesc desc;
    float sc[2];
    int dl;

    switch (hit->op) {
    case 0:
        break;
    case 1:
    case 2:
    case 6:
    case 9:
        heap_bzero(&desc, sizeof desc);
        desc.models = stage_models;
        desc.type = 4;
        desc.kind = 1;
        v3_copy(&desc.pos, hit->pos);
        v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
        desc.lock = hit->lock;
        desc.ang[0] = hit->ang[0];
        desc.ang[1] = hit->ang[1];
        desc.ang[2] = hit->ang[2];
        v3_copy(&desc.v, &hit->lock->imp.v);
        enemy_shatter_create(hit->owner, &desc, 0, 0);
        break;
    case 3:
        *hit->spin += enemy_status_get_ticks() * (ANG_TURN / 6.0f);
        hit->ang[2] += enemy_status_get_ticks() * (ANG_TURN / 30.0f);
        break;
    case 4:
        yuki_model_positions_at(&spos, hit->pos, hit->ang, hit->model, 0x1000000, 1);
        draw_set_blend(4, 1);
        dl = RenderSetCurrentDisplayList(6);
        ulSinCos(*hit->spin, &sc[0], &sc[1]);
        sc[0] = (sc[0] + 1.0f) * 0.5f * 0.70000005f + 0.5f;
        ulGxSetTextureList(stage_textures);
        yuki_sprite2_draw3d(0, RGBA(0x60, 0x60, 0x60, 0x80), 4, &spos, sc[0] * 10.0f, sc[0] * 10.0f);
        RenderSetCurrentDisplayList(dl);
        break;
    }
}

/* steer(cast) */
/* 0x00b15240 */
void e_shot_setup_model38(ShotDesc *blk, EShotParamSet *set)
{
    int *mdl;

    blk->fog = (void *)1;
    blk->type = 0;
    blk->root = (int *)*((int *)pnb_get_entry(stage_models, 38));
    mdl = ((int *)pnb_get_entry(stage_models, 39));
    blk->tail_tex = 1;
    ((union { int i; void *p; } *)&blk->tail_mask)->i = 0x2000000;
    blk->tail_seg = 1;
    *(void **)&blk->mflags = 0;
    blk->clip = *mdl;
    blk->spr2 = 1;
    blk->motion_sec = 30.0f;
    blk->tail_width = 1.0f;
    blk->tail_skip = (int)*(void **)&set->p.tail_skip; /* steer(cast) */
    blk->spin = 0;
}

/* 0x00b152f0 */
void e_shot_missile_event(ShotHit *hit)
{
    Vec3 spos;
    SpawnDesc desc;
    float sc[4];
    float rad;
    int dl;

    switch (hit->op) {
    case 0:
        break;
    case 1:
    case 2:
    case 6:
    case 9:
        heap_bzero(&desc, sizeof desc);
        desc.type = 38;
        desc.kind = 1;
        v3_copy(&desc.pos, hit->pos);
        v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
        desc.lock = hit->lock;
        desc.ang[0] = hit->ang[0];
        desc.ang[1] = hit->ang[1];
        desc.models = stage_models;
        desc.ang[2] = hit->ang[2];
        v3_copy(&desc.v, &hit->lock->imp.v);
        enemy_shatter_create(hit->owner, &desc, 0, 0);
        break;
    case 3:
        (*hit->spin)++;
        hit->ang[2] += 546;
        break;
    case 4:
        dl = RenderSetCurrentDisplayList(6);
        yuki_model_positions_at(&spos, hit->pos, hit->ang, hit->model, 0x1000000, 1);
        draw_set_blend(4, 1);
        if (*hit->spin < 18.0f) {
            ulSinCos((int)((*hit->spin << 15) / 18.0f), &sc[0], &sc[1]);
            rad = sc[0] * 1.9000001f + 0.5f;
            ulGxSetTextureList(stage_textures);
            yuki_sprite2_draw3d(0, RGBA(0x60, 0x60, 0x60, 0x80), 0, &spos, rad * 5.0f, rad * 5.0f);
        } else {
            ulSinCos((int)((*hit->spin - 18.0f) * 10922.667f), &sc[2], &sc[3]);
            sc[2] = (sc[2] + 1.0f) * 0.5f * 0.70000005f + 0.5f;
            ulGxSetTextureList(stage_textures);
            yuki_sprite2_draw3d(0, RGBA(0x60, 0x60, 0x60, 0x80), 0, &spos, sc[2] * 5.0f, sc[2] * 5.0f);
        }
        RenderSetCurrentDisplayList(dl);
        break;
    }
}

/* 0x00b155a8 */
void e_shot_event(ShotHit *hit)
{
    switch (hit->level) {
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
        e_shot_type_event(hit);
        break;
    case 8: case 9: case 10: case 11:
        e_shot_missile_event(hit);
        break;
    }
}

/* 0x00b155e0 */
void *e_shot_fire(void *parent, Vec3 *pos, Vec3 *dir, int level)
{
    ShotDesc desc;
    EShotParamSet *set = &e_shot_sets[level];

    heap_bzero(&desc, sizeof desc);
    switch ((unsigned int)level) {
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
        desc.kind = e_shot_init_arg;
        break;
    case 8: case 9:
        desc.kind = e_shot_vt_bullet_init_arg;
        break;
    case 10: case 11:
        desc.kind = e_shot_stx_missile_init_arg;
        break;
    }
    desc.slot = -1;
    switch (set->p.disp_type) {
    case 0:
        e_shot_setup_model5(&desc, set);
        break;
    case 1:
        e_shot_setup_model38(&desc, set);
        break;
    }
    desc.radius = set->p.collision_r;
    desc.motion_sec = set->p.motion_sec;
    desc.curve_type = set->p.curve_type;
    desc.curve_time = set->p.curve_time;
    desc.cb = e_shot_event;
    desc.tex = stage_textures;
    desc.spd_start = set->p.spd_start;
    desc.spd_chg_dis_start = set->p.spd_chg_dis_start;
    desc.spd_chg_dis_finish = set->p.spd_chg_dis_finish;
    desc.spd_chg_rate = set->p.spd_chg_rate;
    desc.curve_rate = set->p.curve_rate;
    desc.pursuit = set->p.pursuit;
    desc.hp = set->p.hp;
    desc.same_lock = set->p.same_lock;
    desc.score = set->p.score;
    desc.damage = set->p.damage;
    desc.level = level;
    desc.frames = set->p.life_time * FPS;
    desc.use_tail = set->p.use_tail;
    desc.tail_sec = set->p.tail_sec;
    desc.tail_roll_spd = set->p.tail_roll_spd;
    desc.face_target = set->p.test;
    desc.clip_ang = set->p.clip_ang;
    switch ((unsigned int)level) {
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
        return enb_normal_fire(parent, pos, dir, 1, e_shot_name, (int *)&desc);
    case 8: case 9:
        return enb_normal_fire(parent, pos, dir, 1, e_shot_vt_bullet_name, (int *)&desc);
    case 10: case 11:
        return enb_normal_fire(parent, pos, dir, 1, e_shot_stx_missile_name, (int *)&desc);
    }
    return 0;
}

/* 0x00b15828 */
void e_shot_initial_angles(int *ang, Vec3 *pos, Vec3 *dir, int level)
{
    ShotDesc desc;
    EShotParamSet *set = &e_shot_sets[level];

    heap_bzero(&desc, sizeof desc);
    desc.spd_start = set->p.spd_start;
    desc.spd_chg_dis_start = set->p.spd_chg_dis_start;
    desc.spd_chg_dis_finish = set->p.spd_chg_dis_finish;
    desc.spd_chg_rate = set->p.spd_chg_rate;
    desc.curve_rate = set->p.curve_rate;
    desc.curve_type = set->p.curve_type;
    desc.curve_time = set->p.curve_time;
    desc.pursuit = set->p.pursuit;
    enb_normal_initial_angles(ang, pos, dir, 1, (int *)&desc);
}
