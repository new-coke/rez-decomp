/*
 * Earth boss: circles the player inside a shell of cubes while its arm attacks. Breaking the shell
 * exposes the core, whose hit points run down in three stages; then Kuri attacks and the defeat
 * plays. Also the light effects Circle_Light, Cube_Light, Move_Light and Move_Light_Set.
 */

#include "b_earth.h"
#include "mathdef.h"
#include "camera_demo.h"
#include "color.h"
#include "console.h"
#include "debug.h"
#include "enemy_status.h"
#include "fade.h"
#include "gw_varedit.h"
#include "libc.h"
#include "light.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_math.h"
#include "nj_strip.h"
#include "node_draw_state.h"
#include "pad_vib.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "render.h"
#include "sound_beat.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "earth.h"
#include "earth_arm.h"
#include "earth_bubble.h"
#include "earth_cube.h"
#include "earth_demo.h"
#include "../common/boss_common.h"
#include "../common/boss_demo_mes.h"
#include "../common/boss_sound.h"
#include "../common/boss_texture_anime.h"
#include "../common/boss_wire.h"
#include "../common/eff_circle.h"

extern float *matrix_from_axis_angle(float *m, Vec3 *a, int angle);
extern unsigned int stage_exit_flags;

extern BossStarCtl *star_ctl;
extern unsigned int star_color;
extern int material_ids[4];
extern float part_radii[];
extern char earth_vardef[], earth_vardefs[];
extern int load_files[3][2];
extern BossModelDef model_defs[];
extern char str_earth[];
extern BossMainWork *earth_work;
extern int earth_vars_group;
extern float ENEMY_LENGTH, ENEMY_LENGTH_W, ENEMY_LENGTH_R, ENEMY_LENGTH_RB;
extern float EARTH_BANK_LIM, EARTH_BANK_LIMB, core_lock_dist, cmShellBreakTime1;
extern int CMD_CORE_LIFE;
extern int wire_colors[3];
extern char earth_desc[], str_div_err[];
extern float final_rgba[4];
extern BossCfKey core_fade_keys[];
extern float CMD_CR, CMD_CLW;
extern int cmd_cfrm;
extern unsigned int circle_c;
extern float oscilate_x, oscilate_y;
extern float cmShellBreakTime2, cmShellBreakTime3;
extern int CMD_DEB_MOVIE_SW, CMD_DEB_MOVIE_NUM;
extern int arm_damage_rata[];
extern int deb_movies[][2];
extern TaVtx cube_light_vtx[4];

/* "Circle_Light", 132 bytes */
typedef struct BossCircleLight {
    char pad_00[4];
    void *list;                 /* 0x04: drawn with ulGxSetTextureList */
    TaVtx v[4];                 /* 0x08 */
    Vec3 *pos;                  /* 0x68 */
    int count, total;           /* 0x6c */
    int delay;                  /* 0x74 */
    int state;                  /* 0x78 */
    int tex;                    /* 0x7c */
    float size;                 /* 0x80 */
} BossCircleLight;

extern float screen_layout[3];
extern struct TaskDesc circle_light_desc;
extern char cube_light_desc[], cube_light_desc2[];
extern float cube_light_z, cube_light_dz, cube_light_time, cube_light_r;
extern float side_break_speed, side_break_life;

/* "Move_Light", 460 bytes: light streak from a point to a target */
typedef struct BossMoveLight {
    char pad_00[4];
    void *list;                 /* 0x04: drawn with ulGxSetTextureList */
    Vec3 *target;               /* 0x08 */
    Vec3 org;                   /* 0x0c */
    Vec3 pts[32];               /* 0x18: a ring of points, newest at head */
    Vec3 dir;                   /* 0x198 */
    int state;                  /* 0x1a4 */
    int head;                   /* 0x1a8 */
    int count, total;           /* 0x1ac */
    int count2, total2;         /* 0x1b4 */
    int tex;                    /* 0x1bc */
    float frac;                 /* 0x1c0 */
    float acc;                  /* 0x1c4 */
    float speed;                /* 0x1c8 */
} BossMoveLight;

/* "Move_Light_Set", 32 bytes: starts a Move_Light each interval */
typedef struct BossMoveLightSet {
    char pad_00[8];
    int count;                  /* 0x08 */
    int interval;               /* 0x0c */
    int timer;                  /* 0x10 */
    int tex;                    /* 0x14 */
    Vec3 *pos;                  /* 0x18 */
    void *parent;               /* 0x1c */
} BossMoveLightSet;

extern char move_light_desc[], move_light_set_desc[];
extern float move_light_time, move_light_time2, move_light_speed, move_light_dist;
extern float move_light_side, move_light_width, move_light_frac_min, move_light_frac_max;
extern float TURN_TIME;
extern float ENEMY_TIME;
extern unsigned short BASE_HP_SHELL;
extern int CMD_LOOP_BREAK;
extern int shell_break_num[4];
extern int defeat_movies[][2];
extern float move_light_interval, CMD_MODE_FIN_1, CMD_MODE_FIN_3, CMD_MODE_FIN_4, CMD_MODE_FIN_6, CMD_FLASH_ST_FRM;
extern int move_light_count;
extern int defeat_burst_frames[2];

/* 0x00ca0700 */
void star_color_apply(void)
{
    if (star_ctl)
        boss_star_set_color(star_ctl, star_color);
}

/* 0x00ca0720 */
int boss_main_demo_cb(BossMainWork *w)
{
    if (w->state == 0)
        w->state = 1;
    else if (w->state == 2)
        return 1;
    return 0;
}

/* steer(cast) */
/* 0x00ca0750 */
void earth_init(BossMainWork *w, int *arg)
{
    w->arg0 = arg[0];
    w->level = arg[1];
    w->phase = 0;
    w->state = 0;
    boss_files_request((int)load_files[0], (int)load_files[1]);
    boss_start((int *)w, (int)w->parts, w->arg0, w->level);
    w->bgm = boss_bgm_start(w);
    cm_se_laser_create(load_files[2]);
    earth_score_vars_add();
    cube_vars_add();
    earth_1st_demo_create(w, (int (*)(void *))boss_main_demo_cb, w);
    boss_demo_mes_create(w, str_earth);
    earth_work = w;
    earth_vars_group = ((int (*)(struct GwVarNode **, int, char *))gw_varedit_add)(&gw_varedit_root, 0, earth_vardef);
    gw_varedit_add_table(&gw_varedit_root, earth_vars_group, earth_vardefs);
}

/* 0x00ca0848 */
void earth_exec_start(BossMainWork *w)
{
    FogArg arg;

    if (w->state == 1) {
        unsigned int *col = &star_color;
        BossStarCtl **ctl = &star_ctl;

        earth_map_create(w, w->level);
        /* steer(alias): the callbacks are stored as data-pointer members, as the original */
        ((struct { int unk_00[4]; void *exec, *disp; } *)TASK_OF(w))->exec = (void *)earth_exec_battle;
        ((struct { int unk_00[4]; void *exec, *disp; } *)TASK_OF(w))->disp = (void *)earth_display;
        w->state = 2;
        w->wait = 306;
        arm_enable(w->arm);
        boss_adx_request(w->bgm);
        arg.range = 400.0f;
        arg.k = 0.5f;
        arg.count = 200;
        arg.color = *col & 0xffffff;
        *ctl = boss_star_create(w, &arg, 0.0f);
        boss_star_fade(*ctl, *col, 120);
    }
}

/* 0x00ca0940 */
void earth_materials_save(BossMainWork *w)
{
    int i;

    for (i = 0; i < 4; i++)
        w->mat[i] = model_material_find(((WireObj *)w->parts[2]->root)->child->model, &w->save[i],
                                           material_ids[i]);
}

/* 0x00ca09e0 */
void earth_materials_restore(BossMainWork *w)
{
    int i;

    for (i = 0; i < 4; i++)
        store_if((int *)w->mat[i], w->save[i]);
}

/* 0x00ca0a38 */
void earth_exec(BossMainWork *w)
{
    if (boss_files_ready(load_files[0], load_files[1])) {
        w->file0 = model_pnb_load((char *)load_files[0]);
        w->file1 = model_load_timed((char *)load_files[1], w->arg0);
        do {
            boss_parts_build((void **)w, model_defs, w->parts, w->pool);
        } while (0);
        parts_set_radii(w->parts, part_radii);
        cube_models_build(w);
        earth_materials_save(w);
        earth_start(w, 0);
        TASK_OF(w)->exec = (void (*)(void *))earth_exec_start;
    }
}

/* 0x00ca0b00 */
void earth_start(BossMainWork *w, int restart)
{
    Vec3 v;
    BossPartList bubble_parts;
    BossArmArg arg;
    float sc[4];
    WireObj *obj;
    int i, n, quarter, idx;
    float dist;

    v3_set(&w->ofs, 0.0f, 0.0f, 0.0f);
    v3_set(&w->vel, 0.0f, 0.0f, 0.0f);
    iv3_set(w->rot, 0, 0, 0);
    v3_set(&w->vel0, 0.0f, 0.0f, 0.0f);
    iv3_set(w->ang, 0, 0, 0);
    w->amp_z = 2.0f;
    w->amp_y = 2.0f;
    w->amp_x = 12.0f;
    w->turn[1] = 182;
    w->wobble_speed = 0.005f;
    w->turn[2] = 0;
    w->turn[0] = 0;
    w->kuri_state = 0;
    w->orbit_side = 0;
    w->swing_side = 0;
    w->swings = 0;
    w->orbit_state = 0;
    w->dist = ENEMY_LENGTH_W;
    w->dist_back = ENEMY_LENGTH;
    w->unk_1f9e0 = 0;
    w->kuri_timer = 0;
    w->turns = 0;
    w->nerr = 0;
    w->animflags = 0;
    w->flags = 0;
    w->shell_hits = 0;
    w->kuri_hits = 0;
    w->bubbles_end = 0;
    w->loops = 0;
    w->far = 0;
    *(int *)&w->drawflags = -1;
    w->rot[0] = 691;
    player_get_pos(&w->pos);
    dist = w->dist;
    v.x = 0.0f * dist;
    v.y = 0.0f * dist;
    v.z = -dist;
    v3_add(&w->pos, &v);
    earth_materials_restore(w);
    if (!restart) {
        /* not in index order: earth_bubbles_setup bakes p[3..5] (parts 11, 13, 15) */
        bubble_parts.p[0] = w->parts[8];
        bubble_parts.p[1] = w->parts[9];
        bubble_parts.p[2] = w->parts[14];
        bubble_parts.p[3] = w->parts[11];
        bubble_parts.p[4] = w->parts[13];
        bubble_parts.p[5] = w->parts[15];
        bubble_parts.p[6] = w->parts[10];
        bubble_parts.p[7] = w->parts[12];
        earth_bubbles_setup(&bubble_parts);
    }
    boss_parts_reset_time(w->parts);
    arg.pos = &w->pos;
    arg.ofs = &w->ofs;
    arg.dir = &w->dir;
    arg.ang = w->ang;
    arg.roty = &w->roty;
    arg.cmd = &w->arm_cmd;
    arg.part = w->parts[13];
    arg.cb_arg = w;
    arg.update_cb = earth_arm_exec_cb;
    arg.disp_cb = earth_arm_disp_cb;
    if (restart)
        arm_reinit(w->arm, &arg);
    else
        w->arm = arm_create(w, &arg);
    if (!restart) {
        model_uv_create(w, ((WireObj *)w->parts[7]->root)->child->model, 2, 0.0f, 0.04f);
        model_uv_create(w, ((WireObj *)w->parts[7]->root)->child->sibling->model, 2, 0.0f, 0.04f);
    }
    w->hp = CMD_CORE_LIFE;
    for (i = 0; i < 4; i++) {
        LockObj *lock = &w->lock[i];

        lock_init(0, lock, w, &w->lockpos[i], (int)&w->lockdir[i],
                               CMD_CORE_LIFE, 129);
        lockon_entry(lock);
        lock->flags &= ~LOCK_LOCKABLE;
    }
    n = CMD_CORE_LIFE;
    quarter = n / 4;
    w->flags |= 4;
    for (idx = 2; idx >= 0; idx--)
        w->thr[idx] = n - quarter * (idx + 1);
    w->thr[3] = 0;
    w->stage = 0;
    cubes_rebuild(w);
    w->phase = 5;
}

/* 0x00ca0f10 */
void earth_restart(BossMainWork *w)
{
    int i = 4;

    cubes_break_all(w, -1);
    if (w->flags & 1) {
        dc_object_coll_unlock(w->parts[0]);
        w->flags &= ~1;
    }
    if (w->flags & 4) {
        for (i = 0; i < 4; i++)
            lock_remove_entry(0, &w->lock[i]);
        w->flags &= ~4;
    }
    earth_start(w, 1);
    boss_vol_play(w->bgm, 1, 0);
    TASK_OF(w)->exec = (void (*)(void *))earth_exec_battle;
    TASK_OF(w)->disp = (void (*)(void *))earth_display;
    w->shell_timer = cmShellBreakTime1 * FPS;
}

/* 0x00ca1048 */
void earth_release(BossMainWork *w)
{
    cube_models_free(w);
    light_reset();
    light_fog_set();
    if (w->phase >= 5) {
        if (w->flags & 1)
            dc_object_coll_unlock(w->parts[0]);
        if (w->flags & 4) {
            lock_remove_entry(0, &w->lock[0]);
            lock_remove_entry(0, &w->lock[1]);
            lock_remove_entry(0, &w->lock[2]);
            lock_remove_entry(0, &w->lock[3]);
        }
        earth_bubbles_cleanup();
    }
    console2_swap_boss_on(0);
    if (boss_parts_release(w->parts)) {
        model_unload_timed(w->file1);
        model_pnb_unload(w->file0);
        w->file1 = 0;
        w->file0 = 0;
    }
    boss_end(w);
    earth_score_vars_remove();
    cube_vars_remove();
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)earth_vardef);
}

/* 0x00ca1158 */
void earth_turn_to_player(BossMainWork *w)
{
    Vec3 dir, up, axis;
    float mtx[16];
    Vec3 n;
    float hi, k, t;
    int bank;

    if (w->far) {
        hi = ENEMY_LENGTH_RB;
        k = EARTH_BANK_LIMB;
    } else {
        hi = ENEMY_LENGTH_R;
        k = EARTH_BANK_LIM;
    }
    t = (w->dist - ENEMY_LENGTH) / (hi - ENEMY_LENGTH);
    if (t > 1.0f)
        t = 1.0f;
    player_get_pos(&dir);
    v3_sub(&dir, &dir, &w->pos);
    vec3_normalize(&dir, &dir);
    up.x = up.z = 0.0f;
    up.y = 1.0f;
    ulVec3Cross(&axis, &dir, &up);
    if (dir.x == up.x && dir.y == up.y && dir.z == up.z)
        bank = 0;
    else
        bank = DEG_TO_ANG(njSin(t * 1.5707964f * ANG_PER_RAD) * k);
    if (bank) {
        vec3_normalize(&n, &axis);
        matrix_from_axis_angle(mtx, &n, bank);
    } else {
        matrix_unit_set((int)mtx);
    }
    vu0_matrix_rotate_y(mtx, w->roty);
    matrix_to_angles(mtx, w->ang);
}

/* 0x00ca1378 */
void earth_lock_points(BossMainWork *w)
{
    Vec3 lock_ofs, lock_dir;
    Vec3 *p;
    int i;

    lock_ofs.x = lock_ofs.y = 0.0f;
    lock_ofs.z = -core_lock_dist;
    lock_dir.x = lock_dir.y = 0.0f;
    lock_dir.z = 1.0f;
    ulMatrixPush();
    matrix_unit_set(0);
    MTX_TRANS(&w->pos);
    earth_rotate_yxz(w->ang);
    p = w->lockpos;
    for (i = 0; i < 4; i++) {
        vu0_matrix_rotate_y(ulMatrixCur, ANG_90);
        ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&lock_ofs, (float *)p);
        ulMatrixPush();
        ulMatrixSetTranslation((float *)ulMatrixCur, 0.0f, 0.0f, 0.0f);
        ulMatrixApplyVector((float *)ulMatrixCur, &lock_dir.x, &(p - 4)->x);
        ulMatrixPop();
        p++;
    }
    ulMatrixPop();
    if (w->flags & 1) {
        ulMatrixPush();
        MTX_TRANS(&w->pos);
        earth_rotate_yxz(w->ang);
        model_node_run_default(w->parts[0]);
        ulMatrixPop();
    }
}

/* 0x00ca1510 */
void earth_move(BossMainWork *w)
{
    Vec3 v, pl_pos;
    int pl_rot[4];
    float sc[6];
    float x, y, z;

    player_get_pos(&pl_pos);
    x = y = 0.0f;
    z = -1.0f;
    player_get_rot(pl_rot);
    ROT_Y(pl_rot[1], sc, x, z);
    v3_set(&w->dir, x, y, z);
    vec3_normalize(&v, &w->dir);
    v3_scale(&v, w->dist);
    ROT_X(w->rot[0], (sc + 2), v.y, v.z);
    ROT_Y(w->rot[1], (sc + 4), v.x, v.z);
    v3_sum(&w->target, &v, &pl_pos);
    v3_sum(&w->pos, &w->target, &w->ofs);
    v3_copy(&w->vel0, &w->vel);
    v3_add(&w->prev, &pl_pos);
    v3_sub(&w->vel, &w->pos, &w->prev);
    v3_copy(&w->prev, &w->pos);
}

/* steer(cast) */
/* 0x00ca1770 */
void earth_display(BossMainWork *w)
{
    float diff, ambi;
    int z, list;

    ulGxSetTextureList(w->file1);
    light_update();
    boss_texlist_bind((int *)w);
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    earth_rotate_yxz(w->ang);
    draw_push_strip_flags();
    draw_push_override_color();
    z = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    if (w->drawflags & 4) {
        light_get_intensity(&diff, &ambi);
        light_set_intensity(diff, 0.0f);
        draw_set_override_color(w->rgba);
        dc_object_draw(w->parts[2]);
        light_set_intensity(diff, ambi);
    }
    draw_set_override_flags(DRAW_STRIP_OVERRIDE);
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    if (w->drawflags & 2)
        dc_object_draw(w->parts[1]);
    if (w->drawflags & 0x40000000) {
        dc_object_draw(w->parts[4]);
        part_draw_beat(w->parts[6], wire_colors[0], wire_colors[1], 2.0f);
    }
    if (w->drawflags & 0x80000000) {
        ulMatrixPush();
        vu0_matrix_rotate_z(ulMatrixCur, ANG_180);
        dc_object_draw(w->parts[4]);
        part_draw_beat(w->parts[6], wire_colors[0], wire_colors[1], 2.0f);
        ulMatrixPop();
    }
    if (w->drawflags & 0x80) {
        list = RenderSetCurrentDisplayList(6);
        ulGxSetTextureList(w->file1);
        dc_object_draw(w->parts[7]);
        RenderSetCurrentDisplayList(list);
    }
    draw_pop_strip_flags();
    draw_pop_override_color();
    draw_set_override_flags(z);
    ulMatrixPop();
}

/* 0x00ca1980 */
BossMainWork *earth_create(void *parent, void *arg)
{
    return task_create_inherit_arg(parent, earth_desc, sizeof(BossMainWork), arg);
}

/* 0x00ca19b0 */
void task_finish_if(void *w)
{
    FINISH_IF(w);
}

/* 0x00ca19d0 */
int shell_hit_check(BossMainWork *w)
{
    BossShellGrp *grp;
    LockObj *lock;
    int i, j, n;

    grp = w->set.grp;
    if (!(w->flags & 1))
        return 0;
    n = 0;
    for (i = 0; i < w->set.ngrp; i++, grp++) {
        if (grp->done)
            continue;
        n++;
        lock = model_get_part(w->parts[0], i);
        if (!lock)
            for (;;)
                ;
        if (!(lock->flags & LOCK_HIT))
            continue;
        if (lock->life <= 0) {
            lock_score(4, lock);
            boss_se_play((int)w, 8, (int)lock, 0);
            w->hp--;
            if (++w->shell_hits >= 4) {
                w->shell_hits = 0;
                boss_se_play((int)w, 24, (int)lock, 0);
            }
            lock->flags &= ~LOCK_LOCKABLE;
            grp->done = 1;
            for (j = 0; j < grp->n; j++) {
                if (grp->ent[j]) {
                    cube_break_off(w, grp->ent[j], 0, 0);
                } else {
                    sprintf(w->msg, str_div_err);
                    w->nerr++;
                }
            }
        } else {
            boss_se_play((int)w, 7, (int)lock, 0);
        }
    }
    return n;
}

/* 0x00ca1c00 */
void cubes_shatter_all(BossMainWork *w)
{
    BossCube *cube = w->set.ents;
    int i;

    cube_model_reset_poly(w);
    for (i = 0; i < w->set.nents; i++, cube++) {
        if (!cube->state)
            continue;
        cube->state = 0;
        if (cube->locked) {
            kuri_shatter(w, &cube->pos, cube->rot);
            lock_remove_entry(8, &cube->lock);
            cube->locked = 0;
        } else {
            cube_shatter_small(w, &cube->pos, cube->rot);
        }
    }
}

/* 0x00ca1cc0 */
void shell_locks_enable(BossMainWork *w)
{
    BossShellGrp *grp = w->set.grp;
    LockObj *lock;
    int i;

    if (!(w->flags & 1))
        return;
    for (i = 0; i < w->set.ngrp; i++, grp++) {
        if (grp->done)
            continue;
        lock = model_get_part(w->parts[0], i);
        if (lock)
            lock->flags &= ~LOCK_LOCKABLE;
    }
}

/* 0x00ca1d80 */
void earth_arm_disp_cb(void *w)
{
    cubes_disp(w);
}

/* 0x00ca1d88 */
void earth_arm_exec_cb(void *w)
{
    cubes_update(w);
}

/* 0x00ca1d90 */
int earth_orbit_step(BossMainWork *w)
{
    Vec3 v;
    float sc[7][2];
    float hi;
    int ret = 0;
    int dir = !w->orbit_side ? ANG_90 : -ANG_90;

    if (w->far)
        hi = ENEMY_LENGTH_RB;
    else
        hi = ENEMY_LENGTH_R;
    wobble_step(&w->ofs, (Vec3 *)&w->amp_x, (float *)&w->wobble_phase, w->wobble_speed);
    switch (w->orbit_state) {
    case 0:
    case 4:
        if (w->far == 1) {
            w->far = 2;
            core_hp_reset(w);
        }
        if (w->dist_count != 0.0f) {
            w->dist_count -= 1.0f;
            ulSinCosf(w->dist_count * 1.5707964f / (TURN_TIME * FPS), &sc[0][0], &sc[0][1]);
            w->dist = ENEMY_LENGTH + (hi - ENEMY_LENGTH) * sc[0][0];
        }
        if (w->turns) {
            w->orbit_state++;
            w->turn_count = TURN_TIME * FPS;
            w->turns--;
        } else {
            w->turn[1] = (float)w->turn[1] * 0.98f + 3.0f;
        }
        break;
    case 1:
    case 5:
        if (w->dist_count != 0.0f) {
            w->dist_count -= 1.0f;
            ulSinCosf(w->dist_count * 1.5707964f / (TURN_TIME * FPS), &sc[1][0], &sc[1][1]);
            w->dist = ENEMY_LENGTH + (hi - ENEMY_LENGTH) * sc[1][0];
            if (w->flags2 & 4) {
                ulSinCosf((1.0f - w->turn_count / (TURN_TIME * FPS)) * 1.5707964f, &sc[2][0], &sc[2][1]);
                w->turn[1] = (int)DEG_TO_ANG(sc[2][1] * 3.0f) + 182;
            } else {
                w->turn[1] = 182;
            }
        }
        if (w->turn_count != 0.0f) {
            ulSinCosf((1.0f - w->turn_count / (TURN_TIME * FPS)) * 1.5707964f, &sc[3][0], &sc[3][1]);
            w->turn_count -= 1.0f;
            w->rot[1] = dir * sc[3][0];
        } else {
            if (w->flags2 & 4)
                w->flags2 &= ~4;
            w->orbit_state++;
            w->rot[1] = dir;
            ret = 2;
            if (w->turns)
                ret = 0;
        }
        break;
    case 2:
    case 6:
        if (w->far == 2)
            w->far = 0;
        if (w->turns) {
            vec3_normalize(&v, &w->dir);
            v3_scale(&v, w->dist_back);
            w->orbit_state++;
            v3_mul((Vec3 *)w->back_ofs, &v, -1.0f);
            w->turns--;
            if (w->turns)
                w->dist_count = TURN_TIME * FPS * 2.0f;
            else
                w->dist_count = 0.0f;
            w->turn_count = TURN_TIME * FPS;
        } else {
            w->turn[1] = (float)w->turn[1] * 0.98f + 3.0f;
        }
        break;
    case 3:
    case 7:
        if (w->dist_count != 0.0f) {
            w->dist_count -= 1.0f;
            ulSinCosf(w->dist_count * 1.5707964f / (TURN_TIME * FPS), &sc[4][0], &sc[4][1]);
            w->dist = ENEMY_LENGTH + (hi - ENEMY_LENGTH) * sc[4][0];
        }
        if (w->turn_count != 0.0f) {
            w->turn_count -= 1.0f;
            ulSinCosf((1.0f - w->turn_count / (TURN_TIME * FPS)) * 1.5707964f, &sc[5][0], &sc[5][1]);
            w->rot[1] = dir * sc[5][1];
            if (w->flags2 & 4) {
                ulSinCosf((1.0f - w->turn_count / (TURN_TIME * FPS)) * 1.5707964f, &sc[6][0], &sc[6][1]);
                w->turn[1] = (int)DEG_TO_ANG(sc[6][0] * 3.0f) + 182;
            } else {
                w->turn[1] = 182;
            }
        } else {
            w->orbit_side ^= 1;
            w->rot[1] = 0;
            if (w->orbit_side)
                w->orbit_state = 4;
            else
                w->orbit_state = 0;
            ret = w->turns == 0;
        }
        break;
    }
    w->roty += w->turn[1];
    return ret;
}

/* 0x00ca2420 */
void earth_phase_step(BossMainWork *w)
{
    float sc[4];
    BossShellGrp *grp;
    LockObj *lock;
    int *movie;
    int i;

    if (w->wait)
        w->wait--;
    switch (w->phase) {
    case 5:
        w->turn_count = 1.5707964f / (ENEMY_TIME * FPS);
        w->approach = 0.0f;
        w->phase = 6;
        w->dist = ENEMY_LENGTH_W;
        *(int *)&w->drawflags = -1;
        w->unk_1fb4c = 0;
        boss_gage_create(0);
    case 6:
        if (w->wait)
            break;
        if (beat_clock_get_frames_to_beat() < 2) {
            if (!w->animflags)
                w->animflags = -1;
        }
        w->approach += w->turn_count;
        w->kuri_round = 0;
        w->shell_timer = cmShellBreakTime1 * FPS;
        if (w->approach >= 1.5707964f) {
            w->phase = 7;
            w->dist = ENEMY_LENGTH;
            w->flags2 = 1;
            w->orbit_state = 0;
            model_parts_init(w, w->parts[0]);
            grp = w->set.grp;
            for (i = 0; i < w->set.ngrp; i++, grp++) {
                LockObj *k = model_get_part(w->parts[0], i);
                grp->lock = k;
                k->life = k->life_max = BASE_HP_SHELL;
                ((short *)k)[7] = -1;
                k->flags |= LOCK_SILENT;
                lock_set_type(4, k);
            }
            {
                int j;
                for (j = 0; j < 4; j++) {
                    LockObj *k = &w->lock[j];
                    k->flags |= LOCK_LOCKABLE;
                }
            }
            w->flags |= 5;
        } else {
            ulSinCosf(w->approach, &sc[0], &sc[1]);
            ulSinCosf(sc[1] * PI, &sc[2], &sc[3]);
            w->dist = ENEMY_LENGTH + ENEMY_LENGTH_W * (sc[3] * -0.5f + 0.5f);
            break;
        }
    case 7:
        if (w->flags2 & 1) {
            w->flags2 &= ~1;
            if (w->flags2 & 0x20000) {
                w->phase = 10;
                break;
            }
            w->attack_end = 0;
            if (w->level)
                earth_bubble_manage_create(w, &w->pos, 0, (int)&w->bubbles_end, 240);
        }
        w->turn[1] = (float)w->turn[1] * 0.98f + 1.0f;
        if (w->flags & 1) {
            i = shell_hit_check(w);
            if (i <= shell_break_num[w->stage % 4] && !cubes_any_in_state(w, 14)) {
                cubes_break_all(w, 1);
                if (w->orbit_state == 6 || w->orbit_state == 2) {
                    w->arm_cmd = 2;
                    boss_vol_play_stage(w->bgm, 2, 0, w->stage);
                    w->phase = 9;
                } else {
                    w->phase = 8;
                    w->turns++;
                }
                w->attack_end = 1;
            }
        }
        if (w->shell_timer && --w->shell_timer <= 0) {
            if (w->clearing)
                break;
            cubes_break_all(w, 1);
            earth_bubble_manage_create(w, &w->pos, 1, (int)&w->bubbles_end, 0);
        }
        break;
    case 8:
        shell_hit_check(w);
        if (w->flags2 & 2) {
            w->flags2 &= ~2;
            if (!(w->flags2 & 0x40000)) {
                w->arm_cmd = 2;
                if (!cubes_any_in_state(w, 14))
                    boss_vol_play_stage(w->bgm, 2, 0, w->stage);
            }
            w->phase = 9;
        }
        break;
    case 9:
        shell_hit_check(w);
        if (!(w->flags2 & 0x40000) && w->arm_cmd)
            break;
        w->flags2 &= ~0x40000;
        if (w->flags2 & 0x10000) {
            w->turns++;
            w->phase = 7;
            w->flags2 &= ~0x10000;
            if (w->flags2 & 0x20000)
                w->phase = 10;
        } else if (w->flags2 & 0x20000) {
            w->phase = 10;
            w->turns++;
        } else {
            boss_vol_play_stage(w->bgm, 3, 0, w->stage);
            boss_vol_play(w->bgm, 13, 0);
            w->arm_cmd = 3;
            if (++w->loops >= CMD_LOOP_BREAK && !w->far)
                w->far = 1;
            w->phase = 8;
            w->flags2 |= 4;
            w->turns += 2;
        }
        break;
    case 10:
        if (kuri_attack_step(w))
            w->phase = 11;
        break;
    case 11:
        lock_score(0, w->lock);
        {
            int j;
            for (j = 0, lock = w->lock; j < 4; j++, lock++) {
                store_if((int *)w->mat[j], 0xff000000);
                lock->flags &= ~LOCK_LOCKABLE;
            }
        }
        boss_pcm_load(w->bgm, 0);
        TASK_OF(w)->exec = (void (*)(void *))earth_exec_defeat;
        TASK_OF(w)->disp = (void (*)(void *))earth_disp_final;
        w->phase = 12;
        movie = defeat_movies[(unsigned int)player_status_get_level() % 6];
        if (movie[1])
            tex_anime_start(w, movie, boss_get_arg0(), 120);
        else
            tex_anime_start(0, 0, 0, 0);
        break;
    }
}

static inline void i3_sub(int *dst, int *src_a, int *src_b)
{
    dst[0] = src_a[0] - src_b[0];
    dst[1] = src_a[1] - src_b[1];
    dst[2] = src_a[2] - src_b[2];
}

/* 0x00ca2b90 */
void earth_exec_defeat(BossMainWork *w)
{
    int v[3];
    Vec3 ofs;
    int old[3];
    float sc[6];
    int *mot;
    ModelPart *part;
    int i;
    int k;

    w->roty += w->turn[1];
    earth_move(w);
    earth_turn_to_player(w);
    switch (w->phase) {
    case 12:
        kuri_attack_step(w);
        if (!beat_grid_hit(0))
            break;
        if (beat_clock_get_haku() & 3)
            break;
        oscilate_create(oscilate_x, oscilate_y * 0.5f);
        cm_bgm_sound_man_end_children(0);
        boss_se_stop_all();
        boss_demo_mes2_start(w, str_earth, 0);
        ((void (*)(BgmMan *))boss_pcm_start)(w->bgm); /* steer(cast) */
        boss_vol_play(w->bgm, 11, 0);
        console2_swap_boss_on(0);
        cubes_shatter_all(w);
        shell_locks_enable(w);
        cubes_lock_remove(w);
        w->bubbles_end = 1;
        w->attack_end = 2;
        w->drawflags = 4;
        w->animflags = 0;
        iv3_set(w->ang2, 0, 0, 0);
        w->phase = 13;
        TASK_OF(w->arm)->exec = 0;
        TASK_OF(w->arm)->disp = 0;
        w->fin_total = w->fin_count = CMD_MODE_FIN_1 * FPS;
    case 13:
        if (--w->fin_count > 0)
            break;
        if (!beat_grid_hit(0))
            break;
        tex_anime_fade_in_out();
        w->animflags |= 4;
        {
            ModelPart *part = w->parts[2];
            mot = pnb_get_entry(w->file0, 44);
            tree_node_pose((WireObj *)part->root, (void *)*mot, 0, w->fin_ang, 0, 1, 0.0f);
        }
        i3_sum(w->fin_ang, w->fin_ang, w->ang);
        cm_obj_mot_create(w, &w->pos, w->fin_ang, w->parts[24], 0, 0.5f, 0.5f);
        w->phase = 14;
        w->fin_total = w->fin_count = 128;
        break;
    case 14:
        part = w->parts[2];
        k = *(volatile float *)&part->frame;
        if (k == defeat_burst_frames[0] || k == defeat_burst_frames[1]) {
            mot = pnb_get_entry(w->file0, 44);
            tree_node_pose((WireObj *)part->root, (void *)*mot, 0, w->fin_ang, 0, 1, 0.0f);
            i3_sum(w->fin_ang, w->fin_ang, w->ang);
            cm_obj_mot_create(w, &w->pos, w->fin_ang, w->parts[24], 0, 0.5f, 0.5f);
            part = w->parts[2];
        }
        part->frame += 1.0f;
        if (part->frame >= 128.0f) {
            part->frame = 128.0f;
            w->animflags &= ~4;
        }
        if (--w->fin_count > 0)
            break;
        move_light_set_create(w, &w->pos, 2, move_light_count, move_light_interval);
        cm_obj_mot_set_time(cm_obj_mot_create(w, &w->pos, &w->fin_ang[6], w->parts[23], 1, 0.4f, 0.4f), CMD_FLASH_ST_FRM);
        w->phase = 15;
        w->fin_total = w->fin_count = CMD_MODE_FIN_3 * FPS;
    case 15:
        if (--w->fin_count > 0)
            break;
        w->phase = 16;
        w->fin_total = w->fin_count = CMD_MODE_FIN_4 * FPS;
    case 16:
        if (--w->fin_count > 0)
            break;
    case 17:
        if (!beat_grid_hit(0))
            break;
        if (beat_clock_get_haku() & 3)
            break;
        peri_act_play_boss_finish();
        boss_demo_mes2_start(w, str_earth, 1);
        lock_score_level(0, &w->pos);
        *(int *)&w->drawflags &= ~4;
        i3_sum(v, w->ang, w->ang2);
        for (i = 0; i < 4; i++)
            cm_wire_break_simple(w, 5, w->parts[3], &w->pos, 0, v, wire_colors[2]);
        boss_se_play((int)w, 4, (int)w->lock, (int)&w->pos);
        oscilate_create(oscilate_x, oscilate_y * 0.5f);
        w->phase = 18;
        w->fin_total = w->fin_count = CMD_MODE_FIN_6 * FPS;
        fade_start(2, 0, 60);
    case 18:
        if (--w->fin_count > 0)
            break;
        fade_start_out(60);
        ((void (*)(void *))boss_fx_release)(w); /* steer(cast) */
        stage_exit_flags |= 10;
        TASK_OF(w)->exec = 0;
        return;
    }
    if (w->animflags & 4) {
        part = w->parts[2];
        i3_copy(old, w->ang2);
        mot = pnb_get_entry(w->file0, 44);
        tree_node_pose((WireObj *)part->root, (void *)*mot, &ofs, w->ang2, 0, 1, w->parts[2]->frame);
        ROT_Z(w->ang2[2], (sc + 0), ofs.x, ofs.y);
        ROT_X(w->ang2[0], (sc + 2), ofs.y, ofs.z);
        ROT_Y(w->ang2[1], (sc + 4), ofs.x, ofs.z);
        v3_add(&w->pos, &ofs);
        i3_sub(&w->fin_ang[3], w->ang2, old);
        i3_sum(&w->fin_ang[6], w->ang2, w->ang);
    } else {
        w->fin_ang[3] = w->fin_ang[3] * 0.9999f;
        w->fin_ang[4] = w->fin_ang[4] * 0.9999f;
        w->fin_ang[5] = w->fin_ang[5] * 0.9999f;
        i3_sum(w->ang2, w->ang2, &w->fin_ang[3]);
        i3_sum(&w->fin_ang[6], w->ang2, w->ang);
    }
}

/* steer(cast) */
/* 0x00ca34d8 */
void earth_disp_final(BossMainWork *w)
{
    float diff, ambi;
    int z;

    ulGxSetTextureList(w->file1);
    light_update();
    boss_texlist_bind((int *)w);
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    ulMatrixPush();
    earth_rotate_yxz(w->ang);
    earth_rotate_yxz(w->ang2);
    draw_push_strip_flags();
    draw_push_override_color();
    z = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    draw_set_override_color(w->rgba);
    if (w->drawflags & 4) {
        light_get_intensity(&diff, &ambi);
        light_set_intensity(diff, 0.0f);
        draw_set_override_color(w->rgba);
        dc_object_draw(w->parts[2]);
        light_set_intensity(diff, ambi);
    }
    if (w->phase == 12) {
        final_rgba[0] = 0.0f;
        draw_set_override_color(final_rgba);
        dc_object_draw(w->parts[7]);
    } else if (w->phase == 13) {
        final_rgba[0] = (float)w->fin_count / (float)w->fin_total + -1.0f;
        draw_set_override_color(final_rgba);
        dc_object_draw(w->parts[7]);
    }
    ulMatrixPop();
    draw_pop_strip_flags();
    draw_pop_override_color();
    draw_set_override_flags(z);
    ulMatrixPop();
}

/* 0x00ca3698 */
void core_hp_reset(BossMainWork *w)
{
    int i;

    w->hp = w->thr[w->stage];
    for (i = 0; i < 4; i++)
        w->lock[i].life = w->hp;
}

/* 0x00ca36f0 */
int core_hit_check(BossMainWork *w)
{
    EffCircleArg circle;
    Vec3 pos;
    Vec3 *lock_pos;
    int i, sum = 0;

    for (i = 0; i < 4; i++) {
        LockObj *lock = &w->lock[i];

        if (!(lock->flags & LOCK_HIT))
            continue;
        circle.color = circle_c;
        circle.radius = CMD_CR;
        circle.width = CMD_CLW;
        circle.frames = circle.fade = cmd_cfrm;
        circle.n = 16;
        player_get_frame_vel(&pos);
        lock_pos = w->lock[i].pos;
        pos.x = lock_pos->x - pos.x;
        pos.y = lock_pos->y - pos.y;
        pos.z = lock_pos->z - pos.z;
        circle.pos = &pos;
        eff_circle_create(w, &circle);
        boss_se_play((int)w, i, (int)lock, 0);
        if (!w->fade)
            w->fade = cm_cfchange_create(w, (BossCfKey *)w->rgba, core_fade_keys, 4, 1.0f, 0.5f);
        sum += w->hp - lock->life;
    }
    w->hp -= sum;
    for (i = 0; i < 4; i++)
        w->lock[i].life = w->hp;
    if (w->fade && TASK_OF(w->fade)->flags & TASK_F_END)
        w->fade = 0;
    if (w->hp <= 0)
        return 4;
    if (w->thr[w->stage] < w->hp)
        return 0;
    return ++w->stage;
}

/* 0x00ca3920 */
void core_stage1_clear(BossMainWork *w)
{
    w->loops = 0;
    w->clearing = 0;
    w->arm_cmd = 5;
    boss_vol_stop(w->bgm, 13);
    boss_vol_play(w->bgm, 4, 0);
    boss_vol_play(w->bgm, 12, 0);
    oscilate_create(oscilate_x, oscilate_y);
    cubes_scatter(w);
    w->flags2 |= 0x50000;
    earth_side_break(w, 0);
    boss_se_play((int)w, 5, 0, (int)&w->pos);
    lock_score_at(1, &w->pos, 1);
    peri_act_play_boss_break();
}

/* 0x00ca39f8 */
void core_stage2_clear(BossMainWork *w)
{
    w->loops = 0;
    w->clearing = 0;
    w->arm_cmd = 5;
    boss_vol_stop(w->bgm, 13);
    boss_vol_play(w->bgm, 7, 0);
    boss_vol_play(w->bgm, 12, 0);
    oscilate_create(oscilate_x, oscilate_y);
    cubes_scatter(w);
    w->flags2 |= 0x50000;
    earth_side_break(w, 1);
    boss_se_play((int)w, 6, 0, (int)&w->pos);
    lock_score_at(2, &w->pos, 1);
    peri_act_play_boss_break();
}

/* 0x00ca3ad0 */
void core_stage3_clear(BossMainWork *w)
{
    w->loops = 0;
    w->clearing = 0;
    w->arm_cmd = 5;
    boss_vol_stop(w->bgm, 13);
    boss_vol_play(w->bgm, 10, 0);
    oscilate_create(oscilate_x, oscilate_y);
    cubes_break_all(w, 2);
    w->attack_end = 1;
    if (w->phase == 7)
        w->flags2 |= 0x70001;
    else
        w->flags2 |= 0x70000;
    lock_score_at(3, &w->pos, 1);
    peri_act_play_boss_break();
}

/* 0x00ca3ba0 */
void earth_exec_battle(BossMainWork *w)
{
    int n, ret = 0;
    int *movie;

    for (n = enemy_status_get_ticks(); n; n--) {
        if (!ret) {
            ret = earth_orbit_step(w);
            if (ret) {
                switch (ret) {
                case 1:
                    w->flags2 = (w->flags2 & ~7) | 1;
                    break;
                case 2:
                    w->flags2 = (w->flags2 & ~7) | 2;
                    break;
                }
            }
        }
        if (w->animflags & 2)
            dc_object_advance_time(w->parts[1]);
        if (w->animflags & 0x80)
            dc_object_advance_time(w->parts[7]);
    }
    earth_move(w);
    earth_turn_to_player(w);
    earth_lock_points(w);
    earth_phase_step(w);
    switch (core_hit_check(w)) {
    case 1:
        w->clearing = 1;
        w->shell_timer = cmShellBreakTime2 * FPS;
        boss_bgm_switch(w->bgm, (int)core_stage1_clear, (int)w, 0);
        break;
    case 2:
        w->clearing = 1;
        w->shell_timer = cmShellBreakTime3 * FPS;
        boss_bgm_switch(w->bgm, (int)core_stage2_clear, (int)w, 1);
        break;
    case 3:
        w->clearing = 1;
        w->shell_timer = 0;
        boss_bgm_switch(w->bgm, (int)core_stage3_clear, (int)w, 2);
        break;
    case 4:
        if (w->phase < 11)
            w->phase = 11;
        break;
    }
    console2_set_boss_rate((float)w->hp / (float)CMD_CORE_LIFE);
    if (debug_flags_test(4))
        earth_debug_keys(w);
    if (CMD_DEB_MOVIE_SW) {
        movie = deb_movies[CMD_DEB_MOVIE_NUM];
        tex_anime_start(w, movie, boss_get_arg0(), 120);
        tex_anime_fade_in_out();
        CMD_DEB_MOVIE_SW = 0;
    }
}

/* 0x00ca3ea0 */
int earth_get_attack_end(BossMainWork *w)
{
    return w->attack_end;
}

/* 0x00ca3eb8 */
int earth_damage(BossMainWork *w, int damage)
{
    int i;

    w->hp -= damage;
    for (i = 0; i < 4; i++)
        w->lock[i].life = w->hp;
    if (w->hp <= 0)
        return -1;
    return w->hp <= CMD_CORE_LIFE / 4;
}

/* 0x00ca3f40 */
int earth_arm_hit(BossMainWork *w, int deal)
{
    int dmg_state;

    if (w->clearing)
        return 0;
    if (w->far)
        return 0;
    if (w->arm_count <= 0)
        return 1;
    if (deal) {
        dmg_state = earth_damage(w, (float)(CMD_CORE_LIFE / 32)
                                       * (8.0f / (float)arm_damage_rata[w->level]) + 1.0f);
        if (!w->arm_count && !dmg_state) {
            cubes_scatter(w);
            w->flags2 |= 0x10000;
        }
    }
    do {
        w->arm_count--;
    } while (0);
    return 1;
}

/* 0x00ca4060 */
int earth_arm_add(BossMainWork *w)
{
    w->arm_count++;
    return 1;
}

/* 0x00ca4080 */
int earth_get_level(BossMainWork *w)
{
    return w->level;
}

/* 0x00ca4088 */
int earth_is_defeated(BossMainWork *w)
{
    return w->phase >= 10;
}

/* 0x00ca40a8 */
void earth_debug_keys(BossMainWork *w)
{
    if (kbd_state[7] & 0x44) {
        switch (kbd_state[5]) {
        case 21:
        case 22:
            earth_restart(w);
            break;
        case 17:
            earth_damage(w, CMD_CORE_LIFE / 4 + 1);
            break;
        case 8:
            sound_debug_toggle();
            break;
        }
    }
}

/* 0x00ca4140 */
void cube_light_draw_frames(int *ang, unsigned int color, int n, float z, float dz, float half)
{
    Vec3 corner[4];
    Vec3 off;
    int i, j;
    Vec3 *edge_a, *edge_b, *pt;

    v3_set(&corner[0], -half, half, z);
    pt = &corner[1];
    pt->x = half;
    pt->y = half;
    corner[1].z = z;
    v3_set(&corner[2], half, -half, z);
    pt = &corner[3];
    pt->x = -half;
    pt->y = -half;
    pt->z = z;
    off.x = off.y = 0.0f;
    off.z = dz;
    cube_light_vtx[3].color = color;
    cube_light_vtx[2].color = color;
    cube_light_vtx[1].color = color;
    cube_light_vtx[0].color = color;
    for (; n; n--) {
        ulMatrixPush();
        earth_rotate_yxz(ang);
        for (i = 0; i < 4; i++) {
            j = (i + 1) % 4;
            edge_a = &corner[i];
            edge_b = &corner[j];
            v3_copy((Vec3 *)&cube_light_vtx[0], edge_a);
            v3_sum((Vec3 *)&cube_light_vtx[1], edge_a, &off);
            v3_copy((Vec3 *)&cube_light_vtx[2], edge_b);
            v3_sum((Vec3 *)&cube_light_vtx[3], edge_b, &off);
            poly_draw_tex(cube_light_vtx, 4);
        }
        ulMatrixPop();
        ang += 3;
    }
}

/* 0x00ca4310 */
void circle_light_disp(BossCircleLight *w)
{
    Vec3 spos;
    float k;
    int old_flags, old_fc, old_ztest, old_test;
    float *layout;

    ulGxSetTextureList(w->list);
    old_flags = draw_set_override_flags(0);
    k = (1.0f - (float)w->count / (float)w->total) * w->size;
    draw_set_blend(4, 1);
    old_fc = draw_set_fc_mode(8192);
    old_ztest = draw_set_ztest_mode(0x60000);
    old_test = draw_set_test_mode(0);
    ulGxSelectTexture(ulGxGetTextureList(), w->tex);
    nj_tex_strip_begin(1);
    ulMatrixApplyPoint((float *)ulMatrixCur, (float *)w->pos, (float *)&spos);
    project_point(&spos, &spos);
    layout = screen_layout;
    w->v[0].x = w->v[1].x = spos.x - layout[2] * k;
    w->v[2].x = w->v[3].x = spos.x + layout[2] * k;
    w->v[0].y = w->v[2].y = spos.y - layout[2] * k;
    w->v[1].y = w->v[3].y = spos.y + layout[2] * k;
    nj_tex_strip_vertices(w->v, 4);
    nj_tex_strip_end();
    draw_set_ztest_mode(old_ztest);
    draw_set_test_mode(old_test);
    draw_set_fc_mode(old_fc);
    draw_set_override_flags(old_flags);
}

/* 0x00ca4498 */
void circle_light_exec(BossCircleLight *w)
{
    switch (w->state) {
    case 0:
        if (--w->delay > 0)
            return;
        w->state = 1;
        TASK_OF(w)->disp = (void (*)(void *))circle_light_disp;
    case 1:
        if (--w->count > 0)
            return;
        FINISH_IF(w);
    }
}

/* 0x00ca4508 */
BossCircleLight *circle_light_create(void *parent, Vec3 *pos, int tex, float secs, float delay, float size)
{
    BossCircleLight *w = task_create_inherit(parent, &circle_light_desc, 132);

    w->size = size;
    w->pos = pos;
    w->delay = delay * FPS;
    w->tex = tex;
    w->v[0].q = w->v[1].q = w->v[2].q = w->v[3].q = 0.95f;
    w->v[0].color = w->v[1].color = w->v[2].color = w->v[3].color = COL_WHITE;
    w->v[0].u = w->v[1].u = w->v[0].v = w->v[2].v = 0.0f;
    w->v[1].v = w->v[2].u = w->v[3].u = w->v[3].v = 2.0f;
    w->count = w->total = secs * FPS;
    w->state = 0;
    return w;
}

/* 0x00ca4608 */
void cube_light_init(BossCubeLight *w, BossCubeLight *arg)
{
    i3_copy(w->ang2, arg->ang2);
    v3_copy(&w->scl, &arg->scl);
    w->count0 = w->count = arg->count;
    w->z = arg->z;
    w->dz = arg->dz;
    w->total = arg->total;
    w->state = 0;
    w->pos = arg->pos;
    w->ang = arg->ang;
    w->r = arg->r;
    w->tex = arg->tex;
}

/* 0x00ca4698 */
void cube_light_exec(BossCubeLight *w)
{
    if (w->state == 0) {
        if (--w->count > 0)
            return;
        w->state = 1;
        w->count = w->total;
        return;
    }
    if (--w->count > 0)
        return;
    FINISH_IF(w);
}

/* 0x00ca46f0 */
void cube_light_disp(BossCubeLight *w)
{
    float k;
    unsigned int alpha;
    int z;

    ulGxSetTextureList(w->list);
    z = draw_set_override_flags(0);
    ulMatrixPush();
    MTX_TRANS(w->pos);
    earth_rotate_yxz(w->ang);
    vu0_matrix_scale(ulMatrixCur, w->scl.x, w->scl.y, w->scl.z);
    draw_set_blend(4, 1);
    ulGxSelectTexture(ulGxGetTextureList(), w->tex);
    tex_strip_begin(1);
    switch (w->state) {
    case 0:
        k = 1.0f - (float)w->count / (float)w->count0;
        cube_light_draw_frames(w->ang2, COL_WHITE, 1, w->z, w->dz * k * k, w->r);
        break;
    case 1:
        k = (float)w->count / (float)w->total;
        alpha = (int)(k * 255.0f) & 0xff;
        cube_light_draw_frames(w->ang2, (alpha + 1) >> 1 << 24 | 0x808080, 1, w->z, w->dz, w->r);
        break;
    }
    tex_strip_end();
    ulMatrixPop();
    draw_set_override_flags(z);
}

/* 0x00ca4888 */
BossCubeLight *cube_light_create_arg(void *parent, BossCubeLight *arg)
{
    return task_create_inherit_arg(parent, cube_light_desc, 72, arg);
}

/* 0x00ca48b0 */
BossCubeLight *cube_light_create(void *parent, Vec3 *pos, int *ang, int *frame_ang, int tex)
{
    BossCubeLight light;
    float t = cube_light_time * FPS;

    light.pos = pos;
    light.ang = ang;
    light.ang2[0] = frame_ang[0];
    light.ang2[1] = frame_ang[1];
    light.ang2[2] = frame_ang[2];
    light.count = t;
    light.total = t / 3.0f;
    light.z = cube_light_z;
    light.dz = cube_light_dz;
    light.r = cube_light_r;
    light.tex = tex;
    light.scl.x = light.scl.y = light.scl.z = 1.0f;
    return task_create_inherit_arg(parent, cube_light_desc2, 72, &light);
}

/* 0x00ca4978 */
void earth_side_break(BossMainWork *w, int side)
{
    BossCubeLight light;
    Vec3 *pos;
    int i, pitch;

    if (!side) {
        w->drawflags &= ~0x40000000;
        pitch = -ANG_90;
    } else {
        w->drawflags &= ~0x80000000;
        pitch = ANG_90;
    }
    for (i = 0; i < 4; i++) {
        light.pos = &w->lockpos[i];
        light.ang = w->ang;
        light.ang2[0] = pitch;
        light.ang2[2] = 0;
        light.ang2[1] = DEG_TO_ANG((float)(i * 90));
        light.count = cube_light_time / 3.0f * FPS;
        light.dz = cube_light_dz * 8.0f;
        light.z = 0.0f;
        light.total = cube_light_time / 3.0f * FPS;
        light.tex = 2;
        light.r = cube_light_r;
        light.scl.x = 2.0f;
        light.scl.z = 1.0f;
        light.scl.y = 1.0f;
        cube_light_create_arg(w, &light);
    }
    cm_wire_break_create(w, 5, w->parts[5], &w->pos, 0, w->ang, 0, wire_colors[2],
                           side_break_life, side_break_speed, 1.0f, 0);
}

/* 0x00ca4b88 */
void move_light_exec(BossMoveLight *w)
{
    Vec3 pt, pl_vec;
    float k;
    int i;

    w->acc += w->speed;
    if (w->acc >= 1.0f) {
        w->acc -= 1.0f;
        w->head = (w->head + 1) % 32;
    }
    player_get_vec(&pl_vec);
    v3_add(&w->org, &pl_vec);
    for (i = 0; i < 32; i++)
        v3_sum(&w->pts[i], &pl_vec, &w->pts[i]);
    switch (w->state) {
    case 0:
        k = (float)--w->count / (float)w->total;
        v3_lerp(&pt, &w->org, w->target, k);
        v3_sum(&w->pts[w->head], &w->dir, &pt);
        if (k < w->frac)
            w->state = 1;
        break;
    case 1:
        k = (float)--w->count / (float)w->total;
        v3_lerp(&w->pts[w->head], &w->org, w->target, k);
        k = (float)--w->count2 / (float)w->total2;
        v3_mul(&pt, &w->dir, k);
        v3_sum(&w->pts[w->head], &pt, &w->pts[w->head]);
        if (w->count2 <= 0)
            w->state = 2;
        break;
    case 2:
        k = (float)--w->count / (float)w->total;
        v3_lerp(&w->pts[w->head], &w->org, w->target, k);
        if (w->count <= 0) {
            w->state = 3;
            w->count = 32.0f / w->speed;
        }
        break;
    case 3:
        v3_copy(&w->pts[w->head], w->target);
        if (--w->count <= 0)
            FINISH_IF(w);
        break;
    }
}

/* 0x00ca4fe0 */
void move_light_disp(BossMoveLight *w)
{
    ulGxSetTextureList(w->list);
    draw_set_blend(4, 1);
    strip_draw(w->pts, w->head, 32, COL_WHITE, w->tex, move_light_width);
}

/* 0x00ca5038 */
BossMoveLight *move_light_create(void *parent, Vec3 *from, Vec3 *target, int tex, float secs, float secs2, float speed)
{
    BossMoveLight *w = task_create_inherit(parent, move_light_desc, 460);
    Vec3 dir;
    Vec3 *p;
    int i;

    w->tex = tex;
    w->target = target;
    w->head = 0;
    w->count = w->total = secs * FPS;
    v3_copy(&w->org, from);
    w->count2 = w->total2 = secs2 * FPS;
    w->frac = RAND_FLOAT_A() * (move_light_frac_max - move_light_frac_min) + move_light_frac_min;
    w->speed = speed;
    v3_sub(&dir, w->target, &w->org);
    vec_up_for_dir(&dir, &w->dir, DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f));
    v3_scale(&w->dir, move_light_side);
    for (p = w->pts, i = 0; i < 32; i++, p++)
        v3_sum(p, &w->org, &w->dir);
    return w;
}

/* 0x00ca5270 */
void move_light_set_exec(BossMoveLightSet *w)
{
    Vec3 v;
    float sc[4];

    if (--w->timer > 0)
        return;
    w->timer = w->interval;
    v.y = move_light_dist;
    v.x = v.z = 0.0f;
    ROT_X(DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f), sc, v.y, v.z);
    ROT_Y(DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f), (sc + 2), v.x, v.z);
    v3_sum(&v, w->pos, &v);
    move_light_create(w->parent, &v, w->pos, w->tex, move_light_time, move_light_time2, move_light_speed);
    if (--w->count > 0)
        return;
    FINISH_IF(w);
}

/* 0x00ca5468 */
BossMoveLightSet *move_light_set_create(void *parent, Vec3 *pos, int tex, int count, float secs)
{
    BossMoveLightSet *w = task_create_inherit(parent, move_light_set_desc, 32);

    w->count = count;
    w->pos = pos;
    w->parent = parent;
    w->tex = tex;
    w->interval = w->timer = secs * FPS;
    return w;
}

/* 0x00ca54f8 */
void earth_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
