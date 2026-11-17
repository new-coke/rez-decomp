/*
 * Uranus boss orbit forms. First: cube rings along a looped path ahead of the player, wait and
 * move on the beat, switch form after five moves. Second: three tilted, wobbling rings circle the
 * centre and pulse; each pulse requests a switch, and the part manager fires once granted.
 */

#include "types_gw.h"
#include "b_uranus.h"
#include "mathdef.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "heap.h"
#include "pnb.h"
#include "sound_beat.h"
#include "task.h"
#include "ul_math.h"
#include "uranus_orbit.h"
#include "uranus.h"
#include "uranus_map.h"
#include "uranus_part.h"
#include "../common/boss_common.h"

extern char uranus_orbit_vardef[];
extern GwVarDef uranus_orbit_vardefs[];
extern struct GwVarNode *uranus_orbit_vars_group;
extern int uranus_orbit_rings;
extern int uranus_orbit_points;
extern float cmd_offset2_z;
extern float cmd_offset2_zf;
extern float cmd_offset2_y;
extern float cmd_p2_sw_r;
extern float cmd_p2_len;
extern float cmd_p2_len2;
extern float cmd_p2_len_l;
extern float cmd_p2_r_t;
extern float cmd_p2_wait_t;


extern struct TaskDesc u_or_desc;
extern char str_uranus_orbit_c[];
extern char uranus_orbit_wire[];
extern float uranus_orbit_spd;
extern float uranus_orbit_start_sec;
extern struct TaskDesc u_or2_desc;
extern float cmd_p2_dp;
extern float uranus_orbit_gap;
extern float uranus_orbit_ofs_z;
extern float uranus_orbit_ofs_y;
extern float uranus_orbit_wait_sec;
extern float uranus_orbit_move_sec;
extern float cmd_p2_y;
extern float cmd_p2_t;
extern float cmd_p2_t_d;
extern float cmd_p2_at;
extern float cmd_p2_at_d;
extern float cmd_p2_dang;
extern float u_or2_spring_k;
extern float u_or2_spring_damp;

/* first orbit form, 0x3c bytes */
typedef struct UOrWork {
    void *models;
    char pad_04[4];
    int *path;                  /* 0x08: the path motion */
    int unk_0c;                 /* 0x0c: from pnb entry 152, never read */
    float spd;
    char pad_14[4];
    float t;                    /* 0x18 */
    float len;
    UranusPartWork *pm;
    int state;                  /* 0 waiting, 1 moving */
    int wait;
    int moves;
    int slot;
    UranusNode *nodes;
    Vec3 *pts;
} UOrWork;

/* second orbit form ring, 0x3c bytes */
typedef struct UOr2Ring {
    float len;
    float y;
    float spd;
    float ang;
    float period;
    float wobble_spd;
    float wobble;
    float len_vel;
    float len_to;
    float tilt_x_amp;
    char pad_28[4];
    float tilt_z_amp;           /* 0x2c */
    float tilt_x;
    char pad_34[4];
    float tilt_z;               /* 0x38 */
} UOr2Ring;

/* second orbit form, 0x28 bytes */
typedef struct UOr2Work {
    char pad_00[8];
    UranusPartWork *pm;         /* 0x08 */
    int state;
    int wait;
    int slot;
    UranusNode *nodes;
    UOr2Ring *rings;
    Vec3 *jitter;
    float wobble;
} UOr2Work;

/* 0x00caba68 */
void uranus_orbit_vars_add(void)
{
    uranus_orbit_vars_group = gw_varedit_add(&gw_varedit_root, 0, uranus_orbit_vardef);
    gw_varedit_add_table(&gw_varedit_root, uranus_orbit_vars_group, uranus_orbit_vardefs);
}

/* 0x00cabab8 */
void uranus_orbit_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)uranus_orbit_vardef);
}

/* 0x00cabad0 */
void uranus_orbit_path_lerp(Vec3 *pts, Vec3 *out, float t)
{
    int i = (int)t;
    int j = i + 1;
    float frac = t - (float)i;
    float frac2 = 1.0f - frac;

    out->x = pts[j].x * frac + pts[i].x * frac2;
    out->y = pts[j].y * frac + pts[i].y * frac2;
    out->z = pts[j].z * frac + pts[i].z * frac2;
}

/* steer(cast) */
/* 0x00cabb58 */
void u_or_exec(UOrWork *w)
{
    Vec3 v, ofs;
    UranusNode *ring, *pt;
    Vec3 *node_pt;
    float t, t_ring;
    int n;

    for (n = enemy_status_get_ticks(); n; n--) {
        switch (w->state) {
        case 0:
            uranus_pm_set_mode(w->pm, 0);
            {
                float phase = w->t + w->spd;

                w->t = phase;
                if (w->len <= phase)
                    w->t = phase - w->len;
            }
            if (--w->wait <= 0 && beat_grid_hit(0)) {
                w->wait = uranus_orbit_move_sec * FPS;
                w->state = 1;
            }
            break;
        case 1:
            uranus_pm_set_mode(w->pm, 1);
            if (--w->wait <= 0 && beat_grid_hit(0)) {
                w->wait = uranus_orbit_wait_sec * FPS;
                w->state = 0;
                if (5 <= ++w->moves && uranus_pm_sw_request(w->pm, (int)w, 0))
                    w->moves = 0;
            }
            break;
        }
    }
    t = w->t;
    uranus_map_get_pos_half(&v);
    t_ring = t;
    ofs.z = uranus_orbit_ofs_z;
    ofs.y = uranus_orbit_ofs_y;
    ofs.x = 0.0f;
    v3_add(&v, &ofs);
    node_pt = (Vec3 *)w->nodes->child->data;
    ((void (*)(Vec3 *, Vec3 *, float, float))uranus_orbit_path_lerp)(w->pts, node_pt, t, w->len);
    v3_addto(node_pt, &v);
    for (ring = w->nodes->child->child; ring; ring = ring->next) {
        t_ring += uranus_orbit_gap * uranus_orbit_points;
        if (w->len < t_ring)
            t_ring -= w->len;
        ((void (*)(Vec3 *, Vec3 *, float, float))uranus_orbit_path_lerp)(w->pts, (Vec3 *)ring->data, t_ring, w->len);
        v3_add((Vec3 *)ring->data, &v);
        for (pt = ring->child; pt; pt = pt->next) {
            t += uranus_orbit_gap;
            if (w->len < t)
                t -= w->len;
            ((void (*)(Vec3 *, Vec3 *, float, float))uranus_orbit_path_lerp)(w->pts, (Vec3 *)pt->data, t, w->len);
            v3_add((Vec3 *)pt->data, &v);
        }
    }
}

/* 0x00cabe68 */
void u_or_release(UOrWork *w)
{
    uranus_pm_set_mode(w->pm, 0);
    uranus_node_free_tree(w->nodes);
    heap_free(w->pts, 0);
}

/* 0x00cabea8 */
void u_or_nodes_build(UOrWork *w, int data, char *ring_pts, char *pt_pts)
{
    UranusNode *root, *ring;
    int i, j;

    w->nodes = uranus_node_new();
    root = uranus_node_add_child(w->nodes);
    root->data = data;
    for (i = uranus_orbit_rings; i; i--) {
        j = uranus_orbit_points;
        ring = uranus_node_add_child(root);
        ring->data = (int)ring_pts;
        ring_pts += 12;
        for (; j; j--) {
            uranus_node_add_child(ring)->data = (int)pt_pts;
            pt_pts += 12;
        }
    }
}

/* steer(cast) */
/* 0x00cabf90 */
UOrWork *u_or_create(void *parent, UranusPartWork *pm, Vec3 **pts, UranusNode **out, int slot)
{
    UOrWork *w = task_create_inherit(parent, &u_or_desc, 60);
    int i;

    w->slot = slot;
    *(int *)&w->path = *(int *)pnb_get_entry(w->models, 151); /* steer(alias) */
    *(void **)&w->unk_0c = *(void **)pnb_get_entry(w->models, 152); /* steer(alias) */
    w->pm = pm;
    w->len = (unsigned int)(w->path[1] - 1);
    w->wait = uranus_orbit_start_sec * FPS;
    w->spd = uranus_orbit_spd;
    u_or_nodes_build(w, (int)pts[0], (char *)pts[1], (char *)pts[2]);
    if (out)
        *out = w->nodes->child;
    w->pts = heap_block_tag(game_malloc(((int)w->len + 1) * sizeof(Vec3)), str_uranus_orbit_c, 259);
    for (i = 0; i <= (int)w->len; i++)
        tree_node_pose((WireObj *)uranus_orbit_wire, w->path, &w->pts[i], 0, 0, 0, i);
    return w;
}

/* 0x00cac150 */
void u_or2_rings_init(UOr2Work *w, UranusNode *n)
{
    float rad;
    UOr2Ring *ring = w->rings;
    UOr2Ring *first;
    int i, j;

    for (i = 0; i < 3; i++) {
        first = ring;
        switch (i) {
        case 0:
            rad = cmd_p2_len;
            ring->y = -cmd_p2_y;
            ring->len = rad;
            ring->len_to = rad;
            break;
        case 1:
            ring->y = 0.0f;
            ring->len_to = ring->len = cmd_p2_len2;
            break;
        case 2:
            rad = cmd_p2_len;
            ring->y = cmd_p2_y;
            ring->len = rad;
            ring->len_to = rad;
            break;
        }
        ring->ang = RAND_FLOAT_A() * 2.0f * PI;
        ring->wobble = RAND_FLOAT_A() * 2.0f * PI;
        ring->spd = TWO_PI / ((cmd_p2_t + (RAND_FLOAT_A() * (cmd_p2_t_d + cmd_p2_t_d) - cmd_p2_t_d)) * FPS);
        ring->period = (cmd_p2_t + (RAND_FLOAT_A() * (cmd_p2_t_d + cmd_p2_t_d) - cmd_p2_t_d)) * FPS;
        ring->wobble_spd = TWO_PI / ((cmd_p2_at + (RAND_FLOAT_A() * (cmd_p2_at_d + cmd_p2_at_d) - cmd_p2_at_d)) * FPS);
        ring->tilt_x = RAND_FLOAT_A() * (cmd_p2_dang + cmd_p2_dang) - cmd_p2_dang;
        ring->tilt_z = RAND_FLOAT_A() * (cmd_p2_dang + cmd_p2_dang) - cmd_p2_dang;
        ring->tilt_x_amp = RAND_FLOAT_A() * (cmd_p2_dang + cmd_p2_dang) - cmd_p2_dang;
        ring->tilt_z_amp = RAND_FLOAT_A() * (cmd_p2_dang + cmd_p2_dang) - cmd_p2_dang;
        ring++;
        for (j = 1; j < 3; j++, ring++) {
            *ring = *first;
            ring->ang += j * TWO_PI / 3.0f;
        }
    }
}

/* 0x00cac538 */
void u_or2_orbit_step(UOr2Work *w, UranusNode *node, Vec3 *out)
{
    Vec3 *jit = w->jitter;
    UOr2Ring *ring = w->rings;
    Vec3 v;
    int ang[3];
    float wob_sc[4];
    float sc[12];
    float rate, ang_pt, step;
    Vec3 *pt;
    UranusNode *child;

    for (; node; node = node->next, ring++) {
        ring->spd = TWO_PI / ring->period * (cmd_p2_len2 + cmd_p2_len_l) / ring->len;
        ring->ang += ring->spd;
        if (TWO_PI <= ring->ang)
            ring->ang -= TWO_PI;
        ring->wobble += ring->wobble_spd;
        if (TWO_PI <= ring->wobble)
            ring->wobble -= TWO_PI;
        spring_to(&ring->len_vel, &ring->len, ring->len_to, u_or2_spring_k, u_or2_spring_damp);
        rate = ring->len / (cmd_p2_len2 + cmd_p2_len_l);
        ulSinCosf(ring->wobble, &wob_sc[0], &wob_sc[1]);
        ang[0] = DEG_TO_ANG(ring->tilt_x + ring->tilt_x_amp * wob_sc[0]);
        ulSinCosf(ring->wobble, &wob_sc[2], &wob_sc[3]);
        ang[2] = DEG_TO_ANG(ring->tilt_z + ring->tilt_z_amp * wob_sc[3]);
        v.z = ring->len;
        v.y = ring->y;
        v.x = 0.0f;
        ROT_Y(ring->ang * ANG_PER_RAD, sc, v.x, v.z);
        ROT_X(ang[0], (sc + 2), v.y, v.z);
        ROT_Z(ang[2], (sc + 4), v.x, v.y);
        v3_sum((Vec3 *)node->data, &v, out);
        step = TWO_PI / (float)(uranus_orbit_points * 3);
        ang_pt = ring->ang - step * 0.5f * (float)uranus_orbit_points;
        for (child = node->child; child; child = child->next) {
            v.z = ring->len;
            v.y = ring->y;
            v.x = 0.0f;
            ROT_Y(ang_pt * ANG_PER_RAD, (sc + 6), v.x, v.z);
            ROT_X(ang[0], (sc + 8), v.y, v.z);
            ROT_Z(ang[2], (sc + 10), v.x, v.y);
            pt = (Vec3 *)child->data;
            ang_pt += step;
            v3_sum(pt, &v, out);
            v3_mul(&v, jit, rate);
            jit++;
            v3_addto(pt, &v);
        }
    }
}

/* 0x00caca80 */
void u_or2_pulse(UOr2Work *w)
{
    UOr2Ring *ring;
    float rad_out, rad_in;
    int i;

    if (--w->wait > 0)
        return;
    switch (w->state) {
    case 0:
        ring = w->rings;
        for (i = 0; i < 3; i++) {
            ring[i].len_to = cmd_p2_len;
            ring[i + 6].len_to = cmd_p2_len;
            ring[i + 3].len_to = cmd_p2_len2;
        }
        w->state = 1;
        break;
    case 1:
        if (uranus_pm_sw_request(w->pm, (int)w, (int)(cmd_p2_wait_t * FPS))) {
            uranus_pm_fire(w->pm, 0);
            w->state = 2;
            break;
        }
        ring = w->rings;
        for (i = 0; i < 3; i++) {
            ring[i].len_to = cmd_p2_len + cmd_p2_len_l;
            ring[i + 6].len_to = cmd_p2_len + cmd_p2_len_l;
            ring[i + 3].len_to = cmd_p2_len2 + cmd_p2_len_l;
        }
        w->state = 0;
        break;
    case 2:
        rad_out = cmd_p2_len + cmd_p2_len_l;
        rad_in = cmd_p2_len2 + cmd_p2_len_l;
        ring = w->rings;
        for (i = 0; i < 3; i++) {
            ring[i].len_to = rad_out;
            ring[i + 6].len_to = rad_out;
            ring[i + 3].len_to = rad_in;
        }
        w->state = 0;
        break;
    }
    w->wait = (int)(cmd_p2_r_t * FPS);
}

/* 0x00cacc30 */
void u_or2_exec(UOr2Work *w)
{
    Vec3 ctr, ofs;
    UranusNode *root;
    int i;

    for (i = enemy_status_get_ticks(); i; i--)
        u_or2_pulse(w);
    if (uranus_pm_is_core(w->pm))
        ofs.z = cmd_offset2_zf;
    else
        ofs.z = cmd_offset2_z;
    ofs.y = cmd_offset2_y;
    ofs.x = 0.0f;
    uranus_map_get_pos_half(&ctr);
    v3_add(&ctr, &ofs);
    wobble_step_slow(&ofs, cmd_p2_sw_r, &w->wobble);
    v3_addto(&ctr, &ofs);
    root = w->nodes->child;
    v3_copy((Vec3 *)root->data, &ctr);
    u_or2_orbit_step(w, root->child, &ctr);
}

/* 0x00cacd98 */
void u_or2_release(UOr2Work *w)
{
    uranus_node_free_tree(w->nodes);
    heap_free(w->rings, 0);
    heap_free(w->jitter, 0);
}

/* 0x00cacdd8 */
void u_or2_nodes_build(UOr2Work *w, int data, char *ring_pts, char *pt_pts)
{
    UranusNode *root, *ring;
    int i, j;

    w->nodes = uranus_node_new();
    root = uranus_node_add_child(w->nodes);
    root->data = data;
    for (i = uranus_orbit_rings; i; i--) {
        j = uranus_orbit_points;
        ring = uranus_node_add_child(root);
        ring->data = (int)ring_pts;
        ring_pts += 12;
        for (; j; j--) {
            uranus_node_add_child(ring)->data = (int)pt_pts;
            pt_pts += 12;
        }
    }
}

/* 0x00cacec0 */
UOr2Work *u_or2_create(void *parent, UranusPartWork *pm, Vec3 **pts, UranusNode **out, int slot)
{
    UOr2Work *w = task_create_inherit(parent, &u_or2_desc, 40);
    Vec3 *jit;
    int i;

    w->pm = pm;
    w->slot = slot;
    u_or2_nodes_build(w, (int)pts[0], (char *)pts[1], (char *)pts[2]);
    if (out)
        *out = w->nodes->child;
    w->rings = heap_block_tag(game_calloc(uranus_orbit_rings * sizeof(UOr2Ring)), str_uranus_orbit_c, 482);
    u_or2_rings_init(w, w->nodes->child->child);
    jit = w->jitter = heap_block_tag(game_malloc(uranus_orbit_points * sizeof(Vec3) * uranus_orbit_rings), str_uranus_orbit_c, 487);
    for (i = 0; i < uranus_orbit_points * uranus_orbit_rings; i++, jit++) {
        jit->x = RAND_FLOAT_A() * (cmd_p2_dp + cmd_p2_dp) - cmd_p2_dp;
        jit->y = RAND_FLOAT_A() * (cmd_p2_dp + cmd_p2_dp) - cmd_p2_dp;
        jit->z = RAND_FLOAT_A() * (cmd_p2_dp + cmd_p2_dp) - cmd_p2_dp;
    }
    return w;
}
