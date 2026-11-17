/*
 * Area 5 enemy shots (E_Shot): shot sets fired through e_shot_fire: smoke-trail shots, the
 * sidewinder (flashing, bursts into a model break-up) and spinning shots.
 */

#include "stg05.h"
#include "mathdef.h"
#include "color.h"
#include "debug.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "heap.h"
#include "model.h"
#include "node_draw_state.h"
#include "pnb.h"
#include "render.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "enb_sidewinder.h"
#include "ef_mdl_brk.h"
#include "en_aangel_exp.h"
#include "stage05.h"
#include "../common/en_backfire.h"
#include "../common/enb_normal.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"

extern int e_shot_init_arg;
extern int e_shot2_init_arg;
extern int e_shot3_init_arg;
extern EShotParamSet e_shot_sets[7];
extern VarDef e_shot_vardef;
extern VarDef e_shot_vardefs[24];
extern char e_shot_kind[];
extern char e_shot_sting_kind[];
extern Vec3 e_shot_spin_scale;

/* 228 bytes, allocated on event 7: flashing model pair */
typedef struct ShotFlash {
    Rgba4 color;                /* red to white and back */
    ModelPart core;                /* 0x10: model 70, drawn with the colour */
    ModelPart body;                /* 0x6c: model 69 */
    char pad_c8[12];
    int n;                      /* 0xd4: frame counter, 0..17 */
    int ang[3];                 /* 0xd8: spin */
} ShotFlash;

extern char str_enb_sidewinder_c[];

/* 0x00b26fe8 */
void e_shot_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *prm;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&e_shot_vardef);
    for (i = 0; i < 7; i++) {
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

/* 0x00b27100 */
void e_shot_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b27108 */
int e_shot_vars_init(int *fx_id)
{
    e_shot_init_arg = *fx_id;
    e_shot_vars_register();
    return 0;
}

/* 0x00b27130 */
int e_shot_vars_release(void)
{
    e_shot_vars_unregister();
    return 0;
}

/* 0x00b27150 */
int e_shot2_vars_init(int *fx_id)
{
    e_shot2_init_arg = *fx_id;
    return 0;
}

/* 0x00b27168 */
int e_shot2_vars_release(void)
{
    return 0;
}

/* 0x00b27170 */
int e_shot3_vars_init(int *fx_id)
{
    e_shot3_init_arg = *fx_id;
    return 0;
}

/* 0x00b27188 */
int e_shot3_vars_release(void)
{
    return 0;
}

/* 0x00b27190 */
void e_shot_setup_model65(int *w, EShotSet *typ)
{
    void **mdl;

    w[3] = 1;
    w[2] = 0;
    mdl = (void **)pnb_get_entry(stage_models, 65);
    w[31] = 0x2000000;
    w[28] = 1;
    w[8] = 0;
    w[27] = 1;
    *(void **)&w[5] = *mdl;
    *(float *)&w[32] = 1.0f;
    w[33] = typ->v[16];
    w[6] = 0;
    w[41] = 0;
}

/* 0x00b27218 */
void e_shot_trail_event(ShotHit *hit)
{
    Vec3 buf;
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
        desc.type = 66;
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
        if (hit->pos->y < stage_ground_height(hit->pos) + hit->model->radius)
            enb_normal_kill(hit->owner);
        break;
    case 4:
        yuki_model_positions_at(&buf, hit->pos, hit->ang, hit->model, 0x1000000, 1);
        draw_set_blend(4, 1);
        dl = RenderSetCurrentDisplayList(6);
        ulSinCos(*hit->spin, &sc[0], &sc[1]);
        sc[0] = (sc[0] + 1.0f) * 0.5f * 0.70000005f + 0.5f;
        ulGxSetTextureList(stage_textures);
        yuki_sprite2_draw3d(0, RGBA(0x60, 0x60, 0x60, 0x80), 0, &buf, sc[0] * 10.0f, sc[0] * 10.0f);
        RenderSetCurrentDisplayList(dl);
        break;
    }
}

/* 0x00b27488 */
void e_shot_setup_model69(int *w, EShotSet *typ)
{
    int *mdl;

    w[2] = 2;
    w[3] = 1;
    mdl = pnb_get_entry(stage_models, 69);
    w[5] = *mdl;
    w[32] = 0;
    w[6] = 0;
    w[8] = 0;
    w[27] = 0;
    w[31] = 0;
    w[28] = 1;
}

/* steer(cast) */
/* 0x00b274f0 */
void e_shot_sidewinder_event(ShotHit *hit)
{
    EfMdlBrkArg prm;
    ShotFlash *flash = *(ShotFlash **)hit->spin;
    ShotFlash *cur = flash;
    unsigned int col_from, col_to, col;
    int prev;

    switch (hit->op) {
    case 0:
    case 5:
        break;
    case 7:
        flash = heap_block_tag(game_malloc(sizeof *flash), str_enb_sidewinder_c, 640);
        heap_bzero(flash, sizeof *flash);
        dc_object_init(&flash->core, *((struct DcNode **)pnb_get_entry(stage_models, 70)), 0, 0);
        dc_object_init(&flash->body, *((struct DcNode **)pnb_get_entry(stage_models, 69)), 0, 0);
        *(ShotFlash **)hit->spin = flash;
        break;
    case 8:
        dc_object_release(&flash->core);
        dc_object_release(&flash->body);
        heap_free(flash, 0);
        break;
    case 1:
        en_aangel_exp_create(hit->owner, hit->pos, 0, 0, 0);
        break;
    case 2:
    case 6:
    case 9:
        heap_bzero(&prm, sizeof prm);
        prm.pos.x = hit->pos->x;
        prm.ang[0] = hit->ang[0];
        prm.pos.y = hit->pos->y;
        prm.ang[1] = hit->ang[1];
        prm.pos.z = hit->pos->z;
        prm.ang[2] = hit->ang[2];
        prm.gfx = &flash->body;
        prm.scatter_dir.x = 0;
        prm.scatter_dir.y = 0;
        prm.scatter_dir.z = 0;
        prm.one = 1;
        ef_mdl_brk_start(hit->owner, &prm);
        prm.colored = 1;
        prm.gfx = &flash->core;
        prm.color = cur->color;
        ef_mdl_brk_start(hit->owner, &prm);
        break;
    case 3:
        flash->n = (flash->n + 1) % 18;
        if (flash->n / 9 == 0) {
            col_from = RGBA(0x80, 0, 0, 0x80);
            col_to = COL_WHITE;
        } else {
            col_from = COL_WHITE;
            col_to = RGBA(0x80, 0, 0, 0x80);
        }
        col = color_blend(col_from, col_to, flash->n % 4 * 0.25f);
        flash->color.rgba[0] = MIN(MAX_GE(color_get_a(col) * 0.003921569f, 0.0f), 1.0f);
        flash->color.rgba[1] = MIN(MAX_GE((int)((int (*)(unsigned int))color_get_r)(col) * 0.003921569f, 0.0f), 1.0f);
        flash->color.rgba[2] = MIN(MAX_GE((int)color_get_g(col) * 0.003921569f, 0.0f), 1.0f);
        flash->color.rgba[3] = MIN(MAX_GE((int)color_get_b(col) * 0.003921569f, 0.0f), 1.0f);
        flash->ang[0] += enemy_status_get_ticks() * 1092.2667f;
        flash->ang[1] += enemy_status_get_ticks() * 0.0f;
        flash->ang[2] += enemy_status_get_ticks() * 0.0f;
        if (hit->pos->y < stage_ground_height(hit->pos) + flash->body.radius)
            enb_normal_kill(hit->owner);
        break;
    case 4:
        if (view_sphere_visible(hit->pos, flash->body.radius)) {
            ulGxSetTextureList(stage_textures);
            ulMatrixPush();
            do {
                Vec3 *p_ = hit->pos;
                vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
            } while (0);
            e_shot_rotate_yxz(hit->ang);
            vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
            e_shot_rotate_yxz(flash->ang);
            dc_object_draw(&flash->body);
            prev = draw_set_override_flags(16);
            draw_push_override_color();
            draw_set_override_color(flash->color.rgba);
            dc_object_draw(&flash->core);
            draw_pop_override_color();
            draw_set_override_flags(prev);
            ulMatrixPop();
        }
        break;
    }
}

/* 0x00b27b28 */
void e_shot_setup_model19(int *w, EShotSet *typ)
{
    void **mdl;

    w[3] = 1;
    w[2] = 0;
    mdl = (void **)pnb_get_entry(stage_models, 19);
    w[28] = 1;
    w[27] = 1;
    w[8] = 0;
    w[31] = 0;
    w[33] = typ->v[16];
    *(void **)&w[5] = *mdl;
    *(float *)&w[32] = 1.0f;
    w[6] = 0;
}

/* 0x00b27ba8 */
void e_shot_spin_event(ShotHit *hit)
{
    SpawnDesc desc;
    BackFireLink lnk;

    switch (hit->op) {
    case 0:
        break;
    case 7:
        *(void **)hit->spin = yuki_alloc(16);
        heap_bzero(&lnk, 52);
        lnk.gfx = hit->model;
        lnk.flags = 0x1000000;
        lnk.pos = hit->pos;
        lnk.ang = hit->ang;
        lnk.obj_scale = &e_shot_spin_scale;
        en_backfire_create(hit->owner, &lnk, 1);
        break;
    case 8:
        do {
            yuki_free(*(void **)hit->spin);
        } while (0);
        break;
    case 1:
    case 2:
    case 6:
    case 9:
        heap_bzero(&desc, sizeof desc);
        desc.models = stage_models;
        desc.type = 66;
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
        hit->ang[2] += enemy_status_get_ticks() * (ANG_TURN / 30.0f);
        if (hit->pos->y < stage_ground_height(hit->pos) + hit->model->radius)
            enb_normal_kill(hit->owner);
        break;
    }
}

/* 0x00b27d98 */
void e_shot_event(ShotHit *w)
{
    switch (w->level) {
    case 0:
    case 1:
    case 2:
    case 3:
        e_shot_trail_event(w);
        break;
    case 4:
    case 5:
        e_shot_sidewinder_event(w);
        break;
    case 6:
        e_shot_spin_event(w);
        break;
    }
}

/* 0x00b27de8 */
void *e_shot_fire(void *obj, Vec3 *pos, Vec3 *dir, int n)
{
    ShotDesc desc;
    EShotParamSet *typ = &e_shot_sets[n];
    void *k;

    heap_bzero(&desc, sizeof desc);
    desc.slot = -1;
    switch ((unsigned int)typ->p.disp_type) {
    case 0:
        e_shot_setup_model65((int *)&desc, (EShotSet *)typ);
        desc.kind = e_shot_init_arg;
        k = e_shot_kind;
        break;
    case 1:
        e_shot_setup_model69((int *)&desc, (EShotSet *)typ);
        desc.kind = e_shot3_init_arg;
        k = e_shot_kind;
        break;
    case 2:
        e_shot_setup_model19((int *)&desc, (EShotSet *)typ);
        desc.kind = e_shot2_init_arg;
        k = e_shot_sting_kind;
        break;
    default:
        return 0;
    }
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
    return enb_normal_fire(obj, pos, dir, 1, k, (int *)&desc);
}

/* 0x00b27fc8 */
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

/* 0x00b28080 */
void e_shot_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
