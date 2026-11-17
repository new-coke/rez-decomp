/*
 * Uranus boss snake form: cubes trail a head as springy links, each a ring of triangles. The head
 * blends between four moves; t4 is the attack, and between moves the laser charges. After enough
 * attacks it requests a form switch. Also the sound helpers (tables, music, streamed PCM).
 */

#include "types_gw.h"
#include "b_uranus.h"
#include "mathdef.h"
#include "debug.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "player.h"
#include "task.h"
#include "ul_math.h"
#include "ul_vu0.h"
#include "uranus_snake.h"
#include "uranus.h"
#include "uranus_bullet.h"
#include "uranus_map.h"
#include "uranus_part.h"
#include "../common/boss_common.h"
#include "../common/boss_sound.h"

extern BossSe se_table;
extern char uranus_snake_vardef[];
extern GwVarDef uranus_snake_vardefs[];
extern struct GwVarNode *uranus_snake_vars_group;
extern BossVolDef vol_table[];
extern int adx_files[2][2];
extern int pcm_files[][2];
extern int uranus_snake_deb_sw;
extern char *uranus_snake_move_names[];
extern char str_uranus_snake_fmt[];
extern int uranus_snake_rings;
extern int uranus_snake_points;
extern float uranus_snake_move_sec;
extern float uranus_snake_start_dis;
extern struct TaskDesc uranus_snake_desc;
extern char str_uranus_snake_c[];
extern struct TaskDesc uranus_snake_ex_desc;
extern float uranus_snake_t4_ofs_y;
extern float uranus_snake_t4_sway;
extern float uranus_snake_t4_vx;
extern float uranus_snake_t4_r;
extern float uranus_snake_t4_spd;
extern float uranus_snake_t4_len;
extern float cmd_t3_r;
extern float cmd_t3_len;
extern float cmd_t3_vz;
extern float cmd_t3_offsety;
extern float cmd_t3_swr;
extern float cmd_t3_swz;
extern float cmd_t3_swd;
extern float cmd_t2_r;
extern float cmd_t2_len;
extern float cmd_t2_vz;
extern float cmd_t2_drz;
extern float cmd_t2_offsety;
extern float cmd_t2_swr;
extern float cmd_sn_mh;
extern float cmd_sn_mw;
extern float cmd_sn_mz;
extern float cmd_sn_rd;
extern float cmd_sn_rdx;
extern float cmd_sn_swr;
extern float uranus_snake_sn_ofs_y;
extern int uranus_snake_test_move;
extern int uranus_snake_attack_n0;
extern int uranus_snake_attack_n1;
extern float cmd_sn_tri_r;
extern float uranus_snake_spring_k;
extern float uranus_snake_spring_damp;

enum {
    SNAKE_SN,
    SNAKE_T2,
    SNAKE_T3,
    SNAKE_T4                    /* the attack */
};

/* one head move's state, 0x40 bytes */
typedef struct UranusSnakeMove {
    float wobble;
    float ang;
    float ang2;
    Vec3 ofs;                   /* the head's offset from the player */
    char pad_18[12];
    Vec3 v;                     /* 0x24 */
    char pad_30[8];
    int roll;                   /* 0x38 */
    char pad_3c[4];
} UranusSnakeMove;

/* 360 bytes */
typedef struct UranusSnakeWork {
    char pad_00[8];
    int slot;                   /* 0x08 */
    int from_model;
    Vec3 head;
    Vec3 head_prev;
    ModelPart **parts;
    UranusPartWork *pm;
    char pad_30[8];
    int state;                  /* 0x38: 0 moving, 1 attacking, 2 charging */
    int started;
    UranusSnakeMove move[4];
    UranusNode *nodes;
    Vec3 *link_vel;
    int links;
    int cur;
    int next;
    int unk_154;                /* 0x154: set at random by the create functions, never read */
    int attacks;
    float blend;
    float blend_spd;
    struct VenusClWork *laser;
} UranusSnakeWork;

/* 0x00cb2d68 */
void uranus_snake_vars_add(void)
{
    uranus_snake_vars_group = gw_varedit_add(&gw_varedit_root, 0, uranus_snake_vardef);
    gw_varedit_add_table(&gw_varedit_root, uranus_snake_vars_group, uranus_snake_vardefs);
}

/* 0x00cb2db8 */
void uranus_snake_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)uranus_snake_vardef);
}

/* 0x00cb2dd0 */
int uranus_snake_move_t4(UranusSnakeWork *w, UranusSnakeMove *mv, Vec3 *out)
{
    Vec3 v;
    float sc[4];
    int turn = 0;

    player_get_pos(&w->head);
    w->head.y += uranus_snake_t4_ofs_y;
    if (fabsf(mv->v.y) < FLT_EPSILON) {
        if (0.0f <= mv->ofs.y)
            mv->v.y = uranus_snake_t4_vx;
        else
            mv->v.y = -uranus_snake_t4_vx;
    }
    if (uranus_snake_t4_len < mv->ofs.y) {
        turn = 1;
        mv->v.y = -uranus_snake_t4_vx;
    } else if (mv->ofs.y < -uranus_snake_t4_len) {
        turn = 1;
        mv->v.y = uranus_snake_t4_vx;
    }
    mv->ang += uranus_snake_t4_spd;
    mv->v.x = 0.0f;
    mv->v.z = uranus_snake_t4_r;
    mv->ofs.y += mv->v.y;
    ROT_Y(mv->ang * ANG_PER_RAD, sc, mv->v.x, mv->v.z);
    v3_add(&w->head, &mv->v);
    wobble_step_slow(&v, uranus_snake_t4_sway, &mv->wobble);
    v3_add(&w->head, &v);
    v3_sum(out, &mv->ofs, &w->head);
    return turn;
}

/* 0x00cb3010 */
int uranus_snake_move_t3(UranusSnakeWork *w, UranusSnakeMove *mv, Vec3 *out)
{
    Vec3 sway_amp, v;
    float sn, cs, rate, r_sign;
    int turn = 0;

    player_get_pos(&w->head);
    w->head.y += cmd_t3_offsety;
    if (fabsf(mv->v.z) < FLT_EPSILON) {
        if (0.0f <= mv->ofs.z)
            mv->v.z = cmd_t3_vz;
        else
            mv->v.z = -cmd_t3_vz;
    }
    if (cmd_t3_len < mv->ofs.z) {
        turn = 1;
        mv->v.z = -cmd_t3_vz;
    } else if (mv->ofs.z < -cmd_t3_len) {
        turn = 1;
        mv->v.z = cmd_t3_vz;
    }
    mv->ofs.z += mv->v.z;
    rate = fabsf(mv->ofs.z) / cmd_t3_len;
    if (1.0f < rate)
        rate = 1.0f;
    mv->ofs.y *= 0.98f;
    if (0.0f < mv->v.z)
        r_sign = cmd_t3_r;
    else
        r_sign = -cmd_t3_r;
    ulSinCosf(rate * 1.5707964f, &sn, &cs);
    mv->ofs.x = r_sign * cs;
    sway_amp.y = cmd_t3_swr;
    sway_amp.z = cmd_t3_swz;
    sway_amp.x = cmd_t3_swz;
    wobble_step(&v, &sway_amp, &mv->wobble, cmd_t3_swd);
    v3_add(&w->head, &v);
    v3_sum(out, &mv->ofs, &w->head);
    return turn;
}

/* 0x00cb3250 */
int uranus_snake_move_t2(UranusSnakeWork *w, UranusSnakeMove *mv, Vec3 *out)
{
    Vec3 v;
    float sc[4];
    float rate;
    int turn = 0;

    player_get_pos(&w->head);
    w->head.y += cmd_t2_offsety;
    if (fabsf(mv->v.z) < FLT_EPSILON) {
        if (0.0f <= mv->ofs.z)
            mv->v.z = cmd_t2_vz;
        else
            mv->v.z = -cmd_t2_vz;
    }
    if (cmd_t2_len < mv->ofs.z) {
        turn = 1;
        mv->v.z = -cmd_t2_vz;
    } else if (mv->ofs.z < -cmd_t2_len) {
        turn = 1;
        mv->v.z = cmd_t2_vz;
    }
    mv->ofs.z += mv->v.z;
    rate = fabsf(mv->ofs.z) / cmd_t2_len;
    if (1.0f < rate)
        rate = 1.0f;
    ulSinCosf(rate * 1.5707964f, &sc[0], &sc[1]);
    mv->roll += (int)DEG_TO_ANG(cmd_t2_drz);
    mv->ofs.x = 0.0f;
    mv->ofs.y = cmd_t2_r * sc[1];
    ROT_Z(mv->roll, (sc + 2), mv->ofs.x, mv->ofs.y);
    wobble_step_slow(&v, cmd_t2_swr, &mv->wobble);
    v3_add(&w->head, &v);
    v3_sum(out, &mv->ofs, &w->head);
    return turn;
}

/* 0x00cb34a8 */
int uranus_snake_move_sn(UranusSnakeWork *w, UranusSnakeMove *mv, Vec3 *out)
{
    Vec3 v;
    float z_sin, z_cos, h_sin, h_cos, w_sin, w_cos;
    float ang;
    int n;
    int turn = 0;

    player_get_pos(&w->head);
    w->head.x = w->head.y = 0.0f;
    for (n = enemy_status_get_ticks(); n; n--) {
        ang = mv->ang;
        mv->ang += cmd_sn_rd;
        mv->ang2 += cmd_sn_rdx;
        if (ang <= PI && PI < mv->ang)
            turn = 1;
        while (TWO_PI < mv->ang)
            mv->ang -= TWO_PI;
        while (TWO_PI < mv->ang2)
            mv->ang2 -= TWO_PI;
        ang = mv->ang;
        ulSinCosf(ang, &z_sin, &z_cos);
        mv->ofs.z = cmd_sn_mz * z_cos;
        ulSinCosf(mv->ang, &h_sin, &h_cos);
        mv->ofs.y = cmd_sn_mh * h_sin;
        ulSinCosf(mv->ang2, &w_sin, &w_cos);
        mv->ofs.x = cmd_sn_mw * w_sin;
        wobble_step_slow(&v, cmd_sn_swr, &mv->wobble);
        v3_add(&mv->ofs, &v);
        mv->ofs.y += uranus_snake_sn_ofs_y;
    }
    v3_sum(out, &w->head, &mv->ofs);
    return turn;
}

/* 0x00cb3760 */
int uranus_snake_move(UranusSnakeWork *w, int move, Vec3 *out)
{
    switch (move) {
    case SNAKE_SN:
        return uranus_snake_move_sn(w, &w->move[0], out);
    case SNAKE_T2:
        return uranus_snake_move_t2(w, &w->move[1], out);
    case SNAKE_T3:
        return uranus_snake_move_t3(w, &w->move[2], out);
    case SNAKE_T4:
        return uranus_snake_move_t4(w, &w->move[3], out);
    }
    return 0;
}

/* 0x00cb37e8 */
void uranus_snake_drift(UranusSnakeWork *w)
{
    Vec3 drift, drift2;
    UranusNode *link, *tip;

    uranus_map_get_vel_z(&drift);
    uranus_map_get_drift(&drift2);
    v3_addto(&w->head_prev, &drift);
    for (link = w->nodes->child->child; link; link = link->next) {
        v3_add((Vec3 *)link->data, &drift);
        for (tip = link->child; tip; tip = tip->next)
            v3_add((Vec3 *)tip->data, &drift);
    }
}

/* 0x00cb38f8 */
void uranus_snake_chain_step(UranusSnakeWork *w)
{
    Vec3 head, dir, org;
    Vec3 *vel = w->link_vel;
    UranusNode *link, *tip;
    int n, tris, j, k;

    for (n = enemy_status_get_ticks(); n; n--) {
        v3_copy(&head, &w->head_prev);
        v3_copy(&w->head_prev, &w->head);
        for (link = w->nodes->child->child; link; link = link->next) {
            tris = ceilf(uranus_node_count_children(link) / 3.0f);
            k = tris / 3; /* steer(dead) */
            spring_to_v(vel, (Vec3 *)link->data, &head, uranus_snake_spring_k, uranus_snake_spring_damp);
            tip = link->child;
            if (tip) {
                v3_sub(&dir, &head, (Vec3 *)link->data);
                if (v3_is_zero(&dir)) {
                    do {
                        dir.x = dir.y = 0.0f;
                        dir.z = -1.0f;
                    } while (0);
                }
                for (j = 0; j < tris; j++) {
                    v3_mul(&org, &dir, j * 0.25f);
                    v3_addto(&org, (Vec3 *)link->data);
                    for (k = 0; tip; ) {
                        vec_up_for_dir(&dir, (Vec3 *)tip->data, DEG_TO_ANG(k * 120.0f));
                        if (!v3_is_zero((Vec3 *)tip->data))
                            vec_set_length((Vec3 *)tip->data, cmd_sn_tri_r);
                        v3_sum((Vec3 *)tip->data, &org, (Vec3 *)tip->data);
                        tip = tip->next;
                        if (++k >= 3)
                            break;
                    }
                }
            }
            vel++;
            v3_copy(&head, (Vec3 *)link->data);
        }
    }
}

/* 0x00cb3cb8 */
void uranus_snake_pick_move(UranusSnakeWork *w, int attack)
{
    Vec3 pl;
    int move, i;

    player_get_pos(&pl);
    if (attack) {
        w->blend_spd = 1.0f / (uranus_snake_move_sec * FPS) * 0.5f;
        w->next = SNAKE_T4;
    } else {
        move = (int)(RAND_FLOAT_A() * 3.0f);
        for (i = 0; i < 3; i++) {
            if (move != w->cur) {
                w->next = move;
                return;
            }
            move = (move + 1) % 3;
        }
        w->blend_spd = 1.0f / (uranus_snake_move_sec * FPS);
        w->next = w->cur;
    }
    v3_sub(&w->move[w->next].ofs, &w->head, &pl);
}

/* 0x00cb3e28 */
void uranus_snake_exec(UranusSnakeWork *w)
{
    Vec3 pl;
    Vec3 mv_pos[4];
    int turned[4];
    int i, to, from, to_copy, lim;

    player_get_pos(&pl);
    uranus_snake_drift(w);
    if (w->laser && (TASK_OF(w->laser)->flags & TASK_F_END))
        w->laser = 0;
    if (uranus_snake_test_move != 4) {
        uranus_snake_move(w, uranus_snake_test_move, &w->head);
        goto done;
    }
    for (i = 0; i < 4; i++)
        turned[i] = uranus_snake_move(w, i, &mv_pos[i]);
    to = w->next;
    from = w->cur;
    to_copy = to;
    v3_lerp(&w->head, &mv_pos[to], &mv_pos[from], w->blend);
    switch (w->state) {
    case 0:
        if (!turned[from])
            break;
        if (uranus_pm_sw_is_live(w->pm, (int)w)) {
            lim = !w->from_model ? uranus_snake_attack_n0 : uranus_snake_attack_n1;
            if (++w->attacks < lim)
                goto more;
            do { uranus_pm_sw_request(w->pm, (int)w, 10); } while (0); /* steer(barrier) */
            w->attacks = 0;
        }
        uranus_snake_pick_move(w, 0);
        w->state = 2;
        break;
    more:
        uranus_snake_pick_move(w, 1);
        w->state = 1;
        break;
    case 1:
        w->blend += w->blend_spd;
        if (!(w->blend >= 1.0f))
            break;
        if (to == SNAKE_T4) {
            if (uranus_pm_sw_is_live(w->pm, (int)w)) {
                uranus_pm_fire(w->pm, 1);
                to_copy = w->next;
            } else {
                to_copy = w->next;
            }
        }
        w->blend = 0.0f;
        w->cur = to_copy;
        uranus_snake_pick_move(w, 0);
        w->state = 2;
        break;
    case 2:
        w->blend += w->blend_spd;
        if (!(w->blend >= 1.0f))
            break;
        w->blend = 0.0f;
        w->cur = to;
        uranus_snake_pick_move(w, RAND_FLOAT_A() * 2.0f);
        w->state = 0;
        if (uranus_pm_sw_is_live(w->pm, (int)w)) {
            if (!w->laser)
                w->laser = venus_cl_create(w->pm, w->parts);
            w->attacks++;
        }
        break;
    }
done:
    v3_copy((Vec3 *)w->nodes->child->data, &w->head);
    if (!w->started) {
        v3_copy(&w->head_prev, &w->head);
        w->started = 1;
    }
    uranus_snake_chain_step(w);
}

/* 0x00cb4198 */
void uranus_snake_reset_pos(UranusSnakeWork *w, Vec3 *pos)
{
    UranusNode *root, *link, *tip;

    v3_copy(&w->head, pos);
    root = w->nodes->child;
    v3_copy((Vec3 *)root->data, &w->head);
    for (link = root->child; link; link = link->next) {
        v3_copy((Vec3 *)link->data, &w->head);
        for (tip = link->child; tip; tip = tip->next)
            v3_copy((Vec3 *)tip->data, &w->head);
    }
}

/* 0x00cb4250 */
void uranus_snake_nodes_build(UranusSnakeWork *w, int data, char *link_pts, char *pt_pts)
{
    UranusNode *root, *link;
    int i, j;

    w->nodes = uranus_node_new();
    root = uranus_node_add_child(w->nodes);
    root->data = data;
    for (i = uranus_snake_rings; i; i--) {
        j = uranus_snake_points;
        link = uranus_node_add_child(root);
        link->data = (int)link_pts;
        link_pts += 12;
        for (; j; j--) {
            uranus_node_add_child(link)->data = (int)pt_pts;
            pt_pts += 12;
        }
    }
}

/* 0x00cb4338 */
void uranus_snake_release(UranusSnakeWork *w)
{
    heap_free(w->link_vel, 0);
    uranus_node_free_tree(w->nodes);
}

/* 0x00cb4368 */
void uranus_snake_disp(UranusSnakeWork *w)
{
    if (debug_flags_test(4) && uranus_snake_deb_sw) {
        debug_set_origin(10, 44);
        debug_printf(str_uranus_snake_fmt, uranus_snake_move_names[w->cur],
                     uranus_snake_move_names[w->next], (int)(w->blend * 100.0f));
    }
}

/* 0x00cb4408 */
void uranus_snake_switch_cb(UranusSnakeWork *w)
{
    Vec3 core, drift, ofs;
    Vec3 *obj_pos;
    float dist;
    int i;

    player_get_pos(&ofs);
    uranus_map_get_drift(&drift);
    obj_pos = uranus_pm_get_obj0_pos(w->pm);
    v3_sum(&core, obj_pos, &drift);
    v3_sub(&ofs, &core, &ofs);
    if (v3_is_zero(&ofs))
        dist = 0.0f;
    else
        dist = vec3_length(&ofs);
    if (uranus_snake_start_dis < dist) {
        vec3_normalize(&ofs, &ofs);
        v3_scale(&ofs, uranus_snake_start_dis);
    }
    for (i = 0; i < 4; i++) {
        v3_copy(&w->move[i].ofs, &ofs);
        if (ofs.z < 0.0f)
            w->move[i].v.z = -fabsf(w->move[i].v.z);
        else
            w->move[i].v.z = fabsf(w->move[i].v.z);
    }

    v3_copy(&w->head, &core);
    uranus_snake_reset_pos(w, &w->head);
    w->attacks = 0;
}

/* steer(cast) */
/* 0x00cb45d8 */
UranusSnakeWork *uranus_snake_create(void *parent, UranusPartWork *pm, Vec3 **pts, UranusNode **out, int slot, ModelPart **parts)
{
    UranusSnakeWork *w = task_create_inherit(parent, &uranus_snake_desc, 360);
    int links;

    w->pm = pm;
    w->parts = parts;
    w->slot = slot;
    w->from_model = 0;
    uranus_snake_nodes_build(w, (int)pts[0], (char *)pts[1], (char *)pts[2]);
    if (out)
        *out = w->nodes->child;
    links = uranus_snake_rings;
    w->links = links;
    {
        void *link_vel = heap_block_tag(game_calloc(links * sizeof(Vec3)), str_uranus_snake_c, 590);

        w->blend_spd = 1.0f / (uranus_snake_move_sec * FPS);
        w->link_vel = link_vel;
    }
    uranus_snake_move(w, w->cur, &w->head);
    uranus_snake_reset_pos(w, &w->head);
    w->cur = RAND_FLOAT_A() * 3.0f;
    w->next = RAND_FLOAT_A() * 3.0f;
    w->unk_154 = RAND_FLOAT_A() * 3.0f;
    *(int *)&w->blend = 0; /* steer(type) */
    uranus_pm_set_cb(w->pm, (int)w, (void (*)(int))uranus_snake_switch_cb, slot);
    return w;
}

/* 0x00cb47b0 */
void uranus_snake_nodes_set(UranusNode *root, int data, Vec3 *link_pts, Vec3 *pt_pts)
{
    UranusNode *link, *tip;

    root->data = data;
    for (link = root->child; link; link = link->next) {
        link->data = (int)link_pts;
        link_pts++;
        for (tip = link->child; tip; tip = tip->next) {
            tip->data = (int)pt_pts;
            pt_pts++;
        }
    }
}

/* 0x00cb4808 */
UranusSnakeWork *uranus_snake_create_from(void *parent, UranusPartWork *pm, Vec3 **pts, UranusNode **out, int slot, UranusNode *shape, ModelPart **parts)
{
    UranusSnakeWork *w = task_create_inherit(parent, &uranus_snake_ex_desc, 360);
    int links;

    w->pm = pm;
    w->parts = parts;
    w->slot = slot;
    w->from_model = 1;
    w->nodes = uranus_node_new();
    uranus_node_copy_tree(w->nodes, shape);
    uranus_snake_nodes_set(w->nodes->child, (int)pts[0], pts[1], pts[2]);
    if (out)
        *out = w->nodes->child;
    links = uranus_node_count_children(w->nodes->child);
    w->links = links;
    {
        void *link_vel = heap_block_tag(game_calloc(links * sizeof(Vec3)), str_uranus_snake_c, 641);

        w->blend_spd = 1.0f / (uranus_snake_move_sec * FPS);
        w->link_vel = link_vel;
    }
    uranus_snake_move(w, w->cur, &w->head);
    uranus_snake_reset_pos(w, &w->head);
    w->cur = RAND_FLOAT_A() * 3.0f;
    w->next = RAND_FLOAT_A() * 3.0f;
    w->unk_154 = RAND_FLOAT_A() * 3.0f;
    *(int *)&w->blend = 0; /* steer(type) */
    uranus_pm_set_cb(w->pm, (int)w, (void (*)(int))uranus_snake_switch_cb, slot);
    return w;
}

/* 0x00cb4a08 */
void boss_se_play(int id, int lock, int pos)
{
    se_table_play(0, &se_table, id, lock, pos);
}

/* 0x00cb4a28 */
void boss_vol_play(int id, int arg)
{
    vol_table_play(0, vol_table, id, arg);
}

/* 0x00cb4a40 */
void boss_vol_stop(int id)
{
    vol_table_stop(0, vol_table, id);
}

/* 0x00cb4a58 */
void boss_bgm_exit_cb(BgmMan *bgm)
{
    se_table_stop(&se_table);
}

/* 0x00cb4a68 */
void boss_se_stop_all(void)
{
    se_table_stop(&se_table);
}

/* 0x00cb4a78 */
BgmMan *boss_bgm_start(void *parent)
{
    se_table_reset(&se_table);
    return cm_bgm_sound_man_create(parent, boss_bgm_exit_cb);
}

/* 0x00cb4ab8 */
void boss_adx_request(void)
{
    cm_bgm_sound_man_add_file(0, adx_files[0], (int)adx_files[1]);
}

/* 0x00cb4ad0 */
void boss_bgm_switch(int cb, int cb_arg, int idx)
{
    cm_bgm_sound_sub_start(0, cb, cb_arg, 0, 0, idx, idx);
}

/* 0x00cb4af8 */
void boss_pcm_load(int i)
{
    boss_stream_load((int)pcm_files[i], 1);
}

/* 0x00cb4b10 */
void boss_pcm_start(void)
{
    boss_stream_start();
}
