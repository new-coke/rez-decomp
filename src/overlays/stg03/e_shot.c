/* Tiamat_laser_draw_polyline, e_shot_*, dle/enq/etb vars_init. */

#include "stg03.h"
#include "mathdef.h"
#include "approach.h"
#include "camera.h"
#include "color.h"
#include "debug.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "heap.h"
#include "lens.h"
#include "node_draw_state.h"
#include "pnb.h"
#include "render.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "e_shot.h"
#include "../common/enb_normal.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"

extern int e_shot_init_arg;
extern int enq_missile_init_arg;
extern int dle_shot_init_arg;
extern int dle_shot2_init_arg;
extern int dle_shot3_init_arg;
extern int dle_shot4_init_arg;
extern int dle_shot5_init_arg;
extern int dle_shot6_init_arg;
extern int etb_shot_init_arg;
extern EShotParamSet e_shot_sets[16];
extern VarDef e_shot_vardef;
extern VarDef e_shot_vardefs[24];
extern char e_shot_name[];
extern char dle_shot_name[];
extern char enq_missile_name[];
extern char etb_shot_name[];

/* 0x00b0e9d0 */
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

/* 0x00b0eae8 */
void e_shot_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *prm;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&e_shot_vardef);
    for (i = 0; i < 16; i++) {
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

/* 0x00b0ec00 */
void e_shot_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b0ec08 */
int e_shot_vars_init(int *fx_id)
{
    e_shot_init_arg = *fx_id;
    e_shot_vars_register();
    return 0;
}

/* 0x00b0ec30 */
int e_shot_vars_release(void)
{
    e_shot_vars_unregister();
    return 0;
}

/* 0x00b0ec50 */
int enq_missile_vars_init(int *fx_id)
{
    enq_missile_init_arg = *fx_id;
    return 0;
}

/* 0x00b0ec68 */
int dle_shot_vars_init(int *fx_id)
{
    dle_shot_init_arg = *fx_id;
    return 0;
}

/* 0x00b0ec80 */
int dle_shot2_vars_init(int *fx_id)
{
    dle_shot2_init_arg = *fx_id;
    return 0;
}

/* 0x00b0ec98 */
int dle_shot3_vars_init(int *fx_id)
{
    dle_shot3_init_arg = *fx_id;
    return 0;
}

/* 0x00b0ecb0 */
int dle_shot4_vars_init(int *fx_id)
{
    dle_shot4_init_arg = *fx_id;
    return 0;
}

/* 0x00b0ecc8 */
int dle_shot5_vars_init(int *fx_id)
{
    dle_shot5_init_arg = *fx_id;
    return 0;
}

/* 0x00b0ece0 */
int dle_shot6_vars_init(int *fx_id)
{
    dle_shot6_init_arg = *fx_id;
    return 0;
}

/* 0x00b0ecf8 */
int etb_shot_vars_init(int *fx_id)
{
    etb_shot_init_arg = *fx_id;
    return 0;
}

/* 0x00b0ed10 */
void e_shot_setup_model5(ShotDesc *w, EShotParamSet *set)
{
    int *ent;

    w->fog = (void *)1;
    w->type = 0;
    ent = pnb_get_entry(stage_models, 5);
    *(void **)&w->tail_tex = (void *)1;
    *(void **)&w->tail_seg = (void *)1;
    *(void **)&w->mflags = 0;
    *(void **)&w->tail_mask = (void *)0x2000000;
    w->root = (int *)*ent;
    w->tail_width = 1.0f;
    w->tail_skip = (int)*(void **)&set->p.tail_skip; /* steer(cast) */
    w->clip = 0;
    w->spin = 0;
}

/* 0x00b0ed98 */
void e_shot_common_event(ShotHit *hit)
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
        desc.type = 7;
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

/* 0x00b0efd8 */
void e_shot_setup_model15(ShotDesc *w, EShotParamSet *set)
{
    int *ent;

    w->type = 0;
    w->fog = (void *)1;
    ent = pnb_get_entry(stage_models, 15);
    *(void **)&w->mflags = 0;
    *(void **)&w->tail_tex = 0;
    *(void **)&w->tail_seg = 0;
    *(void **)&w->tail_mask = 0;
    w->root = (int *)*ent;
    w->tail_width = 1.0f;
    w->tail_skip = (int)*(void **)&set->p.tail_skip; /* steer(cast) */
    w->clip = 0;
    w->spin = 0;
}

/* 0x00b0f058 */
void e_shot_enkidu_event(ShotHit *hit)
{
    SpawnDesc desc;

    switch (hit->op) {
    case 0:
        break;
    case 1:
    case 2:
    case 6:
    case 9:
        heap_bzero(&desc, sizeof desc);
        desc.models = stage_models;
        desc.type = 17;
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
        break;
    }
}

/* 0x00b0f190 */
void e_shot_sprite_draw3d(float off, float wid, float hgt, Vec3 *pos, int angle, unsigned int col, int tex)
{
    Vec3 v;
    Vec3 eye;
    Vec3 pt;
    Vec3 view;
    Vec3 sc;
    Vec3 dir;
    Vec3 side;
    float unit;
    float len, rate;

    if (!view_sphere_visible(pos, wid >= hgt ? wid : hgt))
        return;
    ulMatrixApplyPoint((float *)ulMatrixCur, (float *)pos, (float *)&v);
    if (!(v.z < -cam_near))
        return;
    unit = 1.0f;
    camera_get_eye(&eye);
    camera_get_view_vec(&view);
    dir.x = pos->x - eye.x;
    dir.y = pos->y - eye.y;
    dir.z = pos->z - eye.z;
    ulVec3Cross(&side, &view, &dir);
    ulVec3Cross(&sc, &side, &dir);
    vec3_normalize(&sc, &sc);
    pt.x = pos->x + sc.x * off;
    pt.y = pos->y + sc.y * off;
    pt.z = pos->z + sc.z * off;
    sc.x = pt.x - eye.x;
    sc.y = pt.y - eye.y;
    sc.z = pt.z - eye.z;
    len = vec3_length(&sc);
    rate = (len - off) / len;
    pt.x = eye.x + sc.x * rate;
    pt.y = eye.y + sc.y * rate;
    pt.z = eye.z + sc.z * rate;
    sc.x = unit;
    sc.y = 0.0f;
    sc.z = v.z;
    project_point(&v, &v);
    project_point_abs((float *)&sc, (float *)&sc);
    ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&pt, (float *)&pt);
    project_point(&pt, &pt);
    ulGxSetTextureList(stage_textures);
    yuki_sprite_draw(angle, col, tex, v.x, v.y, unit / pt.z, sc.x * wid, sc.x * hgt);
}

/* 0x00b0f430 */
void e_shot_setup_model60(ShotDesc *w, EShotParamSet *set)
{
    int *ent;

    w->type = 0;
    w->fog = (void *)1;
    ent = pnb_get_entry(stage_models, 60);
    *(void **)&w->mflags = 0;
    *(void **)&w->tail_tex = 0;
    *(void **)&w->tail_seg = 0;
    *(void **)&w->tail_mask = 0;
    w->root = (int *)*ent;
    w->tail_width = 1.0f;
    w->tail_skip = (int)*(void **)&set->p.tail_skip; /* steer(cast) */
    w->clip = 0;
    w->spin = 0;
}

/* 0x00b0f4b0 */
void e_shot_ishtar_event(ShotHit *hit)
{
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
        desc.type = 61;
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
        draw_set_blend(4, 1);
        dl = RenderSetCurrentDisplayList(6);
        ulSinCos(*hit->spin, &sc[0], &sc[1]);
        sc[0] = (sc[0] + 1.0f) * 0.5f * 0.20000005f + 1.5f;
        sc[1] = (sc[1] + 1.0f) * 0.5f * 0.20000005f + 1.5f;
        ulGxSetTextureList(stage_textures);
        e_shot_sprite_draw3d(7.7f, sc[0] * 10.0f, sc[0] * 10.0f, hit->pos, hit->ang[2], COL_WHITE, 3);
        RenderSetCurrentDisplayList(dl);
        break;
    }
}

/* 0x00b0f700 */
void e_shot_event(ShotHit *hit)
{
    switch (hit->level) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
        e_shot_common_event(hit);
        break;
    case 8:
        e_shot_enkidu_event(hit);
        break;
    case 15:
        e_shot_ishtar_event(hit);
        break;
    }
}

/* 0x00b0f750 */
int e_shot_fire(void *parent, Vec3 *pos, Vec3 *dir, int level)
{
    ShotDesc desc;
    EShotParamSet *set = &e_shot_sets[level];
    char *name;

    heap_bzero(&desc, sizeof desc);
    switch (level) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        desc.kind = e_shot_init_arg;
        name = e_shot_name;
        break;
    case 8:
        desc.kind = enq_missile_init_arg;
        name = enq_missile_name;
        break;
    case 9:
        desc.kind = dle_shot_init_arg;
        name = dle_shot_name;
        break;
    case 10:
        desc.kind = dle_shot2_init_arg;
        name = dle_shot_name;
        break;
    case 11:
        desc.kind = dle_shot3_init_arg;
        name = dle_shot_name;
        break;
    case 12:
        desc.kind = dle_shot4_init_arg;
        name = dle_shot_name;
        break;
    case 13:
        desc.kind = dle_shot5_init_arg;
        name = dle_shot_name;
        break;
    case 14:
        desc.kind = dle_shot6_init_arg;
        name = dle_shot_name;
        break;
    case 15:
        desc.kind = etb_shot_init_arg;
        name = etb_shot_name;
        break;
    default:
        return 0;
    }
    desc.slot = -1;
    switch ((unsigned int)set->p.disp_type) {
    case 0:
        e_shot_setup_model5(&desc, set);
        break;
    case 1:
        e_shot_setup_model15(&desc, set);
        break;
    case 2:
        e_shot_setup_model60(&desc, set);
        break;
    default:
        return 0;
    }
    desc.tex = stage_textures;
    desc.radius = set->p.collision_r;
    desc.motion_sec = set->p.motion_sec;
    desc.spd_start = set->p.spd_start;
    desc.spd_chg_dis_start = set->p.spd_chg_dis_start;
    desc.spd_chg_dis_finish = set->p.spd_chg_dis_finish;
    desc.spd_chg_rate = set->p.spd_chg_rate;
    desc.curve_rate = set->p.curve_rate;
    desc.curve_type = set->p.curve_type;
    desc.curve_time = set->p.curve_time;
    desc.pursuit = set->p.pursuit;
    desc.damage = set->p.damage;
    desc.hp = set->p.hp;
    desc.cb = e_shot_event;
    desc.same_lock = set->p.same_lock;
    desc.score = set->p.score;
    desc.level = level;
    desc.frames = set->p.life_time * FPS;
    desc.use_tail = set->p.use_tail;
    desc.tail_sec = set->p.tail_sec;
    desc.tail_roll_spd = set->p.tail_roll_spd;
    desc.face_target = set->p.test;
    desc.clip_ang = set->p.clip_ang;
    return enb_normal_fire(parent, pos, dir, 1, name, (int *)&desc);
}

/* 0x00b0f9d0 */
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
