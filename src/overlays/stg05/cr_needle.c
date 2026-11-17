/* Needle shots, Area 5: shot sets Thrones fires through cr_needle_fire. */

#include "stg05.h"
#include "mathdef.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "heap.h"
#include "pnb.h"
#include "cr_needle.h"
#include "../common/air_mng.h"
#include "../common/en_backfire.h"
#include "../common/enb_normal.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"

extern char air_mng_class[];
extern int cr_needle_init_arg;
extern CrNeedleSet cr_needle_sets[3];
extern VarDef cr_needle_vardef;
extern VarDef cr_needle_vardefs[24];
extern Vec3 cr_needle_unit_scale;
extern char cr_needle_kind[];

/* 0x00b280d0 */
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

/* 0x00b281e8 */
void cr_needle_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b281f0 */
int cr_needle_vars_init(int *fx_id)
{
    cr_needle_init_arg = *fx_id;
    cr_needle_vars_register();
    return 0;
}

/* 0x00b28218 */
int cr_needle_vars_release(void)
{
    cr_needle_vars_unregister();
    return 0;
}

/* 0x00b28238 */
void cr_needle_desc_setup(ShotDesc *shot, CrNeedleSet *typ)
{
    int *mdl;

    shot->fog = (void *)1;
    shot->type = 0;
    mdl = pnb_get_entry(stage_models, 124);
    shot->root = (int *)*mdl;
    shot->tail_tex = 1;
    shot->tail_mask = 0x4000000;
    shot->tail_width = 1.0f;
    shot->clip = 0;
    shot->mflags = 0;
    shot->spin = 0;
    shot->tail_seg = 1;
    shot->mask = 0x2000000;
}

/* 0x00b282b8 */
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
        desc.type = 125;
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

/* 0x00b28468 */
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

/* 0x00b28628 */
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
