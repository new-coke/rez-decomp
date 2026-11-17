/*
 * Uranus boss course: the scrolling tunnel, a ring of segments picked at random from 30 models.
 * Moves player and camera along the segment motions, gives tasks the scroll and drift vectors and
 * checks segment collision. A lockable object in a segment drops a row of items when shot.
 */

#include "types_gw.h"
#include "b_uranus.h"
#include "mathdef.h"
#include "anim.h"
#include "camera_demo.h"
#include "config.h"
#include "debug.h"
#include "game.h"
#include "gw_varedit.h"
#include "heap.h"
#include "item.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "node_draw_state.h"
#include "player.h"
#include "player_camera.h"
#include "pnb.h"
#include "render.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "uranus_map.h"
#include "uranus.h"
#include "uranus_snake.h"
#include "../common/boss_common.h"
#include "../common/boss_wire.h"

extern Vec3 uranus_map_dir;
extern Vec3 uranus_map_scroll;
extern Vec3 uranus_map_drift;
extern int test_map_v;
extern float test_map_speed;
extern float cmd_map_frm_d;
extern char uranus_map_motion[];
extern void *uranus_map_pose_outs[];
extern Vec3 uranus_map_pose_pos;
extern int uranus_map_pose_ang[3];
extern struct TaskDesc uranus_map_item_desc;

/* uranus_map_seg_srcs entry, 0x50 bytes: pnb entry numbers of one course segment */
typedef struct UranusMapSegSrc {
    int obj_kind;
    int mot;
    int mot_b;
    int solid;
    int wire;
    int wire_data;
    int col;
    Vec3 prev_ofs;
    Vec3 next_ofs;
    int cam_mot;
    int event;                  /* 1 = place an object, 2 = start a camera */
    int obj;
    int obj_wire;
    char *courses;              /* nonzero per course the segment appears in */
    unsigned int cam_arg[2];
} UranusMapSegSrc;

/* one course segment, 0x22c bytes */
typedef struct UranusMapSeg {
    int mot;
    int mot_b;
    int cam_mot;
    float frames;
    float len;
    Vec3 next_ofs;
    Vec3 prev_ofs;
    ModelPart pool[5];
    ModelPart *solid;
    ModelPart *wire;
    ModelPart *col;
    ModelPart *obj;
    ModelPart *obj_wire;
    int id;
    int obj_kind;
    int ncols;
    int (*cols)[16];
    int event;
    unsigned int cam_arg[2];
    char *courses;
} UranusMapSeg;

/* 272 bytes */
typedef struct UranusMapWork {
    int models;
    void *textures;
    ModelPart **parts;
    float t;
    float spd;
    float t_prev;
    float t_prev2;
    float fade;
    float ending;
    int unk_24;                 /* 0x24: cleared when the ring advances, never read */
    Vec3 pos;
    int ang[3];
    char pad_40[8];
    int roll;                   /* 0x48 */
    Vec3 *unk_4c;               /* 0x4c: the boss position (uranus_map_create), never read */
    UranusMapSeg *segs;
    UranusMapSeg *seg;
    UranusMapSeg *ring[30];
    int ring_head;
    int ring_cur;
    int nsegs;
    int seg_wait;
    int seg_end;
    int hold;
    int held;
    int next[3];
    int nnext;
    int next_wait;
    int show_prev;
    int show_cur;
    int show_next;
    int disp_wait;
} UranusMapWork;

/* item pickup, 0x44 bytes: spawns its item when the player comes near */
typedef struct UranusMapItemWork {
    char pad_00[8];
    ModelPart **parts;          /* 0x08 */
    UranusMapWork *map;
    Vec3 pos;
    Vec3 pos0;
    char pad_28[24];
    int life;                   /* 0x40 */
} UranusMapItemWork;

/* lockable object in a segment, 0x84 bytes */
typedef struct UranusMapObjectWork {
    char pad_00[4];
    void *textures;             /* 0x04 */
    UranusMapWork *map;
    ModelPart *model;
    ModelPart *wire;
    ModelPart **parts;
    Vec3 ofs;
    Vec3 pos;
    Vec3 base;
    int life;                   /* segments left: 2 ahead, 1 next, 0 current, -1 behind */
    int kind;
    LockObj lock;
} UranusMapObjectWork;

extern char uranus_map_vardef[];
extern struct TaskDesc uranus_map_object_desc;
extern unsigned int game_flags;

extern PlayerCameraPreset uranus_map_cam_cfg;
extern float cmd_map_item_sl;
extern int uranus_map_cam_sw;
extern int uranus_map_cam_in_frm;
extern int uranus_map_cam_out_frm;
extern int uranus_map_cam_mode;
extern float uranus_map_cam_path_t;
extern float uranus_map_cam_frm;
extern struct TaskDesc uranus_map_desc;
extern char str_uranus_map_c[];
extern UranusMapSegSrc uranus_map_seg_srcs[];
extern GwVarDef uranus_m_vardefs[];
extern int uranus_map_vars_handle;
extern float uranus_map_roll_max;
extern float uranus_map_roll_damp;
extern float cmd_map_drate_1;
extern float cmd_map_drate_2;
extern float cmd_map_drate_3;
extern float cmd_map_drate_4;
extern int test_next_map;
extern unsigned int test_next_map_n;
extern float uranus_map_fade_sec;
extern float uranus_map_object_shake_pow;
extern float uranus_map_object_shake_rate;
extern float cmd_map_item_px;
extern float cmd_map_item_py;
extern float cmd_map_item_py2;
extern float cmd_map_item_py2_b;
extern float cmd_map_item_pz;
extern float cmd_map_item_sz;
extern int cmd_map_item_num;
extern unsigned int uranus_map_wire_col3;
extern float uranus_map_fade[4];
extern Vec3 uranus_map_vis_pts[3];
extern int cmd_disp_col;
extern int cmd_map_debug_disp_sw;
extern int uranus_map_wire_col[2];
extern int uranus_map_b_wire_col[2];
extern char str_uranus_map_fmt[];
extern char str_uranus_map_list_fmt[];
extern char str_uranus_map_pz_fmt[];
extern char str_uranus_map_bz_fmt[];

/* 0x00ca6fd0 */
int uranus_map_motion_eval(int mot, int flags, Vec3 *pos_out, int *ang_out, float t)
{
    anim_eval_hierarchy((int)uranus_map_motion, mot, uranus_map_pose_outs, t);
    if (pos_out)
        v3_copy(pos_out, &uranus_map_pose_pos);
    if (ang_out)
        i3_copy(ang_out, uranus_map_pose_ang);
    return 1;
}

/* 0x00ca7060 */
void uranus_map_path_pose(UranusMapWork *w, UranusMapSeg *seg)
{
    int mot_ang[3];
    Vec3 pos, fwd, up;
    int ang[3];
    Vec3 mot_pos;
    int lim;
    float z;

    if (seg->event == 2) {
        ulMatrixPush();
        motion_point_pose((void *)seg->mot, w->t, &pos, ang);
        matrix_unit_set(0);
        uranus_map_rotate_zyx(ang);
        z = 0.0f;
        fwd.y = z;
        fwd.z = -1.0f;
        fwd.x = z;
        do {
            up.z = z;
            up.x = z;
            up.y = 1.0f;
        } while (0);
        ulMatrixApplyVector((float *)ulMatrixCur, &fwd.x, &fwd.x);
        ulMatrixApplyVector((float *)ulMatrixCur, &up.x, &up.x);
        v3_copy(&w->pos, &pos);
        angles_from_dirs(&fwd, &up, ang);
        w->ang[2] = 0;
        w->ang[0] = ang[0];
        w->ang[1] = ang[1];
        w->roll = ang[2];
        ulMatrixPop();
        return;
    }
    if (w->held && seg->cam_mot)
        uranus_map_motion_eval(seg->cam_mot, 0, &w->pos, w->ang, w->t);
    else
        uranus_map_motion_eval(seg->mot, 0, &w->pos, w->ang, w->t);
    if (seg->mot_b)
        uranus_map_motion_eval(seg->mot_b, 0, &mot_pos, mot_ang, w->t);
    else
        mot_ang[2] = mot_ang[1] = mot_ang[0] = 0;
    mot_ang[2] = w->roll * uranus_map_roll_damp + mot_ang[2] * (1.0f - uranus_map_roll_damp);
    if (mot_ang[2] < 0) {
        lim = DEG_TO_ANG(-uranus_map_roll_max);
        if (mot_ang[2] < lim)
            mot_ang[2] = lim;
    } else {
        lim = DEG_TO_ANG(uranus_map_roll_max);
        if (lim < mot_ang[2])
            mot_ang[2] = lim;
    }
    w->roll = mot_ang[2];
}

/* 0x00ca72a8 */
float uranus_map_time_for(UranusMapWork *w, float x)
{
    UranusMapSeg *seg = uranus_map_ring_get(w, 0);
    float t = x / seg->len;

    if (fabsf(w->spd) < FLT_EPSILON)
        return 1000.0f;
    return seg->frames * t / w->spd;
}

/* 0x00ca7320 */
void uranus_map_pos_at(UranusMapWork *w, Vec3 *out, float t)
{
    UranusMapSeg *seg = uranus_map_ring_get(w, 0);
    UranusMapSeg *cur_seg = seg;
    float cur = w->t_prev2;
    float seg_t;
    Vec3 pl;

    player_get_pos(&pl);
    if (0.0f <= t) {
        seg_t = cur + t;
        if (seg->frames < seg_t) {
            seg_t -= seg->frames;
            seg = uranus_map_ring_get(w, 1);
            uranus_map_motion_eval(seg->mot, 0, out, 0, seg_t);
            v3_addto(out, &cur_seg->next_ofs);
        } else {
            uranus_map_motion_eval(seg->mot, 0, out, 0, seg_t);
        }
    } else {
        seg_t = cur + t;
        if (seg_t < 0.0f) {
            seg = uranus_map_ring_get(w, -1);
            uranus_map_motion_eval(seg->mot, 0, out, 0, seg_t + seg->frames);
            v3_addto(out, &cur_seg->prev_ofs);
        } else {
            uranus_map_motion_eval(seg->mot, 0, out, 0, seg_t);
        }
    }
    v3_sub(out, out, &pl);
}

/* 0x00ca74c0 */
void uranus_map_clock_step(UranusMapWork *w)
{
    float lim, cur, prev;
    UranusMapSeg *seg;

    if (test_map_v)
        w->spd = test_map_speed;
    cur = w->t;
    prev = w->t_prev;
    seg = w->seg;
    w->t_prev2 = prev;
    w->t_prev = cur;
    w->t = cur + w->spd;
    lim = seg->frames;
    if (lim < w->t) {
        w->t -= lim;
        w->t += cmd_map_frm_d;
        uranus_map_advance(w);
    }
}

/* 0x00ca7530 */
void uranus_map_player_set(Vec3 *pos, int *ang, int roll)
{
    Vec3 v;
    int roll_ang[3];
    float sc[3][2];

    player_set_cam_pos(pos);
    v.x = v.y = 0.0f;
    v.z = pos->z;
    player_set_view_pos(&v);
    v.y = 0.0f;
    v.z = -1.0f;
    v.x = 0.0f;
    player_set_view_dir(&v);
    ROT_Z(ang[2], sc[0], v.x, v.y);
    ROT_X(ang[0], sc[1], v.y, v.z);
    ROT_Y(ang[1], sc[2], v.x, v.z);
    player_set_cam_dir(&v);
    roll_ang[2] = roll;
    roll_ang[1] = 0;
    roll_ang[0] = 0;
    player_set_cam_roll(roll_ang[2]);
    roll_ang[2] = 0;
    player_set_view_roll(roll_ang[2]);
}

/* 0x00ca76a8 */
void uranus_map_exec(UranusMapWork *w)
{
    Vec3 v;
    int i;

    w->seg_end = 0;
    if (w->seg_wait && --w->seg_wait <= 0) {
        w->ring_cur = w->ring_head;
        w->seg_end = 1;
        if (w->nnext) {
            if (w->next_wait)
                w->next_wait--;
            else {
                int *slot;

                w->nnext--;
                slot = (int *)((char *)w + w->nnext * 4) + 59; /* steer(cast): &w->next[w->nnext] */
                uranus_map_ring_set(w, -2, *slot);
            }
        }
        if (w->ending) {
            if (w->show_next)
                w->show_next = 0;
            else if (w->show_cur)
                w->show_cur = 0;
            else
                w->show_prev = 0;
        }
    }
    uranus_map_clock_step(w);
    uranus_map_path_pose(w, w->seg);
    uranus_map_player_set(&w->pos, w->ang, w->roll);
    if (test_next_map) {
        if (test_next_map_n == 26)
            uranus_map_ring_set(w, 2, 26);
        else
            for (i = 0; i < 12; i++)
                uranus_map_ring_set(w, i + 2, test_next_map_n % 30);
        test_next_map = 0;
    }
    player_get_vec(&v);
    if (0.0f <= ulVec3Dot(&v, &uranus_map_dir)) {
        uranus_map_dir.x = v.x;
        uranus_map_drift.y = 0.0f;
        uranus_map_dir.y = v.y;
        uranus_map_dir.z = v.z;
        uranus_map_drift.x = 0.0f;
        uranus_map_drift.z = 0.0f;
    } else
        v3_sub(&uranus_map_drift, &v, &uranus_map_dir);
    uranus_map_scroll.z = uranus_map_dir.z;
    uranus_map_scroll.x = uranus_map_scroll.y = 0.0f;
    if (w->ending) {
        w->fade += -1.0f / (uranus_map_fade_sec * FPS);
        if (w->fade <= -1.0f)
            w->fade = -1.0f;
    }
}

/* steer(cast) */
/* 0x00ca7950 */
void uranus_map_disp(UranusMapWork *w)
{
    int vis[3];
    Vec3 pv;
    UranusMapSeg *seg_prev, *seg_cur, *seg_next;
    UranusMapSeg *e0, *e1, *e2, *e3, *e4, *e5, *e6, *e7;
    int old_blend, old_list, row, k;

    if (w->disp_wait) {
        w->disp_wait--;
        return;
    }
    draw_push_override_color();
    draw_push_strip_flags();
    if (!w->ending)
        old_blend = draw_set_override_flags(0);
    else {
        old_blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(-1, 2048);
        uranus_map_fade[0] = w->fade;
        draw_set_override_color(uranus_map_fade);
    }
    if (view_sphere_visible(&uranus_map_vis_pts[0], 2000.0f))
        vis[0] = 1;
    else
        vis[0] = 0;
    if (view_sphere_visible(&uranus_map_vis_pts[1], 2000.0f))
        vis[1] = 1;
    else
        vis[1] = 0;
    if (view_sphere_visible(&uranus_map_vis_pts[2], 2000.0f))
        vis[2] = 1;
    else
        vis[2] = 0;
    ulGxSetTextureList(w->textures);
    seg_prev = uranus_map_ring_get(w, -1);
    seg_cur = uranus_map_ring_get(w, 0);
    seg_next = uranus_map_ring_get(w, 1);
    if (cmd_disp_col & 4) {
        ulMatrixPush();
        MTX_TRANS(&seg_cur->prev_ofs);
        motion_path_draw((unsigned int *)seg_prev->mot);
        if (seg_prev->cam_mot)
            motion_path_draw((unsigned int *)seg_prev->cam_mot);
        ulMatrixPop();
        ulMatrixPush();
        motion_path_draw((unsigned int *)seg_cur->mot);
        if (seg_cur->cam_mot)
            motion_path_draw((unsigned int *)seg_cur->cam_mot);
        ulMatrixPop();
        ulMatrixPush();
        MTX_TRANS(&seg_cur->next_ofs);
        motion_path_draw((unsigned int *)seg_next->mot);
        if (seg_next->cam_mot)
            motion_path_draw((unsigned int *)seg_next->cam_mot);
        ulMatrixPop();
    }
    if (cmd_disp_col & 2) {
        ulMatrixPush();
        MTX_TRANS(&seg_cur->prev_ofs);
        uranus_col_debug_draw(seg_prev->cols, seg_prev->ncols);
        ulMatrixPop();
        ulMatrixPush();
        uranus_col_debug_draw(seg_cur->cols, seg_cur->ncols);
        ulMatrixPop();
        ulMatrixPush();
        MTX_TRANS(&seg_cur->next_ofs);
        uranus_col_debug_draw(seg_next->cols, seg_next->ncols);
        ulMatrixPop();
    }
    if (cmd_disp_col & 1) {
        ulMatrixPush();
        MTX_TRANS(&seg_cur->next_ofs);
        if (w->show_next && vis[2])
            dc_object_draw((struct MapPart *)seg_next->solid);
        old_list = RenderSetCurrentDisplayList(4);
        ulGxSetTextureList(w->textures);
        draw_set_blend(4, 5);
        part_draw_beat(seg_next->wire, uranus_map_wire_col[0], uranus_map_wire_col[1], 2.0f);
        if (seg_next->ncols)
            part_draw_beat(w->parts[31], uranus_map_b_wire_col[0], uranus_map_b_wire_col[1], 2.0f);
        RenderSetCurrentDisplayList(old_list);
        ulMatrixPop();
        ulMatrixPush();
        if (w->show_cur && vis[1])
            dc_object_draw((struct MapPart *)seg_cur->solid);
        if (uranus_map_scroll.z) {
        }
        old_list = RenderSetCurrentDisplayList(4);
        ulGxSetTextureList(w->textures);
        draw_set_blend(4, 5);
        part_draw_beat(seg_cur->wire, uranus_map_wire_col[0], uranus_map_wire_col[1], 2.0f);
        if (seg_cur->ncols)
            part_draw_beat(w->parts[31], uranus_map_b_wire_col[0], uranus_map_b_wire_col[1], 2.0f);
        RenderSetCurrentDisplayList(old_list);
        ulMatrixPop();
        ulMatrixPush();
        MTX_TRANS(&seg_cur->prev_ofs);
        if (w->show_prev && vis[0])
            dc_object_draw((struct MapPart *)seg_prev->solid);
        old_list = RenderSetCurrentDisplayList(4);
        ulGxSetTextureList(w->textures);
        draw_set_blend(4, 5);
        part_draw_beat(seg_prev->wire, uranus_map_wire_col[0], uranus_map_wire_col[1], 2.0f);
        if (seg_prev->ncols)
            part_draw_beat(w->parts[31], uranus_map_b_wire_col[0], uranus_map_b_wire_col[1], 2.0f);
        RenderSetCurrentDisplayList(old_list);
        ulMatrixPop();
    }
    draw_set_override_flags(old_blend);
    draw_pop_override_color();
    draw_pop_strip_flags();
    if (!debug_flags_test(4) || !cmd_map_debug_disp_sw)
        return;
    debug_set_origin(10, 51);
    debug_printf(str_uranus_map_fmt, (int)w->t, (int)uranus_map_ring_get(w, 0)->frames,
                 (int)w->pos.x, (int)w->pos.y, (int)w->pos.z);
    for (row = 0, k = 1; row < 3; k += 8) {
        debug_set_origin(10, row + 52);
        e0 = uranus_map_ring_get(w, row * 8);
        row++;
        e1 = uranus_map_ring_get(w, k);
        e2 = uranus_map_ring_get(w, k + 1);
        e3 = uranus_map_ring_get(w, k + 2);
        e4 = uranus_map_ring_get(w, k + 3);
        e5 = uranus_map_ring_get(w, k + 4);
        e6 = uranus_map_ring_get(w, k + 5);
        e7 = uranus_map_ring_get(w, k + 6);
        debug_printf(str_uranus_map_list_fmt, e0->id, e1->id, e2->id, e3->id,
                     e4->id, e5->id, e6->id, e7->id);
    }
    player_get_vec(&pv);
    debug_set_origin(10, 55);
    debug_printf(str_uranus_map_pz_fmt, pv.z);
    debug_set_origin(10, 56);
    debug_printf(str_uranus_map_bz_fmt, uranus_map_scroll.z);
}

/* steer(cast) */
/* 0x00ca8050 */
int uranus_map_seg_init(UranusMapWork *w, UranusMapSeg *seg, UranusMapSegSrc *src, int i)
{
    int n = 0;
    int k;
    int *ent;

    *(int **)&seg->mot = *(int **)pnb_get_entry((void *)w->models, src->mot); /* steer(alias) */
    if (src->mot_b)
        seg->mot_b = *(int *)pnb_get_entry((void *)w->models, src->mot_b);
    else
        seg->mot_b = 0;
    k = src->event;
    seg->event = k;
    switch (k) {
    case 1:
        seg->cam_mot = *(int *)pnb_get_entry((void *)w->models, src->cam_mot);
        break;
    case 2:
        seg->cam_arg[0] = src->cam_arg[0];
        seg->cam_arg[1] = src->cam_arg[1];
        break;
    }
    seg->id = i;
    {
        int courses = (int)src->courses;
        int obj_kind = src->obj_kind;

        /* steer(alias) */
        *(int *)&seg->solid = (int)&seg->pool[0];
        *(int *)&seg->wire = (int)&seg->pool[1];
        *(int *)&seg->col = (int)&seg->pool[2];
        *(int *)&seg->obj = (int)&seg->pool[3];
        *(int *)&seg->obj_wire = (int)&seg->pool[4];
        seg->obj_kind = obj_kind;
        *(int *)&seg->courses = courses;
    }
    ent = (int *)pnb_get_entry((void *)w->models, src->solid);
    dc_object_init((struct MapPart *)seg->solid, (struct DcNode *)*ent, 0, 0);
    seg->solid->wire_list = 0;
    do {
        if (src->solid != 45 && src->solid != 48 && src->solid != 51)
            dc_object_rebuild((struct MapPart *)seg->solid);
    } while (0);
    do {
        dc_object_init((struct MapPart *)seg->wire, *(struct DcNode **)pnb_get_entry((void *)w->models, src->wire), 0, 0);
        seg->wire->wire_list = src->wire_data ? *(int *)pnb_get_entry((void *)w->models, src->wire_data) : 0;
    } while (0);
    dc_object_set_wire((struct MapPart *)seg->wire, seg->wire->wire_list);
    do {
        if (src->col) {
            ent = (int *)pnb_get_entry((void *)w->models, src->col);
            dc_object_init((struct MapPart *)seg->col, (struct DcNode *)*ent, 0, 0);
            seg->col->wire_list = 0;
        }
    } while (0);
    ent = (int *)pnb_get_entry((void *)w->models, src->obj);
    dc_object_init((struct MapPart *)seg->obj, (struct DcNode *)*ent, 0, 0);
    seg->obj->wire_list = 0;
    do {
        dc_object_init((struct MapPart *)seg->obj_wire, *(struct DcNode **)pnb_get_entry((void *)w->models, src->obj_wire), 0, 0);
        seg->obj_wire->wire_list = src->obj_wire ? (int)model_load_wire_data(*(int *)pnb_get_entry((void *)w->models, src->obj_wire)) : 0;
    } while (0);
    dc_object_set_wire((struct MapPart *)seg->obj_wire, seg->obj_wire->wire_list);
    seg->frames = (*(unsigned int *)(seg->mot + 4) - 1);
    seg->len = seg->solid->radius;
    v3_copy(&seg->prev_ofs, &src->prev_ofs);
    v3_copy(&seg->next_ofs, &src->next_ofs);
    if (src->col) {
        n = uranus_col_tree_count((YNode *)seg->col->root);
        seg->ncols = n;
        seg->cols = heap_block_tag(game_malloc(n * 64), str_uranus_map_c, 700);
        uranus_col_tree_build((YNode *)seg->col->root, (char *)seg->cols);
        uranus_col_merge_cyls(seg->cols, seg->ncols);
    } else {
        *(int *)&seg->cols = 0;
        seg->ncols = 0;
    }
    return n;
}

/* 0x00ca83b0 */
void uranus_map_seg_free(UranusMapSeg *seg)
{
    if (seg->cols)
        heap_free(seg->cols, 0);
    if (seg->obj_wire->wire_list)
        model_free_block((void *)seg->obj_wire->wire_list);
    if (seg->wire)
        dc_object_release((struct MapPart *)seg->wire);
    if (seg->obj_wire)
        dc_object_release((struct MapPart *)seg->obj_wire);
    if (seg->solid)
        dc_object_release((struct MapPart *)seg->solid);
    if (seg->col)
        dc_object_release((struct MapPart *)seg->col);
    if (seg->obj)
        dc_object_release((struct MapPart *)seg->col);
}

/* 0x00ca8468 */
void uranus_map_camera_set(void)
{
    PlayerCameraPreset *cfg = player_camera_get_preset();

    if (game_type_get() == 5 && ConfigGetSelect(8))
        return;
    uranus_map_cam_cfg = *cfg;
    uranus_map_cam_cfg.cam_under_dist = 30.0f;
    uranus_map_cam_cfg.cam_top_dist = 7.0f;
    uranus_map_cam_cfg.cam_under_height = 10.0f;
    uranus_map_cam_cfg.cam_top_height = 2.0f;
    uranus_map_cam_cfg.eye_limit_x_top = 16200;
    player_camera_set_target(&uranus_map_cam_cfg);
}

/* 0x00ca8588 */
void uranus_map_camera_restore(void)
{
    void *cfg = player_camera_get_preset();

    if (game_type_get() != 5 || !ConfigGetSelect(8))
        player_camera_set_data(cfg);
}

/* 0x00ca85e0 */
void uranus_map_release(UranusMapWork *w)
{
    int i;

    for (i = 0; i < w->nsegs; i++)
        uranus_map_seg_free(&w->segs[i]);
    heap_free(w->segs, 0);
    player_swap_fullrange(1);
    uranus_map_camera_restore();
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)uranus_map_vardef);
}

/* steer(cast) */
/* 0x00ca8690 */
UranusMapWork *uranus_map_create(void *parent, ModelPart **parts, int cnt, Vec3 *boss_pos)
{
    UranusMapWork *w = task_create_inherit(parent, &uranus_map_desc, 272);
    int i;

    player_cell_move_clear();
    player_swap_fullrange(0);
    w->parts = parts;
    cm_wire_break_set_drift(&uranus_map_scroll, &uranus_map_drift);
    w->unk_4c = boss_pos;
    w->disp_wait = 2;
    w->nsegs = 30;
    w->segs = heap_block_tag(game_calloc(30 * sizeof(UranusMapSeg)), str_uranus_map_c, 779);
    for (i = 0; i < w->nsegs; i++)
        uranus_map_seg_init(w, &w->segs[i], &uranus_map_seg_srcs[i], i);
    uranus_map_reset(w, 0);
    uranus_map_vars_handle = ((int (*)(struct GwVarNode **, int, char *))gw_varedit_add)(&gw_varedit_root, 0, uranus_map_vardef);
    gw_varedit_add_table(&gw_varedit_root, uranus_map_vars_handle, uranus_m_vardefs);
    task_set_dlist(w, 3);
    return w;
}

/* 0x00ca8810 */
int uranus_map_pick_seg(UranusMapWork *w, int course)
{
    int i;
    int n = 0;

    for (i = 0; i < w->nsegs; i++) {
        char *courses = (char *)&w->segs[i]; /* steer(cast): one register for both */

        courses = ((UranusMapSeg *)courses)->courses;
        if (courses[course])
            n++;
    }
    n = (int)(RAND_FLOAT_A() * (float)n);
    for (i = 0; i < w->nsegs; i++)
        if (w->segs[i].courses[course] && --n <= 0)
            return i;
    return 0;
}

/* 0x00ca8918 */
int uranus_map_pick_seg_rnd(UranusMapWork *w, int course, float rate)
{
    float rnd_div = 32769.0f;

    if ((float)rand_lcg_next_a() / rnd_div < rate)
        return (int)((float)rand_lcg_next_a() / rnd_div * 4.0f) + 18;
    return uranus_map_pick_seg(w, course);
}

/* 0x00ca89d0 */
void uranus_map_set_next(UranusMapWork *w, int course)
{
    uranus_map_ring_set(w, 2, uranus_map_pick_seg(w, course));
}

/* 0x00ca8a00 */
void uranus_map_build_course(UranusMapWork *w, int course)
{
    unsigned int i;
    float rate;
    int j;

    uranus_map_ring_set(w, 2, 0);
    switch (course) {
    case 0:
        for (i = 0; i < 26; i++) {
            rate = (i & 1) ? cmd_map_drate_1 : 0.0f;
            j = i + 3;
            uranus_map_ring_set(w, j, uranus_map_pick_seg_rnd(w, course, rate));
        }
        uranus_map_ring_set(w, 0, w->nsegs - 3);
        uranus_map_ring_set(w, 1, w->nsegs - 2);
        uranus_map_ring_set(w, 2, w->nsegs - 1);
        w->nnext = 3;
        w->next_wait = 1;
        w->next[0] = uranus_map_pick_seg(w, course);
        break;
    case 2:
        for (i = 0; i < 26; i++) {
            rate = (i & 1) ? cmd_map_drate_3 : 0.0f;
            j = i + 3;
            uranus_map_ring_set(w, j, uranus_map_pick_seg_rnd(w, course, rate));
        }
        uranus_map_ring_set(w, 3, w->nsegs - 4);
        w->nnext = 3;
        w->next_wait = 3;
        w->next[0] = uranus_map_pick_seg(w, course);
        break;
    case 3:
        for (i = 0; i < 26; i++) {
            rate = (i & 1) ? cmd_map_drate_4 : 0.0f;
            j = i + 3;
            uranus_map_ring_set(w, j, uranus_map_pick_seg_rnd(w, course, rate));
        }
        w->next_wait = 0;
        w->nnext = 3;
        w->next[0] = uranus_map_pick_seg(w, course);
        break;
    default:
        for (i = 0; i < 26; i++) {
            rate = (i & 1) ? cmd_map_drate_2 : 0.0f;
            j = i + 3;
            uranus_map_ring_set(w, j, uranus_map_pick_seg_rnd(w, course, rate));
        }
        w->next_wait = 0;
        w->nnext = 3;
        w->next[0] = uranus_map_pick_seg(w, course);
        break;
    }
    w->next[1] = uranus_map_pick_seg(w, course);
    w->next[2] = uranus_map_pick_seg(w, course);
}

/* 0x00ca8c68 */
void uranus_map_reset(UranusMapWork *w, int course)
{
    unsigned int i;
    float z;

    uranus_map_camera_set();
    w->spd = 0.0f;
    w->seg_wait = 0;
    w->ring_cur = 0;
    w->ring_head = 0;
    w->t = 0;
    w->t_prev = w->spd;
    w->t_prev2 = w->spd;
    w->held = 0;
    w->hold = 0;
    w->nnext = 0;
    w->next_wait = 0;
    w->fade = w->spd;
    w->ending = w->spd;
    w->show_next = 1;
    w->show_cur = 1;
    w->show_prev = 1;
    for (i = 0; i < 30; i++)
        uranus_map_ring_set(w, i, uranus_map_pick_seg(w, course));
    z = 0.0f;
    uranus_map_dir.x = z;
    uranus_map_dir.y = z;
    uranus_map_dir.z = -1.0f;
    uranus_map_exec(w);
}

/* 0x00ca8d40 */
void uranus_map_start_end(UranusMapWork *w)
{
    w->ending = 1.0f;
    uranus_map_ring_fill(w, 0);
}

/* 0x00ca8d58 */
void uranus_map_set_spd(UranusMapWork *w, float spd)
{
    w->spd = spd;
}

/* 0x00ca8d60 */
float uranus_map_get_spd(UranusMapWork *w)
{
    return w->spd;
}

/* 0x00ca8d68 */
void uranus_map_blend_spd(UranusMapWork *w, float spd, float rate)
{
    w->spd = (1.0f - rate) * w->spd + spd * rate;
}

/* 0x00ca8d90 */
void uranus_map_ring_set(UranusMapWork *w, int i, int seg)
{
    int cur = w->ring_head;
    UranusMapSeg **ring = w->ring;
    UranusMapSeg **slot = &ring[(unsigned int)(cur + i + 30) % 30];

    *slot = &w->segs[seg % w->nsegs];
    if (!i) {
        UranusMapSeg **cur_slot = &ring[cur];

        w->seg = *cur_slot;
    }
}

/* 0x00ca8e00 */
void uranus_map_ring_fill(UranusMapWork *w, int seg)
{
    UranusMapSeg **ring = w->ring;
    unsigned int base = (unsigned int)(w->ring_head + 2) % 30;
    unsigned int i;
    int n = w->nsegs;

    for (i = 0; i < 28; i++) {
        UranusMapSeg **slot = &ring[(base + i) % 30];

        *slot = &w->segs[seg % n];
    }
}

/* 0x00ca8e90 */
UranusMapSeg *uranus_map_ring_get(UranusMapWork *w, int i)
{
    return w->ring[(unsigned int)(w->ring_cur + i + 30) % 30];
}

/* 0x00ca8ec0 */
void uranus_map_advance(UranusMapWork *w)
{
    UranusMapSeg *seg;
    BossCameraWork *cam;
    UranusMapSeg **ring = w->ring;

    w->ring_head = (unsigned int)(w->ring_head + 1) % 30;
    w->unk_24 = 0;
    w->seg_wait = 2;
    {
        UranusMapSeg **slot = &ring[w->ring_head];

        w->seg = *slot;
    }
    if (w->hold) {
        w->hold = 0;
        w->held = 1;
    } else {
        w->held = 0;
    }
    {
        UranusMapSeg **slot = &ring[(unsigned int)(w->ring_head + 1) % 30];

        seg = *slot;
    }
    switch (seg->event) {
    case 1:
        uranus_map_object_create(w, w->parts, seg->obj, seg->obj_wire, seg->obj_kind);
        break;
    case 2:
        if (uranus_map_cam_sw) {
            cam = boss_camera_create(w, w->spd, seg->cam_arg[0], seg->cam_arg[1], 0, 89, uranus_map_cam_in_frm, uranus_map_cam_mode, uranus_map_cam_out_frm);
            boss_camera_add_frame(cam, uranus_map_cam_frm);
            boss_camera_set_path(cam, seg->mot, uranus_map_cam_path_t);
        }
        break;
    }
}

/* 0x00ca9028 */
int uranus_map_col_records(int *rec, int mask, int n, int pt, float rad)
{
    int hits = 0;
    int nb;
    int hit;

    if (mask & 0x400)
        nb = 0;
    else
        nb = 1;
    for (; n; n--, rec += 16) {
        if (*rec & mask) {
            hit = uranus_col_check(rec, pt, nb, rad);
            if (hit)
                hits |= hit;
            if (hit == 1)
                return hits;
        }
    }
    return hits;
}

/* 0x00ca90e0 */
int uranus_map_col_check(UranusMapWork *w, Vec3 *pt, float *hit, float *nrm, float rad, int mask)
{
    UranusMapSeg *seg = uranus_map_ring_get(w, 0);
    UranusMapSeg *nseg;
    UranusPtDir ray;
    Vec3 *ofs, *nofs;
    int hits;

    ray = *(UranusPtDir *)pt;
    hits = uranus_map_col_records((int *)seg->cols, mask, seg->ncols, (int)&ray, rad);
    if (hits) {
        hit[0] = ray.p.x;
        hit[1] = ray.p.y;
        hit[2] = ray.p.z;
        nrm[0] = ray.dir.x;
        nrm[1] = ray.dir.y;
        nrm[2] = ray.dir.z;
        return hits;
    }
    ofs = &seg->prev_ofs;
    v3_sub(&ray.p, pt, ofs);
    nseg = uranus_map_ring_get(w, -1);
    hits = uranus_map_col_records((int *)nseg->cols, mask, nseg->ncols, (int)&ray, rad);
    if (hits) {
        hit[0] = ray.p.x + ofs->x;
        hit[1] = ray.p.y + ofs->y;
        hit[2] = ray.p.z + ofs->z;
        nrm[0] = ray.dir.x;
        nrm[1] = ray.dir.y;
        nrm[2] = ray.dir.z;
        return hits;
    }
    nofs = &seg->next_ofs;
    v3_sub(&ray.p, pt, nofs);
    nseg = uranus_map_ring_get(w, 1);
    hits = uranus_map_col_records((int *)nseg->cols, mask, nseg->ncols, (int)&ray, rad);
    if (hits) {
        hit[0] = ray.p.x + nofs->x;
        hit[1] = ray.p.y + nofs->y;
        hit[2] = ray.p.z + nofs->z;
        nrm[0] = ray.dir.x;
        nrm[1] = ray.dir.y;
        nrm[2] = ray.dir.z;
        return hits;
    }
    return 0;
}

/* 0x00ca92e8 */
void uranus_map_get_vel_z(Vec3 *v)
{
    player_get_vec(v);
    v->x = v->y = 0.0f;
}

/* 0x00ca9318 */
void uranus_map_get_pos_half(Vec3 *v)
{
    player_get_pos(v);
    v->x *= 0.5f;
    v->y *= 0.5f;
}

/* 0x00ca9360 */
void uranus_map_get_dir(Vec3 *v)
{
    Vec3 *src = &uranus_map_dir;

    v->x = src->x;
    v->y = src->y;
    v->z = src->z;
}

/* 0x00ca9388 */
void uranus_map_get_drift(Vec3 *v)
{
    Vec3 *src = &uranus_map_drift;

    v->x = src->x;
    v->y = src->y;
    v->z = src->z;
}

/* 0x00ca93b0 */
void uranus_map_get_scroll(Vec3 *v)
{
    Vec3 *src = &uranus_map_scroll;

    v->x = src->x;
    v->y = src->y;
    v->z = src->z;
}

/* 0x00ca93d8 */
void uranus_map_get_drift_b(Vec3 *v)
{
    Vec3 *src = &uranus_map_drift;

    v->x = src->x;
    v->y = src->y;
    v->z = src->z;
}

/* 0x00ca9400 */
int uranus_map_is_seg_end(UranusMapWork *w)
{
    return w->seg_end;
}

/* 0x00ca9408 */
void uranus_map_hold(UranusMapWork *w)
{
    w->hold++;
}

/* 0x00ca9418 */
void uranus_map_get_seg_ofs(UranusMapWork *w, Vec3 *out, int n)
{
    UranusMapSeg *seg = uranus_map_ring_get(w, 0);

    if (n > 0) {
        v3_copy(out, &seg->next_ofs);
        v3_scale(out, (float)n);
    } else if (n < 0) {
        v3_copy(out, &seg->prev_ofs);
        v3_scale(out, (float)n);
    } else {
        out->x = 0.0f;
        out->y = 0.0f;
        out->z = 0.0f;
    }
}

/* 0x00ca94b8 */
int uranus_map_check_event(UranusMapWork *w, int event)
{
    switch (event) {
    case -3:
        if (w->t + w->spd > w->seg->frames)
            return 1;
    case -2:
        if (w->seg_wait == 2)
            return 1;
    case -1:
        if (w->seg_wait == 1)
            return 1;
    }
    return w->seg_end != 0;
}

/* 0x00ca9540 */
int uranus_map_get_seg_id(UranusMapWork *w, int i)
{
    return uranus_map_ring_get(w, i)->id;
}

/* 0x00ca9560 */
void uranus_map_item_spawn(Vec3 *pos)
{
    Spawn spawn;

    switch ((int)(RAND_FLOAT_A() * 4.0f)) {
    case 0:
        spawn.kind = 0;
        break;
    case 1:
        spawn.kind = 2;
        break;
    case 2:
        spawn.kind = 3;
        break;
    default:
        if (!(game_flags & 0x40))
            spawn.kind = 2;
        else
            spawn.kind = 1;
        break;
    }
    uranus_map_get_dir((Vec3 *)&spawn.vel_x);
    ((float *)&spawn.vel_x)[2] = -((float *)&spawn.vel_x)[2];
    spawn.scale = 1.0f;
    ((float *)&spawn.vel_x)[0] = ((float *)&spawn.vel_x)[1] = 0.0f;
    spawn.x = pos->x;
    spawn.y = pos->y;
    spawn.z = pos->z;
    item_state_if_alive(item_create(&spawn), 3);
}

/* 0x00ca9678 */
void uranus_map_item_exec(UranusMapItemWork *w)
{
    Vec3 v, to_pl;

    if (uranus_map_is_seg_end(w->map) && --w->life < -1) {
        task_end_with_exit(w);
        return;
    }
    uranus_map_get_drift(&v);
    v3_addto(&w->pos0, &v);
    player_get_pos(&to_pl);
    v3_sub(&to_pl, &w->pos0, &to_pl);
    if (vec3_length(&to_pl) < cmd_map_item_sl) {
        uranus_map_item_spawn(&w->pos0);
        FINISH_IF(w);
    }
    v3_copy(&w->pos, &w->pos0);
}

/* 0x00ca97a8 */
UranusMapItemWork *uranus_map_item_create(void *parent, UranusMapWork *map, Vec3 *pos, ModelPart **parts)
{
    UranusMapItemWork *w = task_create_inherit(parent, &uranus_map_item_desc, 68);

    v3_copy(&w->pos, pos);
    w->parts = parts;
    w->map = map;
    w->life = 1;
    v3_copy(&w->pos0, pos);
    return w;
}

/* 0x00ca9840 */
void uranus_map_object_exec(UranusMapObjectWork *w)
{
    Vec3 drop;
    Vec3 vel;
    int ang[3];
    UranusMapSeg *seg;
    int i;

    if (uranus_map_is_seg_end(w->map)) {
        if (w->life == 2)
            w->lock.flags |= LOCK_LOCKABLE;
        w->life--;
    }
    if (w->life < -1)
        goto done;
    seg = uranus_map_ring_get(w->map, 0);
    switch (w->life) {
    case 2:
        return;
    case 1:
        v3_copy(&w->ofs, &seg->next_ofs);
        v3_sum(&w->pos, &w->ofs, &w->base);
        break;
    case 0:
        v3_set(&w->ofs, 0.0f, 0.0f, 0.0f);
        v3_copy(&w->pos, &w->base);
        break;
    case -1:
        v3_copy(&w->ofs, &seg->prev_ofs);
        v3_sum(&w->pos, &w->ofs, &w->base);
        break;
    }
    if (!(w->lock.flags & LOCK_HIT) || 0 < w->lock.life)
        return;
    boss_se_play(16, (int)&w->lock, 0);
    if (w->life != 1)
        goto done;
    lock_score(6, &w->lock);
    v3_set(&vel, 0.0f, 0.0f, 0.0f);
    ang[2] = ang[1] = ang[0] = 0;
    cm_wire_break_simple(w, 38, w->wire, &w->pos, &vel, ang, uranus_map_wire_col3);
    uranus_map_hold(w->map);
    oscilate_create(uranus_map_object_shake_pow, uranus_map_object_shake_rate);
    v3_copy(&drop, &w->ofs);
    switch (w->kind) {
    case 20:
        drop.x += cmd_map_item_px;
        drop.y += cmd_map_item_py;
        drop.z += cmd_map_item_pz;
        break;
    case 21:
        drop.x -= cmd_map_item_px;
        drop.y += cmd_map_item_py;
        drop.z += cmd_map_item_pz;
        break;
    case 19:
        drop.y += cmd_map_item_py2_b;
        drop.z += cmd_map_item_pz;
        break;
    case 18:
        drop.y -= cmd_map_item_py2;
        drop.z += cmd_map_item_pz;
        break;
    }
    for (i = 0; i < cmd_map_item_num; i++) {
        drop.z += cmd_map_item_sz;
        uranus_map_item_create(w->map, w->map, &drop, w->parts);
    }
done:
    FINISH_IF(w);
}

/* 0x00ca9b88 */
void uranus_map_object_disp(UranusMapObjectWork *w)
{
    int old_blend;

    if (w->life == 2)
        return;
    ulGxSetTextureList(w->textures);
    old_blend = draw_set_override_flags(0);
    ulMatrixPush();
    MTX_TRANS(&w->ofs);
    dc_object_draw((struct MapPart *)w->model);
    ulMatrixPop();
    draw_set_override_flags(old_blend);
}

/* 0x00ca9c18 */
void uranus_map_object_release(UranusMapObjectWork *w)
{
    lock_remove_entry(6, &w->lock);
}

/* 0x00ca9c28 */
UranusMapObjectWork *uranus_map_object_create(UranusMapWork *parent, ModelPart **parts, ModelPart *model, ModelPart *wire, int kind)
{
    UranusMapObjectWork *w = task_create_inherit(parent, &uranus_map_object_desc, 132);

    w->map = parent;
    w->kind = kind;
    w->model = model;
    w->wire = wire;
    w->parts = parts;
    w->life = 2;
    lock_init(6, &w->lock, w, &w->pos, 0, 1, 0);
    v3_copy(&w->base, (Vec3 *)&((struct DcNode *)w->model->root)->child->pos);
    lockon_entry(&w->lock);
    w->lock.flags &= ~LOCK_LOCKABLE;
    return w;
}

/* 0x00ca9d20 */
void uranus_map_rotate_zyx(int *ang)
{
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
}
