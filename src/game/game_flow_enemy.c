/*
 * Game flow enemy events: enemy type table (16 built in, more per stage), the stage's enemy
 * records, and the FlowEdit page that places and tunes a record.
 */

#include "types_game.h"
#include "color.h"
#include "mathdef.h"
#include "debug.h"
#include "libc.h"
#include "gw_draw.h"
#include "gw_system.h"
#include "gw_window.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "game_flow_enemy.h"
#include "camera.h"
#include "debugdraw.h"
#include "game_flow_editor.h"
#include "heap.h"
#include "lockon.h"
#include "pad.h"
#include "player.h"
#include "task.h"
#include "system.h"
#include "passedit.h"

typedef struct GeRec {
    int w[16];
} GeRec;

typedef struct GameFlowEnemyGeRec {
    unsigned char type;
    unsigned char flags;
    short formation;
    int param[15];
} GameFlowEnemyGeRec;

typedef struct FlowEnemyEventNode {
    int w[16];
    struct FlowEnemyEventNode *next;
} FlowEnemyEventNode;

#define ROT_Z(ang, x, y) \
    do { \
        float rot_s, rot_c, rot_u = x, rot_v = y; \
        ulSinCos(ang, &rot_s, &rot_c); \
        x = rot_u * rot_c - rot_v * rot_s; y = rot_u * rot_s + rot_v * rot_c; \
    } while (0)

#define ROT_X(ang, y, z) \
    do { \
        float rot_s, rot_c, rot_u = y, rot_v = z; \
        ulSinCos(ang, &rot_s, &rot_c); \
        y = rot_u * rot_c - rot_v * rot_s; z = rot_u * rot_s + rot_v * rot_c; \
    } while (0)

#define ROT_Y(ang, x, z) \
    do { \
        float rot_s, rot_c, rot_u = x, rot_v = z; \
        ulSinCos(ang, &rot_s, &rot_c); \
        x = rot_u * rot_c + rot_v * rot_s; z = -rot_u * rot_s + rot_v * rot_c; \
    } while (0)

typedef struct GameFlowEnemyFlowEnemyEvent {
    unsigned char type;
    unsigned char flags;
    char pad_02[2];
    float pos[3];
} GameFlowEnemyFlowEnemyEvent;

/* steer(alias): the code that matched reads FlowEnemyEvent through these member types; one view
 * shared by those sites (src/game/game_flow_enemy.c). */
typedef struct FlowEnemyEventHeadView {
    int head;
    int pos_x;
    int pos_y;
    int pos_z;
    int ang1;
    int ang2;
    int ang0;
    float v[3];
    int rest[6];
} FlowEnemyEventHeadView;

/*@@HUB FlowEnemyEventHeadView@@*/

typedef struct { float x, y, z; } FlowVec3;

/* steer(alias): the code that matched reads FlowEnemyEvent through these member types; one view
 * shared by those sites (src/game/game_flow_enemy.c). */
typedef struct FlowEnemyEventTypeView {
    unsigned char type;
    unsigned char flags;
    unsigned short formation;
    float pos[3];
    int angB;
    int angC;
    int angA;
    float dir[3];
    int rest[6];
} FlowEnemyEventTypeView;

#define ROTZ(a, n) do { float rot_u = tip[0], rot_v = tip[1]; ulSinCos(a, &sc[n][0], &sc[n][1]); \
    tip[0] = rot_u * sc[n][1] - rot_v * sc[n][0]; tip[1] = rot_u * sc[n][0] + rot_v * sc[n][1]; } while (0)

#define ROTX(a, n) do { float rot_u = tip[1], rot_v = tip[2]; ulSinCos(a, &sc[n][0], &sc[n][1]); \
    tip[1] = rot_u * sc[n][1] - rot_v * sc[n][0]; tip[2] = rot_u * sc[n][0] + rot_v * sc[n][1]; } while (0)

#define ROTY(a, n) do { float rot_u = tip[0], rot_v = tip[2]; ulSinCos(a, &sc[n][0], &sc[n][1]); \
    tip[0] = rot_u * sc[n][1] + rot_v * sc[n][0]; tip[2] = -rot_u * sc[n][0] + rot_v * sc[n][1]; } while (0)

extern char str_enemy_task_init[];
extern struct TaskDesc enemy_root_desc;
extern struct TaskDesc enemy_flow_desc;
extern char str_enemy_task_exit[];
extern char str_enemy_type_is_old[];
extern char str_flow_enemy_data[];
extern char str_game_flow_enemy_file[];
extern int flow_enemy_builtin_types[];
extern GeRec flow_enemy_event_default;
extern char str_flow_enemy_call_nop[];
extern int flow_editor_undo_saved;
extern ScreenView screen_layout;
extern int debug_disp_level;
extern char str_flow_enemy_help_vec_to_ang[];
extern char str_flow_enemy_help_ang_to_vec[];
extern char str_flow_enemy_help_cam_to_target[];
extern char str_flow_enemy_help_target_to_cam[];
extern char str_flow_enemy_help_make[];
extern char str_flow_enemy_help_disp[];
extern char str_flow_enemy_help_undo[];
extern char str_flow_enemy_help_copy[];
extern char str_flow_enemy_help_paste[];
extern int flow_enemy_disp_on;
extern int flow_editor_param_colors[];
extern unsigned char kbd_state[];
extern unsigned int flow_editor_param_cursor;
extern FlowEnemyEventHeadView flow_editor_undo_buf;
extern FlowEnemyEventHeadView flow_editor_copy_buf;
extern FlowVec3 flow_editor_base_dir;
extern int ulMatrixPush(void);
extern void flow_editor_draw_gizmo(void *data, FlowEnemyEventTypeView *src);
extern int flow_editor_param_handle_input(void *data, FlowEnemyEventHeadView *blk, void *idx);

/* Root task in data[7], flow task under it in data[8]; built-in type init callbacks run. */
/* 0x0011dde8 */
void EnemyTaskInit(void *parent, int *data)
{
    DEBUG_PRINT(str_enemy_task_init);
    data[7] = task_create_inherit(parent, &enemy_root_desc, 8);
    data[8] = task_create_inherit((void *)data[7], &enemy_flow_desc, 8);
    flow_enemy_tbl_call_fn4_builtin(data);
}

/* Ends every enemy task under the root. */
/* 0x0011de58 */
void EnemyTaskExit(int *data)
{
    char *root;

    DEBUG_PRINT(str_enemy_task_exit);
    do {
        root = (char *)data[7];
        if (root != 0)
            TASK_OF(root)->flags |= TASK_F_END;
    } while (0);
}

/* Arg {type index, offset}; offsets from w[2], step 100. No version entry (type[11]): "old". */
/* 0x0011dea0 */
void flow_enemy_tbl_call_fn4_builtin(int *w)
{
    int left;
    int i = 0;
    int ofs = w[2];
    int *type;
    int buf[2];
    void (*fn)(int *);
    char *name;

    left = 16;
    do {
        type = flow_enemy_tbl_get(w, i);
        left--;
        if (type != 0) {
            fn = (void (*)(int *))type[1];
            if (fn != 0) {
                buf[0] = i;
                buf[1] = ofs;
                fn(buf);
            }
            if (type[11] == 0) {
                name = type[0] ? (char *)type[0] : str_empty;
                DEBUG_PRINTF(str_enemy_type_is_old, name);
            }
        }
        i++;
        ofs += 100;
    } while (left != 0);
}

/* The w[4] stage types (index 16 on), offsets from w[2] + 1600. */
/* 0x0011df60 */
void flow_enemy_tbl_call_fn4_regist(int *w)
{
    int ofs;
    int i;
    int n;
    int *type;
    int buf[2];
    void (*fn)(int *);
    char *name;

    n = w[4];
    i = 16;
    if (n > 0) {
        ofs = w[2];
        ofs += 1600;
        do {
            type = flow_enemy_tbl_get(w, i);
            if (type != 0) {
                fn = (void (*)(int *))type[1];
                if (fn != 0) {
                    buf[0] = i;
                    buf[1] = ofs;
                    fn(buf);
                }
                if (type[11] == 0) {
                    name = type[0] ? (char *)type[0] : str_empty;
                    DEBUG_PRINTF(str_enemy_type_is_old, name);
                }
            }
            i++;
            n--;
            ofs += 100;
        } while (n != 0);
    }
}

/* type[2] of every stage type. */
/* 0x0011e030 */
void flow_enemy_tbl_call_fn8_regist(int *data)
{
    int n = data[4];
    int i = 16;
    int *type;

    if (n > 0) {
        do {
            type = flow_enemy_tbl_get(data, i);
            n--;
            if (type != 0 && type[2] != 0)
                ((void (*)(void))type[2])();
            i++;
        } while (n != 0);
    }
}

/* 0x0011e0a0 */
void flow_enemy_kill_all(void)
{
    int *p = game_flow_get_data();

    if (p != 0)
        task_end_children(p[8]);
}

/* 3 (no such enemy) if missing or nameless, else 0. */
/* 0x0011e0d0 */
int flow_enemy_type_check(int data, int idx, int rec, int *type)
{
    if (type == 0 || *type == 0)
        return 3;
    return 0;
}

/* data[11]. */
/* 0x0011e0f8 */
void flow_enemy_events_free_array(int *data)
{
    data[12] = 0;
    heap_free(data[11], 0);
    data[11] = 0;
}

/* data[17]. */
/* 0x0011e130 */
void flow_enemy_events_free_list(int *data)
{
    int *node = (int *)data[17];
    int *dead;

    if (node != 0) {
        do {
            dead = node;
            node = (int *)node[16];
            heap_free(dead, 0);
        } while (node != 0);
    }
    data[17] = 0;
    data[18] = 0;
}

/* Returns the end of the copy. */
/* 0x0011e188 */
GeRec *flow_enemy_events_copy_array(int *data, GeRec *dst, int count)
{
    GeRec *src;
    int i;

    count = count < data[12] ? count : data[12];
    src = (GeRec *)data[11];
    for (i = 0; i < count; i++) {
        *dst = *src;
        src++;
        dst++;
    }
    return dst;
}

/* Returns the end of the source data. */
/* 0x0011e240 */
GeRec *flow_enemy_events_set_array(int *data, GeRec *src, int count)
{
    GeRec *dst;

    flow_enemy_events_free_array(data);
    if (count > 0) {
        data[12] = count;
        dst = heap_block_tag(game_malloc(count << 6), str_flow_enemy_data, -1);
        data[11] = (int)dst;
        for (; count != 0; count--) {
            *dst = *src;
            src++;
            dst++;
        }
    }
    return src;
}

/*
 * 52-byte entries; flags cleared, formation -1. Each copy is a full 64 bytes, so the tail holds the
 * next entry's start. Returns src.
 */
/* 0x0011e340 */
char *flow_enemy_events_set_array_v0(int *obj, char *src, int n)
{
    char *p = src;
    GameFlowEnemyGeRec *dst;

    flow_enemy_events_free_array(obj);
    if (n > 0) {
        obj[12] = n;
        dst = heap_block_tag(game_malloc(n << 6), str_game_flow_enemy_file, 399);
        obj[11] = (int)dst;
        for (; n != 0; n--) {
            *dst = *(GameFlowEnemyGeRec *)p;
            dst->flags = 0;
            dst->formation = -1;
            p += 52;
            dst++;
        }
    }
    return src;
}

/* 64-byte entries; flags cleared, formation -1. Returns src. */
/* 0x0011e458 */
char *flow_enemy_events_set_array_v1(int *obj, char *src, int n)
{
    char *p = src;
    GameFlowEnemyGeRec *dst;

    flow_enemy_events_free_array(obj);
    if (n > 0) {
        obj[12] = n;
        dst = heap_block_tag(game_malloc(n << 6), str_game_flow_enemy_file, 437);
        obj[11] = (int)dst;
        for (; n != 0; n--) {
            *dst = *(GameFlowEnemyGeRec *)p;
            dst->flags = 0;
            dst->formation = -1;
            p += 64;
            dst++;
        }
    }
    return src;
}

/* 0x0011e570 */
void flow_enemy_events_array_to_list(int *obj)
{
    int *tail = obj + 17;
    int n = obj[12];
    GeRec *src = (GeRec *)obj[11];
    int *node;
    int cur;

    flow_enemy_events_free_list(obj);
    obj[18] = n;
    if (n == 0) {
        obj[17] = 0;
        return;
    }
    do {
        node = heap_block_tag(game_malloc(68), str_game_flow_enemy_file, 470);
        *(GeRec *)node = *src;
        node[16] = 0;
        src++;
        cur = *tail;
        if (cur == 0)
            *tail = (int)node;
        else
            ((int *)cur)[16] = (int)node;
        n--;
        tail = &node[16];
    } while (n != 0);
}

/* 0x0011e690 */
void flow_enemy_events_list_to_array(int *obj)
{
    int n;
    FlowEnemyEventNode *src;
    GeRec *dst;

    flow_enemy_events_free_array(obj);
    if (obj[18] > 0) {
        obj[12] = obj[18];
        n = obj[18];
        src = (FlowEnemyEventNode *)obj[17];
        dst = heap_block_tag(game_malloc(n << 6), str_game_flow_enemy_file, 498);
        obj[11] = (int)dst;
        for (; n != 0; n--) {
            *dst = *(GeRec *)src;
            dst++;
            src = src->next;
        }
    }
}

/* Last link when the list is shorter. */
/* 0x0011e790 */
FlowEnemyEventNode **flow_enemy_event_list_slot(char *data, int idx)
{
    FlowEnemyEventNode **slot = (FlowEnemyEventNode **)(data + 68);
    FlowEnemyEventNode *node;

    if (*slot != 0) {
        for (;;) {
            if (idx == 0)
                return slot;
            node = *slot;
            idx--;
            slot = &node->next;
            if (node->next == 0)
                break;
        }
    }
    return slot;
}

/* 0x0011e7c8 */
FlowEnemyEventNode *flow_enemy_event_list_nth(char *data, int idx)
{
    return *flow_enemy_event_list_slot(data, idx);
}

/* 0..15 built in, then the data[4] stage types in data[3]. Off by one: allows idx == count. */
/* 0x0011e7e8 */
int flow_enemy_tbl_get(int *data, int idx)
{
    if (idx < 16)
        return flow_enemy_builtin_types[idx];
    idx -= 16;
    if (data[4] >= idx)
        return ((int *)data[3])[idx];
    return 0;
}

/* flow_enemy_event_default, in front of the camera. Returns its index. */
/* 0x0011e830 */
int flow_enemy_event_list_append(int *obj)
{
    FlowEnemyEventNode *p;
    FlowEnemyEventNode *node;
    int idx = 0;

    p = (FlowEnemyEventNode *)obj[17];
    node = heap_block_tag(game_malloc(68), str_game_flow_enemy_file, 586);
    *(GeRec *)node = flow_enemy_event_default;
    flow_enemy_place_at_camera(node);
    node->next = 0;
    if (p == 0) {
        obj[17] = (int)node;
    } else {
        idx = 1;
        while (p->next != 0) {
            p = p->next;
            idx++;
        }
        p->next = node;
    }
    obj[18]++;
    return idx;
}

/* Flow tags of kind 1 at or after n are moved down by one first. */
/* 0x0011e980 */
void flow_enemy_event_list_remove(char *obj, int n)
{
    FlowTag *tag;
    FlowEnemyEventNode **slot;
    FlowEnemyEventNode *cur;

    for (tag = *(FlowTag **)(obj + 60); tag != 0; tag = tag->next) {
        if (tag->kind == 1 && tag->idx >= n)
            tag->idx--;
    }
    slot = (FlowEnemyEventNode **)(obj + 68);
    while (n--) {
        FlowEnemyEventNode *node = *slot;
        if (node == 0)
            return;
        slot = &node->next;
    }
    cur = *slot;
    if (cur != 0) {
        *slot = cur->next;
        heap_free(cur, 0);
        *(int *)(obj + 72) -= 1;
    }
}

/* Player frame -> world: pos, dir rotated Z, X, Y; pos offset by frame position less velocity. */
/* 0x0011ea40 */
void flow_enemy_apply_camera(float *pos, float *dir, int *angles)
{
    float org[3];
    int ang[3];
    float vel[3];
    float x, y, z;

    player_get_frame_pos(org);
    player_get_frame_ang(ang);
    player_get_frame_vel(vel);
    {
        float ox = org[0], oy = org[1], oz = org[2];
        float vx = vel[0], vy = vel[1], vz = vel[2];
        org[0] = ox - vx;
        org[1] = oy - vy;
        org[2] = oz - vz;
    }
    do {
        x = pos[0];
        y = pos[1];
        z = pos[2];
    } while (0);
    ROT_Z(ang[2], x, y);
    ROT_X(ang[0], y, z);
    ROT_Y(ang[1], x, z);
    pos[0] = x + org[0];
    pos[1] = y + org[1];
    pos[2] = z + org[2];
    angles[0] = angles[0] + ang[0];
    angles[1] = angles[1] + ang[1];
    angles[2] = angles[2] + ang[2];
    do {
        x = dir[0];
        y = dir[1];
        z = dir[2];
    } while (0);
    ROT_Z(ang[2], x, y);
    ROT_X(ang[0], y, z);
    ROT_Y(ang[1], x, z);
    dir[0] = x;
    dir[1] = y;
    dir[2] = z;
}

/* Editable list while editing, else the array. Spawn via type[3]; relative records converted. */
/* 0x0011ec78 */
void flow_enemy_event_call(int *data, int idx)
{
    GameFlowEnemyGeRec *rec;
    GameFlowEnemyGeRec ev;
    int *ent;
    int mask;
    GameFlowEnemyGeRec *arg;

    if (data[0] & 1)
        rec = flow_enemy_event_list_nth(data, idx);
    else
        rec = (GameFlowEnemyGeRec *)data[11] + idx;
    ent = flow_enemy_tbl_get(data, rec->type);
    if (ent == 0 || ent[0] == 0) {
        mask = debug_color_set(RGBA(0x80, 0, 0, 0x80));
        DEBUG_PRINTF(str_flow_enemy_call_nop, idx);
        debug_color_set(mask);
        return;
    }
    if (ent[3] == 0)
        return;
    if (rec->flags & 1) {
        ev = *rec;
        flow_enemy_apply_camera(&ev.param[0], &ev.param[6], &ev.param[3]);
        arg = &ev;
    } else {
        arg = rec;
    }
    ((void (*)(int, GameFlowEnemyGeRec *))ent[3])(data[8], arg);
}

/* Also drops the undo copy. */
/* 0x0011edf0 */
void flow_enemy_edit_colors_reset(void)
{
    int i;
    unsigned int col = COL_WHITE;

    for (i = 11; i >= 0; i--)
        flow_editor_param_colors[i] = col;
    flow_editor_undo_saved = 0;
}

/* 4-pixel margin right and bottom. */
/* 0x0011ee30 */
void flow_edit_window_clamp(void *win, int width, int height)
{
    int x, y;
    float limit;

    gw_window_set_size(win, width, height);
    gw_window_get_pos(win, &x, &y);
    if (x < 0)
        x = 0;
    limit = screen_layout.right - (float)width - 4.0f;
    if (limit < (float)x)
        x = (int)limit;
    if (y < 0)
        y = 0;
    limit = screen_layout.bottom - (float)height - 4.0f;
    if (limit < (float)y)
        y = (int)limit;
    gw_window_set_pos(win, x, y);
}

/* Unless debug_disp_level hides it. */
/* 0x0011ef30 */
void flow_enemy_edit_key_help(void)
{
    if (debug_disp_level == 0) {
        debug_set_origin(0, -9);
        debug_puts(str_flow_enemy_help_vec_to_ang);
        debug_newline();
        debug_puts(str_flow_enemy_help_ang_to_vec);
        debug_newline();
        debug_puts(str_flow_enemy_help_cam_to_target);
        debug_newline();
        debug_puts(str_flow_enemy_help_target_to_cam);
        debug_newline();
        debug_puts(str_flow_enemy_help_make);
        debug_newline();
        debug_puts(str_flow_enemy_help_disp);
        debug_newline();
        debug_puts(str_flow_enemy_help_undo);
        debug_newline();
        debug_puts(str_flow_enemy_help_copy);
        debug_newline();
        debug_puts(str_flow_enemy_help_paste);
        debug_newline();
    }
}

/*
 * Record index in win[27]. Preview via the type's display callback while flow_enemy_disp_on. Line
 * colours from flow_editor_param_colors, so the cursor line stands out.
 */
/* 0x0011f018 */
void flow_enemy_edit_disp(int *win)
{
    FlowEnemyEv ev;
    int cw, ch;
    char *obj;
    FlowEnemyEv *rec;
    FlowEnemyType *type;
    int idx, res, y, y_top, i;
    float len, len_sec;

    obj = game_flow_get_data();
    idx = win[27];
    gw_get_cell_size(&cw, &ch);
    flow_edit_window_clamp(win, cw * 31, ch * 13);
    rec = flow_enemy_event_list_nth(obj, idx);
    if (rec == 0)
        return;
    type = flow_enemy_tbl_get(obj, rec->type);
    flow_editor_draw_gizmo(obj, rec);
    gw_window_set_draw_color(win, ARGB(0x80, 0x80, 0x80, 0x80));
    gw_win_printf(win, 4, 0, "%3d", idx);
    res = flow_enemy_type_check(obj, idx, rec, type);
    gw_window_set_draw_color(win, ARGB(0x80, 0, 0, 0x80));
    switch (res) {
    case 0:
        break;
    case 3:
        gw_win_printf(win, cw * 4 + 4, 0, "%3d:Nop Enemy", rec->type);
        return;
    }
    if (flow_enemy_disp_on && type->disp) {
        if (rec->flags & 1) {
            ev = *rec;
            flow_enemy_apply_camera(ev.pos, ev.dir, ev.ang);
            type->disp(*(int *)(obj + 32), &ev);
        } else
            type->disp(*(int *)(obj + 32), rec);
    }
    gw_window_set_draw_color(win, flow_editor_param_colors[0]);
    gw_win_printf(win, cw * 4 + 4, 0, "%3d:%s", rec->type, type->name ? type->name : "");
    y_top = ch;
    gw_window_set_draw_color(win, flow_editor_param_colors[1]);
    gw_win_printf(win, 4, y_top, "TYPE %s", (rec->flags & 1) ? "RELATIVE" : "WORLD");
    y = y_top + ch;
    gw_window_set_draw_color(win, flow_editor_param_colors[2]);
    gw_win_printf(win, 4, y, "Formation % 4d", rec->formation);
    y += ch;
    gw_window_set_draw_color(win, flow_editor_param_colors[3]);
    gw_win_printf(win, 4, y, "pos % 7d % 7d % 7d", (int)rec->pos[0], (int)rec->pos[1], (int)rec->pos[2]);
    y += ch;
    gw_window_set_draw_color(win, flow_editor_param_colors[4]);
    gw_win_printf(win, 4, y, "ang % 7d % 7d % 7d", (int)(rec->ang[0] * (360.0f / ANG_TURN)),
                  (int)(rec->ang[1] * (360.0f / ANG_TURN)), (int)(rec->ang[2] * (360.0f / ANG_TURN)));
    y += ch;
    gw_window_set_draw_color(win, flow_editor_param_colors[5]);
    len = vec3_length(rec->dir);
    len_sec = len * FPS;
    gw_win_printf(win, 4, y, "vec % 7.3f % 7.3f % 7.3f\nvec %8.3f/%8.3fms", rec->dir[0], rec->dir[1],
                  rec->dir[2], len, len_sec);
    y += ch * 2;
    for (i = 0; i < 6; i++) {
        if (type->param_name[i]) {
            gw_window_set_draw_color(win, flow_editor_param_colors[6 + i]);
            gw_win_printf(win, 4, y, "%-20s % 8.3f", type->param_name[i], rec->param[i]);
            y += ch;
        }
    }
    gw_window_set_draw_color(win, ARGB(0x80, 0x80, 0x80, 0x80));
}

static inline void ge_vscale(float *vec, float scale)
{
    vec[0] *= scale;
    vec[1] *= scale;
    vec[2] *= scale;
}

static inline void ge_vadd(float *dst, float *va, float *vb)
{
    dst[0] = va[0] + vb[0];
    dst[1] = va[1] + vb[1];
    dst[2] = va[2] + vb[2];
}

static inline void ge_vsub(float *dst, float *to, float *from)
{
    dst[0] = to[0] - from[0];
    dst[1] = to[1] - from[1];
    dst[2] = to[2] - from[2];
}

/* 100 units ahead; relative records stored relative to the player. */
/* 0x0011f528 */
void flow_enemy_place_at_camera(FlowEnemyEvent *ev)
{
    Vec3 eye;
    Vec3 dir;
    Vec3 z;

    camera_get_params(&eye, &dir, 0);
    ge_vscale(&dir.x, 100.0f);
    ge_vadd(&ev->pos.x, &dir.x, &eye.x);
    if (ev->flags & 1) {
        player_get_pos(&z);
        ge_vsub(&ev->pos.x, &ev->pos.x, &z.x);
    }
}

static inline void vscale(float *dst, float *src, float scale)
{
    dst[0] = src[0] * scale;
    dst[1] = src[1] * scale;
    dst[2] = src[2] * scale;
}

static inline void vsub(float *dst, float *to, float *from)
{
    dst[0] = to[0] - from[0];
    dst[1] = to[1] - from[1];
    dst[2] = to[2] - from[2];
}

static inline void vadd(float *dst, float *va, float *vb)
{
    dst[0] = va[0] + vb[0];
    dst[1] = va[1] + vb[1];
    dst[2] = va[2] + vb[2];
}

/* Inverse of flow_enemy_place_at_camera. */
/* 0x0011f600 */
void flow_enemy_move_camera(GameFlowEnemyFlowEnemyEvent *ev)
{
    float eye[3];
    float w[3];
    float z[3];

    camera_get_params(eye, w, 0);
    vscale(w, w, 100.0f);
    vsub(eye, ev->pos, w);
    if (ev->flags & 1) {
        player_get_pos(z);
        vadd(eye, eye, z);
    }
    camera_move_eye(eye);
}

/*
 * Right/Left, wrapping over all types. Editor keys are USB usage codes: 79/80 Right/Left, 82/81
 * Up/Down, 75/78 Page Up/Down, 74 Home; modifier mask 0x22 Shift, 0x11 Ctrl.
 */
/* 0x0011f6d0 */
void flow_editor_pad_type(int *data, unsigned char *type)
{
    int n = data[4] + 16;
    int val;
    int last;
    int clamped;

    val = *type;
    switch (kbd_state[6]) {
    case 'O':
        val = val + 1;
        if (!(val < n))
            val = 0;
        break;
    case 'P':
        val = val - 1;
        if (!(-1 < val))
            val = data[4] + 15;
        break;
    }
    last = n - 1;
    clamped = val > -1 ? val : 0;
    last = clamped < last ? clamped : last;
    *type = last;
}

/* RELATIVE <-> WORLD. */
/* 0x0011f748 */
void flow_editor_pad_flag(int data, unsigned char *p)
{
    if (kbd_state[5] == 'O')
        p[1]++;
    if (kbd_state[5] == 'P')
        p[1]--;
    p[1] &= 1;
}

/* Formation, by 10 with Shift, -1 (none)..32767. */
/* 0x0011f798 */
void flow_editor_pad_short(int data, short *p)
{
    int val;
    int clamped;
    int step;

    val = p[1];
    step = 1;
    if (kbd_state[7] & 0x22)
        step = 10;
    if (kbd_state[6] == 'O')
        val = val + step;
    if (kbd_state[6] == 'P')
        val = val - step;
    clamped = val > -2 ? val : -1;
    clamped = clamped < 32768 ? clamped : 32767;
    p[1] = clamped;
}

/* Right/Left x, Down/Up z, Page Up/Down y; 1 unit per frame, 5 with Shift. */
/* 0x0011f7f8 */
void flow_editor_pad_pos(int data, float *p)
{
    float step;

    step = 1.0f;
    if (kbd_mods & 0x22)
        step = 5.0f;
    if (pad_keyboard_key_down(79))
        p[1] = p[1] + step;
    if (pad_keyboard_key_down(80))
        p[1] = p[1] - step;
    if (pad_keyboard_key_down(82))
        p[3] = p[3] - step;
    if (pad_keyboard_key_down(81))
        p[3] = p[3] + step;
    if (pad_keyboard_key_down(75))
        p[2] = p[2] + step;
    if (pad_keyboard_key_down(78))
        p[2] = p[2] - step;
}

/* Right/Left y, Up/Down x, PgUp/PgDn z; 1 degree (182 units) per frame, 10 Shift. Home resets. */
/* 0x0011f8e8 */
void flow_editor_pad_angle(int data, FlowEnemyEvent *ev)
{
    int step;
    int *ang;

    step = 182;
    if (kbd_state[7] & 0x22)
        step = 1820;
    if (pad_keyboard_key_down(79))
        ev->ang[1] = ev->ang[1] - step;
    if (pad_keyboard_key_down(80))
        ev->ang[1] = ev->ang[1] + step;
    if (pad_keyboard_key_down(82))
        ev->ang[0] = ev->ang[0] + step;
    if (pad_keyboard_key_down(81))
        ev->ang[0] = ev->ang[0] - step;
    if (pad_keyboard_key_down(75))
        ev->ang[2] = ev->ang[2] + step;
    if (pad_keyboard_key_down(78))
        ev->ang[2] = ev->ang[2] - step;
    if (kbd_state[5] == 'J') {
        ang = ev->ang;
        ang[0] = 0;
        ang[1] = 0;
        ang[2] = 0;
    }
}

/*
 * As heading, pitch, length: 1 degree per frame (10 with Shift); length 0.1 (1.0 with Shift); Home
 * resets to length 1 along -z. Rebuilt as -z rotated by the angles.
 */
/* 0x0011f9f8 */
void flow_editor_pad_vector(int data, float *p)
{
    float dir[3];
    int ang[3];
    float sc[4];
    float len;
    float step;
    int istep;

    istep = 182;
    len = vec3_length(&p[7]);
    step = 0.1f;
    do {
        ang[1] = (int)RAD_TO_ANG(atan2f(-p[7], -p[9]));
        ang[0] = (int)RAD_TO_ANG(atan2f(p[8], sqrtf(p[7] * p[7] + p[9] * p[9])));
    } while (0);
    if (kbd_state[7] & 0x22) {
        step = 1.0f;
        istep = 1820;
    }
    if (pad_keyboard_key_down(79))
        ang[1] = ang[1] - istep;
    if (pad_keyboard_key_down(80))
        ang[1] = ang[1] + istep;
    if (pad_keyboard_key_down(82))
        ang[0] = ang[0] + istep;
    if (pad_keyboard_key_down(81))
        ang[0] = ang[0] - istep;
    if (pad_keyboard_key_down(75))
        len = len + step;
    if (pad_keyboard_key_down(78))
        len = len - step;
    if (kbd_state[5] == 'J') {
        ang[0] = 0;
        ang[1] = 0;
        ang[2] = 0;
        len = 1.0f;
    }
    *(int *)&dir[0] = 0;
    dir[2] = -1.0f;
    *(int *)&dir[1] = 0;
    do {
        float u0 = dir[0], u1 = dir[2];

        ulSinCos(ang[0], &sc[0], &sc[1]);
        dir[1] = sc[1] * u0 - sc[0] * u1;
        dir[2] = sc[0] * u0 + sc[1] * u1;
    } while (0);
    do {
        float u0 = dir[0], u1 = dir[2];

        ulSinCos(ang[1], &sc[2], &sc[3]);
        dir[0] = u0 * sc[3] + u1 * sc[2];
        dir[2] = -u0 * sc[2] + u1 * sc[3];
    } while (0);
    p[7] = dir[0] * len;
    p[9] = dir[2] * len;
    p[8] = dir[1] * len;
}

/* Right/Left 0.001 (0.01 Shift), Up/Down 1 (10 Shift); Home sets 0. */
/* 0x0011fc78 */
void flow_editor_pad_adjust_float(void *data, float *val)
{
    KbdState *kbd = &kbd_state;
    float step = 0.001f;

    if (kbd->mod & 0x22)
        step = 0.01f;
    if (kbd->key == 79)
        *val += step;
    if (kbd->key == 80)
        *val -= step;
    step = 1.0f;
    if (kbd->mod & 0x22)
        step = 10.0f;
    if (kbd->key == 82)
        *val += step;
    if (kbd->key == 81)
        *val -= step;
    if (kbd->key5 == 74)
        *val = 0;
}

/* Wraps at 12, skips unnamed parameter lines; the old line turns grey. */
/* 0x0011fd40 */
void flow_editor_param_cursor_set(unsigned char *ev, unsigned int idx)
{
    int *base;
    unsigned int cur;

    flow_editor_param_colors[flow_editor_param_cursor] = COL_WHITE;
    flow_editor_param_cursor = idx;
    if (flow_editor_param_cursor >= 12)
        flow_editor_param_cursor = 0;
    if (flow_editor_param_cursor >= 6) {
        base = flow_enemy_tbl_get(game_flow_get_data(), *ev);
        if (base != 0) {
            int *names = base + 5;
            int k;

        loop:
            cur = flow_editor_param_cursor;
            k = cur - 6;
            if (*(int *)((char *)names + (k << 2)) == 0) {
                flow_editor_param_cursor = cur + 1;
                if (cur + 1 < 12)
                    goto loop;
                do { flow_editor_param_cursor = 0; } while (0);
            }
        }
    }
}

/*
 * Tab, 1..7: cursor. Shift-C place at camera; Ctrl-U undo, Ctrl-V paste, Ctrl-C copy. M make enemy,
 * V vector -> angles, A angles -> vector, C camera to record, P preview. Enter/Esc/Backspace -> 1.
 */
/* 0x0011fe10 */
int flow_editor_param_handle_input(void *data, FlowEnemyEventHeadView *blk, void *idx)
{
    int key = (*(KbdState *)&kbd_state).key5; /* steer(cast) */
    int ret = 0;
    FlowEnemyEventHeadView *ev = blk;
    FlowVec3 dir;
    float full_ang, two_pi, rad, len;

    switch (key) {
    case 0x2b:
        if (pad_keyboard_modifiers_match(0x22) != 0)
            flow_editor_param_cursor_set(blk, flow_editor_param_cursor - 1);
        else
            flow_editor_param_cursor_set(blk, flow_editor_param_cursor + 1);
        break;
    case 0x1e:
        flow_editor_param_cursor_set(blk, 0);
        break;
    case 0x1f:
        flow_editor_param_cursor_set(blk, 3);
        break;
    case 0x20:
        flow_editor_param_cursor_set(blk, 4);
        break;
    case 0x21:
        flow_editor_param_cursor_set(blk, 5);
        break;
    case 0x22:
        flow_editor_param_cursor_set(blk, 6);
        break;
    case 0x23:
        flow_editor_param_cursor_set(blk, 7);
        break;
    case 0x24:
        flow_editor_param_cursor_set(blk, 8);
        break;
    }
    if (((*(KbdState *)&kbd_state).mod & 0x22) != 0) { /* steer(cast) */
        if (key == 6)
            flow_enemy_place_at_camera(blk);
    } else if (((*(KbdState *)&kbd_state).mod & 0x11) != 0) { /* steer(cast) */
        switch (key) {
        case 0x18:
            *blk = flow_editor_undo_buf;
            break;
        case 0x19:
            *blk = flow_editor_copy_buf;
            break;
        case 6:
            flow_editor_copy_buf = *blk;
            break;
        }
    } else {
        switch (key) {
        case 0x10:
            flow_enemy_event_call(data, idx);
            break;
        case 0x19:
            full_ang = ANG_TURN;
            two_pi = TWO_PI;
            rad = atan2f(-ev->v[0], -ev->v[2]);
            ev->ang2 = (int)(rad * full_ang / two_pi);
            rad = atan2f(ev->v[1], sqrtf(ev->v[0] * ev->v[0] + ev->v[2] * ev->v[2]));
            ev->ang1 = (int)(rad * full_ang / two_pi);
            break;
        case 6:
            flow_enemy_move_camera(blk);
            break;
        case 0x13:
            flow_enemy_disp_on = flow_enemy_disp_on ^ 1;
            break;
        case 0x28:
        case 0x29:
        case 0x2a:
            ret = 1;
            break;
        }
        if ((*(KbdState *)&kbd_state).key5 == 4) { /* steer(cast) */
            dir = flow_editor_base_dir;
            len = vec3_length(ev->v);
            do {
                float s, c, u0 = dir.y, u1 = dir.z;
                ulSinCos(ev->ang1, &s, &c);
                dir.y = u0 * c - u1 * s;
                dir.z = u0 * s + u1 * c;
            } while (0);
            do {
                float s, c, u0 = dir.x, u1 = dir.z;
                ulSinCos(ev->ang2, &s, &c);
                dir.x = u0 * c + u1 * s;
                dir.z = -u0 * s + u1 * c;
            } while (0);
            ev->v[0] = dir.x * len;
            ev->v[2] = dir.z * len;
            ev->v[1] = dir.y * len;
        }
    }
    return ret;
}

/* Box, forward and up axes by its angles, direction vector. */
/* 0x00120380 */
void flow_editor_draw_gizmo(void *data, FlowEnemyEventTypeView *src)
{
    float pos[3];
    float tip[3];
    FlowEnemyEventTypeView ev;
    float sc[6][2];
    float *pv;

    ev = *src;
    if ((ev.flags & 1) != 0)
        flow_enemy_apply_camera(&ev.pos[0], &ev.dir[0], &ev.angB);
    pos[0] = ev.pos[0];
    pos[1] = ev.pos[1];
    pos[2] = ev.pos[2];
    do { ulMatrixPush(); } while (0);
    vu0_matrix_translate(ulMatrixCur, pos[0], pos[1], pos[2]);
    debug_draw_box(RGBA(0x60, 0x60, 0x60, 0x80), 30.0f);
    ulMatrixPop();
    pv = tip;
    tip[0] = 0.0f;
    tip[1] = 0.0f;
    pv[2] = -30.0f;
    ROTZ(ev.angA, 0);
    ROTX(ev.angB, 1);
    ROTY(ev.angC, 2);
    tip[0] = tip[0] + pos[0];
    tip[1] = tip[1] + pos[1];
    tip[2] = tip[2] + pos[2];
    lockon_draw_line_3d(pos, tip, RGBA(0, 0, 0x80, 0x80), RGBA(0, 0, 0x80, 0x80));
    tip[0] = 0.0f;
    pv[1] = 15.0f;
    tip[2] = 0.0f;
    ROTZ(ev.angA, 3);
    ROTX(ev.angB, 4);
    ROTY(ev.angC, 5);
    tip[0] = tip[0] + pos[0];
    tip[1] = tip[1] + pos[1];
    tip[2] = tip[2] + pos[2];
    lockon_draw_line_3d(pos, tip, RGBA(0x80, 0, 0, 0x80), RGBA(0x80, 0, 0, 0x80));
    tip[0] = ev.dir[0] * 15.0f + pos[0];
    tip[1] = ev.dir[1] * 15.0f + pos[1];
    tip[2] = ev.dir[2] * 15.0f + pos[2];
    lockon_draw_line_3d(pos, tip, RGBA(0x60, 0x60, 0x60, 0x80), RGBA(0, 0x80, 0, 0x80));
}

/* Undo copy from the first frame. Returns 0 when editing ends. */
/* 0x00120720 */
int flow_editor_param_exec(void *data, int idx)
{
    FlowEnemyEventHeadView *src = flow_enemy_event_list_nth(data, idx);
    int quit;

    flow_editor_param_colors[flow_editor_param_cursor] = RGBA(0, 0x80, 0, 0x80);
    quit = flow_editor_param_handle_input(data, src, idx);
    if (flow_editor_undo_saved == 0) {
        flow_editor_undo_buf = *src;
        flow_editor_undo_saved = 1;
    }
    switch (flow_editor_param_cursor) {
    case 0:
        flow_editor_pad_type(data, src);
        break;
    case 1:
        flow_editor_pad_flag(data, src);
        break;
    case 2:
        flow_editor_pad_short(data, src);
        break;
    case 3:
        flow_editor_pad_pos(data, src);
        break;
    case 4:
        flow_editor_pad_angle(data, src);
        break;
    case 5:
        flow_editor_pad_vector(data, src);
        break;
    default:
        flow_editor_pad_adjust_float(data, &((float *)src)[flow_editor_param_cursor + 4]);
        break;
    }
    flow_enemy_edit_key_help();
    if (quit != 0) {
        flow_enemy_disp_on = 1;
        flow_enemy_edit_colors_reset();
        return 0;
    }
    return 1;
}
