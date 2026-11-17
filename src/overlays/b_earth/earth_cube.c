/*
 * Earth boss cubes and Kuri: the shell cubes, placed on a grid over the body and built into node
 * trees. Shot cubes break off, fly away and regrow. In the Kuri attack the hands throw cubes at
 * the player, who can shoot them down. Also player contact checks and cube shatter effects.
 */

#include "types_gw.h"
#include "b_earth.h"
#include "mathdef.h"
#include "debug.h"
#include "camera.h"
#include "color.h"
#include "enemy_status.h"
#include "fog.h"
#include "gw_varedit.h"
#include "libc.h"
#include "light.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_cnk_vu1.h"
#include "nj_math.h"
#include "nj_matrix.h"
#include "node_draw_state.h"
#include "player.h"
#include "player_status.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "earth_cube.h"
#include "earth.h"
#include "earth_arm.h"
#include "earth_demo.h"
#include "../common/boss_common.h"
#include "../common/boss_wire.h"

/* cube tunables (debug variable editor) */
extern char earth_p_vardef[];

extern GwVarDef earth_p_vardefs[];
extern struct GwVarNode *earth_p_vars_group;
extern short cube_face_table[];
extern int cube_face_edit_idx;
extern unsigned short cube_face_edit_val;
extern signed char cube_grid[];
extern float cube_ofs_y, cube_grid_scale;
extern ModelPart *cube_models[];
extern float cube_fly_time, cube_regrow_y, cube_regrow_k, cube_regrow_rand;
extern float cube_fly_speed, cube_fly_time_rand, satr;
extern char str_div_err2[], str_part_err[];
extern float cube_swing_time, kuri_sweep_time, CMD_KURI_AT_TR;

/* Kuri attack settings per level (kuri_levels) */
typedef struct BossRate {
    float t;                    /* seconds between sweeps */
    int two;                    /* both hands */
} BossRate;

extern BossRate kuri_levels[];
extern int kuri_max;
extern float KURI_NL, KURI_NL2, kuri_pick_time;
extern float KURI_WAIT_X, KURI_WAIT_LIMX, KURI_AT_WAIT_TIME;
extern char str_cube_free[];

/* radius by distance (player_touch_curve); a negative key ends it */
typedef struct BossCurve {
    float key;
    float val;
} BossCurve;

extern BossCurve earth_mine_hit_curve[];
extern unsigned int cmPartWireColor1, cmPartWireColor2;
extern float cube_shatter_speed, cube_shatter_life, BUBBLE_LEN;
extern ModelPart cube_model_pool[10];
extern int cube_poly_flags[3];
extern float kuri_dmg, kuri_spin_min, kuri_spin_max, kuri_hit_r, KURI_AT_TIME;
extern float CMD_KURI_AT_FRM_V, CMD_KURI_BANK_Z, KURI_ROTY;
extern float satr2, KURI_TIME, KURI_TIME4, kuri_throw_k;
extern int kuri_lock_hits;
extern int CMD_FACE_EDIT_SW;
extern int CUBE_DRAW_SW;
extern float cube_scale_y, CUBE_INTEN;
extern unsigned int *cube_color_sets[];
extern unsigned int part_wire_colors[5];
extern int modeltest_compile_obj;

#ifdef VERSION_US
/* each cube is culled against parts[16]'s radius first */
#define CUBE_CULL(e) if (draw_cull_box(rad * (e)->gscl, (float *)&(e)->pos)) continue
#define CUBE_TREE_DRAW(k) cube_part_draw(cube_models[k])
#else
#define CUBE_CULL(e) (void)0
#define CUBE_TREE_DRAW(k) tree_draw_culled2(dc_object_get_compiled(cube_models[k]))
#endif

#define PLIST_HEAD(m, v) \
    do { \
        ((short *)(m)->plist)[0] = (v); \
    } while (0)

#define PLIST_FLAGS(m, v) \
    do { \
        ((short *)(m)->plist)[7] = (v) | 0x4000; \
    } while (0)

/* uniform scale of the current matrix; second form loads the scale into a local first */
#define MTX_SCALE1(s) \
    do { \
        vu0_matrix_scale(ulMatrixCur, (s), (s), (s)); \
    } while (0)

#define MTX_SCALE1_COPY(s) \
    do { \
        float s_ = (s); \
        vu0_matrix_scale(ulMatrixCur, s_, s_, s_); \
    } while (0)

/* 0x00ca5548 */
void cube_face_edit_store(void)
{
    cube_face_table[cube_face_edit_idx] = cube_face_edit_val;
}

/* 0x00ca5570 */
void cube_vars_add(void)
{
    earth_p_vars_group = gw_varedit_add(&gw_varedit_root, 0, earth_p_vardef);
    gw_varedit_add_table(&gw_varedit_root, earth_p_vars_group, earth_p_vardefs);
}

/* 0x00ca55c0 */
void cube_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)earth_p_vardef);
}

/* 0x00ca55d8 */
int kuri_hit_check(BossMainWork *w)
{
    BossCube *cube = w->set.ents;
    int i, active = 0;

    for (i = 0; i < w->set.nents; i++, cube++) {
        if ((unsigned int)(cube->state - 7) <= 5)
            active = 1;
        if (!cube->locked)
            continue;
        if (!(cube->lock.flags & LOCK_HIT))
            continue;
        if (cube->lock.life <= 0) {
            lock_score(8, &cube->lock);
            boss_se_play((int)w, 15, (int)&cube->lock, (int)&cube->pos);
            if (fabsf(cube->anim_t) < FLT_EPSILON)
                kuri_shatter(w, &cube->pos, cube->rot);
            else
                kuri_shatter_both(w, &cube->pos, cube->rot);
            earth_damage(w, 1);
            lock_remove_entry(8, &cube->lock);
            cube->state = 0;
            cube->locked = 0;
            if (++w->kuri_hits >= 8) {
                w->kuri_hits = 0;
                boss_se_play((int)w, 25, (int)&cube->lock, 0);
            }
            w->kuri_count--;
        } else {
            boss_se_play((int)w, 14, (int)&cube->lock, (int)&cube->pos);
        }
    }
    return active;
}

/* 0x00ca57b8 */
int cubes_any_in_state(BossMainWork *w, int state)
{
    BossCube *cube = w->set.ents;
    int i;

    for (i = 0; i < w->set.nents; i++, cube++) {
        if (cube->state == state)
            return 1;
    }
    return 0;
}

/* 0x00ca5800 */
void cubes_build_tree(void *w, BossCubeSet *set, WireObj *obj, int *depth)
{
    BossCube *cube;

    if (*depth >= 2) {
        if (obj->model) {
            cube = &set->ents[set->nents];
            i3_set(cube->tilt, 0, 0, 0);
            i3_set(cube->hand_ang, 0, 0, 0);
            cube->state = 0;
            cube->obj = obj;
            v3_copy(&cube->pos, (Vec3 *)obj->pos);
            cube->grp = &set->grp[set->ngrp];
            v3_copy((Vec3 *)cube->rot, (Vec3 *)obj->ang);
            v3_set(&cube->vel, 0.0f, 0.0f, 0.0f);
            set->grp[set->ngrp].ent[set->nslot] = cube;
            set->grp[set->ngrp].n++;
            set->grp[set->ngrp].present++;
            set->nslot++;
            set->nents++;
        } else {
            set->ngrp++;
            set->grp[set->ngrp].n = 0;
            set->nslot = 0;
        }
    }
    if (obj->child) {
        (*depth)++;
        cubes_build_tree(w, set, obj->child, depth);
        (*depth)--;
    }
    if (obj->sibling)
        cubes_build_tree(w, set, obj->sibling, depth);
}

/* 0x00ca59d8 */
void cubes_build_tree4(void *w, BossCubeSet *set, WireObj *obj, WireObj *alt_a, WireObj *alt_b, WireObj *alt_c, int *depth)
{
    BossCube *cube;

    if (*depth >= 2) {
        if (obj->model) {
            cube = &set->ents[set->nents];
            i3_set(cube->tilt, 0, 0, 0);
            i3_set(cube->hand_ang, 0, 0, 0);
            v3_copy(&cube->pos, (Vec3 *)obj->pos);
            cube->obj = obj;
            v3_copy((Vec3 *)cube->rot, (Vec3 *)obj->ang);
            cube->state = 0;
            cube->obj2[0] = alt_a;
            v3_set(&cube->vel, 0.0f, 0.0f, 0.0f);
            cube->obj2[1] = alt_b;
            cube->obj2[2] = alt_c;
            cube->grp = &set->grp[set->ngrp];
            set->grp[set->ngrp].ent[set->nslot] = cube;
            set->grp[set->ngrp].n++;
            set->grp[set->ngrp].present++;
            set->nslot++;
            set->nents++;
        } else {
            set->ngrp++;
            set->grp[set->ngrp].n = 0;
            set->nslot = 0;
        }
    }
    if (obj->child) {
        (*depth)++;
        cubes_build_tree4(w, set, obj->child, alt_a->child, alt_b->child, alt_c->child, depth);
        (*depth)--;
    }
    if (obj->sibling)
        cubes_build_tree4(w, set, obj->sibling, alt_a->sibling, alt_b->sibling, alt_c->sibling, depth);
}

/* 0x00ca5c00 */
void tree_show_models(WireObj *obj)
{
    if (obj->model)
        obj->flags &= ~8;
    if (obj->child)
        tree_show_models(obj->child);
    if (obj->sibling)
        tree_show_models(obj->sibling);
}

/* 0x00ca5c58 */
void grid_find_cell(signed char *grid, int n, float *x, float *y)
{
    int i, j, k = 0;

    for (i = 0; i < 18; i++) {
        for (j = 0; j < 18; j++, grid++) {
            if (*grid == 1) {
                if (k == n) {
                    *x = ((float)(j - 9) + 0.5f) / 18.0f;
                    *y = ((float)(i - 9) + 0.5f) / 18.0f;
                    return;
                }
                k++;
            }
        }
    }
    *x = 0.0f;
    *y = 0.0f;
}

/* 0x00ca5d00 */
void cube_place_on_grid(BossMainWork *w, BossCube *cube, int n, int idx)
{
    float cell[2];

    if (!cube->sign)
        cube->sign = (int)(RAND_FLOAT_A() * 2.0f) * 2 - 1;
    grid_find_cell(cube_grid, idx, &cell[0], &cell[1]);
    cube->ofs.y = cube_ofs_y * (float)cube->sign;
    cube->ofs.x = cube_grid_scale * cell[0];
    cube->ofs.z = cube_grid_scale * cell[1];
    i3_set(cube->hand_ang, 0, 0, 0);
}

/* 0x00ca5dd8 */
void cubes_place_on_grid(BossMainWork *w)
{
    BossCube *cube = w->set.ents;
    int i;

    for (i = 0; i < w->set.nents; i++, cube++)
        cube_place_on_grid(w, cube, w->set.nents, i);
}

/* 0x00ca5e60 */
int cube_face_check(int v)
{
    int i;

    for (i = 0; i < 15U; i++) {
        if (cube_face_table[i] == v)
            return 2;
    }
    return 0;
}

/* 0x00ca5e98 */
void cubes_rebuild(BossMainWork *w)
{
    BossCube *cube = w->set.ents;
    WireObj *alt_a, *alt_b, *alt_c;
    ModelPart *part;
    int i;
    BossShellGrp *grp;

    tree_show_models((WireObj *)w->parts[0]->root);
    i = 0;
    w->nloose = 0;
    w->set.nents = 0;
    w->set.ngrp = 0;
    w->set.nslot = 0;
    w->set.grp[0].n = 0;
    memset(w->set.grp, 0, sizeof(w->set.grp));
    if (dc_object_get_compiled(cube_models[7])) {
        part = w->parts[0];
        alt_a = dc_object_get_compiled(cube_models[7]);
        alt_b = dc_object_get_compiled(cube_models[8]);
        alt_c = dc_object_get_compiled(cube_models[9]);
        cubes_build_tree4(w, &w->set, (WireObj *)part->root, alt_a, alt_b, alt_c, &i);
    } else {
        cubes_build_tree(w, &w->set, (WireObj *)w->parts[0]->root, &i);
    }
    cubes_place_on_grid(w);
    for (i = 0, grp = w->set.grp; i < w->set.ngrp; i++, grp++)
        grp->lock = model_get_part(w->parts[0], i);
    if (player_status_get_level() == 7) {
        for (i = 0, cube = w->set.ents; i < w->set.nents; i++, cube++) {
            cube->mode = 2;
            cube->poly = cube_face_check(i);
            cube->anim_t = 0.0f;
        }
    } else {
        i = 0;
        if (w->set.nents > 0) {
            do {
                cube->mode = 2;
                cube->poly = 0;
                cube->anim_t = 0.0f;
                cube++;
            } while (++i < w->set.nents);
        }
    }
}

/* 0x00ca60a8 */
void cube_world_pose(BossMainWork *w, BossCube *cube, float *pos, int *ang)
{
    ulMatrixPush();
    matrix_unit_set(0);
    MTX_TRANS(&w->pos);
    cube_rotate_yxz(w->ang);
    MTX_TRANS((Vec3 *)cube->obj->pos);
    cube_rotate_yxz(cube->obj->ang);
    ulMatrixGetTranslation((float *)ulMatrixCur, pos);
    i3_sum(ang, w->ang, cube->obj->ang);
    ang[0] += ANG_90;
    ulMatrixPop();
}

/* 0x00ca61a8 */
void cube_restore(BossMainWork *w, BossCube *cube)
{
    BossShellGrp *grp;

    cube->obj->flags &= ~8;
    cube_set_mode(cube, 1);
    cube->grp->present++;
    cube_light_create(w, &w->pos, w->ang, cube->obj->ang, 2);
    grp = cube->grp;
    if (!(grp->lock->flags & LOCK_LOCKABLE)) {
        grp->done = 0;
        grp->lock->life = grp->lock->life_max;
        grp->lock->flags |= LOCK_LOCKABLE;
    }
}

/* 0x00ca6250 */
int cube_regrow_time(BossCube *cube)
{
    float height = cube->obj->pos[1] + cube_regrow_y;

    return (height * (cube_regrow_k + RAND_FLOAT_A() * cube_regrow_rand)
            + cube_fly_time) * FPS;
}

/* 0x00ca62e0 */
void cube_set_mode(BossCube *cube, int mode)
{
    switch (mode) {
    case -1:
        cube->fade = 0.0f;
        cube->mode = 0;
        cube->timer = 0;
        break;
    case 2:
        cube->mode = 2;
        cube->fade = 1.0f;
        cube->timer = 0;
        break;
    case 0:
        cube->mode = 1;
        cube->timer = 60;
        cube->fade_total = 60;
        cube->fade_in = 0;
        break;
    case 1:
        cube->fade_in = 1;
        cube->mode = 1;
        cube->timer = 60;
        cube->fade_total = 60;
        break;
    }
}

/* 0x00ca6370 */
void cube_break_off(BossMainWork *w, BossCube *cube, Vec3 *vel, int flags)
{
    Vec3 pl_vec;

    cube->obj->flags |= 8;
    cube_set_mode(cube, -1);
    cube->grp->present--;
    ulMatrixPush();
    player_get_vec(&pl_vec);
    cube->stage = w->stage;
    cube_world_pose(w, cube, &cube->pos.x, cube->rot);
    v3_sub(&cube->pos, &cube->pos, &pl_vec);
    v3_copy(&cube->prev, &cube->pos);
    i3_copy(cube->turn, w->turn);
    if (!(flags & 2))
        cube_light_create(w, &w->pos, w->ang, cube->obj->ang, 2);
    if (vel) {
        v3_copy(&cube->vel, vel);
    } else {
        v3_sub(&cube->vel, &cube->pos, &w->pos);
        vec3_normalize(&cube->vel, &cube->vel);
        v3_scale(&cube->vel, cube_fly_speed);
    }
    ulMatrixPop();
    cube->gscl = satr;
    cube->spin[0] = DEG_TO_ANG(RAND_FLOAT_A() * 120.0f / 60.0f * 2.0f - 2.0f);
    cube->spin[1] = DEG_TO_ANG(RAND_FLOAT_A() * 120.0f / 60.0f * 2.0f - 2.0f);
    cube->spin[2] = DEG_TO_ANG(RAND_FLOAT_A() * 120.0f / 60.0f * 2.0f - 2.0f);
    if (flags <= 0 && (RAND_FLOAT_A() < 0.66f || w->nloose >= 100)) {
        cube->state = 3;
        cube->frames = (cube_fly_time + (RAND_FLOAT_A() * (cube_fly_time_rand + cube_fly_time_rand) - cube_fly_time_rand)) * FPS;
        cube->pull = 0;
        if (arm_attach_cube(w->arm, &cube->node_pos, &cube->node_ang, &cube->node_scale, &cube->color, cube, &cube->node_flags))
            return;
    }
    cube->state = 1;
    cube->pull = 0;
    cube->frames = cube_fly_time * FPS;
    w->nloose++;
}

/* 0x00ca6788 */
void cubes_break_all(BossMainWork *w, int mode)
{
    BossShellGrp *grp = w->set.grp;
    BossCube *cube;
    LockObj *lock;
    Vec3 pos;
    int ang[3];
    int i, j, flag = 0;

    for (i = 0; i < w->set.ngrp; i++, grp++) {
        switch (mode) {
        case 2:
            if (grp->done)
                break;
            lock = model_get_part(w->parts[0], i);
            lock->flags &= ~LOCK_LOCKABLE;
            grp->done = -1;
            for (j = 0; j < grp->n; j++) {
                if (grp->ent[j]) {
                    cube_break_off(w, grp->ent[j], 0, 1);
                } else {
                    sprintf(w->msg, str_div_err2);
                    w->nerr++;
                }
            }
            break;
        case 1:
            if (grp->done)
                break;
            lock = model_get_part(w->parts[0], i);
            lock->flags &= ~LOCK_LOCKABLE;
            grp->done = -1;
            for (j = 0; j < grp->n; j++) {
                if (grp->ent[j]) {
                    cube_break_off(w, grp->ent[j], 0, 0);
                } else {
                    sprintf(w->msg, str_div_err2);
                    w->nerr++;
                }
            }
            break;
        case 0:
            flag = 1;
        default:
            if (!grp->done) {
                grp->done = -1;
                lock = model_get_part(w->parts[0], i);
                lock->flags &= ~LOCK_LOCKABLE;
                if (flag) {
                    for (j = 0; j < grp->n; j++) {
                        matrix_unit_set(0);
                        MTX_TRANS(&w->pos);
                        cube_rotate_yxz(w->ang);
                        MTX_TRANS((Vec3 *)grp->ent[j]->obj->pos);
                        cube_rotate_yxz(grp->ent[j]->obj->ang);
                        ulMatrixGetTranslation((float *)ulMatrixCur, &pos.x);
                        i3_sum(ang, w->ang, grp->ent[j]->obj->ang);
                        ang[0] += ANG_90;
                        ulMatrixPop();
                        grp->ent[j]->obj->flags |= 8;
                        grp->ent[j]->state = 0;
                    }
                }
            }
            for (j = 0; j < grp->n; j++) {
                cube = grp->ent[j];
                if (cube) {
                    if (!cube->state)
                        continue;
                    if (cube->locked) {
                        cube->locked = 0;
                        cube->lock.flags &= ~LOCK_LOCKABLE;
                    }
                    cube->obj->flags |= 8;
                    cube->state = 0;
                } else {
                    sprintf(w->msg, str_part_err);
                    w->nerr++;
                }
            }
            break;
        }
    }
}

/* 0x00ca6c18 */
void cubes_swing(BossMainWork *w)
{
    BossCube *cube = w->set.ents;
    float sc[2];
    float amp;
    int i, sgn;

    if (!w->swings)
        return;
    if (!(w->swing_side & 1)) {
        sgn = 1;
        amp = -cube_ofs_y;
    } else {
        sgn = -1;
        amp = cube_ofs_y;
    }
    if (w->swing_side & 2)
        amp = -amp;
    w->swing_count -= 1.0f;
    if (w->swing_count <= 0.0f) {
        w->swings--;
        w->swing_side ^= 1;
        if (!(w->swing_side & 1))
            w->swing_side ^= 2;
    }
    for (i = 0; i < w->set.nents; i++, cube++) {
        if (cube->state == 2 && sgn == cube->sign) {
            ulSinCosf(w->swing_count * PI / (cube_swing_time * FPS), &sc[0], &sc[1]);
            cube->ofs.y = amp * sc[1];
        }
    }
    if (w->swing_count <= 0.0f && w->swings)
        w->swing_count = cube_swing_time * FPS;
}

/* 0x00ca6dc8 */
int cube_damp(BossCube *cube, int *ang, float k)
{
    cube->spin[0] *= 0.98f;
    cube->spin[1] *= 0.98f;
    cube->spin[2] *= 0.98f;
    cube->rot[0] = (unsigned short)cube->rot[0];
    cube->rot[1] = (unsigned short)cube->rot[1];
    cube->rot[2] = (unsigned short)cube->rot[2];
    if (ang) {
        cube->rot[0] = cube->rot[0] * 0.98f + ang[0] * 0.02f;
        cube->rot[1] = cube->rot[1] * 0.98f + ang[1] * 0.02f;
        cube->rot[2] = cube->rot[2] * 0.98f + ang[2] * 0.02f;
    } else {
        cube->rot[0] *= 0.98f;
        cube->rot[1] *= 0.98f;
        cube->rot[2] *= 0.98f;
    }
    cube->gscl = cube->gscl * 0.98f + k * 0.02f;
    return 1;
}

/* 0x00ca6f20 */
int kuri_attack_step(BossMainWork *w)
{
    BossCube *cube;
    float k;
    int i, active;

    switch (w->kuri_state) {
    case 0:
        w->kuri_count = 0;
        kuri_hands_step(w, 0);
        kuri_hands_start(w, 1);
        w->kuri_timer = kuri_sweep_time * FPS;
        w->kuri_state = 1;
        w->kuri_count_vol = w->kuri_count;
        boss_se_play((int)w, 17, 0, 0);
        boss_vol_play(w->bgm, 14, (int)&w->kuri_count_vol);
        return 0;
    case 1:
        kuri_hands_step(w, 1);
        kuri_hit_check(w);
        if (--w->kuri_timer > 0)
            break;
        k = 10.0f - (float)w->kuri_round;
        if (k < 1.0f)
            k = CMD_KURI_AT_TR;
        else
            k *= CMD_KURI_AT_TR;
        w->kuri_timer = kuri_levels[w->level].t * k * FPS;
        w->kuri_state = 2;
        boss_se_play((int)w, 18, 0, 0);
        return 0;
    case 2:
        kuri_hands_step(w, 1);
        if (!kuri_hit_check(w)) {
            w->kuri_state = 3;
            break;
        }
        if (--w->kuri_timer > 0)
            break;
        w->kuri_timer = kuri_levels[w->level].t * FPS;
        kuri_release_one(w);
        return 0;
    case 3:
        cube = w->set.ents;
        active = 0;
        for (i = 0; i < w->set.nents; i++, cube++) {
            if (cube->state) {
                active = 1;
                break;
            }
        }
        if (!active) {
            w->kuri_state = 0;
            return 1;
        }
        w->swing_count = 60.0f;
        w->kuri_state = 4;
        w->swings = 2;
        break;
    case 4:
        w->kuri_timer = 1;
        w->kuri_round++;
        w->kuri_state = 5;
        w->kuri_count = 0;
        break;
    case 5:
        if (--w->kuri_timer > 0)
            break;
        w->kuri_state = 0;
        break;
    }
    return 0;
}

/* 0x00ca7220 */
void kuri_hand_arm(BossMainWork *w, int hand, int on)
{
    BossCube *cube = w->set.ents;
    float sc[2];
    int i, pick, n, k, step;

    if (!on) {
        for (i = 0; i < w->set.nents; i++, cube++) {
            if (cube->locked) {
                cube_place_on_grid(w, cube, w->set.nents, i);
                cube->frames = cube_fly_time * FPS;
                cube->pull = 0.0f;
                cube->state = 1;
                cube->locked = 0;
                {
                    Vec3 *prev = &cube->prev;

                    prev->x = cube->pos.x;
                    prev->y = cube->pos.y;
                    prev->z = cube->pos.z;
                }
                lock_remove_entry(8, &cube->lock);
            }
        }
        return;
    }
    n = 0;
    for (i = 0; i < w->set.nents; i++, cube++) {
        if ((unsigned int)(cube->state - 1) <= 1)
            n++;
    }
    if (n > kuri_max)
        n = kuri_max;
    if (n)
        step = 0x10000 / n;
    else
        step = 0;
    k = 0;
    for (i = 0; i < kuri_max; i++) {
        pick = RAND_FLOAT_A() * (float)w->set.nents;
        for (n = 0; n < w->set.nents; n++, pick++) {
            cube = &w->set.ents[pick % w->set.nents];
            if ((unsigned int)(cube->state - 1) < 2) {
                v3_set(&cube->vel, 0.0f, 0.0f, 0.0f);
                cube->hand_ang[0] = k * step;
                cube->state = 7;
                v3_copy(&cube->ofs2, &cube->ofs);
                cube->hand = hand;
                cube->hand_ang[1] = 0;
                cube->hand_ang[2] = 0;
                cube->ofs.y = hand ? KURI_NL2 : KURI_NL;
                cube->ofs.x = 0.0f;
                cube->ofs.z = 0.0f;
                ROT_X(cube->hand_ang[0], sc, cube->ofs.y, cube->ofs.z);
                k++;
                cube->frames = kuri_pick_time * FPS;
                cube->pull = (float)k * (float)cube->frames * 0.8f / (float)kuri_max;
                w->kuri_count++;
                break;
            }
        }
        if (n == w->set.nents)
            break;
    }
    w->hand_speed_max = 546;
}

/* 0x00ca7608 */
void kuri_hands_start(BossMainWork *w, int on)
{
    kuri_hand_arm(w, 0, on);
    i3_set(w->hand[0].ang, 0, 0, 0);
    i3_set(w->hand[0].b, 0, 0, 0);
    w->hand[0].c[0] = 1;
    if (kuri_levels[w->level].two) {
        kuri_hand_arm(w, 1, on);
        i3_set(w->hand[1].ang, 0, 0, 0);
        i3_set(w->hand[1].b, 0, 0, 0);
        w->hand[1].c[0] = 1;
    }
}

/* 0x00ca76f8 */
BossCube *kuri_nearest_by_height(BossMainWork *w, BossCube **list, int n)
{
    BossCube *best = list[0];
    float best_dist = fabsf(best->pos.y - w->pos.y);
    float dist;

    for (; n; n--) {
        dist = fabsf(list[n - 1]->pos.y - w->pos.y);
        if (dist < best_dist) {
            best = list[n - 1];
            best_dist = dist;
        }
    }
    return best;
}

/* 0x00ca7758 */
int kuri_release_one(BossMainWork *w)
{
    BossCube *list[64];
    BossCube *cube = w->set.ents;
    int i, n = 0, busy;

    for (i = 0, busy = 0; i < w->set.nents; i++, cube++) {
        if (cube->state == 10) {
            list[n++] = cube;
            if (n >= kuri_max)
                break;
        } else if (cube->locked) {
            busy++;
        }
    }
    if (!n)
        return busy ? 0 : -1;
    cube = kuri_nearest_by_height(w, list, n);
    if (cube->state != 10)
        return 0;
    boss_se_play((int)w, 16, (int)&cube->lock, (int)&cube->pos);
    cube->state = 11;
    cube->frames = KURI_AT_WAIT_TIME * FPS;
    if (KURI_WAIT_LIMX < cube->pos.x)
        cube->drift_x = KURI_WAIT_X / (float)cube->frames;
    else if (cube->pos.x < -KURI_WAIT_LIMX)
        cube->drift_x = -KURI_WAIT_X / (float)cube->frames;
    else if ((int)(RAND_FLOAT_A() * 2.0f))
        cube->drift_x = KURI_WAIT_X / (float)cube->frames;
    else
        cube->drift_x = -KURI_WAIT_X / (float)cube->frames;
    cube->anim_end = w->parts[20]->frames;
    cube->anim_t = 0.0f;
    cube->spin_y = 0.0f;
    cube->tilt[0] = DEG_TO_ANG((RAND_FLOAT_A() * 2.0f) * 15.0f - 15.0f);
    if (cube->tilt[0] < 0)
        cube->tilt[0] -= 910;
    else
        cube->tilt[0] += 910;
    cube->tilt[1] = DEG_TO_ANG((RAND_FLOAT_A() * 2.0f) * 15.0f - 15.0f);
    if (cube->tilt[1] < 0)
        cube->tilt[1] -= 910;
    else
        cube->tilt[1] += 910;
    return 1;
}

/* 0x00ca7a40 */
void kuri_hand_step(BossMainWork *w, BossKuriHand *hand, int on)
{
    int ang[3], vel[3], acc[3], timer[3];

    i3_copy(ang, hand->ang);
    i3_copy(vel, hand->b);
    i3_copy(acc, hand->c);
    i3_copy(timer, hand->d);
    if (!on) {
        acc[0] = 0;
        vel[0] = 0;
    }
    vel[0] += acc[0];
    if (vel[0] > w->hand_speed_max || vel[0] < -w->hand_speed_max)
        vel[0] = w->hand_speed_max;
    ang[0] += vel[0];
    while (ang[0] > 0x10000)
        ang[0] -= 0x10000;
    while (ang[0] < -0x10000)
        ang[0] += 0x10000;
    ang[1] = vel[1] * njSin(timer[1] * PI / acc[1] * ANG_PER_RAD);
    if (--timer[1] <= 0) {
        vel[1] = DEG_TO_ANG((RAND_FLOAT_A() * 2.0f - 1.0f) * 30.0f);
        timer[1] = (RAND_FLOAT_A() * 2.0f - 1.0f) * 480.0f;
        if (vel[1] >= 0)
            vel[1] += 1820;
        else
            vel[1] -= 1820;
        if (timer[1] < 0)
            timer[1] = -timer[1];
        acc[1] = timer[1] = timer[1] + 300.0f;
    }
    ang[2] = vel[2] * njSin(timer[2] * PI / acc[2] * ANG_PER_RAD);
    if (--timer[2] <= 0) {
        vel[2] = DEG_TO_ANG((RAND_FLOAT_A() * 2.0f - 1.0f) * 30.0f);
        timer[2] = (RAND_FLOAT_A() * 2.0f - 1.0f) * 480.0f;
        if (vel[2] >= 0)
            vel[2] += 1820;
        else
            vel[2] -= 1820;
        if (timer[2] < 0)
            timer[2] = -timer[2];
        acc[2] = timer[2] = timer[2] + 300.0f;
    }
    i3_copy(hand->ang, ang);
    i3_copy(hand->b, vel);
    i3_copy(hand->c, acc);
    i3_copy(hand->d, timer);
}

/* 0x00ca7ec8 */
void kuri_hands_step(BossMainWork *w, int on)
{
    int n;

    for (n = enemy_status_get_ticks(); n; n--) {
        kuri_hand_step(w, &w->hand[0], on);
        if (kuri_levels[w->level].two)
            kuri_hand_step(w, &w->hand[1], on);
    }
}

/* 0x00ca7f70 */
void cube_update(BossMainWork *w, BossCube *cube, float *mtx, Vec3 *vel, int on)
{
    int ang[3];
    Vec3 v, ofs;
    float sc[4];
    BossKuriHand *hand;
    float ua, ub, ang_rad, t, spin_range;
    int k;

    if (!cube->hand)
        hand = &w->hand[0];
    else
        hand = &w->hand[1];
    switch (cube->state) {
    case 10:
        ulMatrixApplyPoint(mtx, (float *)&cube->ofs, (float *)&v);
        ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&v, (float *)&cube->pos);
        if (!on)
            break;
        ang[0] = hand->ang[0] + cube->hand_ang[0];
        ang[1] = 0;
        if (!cube->hand)
            ang[2] = hand->ang[2];
        else
            ang[2] = hand->ang[2] + ANG_90;
        cube->gscl = cube->gscl * 0.98f + satr * 0.02f;
        i3_copy(cube->rot, ang);
        break;
    case 11:
        v3_sum(&cube->pos, vel, &cube->pos);
        if (!on)
            break;
        cube->pos.x += cube->drift_x;
        cube->spin_y -= 1.0f;
        if (cube->spin_y < KURI_ROTY)
            cube->spin_y = KURI_ROTY;
        cube->rot[0] = cube->rot[0] * 0.9f;
        cube->rot[1] += (int)DEG_TO_ANG(cube->spin_y);
        cube->rot[2] = cube->rot[2] * 0.9f;
        cube->frames--;
        if (cube->frames <= (int)(cube->anim_end / CMD_KURI_AT_FRM_V) + 2) {
            cube->anim_t += CMD_KURI_AT_FRM_V;
            if (cube->anim_end <= cube->anim_t)
                cube->anim_t = cube->anim_end;
        }
        if (cube->frames > 0)
            break;
        v3_copy(&cube->prev, &cube->pos);
        cube->frames = KURI_AT_TIME * FPS;
        player_get_pos(&v);
        cube->state = 12;
        v3_sub(&cube->ofs2, &v, &cube->pos);
        spin_range = kuri_spin_max - kuri_spin_min;
        cube->spin_y = spin_range * 2.0f * RAND_FLOAT_A() - spin_range;
        if (cube->spin_y < 0.0f)
            cube->spin_y -= kuri_spin_min;
        else
            cube->spin_y += kuri_spin_min;
        cube->drift_x = cube->spin_y = 0.0f;
        cube->tilt[2] = DEG_TO_ANG(RAND_FLOAT_A() * 180.0f);
        break;
    case 12:
        cube->anim_t += CMD_KURI_AT_FRM_V;
        if (cube->anim_end <= cube->anim_t)
            cube->anim_t = cube->anim_end;
        v3_add(&cube->prev, vel);
        t = 1.0f - cube->frames / (KURI_AT_TIME * FPS);
        ang_rad = t * PI;
        k = ang_rad * ANG_PER_RAD;
        v3_mul(&ofs, &cube->ofs2, njSin(k));
        v3_sum(&cube->pos, &cube->prev, &ofs);
        v3_copy(&ofs, (Vec3 *)&cube->drift_x);
        do {
            float ua = ofs.x, ub = ofs.y;
            ulSinCosf(ang_rad, &sc[0], &sc[1]);
            ulSinCos((unsigned int)(cube->tilt[2] * sc[0]), &sc[2], &sc[3]);
            ofs.x = ua * sc[3] - ub * sc[2];
            ofs.y = ua * sc[2] + ub * sc[3];
        } while (0);
        v3_add(&cube->pos, &ofs);
        if (!on)
            break;
        cube->rot[2] = DEG_TO_ANG(CMD_KURI_BANK_Z * njSin(k));
        cube->rot[0] = DEG_TO_ANG(njSin(t * 1.5707964f * ANG_PER_RAD) * 180.0f);
        cube->rot[1] += (int)DEG_TO_ANG(KURI_ROTY);
        if (player_sphere_test(&cube->pos, kuri_hit_r) & 1) {
            player_status_add_damage(kuri_dmg);
            kuri_shatter_both(w, &cube->pos, cube->rot);
            lock_remove_entry(8, &cube->lock);
            earth_damage(w, 1);
            cube->state = 0;
            cube->locked = 0;
        } else if (--cube->frames <= KURI_AT_TIME * 0.5f * FPS) {
            player_status_add_damage(kuri_dmg);
            kuri_shatter_both(w, &cube->pos, cube->rot);
            lock_remove_entry(8, &cube->lock);
            earth_damage(w, 1);
            cube->state = 0;
            cube->locked = 0;
        }
        break;
    }
}

/* 0x00ca8650 */
void cubes_update(void *work)
{
    BossMainWork *w = work;
    int v[3];
    int ang[3];
    float mtx[2][16];
    Vec3 pl, prev_pos, pl_vec, aim_pos, home_pos;
    int home_ang[3];
    float sc[4];
    BossCube *cube;
    int n, i, on;
    int x, y;

    player_get_pos(&pl);
    ulMatrixPush();
    matrix_unit_set(0);
    MTX_TRANS(&w->pos);
    ang[1] = w->roty;
    ang[2] = 0;
    ang[0] = 0;
    cube_rotate_yxz(ang);
    matrix_unit_set((int)mtx[0]);
    vu0_matrix_rotate_y(mtx[0], -w->roty);
    vu0_matrix_rotate_z(mtx[0], w->hand[0].ang[2]);
    vu0_matrix_rotate_y(mtx[0], w->hand[0].ang[1]);
    vu0_matrix_rotate_x(mtx[0], w->hand[0].ang[0]);
    if (kuri_levels[w->level].two) {
        matrix_unit_set((int)mtx[1]);
        vu0_matrix_rotate_y(mtx[1], -w->roty);
        vu0_matrix_rotate_z(mtx[1], w->hand[1].ang[2] + ANG_90);
        vu0_matrix_rotate_y(mtx[1], w->hand[1].ang[1]);
        vu0_matrix_rotate_x(mtx[1], w->hand[1].ang[0]);
    }
    {
    BossCube *ents;

    ents = w->set.ents;
    cubes_swing(w);
    n = enemy_status_get_ticks();
    on = n != 0;
    do {
        if (n == 1) {
            player_get_vec(&pl_vec);
        } else {
            pl_vec.x = 0.0f;
            pl_vec.y = 0.0f;
            pl_vec.z = 0.0f;
        }
        cube = ents;
        for (i = 0; i < w->set.nents; i++, cube++) {
            if (cube->timer) {
                cube->timer--;
                if (cube->state)
                    cube->fade = 1.0f - (float)cube->timer / (float)cube->fade_total;
                else
                    cube->fade = (float)cube->timer / (float)cube->fade_total;
                if (!cube->timer) {
                    if (cube->fade_in == 1)
                        cube->mode = 2;
                    else
                        cube->mode = 0;
                }
            }
            switch (cube->state) {
            case 13:
                {
                    Vec3 *src = &cube->pos, *dst = &cube->prev;

                    dst->x = src->x;
                    dst->y = src->y;
                    dst->z = src->z;
                }
                v3_sub(&cube->vel, &cube->pos, &w->pos);
                vec3_normalize(&cube->vel, &cube->vel);
                v3_scale(&cube->vel, cube_fly_speed);
                cube->state = 14;
                v3_copy((Vec3 *)cube->turn, (Vec3 *)w->turn);
                cube->frames = cube_regrow_time(cube);
                cube->pull = 0.0f;
                cube->poly = w->stage;
            case 14:
                v3_add(&cube->pos, &pl_vec);
                v3_add(&cube->prev, &pl_vec);
                if (!on)
                    break;
                cube_world_pose(w, cube, (float *)&home_pos, home_ang);
                cube->gscl = satr;
                /* rot += spin */
                x = cube->rot[0];
                y = cube->spin[0];
                {
                    int *dst = cube->rot, *src = cube->spin;

                    x += y;
                    dst[0] = x;
                    dst[1] = dst[1] + src[1];
                    dst[2] = dst[2] + src[2];
                }
                ROT_Y(cube->turn[1], (sc + 0), cube->vel.x, cube->vel.z);
                if (homing_step(&cube->prev, &home_pos, &cube->pos, &cube->vel, &cube->pull, &cube->frames)) {
                    if (earth_is_defeated(w)) {
                        cube_break_off(w, cube, 0, 2);
                    } else {
                        cube_restore(w, cube);
                        cube->state = 0;
                    }
                }
                break;
            case 1:
                v3_add(&cube->pos, &pl_vec);
                v3_add(&cube->prev, &pl_vec);
                if (!on)
                    break;
                {
                    float *cur_mtx = (float *)ulMatrixCur;

                    i3_sum(cube->rot, cube->rot, cube->spin);
                    ulMatrixApplyPoint(cur_mtx, (float *)&cube->ofs, (float *)&aim_pos);
                }
                if (homing_step(&cube->prev, &aim_pos, &cube->pos, &cube->vel, &cube->pull, &cube->frames))
                    cube->state = 2;
                break;
            case 2:
                ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&cube->ofs, (float *)&cube->pos);
                if (!on)
                    break;
                do { cube->gscl = cube->gscl * 0.98f + satr2 * 0.02f;
                i3_sum(cube->rot, cube->spin, cube->rot);
                i3_copy(cube->rot, ang); } while (0);
                break;
            case 3:
                v3_add(&cube->pos, &pl_vec);
                v3_add(&cube->prev, &pl_vec);
                if (!on)
                    break;
                cube->gscl = satr;
                /* rot += spin */
                x = cube->rot[0];
                y = cube->spin[0];
                {
                    int *dst = cube->rot, *src = cube->spin;

                    x += y;
                    dst[0] = x;
                    dst[1] = dst[1] + src[1];
                    dst[2] = dst[2] + src[2];
                }
                ROT_Y(cube->turn[1], (sc + 2), cube->vel.x, cube->vel.z);
                if (homing_step(&cube->prev, (Vec3 *)cube->node_pos, &cube->pos, &cube->vel, &cube->pull, &cube->frames))
                    cube->state = 4;
                break;
            case 4:
            case 5:
                v3_copy(&cube->pos, (Vec3 *)cube->node_pos);
                if (!on)
                    break;
                cube_damp(cube, (int *)cube->node_ang, satr);
                i3_sum(cube->rot, cube->spin, cube->rot);
                break;
            case 6:
                v3_add(&cube->pos, &pl_vec);
                v3_copy(&cube->prev, &cube->pos);
                v3_set(&cube->vel, 0.0f, 0.0f, 0.0f);
                if (!on)
                    break;
                cube->state = 1;
                cube->pull = 0.0f;
                cube->frames = cube_fly_time * FPS;
                break;
            case 7:
                ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&cube->ofs2, (float *)&cube->pos);
                if (!on)
                    break;
                i3_copy(cube->rot, ang);
                cube->pull -= 1.0f;
                cube->frames--;
                if (cube->pull != 0.0f)
                    break;
                cube->state = 8;
            case 8:
                v3_copy(&prev_pos, &cube->pos);
                ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&cube->ofs2, (float *)&cube->pos);
                if (!on)
                    break;
                if (--cube->frames)
                    break;
                v3_add(&prev_pos, &pl_vec);
                v3_sub(&cube->vel, &cube->pos, &prev_pos);
                v3_scale(&cube->vel, kuri_throw_k);
                i3_set(cube->spin, 0, 0, 0);
                v3_copy(&cube->prev, &cube->pos);
                i3_copy(cube->rot, ang);
                if (w->arm_count)
                    cube->frames = KURI_TIME * FPS;
                else
                    cube->frames = KURI_TIME4 * FPS;
                cube->pull = 0.0f;
                cube->state = 9;
                break;
            case 9:
                v3_add(&cube->pos, &pl_vec);
                v3_add(&cube->prev, &pl_vec);
                if (!on)
                    break;
                ulMatrixApplyPoint(mtx[cube->hand], (float *)&cube->ofs, (float *)&aim_pos);
                ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&aim_pos, (float *)&aim_pos);
                if (homing_step(&cube->prev, &aim_pos, &cube->pos, &cube->vel, &cube->pull, &cube->frames)) {
                    if (!cube->locked) {
                        lock_init(8, &cube->lock, w, &cube->pos, 0, kuri_lock_hits, 128);
                        lockon_entry(&cube->lock);
                        cube->locked = 1;
                    }
                    cube->state = 10;
                }
                if (!cube->hand) {
                    v[0] = w->hand[0].ang[0] + cube->hand_ang[0];
                    v[2] = w->hand[0].ang[2];
                } else {
                    v[0] = w->hand[1].ang[0] + cube->hand_ang[0];
                    v[2] = w->hand[1].ang[2] + ANG_90;
                }
                v[1] = 0;
                cube_damp(cube, v, satr);
                break;
            case 10:
            case 11:
            case 12:
                cube_update(w, cube, mtx[cube->hand], &pl_vec, on);
                break;
            }
        }
    } while (--n > 0);
    ulMatrixPop();
    }
}

/* 0x00ca9298 */
void cubes_lock_remove(BossMainWork *w)
{
    BossCube *cube = w->set.ents;
    int i;

    for (i = 0; i < w->set.nents; i++, cube++) {
        if (cube->locked) {
            lock_remove_entry(8, &cube->lock);
            cube->locked = 0;
        }
    }
}

/* 0x00ca9318 */
void cubes_scatter(BossMainWork *w)
{
    BossCube *cube = w->set.ents;
    int i;

    for (i = 0; i < w->set.nents; i++, cube++) {
        if (cube->state)
            cube->state = 13;
    }
    w->nloose = 0;
    w->shell_hits = 0;
}

/* 0x00ca9370 */
void cube_models_free(BossMainWork *w)
{
    int i;

    for (i = 0; i < 16; i++) {
        if (cube_models[i]) {
            dc_object_release(cube_models[i]);
            DEBUG_PRINTF(str_cube_free, cube_models[i]);
        }
    }
}

/* 0x00ca93e0 */
void tree_set_poly_flags(WireObj *obj, int flags)
{
    do {
        if (obj->model) {
            do {
                (obj->model->plist)[7] = flags | 0x4000;
            } while (0);
        }
        if (obj->child)
            tree_set_poly_flags(obj->child, flags);
    } while ((obj = obj->sibling));
}

/* 0x00ca9450 */
void cube_models_build(BossMainWork *w)
{
    int i;

    for (i = 15; i >= 0; i--)
        cube_models[i] = 0;
    i = 0;
    do {
        (((WireObj *)w->parts[16]->root)->model->plist)[0] = 8467;
    } while (0);
    do {
        (((WireObj *)w->parts[16]->root)->model->plist)[7] = 6 | 0x4000;
    } while (0);
    cube_models[i] = dc_object_init(&cube_model_pool[i], (struct DcNode *)w->parts[16]->root, 0, 0);
    dc_object_rebuild(cube_models[i]);
    i++;
    do {
        (((WireObj *)w->parts[16]->root)->model->plist)[0] = 2067;
    } while (0);
    do {
        (((WireObj *)w->parts[16]->root)->model->plist)[7] = 7 | 0x4000;
    } while (0);
    cube_models[i] = dc_object_init(&cube_model_pool[i], (struct DcNode *)w->parts[16]->root, 0, 0);
    dc_object_rebuild(cube_models[i]);
    i++;
    do {
        (((WireObj *)w->parts[16]->root)->model->plist)[0] = 8467;
    } while (0);
    do {
        (((WireObj *)w->parts[16]->root)->model->plist)[7] = 8 | 0x4000;
    } while (0);
    cube_models[i] = dc_object_init(&cube_model_pool[i], (struct DcNode *)w->parts[16]->root, 0, 0);
    dc_object_rebuild(cube_models[i]);
    i++;
    {
        (((WireObj *)w->parts[16]->root)->model->plist)[0] = 8467;
    }
    do {
        (((WireObj *)w->parts[16]->root)->model->plist)[7] = 7 | 0x4000;
    } while (0);
    cube_models[i] = dc_object_init(&cube_model_pool[i], (struct DcNode *)w->parts[16]->root, 0, 0);
    dc_object_rebuild(cube_models[i]);
    i++;
    do {
        (((WireObj *)w->parts[16]->root)->model->plist)[0] = 8467;
    } while (0);
    do {
        (((WireObj *)w->parts[16]->root)->model->plist)[7] = 9 | 0x4000;
    } while (0);
    cube_models[i] = dc_object_init(&cube_model_pool[i], (struct DcNode *)w->parts[16]->root, 0, 0);
    dc_object_rebuild(cube_models[i]);
    i++;
    do {
        (((WireObj *)w->parts[16]->root)->model->plist)[0] = 8467;
    } while (0);
    do {
        (((WireObj *)w->parts[16]->root)->model->plist)[7] = 6 | 0x4000;
    } while (0);
    cube_models[i] = dc_object_init(&cube_model_pool[i], (struct DcNode *)w->parts[16]->root, 0, 0);
    dc_object_rebuild(cube_models[i]);
    i++;
    cube_models[i] = dc_object_init(&cube_model_pool[i], (struct DcNode *)w->parts[17]->root, 0, 0);
    dc_object_rebuild(cube_models[i]);
    i++;
    tree_set_poly_flags((WireObj *)w->parts[0]->root, cube_poly_flags[i - 7]);
    cube_models[i] = dc_object_init(&cube_model_pool[i], (struct DcNode *)w->parts[0]->root, 0, 0);
    dc_object_rebuild(cube_models[i]);
    i++;
    tree_set_poly_flags((WireObj *)w->parts[0]->root, cube_poly_flags[i - 7]);
    cube_models[i] = dc_object_init(&cube_model_pool[i], (struct DcNode *)w->parts[0]->root, 0, 0);
    dc_object_rebuild(cube_models[i]);
    i++;
    tree_set_poly_flags((WireObj *)w->parts[0]->root, cube_poly_flags[i - 7]);
    cube_models[i] = dc_object_init(&cube_model_pool[i], (struct DcNode *)w->parts[0]->root, 0, 0);
    dc_object_rebuild(cube_models[i]);
    i++;
}

#ifdef VERSION_US
/* Draws a node tree without per-node matrices, culling each model by its bounds. */
/* 0x00ca96f8 */
void tree_draw_flat(WireObj *obj)
{
    unsigned int fl;
    unsigned int vis;

    do {
        fl = obj->flags;
        if (!(fl & 8)) {
            vis = draw_cull_box(obj->model->r, &obj->model->center.x);
            if (!(vis & 0xff)) {
                vis &= 0xff00;
                if (vis)
                    nj_cnk_vu1pkt_draw_clip(obj->model);
                else
                    nj_cnk_vu1pkt_draw_noclip(obj->model);
            }
        }
        if (!(fl & 0x10))
            tree_draw_flat(obj->child);
        obj = obj->sibling;
    } while (obj);
}

/* 0x00ca9790 */
void cube_part_draw(ModelPart *part)
{
    WireObj *obj = dc_object_get_compiled(part);

    if (obj)
        tree_draw_flat(obj);
    else
        boss_dc_object_draw(part);
}
#else
/* 0x00ca96f8 */
void tree_draw_flat(WireObj *obj)
{
    unsigned int fl;
    float *p;

    do {
        fl = obj->flags;
        ulMatrixPush();
        p = obj->pos;
        vu0_matrix_translate(ulMatrixCur, p[0], p[1], p[2]);
        nj_matrix_rotate_cur_ang3(obj->ang, fl & 0x20);
        vu0_matrix_scale(ulMatrixCur, obj->scl[0], obj->scl[1], obj->scl[2]);
        if (!(fl & 8))
            nj_cnk_vu1pkt_draw_noclip(obj->model);
        if (!(fl & 0x10))
            tree_draw_flat(obj->child);
        ulMatrixPop();
        obj = obj->sibling;
    } while (obj);
}
#endif

/* 0x00ca97d0 */
void color_unpack(unsigned int col, float *out)
{
    out[0] = (float)color_get_a(col) / 255.0f;
    out[1] = (float)(col & 0xff) / 255.0f;
    out[2] = (float)(col >> 8 & 0xff) / 255.0f;
    out[3] = (float)(col >> 16 & 0xff) / 255.0f;
}

/* steer(cast) */
/* 0x00ca98f8 */
void cubes_disp(void *work)
{
    BossMainWork *w = work;
    float rgba[9][4];
    float diff, ambi;
    int old_flags, cube_flags;
    BossCube *ents, *cube;
    unsigned int *col;
    NjCnkModel *model;
    WireObj *obj;
    unsigned int cull;
    int i, v, rsc, old_fog;
#ifdef VERSION_US
    float rad = w->parts[16]->radius;
#endif

    ulGxSetTextureList(w->file1);
    if (debug_flags_test(4) && CMD_FACE_EDIT_SW) {
        cube = w->set.ents;
        for (i = 0; i < w->set.nents; i++, cube++)
            cube->poly = cube_face_check(i);
    }
    ents = w->set.ents;
    old_flags = draw_set_override_flags(0x20810);
    cube_flags = draw_set_override_flags(0);
    draw_push_strip_flags();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 22528);
    ulMatrixPush();
    rgba[5][1] = rgba[5][2] = rgba[5][3] = 1.0f;
    cube = ents;
    MTX_TRANS(&w->pos);
    cube_rotate_yxz(w->ang);
    for (i = 0; i < w->set.nents; i++, cube++) {
        switch (cube->mode) {
        case 1:
            draw_set_override_flags(0);
            ulMatrixPush();
            MTX_TRANS((Vec3 *)cube->obj->pos);
            vu0_matrix_rotate_y(ulMatrixCur, cube->obj->ang[1]);
            vu0_matrix_rotate_x(ulMatrixCur, cube->obj->ang[0]);
            if (cube->obj2[cube->poly % 4]) {
                model = cube->obj2[cube->poly % 4]->model;
                cull = draw_cull_box(model->r, &model->center.x);
                if (!(cull & 0xff)) {
                    cull &= 0xff00;
                    if (cull)
                        nj_cnk_vu1pkt_draw_clip(model);
                    else
                        nj_cnk_vu1pkt_draw_noclip(model);
                }
            } else {
                model = cube->obj->model;
                PLIST_FLAGS(model, cube_poly_flags[cube->poly]);
                model_draw_culled(cube->obj->model);
            }
            ulMatrixPop();
            break;
        case 2:
            draw_set_override_flags(0);
            ulMatrixPush();
            MTX_TRANS((Vec3 *)cube->obj->pos);
            vu0_matrix_rotate_y(ulMatrixCur, cube->obj->ang[1]);
            vu0_matrix_rotate_x(ulMatrixCur, cube->obj->ang[0]);
            if (cube->obj2[cube->poly % 4]) {
                model = cube->obj2[cube->poly % 4]->model;
                cull = draw_cull_box(model->r, &model->center.x);
                if (!(cull & 0xff)) {
                    cull &= 0xff00;
                    if (cull)
                        nj_cnk_vu1pkt_draw_clip(model);
                    else
                        nj_cnk_vu1pkt_draw_noclip(model);
                }
            } else {
                model = cube->obj->model;
                PLIST_FLAGS(model, cube_poly_flags[cube->poly]);
                model_draw_culled(cube->obj->model);
            }
            ulMatrixPop();
            break;
        }
    }
    ulMatrixPop();
    draw_set_override_flags(cube_flags);
    if (dc_object_get_compiled(cube_models[0]) && modeltest_compile_obj)
        CUBE_DRAW_SW = 1;
    else
        CUBE_DRAW_SW = 0;
    {
        unsigned int *cs = cube_color_sets[0];

        color_unpack(cs[0], rgba[0]);
        color_unpack(cs[1], rgba[1]);
        color_unpack(cs[2], rgba[2]);
        color_unpack(cs[3], rgba[3]);
        color_unpack(cs[4], rgba[4]);
    }
    cube = ents;
    for (i = 0; i < w->set.nents; i++, cube++) {
        col = cube_color_sets[cube->stage];
        switch (cube->state) {
        case 0:
            break;
        case 4:
            CUBE_CULL(cube);
            ulMatrixPush();
            MTX_TRANS(&cube->pos);
            cube_rotate_yxz((int *)cube->node_ang);
            MTX_SCALE1_COPY(cube->gscl);
            vu0_matrix_scale(ulMatrixCur, ((float *)cube->node_scale)[0], ((float *)cube->node_scale)[1], ((float *)cube->node_scale)[2]);
            draw_set_override_color(rgba[1]);
            if (CUBE_DRAW_SW) {
                CUBE_TREE_DRAW(0);
            } else {
                PLIST_HEAD(((WireObj *)w->parts[16]->root)->model, 8467);
                PLIST_FLAGS(((WireObj *)w->parts[16]->root)->model, 6);
                tree_draw_bake_w((WireObj *)w->parts[16]->root);
            }
            ulMatrixPop();
            break;
        case 5:
            CUBE_CULL(cube);
            ulMatrixPush();
            MTX_TRANS(&cube->pos);
            cube_rotate_yxz((int *)cube->node_ang);
            MTX_SCALE1(cube->gscl);
            if (*(int *)cube->node_flags & 0x1002) {
                vu0_matrix_scale(ulMatrixCur, ((float *)cube->node_scale)[0], ((float *)cube->node_scale)[1], ((float *)cube->node_scale)[2]);
                rgba[5][0] = (float)color_get_a(cube->color) / 255.0f;
                rgba[5][1] = (float)(cube->color & 0xff) / 255.0f;
                rgba[5][2] = (float)(cube->color >> 8 & 0xff) / 255.0f;
                rgba[5][3] = (float)(cube->color >> 16 & 0xff) / 255.0f;
                draw_set_override_color(rgba[5]);
                if (CUBE_DRAW_SW) {
                    CUBE_TREE_DRAW(1);
                } else {
                    PLIST_HEAD(((WireObj *)w->parts[16]->root)->model, 2067);
                    PLIST_FLAGS(((WireObj *)w->parts[16]->root)->model, 7);
                    tree_draw_bake_w((WireObj *)w->parts[16]->root);
                }
                if (*(int *)cube->node_flags & 0x1000) {
                    light_get_intensity(&diff, &ambi);
                    light_set_intensity(diff, CUBE_INTEN);
                    draw_set_override_color(rgba[4]);
                    vu0_matrix_scale(ulMatrixCur, 1.0f, cube_scale_y, 1.0f);
                    if (CUBE_DRAW_SW) {
                        CUBE_TREE_DRAW(2);
                    } else {
                        PLIST_HEAD(((WireObj *)w->parts[16]->root)->model, 8467);
                        PLIST_FLAGS(((WireObj *)w->parts[16]->root)->model, 8);
                        tree_draw_bake_w((WireObj *)w->parts[16]->root);
                    }
                    light_set_intensity(diff, ambi);
                    draw_set_blend(4, 5);
                    draw_set_color(part_wire_colors[0]);
                    dc_object_draw(w->parts[19]);
                }
            } else {
                vu0_matrix_scale(ulMatrixCur, ((float *)cube->node_scale)[0], ((float *)cube->node_scale)[1], ((float *)cube->node_scale)[2]);
                rgba[6][0] = (float)color_get_a(cube->color) / 255.0f;
                rgba[6][1] = (float)(cube->color & 0xff) / 255.0f;
                rgba[6][2] = (float)(cube->color >> 8 & 0xff) / 255.0f;
                rgba[6][3] = (float)(cube->color >> 16 & 0xff) / 255.0f;
                draw_set_override_color(rgba[6]);
                if (CUBE_DRAW_SW) {
                    CUBE_TREE_DRAW(3);
                } else {
                    PLIST_HEAD(((WireObj *)w->parts[16]->root)->model, 8467);
                    PLIST_FLAGS(((WireObj *)w->parts[16]->root)->model, 7);
                    tree_draw_bake_w((WireObj *)w->parts[16]->root);
                }
            }
            ulMatrixPop();
            break;
        case 8:
            CUBE_CULL(cube);
            ulMatrixPush();
            MTX_TRANS(&cube->pos);
            vu0_matrix_rotate_y(ulMatrixCur, w->roty);
            vu0_matrix_scale(ulMatrixCur, cube->gscl, cube->gscl, cube->gscl);
            draw_set_override_color(rgba[0]);
            if (CUBE_DRAW_SW) {
                CUBE_TREE_DRAW(4);
            } else {
                PLIST_HEAD(((WireObj *)w->parts[16]->root)->model, 8467);
                PLIST_FLAGS(((WireObj *)w->parts[16]->root)->model, 9);
                tree_draw_bake_w((WireObj *)w->parts[16]->root);
            }
            ulMatrixPop();
            break;
        case 9:
            CUBE_CULL(cube);
            ulMatrixPush();
            MTX_TRANS(&cube->pos);
            cube_rotate_yxz(cube->rot);
            vu0_matrix_scale(ulMatrixCur, cube->gscl, cube->gscl, cube->gscl);
            if (w->arm_count)
                v = boss_color_blend(col[0], col[2], cube->frames / (KURI_TIME * FPS));
            else
                v = boss_color_blend(col[0], col[2], cube->frames / (KURI_TIME4 * FPS));
            {
                int a = color_get_a(v), b = v >> 16, g, red;

                do { } while (0);
                g = v >> 8 & 0xff;
                do {
                    red = v & 0xff;
                    b &= 0xff;
                    rgba[7][0] = (float)a / 255.0f;
                    rgba[7][1] = (float)red / 255.0f;
                    rgba[7][2] = (float)g / 255.0f;
                    rgba[7][3] = (float)b / 255.0f;
                    draw_set_override_color(rgba[7]);
                } while (0);
            }
            if (CUBE_DRAW_SW)
                CUBE_TREE_DRAW(6);
            else
                tree_draw_bake_w((WireObj *)w->parts[17]->root);
            part_draw_beat(w->parts[19], part_wire_colors[3], part_wire_colors[4], 2.0f);
            ulMatrixPop();
            break;
        case 10:
            CUBE_CULL(cube);
            ulMatrixPush();
            MTX_TRANS(&cube->pos);
            cube_rotate_zyx(cube->rot);
            vu0_matrix_scale(ulMatrixCur, cube->gscl, cube->gscl, cube->gscl);
            rgba[8][0] = (float)color_get_a(col[2]) / 255.0f;
            rgba[8][1] = (float)(col[2] & 0xff) / 255.0f;
            rgba[8][2] = (float)(col[2] >> 8 & 0xff) / 255.0f;
            rgba[8][3] = (float)(col[2] >> 16 & 0xff) / 255.0f;
            draw_set_override_color(rgba[8]);
            if (CUBE_DRAW_SW)
                CUBE_TREE_DRAW(6);
            else
                tree_draw_bake_w((WireObj *)w->parts[17]->root);
            part_draw_beat(w->parts[19], part_wire_colors[3], part_wire_colors[4], 2.0f);
            draw_set_override_flags(cube_flags);
            ulMatrixPop();
            break;
        case 11:
            CUBE_CULL(cube);
            ulMatrixPush();
            MTX_TRANS(&cube->pos);
            cube_rotate_zyx(cube->rot);
            vu0_matrix_scale(ulMatrixCur, cube->gscl, cube->gscl, cube->gscl);
            draw_set_override_color(rgba[2]);
            if (CUBE_DRAW_SW)
                CUBE_TREE_DRAW(6);
            else
                tree_draw_bake_w((WireObj *)w->parts[17]->root);
            part_draw_beat(w->parts[19], part_wire_colors[3], part_wire_colors[4], 2.0f);
            rsc = RenderSetCurrentDisplayList(6);
            ulGxSetTextureList(w->file1);
            draw_set_override_flags(0);
            old_fog = (int)fog_set_enable(0);
            model_object_draw(w->parts[20], cube->anim_t);
            fog_set_enable((void *)old_fog);
            part_draw_beat_frame(w->parts[22], cube->anim_t, part_wire_colors[3], part_wire_colors[4], 2.0f);
            draw_set_override_flags(cube_flags);
            RenderSetCurrentDisplayList(rsc);
            ulMatrixPop();
            break;
        case 12:
            CUBE_CULL(cube);
            ulMatrixPush();
            MTX_TRANS(&cube->pos);
            vu0_matrix_rotate_y(ulMatrixCur, cube->rot[0]);
            vu0_matrix_rotate_z(ulMatrixCur, cube->rot[2]);
            vu0_matrix_rotate_y(ulMatrixCur, cube->rot[1]);
            vu0_matrix_scale(ulMatrixCur, cube->gscl, cube->gscl, cube->gscl);
            draw_set_override_color(rgba[3]);
            if (CUBE_DRAW_SW)
                CUBE_TREE_DRAW(6);
            else
                tree_draw_bake_w((WireObj *)w->parts[17]->root);
            part_draw_beat(w->parts[19], part_wire_colors[3], part_wire_colors[4], 2.0f);
            rsc = RenderSetCurrentDisplayList(6);
            ulGxSetTextureList(w->file1);
            draw_set_override_flags(0);
            old_fog = (int)fog_set_enable(0);
            model_object_draw(w->parts[20], cube->anim_t);
            fog_set_enable((void *)old_fog);
            part_draw_beat_frame(w->parts[22], cube->anim_t, part_wire_colors[3], part_wire_colors[4], 2.0f);
            draw_set_override_flags(cube_flags);
            RenderSetCurrentDisplayList(rsc);
            ulMatrixPop();
            break;
        default:
#ifdef VERSION_US
            CUBE_CULL(cube);
            {
#else
            if (!(draw_cull_box(w->parts[16]->radius * cube->gscl, (float *)&cube->pos) & 0xff)) {
#endif
                rsc = RenderSetCurrentDisplayList(6);
                ulGxSetTextureList(w->file1);
                ulMatrixPush();
                MTX_TRANS(&cube->pos);
                cube_rotate_yxz(cube->rot);
                vu0_matrix_scale(ulMatrixCur, cube->gscl, cube->gscl, cube->gscl);
                draw_set_override_color(rgba[0]);
                if (CUBE_DRAW_SW) {
                    CUBE_TREE_DRAW(5);
                } else {
                    PLIST_HEAD(((WireObj *)w->parts[16]->root)->model, 8467);
                    PLIST_FLAGS(((WireObj *)w->parts[16]->root)->model, 6);
                    tree_draw_bake_w((WireObj *)w->parts[16]->root);
                }
                ulMatrixPop();
                RenderSetCurrentDisplayList(rsc);
            }
            break;
        }
    }
    draw_pop_strip_flags();
    draw_set_override_flags(old_flags);
}

/* 0x00caa8d0 */
int homing_step(Vec3 *lead, Vec3 *tgt, Vec3 *pos, Vec3 *vel, float *k, int *n)
{
    float inv, x, y, z, t, tx, ty, tz;

    if (--*n <= 0) {
        v3_copy(lead, tgt);
        v3_copy(pos, tgt);
        return 1;
    }
    inv = 1.0f / (float)*n;
    do {
        x = lead->x;
        y = lead->y;
        z = lead->z;
    } while (0);
    do {
        tx = tgt->x;
        ty = tgt->y;
        tz = tgt->z;
    } while (0);
    x += (tx - x) * inv;
    y += (ty - y) * inv;
    z += (tz - z) * inv;
    lead->x = x;
    lead->y = y;
    lead->z = z;
    t = *k;
    t += (1.0f - t) / (float)*n;
    *k = t;
    pos->x += vel->x;
    pos->y += vel->y;
    pos->z += vel->z;
    pos->x += (x - pos->x) * t;
    pos->y += (y - pos->y) * t;
    pos->z += (z - pos->z) * t;
    return 0;
}

/* 0x00caaa00 */
void kuri_shatter(BossMainWork *w, Vec3 *pos, int *rot)
{
    float scale;

    if (kuri_levels[w->level].two)
        scale = 1.0f;
    else
        scale = RAND_FLOAT_B() * 0.5f + 0.5f;
    cm_wire_break_create(w, 1, w->parts[18], pos, 0, rot, 0, cmPartWireColor1,
                           cube_shatter_life, cube_shatter_speed, scale, 0);
}

/* 0x00caaad8 */
void kuri_shatter_both(BossMainWork *w, Vec3 *pos, int *rot)
{
    cm_wire_break_create(w, 1, w->parts[21], pos, 0, rot, 0, cmPartWireColor1,
                           cube_shatter_life, cube_shatter_speed, 1.0f, 0);
    kuri_shatter(w, pos, rot);
}

/* 0x00caab60 */
void cube_model_reset_poly(BossMainWork *w)
{
    short *p = ((WireObj *)w->parts[16]->root)->model->plist;
    short *head;
    short n;

    n = 16390;
    do {
        p[7] = n;
    } while (0);
    p += ((int)head & 1) >> 1;
    head = p + (((int)p & 1) >> 1);
    do {
        head[0] = 8467;
    } while (0);
    if (((int)head & 1) >> 1)
        head[1] = n;
}

/* 0x00caab88 */
void cube_shatter_small(BossMainWork *w, Vec3 *pos, int *rot)
{
    cm_wire_break_create(w, 1, w->parts[18], pos, 0, rot, 0, cmPartWireColor2,
                           cube_shatter_life, cube_shatter_speed, 0.1f, 0);
}

/* 0x00caabe0 */
void cube_rotate_zyx(int *ang)
{
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
}

/* 0x00caac30 */
void cube_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}

/* 0x00caac80 */
int player_range_check(Vec3 *pos)
{
    Vec3 pl;
    float far_sq = camera_get_far() * 2.0f;
    float dist_sq;

    far_sq = far_sq * far_sq;
    player_get_pos(&pl);
    dist_sq = vec3_distance_squared(&pl, pos);
    if (far_sq < dist_sq)
        return -1;
    if (dist_sq < BUBBLE_LEN * BUBBLE_LEN)
        return 1;
    return 0;
}

/* 0x00caad00 */
int player_touch(Vec3 *pos, float rad, float damage)
{
    Vec3 pl;

    player_get_pos(&pl);
    if (vec3_distance_squared(&pl, pos) < rad * rad) {
        player_status_add_damage(damage);
        return 1;
    }
    return 0;
}

/* 0x00caad70 */
int player_touch_curve(Vec3 *pos, int amount, float key, float height)
{
    BossCurve *curve = earth_mine_hit_curve;
    Vec3 pl;
    float rad, t;
    int i;

    for (i = 0; curve[i].key >= 0.0f && !(key <= curve[i].key); i++)
        ;
    if (!i) {
        rad = curve[0].val;
    } else if (curve[i].key < 0.0f) {
        rad = curve[i - 1].val;
    } else {
        t = (key - curve[i - 1].key) / (curve[i].key - curve[i - 1].key);
        rad = curve[i].val * t + curve[i].val * (1.0f - t);
    }
    player_get_pos(&pl);
    if (pos->y + height < pl.y)
        return 0;
    if (pl.y < pos->y - height)
        return 0;
    if (vec3_distance_squared(&pl, pos) < rad * rad) {
        player_status_add_damage((float)amount);
        return 1;
    }
    return 0;
}
