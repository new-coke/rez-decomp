/*
 * Uranus boss core. The pit task brings the core in; as it closes it switches the beat, shows the
 * gauge and makes the core lockable. The dcube task is the grid of small cubes around it, each
 * bursting when shot. The cm task plays the defeat: score, boss name, core shatter, fade out.
 */

#include "types_gw.h"
#include "b_uranus.h"
#include "mathdef.h"
#include "camera_demo.h"
#include "debug.h"
#include "fade.h"
#include "gw_varedit.h"
#include "heap.h"
#include "model.h"
#include "node_draw_state.h"
#include "pad_vib.h"
#include "player.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "uranus_core.h"

#define DCUBE_COL RGBA(0x14, 0, 0x28, 0x80)
#define CORE_END_COL RGBA(0x32, 0, 0, 0x80)
#include "uranus.h"
#include "uranus_map.h"
#include "uranus_part.h"
#include "uranus_snake.h"
#include "../common/boss_common.h"
#include "../common/boss_demo_mes.h"
#include "../common/boss_texture_anime.h"

extern char uranus_core_vardef[];
extern GwVarDef uranus_core_vardefs[];
extern struct GwVarNode *uranus_core_vars_group;
extern int uranus_cm_blur_num;
extern int uranus_cm_blur_step;
extern float uranus_cm_blur_len;
extern float uranus_cm_blur_alpha;
extern int uranus_core_deb_sw;
extern char str_uranus_pit_fmt[];
extern int uranus_cm_frm;
extern float uranus_cm_fog_sec;
extern struct TaskDesc uranus_cm_desc;
extern char str_uranus_core_c[];
extern struct TaskDesc uranus_dcube_desc;
extern int uranus_dcube_nx;
extern int uranus_dcube_ny;
extern int uranus_dcube_nz;
extern int uranus_dcube_holes;
extern float uranus_dcube_dx;
extern float uranus_dcube_dy;
extern float uranus_dcube_dz;
extern float uranus_dcube_ofs_x;
extern float uranus_dcube_ofs_y;
extern float uranus_dcube_ofs_z;

/* the core's pit form, 0xa4 bytes */
typedef struct UranusPitWork {
    char pad_00[12];
    UranusPartWork *pm;         /* 0x0c */
    struct UranusMapWork *map;
    int got_pos;
    int nlinks;
    UranusNode *nodes;
    Vec3 *ofs;                  /* per link, a random grid offset */
    Vec3 pos;
    Vec3 pos_from;
    Vec3 entry;
    Vec3 vel;
    Vec3 approach;
    Vec3 start;
    int ang[3];
    float wobble;
    float dist0;
    float rate;                 /* distance left to the player, 1 at the start */
    int wait;
    int from_core;
    int flown;
    int blurred;
    int cue_ms;
    int cue_se;
    int cue_lock;
    ModelPart **parts;
} UranusPitWork;

/* dcube grid cube, 0x50 bytes */
typedef struct UranusDcubePoint {
    Vec3 p;
    LockObj lock;
    int on;
} UranusDcubePoint;

/* grid of cubes round the pit, 0x24 bytes */
typedef struct UranusDcubeWork {
    char pad_00[4];
    void *textures;             /* 0x04 */
    UranusDcubePoint *cubes;
    int ncubes;
    int form;
    int wait;
    UranusPartWork *pm;
    struct UranusMapWork *map;
    ModelPart **parts;
} UranusDcubeWork;

/* the exposed core's end, 0x5c bytes */
typedef struct UranusCmWork {
    char pad_00[4];
    void *textures;             /* 0x04 */
    UranusPartWork *pm;
    CmObjMotWork *mot;
    ModelPart **parts;
    Vec3 pos;
    Vec3 base;
    int ang[3];
    Vec3 vel;
    float wobble;
    float amp;
    int wait;
    int wait_max;
    int state;
    int beats;
} UranusCmWork;

extern float cmd_pit_core_sr;
extern float cmd_core_bz;
extern float uranus_cm_shake_pow;
extern float uranus_cm_shake_rate;
extern char str_uranus_cm_name[];
extern struct TaskDesc uranus_pit_desc;
extern float cmd_pit_st_wz;
extern float cmd_pit_off_z;
extern float cmd_pit_set_z;
extern float cmd_pit_set_y;
extern float cmd_pit_st_len;
extern float cmd_pit_st_len2;
extern float cmd_pit_near_vr;
extern float cmd_pit_sw_rd;
extern float cmd_pit_map_v;
extern float cmd_core_pit_z;
extern float uranus_pit_core_z_rate;
extern float cmd_core_pit_zr;
extern float cmd_pit_pf_len;
extern float cmd_pit_pf_rate;
extern float cmd_pit_pf_prate;
extern float cmd_pit_ms_rate;
extern float cmd_pit_se_rate;
extern float cmd_pit_lock_rate;

/* 0x00ca3738 */
void uranus_core_vars_add(void)
{
    uranus_core_vars_group = gw_varedit_add(&gw_varedit_root, 0, uranus_core_vardef);
    gw_varedit_add_table(&gw_varedit_root, uranus_core_vars_group, uranus_core_vardefs);
}

/* 0x00ca3788 */
void uranus_core_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)uranus_core_vardef);
}

/* 0x00ca37a0 */
void uranus_core_beat_tick(void)
{
    if (!light_mode_is_fog())
        peri_act_beat_tick();
    boss_vol_play(1, 0);
}

/* 0x00ca37d8 */
void uranus_pit_exec(UranusPitWork *w)
{
    Vec3 v, scroll, pl, tgt, to_pl, to_pl2, pl_tgt, sway, wob;
    float ease_sin, ease_cos;
    Vec3 *pa;
    int *pb;
    UranusNode *first, *node, *tip;
    Vec3 *ofs;
    float lim, curve;

    player_get_pos(&pl);
    if (uranus_map_is_seg_end(w->map) && w->wait)
        w->wait--;
    uranus_map_get_drift(&v);
    player_get_frame_vel(&scroll);
    v3_addto(&w->pos, &v);
    v3_addto(&w->entry, &v);
    v3_addto(&w->start, &v);
    v3_addto(&w->pos_from, &scroll);
    if (!w->from_core)
        lim = cmd_pit_st_len;
    else
        lim = cmd_pit_st_len2;
    v3_sub(&to_pl, &w->start, &pl);
    if (!w->wait && vec3_length(&to_pl) < lim) {
        uranus_pm_pit_reached(w->pm, (int)w);
        uranus_pm_set_lockable(w->pm, 1);
    }
    v3_sub(&to_pl2, &w->entry, &pl);
    w->rate = vec3_length(&to_pl2) / w->dist0;
    ulSinCosf(w->rate * PI, &ease_sin, &ease_cos);
    curve = ease_cos * -0.5f + 0.5f;
    node = w->nodes->child;
    first = node;
    if (!w->from_core) {
        if (!w->wait) {
            player_get_pos(&tgt);
            tgt.z += cmd_core_pit_z + uranus_pit_core_z_rate * curve;
            v3_lerp(&w->pos, &w->pos, &tgt, cmd_core_pit_zr);
        }
        if (w->rate < cmd_pit_ms_rate && !w->cue_ms)
            w->cue_ms = 1;
        if (w->rate < cmd_pit_se_rate && !w->cue_se) {
            w->cue_se = 1;
            boss_bgm_switch((int)uranus_core_beat_tick, (int)w, 0);
        }
        if (w->rate < cmd_pit_lock_rate && !w->cue_lock) {
            w->cue_lock = 1;
            uranus_pm_set_gauge(w->pm, 1);
            uranus_pm_set_lockable(w->pm, 1);
        }
    } else {
        if (cmd_pit_pf_rate < w->rate) {
            player_get_pos(&pl_tgt);
            pl_tgt.z += cmd_pit_pf_len;
        } else {
            v3_copy(&pl_tgt, &w->entry);
            if (!w->blurred) {
                uranus_pm_get_core(w->pm, &pa, &pb);
                uranus_cm_blur_create(w->pm, pa, pb, w->parts);
                w->blurred = 1;
            }
        }
        point_approach(&w->pos_from, &pl_tgt, &w->pos, &w->approach, 1.0f - w->rate);
        if (!w->flown && w->rate < cmd_pit_pf_prate) {
            uranus_pm_set_lockable(w->pm, 0);
            uranus_map_set_spd(w->map, cmd_pit_map_v);
            cm_sc_blur_create(w, 1.0f, 200.0f, 0.0f, 1.5f);
            boss_pl_di_create(w, 0, 0);
            w->flown = 1;
        }
    }
    wob.z = 0.0f;
    wob.x = cmd_pit_core_sr;
    wob.y = cmd_pit_core_sr;
    wobble_step(&sway, &wob, &w->wobble, cmd_pit_sw_rd);
    v3_sum((Vec3 *)first->data, &sway, &w->pos);
    ofs = w->ofs;
    for (node = first->child; node; ofs++, node = node->next) {
        v3_add((Vec3 *)node->data, &v);
        if (((Vec3 *)first->data)->z < ((Vec3 *)node->data)->z) {
            v3_sum(&pl_tgt, (Vec3 *)first->data, ofs);
            v3_sub(&pl_tgt, &pl_tgt, (Vec3 *)node->data);
            v3_scale(&pl_tgt, cmd_pit_near_vr);
            v3_add((Vec3 *)node->data, &pl_tgt);
            for (tip = node->child; tip; tip = tip->next)
                v3_copy((Vec3 *)tip->data, (Vec3 *)node->data);
        }
    }
}

/* 0x00ca3e88 */
void uranus_pit_disp(UranusPitWork *w)
{
    if (debug_flags_test(4) && uranus_core_deb_sw) {
        debug_set_origin(10, 42);
        debug_printf(str_uranus_pit_fmt, (int)(w->rate * 100.0f));
    }
}

/* 0x00ca3f00 */
void uranus_pit_release(UranusPitWork *w)
{
    uranus_node_free_tree(w->nodes);
    heap_free(w->ofs, 0);
}

/* 0x00ca3f30 */
UranusPitWork *uranus_pit_create(void *parent, UranusPartWork *pm, Vec3 **pts, UranusNode **out, Vec3 *pos, UranusNode *shape, ModelPart **parts)
{
    UranusPitWork *w = task_create_inherit(parent, &uranus_pit_desc, 164);
    Vec3 start, pl;
    UranusNode *head, *node, *tip;
    Vec3 *tip_pt, *node_pt;
    Vec3 *ofs;
    int i;

    w->parts = parts;
    w->pm = pm;
    w->map = uranus_pm_get_map(pm);
    w->nodes = uranus_node_new();
    uranus_node_copy_tree(w->nodes, shape);
    i = uranus_node_count_children(w->nodes->child);
    head = w->nodes->child;
    head->data = (int)pts[0];
    *(void **)&w->nlinks = (void *)i; /* steer(type) */
    node_pt = pts[1];
    tip_pt = pts[2];
    if (pos) {
        float z;

        player_get_frame_vel(&start);
        v3_sub(&start, pos, &start);
        z = 0.0f;
        w->start.z = cmd_pit_off_z + cmd_pit_set_z + start.z;
        w->wait = 1;
        w->from_core = 1;
        w->start.y = z;
        w->start.x = z;
        w->wobble = RAND_FLOAT_A() * 2.0f * PI;
    } else {
        float z;

        z = 0.0f;
        w->start.z = cmd_pit_off_z + cmd_pit_set_z - cmd_pit_st_wz;
        w->from_core = 0;
        w->start.x = w->start.y = z;
        v3_copy(&start, &w->start);
        w->wait = 2;
    }
    v3_copy(&w->entry, &w->start);
    w->entry.y += cmd_pit_set_y;
    v3_copy(&w->pos_from, &start);
    v3_copy(&w->pos, &start);
    v3_copy((Vec3 *)head->data, &start);
    player_get_pos(&pl);
    v3_sub(&pl, &w->start, &pl);
    w->dist0 = vec3_length(&pl);
    w->ofs = heap_block_tag(game_calloc(w->nlinks * sizeof(Vec3)), str_uranus_core_c, 362);
    for (ofs = w->ofs, i = w->nlinks; i; i--, ofs++) {
        ofs->x = uranus_dcube_ofs_x + (int)(RAND_FLOAT_A() * uranus_dcube_nx) * uranus_dcube_dx - uranus_dcube_dx * 0.5f * uranus_dcube_nx;
        ofs->y = uranus_dcube_ofs_y + (int)(RAND_FLOAT_A() * uranus_dcube_ny) * uranus_dcube_dy - uranus_dcube_dy * 0.5f * uranus_dcube_ny;
        ofs->z = uranus_dcube_ofs_z + (int)(RAND_FLOAT_A() * uranus_dcube_nz) * uranus_dcube_dz - uranus_dcube_dz * 0.5f * uranus_dcube_nz;
    }
    {
        Vec3 *pt_ofs = w->ofs;

        for (node = w->nodes->child->child; node; node = node->next) {
            node->data = (int)node_pt;
            node->flags = 1;
            v3_sum(node_pt, &w->start, pt_ofs);
            pt_ofs++;
            node_pt++;
            for (tip = node->child; tip; tip = tip->next) {
                do { tip->data = (int)tip_pt; } while (0); /* steer(barrier) */
                tip->flags = 1;
                v3_copy(tip_pt, (Vec3 *)node->data);
                tip_pt++;
            }
        }
    }
    if (out)
        *out = w->nodes->child;
    uranus_dcube_create(parent, w->pm, &w->start, w->parts, uranus_pm_get_models(w->pm), w->wait + 1);
    return w;
}

/* 0x00ca4458 */
void uranus_pit_get_pos(UranusPitWork *w, Vec3 *pos, Vec3 *vel, int *ang)
{
    if (pos) {
        v3_copy(&w->pos, pos);
        v3_copy((Vec3 *)w->nodes->child->data, pos);
    }
    if (vel)
        v3_copy(&w->vel, vel);
    if (ang)
        i3_copy(w->ang, ang);
    w->got_pos = 1;
}

/* 0x00ca44f0 */
void uranus_dcube_exec(UranusDcubeWork *w)
{
    UranusDcubePoint *cube = w->cubes;
    Vec3 v;
    Vec3 zero;
    int zero_ang[3];
    int i;

    if (uranus_map_is_seg_end(w->map) && !--w->wait) {
        task_end_with_exit(w);
        return;
    }
    uranus_map_get_drift(&v);
    for (i = 0; i < w->ncubes; i++, cube++) {
        v3_add(&cube->p, &v);
        if (cube->on && (cube->lock.flags & LOCK_HIT) && cube->lock.life <= 0) {
            zero.x = 0.0f;
            zero.y = 0.0f;
            zero.z = 0.0f;
            zero_ang[0] = 0;
            zero_ang[1] = 0;
            zero_ang[2] = 0;
            venus_zxyb_create(w->pm, &cube->p, &zero, zero_ang, 4.0f, 4.0f, 0.2f, 4.5f);
            cube->on = 0;
        }
    }
}

/* 0x00ca4630 */
void uranus_dcube_disp(UranusDcubeWork *w)
{
    UranusDcubePoint *cube = w->cubes;
    int i, old_blend, old_list;
    float old_lw;

    ulGxSetTextureList(w->textures);
    old_blend = draw_set_override_flags(0);
    old_lw = line_width_set(2.0f);
    draw_set_color(DCUBE_COL);
    for (i = 0; i < w->ncubes; i++, cube++) {
        ulMatrixPush();
        do {
            vu0_matrix_translate(ulMatrixCur, cube->p.x, cube->p.y, cube->p.z);
        } while (0);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_scale(ulMatrixCur, 1.5f, 1.5f, 1.5f);
        if (cube->on)
            dc_object_draw((struct MapPart *)w->parts[w->form * 6 + 2]);
        old_list = RenderSetCurrentDisplayList(6);
        ulGxSetTextureList(w->textures);
        draw_set_color(DCUBE_COL);
        draw_set_blend(4, 5);
        dc_object_draw((struct MapPart *)w->parts[w->form * 6 + 3]);
        RenderSetCurrentDisplayList(old_list);
        ulMatrixPop();
    }
    line_width_set(old_lw);
    draw_set_override_flags(old_blend);
}

/* 0x00ca47b0 */
void uranus_dcube_release(UranusDcubeWork *w)
{
    int i;

    for (i = 0; i < w->ncubes; i++)
        ;
    heap_free(w->cubes, 0);
}

/* 0x00ca47e8 */
UranusDcubeWork *uranus_dcube_create(void *parent, UranusPartWork *pm, Vec3 *pos, ModelPart **parts, int form, int wait)
{
    UranusDcubeWork *w = task_create_inherit(parent, &uranus_dcube_desc, 36);
    Vec3 org;
    UranusDcubePoint *cube;
    float x, y, z;
    int i, j, k;

    org.x = pos->x + uranus_dcube_ofs_x;
    org.y = pos->y + uranus_dcube_ofs_y;
    w->form = form;
    w->pm = pm;
    org.z = pos->z + uranus_dcube_ofs_z;
    w->wait = wait + 1;
    w->map = uranus_pm_get_map(pm);
    w->parts = parts;
    w->ncubes = uranus_dcube_nx * uranus_dcube_ny * uranus_dcube_nz;
    w->cubes = heap_block_tag(game_calloc(w->ncubes * sizeof(UranusDcubePoint)), str_uranus_core_c, 535);
    cube = w->cubes;
    z = 0.0f;
    for (i = 0; i < uranus_dcube_nz; i++) {
        x = uranus_dcube_dx * -0.5f * uranus_dcube_nx;
        for (j = 0; j < uranus_dcube_nx; j++) {
            y = uranus_dcube_dy * -0.5f * uranus_dcube_ny;
            for (k = 0; k < uranus_dcube_ny; k++) {
                v3_copy(&cube->p, &org);
                cube->on = 1;
                cube->p.x += x;
                cube->p.y += y;
                cube->p.z += z;
                y += uranus_dcube_dy;
                cube++;
            }
            x += uranus_dcube_dx;
        }
        z += uranus_dcube_dz;
    }
    {
        int n;

        for (n = 0; n < uranus_dcube_holes; n++)
            w->cubes[(int)(RAND_FLOAT_A() * w->ncubes)].on = 0;
    }
    return w;
}

/* 0x00ca4ab0 */
void uranus_cm_exec(UranusCmWork *w)
{
    Vec3 scroll, to_pl;
    float rate;

    player_get_frame_vel(&scroll);
    v3_add(&w->base, &scroll);
    if (w->mot && (TASK_OF(w->mot)->flags & TASK_F_END))
        w->mot = 0;
    player_get_pos(&to_pl);
    to_pl.z += cmd_core_bz;
    to_pl.x = to_pl.y = 0.0f;
    v3_sub(&to_pl, &to_pl, &w->base);
    if (w->wait)
        w->wait--;
    rate = 1.0f - (float)w->wait / (float)w->wait_max;
    v3_scale(&to_pl, rate);
    v3_addto(&w->base, &to_pl);
    w->amp = w->amp * 0.99f + cmd_pit_core_sr * 0.01f;
    wobble_step_slow(&scroll, w->amp, &w->wobble);
    v3_sum(&w->pos, &w->base, &scroll);
    switch (w->state) {
    case 0:
        w->state++;
        break;
    case 1:
        if (!beat_grid_hit(0) || (beat_clock_get_haku() & 3))
            break;
        if (--w->beats == 1) {
            w->mot = cm_obj_mot_create(w, &w->pos, w->ang, w->parts[33], 1, 0.0f, 1.0f);
        } else if (w->beats <= 0) {
            w->beats = 2;
            tex_anime_fade_in_out();
            w->state++;
        }
        break;
    case 2:
        if (!beat_grid_hit(0) || (beat_clock_get_haku() & 3))
            break;
        if (--w->beats > 0)
            break;
        peri_act_play_boss_finish();
        lock_score_level(3, &w->pos);
        if (boss_get_scoring())
            boss_demo_mes2_start(boss_get_task(), str_uranus_cm_name, 1);
        else
            boss_demo_mes2_start(boss_get_task(), str_uranus_cm_name, 3);
        oscilate_create(uranus_cm_shake_pow, uranus_cm_shake_rate);
        TASK_OF(w)->disp = 0;
        cm_wire_break_simple(w->pm, 38, w->parts[30], &w->pos, &w->vel, w->ang, CORE_END_COL);
        fade_start(2, 0, 30);
        w->wait = 30;
        w->state++;
        break;
    case 3:
        if (!beat_grid_hit(0) || (beat_clock_get_haku() & 3))
            break;
        w->state++;
    case 4:
        if (--w->wait > 0)
            break;
        fade_start_out(60);
        FINISH_IF(w);
        break;
    }
}

/* 0x00ca4e90 */
void uranus_cm_disp(UranusCmWork *w)
{
    int old_blend;

    ulGxSetTextureList(w->textures);
    old_blend = draw_set_override_flags(0);
    ulMatrixPush();
    if (w->state < 3) {
        MTX_TRANS(&w->pos);
        uranus_core_rotate_yxz(w->ang);
        dc_object_draw((struct MapPart *)w->parts[24]);
        draw_set_color(CORE_END_COL);
        dc_object_draw((struct MapPart *)w->parts[25]);
    }
    ulMatrixPop();
    draw_set_override_flags(old_blend);
}

/* 0x00ca4f30 */
UranusCmWork *uranus_cm_create(void *parent, UranusPartWork *pm, Vec3 *pos, int *ang, ModelPart **parts)
{
    UranusCmWork *w = task_create_inherit(parent, &uranus_cm_desc, 92);
    Vec3 scroll;

    w->pm = pm;
    player_get_frame_vel(&scroll);
    v3_sub(&w->base, pos, &scroll);
    i3_copy(w->ang, ang);
    w->wait_max = w->wait = (int)*(void **)&uranus_cm_frm; /* steer(alias) */
    w->parts = parts;
    uranus_cm_blur_create(w, &w->pos, w->ang, parts);
    venus_fog_create(w->pm, 0, uranus_cm_fog_sec);
    w->beats = 2;
    return w;
}

/* 0x00ca5038 */
CmObjectBlurWork *uranus_cm_blur_create(void *parent, Vec3 *pos, int *ang, ModelPart **parts)
{
    return cm_object_blur_create(parent, pos, ang, parts[24], uranus_cm_blur_num, uranus_cm_blur_step, uranus_cm_blur_len, uranus_cm_blur_alpha);
}

/* 0x00ca5078 */
void uranus_core_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
