/*
 * Enemy shots ("E_Shot"): e_shot_fire launches a shot from a position along a direction with the
 * starting angles of its level, the hit events react to a shot being hit by its kind, and the
 * shots' tuning variables are registered with the variable editor. Each stage builds its own
 * copy; this one is Area 1's, with a separate hit event for Khafra's shots.
 */

#include "stg01.h"
#include "mathdef.h"
#include "color.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "heap.h"
#include "model.h"
#include "node_draw_state.h"
#include "pnb.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "e_shot.h"
#include "../common/enb_normal.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"

/* Area 1's E_Shot kinds add the wire cover's colours. */
typedef struct EShotColorSet {
    char *name;
    char *label;                /* 0x04 */
    EShotParams p;              /* 0x08 */
    int wire;                   /* 0x68 */
    int wire_linear;            /* 0x6c */
    int wire_col;               /* 0x70 */
    int wire_col2;              /* 0x74 */
    float wire_near;            /* 0x78 */
    float wire_far;             /* 0x7c */
    float wire_near_rate;       /* 0x80 */
    float wire_far_rate;        /* 0x84 */
    int wire_beat;              /* 0x88 */
} EShotColorSet;

extern char e_shot_name[];
extern int e_shot_init_arg;
extern int e_shot2_init_arg;
extern ModelPart e_shot_model;
extern int e_shot_model_loaded;
extern EShotColorSet e_shot_sets[8];
extern VarDef e_shot_vardef;
extern VarDef e_shot_vardefs[33];

/* 0x00b0abc0 */
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
        for (j = 0; j < 33; j++) {
            def->ptr = prm;
            prm++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)e_shot_vardefs);
    }
}

/* 0x00b0acd8 */
void e_shot_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b0ace0 */
int e_shot_vars_init(int *fx_id)
{
    e_shot_init_arg = *fx_id;
    e_shot_vars_register();
    e_shot_model_loaded = 0;
    return 0;
}

/* 0x00b0ad10 */
int e_shot_vars_release(void)
{
    e_shot_vars_unregister();
    if (e_shot_model_loaded) {
        dc_object_release(&e_shot_model);
        e_shot_model_loaded = 0;
    }
    return 0;
}

/* 0x00b0ad58 */
int e_shot2_vars_init(int *fx_id)
{
    e_shot2_init_arg = *fx_id;
    return 0;
}

/* 0x00b0ad70 */
int e_shot2_vars_release(void)
{
    return 0;
}

/* 0x00b0ad78 */
void e_shot_khafra_event(ShotHit *sh)
{
    Vec3 spos;
    SpawnDesc dsc;
    float sc[2];
    int dl;

    switch (sh->op) {
    case 0:
        break;
    case 1:
    case 2:
    case 6:
    case 9:
        heap_bzero(&dsc, sizeof dsc);
        dsc.models = stage_models;
        dsc.type = 43;
        dsc.kind = 1;
        v3_copy(&dsc.pos, sh->pos);
        v3_set(&dsc.scale, 1.0f, 1.0f, 1.0f);
        dsc.lock = sh->lock;
        dsc.ang[0] = sh->ang[0];
        dsc.ang[1] = sh->ang[1];
        dsc.ang[2] = sh->ang[2];
        v3_copy(&dsc.v, &sh->lock->imp.v);
        enemy_shatter_create(sh->owner, &dsc, 0, 0);
        break;
    case 3:
        *sh->spin += enemy_status_get_ticks() * (ANG_TURN / 6.0f);
        sh->ang[2] += enemy_status_get_ticks() * (ANG_TURN / 30.0f);
        break;
    case 4:
        yuki_model_positions_at(&spos, sh->pos, sh->ang, sh->model, 0x1000000, 1);
        draw_set_blend(4, 1);
        dl = RenderSetCurrentDisplayList(6);
        ulSinCos(*sh->spin, &sc[0], &sc[1]);
        sc[0] = (sc[0] + 1.0f) * 0.5f * 0.70000005f + 0.5f;
        ulGxSetTextureList(stage_textures);
        yuki_sprite2_draw3d(0, RGBA(0x60, 0x60, 0x60, 0x80), 1, &spos, sc[0] * 10.0f, sc[0] * 10.0f);
        RenderSetCurrentDisplayList(dl);
        break;
    }
}

/* steer(cast) */
/* 0x00b0afb8 */
void e_shot_type_event(ShotHit *sh)
{
    Vec3 spos;
    SpawnDesc dsc;
    float sc[2];
    EShotColorSet *es = &e_shot_sets[sh->level];

    switch (sh->op) {
    case 0:
        break;
    case 7:
        if (!e_shot_model_loaded) {
            e_shot_model_loaded = 1;
            dc_object_init(&e_shot_model, *((struct DcNode **)pnb_get_entry(stage_models, 45)), 0, 0x500);
            dc_object_set_wire(&e_shot_model, model_pnb_get_wire(stage_models, 45));
        }
        break;
    case 1:
    case 2:
    case 6:
    case 9:
        heap_bzero(&dsc, sizeof dsc);
        dsc.type = 46;
        dsc.kind = 1;
        v3_copy(&dsc.pos, sh->pos);
        v3_set(&dsc.scale, 1.0f, 1.0f, 1.0f);
        dsc.lock = sh->lock;
        dsc.ang[0] = sh->ang[0];
        dsc.models = stage_models;
        dsc.ang[1] = sh->ang[1];
        dsc.ang[2] = sh->ang[2];
        v3_copy(&dsc.v, &sh->lock->imp.v);
        enemy_shatter_create(sh->owner, &dsc, 0, 0);

        break;
    case 3:
        *sh->spin += enemy_status_get_ticks() * (ANG_TURN / 6.0f);
        sh->ang[2] += enemy_status_get_ticks() * (ANG_TURN / 30.0f);
        break;
    case 4:
        yuki_model_positions_at(&spos, sh->pos, sh->ang, sh->model, 0x1000000, 1);
        draw_set_blend(4, 1);
        ulSinCos(*sh->spin, &sc[0], &sc[1]);
        sc[0] = (sc[0] + 1.0f) * 0.5f * 0.70000005f + 0.5f;
        ulGxSetTextureList(stage_textures);
        yuki_sprite2_draw3d(0, RGBA(0x60, 0x60, 0x60, 0x80), 1, &spos, sc[0] * 10.0f, sc[0] * 10.0f);
        ulMatrixPush();
        do {
            Vec3 *p_ = sh->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        e_shot_rotate_yxz(sh->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        if (es->wire) {
            draw_set_color(*sh->flip ? es->wire_col : es->wire_col2);
            *sh->flip = 1 - *sh->flip;
            draw_set_blend(4, 5);
            model_object_draw(&e_shot_model, sh->model->frame);
        }
        ulMatrixPop();
        break;
    }
}

/* 0x00b0b300 */
void e_shot_event(ShotHit *hit)
{
    switch (hit->level) {
    case 0: case 1: case 2: case 3:
        e_shot_type_event(hit);
        break;
    case 4: case 5: case 6: case 7:
        e_shot_khafra_event(hit);
        break;
    }
}

/* steer(cast) */
/* 0x00b0b338 */
void e_shot_fire(void *par, Vec3 *pos, Vec3 *dir, int level)
{
    ShotDesc desc;
    EShotColorSet *es = &e_shot_sets[level];

    heap_bzero(&desc, sizeof desc);
    desc.slot = -1;
    switch (es->p.disp_type) {
    case 0:
        desc.type = 0;
        desc.fog = (void *)1;
        desc.root = (int *)*((int *)pnb_get_entry(stage_models, 44));
        desc.tex = stage_textures;
        desc.use_tail = es->p.use_tail;
        desc.tail_sec = es->p.tail_sec;
        desc.tail_roll_spd = es->p.tail_roll_spd;
        desc.tail_seg = 1;
        desc.tail_mask = 0x2000000;
        desc.tail_width = 1.0f;
        desc.tail_skip = es->p.tail_skip;
        desc.kind = e_shot_init_arg;
        desc.clip = 0;
        desc.mflags = 0;
        desc.tail_tex = 0;
        desc.spin = 0;
        desc.flip = 0;
        break;
    case 1:
        desc.type = 0;
        desc.fog = (void *)1;
        desc.root = (int *)*((int *)pnb_get_entry(stage_models, 42));
        desc.tex = stage_textures;
        desc.use_tail = es->p.use_tail;
        desc.tail_sec = es->p.tail_sec;
        desc.tail_roll_spd = es->p.tail_roll_spd;
        desc.tail_seg = 1;
        desc.tail_mask = 0x2000000;
        desc.tail_width = 1.0f;
        desc.tail_skip = es->p.tail_skip;
        desc.kind = e_shot2_init_arg;
        desc.clip = 0;
        desc.mflags = 0;
        desc.tail_tex = 0;
        desc.spin = 0;
        break;
    }
    desc.radius = es->p.collision_r;
    desc.motion_sec = es->p.motion_sec;
    desc.spd_start = es->p.spd_start;
    desc.spd_chg_dis_start = es->p.spd_chg_dis_start;
    desc.spd_chg_dis_finish = es->p.spd_chg_dis_finish;
    desc.spd_chg_rate = es->p.spd_chg_rate;
    desc.curve_rate = es->p.curve_rate;
    desc.curve_type = es->p.curve_type;
    desc.curve_time = es->p.curve_time;
    desc.pursuit = es->p.pursuit;
    desc.hp = es->p.hp;
    desc.same_lock = es->p.same_lock;
    desc.score = es->p.score;
    desc.damage = es->p.damage;
    desc.frames = es->p.life_time * FPS;
    desc.cb = e_shot_event;
    desc.level = level;
    desc.clip_ang = es->p.clip_ang;
    desc.spin = 0;
    enb_normal_fire(par, pos, dir, 1, e_shot_name, (int *)&desc);
}

/* 0x00b0b5a8 */
void e_shot_initial_angles(int *ang, Vec3 *pos, Vec3 *dir, int level)
{
    ShotDesc desc;
    EShotColorSet *set = &e_shot_sets[level];

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

/* 0x00b0b660 */
void e_shot_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
