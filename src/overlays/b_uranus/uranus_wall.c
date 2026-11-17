/*
 * Uranus boss wall form: the cubes stand in six spinning rings on the floor, three each side.
 * The rings slide past the player while the laser fires, then request a form switch.
 */

#include "types_gw.h"
#include "b_uranus.h"
#include "mathdef.h"
#include "gw_varedit.h"
#include "player.h"
#include "uranus_wall.h"
#include "uranus.h"
#include "uranus_bullet.h"
#include "uranus_map.h"
#include "uranus_part.h"
#include "uranus_snake.h"

extern char uranus_wall_vardef[];
extern GwVarDef uranus_wall_vardefs[];
extern struct GwVarNode *uranus_wall_vars_group;
extern struct TaskDesc uranus_wall_desc;
extern float cmd_cube_offz;
extern float cmd_mv;
extern float cmd_cube_offz_s;
extern const float cmd_cube_offz_f;
extern float cmd_cube_offy;
extern float uranus_wall_spin;
extern float uranus_wall_ring0_x;
extern float uranus_wall_ring0_r;
extern float uranus_wall_ring1_x;
extern float uranus_wall_ring1_r;
extern float uranus_wall_ring2_x;
extern float uranus_wall_ring2_r;

/* 84 bytes */
typedef struct UranusWallWork {
    char pad_00[4];
    UranusNode *nodes;          /* 0x04 */
    UranusPartWork *pm;
    struct UranusMapWork *map;
    struct VenusClWork *laser;
    ModelPart **parts;
    Vec3 ofs;
    Vec3 pos;
    int ring_ang;
    int broken;
    int sw_req;
    char pad_3c[8];
    float ofs_z_to;             /* 0x44 */
    float spd;
    float vel_z;
    float floor_y;
} UranusWallWork;

/* 0x00cb4b18 */
void uranus_wall_vars_add(void)
{
    uranus_wall_vars_group = gw_varedit_add(&gw_varedit_root, 0, uranus_wall_vardef);
    gw_varedit_add_table(&gw_varedit_root, uranus_wall_vars_group, uranus_wall_vardefs);
}

/* 0x00cb4b68 */
void uranus_wall_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)uranus_wall_vardef);
}

/* 0x00cb4b80 */
void uranus_wall_ring_place(UranusNode **link, UranusNode **node, Vec3 *ctr, int start_ang, int cnt, float rad)
{
    int step = 0xffff / cnt;
    int i;
    Vec3 v;
    Vec3 *pt;
    float sc[2];

    if (!*node)
        *node = (*link)->child;
    for (i = 0; i < cnt; i++) {
        v.y = rad;
        v.x = v.z = 0.0f;
        ROT_X(step * i + start_ang, sc, v.y, v.z);
        pt = (Vec3 *)(*node)->data;
        *node = (*node)->next;
        pt->x = ctr->x + v.x;
        pt->y = ctr->y + v.y;
        pt->z = ctr->z + v.z;
        if (!*node) {
            *link = (*link)->next;
            if (*link)
                *node = (*link)->child;
        }
    }
}

/* 0x00cb4cf8 */
void uranus_wall_set_laser(UranusWallWork *w, int on)
{
    if (on) {
        if (!w->laser)
            w->laser = venus_cl_create(w->pm, w->parts);
    } else if (w->laser) {
        venus_cl_stop(w->laser);
        w->laser = 0;
    }
}

/* 0x00cb4d58 */
void uranus_wall_exec(UranusWallWork *w)
{
    Vec3 v;
    UranusPtDir ray;
    Vec3 hit, nrm, ctr;
    Vec3 *pos;
    UranusNode *link, *node;
    float slide;

    if (!w->laser || (TASK_OF(w->laser)->flags & TASK_F_END))
        w->laser = 0;
    if (uranus_pm_sw_is_live(w->pm, (int)w)) {
        if (!uranus_pm_is_core(w->pm))
            uranus_wall_set_laser(w, 1);
    } else
        uranus_wall_set_laser(w, 0);
    if (uranus_pm_sw_is_ready(w->pm, (int)w) && w->sw_req)
        w->sw_req = 0;
    slide = w->ofs_z_to;
    if (0.0f < slide) {
        if (slide < w->ofs.z) {
            w->vel_z = 0.0f;
            uranus_pm_sw_request(w->pm, (int)w, 10);
        } else
            w->vel_z = w->spd;
    } else if (slide < 0.0f) {
        if (w->ofs.z < slide) {
            w->vel_z = 0.0f;
            uranus_pm_sw_request(w->pm, (int)w, 10);
        } else
            w->vel_z = -w->spd;
    } else
        goto skip;
    w->ofs.z += w->vel_z;
skip:
    pos = &w->pos;
    player_get_pos(pos);
    v3_sum(pos, &w->ofs, pos);
    ray.p.y = pos->y - 200.0f;
    ray.p.z = pos->z;
    ray.p.x = pos->x;
    if (uranus_map_col_check(w->map, &ray.p, &hit.x, &nrm.x, 0.0f, 1040))
        w->floor_y = hit.y + cmd_cube_offy;
    w->pos.y = w->floor_y;
    uranus_map_get_dir(&v);
    w->ring_ang += (int)(uranus_wall_spin * v.z / (uranus_wall_ring0_r * TWO_PI) * 65535.0f);
    link = w->nodes->child->child;
    node = 0;
    ctr.y = pos->y;
    ctr.z = pos->z;
    ctr.x = uranus_wall_ring0_x;
    uranus_wall_ring_place(&link, &node, &ctr, w->ring_ang, 32, uranus_wall_ring0_r);
    ctr.x = -uranus_wall_ring0_x;
    uranus_wall_ring_place(&link, &node, &ctr, w->ring_ang, 32, uranus_wall_ring0_r);
    ctr.x = uranus_wall_ring1_x;
    uranus_wall_ring_place(&link, &node, &ctr, w->ring_ang, 24, uranus_wall_ring1_r);
    ctr.x = -uranus_wall_ring1_x;
    uranus_wall_ring_place(&link, &node, &ctr, w->ring_ang, 24, uranus_wall_ring1_r);
    ctr.x = uranus_wall_ring2_x;
    uranus_wall_ring_place(&link, &node, &ctr, w->ring_ang, 16, uranus_wall_ring2_r);
    ctr.x = -uranus_wall_ring2_x;
    uranus_wall_ring_place(&link, &node, &ctr, w->ring_ang, 16, uranus_wall_ring2_r);
    for (link = w->nodes->child->child; link; link = link->next)
        v3_copy((Vec3 *)link->data, &w->pos);
    v3_copy((Vec3 *)w->nodes->child->data, &w->pos);
}

/* 0x00cb5150 */
void uranus_wall_nodes_build(UranusWallWork *w, int data, char *link_pts, char *pt_pts)
{
    UranusNode *root, *link;
    int i, j;

    w->nodes = uranus_node_new();
    root = uranus_node_add_child(w->nodes);
    root->data = data;
    for (i = 9; i; i--) {
        link = uranus_node_add_child(root);
        link->data = (int)link_pts;
        link_pts += 12;
        for (j = 16; j; j--) {
            uranus_node_add_child(link)->data = (int)pt_pts;
            pt_pts += 12;
        }
    }
}

/* 0x00cb5210 */
void uranus_wall_release(UranusWallWork *w)
{
    uranus_node_free_tree(w->nodes);
}

/* 0x00cb5218 */
void uranus_wall_switch_cb(UranusWallWork *w)
{
    Vec3 *obj_pos;
    Vec3 core, pl;

    if (!w->broken) {
        uranus_pm_break_objs(w->pm);
        w->broken = 1;
    }
    w->sw_req = 1;
    if (uranus_pm_is_core(w->pm)) {
        float t;

        do { t = cmd_cube_offz_f; } while (0); /* steer(barrier) */
        w->spd = 0.0f;
        w->ofs.z = t;
        w->ofs_z_to = -t;
        return;
    }
    obj_pos = uranus_pm_get_obj0_pos(w->pm);
    w->spd = cmd_mv;
    player_get_frame_vel(&core);
    v3_addto(&core, obj_pos);
    player_get_pos(&pl);
    v3_sub(&core, &core, &pl);
    if (core.z > 0.0f) {
        w->ofs.z = cmd_cube_offz_s;
        w->ofs_z_to = -cmd_cube_offz;
    } else {
        w->ofs.z = -cmd_cube_offz_s;
        w->ofs_z_to = cmd_cube_offz;
    }
    boss_se_play(17, 0, (int)obj_pos);
}

/* 0x00cb5370 */
UranusWallWork *uranus_wall_create(void *parent, UranusPartWork *pm, Vec3 **pts, UranusNode **out, int slot, ModelPart **parts)
{
    UranusWallWork *w = task_create(parent, &uranus_wall_desc, 84);

    w->parts = parts;
    w->pm = pm;
    w->map = uranus_pm_get_map(pm);
    uranus_wall_nodes_build(w, (int)pts[0], (char *)pts[1], (char *)pts[2]);
    {
        float t = cmd_mv;

        w->ofs.z -= cmd_cube_offz;
        w->spd = t;
    }
    if (out)
        *out = w->nodes->child;
    uranus_wall_exec(w);
    uranus_pm_set_cb(w->pm, (int)w, (void (*)(int))uranus_wall_switch_cb, slot);
    return w;
}
