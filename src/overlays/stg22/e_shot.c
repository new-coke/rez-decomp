/* , then models_load/free and use_circle_path. */

#include "stg22.h"
#include "mathdef.h"
#include "color.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "heap.h"
#include "model.h"
#include "node_draw_state.h"
#include "pnb.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "yuki_tool.h"
#include "e_shot.h"
#include "../common/attack_ray.h"
#include "../common/en_backfire.h"
#include "../common/en_hanuman.h"
#include "../common/en_hanuman_shield.h"
#include "../common/en_hunter.h"
#include "../common/en_perfectwall.h"
#include "../common/en_shatter.h"
#include "../common/en_thrones_armor_die.h"
#include "../common/enb_hunter.h"
#include "../common/enb_normal.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"

extern int e_shot_init_arg;
extern int cr_needle_init_arg;
extern EShotParamSet e_shot_sets[8];
extern VarDef e_shot_vardef;
extern VarDef e_shot_vardefs[24];
extern CrNeedleSet cr_needle_sets[3];
extern VarDef cr_needle_vardef;
extern VarDef cr_needle_vardefs[24];
extern void sequence_free(void *h);
extern char stage_textures_file[];
extern void *sequence_load(char *req);
extern char e_shot_kind[];
extern Vec3 cr_needle_unit_scale;
extern char cr_needle_kind[];

/* 0x00b1e310 */
void e_shot_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *prm;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&e_shot_vardef);
    for (i = 0; i < 8; i++) {
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

/* 0x00b1e428 */
void e_shot_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b1e430 */
int e_shot_vars_init(int *fx_id)
{
    e_shot_init_arg = *fx_id;
    e_shot_vars_register();
    return 0;
}

/* 0x00b1e458 */
int e_shot_vars_release(void)
{
    e_shot_vars_unregister();
    return 0;
}

/* 0x00b1e478 */
void e_shot_desc_setup(ShotDesc *shot, EShotParamSet *set)
{
    int *mdl;

    shot->fog = (void *)1;
    shot->type = 0;
    mdl = pnb_get_entry(stage_models, 167);
    shot->tail_seg = 1;
    shot->tail_mask = 0x2000000;
    shot->mflags = 0;
    *(void **)&shot->tail_tex = 0;
    shot->root = (int *)*mdl;
    shot->tail_width = 1.0f;
    shot->tail_skip = (int)*(void **)&set->p.tail_skip; /* steer(cast) */
    shot->clip = 0;
    shot->spin = 0;
}

/* 0x00b1e500 */
void e_shot_type_event(ShotHit *hit)
{
    Vec3 spos;
    SpawnDesc desc;
    float sc[2];

    switch (hit->op) {
    case 0:
        break;
    case 1:
    case 2:
    case 6:
    case 9:
        heap_bzero(&desc, sizeof desc);
        desc.type = 169;
        desc.kind = 1;
        v3_copy(&desc.pos, hit->pos);
        v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
        desc.lock = hit->lock;
        desc.ang[0] = hit->ang[0];
        desc.models = stage_models;
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
        ulSinCos(*hit->spin, &sc[0], &sc[1]);
        sc[0] = (sc[0] + 1.0f) * 0.5f * 0.70000005f + 0.5f;
        ulGxSetTextureList(stage_textures);
        yuki_sprite2_draw3d(0, RGBA(0x60, 0x60, 0x60, 0x80), 1, &spos, sc[0] * 10.0f, sc[0] * 10.0f);
        break;
    }
}

/* 0x00b1e728 */
void e_shot_event(ShotHit *hit)
{
    switch (hit->level) {
    case 0: case 1: case 2: case 3:
    case 4: case 5: case 6: case 7:
        e_shot_type_event(hit);
        break;
    }
}

/* 0x00b1e750 */
int e_shot_fire(void *obj, Vec3 *pos, Vec3 *dir, int n)
{
    ShotDesc desc;
    EShotParamSet *typ = &e_shot_sets[n];
    void *k;

    heap_bzero(&desc, sizeof desc);
    switch ((unsigned int)n) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        desc.kind = e_shot_init_arg;
        k = e_shot_kind;
        desc.slot = -1;
        if (typ->p.disp_type == 0)
            break;
    default:
        return 0;
    }
    e_shot_desc_setup(&desc, typ);
    desc.tex = stage_textures;
    desc.radius = typ->p.collision_r;
    desc.motion_sec = typ->p.motion_sec;
    desc.spd_start = typ->p.spd_start;
    desc.spd_chg_dis_start = typ->p.spd_chg_dis_start;
    desc.spd_chg_dis_finish = typ->p.spd_chg_dis_finish;
    desc.spd_chg_rate = typ->p.spd_chg_rate;
    desc.curve_rate = typ->p.curve_rate;
    desc.curve_type = typ->p.curve_type;
    desc.curve_time = typ->p.curve_time;
    desc.pursuit = typ->p.pursuit;
    desc.damage = typ->p.damage;
    desc.hp = typ->p.hp;
    desc.cb = e_shot_event;
    desc.same_lock = typ->p.same_lock;
    desc.score = typ->p.score;
    desc.level = n;
    desc.frames = typ->p.life_time * FPS;
    desc.use_tail = typ->p.use_tail;
    desc.tail_sec = typ->p.tail_sec;
    desc.tail_roll_spd = typ->p.tail_roll_spd;
    desc.face_target = typ->p.test;
    desc.clip_ang = typ->p.clip_ang;
    enb_normal_fire(obj, pos, dir, 1, k, (int *)&desc);
}

/* 0x00b1e8f0 */
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

/* 0x00b1e9a8 */
void cr_needle_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&cr_needle_vardef);
    for (i = 0; i < 3; i++) {
        def = cr_needle_vardefs;
        val = (int *)&cr_needle_sets[i].p;
        cr_needle_vardef.name = cr_needle_sets[i].name;
        cr_needle_vardef.fmt = cr_needle_sets[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&cr_needle_vardef);
        for (j = 0; j < 24; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)cr_needle_vardefs);
    }
}

/* 0x00b1eac0 */
void cr_needle_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b1eac8 */
int cr_needle_vars_init(int *fx_id)
{
    cr_needle_init_arg = *fx_id;
    cr_needle_vars_register();
    return 0;
}

/* 0x00b1eaf0 */
int cr_needle_vars_release(void)
{
    cr_needle_vars_unregister();
    return 0;
}

/* 0x00b1eb10 */
void cr_needle_desc_setup(ShotDesc *shot, CrNeedleSet *typ)
{
    int *mdl;

    shot->fog = (void *)1;
    shot->type = 0;
    mdl = pnb_get_entry(stage_models, 78);
    shot->root = (int *)*mdl;
    shot->tail_mask = 0x4000000;
    shot->tail_width = 1.0f;
    shot->clip = 0;
    shot->mflags = 0;
    shot->tail_tex = 0;
    shot->spin = 0;
    shot->tail_seg = 1;
    shot->mask = 0x2000000;
}

/* 0x00b1eb90 */
void cr_needle_event(ShotHit *hit)
{
    SpawnDesc desc;
    BackFireLink lnk;

    switch (hit->op) {
    case 0:
        break;
    case 7:
        hit->model->radius = 20.0f;
        heap_bzero(&lnk, 52);
        lnk.gfx = hit->model;
        lnk.flags = 0x4000000;
        lnk.pos = hit->pos;
        lnk.ang = hit->ang;
        lnk.obj_scale = &cr_needle_unit_scale;
        lnk.use_pose = 1;
        en_backfire_create(hit->owner, &lnk, 0);
        break;
    case 1:
    case 2:
    case 6:
    case 9:
        heap_bzero(&desc, sizeof desc);
        desc.models = stage_models;
        desc.type = 79;
        desc.kind = 1;
        v3_copy(&desc.pos, hit->lpos);
        v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
        desc.lock = hit->lock;
        desc.ang[0] = hit->ang[0];
        desc.ang[1] = hit->ang[1];
        desc.ang[2] = hit->ang[2];
        v3_copy(&desc.v, &hit->lock->imp.v);
        enemy_shatter_create(hit->owner, &desc, 0, 0);
        break;
    case 3:
        hit->ang[2] += enemy_status_get_ticks() * (ANG_TURN / FPS);
        break;
    }
}

/* 0x00b1ed40 */
void *cr_needle_fire(void *obj, Vec3 *pos, Vec3 *dir, int n)
{
    ShotDesc shot;
    CrNeedleSet *typ = &cr_needle_sets[n];

    heap_bzero(&shot, sizeof shot);
    shot.slot = -1;
    shot.kind = cr_needle_init_arg;
    cr_needle_desc_setup(&shot, typ);
    shot.tex = stage_textures;
    shot.radius = typ->p.collision_r;
    shot.motion_sec = typ->p.motion_sec;
    shot.spd_start = typ->p.spd_start;
    shot.spd_chg_dis_start = typ->p.spd_chg_dis_start;
    shot.spd_chg_dis_finish = typ->p.spd_chg_dis_finish;
    shot.spd_chg_rate = typ->p.spd_chg_rate;
    shot.curve_rate = typ->p.curve_rate;
    shot.curve_type = typ->p.curve_type;
    shot.curve_time = typ->p.curve_time;
    shot.pursuit = typ->p.pursuit;
    shot.hp = typ->p.hp;
    shot.same_lock = typ->p.same_lock;
    shot.score = typ->p.score;
    shot.damage = typ->p.damage;
    shot.cb = cr_needle_event;
    shot.level = n;
    shot.frames = typ->p.life_time * FPS;
    shot.use_tail = typ->p.use_tail;
    shot.tail_sec = typ->p.tail_sec;
    shot.tail_roll_spd = typ->p.tail_roll_spd;
    shot.tail_skip = typ->p.tail_skip;
    shot.face_target = typ->p.test;
    shot.clip_ang = typ->p.clip_ang;
    v3_scale(dir, typ->p.shot_spd * FRAME_SEC);
    return enb_normal_fire(obj, pos, dir, 1, cr_needle_kind, (int *)&shot);
}

/* 0x00b1ef00 */
void cr_needle_initial_angles(int *ang, Vec3 *pos, Vec3 *dir, int level)
{
    ShotDesc shot;
    CrNeedleSet *typ = &cr_needle_sets[level];

    heap_bzero(&shot, sizeof shot);
    shot.spd_start = typ->p.spd_start;
    shot.spd_chg_dis_start = typ->p.spd_chg_dis_start;
    shot.spd_chg_dis_finish = typ->p.spd_chg_dis_finish;
    shot.spd_chg_rate = typ->p.spd_chg_rate;
    shot.curve_rate = typ->p.curve_rate;
    shot.curve_type = typ->p.curve_type;
    shot.curve_time = typ->p.curve_time;
    shot.pursuit = typ->p.pursuit;
    enb_normal_initial_angles(ang, pos, dir, 1, (int *)&shot);
}

/* 0x00b1efb8 */
void models_load(void)
{
    stage_textures = model_load_timed(stage_textures_file, 0x223000);
    stage_models = model_pnb_load(stage_textures_file - 48);
    stage_sequence = sequence_load(stage_textures_file - 40);
    attack_ray_set_model((int)stage_models + 0xac8, (int)stage_models + 0xce8, stage_textures);
    attack_ray_set_radius(50.0f);
    en_backfire_set_texture(2);
    en_backfire_beam_set_texture(0);
    enemy_shatter_vars_register();
    en_shatter_vars_register();
    en_hanuman_models_init();
    en_hanuman_shield_nodes_init();
    en_hunter_nodes_init();
    enb_hunter_models_init();
    en_perfectwall_models_init();
    en_thrones_armor_die_vars_register();
    en_sort_vars_register();
}

/* 0x00b1f098 */
void models_free(void)
{
    en_sort_vars_unregister();
    en_thrones_armor_die_vars_unregister();
    en_shatter_vars_unregister();
    enemy_shatter_vars_unregister();
    sequence_free(stage_sequence);
    model_pnb_unload(stage_models);
    model_unload_timed(stage_textures);
}

/* 0x00b1f0f0 */
int use_circle_path(void)
{
    return 0;
}
