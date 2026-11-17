/*
 * Game flow function events: timed calls into the stage's function table (up to three float
 * parameters), their FlowEdit page, and built-in controls (hold/skip on enemy count, BPM change).
 */

#include "types_game.h"
#include "color.h"
#include "debug.h"
#include "gw_draw.h"
#include "gw_system.h"
#include "game_flow_func.h"
#include "enemy_status.h"
#include "game.h"
#include "game_flow_editor.h"
#include "heap.h"
#include "pad.h"
#include "sound_beat.h"
#include "task.h"

typedef struct GameFlowFuncFlowFuncEvNode GameFlowFuncFlowFuncEvNode;
struct GameFlowFuncFlowFuncEvNode { char pad_00[16]; GameFlowFuncFlowFuncEvNode *next; };
typedef struct { int name; void (*fn)(int *); } GameFlowFuncFlowFuncEnt;
typedef struct { char pad_00[24]; int count; } FlowFuncTblCallFn4AllFlowFuncObj;
typedef struct { int v; } FlowFuncArg;
typedef struct { int name; int fn4; void (*fn)(void); } FlowFuncTblCallFn8AllFlowFuncEnt;
typedef struct { int id; float param[3]; } FlowFuncEvView;

extern int flow_func_edit_colors[4];
extern KbdState kbd_state;
extern unsigned int flow_func_edit_cursor;
extern struct TaskDesc flow_ctrl_stop_desc;
extern struct TaskDesc flow_ctrl_skip_desc;
extern void flow_func_edit_cursor_step(FlowFuncTblCallFn4AllFlowFuncObj *data, int *idx);
extern void flow_func_events_free_array(FlowFuncObj *data);
extern void flow_func_events_free_list(FlowFuncObj *data);
extern int flow_func_tbl_get(FlowFuncObj *data, int idx);

/* 0x00120918 */
void flow_func_events_free_array(FlowFuncObj *data)
{
    data->arr_count = 0;
    heap_free(data->arr, 0);
    data->arr = 0;
}

/* 0x00120950 */
void flow_func_events_free_list(FlowFuncObj *data)
{
    GameFlowFuncFlowFuncEvNode *node = data->head;
    GameFlowFuncFlowFuncEvNode *next;

    if (node != 0) {
        do {
            next = node->next;
            heap_free(node, 0);
            node = next;
        } while (node != 0);
    }
    data->head = 0;
    data->list_count = 0;
}

/* Always looks up entry 0, so only the first entry's callback runs (count times). */
/* 0x001209a8 */
void flow_func_tbl_call_fn4_all(FlowFuncTblCallFn4AllFlowFuncObj *data)
{
    int n = data->count;
    GameFlowFuncFlowFuncEnt *ent;
    FlowFuncArg arg;

    if (n > 0) {
        do {
            ent = flow_func_tbl_get(data, 0);
            if (ent != 0 && ent->fn != 0) {
                arg.v = 0;
                ent->fn(&arg.v);
            }
            n--;
        } while (n != 0);
    }
}

/* Also always looks up entry 0. */
/* 0x00120a20 */
void flow_func_tbl_call_fn8_all(FlowFuncTblCallFn4AllFlowFuncObj *data)
{
    int n = data->count;
    FlowFuncTblCallFn8AllFlowFuncEnt *ent;

    if (n > 0) {
        do {
            ent = flow_func_tbl_get(data, 0);
            if (ent != 0 && ent->fn != 0)
                ent->fn();
            n--;
        } while (n != 0);
    }
}

/* Returns the end of the source. */
/* 0x00120a88 */
FlowFuncEv *flow_func_events_set_array(FlowFuncObj *data, FlowFuncEv *src, int n)
{
    FlowFuncEv *dst;

    flow_func_events_free_array(data);
    if (n > 0) {
        data->arr_count = n;
        data->arr = heap_block_tag(game_malloc(n << 4), "GameFlow FlowFuncData", -1);
        dst = data->arr;
        do {
            *dst++ = *src++;
            n--;
        } while (n != 0);
    }
    return src;
}

/* Editable list while editing, else the array. */
/* 0x00120b28 */
void flow_func_event_call(FlowFuncObj *data, int idx)
{
    FlowFuncEv *ev;
    FlowFuncEnt *ent;

    if (data->flags & 1)
        ev = flow_func_event_list_nth(data, idx);
    else
        ev = data->arr + idx;
    ent = flow_func_tbl_get(data, ev->id);
    if (ent == 0) {
        DEBUG_PRINTF("FlowEvent [%d] Call nop Func\n", idx);
        return;
    }
    if (ent->fnc != 0)
        ent->fnc(data->arg, ev);
}

/* Returns its index. */
/* 0x00120bd0 */
int flow_func_event_list_append(FlowFuncObj *data)
{
    int idx = 0;
    FlowFuncEvNode *p = data->head;
    FlowFuncEvNode *node;

    node = heap_block_tag(game_calloc(20), "game_flow_func.c", 205);
    node->next = 0;
    if (p == 0) {
        data->head = node;
    } else {
        idx = 1;
        while (p->next != 0) {
            p = p->next;
            idx++;
        }
        p->next = node;
    }
    data->list_count++;
    return idx;
}

/* Flow tags of kind 2 at or after idx are moved down by one first. */
/* 0x00120c88 */
void flow_func_event_list_remove(FlowFuncObj *data, int idx)
{
    FlowTag *tag;
    FlowFuncEvNode **pp;
    FlowFuncEvNode *cur;

    for (tag = data->tags; tag != 0; tag = tag->next) {
        if (tag->kind == 2 && tag->idx >= idx)
            tag->idx--;
    }
    pp = &data->head;
    while (idx--) {
        FlowFuncEvNode *node = *pp;
        if (node == 0)
            return;
        pp = &node->next;
    }
    cur = *pp;
    if (cur != 0) {
        *pp = cur->next;
        heap_free(cur, 0);
        data->list_count--;
    }
}

/* 0x00120d48 */
GameFlowFuncFlowFuncEvNode *flow_func_event_list_nth(char *data, int n)
{
    GameFlowFuncFlowFuncEvNode **pp = (GameFlowFuncFlowFuncEvNode **)(data + 76);

    if (*pp != 0 && n != 0) {
        do {
            GameFlowFuncFlowFuncEvNode *cur = *pp;

            n--;
            pp = &cur->next;
            if (cur->next == 0)
                break;
        } while (n != 0);
    }
    if (n != 0)
        return 0;
    return *pp;
}

/* 0 when the stage has none. */
/* 0x00120d90 */
int flow_func_tbl_get(FlowFuncObj *data, int idx)
{
    if (data->table == 0)
        return 0;
    return data->table[idx];
}

/* 0x00120db0 */
void flow_func_edit_colors_reset(void)
{
    int i;

    for (i = 0; i < 4; i++)
        flow_func_edit_colors[i] = COL_WHITE;
}

/* Returns the end of the copy. */
/* 0x00120df0 */
FlowFuncEv *flow_func_events_copy_array(FlowFuncObj *data, FlowFuncEv *dst, int n)
{
    FlowFuncEv *src;

    n = n < data->arr_count ? n : data->arr_count;
    src = data->arr;
    if (n > 0) {
        do {
            *dst++ = *src++;
            n--;
        } while (n != 0);
    }
    return dst;
}

/* 0x00120e48 */
void flow_func_events_array_to_list(FlowFuncObj *data)
{
    FlowFuncEvNode **pp = &data->head;
    int n = data->arr_count;
    FlowFuncEv *src = data->arr;
    FlowFuncEvNode *node;

    flow_func_events_free_list(data);
    data->list_count = n;
    if (n == 0) {
        data->head = 0;
        return;
    }
    do {
        node = heap_block_tag(game_malloc(20), "game_flow_func.c", 394);
        node->ev = *src;
        node->next = 0;
        src++;
        if (*pp == 0)
            *pp = node;
        else
            (*pp)->next = node;
        n--;
        pp = &node->next;
    } while (n != 0);
}

/* 0x00120f08 */
void flow_func_events_list_to_array(FlowFuncObj *data)
{
    int n;
    FlowFuncEvNode *p;
    FlowFuncEv *dst;

    flow_func_events_free_array(data);
    if (data->list_count > 0) {
        data->arr_count = data->list_count;
        n = data->list_count;
        p = data->head;
        dst = heap_block_tag(game_malloc(n << 4), "game_flow_func.c", 429);
        data->arr = dst;
        do {
            *dst = p->ev;
            dst++;
            n--;
            p = p->next;
        } while (n != 0);
    }
}

/* Right/Left (keys 79/80), wrapping. */
/* 0x00120fa8 */
void flow_func_edit_cursor_step(FlowFuncTblCallFn4AllFlowFuncObj *data, int *idx)
{
    int n = data->count;
    int i = *idx;
    int clamped;

    switch (kbd_key2) {
    case 79:
        i++;
        i = i < n ? i : 0;
        break;
    case 80:
        i--;
        i = i > -1 ? i : n - 1;
        break;
    }
    clamped = i > -1 ? i : 0;
    *idx = clamped < n - 1 ? clamped : n - 1;
}

/* As flow_editor_pad_adjust_float. */
/* 0x00121018 */
void flow_func_edit_float_pad(void *data, float *val)
{
    unsigned char key = kbd_state.key;
    float step = 0.001f;

    if (kbd_state.mod & 0x22)
        step = 0.01f;
    switch (key) {
    case 79:
        *val += step;
        break;
    case 80:
        *val -= step;
        break;
    }
    step = 1.0f;
    if (kbd_state.mod & 0x22)
        step = 10.0f;
    switch (key) {
    case 82:
        *val += step;
        break;
    case 81:
        *val -= step;
        break;
    }
    if (kbd_state.key5 == 74)
        *val = 0;
}

/* Wraps at 4, skips unnamed parameter lines; the old line turns grey. */
/* 0x001210e8 */
void flow_func_edit_param_cursor_set(int *ev, unsigned int idx)
{
    int *ent;
    unsigned int cur;

    flow_func_edit_colors[flow_func_edit_cursor] = COL_WHITE;
    flow_func_edit_cursor = idx;
    if (flow_func_edit_cursor >= 4)
        flow_func_edit_cursor = 0;
    if (flow_func_edit_cursor != 0) {
        ent = flow_func_tbl_get(game_flow_get_data(), *ev);
        if (ent != 0) {
            int *names = ent + 4;
            int k;

            do { } while (0);
        loop:
            cur = flow_func_edit_cursor;
            k = cur - 1;
            if (*(int *)((char *)names + (k << 2)) == 0) {
                flow_func_edit_cursor = cur + 1;
                if (cur + 1 < 4)
                    goto loop;
                do { flow_func_edit_cursor = 0; } while (0);
            }
        }
    }
}

/* Tab down, Shift-Tab up. 1 on Enter/Esc/Backspace without Shift or Ctrl. */
/* 0x001211b8 */
int flow_func_edit_param_input(void *data, int *ev)
{
    int key = kbd_state.key5;
    int ret = 0;
    int mod;

    if (key == 43) {
        if (pad_keyboard_modifiers_match(34))
            flow_func_edit_param_cursor_set(ev, flow_func_edit_cursor - 1);
        else
            flow_func_edit_param_cursor_set(ev, flow_func_edit_cursor + 1);
    }
    mod = kbd_state.mod;
    if ((mod & 0x22) == 0) {
        if ((mod & 0x11) == 0) {
            if (key < 43)
                ret = key >= 40;
        }
    }
    return ret;
}

/* Line 0 edits the function, others the parameters. Returns 0 when editing ends. */
/* 0x00121278 */
int flow_func_edit_update(void *data, int idx)
{
    int *ev;
    int quit;

    ev = flow_func_event_list_nth(data, idx);
    flow_func_edit_colors[flow_func_edit_cursor] = RGBA(0, 0x80, 0, 0x80);
    quit = flow_func_edit_param_input(data, ev);
    if (flow_func_edit_cursor == 0)
        flow_func_edit_cursor_step(data, ev);
    else
        flow_func_edit_float_pad(data, ev + flow_func_edit_cursor);
    if (quit != 0) {
        flow_func_edit_colors_reset();
        return 0;
    }
    return 1;
}

/* Line colours from flow_func_edit_colors. */
/* 0x00121330 */
void flow_func_edit_disp(FlowFuncObj *win)
{
    int cw, line;
    int y;
    int i;
    int idx;
    FlowFuncObj *data;
    FlowFuncEvView *ev;
    FlowFuncEnt *ent;

    data = game_flow_get_data();
    idx = *(int *)((char *)win + 108);
    gw_get_cell_size(&cw, &line);
    ev = flow_func_event_list_nth(data, idx);
    if (ev != 0) {
        ent = flow_func_tbl_get(data, ev->id);
        gw_win_printf(win, 4, 0, "%3d", idx);
        if (ent != 0) {
            gw_window_set_draw_color(win, flow_func_edit_colors[0]);
            gw_win_printf(win, cw * 4 + 4, 0, "%3d:%s", ev->id, ent->name != 0 ? ent->name : "");
            y = line;
            for (i = 0; i < 3; i++) {
                if (ent->pname[i] != 0) {
                    gw_window_set_draw_color(win, flow_func_edit_colors[i + 1]);
                    gw_win_printf(win, 4, y, "%-20s % 8.3f", ent->pname[i], (double)ev->param[i]);
                }
                y += line;
            }
        }
        gw_window_set_draw_color(win, ARGB(0x80, 0x80, 0x80, 0x80));
    }
}

/* Holds the flow count (GameFlowCountStart). */
/* 0x001214c8 */
void flow_ctrl_stop_init(FlowCtrl *w, FlowCtrl *src)
{
    int x = src->kind;
    int y = src->count;
    w->kind = x;
    w->count = y;
    GameFlowCountStart();
}

/* 0x001214e0 */
void flow_ctrl_release(void)
{
    GameFlowCountStop();
}

/* Ends once the enemy count drops to the requested count. */
/* 0x001214e8 */
void flow_ctrl_stop_exec(FlowCtrl *w)
{
    if (w->count >= enemy_status_get_cur()) {
        do {
            if (w != 0)
                TASK_OF(w)->flags |= TASK_F_END;
        } while (0);
        DEBUG_PRINTF("CountStop End\n");
    }
}

/* 0x00121548 */
void flow_ctrl_skip_init(FlowCtrl *w, FlowCtrl *src)
{
    int x, y;
    float z;

    do {
        z = src->time;
    } while (0);
    x = src->kind;
    y = src->count;
    do {
        w->time = z;
    } while (0);
    w->kind = x;
    w->count = y;
}

/* Count reached before the target time -> jumps the flow there. Ends on either. */
/* 0x00121568 */
void flow_ctrl_skip_exec(FlowCtrl *w)
{
    if (!(w->time <= game_flow_time_get())) {
        if (w->count < enemy_status_get_cur())
            return;
        if (game_flow_get_data() != 0) {
            game_flow_time_set(w->time);
            game_flow_seek_to_time();
            DEBUG_PRINTF("CountSkip %8.3f\n", (double)w->time);
        }
    }
    do {
        if (w != 0)
            TASK_OF(w)->flags |= TASK_F_END;
    } while (0);
}

/* Kind 0: FlowCtrlStop, 1: FlowCtrlSkip. */
/* 0x00121608 */
void *flow_ctrl_request(void *owner, FlowCtrl *ctrl)
{
    void *parent = game_flow_get_current();

    switch (ctrl->kind) {
    case 0:
        DEBUG_PRINTF("FlowCount Stop %d\n", ctrl->count);
        return task_create_arg(parent, &flow_ctrl_stop_desc, 12, ctrl);
    case 1:
        DEBUG_PRINTF("FlowCount Skip %d % 8.3f\n", ctrl->count, (double)ctrl->time);
        do {
            return task_create_arg(parent, &flow_ctrl_skip_desc, 12, ctrl);
        } while (0);
    }
    return 0;
}

/* Holds the flow until the enemy count drops to p[1]. */
/* 0x001216c8 */
void flow_ctrl_request_stop(void *owner, float *p)
{
    FlowCtrl ctrl;

    ctrl.kind = 0;
    ctrl.count = (int)p[1];
    flow_ctrl_request(owner, &ctrl);
}

/* Skips to flow time p[2] once the enemy count drops to p[1]. */
/* 0x001216f8 */
void flow_ctrl_request_skip(void *owner, float *p)
{
    FlowCtrl ctrl;

    ctrl.kind = 1;
    ctrl.count = (int)p[1];
    ctrl.time = p[2];
    flow_ctrl_request(owner, &ctrl);
}

/* "BPM Change": tempo p[1]. */
/* 0x00121730 */
void flow_func_bpm_change(void *owner, float *p)
{
    beat_clock_request_tempo(p[1]);
}
