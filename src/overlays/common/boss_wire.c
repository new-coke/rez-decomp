/*
 * Boss wireframes: models drawn as edges from their edge lists, still or posed, in set colours and
 * widths or pulsing with the beat. Also the debug wire editor (picks and toggles edges, saves .njw
 * files).
 */

#include "types_common.h"

#include "mathdef.h"
#include "types_overlays.h"
#include "anim.h"
#include "camera.h"
#include "color.h"
#include "gw_mouse.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "math_util.h"
#include "model.h"
#include "nj_cnk_direct.h"
#include "nj_matrix.h"
#include "pcfile.h"
#include "player.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "line_batch.h"
#include "boss_wire.h"
#include "boss_common.h"
#include "system.h"
#include "debug.h"

static inline void v3_set(Vec3 *v, float x, float y, float z)
{
    v->x = x;
    v->y = y;
    v->z = z;
}

#define ROT_Y(ang, sc, x, z) \
    do { \
        float rot_u = x, rot_v = z; \
        ulSinCos(ang, &sc[0], &sc[1]); \
        x = rot_u * sc[1] + rot_v * sc[0]; \
        z = -rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

extern ScreenCfg screen_cfg;

extern int ulMatrixPush(void);

extern struct GwVarNode *gw_varedit_root;

extern unsigned char kbd_state[];

extern CmTexAnimeWork *tex_anime_work;

extern SVtx gx_vertex_buffer[];
extern char wire_edit_vardef[];
extern GwVarDef wire_edit_vardefs[];
extern struct GwVarNode *wire_edit_vars_group;
extern void **wire_draw_list;
extern unsigned int wire_color;
extern unsigned int wire_edge_color;
extern float wire_width;

extern int cmd_r_sw, cmd_m_sw, wire_edit_cycle;
extern float cmd_ed_len, cmd_ed_yrad, cmd_mot_v;
extern struct TaskDesc wire_edit_desc;
extern char str_njw[];
extern char str_boss_wire_c[];

/* running edge index of the wire walks */
typedef enum { WIRE_INDEX_FIRST } WireIndex;

extern WireIndex wire_edge_index;
extern int wire_list;
extern Vec3 *wire_edges_a;
extern Vec3 *wire_edges_b;
extern int wire_edges_count;
extern float wire_edge_dist2;
extern unsigned char wire_mode;
extern char str_pvr_err[];
extern int wire_nearest_x, wire_nearest_y;
extern int wire_nearest_index, wire_nearest_dist2;
extern char str_draw_model[];
extern char str_cm_dc_wire_draw_object[];
extern float wire_motion_time;
extern int wire_edge_flags;
extern char str_njwm[];
extern char *wire_out;
extern char *wire_out_base;
extern char wire_key[];
extern float wire_pick_pos[3];
extern int wire_picked_pair[2];
extern int wire_pick_on;

/* 0x00cc5128 b_earth, 0x00cd7668 b_eden, 0x00cc3cd0 b_mars, 0x00cc8400 b_uranus, 0x00ccb138 b_venus */
void wire_pvr_debug(void)
{
    CmTexAnimeWork *w = tex_anime_work;

    if (tex_anime_running_get()) {
        debug_set_origin(10, 30);
        debug_printf(str_pvr_err, w->pvr, w->err);
    }
}

/* 0x00cc5180 b_earth, 0x00cd76c0 b_eden, 0x00cc3d28 b_mars, 0x00cc8458 b_uranus, 0x00ccb190 b_venus */
void wire_edit_vars_add(void)
{
    wire_edit_vars_group = gw_varedit_add(&gw_varedit_root, 0, wire_edit_vardef);
    gw_varedit_add_table(&gw_varedit_root, wire_edit_vars_group, wire_edit_vardefs);
}

/* 0x00cc51d0 b_earth, 0x00cd7710 b_eden, 0x00cc3d78 b_mars, 0x00cc84a8 b_uranus, 0x00ccb1e0 b_venus */
void wire_edit_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)wire_edit_vardef);
}

/* 0x00cc51e8 b_earth, 0x00cd7728 b_eden, 0x00cc3d90 b_mars, 0x00cc84c0 b_uranus, 0x00ccb1f8 b_venus */
int wire_edge_known(float ax, float ay, float az, float bx, float by, float bz)
{
    int i;
    Vec3 *beg, *end;
    float dsq, rsq;

    for (i = 0; i < wire_edges_count; i++) {
        beg = &wire_edges_a[i];
        end = &wire_edges_b[i];
        rsq = wire_edge_dist2;
        dsq = (ax - beg->x) * (ax - beg->x) + (ay - beg->y) * (ay - beg->y) + (az - beg->z) * (az - beg->z);
        if (dsq < rsq) {
            dsq = (bx - end->x) * (bx - end->x) + (by - end->y) * (by - end->y) + (bz - end->z) * (bz - end->z);
        } else {
            if (!((ax - end->x) * (ax - end->x) + (ay - end->y) * (ay - end->y) + (az - end->z) * (az - end->z) < rsq))
                continue;
            dsq = (bx - beg->x) * (bx - beg->x) + (by - beg->y) * (by - beg->y) + (bz - beg->z) * (bz - beg->z);
        }
        if (dsq < rsq)
            return 1;
    }
    return 0;
}

/* 0x00cc5318 b_earth, 0x00cd7858 b_eden, 0x00cc3ec0 b_mars, 0x00cc85f0 b_uranus, 0x00ccb328 b_venus */
void wire_nearest_reset(void)
{
    wire_nearest_dist2 = 10000000;
}

/* 0x00cc5330 b_earth, 0x00cd7870 b_eden, 0x00cc3ed8 b_mars, 0x00cc8608 b_uranus, 0x00ccb340 b_venus */
void wire_nearest_check(int x, int y, int idx)
{
    int dsq;

    x -= wire_nearest_x;
    y -= wire_nearest_y;
    dsq = x * x + y * y;
    if (dsq < wire_nearest_dist2) {
        wire_nearest_dist2 = dsq;
        wire_nearest_index = idx;
    }
}

/* 0x00cc5378 b_earth, 0x00cd78b8 b_eden, 0x00cc3f20 b_mars, 0x00cc8650 b_uranus, 0x00ccb388 b_venus */
void wire_unproject(float *out, float x, float y, float z)
{
    float *mtx = camera_get_inv_mtx();

    out[2] = -1.0f / z;
    out[0] = (x - screen_cfg.cx) / (z * screen_cfg.sx);
    out[1] = (y - screen_cfg.cy) / (z * screen_cfg.sy);
    ulMatrixApplyPoint(mtx, out, out);
}

/* 0x00cc5430 b_earth, 0x00cd7970 b_eden, 0x00cc3fd8 b_mars, 0x00cc8708 b_uranus, 0x00ccb440 b_venus */
/* steer(cast) */
void *wire_draw_edges(void *lines, int *edges, BossV4 *verts, float width)
{
    unsigned short *p = (unsigned short *)(edges + 2);
    int mode;
    BossV4 *vtx;
    float ax, ay, az, bx, by, bz;
    int n;

    n = edges[1];
    if (n > 0) do {
        vtx = &verts[*p++];
        ax = vtx->x;
        ay = vtx->y;
        az = vtx->z;
        vtx = &verts[*p++];
        bx = vtx->x;
        by = vtx->y;
        bz = vtx->z;
        if ((signed char)wire_mode == 4) {
            v3_set(&wire_edges_a[wire_edge_index], ax, ay, az);
            v3_set(&wire_edges_b[wire_edge_index], bx, by, bz);
        }
        if ((signed char)wire_mode == 8 && !wire_edge_known(ax, ay, az, bx, by, bz))
            ((unsigned char *)wire_edge_flags)[wire_edge_index] &= ~1;
        mode = wire_mode;
        if ((signed char)wire_mode == 2 && (mode & ((unsigned char *)wire_edge_flags)[wire_edge_index])) {
            p -= 2;
            wire_picked_pair[0] = *p++;
            wire_picked_pair[1] = *p++;
        }
        if (mode & ((unsigned char *)wire_edge_flags)[wire_edge_index])
            lines = ((void *(*)(void *, float, float, float, float, float, float, float))line_add_f_w)(lines, ax, ay, az, bx, by, bz, width);
        if (wire_pick_on & 1)
            wire_unproject(wire_pick_pos, (ax + bx) * 0.5f, (ay + by) * 0.5f, (az + bz) * 0.5f);
        if (wire_pick_on == 2 && bz <= 1.0f && 0.0f < bz)
            wire_nearest_check((ax + bx) * 0.5f, (ay + by) * 0.5f, wire_edge_index);
        wire_edge_index++;
    } while (--n);
    return lines;
}

/* 0x00cc5708 b_earth, 0x00cd7c48 b_eden, 0x00cc42b0 b_mars, 0x00cc89e0 b_uranus, 0x00ccb718 b_venus */
int draw_model(int acc, NjCnkModel *model)
{
    printf(str_draw_model);
    wire_draw_edges(0, 0, 0, 0.0f);
    return 0;
}

/* 0x00cc5740 b_earth, 0x00cd7c80 b_eden, 0x00cc42e8 b_mars, 0x00cc8a18 b_uranus, 0x00ccb750 b_venus */
/* steer(cast) */
int wire_tree_walk(int acc, WireObj *obj)
{
    unsigned int flags;
    NjCnkModel *model;
    float *pos;

    do {
        flags = obj->flags;
        model = obj->model;
        if ((flags & 7) == 7) {
            if (!(flags & 8))
                acc = draw_model(acc, model);
            wire_list = *(int *)wire_list;
            if (!(flags & 0x10))
                acc = wire_tree_walk(acc, obj->child);
        } else {
            njPushMatrixEx();
            if (!(flags & 1)) {
                pos = obj->pos;
                do {
                    vu0_matrix_translate(ulMatrixCur, pos[0], pos[1], pos[2]);
                } while (0);
            }
            if (!(flags & 2)) {
                if (!(flags & 0x1000)) {
                    if (flags & 0x400)
                        quat_to_matrix_split(obj->qa, (float *)obj->ang);
                    else
                        ((void (*)(int *, int))nj_matrix_rotate_cur_ang3)(obj->ang, flags & 0x20);
                }
            }
            if (!(flags & 4))
                vu0_matrix_scale(ulMatrixCur, obj->scl[0], obj->scl[1], obj->scl[2]);
            if (!(flags & 8))
                acc = draw_model(acc, model);
            wire_list = *(int *)wire_list;
            if (!(flags & 0x10))
                acc = wire_tree_walk(acc, obj->child);
            njPopMatrixEx();
        }
        obj = obj->sibling;
    } while (obj);
    return acc;
}

/* 0x00cc58d8 b_earth, 0x00cd7e18 b_eden, 0x00cc4480 b_mars, 0x00cc8bb0 b_uranus, 0x00ccb8e8 b_venus */
void cmDcWireDrawObject(void *model, int edges, char *flags, unsigned int color, float width)
{
    printf(str_cm_dc_wire_draw_object);
}

/* 0x00cc58e8 b_earth, 0x00cd7e28 b_eden, 0x00cc4490 b_mars, 0x00cc8bc0 b_uranus, 0x00ccb8f8 b_venus */
int wire_tree_walk_motion(int acc, WireObj *obj)
{
    unsigned int flags;

    do {
        flags = obj->flags;
        ulMatrixPush();
        anim_apply_node((struct AnimNode *)obj);
        if (!(flags & 8))
            acc = draw_model(acc, obj->model);
        wire_list = *(int *)wire_list;
        if (!(flags & 0x10))
            acc = wire_tree_walk_motion(acc, obj->child);
        ulMatrixPop();
        obj = obj->sibling;
    } while (obj);
    return acc;
}

/* 0x00cc5990 b_earth, 0x00cd7ed0 b_eden, 0x00cc4538 b_mars, 0x00cc8c68 b_uranus, 0x00ccb9a0 b_venus */
void wire_draw_motion(WireObj *obj, void *src, int edge_flags, int edges, unsigned int color, float t, float motion_time)
{
    wire_motion_time = motion_time;
    wire_edge_flags = edge_flags;
    anim_start(src, t);
    if (edges) {
        int lines = line_set_color(color);

        wire_list = edges;
        line_flush(wire_tree_walk_motion(lines, obj));
    }
}

/* 0x00cc5a20 b_earth, 0x00cd7f60 b_eden, 0x00cc45c8 b_mars, 0x00cc8cf8 b_uranus, 0x00ccba30 b_venus */
void wire_count_edges(WireObj *obj)
{
    do {
        wire_edge_index += ((int *)wire_list)[1];
        wire_list = (int)*(void **)wire_list;
        if (!(obj->flags & 0x10))
            wire_count_edges(obj->child);
        obj = obj->sibling;
    } while (obj);
}

/* 0x00cc5a98 b_earth, 0x00cd7fd8 b_eden, 0x00cc4640 b_mars, 0x00cc8d70 b_uranus, 0x00ccbaa8 b_venus */
void wire_count_enabled(WireObj *obj, int *count, int *total)
{
    int i, n, flags;

    do {
        flags = obj->flags;
        n = 0;
        for (i = 0; i < ((int *)wire_list)[1]; i++) {
            if (((unsigned char *)wire_edge_flags)[wire_edge_index + i] & 1) {
                (*count)++;
                n++;
            }
        }
        if (!n)
            (*count)++;
        (*total)++;
        wire_edge_index += ((int *)wire_list)[1];
        wire_list = (int)*(void **)wire_list;
        if (!(flags & 0x10))
            wire_count_enabled(obj->child, count, total);
        obj = obj->sibling;
    } while (obj);
}

/* 0x00cc5be0 b_earth, 0x00cd8120 b_eden, 0x00cc4788 b_mars, 0x00cc8eb8 b_uranus, 0x00ccbbf0 b_venus */
void wire_write_edges(WireObj *obj)
{
    unsigned short *out;
    int i, n, flags;

    do {
        flags = obj->flags;
        n = 0;
        for (i = 0; i < ((int *)wire_list)[1]; i++)
            n += ((unsigned char *)wire_edge_flags)[wire_edge_index + i] & 1;
        ((int *)wire_out)[1] = n;
        out = (unsigned short *)(wire_out + 8);
        for (i = 0; i < ((int *)wire_list)[1]; i++) {
            if (((unsigned char *)wire_edge_flags)[wire_edge_index + i] & 1) {
                *out++ = ((WireEdgeBlk *)wire_list)->e[i * 2];
                *out++ = ((WireEdgeBlk *)wire_list)->e[i * 2 + 1];
            }
        }
        if (!n) {
            *out++ = 0;
            *out++ = 0;
        }
        wire_edge_index += ((int *)wire_list)[1];
        wire_list = (int)*(void **)wire_list;
        if (wire_list) {
            *(int *)wire_out = (char *)out - wire_out_base;
            *(int *)&wire_out = (int)out;
        } else {
            *(int *)wire_out = 0;
        }
        if (!(flags & 0x10))
            wire_write_edges(obj->child);
        obj = obj->sibling;
    } while (obj);
}

/* 0x00cc5db0 b_earth, 0x00cd82f0 b_eden, 0x00cc4958 b_mars, 0x00cc9088 b_uranus, 0x00ccbdc0 b_venus */
void *wire_build_njw(void *model, void *data, char *flags, int *size, int mode)
{
    int cnt[2];

    wire_edge_flags = (int)flags;
    cnt[0] = 0;
    cnt[1] = 0;
    wire_list = (int)data;
    wire_edge_index = 0;
    wire_count_enabled(model, &cnt[0], &cnt[1]);
    wire_list = (int)data;
    wire_edge_index = 0;
    if (mode) {
        mode = cnt[1] * 16 + cnt[0] * 4 + 8;
        *size = mode;
        data = heap_block_tag(game_calloc(mode), str_boss_wire_c, 490);
        sprintf(data, str_njwm);
        ((int *)data)[1] = mode;
        wire_out_base = wire_out = (char *)data + 8;
    } else {
        mode = cnt[1] * 16 + cnt[0] * 4;
        *size = mode;
        wire_out_base = wire_out = data = heap_block_tag(game_calloc(mode), str_boss_wire_c, 496);
    }
    wire_write_edges(model);
    return data;
}

/* 0x00cc5ef8 b_earth, 0x00cd8438 b_eden, 0x00cc4aa0 b_mars, 0x00cc91d0 b_uranus, 0x00ccbf08 b_venus */
int *wire_offsets_to_ptrs(int *base)
{
    int *blk = base;

    if (base && *base) {
        do {
            *blk += (int)base;
            blk = (int *)*blk;
        } while (blk && *blk);
    }
    return base;
}

/* 0x00cc5f38 b_earth, 0x00cd8478 b_eden, 0x00cc4ae0 b_mars, 0x00cc9210 b_uranus, 0x00ccbf48 b_venus */
void wire_njw_relocate(int *data)
{
    wire_offsets_to_ptrs(data);
}

/*
 * Debug: keys toggle view, edit mode and motion, save or rebuild the edge image; in mode 1 the pad
 * picks and toggles edges, in mode 2 the mouse. The view orbits the model.
 */
/* 0x00cc5f40 b_earth, 0x00cd8480 b_eden, 0x00cc4ae8 b_mars, 0x00cc9218 b_uranus, 0x00ccbf50 b_venus */
void wire_edit_exec(WireEditWorkView *w)
{
    Vec3 ofs, eye;
    float sc[2];
    float z;
    unsigned char *sel_flag;

    if (!w->motion)
        cmd_m_sw = 0;
    if (cmd_m_sw == 1) {
        w->frame += cmd_mot_v;
        if (w->frames < w->frame)
            w->frame -= w->frames;
    }
    {
        int key = kbd_state[6];

        w->flags[w->sel] |= 2;
        switch (key) {
        case 6:
            cmd_r_sw ^= 1;
            break;
        case 16:
            if (w->motion) {
                if (++cmd_m_sw >= 3)
                    cmd_m_sw = 0;
            }
            break;
        case 8:
            if (++wire_edit_cycle >= 3)
                wire_edit_cycle = 0;
            break;
        case 22:
            if (w->data)
                heap_free(w->data, 0);
            w->data = wire_build_njw(w->model, (void *)w->c, (char *)w->flags, &w->size, 1);
            pcfile_save_set(w->src, w->data, w->size);
            heap_free(w->data, 0);
            w->data = 0;
            break;
        case 17:
            if (w->data)
                heap_free(w->data, 0);
            w->data = wire_build_njw(w->model, (void *)w->c, (char *)w->flags, &w->size, 0);
            wire_offsets_to_ptrs(w->data);
            break;
        case 75:
            w->yaw_spd += cmd_ed_yrad;
            break;
        case 78:
            w->yaw_spd -= cmd_ed_yrad;
            break;
        case 82:
            w->dist += 1.0f;
            break;
        case 81:
            w->dist -= 1.0f;
            break;
        }
    }
    if (wire_edit_cycle == 1) {
        int key = kbd_state[6];

        w->flags[w->sel] |= 2;
        switch (key) {
        case 80:
            w->flags[w->sel] &= ~2;
            if (--w->sel < 0)
                w->sel = w->nflags - 1;
            break;
        case 79:
            w->flags[w->sel] &= ~2;
            if (++w->sel >= w->nflags)
                w->sel = 0;
            break;
        case 40:
            if (w->flags[w->sel] & 1)
                w->flags[w->sel] &= ~1;
            else
                w->flags[w->sel] |= 1;
            break;
        }
    } else if (wire_edit_cycle == 2) {
        if (wire_nearest_index != w->sel) {
            w->flags[w->sel] &= ~2;
            w->sel = wire_nearest_index;
            w->flags[w->sel] |= 2;
        }
        if (gw_mouse_get_press() & 7) {
            sel_flag = &w->flags[w->sel];
            *sel_flag = (*sel_flag & 1) ? *sel_flag & ~1 : *sel_flag | 1;
        }
        wire_nearest_reset();
        gw_mouse_get_pos(&wire_nearest_x, &wire_nearest_y);
    }
    if (cmd_r_sw) {
        z = -cmd_ed_len + w->dist;
        w->yaw += w->yaw_spd + cmd_ed_yrad;
        ofs.x = ofs.y = 0.0f;
        ofs.z = z;
        ROT_Y((int)DEG_TO_ANG(w->yaw), sc, ofs.x, ofs.z);
        eye.x = wire_pick_pos[0] + ofs.x;
        eye.y = wire_pick_pos[1] + ofs.y;
        eye.z = wire_pick_pos[2] + ofs.z;
        player_set_cam_pos(&eye);
        player_set_cam_dir(&ofs);
    }
}

/* 0x00cc6420 b_earth, 0x00cd8960 b_eden, 0x00cc4fc8 b_mars, 0x00cc96f8 b_uranus, 0x00ccc430 b_venus */
void wire_edit_disp(void **w)
{
    ulGxSetTextureList(w[1]);
}

/* 0x00cc6428 b_earth, 0x00cd8968 b_eden, 0x00cc4fd0 b_mars, 0x00cc9700 b_uranus, 0x00ccc438 b_venus */
void wire_edit_release(WireEditWork *w)
{
    heap_free(w->flags, 0);
    if (w->buf)
        heap_free((void *)w->buf, 0);
    wire_edit_vars_remove();
}

/* 0x00cc6468 b_earth, 0x00cd89a8 b_eden, 0x00cc5010 b_mars, 0x00cc9740 b_uranus, 0x00ccc478 b_venus */
char *name_find_dot(char *name)
{
    while (*name != '.') {
        name++;
        if (!*name)
            return 0;
    }
    return name;
}

/* 0x00cc64a8 b_earth, 0x00cd89e8 b_eden, 0x00cc5050 b_mars, 0x00cc9780 b_uranus, 0x00ccc4b8 b_venus */
WireEditWork *wire_edit_create(void *parent, void *model, int edges, unsigned int *motion, char *name)
{
    WireEditWork *w = task_create_inherit(parent, &wire_edit_desc, 128);
    char *ext;
    int i;

    w->motion = motion;
    w->model = model;
    w->c = edges;
    sprintf(w->name, name);
    w->src = name;
    ext = name_find_dot(name);
    if (ext)
        sprintf(ext, str_njw);
    if (w->motion)
        w->frames = w->motion[1] - 1;
    wire_edge_index = 0;
    w->frame = 0.0f;
    wire_list = edges;
    wire_count_edges(model);
    w->nflags = wire_edge_index;
    w->flags = heap_block_tag(game_calloc(w->nflags), str_boss_wire_c, 767);
    for (i = 0; i < w->nflags; i++)
        w->flags[i] = 1;
    wire_edit_vars_add();
    return w;
}

/* 0x00cc6608 b_earth, 0x00cd8b48 b_eden, 0x00cc51b0 b_mars, 0x00cc98e0 b_uranus, 0x00ccc618 b_venus */
void wire_edges_build(void *model, int edges)
{
    char *flags;
    int n, i;

    wire_list = edges;
    wire_edge_index = 0;
    wire_count_edges(model);
    n = wire_edge_index;
    flags = heap_block_tag(game_calloc(n), str_boss_wire_c, 781);
    for (i = 0; i < n; i++)
        flags[i] = 1;
    wire_edges_count = n;
    wire_edges_a = heap_block_tag(game_calloc(n * 12), str_boss_wire_c, 784);
    wire_edges_b = heap_block_tag(game_calloc(n * 12), str_boss_wire_c, 785);
    wire_mode = 4;
    wire_edge_index = 0;
    cmDcWireDrawObject(model, edges, flags, COL_WHITE, 2.0f);
    heap_free(flags, 0);
}

/* 0x00cc6778 b_earth, 0x00cd8cb8 b_eden, 0x00cc5320 b_mars, 0x00cc9a50 b_uranus, 0x00ccc788 b_venus */
void wire_edges_free(void)
{
    heap_free(wire_edges_a, 0);
    heap_free(wire_edges_b, 0);
    wire_edges_count = 0;
}

/* 0x00cc67b8 b_earth, 0x00cd8cf8 b_eden, 0x00cc5360 b_mars, 0x00cc9a90 b_uranus, 0x00ccc7c8 b_venus */
void wire_edges_rebuild(WireEditWork *w, float rad)
{
    wire_mode = 8;
    wire_edge_dist2 = rad * rad;
    wire_edge_index = 0;
    cmDcWireDrawObject(w->model, w->c, w->flags, COL_WHITE, 2.0f);
}

/* 0x00cc6800 b_earth, 0x00cd8d40 b_eden, 0x00cc53a8 b_mars, 0x00cc9ad8 b_uranus, 0x00ccc810 b_venus */
int wire_create_none(void)
{
    return 0;
}

/* 0x00cc6808 b_earth, 0x00cd8d48 b_eden, 0x00cc53b0 b_mars, 0x00cc9ae0 b_uranus, 0x00ccc818 b_venus */
void wire_export_model(void *model, char *path)
{
    char name[64];
    int size;
    void *data, *out;
    char *flags, *ext;
    int n, i;

    data = model_load_wire_data(model);
    wire_edge_index = 0;
    wire_list = (int)data;
    wire_count_edges(model);
    n = wire_edge_index;
    flags = heap_block_tag(game_calloc(n), str_boss_wire_c, 887);
    for (i = 0; i < n; i++)
        flags[i] = 1;
    sprintf(name, path);
    out = name_find_dot(name);
    if (out)
        sprintf(out, str_njw);
    out = wire_build_njw(model, data, flags, &size, 1);
    pcfile_save_set(name, out, size);
    heap_free(out, 0);
    heap_free(flags, 0);
    model_free_block(data);
}

/* 0x00cc6948 b_earth, 0x00cd8e88 b_eden, 0x00cc54f0 b_mars, 0x00cc9c20 b_uranus, 0x00ccc958 b_venus */
/* steer(cast) */
void wire_export_list(void **bank, int *pairs, int n)
{
    int i;

    if (n > 0) {
        i = n;
        do {
            i--;
            wire_export_model((void *)*((int *(*)(void *, unsigned int))pnb_get_entry)(*bank, pairs[0]), (char *)pairs[1]);
            pairs += 2;
        } while (i);
    }
}

/* 0x00cc69b0 b_earth, 0x00cd8ef0 b_eden, 0x00cc5558 b_mars, 0x00cc9c88 b_uranus, 0x00ccc9c0 b_venus */
void wire_key_register(void)
{
    pnb_add_handler(wire_key);
}

/* 0x00cc69c0 b_earth, 0x00cd8f00 b_eden, 0x00cc5568 b_mars, 0x00cc9c98 b_uranus, 0x00ccc9d0 b_venus */
void wire_key_unregister(void)
{
    pnb_remove_handler(wire_key);
}

/* 0x00cc69d0 b_earth, 0x00cd8f10 b_eden, 0x00cc5578 b_mars, 0x00cc9ca8 b_uranus, 0x00ccc9e0 b_venus */
/* steer(cast) */
void wire_draw_screen_edges(void **edges, SVtx *verts)
{
    unsigned short *p = (unsigned short *)(edges + 2);
    SVtx *vtx;
    SVtxQuad col0, col1;
    float x0, y0, q0, x1, y1, q1;
    int n;

    n = (int)edges[1];
    if (n > 0) {
        do {
            vtx = &verts[*p++];
            if (((int *)vtx)[15] & 0xf) {
                p++;
                continue;
            }
            x0 = (vtx->scr[1] - 32768.0f) * 0.0625f + 320.0f;
            y0 = (vtx->scr[2] - 32768.0f) * 0.125f + 240.0f;
            q0 = vtx->rq;
            col0 = *(SVtxQuad *)vtx->col;
            vtx = &verts[*p++];
            if (((int *)vtx)[15] & 0xf) {
                p++;
                continue;
            }
            x1 = (vtx->scr[1] - 32768.0f) * 0.0625f + 320.0f;
            y1 = (vtx->scr[2] - 32768.0f) * 0.125f + 240.0f;
            q1 = vtx->rq;
            col1 = *(SVtxQuad *)vtx->col;
            line_flush(((void *(*)(void *, float, float, float, float, float, float, float))line_add_f_w)(line_set_color(boss_color_blend(wire_color, wire_edge_color, 0.5f)),
                          x0, y0, q0, x1, y1, q1, wire_width));
        } while (--n);
    }
}

/* 0x00cc6b88 b_earth, 0x00cd90c8 b_eden, 0x00cc5730 b_mars, 0x00cc9e60 b_uranus, 0x00cccb98 b_venus */
void wire_draw_model(NjCnkModel *model)
{
    void **edges = wire_draw_list;
    SVtx *verts = gx_vertex_buffer;

    if (model->vlist)
        nj_cnk_wire_vlist(model->vlist, verts);
    ulGxSetMatrix(0, ulMatrixCur);
    wire_draw_screen_edges(edges, verts);
}

/* 0x00cc6be8 b_earth, 0x00cd9128 b_eden, 0x00cc5790 b_mars, 0x00cc9ec0 b_uranus, 0x00cccbf8 b_venus */
/* steer(cast) */
void wire_tree_draw(WireObj *obj)
{
    unsigned int flags;
    NjCnkModel *model;
    float *pos;

    do {
        flags = obj->flags;
        model = obj->model;
        if ((flags & 7) == 7) {
            if (!(flags & 8))
                wire_draw_model(model);
            wire_draw_list = *wire_draw_list;
            if (!(flags & 0x10))
                wire_tree_draw(obj->child);
        } else {
            njPushMatrixEx();
            if (!(flags & 1)) {
                pos = obj->pos;
                do {
                    vu0_matrix_translate(ulMatrixCur, pos[0], pos[1], pos[2]);
                } while (0);
            }
            if (!(flags & 2)) {
                if (!(flags & 0x1000)) {
                    if (flags & 0x400)
                        quat_to_matrix_split(obj->qa, (float *)obj->ang);
                    else
                        ((void (*)(int *, int))nj_matrix_rotate_cur_ang3)(obj->ang, flags & 0x20);
                }
            }
            if (!(flags & 4))
                vu0_matrix_scale(ulMatrixCur, obj->scl[0], obj->scl[1], obj->scl[2]);
            if (!(flags & 8))
                wire_draw_model(model);
            wire_draw_list = *wire_draw_list;
            if (!(flags & 0x10))
                wire_tree_draw(obj->child);
            njPopMatrixEx();
        }
        obj = obj->sibling;
    } while (obj);
}

/* 0x00cc6d50 b_earth, 0x00cd9290 b_eden, 0x00cc58f8 b_mars, 0x00cca028 b_uranus, 0x00cccd60 b_venus */
void wire_draw_color(int model, int motion)
{
    draw_set_color(wire_color);
    model_draw_kind_list(model, motion);
}

/* 0x00cc6d90 b_earth, 0x00cd92d0 b_eden, 0x00cc5938 b_mars, 0x00cca068 b_uranus, 0x00cccda0 b_venus */
void wire_draw_color_b(void *obj, void *arg)
{
    draw_set_color(wire_color);
    model_obj_draw(obj, arg);
}

/* 0x00cc6dd0 b_earth, 0x00cd9310 b_eden, 0x00cc5978 b_mars, 0x00cca0a8 b_uranus, 0x00cccde0 b_venus */
void wire_tree_draw_motion(WireObj *obj)
{
    unsigned int flags;
    NjCnkModel *model;

    do {
        flags = obj->flags;
        model = obj->model;
        ulMatrixPush();
        anim_apply_node((struct AnimNode *)obj);
        if (!(flags & 8))
            wire_draw_model(model);
        wire_draw_list = *wire_draw_list;
        if (!(flags & 0x10))
            wire_tree_draw_motion(obj->child);
        ulMatrixPop();
        obj = obj->sibling;
    } while (obj);
}

/* 0x00cc6e68 b_earth, 0x00cd93a8 b_eden, 0x00cc5a10 b_mars, 0x00cca140 b_uranus, 0x00ccce78 b_venus */
void wire_draw_color_c(int tree, int motion, float t, int wire)
{
    draw_set_color(wire_color);
    model_node_walk_list_anim(tree, motion, t, wire);
}

/* 0x00cc6ec8 b_earth, 0x00cd9408 b_eden, 0x00cc5a70 b_mars, 0x00cca1a0 b_uranus, 0x00ccced8 b_venus */
void wire_colors_set(unsigned int color, unsigned int color2, float width)
{
    wire_edge_color = color2;
    wire_color = color;
    wire_width = width;
}

/* 0x00cc6ee8 b_earth, 0x00cd9428 b_eden, 0x00cc5a90 b_mars, 0x00cca1c0 b_uranus, 0x00cccef8 b_venus */
void wire_color_set(unsigned int color, float width)
{
    float a;
    int ai, r, g, b;

    wire_color = color;
    a = color_get_a(color);
    ai = a;
    r = (color & 0xff) * 0.54f;
    g = ((color >> 8) & 0xff) * 0.54f;
    b = ((color >> 16) & 0xff) * 0.54f;
    wire_edge_color = ((ai + 1) >> 1) << 24 | (b & 0xff) << 16 | (g & 0xff) << 8 | (r & 0xff);
    wire_width = width;
}

/* 0x00cc7040 b_earth, 0x00cd9580 b_eden, 0x00cc5be8 b_mars, 0x00cca318 b_uranus, 0x00ccd050 b_venus */
void part_draw_beat(ModelPart *part, int col_a, int col_b, float width)
{
    float old_lw = line_width_set(width);

    draw_set_color(color_lerp(col_a, col_b, beat_curve(1)));
    dc_object_draw(part);
    line_width_set(old_lw);
}

/* 0x00cc70b8 b_earth, 0x00cd95f8 b_eden, 0x00cc5c60 b_mars, 0x00cca390 b_uranus, 0x00ccd0c8 b_venus */
void part_draw_beat_frame(ModelPart *part, float t, int col_a, int col_b, float width)
{
    float old_lw = line_width_set(width);
    float old_time;

    draw_set_color(color_lerp(col_a, col_b, beat_curve(1)));
    old_time = part->frame;
    part->frame = t;
    dc_object_draw(part);
    line_width_set(old_lw);
    part->frame = old_time;
}

/* 0x00cc7158 b_earth, 0x00cd9698 b_eden, 0x00cc5d00 b_mars, 0x00cca430 b_uranus, 0x00ccd168 b_venus */
void part_draw_beat_motion(ModelPart *part, int motion, int col_a, int col_b, float t, float width)
{
    float old_lw = line_width_set(width);
    float old_time;
    int old_mot;

    draw_set_color(color_lerp(col_a, col_b, beat_curve(1)));
    old_time = part->frame;
    old_mot = part->clip;
    part->clip = motion;
    part->frame = t;
    dc_object_draw(part);
    line_width_set(old_lw);
    part->frame = old_time;
    part->clip = old_mot;
}
