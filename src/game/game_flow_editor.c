/*
 * Game flow: each stage area's timed event list (enemy spawns, function calls, tags), loaded from
 * "stg*.eve" or a memory card copy and played against the flow time. Also the FlowEdit editor.
 */

#include "debug.h"
#include "types_game.h"
#include "color.h"
#include "libc.h"
#include "gw_draw.h"
#include "gw_system.h"
#include "gw_window.h"
#include "game_flow_editor.h"
#include "backup.h"
#include "debugmenu.h"
#include "file.h"
#include "game.h"
#include "game_flow_enemy.h"
#include "game_flow_func.h"
#include "heap.h"
#include "item.h"
#include "module.h"
#include "pad.h"
#include "pcfile.h"
#include "stage.h"
#include "task.h"
#include "system.h"

typedef struct FlowEvent {
    float time;
    int type;
    int arg;
} FlowEvent;

typedef struct GameFlowEditorFlowEvent {
    float time;
    unsigned int type;
    int arg;
} GameFlowEditorFlowEvent;

typedef struct GameFlowEditorEntry {
    int id;
    int (*create)(void);
    int task;
} GameFlowEditorEntry;

typedef struct FlowEventNode {
    int a;
    int b;
    int c;
    struct FlowEventNode *next;
} FlowEventNode;

/* steer(alias): the code that matched reads FlowEventNode through these member types; one view
 * shared by those sites (src/game/game_flow_editor.c). */
typedef struct FlowEventNodeView {
    float time;
    int type;
    int arg;
    struct FlowEventNodeView *next;
} FlowEventNodeView;

typedef struct GameFlowData {
    char pad_00[40];
    int event_cnt;
    char pad_2c[4];
    int enemy_cnt;
    char pad_34[4];
    int func_cnt;
    FlowEventNodeView *head;
    int count;
} GameFlowData;

typedef struct FlowEditWork {
    int top;
    int cursor;
    float time;
    float sel_kind;
    int win[3];
    float time_saved;
} FlowEditWork;

typedef struct FlowEventSaveWork {
    char *name;
    int *buf;
    int size;
} FlowEventSaveWork;

typedef struct FlowEditMakeInfo {
    float value;
    int type;
} FlowEditMakeInfo;

/* steer(alias): the code that matched reads FlowEventNode through these member types; one view
 * shared by those sites (src/game/game_flow_editor.c). */
typedef struct FlowEventNodeStateView {
    float v;
    unsigned int state;
    int arg;
    struct FlowEventNodeStateView *next;
} FlowEventNodeStateView;

extern int game_flow_file_no;
extern char game_flow_file_name[];
extern char str_game_flow_event_file[];
extern char str_game_flow_init[];
extern char str_game_flow_exit_null[];
extern char str_game_flow_exit[];
extern char str_game_flow_enemy_regist_set[];
extern char str_game_flow_func_regist_set[];
extern void flow_func_tbl_call_fn4_all(void *);
extern void flow_func_tbl_call_fn8_all(void *);
extern char *game_flow_cur;
extern char str_game_flow_task_missing[];
extern void flow_func_events_free_array(void *);
extern char str_game_flow_load_file[];
extern int game_flow_file_desc[];
extern char str_game_flow_clear[];
extern char str_game_flow_is_load_end[];
extern char str_game_flow_start[];
extern char str_game_flow_stop[];
extern int game_flow_seek_req;
extern int game_flow_magic_v3;
extern int game_flow_magic_v2;
extern int game_flow_magic_v1;
extern int game_flow_magic_v0;
extern char str_game_flow_load[];
extern char str_game_flow_load_v2[];
extern char str_game_flow_load_v1[];
extern char str_game_flow_load_v0[];
extern char str_game_flow_load_unknown[];
extern char str_game_flow_mc_load[];
extern char str_game_flow_load_vms_done[];
extern char str_game_flow_load_not_found[];
extern char str_game_flow_load_file_done[];
extern char str_game_flow_count_rewind[];
extern char str_game_flow_index[];
extern char str_game_flow_index_end[];
extern char str_game_flow_enemy_set[];
extern char str_game_flow_func_set[];
extern struct TaskDesc game_flow_task_desc;
extern char str_game_flow_editor_init[];
extern char str_game_flow_menu[];
extern int game_flow_editor_entries[];
extern char str_game_flow_editor_exit[];
extern char str_flow_event_data[];
extern char str_game_flow_editor_file[];
extern void flow_func_events_free_list(void *);
extern unsigned char kbd_state[];
extern int debug_disp_level;
extern char str_flow_edit_help_edit[];
extern char str_flow_edit_help_new[];
extern char str_flow_edit_help_kill[];
extern char str_flow_edit_help_count[];
extern char str_flow_edit_help_save[];
extern char str_flow_edit_help_delete[];
extern float flow_edit_new_event;
extern char str_flow_edit_no_event[];
extern char str_flow_edit_nop[];
extern char str_flow_edit_enemy[];
extern char str_flow_edit_func[];
extern char str_flow_edit_tag[];
extern char str_flow_edit_empty[];
extern char str_flow_edit_enemy_edit[];
extern char str_flow_edit_func_edit[];
extern char str_flow_edit_count[];
extern char str_flow_edit_new[];
extern void flow_func_events_copy_array(GameFlowData *, char *, int);
extern struct TaskDesc flow_event_save_desc;
extern int *game_work;
extern unsigned int stage_exit_flags;
extern int (*flow_edit_mode_funcs[])(int *, int *);
extern void *system1_work;
extern struct TaskDesc flow_edit_desc;
extern char *flow_event_type_names[];
extern char str_flow_edit_fmt_s[];
extern char str_flow_edit_fmt_count[];
extern ScreenView screen_layout;
extern char str_flow_edit_main_flow[];
extern char str_flow_edit_sub_flow[];
extern char str_flow_edit_game_count[];
extern char str_flow_edit_cursor[];
extern char str_flow_edit_row[];
extern char str_flow_edit_enemy_row[];
extern char str_flow_edit_index[];
extern char ***flow_func_tbl_get(int *, int);

static void GameFlowEnemyRegistSet(void *table);
static void GameFlowFuncRegistSet(void *table);
static char *game_flow_get_named(char *name);
static void game_flow_free_data(unsigned int *data);
static int game_flow_task_create(void *parent);
static void GamwFlowEditorInit(void);
static void GameFlowEditorExit(void);
static void game_flow_editor_menu_cb(int item, int val, int idx);
static void flow_events_free_array(int *data);
static FlowEvent *flow_events_set_array(int *data, FlowEvent *src, int count);
static void flow_edit_make_window_draw(int *w);
static void flow_edit_windows_open(int *w);
static void flow_edit_list_draw(int *w);

/* 0x0011b4b8 */
static void game_flow_reset(int *data)
{
    data[3] = 0;
    data[4] = 0;
    data[5] = 0;
    data[6] = 0;
    data[7] = 0;
    data[8] = 0;
    data[9] = 0;
    data[10] = 0;
    data[11] = 0;
    data[12] = 0;
    data[15] = 0;
    data[16] = 0;
    data[17] = 0;
    data[18] = 0;
}

/* "stg<name>_<nn>.eve": nn = stage phase + 1, or + 11 for game types 4 and 5. */
/* 0x0011b4f8 */
static char *game_flow_event_file_name(void)
{
    unsigned int type = game_type_get();
    char *name;
    char *buf;
    char *fmt;

    if (type < 6) {
        if (type < 4)
            game_flow_file_no = 1;
        else
            game_flow_file_no = 11;
    } else {
        game_flow_file_no = 1;
    }
    buf = game_flow_file_name;
    fmt = str_game_flow_event_file;
    name = stage_get_name();
    sprintf(buf, fmt, name, stage_get_phase() + game_flow_file_no);
    return buf;
}

/* Ends at -1; 0 for NULL. */
/* 0x0011b598 */
static int game_flow_enemy_table_count(unsigned int *table)
{
    int n = 0;

    if (table != 0) {
        while (*table != 0xffffffff) {
            table++;
            n++;
        }
    }
    return n;
}

/* Ends at -1; 0 for NULL. */
/* 0x0011b5e8 */
static int game_flow_func_table_count(unsigned int *table)
{
    int n = 0;

    if (table != 0) {
        while (*table != 0xffffffff) {
            table++;
            n++;
        }
    }
    return n;
}

/* Also the item and enemy tasks and the FlowEdit menu. */
/* 0x0011b638 */
char *GameFlowInit(void *parent, int val, void *enemy_tbl, void *func_tbl)
{
    char *flow;
    int *data;

    DEBUG_PRINT(str_game_flow_init);
    flow = game_flow_task_create(parent);
    data = (int *)(flow + 24);
    game_flow_free_data(data);
    data[2] = val;
    GameFlowEnemyRegistSet(enemy_tbl);
    GameFlowFuncRegistSet(func_tbl);
    item_task_create(flow);
    EnemyTaskInit(flow, data);
    GamwFlowEditorInit();
    return flow;
}

/* 0x0011b6e0 */
void GameFlowExit(char *flow)
{
    char *data;

    if (flow == 0) {
        DEBUG_PRINT(str_game_flow_exit_null);
        return;
    }
    data = flow + 24;
    GameFlowEditorExit();
    DEBUG_PRINT(str_game_flow_exit);
    EnemyTaskExit(data);
    game_flow_free_data(data);
    do {
        TASK_OF(flow)->flags |= TASK_F_END;
    } while (0);
}

/* 0x0011b758 */
static void GameFlowEnemyRegistSet(void *table)
{
    int *data = (int *)(game_flow_get_named(str_game_flow_enemy_regist_set) + 24);
    int count;

    count = game_flow_enemy_table_count(table);
    data[3] = (int)table;
    data[4] = count;
}

/* 0x0011b7a0 */
static void GameFlowFuncRegistSet(void *table)
{
    int *data = (int *)(game_flow_get_named(str_game_flow_func_regist_set) + 24);
    int count;

    count = game_flow_func_table_count(table);
    data[5] = (int)table;
    data[6] = count;
}

/* Register hooks of every enemy and function entry. */
/* 0x0011b7e8 */
void game_flow_regist_init(char *flow)
{
    char *data = flow + 24;

    flow_enemy_tbl_call_fn4_regist(data);
    flow_func_tbl_call_fn4_all(data);
}

/* Release hooks of every enemy and function entry. */
/* 0x0011b818 */
void game_flow_regist_release(char *flow)
{
    char *data;

    if (flow != 0) {
        data = flow + 24;
        flow_enemy_tbl_call_fn8_regist(data);
        flow_func_tbl_call_fn8_all(data);
    }
}

/* Reports name when none or ending. */
/* 0x0011b858 */
static char *game_flow_get_named(char *name)
{
    char *flow = game_flow_cur;

    if (flow == 0 || (TASK_OF(flow)->flags & TASK_F_END))
        ERROR_PRINTF(str_game_flow_task_missing, name);
    return flow;
}

/* Also clears the finished flag. */
/* 0x0011b8a8 */
static void game_flow_free_data(unsigned int *data)
{
    flow_events_free_array(data);
    flow_enemy_events_free_array(data);
    flow_func_events_free_array(data);
    data[0] &= ~0x10;
}

/* The load runs in game_flow_load_step. */
/* 0x0011b8f0 */
void GameFlowLoadFile(void)
{
    int *flow = (int *)game_flow_get_named(str_game_flow_load_file);
    int *data;

    game_flow_file_desc[0] = stage_get_module();
    data = flow + 6;
    game_flow_file_desc[1] = game_flow_event_file_name();
    if (flow != 0) {
        game_flow_free_data(data);
        flow[3] = (int)game_flow_file_desc;
        flow[6] |= 2;
        flow[2] = 0;
    }
}

/* Also cancels a load in progress. */
/* 0x0011b968 */
void GameFlowClear(void)
{
    unsigned int *flow = game_flow_get_named(str_game_flow_clear);
    unsigned int *data = flow + 6;

    if (flow != 0) {
        game_flow_free_data(data);
        if (flow[4] != 0)
            file_request_cancel(flow[3]);
        flow[6] &= ~2;
        flow[6] &= ~4;
        flow[4] = 0;
        flow[2] = 0;
        flow[3] = 0;
    }
}

/* No flow, no load asked, or finished. */
/* 0x0011b9e8 */
static int GameFlowIsLoadEnd(void)
{
    int *p = game_flow_get_named(str_game_flow_is_load_end);

    if (p == 0)
        return 1;
    if (p[6] & 2)
        return (p[6] >> 2) & 1;
    return 1;
}

/* 0x0011ba30 */
int game_flow_is_finished(void)
{
    int *p = game_flow_get_named(str_game_flow_is_load_end);

    return p[6] & 0x10;
}

/* Lets the counters run (GameCountStop). */
/* 0x0011ba58 */
void GameFlowStart(void)
{
    int *p = game_flow_get_named(str_game_flow_start);

    if (p != 0) {
        p[6] |= 8;
        GameCountStop();
    }
}

/* Resets the times, stops the counters (GameCountStart). */
/* 0x0011ba98 */
void GameFlowStop(void)
{
    unsigned int *p = game_flow_get_named(str_game_flow_stop);

    if (p != 0) {
        game_times_reset();
        GameCountStart();
        p[6] &= ~8;
    }
}

/* To the previous tag event, in game_flow_play_events. */
/* 0x0011baf0 */
void game_flow_queue_back(void)
{
    game_flow_seek_req--;
}

/* To the next tag event. */
/* 0x0011bb08 */
void game_flow_queue_forward(void)
{
    game_flow_seek_req++;
}

/*
 * Versions game_flow_magic_v0..v3: flow events, then enemy events in a per-version layout; v3 adds
 * function events. NULL image drops the events; unknown versions reported.
 */
/* 0x0011bb20 */
static void game_flow_load(int *ed, int *image)
{
    int *src;

    if (image != 0) {
        if (image[0] == game_flow_magic_v3) {
            DEBUG_PRINT(str_game_flow_load);
            src = flow_events_set_array(ed, image + 4, image[1]);
            src = flow_enemy_events_set_array(ed, src, image[2]);
            flow_func_events_set_array(ed, src, image[3]);
        } else if (image[0] == game_flow_magic_v2) {
            DEBUG_PRINT(str_game_flow_load_v2);
            src = flow_events_set_array(ed, image + 3, image[1]);
            flow_enemy_events_set_array(ed, src, image[2]);
        } else if (image[0] == game_flow_magic_v1) {
            DEBUG_PRINT(str_game_flow_load_v1);
            src = flow_events_set_array(ed, image + 3, image[1]);
            flow_enemy_events_set_array_v1(ed, src, image[2]);
        } else if (image[0] == game_flow_magic_v0) {
            DEBUG_PRINT(str_game_flow_load_v0);
            src = flow_events_set_array(ed, image + 3, image[1]);
            flow_enemy_events_set_array_v0(ed, src, image[2]);
        } else {
            ERROR_PRINTF(str_game_flow_load_unknown);
        }
    } else {
        game_flow_free_data(ed);
    }
}

/* Becomes game_flow_cur; the previous one is restored on release. */
/* 0x0011bca8 */
static void game_flow_task_init(int *flow)
{
    int *prev = game_flow_cur;
    int *data = flow + 6;

    game_flow_cur = flow;
    flow[27] = (int)prev;
    game_flow_reset(data);
    flow[6] = 0;
    data[1] = 0;
    flow[4] = 0;
    flow[5] = (int)game_flow_time_get();
    flow[2] = 0;
    flow[28] = 0;
}

/* Abandons card and file loads in progress. */
/* 0x0011bd10 */
static void game_flow_task_release(int *flow)
{
    int mask;

    if (flow[28] != 0) {
        heap_free(filereq_get_buf(), 0);
        filereq_clear();
        mask = debug_color_set(RGBA(0x80, 0, 0, 0x80));
        DEBUG_PRINT(str_game_flow_mc_load);
        debug_color_set(mask);
    }
    game_flow_cur = (int *)flow[27];
    if (flow[4] != 0)
        file_request_cancel(flow[3]);
}

/* States 100-101: card (auto_load_drive). 1 when taken; on failure falls back to disc (200). */
/* 0x0011bdb0 */
static int game_flow_load_vms_step(int *flow, int data)
{
    int done = 0;
    int buf;

    switch (flow[2]) {
    case 100:
        if (filereq_is_free()) {
            filereq_set_path(auto_load_drive, ((int *)flow[3])[1], 0);
            BackupLoadTaskSet(flow);
            flow[28] = 1;
            flow[2]++;
        }
        break;
    case 101:
        if (filereq_is_idle()) {
            buf = filereq_get_buf();
            if (filereq_get_state() == 2) {
                game_flow_load(data, buf);
                done = 1;
                DEBUG_PRINT(str_game_flow_load_vms_done);
            } else {
                flow[2] = 200;
            }
            flow[28] = 0;
            heap_free(buf, 0);
            filereq_clear();
        }
        break;
    }
    return done;
}

/* States 200-201: 1 when taken. A missing file is reported, leaves no events, and is retried. */
/* 0x0011bec8 */
static int game_flow_load_file_step(int *flow, int data)
{
    int done = 0;
    int buf;
    int status;

    switch (flow[2]) {
    case 200:
        file_request_simple(flow[3], 0);
        flow[4] = 1;
        flow[2] = flow[2] + 1;
        break;
    case 201:
        status = file_request_status(flow[3]);
        switch (status) {
        case -1:
            DEBUG_PRINTF(str_game_flow_load_not_found, ((int *)flow[3])[1]);
            game_flow_load(data, 0);
            flow[2] = 200;
            break;
        case 1:
            filer_set_flags(1);
            done = 1;
            buf = file_load_sync(flow[3], 0, 0);
            game_flow_load(data, buf);
            filer_reset_flags();
            file_heap_free(buf);
            DEBUG_PRINTF(str_game_flow_load_file_done, ((int *)flow[3])[1]);
            flow[3] = 0;
            flow[4] = 0;
            flow[2] = 0;
            break;
        }
        break;
    }
    return done;
}

/* From disc (state 200). When done: finished flag, index rewound, flow time reset. */
/* 0x0011c000 */
static void game_flow_load_step(int *flow, unsigned int *data)
{
    int done = 0;
    int state = flow[2];

    if (state == 0) {
        data[0] = (data[0] | 2) & ~4;
        flow[2] = 200;
        state = 200;
    }
    if (state >= 200)
        done = game_flow_load_file_step(flow, data);
    else if (state >= 100)
        done = game_flow_load_vms_step(flow, data);
    if (done != 0) {
        data[0] = (data[0] & ~2) | 4;
        flow[2] = 0;
        data[1] = 0;
        flow[5] = 0;
        game_flow_time_reset();
    }
}

/* To the nearest tag event (type 3) or the first event; flow time set to it. */
/* 0x0011c0c8 */
static void game_flow_seek_back(int *data)
{
    FlowEvent *ev = (FlowEvent *)data[9] + data[1];

    while (data[1] > 0 && ev->type != 3) {
        data[1]--;
        ev--;
    }
    game_flow_time_set(ev->time);
}

/* To the next tag event or the end; flow time = whole-frame time of the last event reached. */
/* 0x0011c118 */
static void game_flow_seek_forward(int *data)
{
    int count = data[10];
    int idx = data[1];
    int next;
    FlowEvent *ev;
    int ev_time;

    if (idx < count) {
        ev = (FlowEvent *)data[9] + idx;
        ev_time = (int)ev->time;
        if (ev->type != 3) {
            do {
                next = idx + 1;
                ev++;
                data[1] = next;
                if (!(next < count))
                    break;
                ev_time = (int)ev->time;
                if (ev->type == 3)
                    break;
                idx = next;
            } while (1);
        }
        game_flow_time_set((float)ev_time);
    }
}

/*
 * Flow time gone backwards: index moved back (reported). Applies requested seeks. Due events fire:
 * type 1 via flow_enemy_event_call, type 2 via flow_func_event_call; none while FlowEdit is open.
 */
/* 0x0011c198 */
static void game_flow_play_events(int *flow, int *data)
{
    int now;
    GameFlowEditorFlowEvent *ev;
    int i;
    int n;

    if (data[9] == 0)
        return;
    now = (int)game_flow_time_get();
    if (now < flow[5]) {
        int i;

        i = 0;
        ERROR_PRINTF(str_game_flow_count_rewind, flow[5], now);
        ERROR_PRINTF(str_game_flow_index, data[1]);
        ev = (GameFlowEditorFlowEvent *)data[9];
        if (ev->time <= (float)now) {
            do {
                ev++;
                i++;
            } while (ev->time <= (float)now);
        }
        data[1] = i;
        ERROR_PRINTF(str_game_flow_index_end, i);
    }
    flow[5] = now;
    if (game_flow_seek_req != 0) {
        if (game_flow_seek_req > 0) {
            do {
                game_flow_seek_forward(data);
                game_flow_seek_req--;
            } while (game_flow_seek_req > 0);
        }
        if (game_flow_seek_req < 0) {
            do {
                game_flow_seek_back(data);
                game_flow_seek_req++;
            } while (game_flow_seek_req < 0);
        }
        now = (int)game_flow_time_get();
    }
    i = data[1];
    n = data[10];
    if (i < n) {
        int next;
        float limit;

        ev = (GameFlowEditorFlowEvent *)data[9] + i;
        limit = (float)now;
        if (ev->time <= limit) {
            do {
                if (!(data[0] & 1)) {
                    switch (ev->type) {
                    case 1:
                        DEBUG_PRINTF(str_game_flow_enemy_set, ev->time, ev->arg);
                        flow_enemy_event_call(data, ev->arg);
                        n = data[10];
                        break;
                    case 0:
                        break;
                    case 2:
                        DEBUG_PRINTF(str_game_flow_func_set, ev->time, ev->arg);
                        flow_func_event_call(data, ev->arg);
                        n = data[10];
                        break;
                    }
                }
                ev++;
                next = data[1] + 1;
                data[1] = next;
                if (!(next < n))
                    break;
            } while (ev->time <= limit);
        }
    } else {
        data[0] |= 0x10;
    }
}

/*
 * Plays while playing and not loading; advances a load. data[0] flags: bit 0 FlowEdit open,
 * 1 loading, 2 load finished, 3 playing, 4 all events played.
 */
/* 0x0011c420 */
static void game_flow_task_exec(int *flow)
{
    int *data = flow + 6;

    if (flow[6] & 8) {
        if (!(flow[6] & 2))
            game_flow_play_events(flow, data);
    }
    if (flow[6] & 2)
        game_flow_load_step(flow, data);
}

/* 0x0011c498 */
static void game_flow_task_disp(void)
{
}

/* 0x0011c4a0 */
static int game_flow_task_create(void *parent)
{
    return task_create_inherit(parent, &game_flow_task_desc, 0x74);
}

/* 0 if none. */
/* 0x0011c4c8 */
void *game_flow_get_data(void)
{
    if (game_flow_cur != 0)
        return game_flow_cur + 0x18;
    return 0;
}

/* 0x0011c4e8 */
void *game_flow_get_current(void)
{
    return game_flow_cur;
}

/* 0x0011c4f8 */
int game_flow_get_task_parent(void)
{
    if (game_flow_cur != 0)
        return *(int *)(game_flow_cur + 0x38);
    return 0;
}

/* First event at or after the flow time. */
/* 0x0011c518 */
void game_flow_seek_to_time(void)
{
    int *data = game_flow_get_data();
    int idx, now;
    FlowEvent *ev;
    float fl;

    if (data[1] < data[10]) {
        now = (int)game_flow_time_get();
        idx = data[1];
        ev = (FlowEvent *)data[9] + idx;
        if (idx < data[10]) {
            fl = (float)now;
            if (!(fl <= ev->time)) {
                do {
                    idx++;
                    ev++;
                    if (idx >= data[10])
                        break;
                } while (!(fl <= ev->time));
            }
        }
        data[1] = idx;
    }
}

/* One item per game_flow_editor_entries entry; only FlowEdit is used. */
/* 0x0011c5d0 */
static void GamwFlowEditorInit(void)
{
    unsigned int i;
    int *ent;

    DEBUG_PRINT(str_game_flow_editor_init);
    menu_item_add(0, str_game_flow_menu, 0, 0, 1);
    ent = game_flow_editor_entries;
    for (i = 0; i < 1; i++, ent += 3)
        menu_item_add(str_game_flow_menu, (void *)ent[0], game_flow_editor_menu_cb, i, 0);
}

/* 0x0011c660 */
static void GameFlowEditorExit(void)
{
    DEBUG_PRINT(str_game_flow_editor_exit);
    menu_item_remove(0, str_game_flow_menu);
}

/* 0x0011c690 */
static void game_flow_editor_menu_cb(int item, int val, int idx)
{
    game_flow_editor_toggle(idx);
}

/* Ends its task if running, else creates it; menu tick follows. */
/* 0x0011c698 */
void game_flow_editor_toggle(int idx)
{
    GameFlowEditorEntry *ent = &((GameFlowEditorEntry *)&game_flow_editor_entries)[idx]; /* steer(cast) */

    if (ent->task != 0) {
        do {
            TASK_OF(ent->task)->flags |= TASK_F_END;
        } while (0);
        ent->task = 0;
        menu_item_set_value(str_game_flow_menu, ent->id, 0);
    } else {
        ent->task = ent->create();
        if (ent->task != 0)
            menu_item_set_value(str_game_flow_menu, ent->id, 1);
    }
}

/* 0x0011c738 */
static void flow_events_free_array(int *data)
{
    data[10] = 0;
    heap_free(data[9], 0);
    data[9] = 0;
}

/* 0x0011c770 */
static void flow_events_free_list(int *data)
{
    int *node = (int *)data[15];
    int *dead;

    if (node != 0) {
        do {
            dead = node;
            node = (int *)node[3];
            heap_free(dead, 0);
        } while (node != 0);
    }
    data[15] = 0;
    data[16] = 0;
}

/* For saving; returns the end of what was written. */
/* 0x0011c7c8 */
static FlowEvent *flow_events_copy_array(int *data, FlowEvent *dst, int count)
{
    FlowEvent *src;
    int i;

    count = count < data[10] ? count : data[10];
    src = (FlowEvent *)data[9];
    for (i = 0; i < count; i++) {
        *dst = *src;
        src++;
        dst++;
    }
    return dst;
}

/* Returns the data after them. */
/* 0x0011c818 */
static FlowEvent *flow_events_set_array(int *data, FlowEvent *src, int count)
{
    FlowEvent *dst;

    flow_events_free_array(data);
    if (count > 0) {
        data[10] = count;
        dst = heap_block_tag(game_malloc(count * 12), str_flow_event_data, -1);
        data[9] = (int)dst;
        for (; count != 0; count--) {
            *dst = *src;
            src++;
            dst++;
        }
    }
    return src;
}

/* 0x0011c8b0 */
static void flow_events_array_to_list(int *data)
{
    int *tail = data + 15;
    int n = data[10];
    FlowEvent *src = (FlowEvent *)data[9];
    int *node;
    int cur;

    if (data[15] != 0)
        flow_events_free_list(data);
    data[16] = n;
    if (n == 0) {
        data[15] = 0;
        return;
    }
    do {
        node = heap_block_tag(game_malloc(16), str_game_flow_editor_file, 231);
        *(FlowEvent *)node = *src;
        node[3] = 0;
        src++;
        cur = *tail;
        if (cur == 0)
            *tail = (int)node;
        else
            ((int *)cur)[3] = (int)node;
        n--;
        tail = &node[3];
    } while (n != 0);
}

/* 0x0011c978 */
static void flow_events_list_to_array(int *data)
{
    int n;
    FlowEventNode *src;
    FlowEvent *dst;

    flow_events_free_array(data);
    if (data[16] > 0) {
        data[10] = data[16];
        n = data[16];
        src = (FlowEventNode *)data[15];
        dst = heap_block_tag(game_malloc(n * 12), str_game_flow_editor_file, 258);
        data[9] = (int)dst;
        for (; n != 0; n--) {
            *dst = *(FlowEvent *)src;
            dst++;
            src = src->next;
        }
    }
}

/* Last link when idx is past the end. */
/* 0x0011ca18 */
static FlowEventNode **flow_event_list_slot(char *data, int idx)
{
    FlowEventNode **slot = (FlowEventNode **)(data + 60);
    FlowEventNode *node;

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

/* 0x0011ca50 */
static FlowEventNode *flow_event_list_nth(char *data, int idx)
{
    return *flow_event_list_slot(data, idx);
}

/* After every event with the same or an earlier time. Returns its position. */
/* 0x0011ca70 */
static int flow_event_list_insert(GameFlowData *data, FlowEvent *src)
{
    FlowEventNodeView *node;
    FlowEventNodeView **slot;
    FlowEventNodeView *cur;
    void *mem;
    int idx;

    mem = game_malloc(16);
    slot = &data->head;
    node = heap_block_tag(mem, str_game_flow_editor_file, 310);
    *(FlowEvent *)node = *src;
    node->next = 0;
    idx = 0;
    for (;;) {
        cur = *slot;
        if (cur == 0) {
            *slot = node;
            break;
        }
        if (cur->time > node->time) {
            *slot = node;
            node->next = cur;
            break;
        }
        slot = &cur->next;
        idx++;
    }
    data->count++;
    return idx;
}

/* Hangs if the count goes negative. */
/* 0x0011cb30 */
static void flow_event_list_remove(int *data, int idx)
{
    FlowEventNode **slot = flow_event_list_slot(data, idx);
    FlowEventNode *node = *slot;

    if (node != 0) {
        *slot = node->next;
        heap_free(node, 0);
        if (--data[16] < 0)
            for (;;)
                ;
    }
}

/* Stops playback, keeps flow time, tables -> lists, opens windows, takes pad, stops counters. */
/* 0x0011cba0 */
static void flow_edit_init(FlowEditWork *w)
{
    unsigned int *data;
    float now;

    data = game_flow_get_data();
    w->sel_kind = 0;
    *data |= 1;
    w->top = 0;
    w->cursor = 0;
    now = game_flow_time_get();
    w->time_saved = now;
    w->time = now;
    flow_events_array_to_list(data);
    flow_enemy_events_array_to_list(data);
    flow_func_events_array_to_list(data);
    flow_edit_windows_open(w);
    pad_state_owner_add(w);
    GameCountStart();
    flow_enemy_edit_colors_reset();
    flow_func_edit_colors_reset();
}

/* Restores flow time and counters; lists -> tables. */
/* 0x0011cc30 */
static void flow_edit_release(FlowEditWork *w)
{
    unsigned int *data = game_flow_get_data();

    data[0] &= ~1;
    game_flow_time_set(w->time_saved);
    GameCountStop();
    flow_events_list_to_array(data);
    flow_enemy_events_list_to_array(data);
    flow_func_events_list_to_array(data);
    flow_events_free_list(data);
    flow_enemy_events_free_list(data);
    flow_func_events_free_list(data);
    pad_owner_remove(w);
    gw_window_close(w->win[1]);
    gw_window_close(w->win[0]);
}

/* 'O' forward, 'P' back; 1 frame, 10 with a modifier; not below 0. */
/* 0x0011ccc8 */
static void flow_edit_pad_seek(float *ed)
{
    float step;
    float t;

    step = 1.0f;
    t = ed[2];
    if (kbd_state[7] & 0x22)
        step = 10.0f;
    if (kbd_state[6] == 'O')
        t = t + step;
    if (kbd_state[6] == 'P')
        t = t - step;
    if (t >= 0.0f)
        ed[2] = t;
    else
        ed[2] = 0.0f;
    game_flow_time_set(ed[2]);
}

/* 'Q' down, 'R' up; 1, or 5 with a modifier. */
/* 0x0011cd30 */
static void flow_edit_pad_select(int *ed)
{
    int sel;

    sel = ed[1];
    if (kbd_state[7] & 0x22) {
        if (kbd_state[6] == 'Q')
            sel = sel + 5;
        if (kbd_state[6] == 'R')
            sel = sel - 5;
    } else {
        if (kbd_state[6] == 'Q')
            sel = sel + 1;
        if (kbd_state[6] == 'R')
            sel = sel - 1;
    }
    ed[1] = sel;
}

/* Cursor kept on an event and among the visible rows. */
/* 0x0011cd90 */
static void flow_edit_scroll_clamp(int *ed, int *data)
{
    int sel, clamped, last, top, min_top;

    sel = ed[1];
    clamped = sel > -1 ? sel : 0;
    if (!(clamped < data[16] - 1))
        clamped = data[16] - 1;
    top = ed[0];
    min_top = sel + 1;
    min_top = min_top - ed[6];
    if (top < min_top)
        top = min_top;
    ed[1] = clamped;
    if (!(top < sel))
        top = sel;
    if (!(-1 < top))
        top = 0;
    last = data[16] - 1;
    if (!(top < last))
        top = last;
    ed[0] = top;
    /* Never runs: (sel & 1) >> 1 is 0. */
    while ((sel & 1) >> 1)
        sel--;
}

/* Debug display level 0. */
/* 0x0011cdf8 */
static void flow_edit_key_help(void)
{
    if (debug_disp_level == 0) {
        debug_set_origin(0, -6);
        debug_puts(str_flow_edit_help_edit);
        debug_newline();
        debug_puts(str_flow_edit_help_new);
        debug_newline();
        debug_puts(str_flow_edit_help_kill);
        debug_newline();
        debug_puts(str_flow_edit_help_count);
        debug_newline();
        debug_puts(str_flow_edit_help_save);
        debug_newline();
        debug_puts(str_flow_edit_help_delete);
        debug_newline();
    }
}

/* 'O'/'P' 1 frame (10 with a modifier), 'R'/'Q' 100 steps; not below 0. */
/* 0x0011cea0 */
static void flow_edit_pad_time(float *rec)
{
    float step;
    float t;
    float clamped;

    step = 1.0f;
    t = rec[0];
    if (kbd_state[7] & 0x22)
        step = 10.0f;
    if (kbd_state[6] == 'O')
        t = t + step;
    if (kbd_state[6] == 'P')
        t = t - step;
    if (kbd_state[6] == 'R')
        t = t + step * 100.0f;
    if (kbd_state[6] == 'Q')
        t = t - step * 100.0f;
    clamped = 0.0f;
    if (t >= 0.0f)
        clamped = t;
    rec[0] = clamped;
}

/* Types 0 nop, 1 enemy, 2 function, 3 tag; 'K' next, 'N' previous; out of range -> 3. */
/* 0x0011cf38 */
static void flow_edit_pad_type(int *rec)
{
    unsigned type;

    type = rec[1];
    if (kbd_key2 == 'K')
        type = type + 1;
    if (kbd_key2 == 'N')
        type = type - 1;
    rec[1] = type < 4 ? type : 3;
}

/*
 * Mode 0: seek, cursor, selected event in SubFlow; keys switch to edit (Enter), new, kill, count
 * change, save; another key removes all flow enemies.
 */
/* 0x0011cf70 */
static int flow_edit_mode_list(int *ed, int data)
{
    unsigned int key = kbd_key2;
    unsigned int *ev;

    ((void (*)(int *, int))flow_edit_pad_seek)(ed, data); /* steer(cast) */
    ev = flow_event_list_nth(data, ed[1]);
    if (key == 17) {
        if (ev != 0)
            flow_edit_new_event = *(float *)ev;
        ed[3] = 4;
    }
    if (ev == 0) {
        gw_window_set_title(ed[5], str_flow_edit_no_event);
        gw_window_set_handlers(ed[5], 0, 0, 0);
        return 1;
    }
    ((void (*)(int *, int))flow_edit_pad_select)(ed, data); /* steer(cast) */
    switch (ev[1]) {
    case 0:
        gw_window_set_title(ed[5], str_flow_edit_nop);
        gw_window_set_handlers(ed[5], 0, 0, 0);
        break;
    case 1:
        gw_window_set_title(ed[5], str_flow_edit_enemy);
        gw_window_set_handlers(ed[5], 0, flow_enemy_edit_disp, 0);
        ((int *)ed[5])[27] = ev[2];
        break;
    case 2:
        gw_window_set_title(ed[5], str_flow_edit_func);
        gw_window_set_handlers(ed[5], 0, flow_func_edit_disp, 0);
        ((int *)ed[5])[27] = ev[2];
        break;
    case 3:
        gw_window_set_title(ed[5], str_flow_edit_tag);
        gw_window_set_handlers(ed[5], 0, 0, 0);
        break;
    default:
        gw_window_set_title(ed[5], str_flow_edit_empty);
        gw_window_set_handlers(ed[5], 0, 0, 0);
        break;
    }
    if (ev != 0) {
        switch (key) {
        case 14:
            ed[3] = 2;
            break;
        case 6:
            ed[3] = 3;
            break;
        case 40:
            ed[3] = 1;
            break;
        case 22:
            ed[3] = 5;
            break;
        case 7:
            flow_enemy_kill_all();
            break;
        }
    }
    flow_edit_key_help();
    return 1;
}

/* Mode 1. Nop and tag events return to the list at once. */
/* 0x0011d178 */
static int flow_edit_mode_edit(int *ed, int data)
{
    unsigned int *ev;
    int stay = 1;

    ev = flow_event_list_nth(data, ed[1]);
    switch (ev[1]) {
    case 0:
        stay = 0;
        break;
    case 1:
        gw_window_set_title(ed[5], str_flow_edit_enemy_edit);
        stay = flow_editor_param_exec(data, ev[2]);
        break;
    case 2:
        gw_window_set_title(ed[5], str_flow_edit_func_edit);
        stay = flow_func_edit_update(data, ev[2]);
        break;
    case 3:
        stay = 0;
        break;
    default:
        stay = 1;
        break;
    }
    return stay;
}

/* Mode 2: also removes its enemy or function record. */
/* 0x0011d240 */
static int flow_edit_mode_kill(int *ed, int *data)
{
    unsigned int *ev = *flow_event_list_slot(data, ed[1]);

    switch (ev[1]) {
    case 0:
        break;
    case 1:
        flow_enemy_event_list_remove(data, ev[2]);
        break;
    case 2:
        flow_func_event_list_remove(data, ev[2]);
        break;
    }
    flow_event_list_remove(data, ed[1]);
    if (ed[1] == data[16]) {
        ed[1] = ed[1] - 1;
        if (ed[1] < 0)
            ed[1] = 0;
    }
    return 0;
}

/* Mode 3: edits the time and re-sorts, cursor following. Key 0x28 returns to the list. */
/* 0x0011d2f0 */
static int flow_edit_mode_count_change(int *ed, int data)
{
    FlowEvent *ev;
    FlowEvent rec;
    float old;
    int stay = 1;

    gw_window_set_title(ed[5], str_flow_edit_count);
    gw_window_set_handlers(ed[5], 0, 0, 0);
    ((int *)ed[5])[27] = (int)ed;
    ev = flow_event_list_nth(data, ed[1]);
    old = *(float *)ev;
    flow_edit_pad_time((float *)ev);
    if (*(float *)ev != old) {
        rec = *ev;
        flow_event_list_remove(data, ed[1]);
        ed[1] = flow_event_list_insert(data, &rec);
    }
    if (kbd_key == 0x28)
        stay = 0;
    return stay;
}

/* Mode 4: edits flow_edit_new_event. '(' adds it (with a new record if needed), '*' cancels. */
/* 0x0011d3d0 */
static int flow_edit_mode_new(int *ed, int data)
{
    int stay;
    int *win;
    FlowEvent *rec = &flow_edit_new_event;

    gw_window_set_title(ed[5], str_flow_edit_new);
    gw_window_set_handlers(ed[5], 0, flow_edit_make_window_draw, 0);
    win = (int *)ed[5];
    win[27] = (int)ed;
    win[28] = (int)rec;
    flow_edit_pad_time(rec);
    flow_edit_pad_type(rec);
    stay = 1;
    if (kbd_key == '*')
        stay = 0;
    if (kbd_key == '(') {
        switch ((unsigned int)rec->type) {
        case 1:
            rec->arg = flow_enemy_event_list_append(data);
            break;
        case 0:
            break;
        case 2:
            rec->arg = flow_func_event_list_append(data);
            break;
        default:
            break;
        }
        stay = 0;
        ed[1] = flow_event_list_insert(data, &flow_edit_new_event);
    }
    return stay;
}

/* v3 image: header with the three counts, then flow, enemy, function events; area's file name. */
/* 0x0011d4d8 */
static void flow_event_save_init(FlowEventSaveWork *out, GameFlowData *data)
{
    int size;
    int n;
    int enemy_size;
    int *buf;
    char *dst;

    out->name = game_flow_event_file_name();
    flow_events_list_to_array(data);
    flow_enemy_events_list_to_array(data);
    flow_func_events_list_to_array(data);
    size = data->event_cnt * 12 + data->func_cnt * 16;
    enemy_size = data->enemy_cnt * 64 + 16;
    size = size + enemy_size;
    out->size = size;
    buf = heap_block_tag(game_malloc(size), str_game_flow_editor_file, 862);
    out->buf = buf;
    buf[0] = (*(const int *)&game_flow_magic_v3); /* steer(cast) */
    n = data->event_cnt;
    buf[1] = n;
    buf[2] = data->enemy_cnt;
    buf[3] = data->func_cnt;
    dst = flow_events_copy_array(data, (FlowEvent *)(buf + 4), n);
    dst = flow_enemy_events_copy_array(data, dst, out->buf[2]);
    flow_func_events_copy_array(data, dst, out->buf[3]);
}

/* Also saves to the host PC, then frees. */
/* 0x0011d5c8 */
static void flow_event_save_release(int *save)
{
    pcfile_save_set(save[0], save[1], save[2]);
    heap_free(save[1], 0);
}

/* Ends once the card write is done. */
/* steer(cast) */
/* 0x0011d600 */
static void flow_event_save_wait(char *save)
{
    if (((int (*)())filereq_is_idle)(save)) {
        filereq_clear();
        if (save != 0)
            TASK_OF(save)->flags |= TASK_F_END;
    }
}

/* Card free: writes to auto_load_drive, then exec -> flow_event_save_wait. */
/* steer(cast) */
/* 0x0011d648 */
static void flow_event_save_exec(int *save)
{
    if (((int (*)())filereq_is_free)(save)) {
        filereq_set(4, auto_load_drive, save[0], save[1], save[2], 1);
        BackupSaveTaskSet();
        *(void (**)(void *))((char *)save - 48) = flow_event_save_wait;
    }
}

/* 0x0011d6a8 */
static void *flow_event_save_create(void *parent, int prm)
{
    return task_create_arg(parent, &flow_event_save_desc, 12, prm);
}

/* Mode 5. */
/* 0x0011d6d0 */
static int flow_edit_mode_save(void *ed, int data)
{
    flow_event_save_create(ed, data);
    return 0;
}

/*
 * Closes when the game ends or the stage exits. While MainFlow is active, runs
 * flow_edit_mode_funcs[mode] (0 -> back to the list). Runs on the module heap.
 */
/* 0x0011d6f0 */
static void flow_edit_exec(int *w)
{
    int old_heap;
    int *data;

    if (game_work == 0 || (game_work[-9] & 1) || (stage_exit_flags & 0x13)) {
        game_flow_editor_toggle(0);
        gw_window_set_handlers(w[5], 0, 0, 0);
        return;
    }
    old_heap = heap_set_current(module_get_heap());
    *(float *)(w + 7) = game_flow_time_get();
    pad_state_restore_owned(w);
    if (gw_window_has_active(w[4])) {
        data = game_flow_get_data();
        if (flow_edit_mode_funcs[w[3]](w, data) == 0)
            w[3] = 0;
        flow_edit_scroll_clamp(w, data);
    }
    pad_state_clear_owned(w);
    heap_set_current(old_heap);
}

/* 0x0011d800 */
static void flow_edit_disp(void)
{
}

/* Only with a game flow running; task or 0. */
/* 0x0011d808 */
static char *flow_edit_create(void)
{
    int old_heap;
    char *task;

    if (game_work == 0 || game_flow_get_data() == 0)
        return 0;
    old_heap = heap_set_current(module_get_heap());
    task = task_create(system1_work, &flow_edit_desc, 32);
    TASK_OF(task)->flags |= TASK_F_RUN_PAUSED;
    heap_set_current(old_heap);
    return task;
}

/* New-event mode: type name and time. */
/* 0x0011d898 */
static void flow_edit_make_window_draw(int *w)
{
    int cell[2];
    FlowEditMakeInfo *info = (FlowEditMakeInfo *)w[28];

    gw_get_cell_size(&cell[0], &cell[1]);
    gw_window_set_size(w, cell[0] * 31, cell[1] * 8);
    gw_win_printf(w, 4, 0, str_flow_edit_fmt_s, flow_event_type_names[info->type]);
    gw_win_printf(w, 4, cell[1], str_flow_edit_fmt_count, (int)info->value);
}

/* MainFlow: event list, top left, 15 rows. SubFlow: selected event, bottom right, its child. */
/* 0x0011d940 */
static void flow_edit_windows_open(int *w)
{
    int x, y;
    int win;

    gw_get_cell_size(&x, &y);
    w[4] = gw_window_open(str_flow_edit_main_flow, 95,
                        (int)(screen_layout.left + 4.0f),
                        (int)(screen_layout.top + 23.0f),
                        x * 33, y * 15, 0);
    gw_window_set_handlers(w[4], 0, flow_edit_list_draw, 0);
    win = w[4];
    ((int *)win)[27] = (int)w;
    w[6] = 15;
    w[5] = gw_window_open(str_flow_edit_sub_flow, 95,
                        (int)(screen_layout.right - (float)(x * 31) - 4.0f),
                        (int)(screen_layout.bottom - (float)(y * 13) - 4.0f),
                        x * 31, y * 13, win);
    gw_window_set_owner(w[5], 0);
    gw_window_set_handlers(w[5], 0, 0, 0);
}

/*
 * Flow time, then per visible event: index, time, type, enemy formation and name or function name.
 * Next event marked; selected row coloured by mode.
 */
/* 0x0011da90 */
static void flow_edit_list_draw(int *w)
{
    int dim[4];
    int *ed;
    int *data;
    FlowEventNodeStateView *node;
    FlowEnemyEvent *rec;
    char **ent;
    int *fnode;
    char ***fe;
    int lines, rows, top, i, idx, y;

    ed = (int *)w[27];
    gw_window_get_size(w, &dim[0], &dim[1]);
    gw_get_cell_size(&dim[2], &dim[3]);
    lines = dim[1] / dim[3];
    gw_win_printf(w, 4, 0, str_flow_edit_game_count, (int)game_flow_time_get());
    lines = lines - 1;
    y = dim[3];
    ed[6] = lines;
    data = game_flow_get_data();
    rows = lines;
    i = data[16] - ed[0];
    if (i < rows)
        rows = i;
    node = (FlowEventNodeStateView *)data[15];
    if (rows == 0)
        return;
    if (node == 0)
        return;
    top = ed[0];
    if (top != 0) {
        idx = top - 1;
        do {
            node = node->next;
            idx--;
        } while (idx != -1);
    }
    if (rows > 0) {
        idx = top;
        do {
            if (idx == data[1]) {
                gw_window_set_draw_color(w, ARGB(0x80, 0x80, 0x80, 0x80));
                gw_win_printf(w, 4, y, str_flow_edit_cursor);
            }
            if (idx == ed[1]) {
                switch ((unsigned int)ed[3]) {
                case 0:
                    gw_window_set_draw_color(w, ARGB(0x80, 0, 0x80, 0));
                    break;
                case 1:
                    gw_window_set_draw_color(w, ARGB(0x80, 0, 0, 0x80));
                    break;
                case 2:
                    break;
                case 3:
                    gw_window_set_draw_color(w, ARGB(0x80, 0, 0x80, 0x80));
                    break;
                case 4:
                    break;
                }
            } else {
                gw_window_set_draw_color(w, ARGB(0x80, 0x80, 0x80, 0x80));
            }
            gw_win_printf(w, 4, y, str_flow_edit_row, idx, (int)node->v, flow_event_type_names[node->state]);
            switch (node->state) {
            case 1:
                rec = flow_enemy_event_list_nth(data, node->arg);
                if (rec != 0) {
                    ent = flow_enemy_tbl_get(data, rec->type);
                    if (ent != 0) {
                        if (ent[0] != 0)
                            gw_win_printf(w, dim[2] * 20 + 4, y, str_flow_edit_enemy_row, rec->formation, ent[0]);
                        else
                            gw_win_printf(w, dim[2] * 20 + 4, y, str_flow_edit_index, rec->formation);
                    }
                }
                break;
            case 2:
                fnode = flow_func_event_list_nth(data, node->arg);
                if (fnode != 0) {
                    fe = flow_func_tbl_get(data, fnode[0]);
                    if (fe != 0) {
                        if (fe[0] != 0)
                            gw_win_printf(w, dim[2] * 20 + 4, y, str_flow_edit_fmt_s, fe[0]);
                    }
                }
                break;
            }
            node = node->next;
            idx++;
            y += dim[3];
            rows--;
        } while (rows > 0);
    }
    gw_window_set_draw_color(w, ARGB(0x80, 0x80, 0x80, 0x80));
}
