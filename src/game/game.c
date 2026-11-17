/*
 * Game task, one per stage play: starts the status managers, formations, stage, console and stage
 * sound; runs the game, flow (drives the event list) and pass time counters with speeds and
 * stop/start nesting. Also game exit, demo playback, debug pause keys, the GameInfo window.
 */

#include "mathdef.h"
#include "debug.h"
#include "types_common.h"
#include "color.h"
#include "types_game.h"
#include "libc.h"
#include "gw_draw.h"
#include "gw_system.h"
#include "gw_varedit.h"
#include "gw_window.h"
#include "game.h"
#include "console.h"
#include "debugmenu.h"
#include "enemy_formation.h"
#include "enemy_status.h"
#include "fog.h"
#include "game_flow_editor.h"
#include "game_flow_enemy.h"
#include "map_manager.h"
#include "pad.h"
#include "player_status.h"
#include "replay.h"
#include "sequence.h"
#include "sound_main.h"
#include "stage.h"
#include "system.h"

typedef struct SystemTime {
    char pad_00[8];
    int play_num;
    char pad_0c[4];
    int cur_play_time;
    int cur_play_num;
} SystemTime;

extern char str_game[];
extern char gw_varedit_root[];
extern GwVarDef game_vardef[];
extern GwVarDef game_vardefs[];
extern GwVarDef game_type_vardefs[];
extern GwVarDef status_vardef[];
extern GwVarDef status_vardefs[];
extern int game_type;
extern void soft_reset(const char *);
extern char test_game_vardef[];
extern int test_game_vars_group;
extern float count_spd;
extern int flow_count_link;
extern float flow_count_rate;
extern float flow_count_spd;
extern float pass_count_spd;
extern unsigned int game_flags;
extern unsigned int system_flags;
extern int game_demo_obj;
extern int game_state_req;
extern GameWork *game_work;
extern char str_game_task_exists[];
extern char *game_exit_type_names[];
extern char str_game_exit[];
extern float game_times[];
extern int count_time;
extern int flow_count_time;
extern int pass_count_time;
extern SystemTime system_time;
extern char str_game_start[];
extern char str_game_mode_play[];
extern char str_game_play_num[];
extern char str_game_mode_replay[];
extern char str_game_start_rule[];
extern int play_time_frames;
extern char str_game_end[];
extern char str_game_play_time[];
extern char str_game_end_rule[];
extern int *pad_main;
extern struct TaskDesc game_task_desc;
extern float game_prev_time;
extern float game_total_time;
extern char str_game_count_stop_error[];
extern float game_flow_time;
extern char str_game_flow_count_stop_error[];
extern float game_pass_time;
extern char str_game_pass_count_stop_error[];
extern int game_info_flag_labels[];
extern char str_game_info_label[];
extern int game_info_window;
extern char str_game_info[];
extern ScreenView screen_layout;

extern char str_game_info_replay[];
extern char str_game_info_time[];
extern char str_game_info_game_time[];
extern char str_game_info_flow_time[];
extern char str_game_info_pass_time[];
extern char str_game_info_phase[];
extern char str_game_info_status[];

/* Frames (60 per second) -> minutes, seconds, centiseconds, frames. */
#define GI_SPLIT_TIME(t, min, sec, cs, fr) \
    do { \
        (min) = (int)((t) / 3600.0f); \
        (sec) = (int)((t) / FPS) % 60; \
        (cs) = (int)((t) * 100.0f / FPS) % 100; \
        (fr) = (int)(t); \
    } while (0)

/* Info window line for game_times[k]. */
#define GI_TIME_LINE(fmt, k) \
    frames = game_times[k]; \
    GI_SPLIT_TIME(frames, min, sec, cs, fr); \
    gw_win_printf(w, 4, ofs, fmt, min, sec, cs, fr)

/* Clears game_flags and game_work; GameInfo menu item; debug variables. */
/* 0x0011a150 */
void game_core_init(void)
{
    struct GwVarNode *game_node;
    struct GwVarNode *status_node;

    soft_reset(str_game);
    game_flags = 0;
    game_work = 0;
    game_info_menu_register();
    game_node = gw_varedit_add(gw_varedit_root, 0, game_vardef);
    gw_varedit_add_table(gw_varedit_root, game_node, game_vardefs);
    gw_varedit_add_table(gw_varedit_root, game_node, game_type_vardefs);
    status_node = gw_varedit_add(gw_varedit_root, game_node, status_vardef);
    gw_varedit_add_table(gw_varedit_root, status_node, status_vardefs);
    player_status_vars_register();
    enemy_status_vars_register();
    game_type = 0;
}

/* 0x0011a220 */
void game_type_set(int type)
{
    game_type = type;
}

/* 0x0011a230 */
int game_type_get(void)
{
    return game_type;
}

/* Test-game variables. */
/* steer(cast) */
/* 0x0011a240 */
void game_varedit_register(void)
{
    test_game_vars_group = ((int (*)(void *, int, void *))gw_varedit_add)(gw_varedit_root, 0, test_game_vardef);
}

/* 0x0011a278 */
int game_varedit_get(void)
{
    return test_game_vars_group;
}

/* Normal speed; flow follows game speed directly (flow_count_link 0). */
/* 0x0011a288 */
void game_count_spd_default(void)
{
    count_spd = 1.0f;
    flow_count_link = 0;
    flow_count_rate = 0;
    flow_count_spd = 1.0f;
}

/* flow_count_link 0: same; 1: game speed's deviation from 1 * flow_count_rate; 2: independent. */
/* 0x0011a2b8 */
void game_count_spd_changed(void)
{
    switch (flow_count_link) {
    case 0:
        flow_count_spd = count_spd;
        break;
    case 1:
        flow_count_spd = (count_spd - 1.0f) * flow_count_rate + 1.0f;
        break;
    case 2:
        break;
    }
}

/* As game_count_spd_changed. */
/* 0x0011a320 */
void game_flow_count_link_changed(void)
{
    switch (flow_count_link) {
    case 0:
        flow_count_spd = count_spd;
        break;
    case 1:
        flow_count_spd = (count_spd - 1.0f) * flow_count_rate + 1.0f;
        break;
    case 2:
        break;
    }
}

/* Only when linked by rate. */
/* 0x0011a388 */
void game_flow_count_rate_changed(void)
{
    if (flow_count_link == 1)
        flow_count_spd = (count_spd - 1.0f) * flow_count_rate + 1.0f;
}

/* Link worked backwards: game speed from flow speed; untouched when the rate is zero. */
/* 0x0011a3d0 */
void game_flow_count_spd_changed(void)
{
    float rate;

    switch (flow_count_link) {
    case 0:
        count_spd = flow_count_spd;
        break;
    case 1:
        rate = flow_count_rate;
        if (fabsf(rate) < FLT_EPSILON)
            break;
        count_spd = (flow_count_spd - 1.0f) / rate + 1.0f;
        break;
    case 2:
        break;
    }
}

/* [0] game, [1] previous game, [2] total, [3] flow, [4] pass time. */
/* 0x0011a458 */
void game_times_clear(float *times)
{
    times[0] = 0;
    times[1] = 0;
    times[2] = 0;
    times[3] = 0;
    times[4] = 0;
}

/* Game, flow, pass times reset next frame; flow and pass speeds normal. */
/* 0x0011a470 */
void game_count_speed_reset(float *times)
{
    game_flags |= 0x38;
    flow_count_spd = 1.0f;
    pass_count_spd = 1.0f;
}

/* 0 when no game runs. */
/* 0x0011a4a0 */
int game_get_sound_task(void)
{
    if (game_work)
        return game_work->sound_task;
    return 0;
}

/* Disables pausing, clears system_flags bit 0. */
/* 0x0011a4c0 */
void game_demo_start(int obj)
{
    game_demo_obj = obj;
    game_state_req = 1;
    game_flags |= 0x10000;
    system_pause_disable();
    system_flags &= ~SYS_PAUSED;
}

/* Pausing allowed again. */
/* 0x0011a518 */
void game_demo_end(int w)
{
    game_flags &= ~0x10000;
    game_demo_obj = 0;
    game_state_req = 0;
    system_pause_enable();
}

/* 0 (and a report) if one is running. */
/* 0x0011a548 */
void *game_task_request(void *parent)
{
    if (game_work) {
        ERROR_PRINTF(str_game_task_exists);
        return 0;
    }
    return game_task_create(parent);
}

/*
 * Once. Exit types: 0 normal, 1 clear (sequence 12), 2 game over (player status restarted,
 * sequence 13), 3 logout (player logged out, sequence 11, black background).
 */
/* 0x0011a588 */
void GameExit(unsigned int code)
{
    GameWork *w = game_work;

    if (!w)
        return;
    if (TASK_OF(w)->flags & TASK_F_END)
        return;
    do {
        TASK_OF(w)->flags |= TASK_F_END;
    } while (0);
    DEBUG_PRINTF(str_game_exit, game_exit_type_names[code]);
    switch (code) {
    case 1:
        sound_sequence_set_next(12);
        return;
    case 0:
        return;
    case 2:
        player_status_restart();
        sound_sequence_set_next(13);
        return;
    case 3:
        player_status_logout();
        sound_sequence_set_next(11);
        BackColReq(COL_BLACK);
        return;
    default:
        return;
    }
}

/* Only if a game runs; returns it or 0. */
/* 0x0011a670 */
void *game_stage_start(int *prm)
{
    void *w = game_work;

    if (w != 0)
        stage_start(*prm);
    return w;
}

/* Freezes enemies and time counters. */
/* 0x0011a6a8 */
void game_debug_pause(GameWork *game)
{
    enemy_status_set_flags(1);
    GameCountStart();
    game->paused = 1;
}

/* 0x0011a6e0 */
void game_debug_resume(GameWork *game)
{
    enemy_status_clear_flags(1);
    GameCountStop();
    game->paused = 0;
}

/*
 * Debug flag 31: pad menu button toggles the debug pause. Key 'D' with a modifier seeks the event
 * list back/forward, without restarts the counters; both clear pass time and all flow enemies.
 */
/* 0x0011a718 */
void game_debug_keys(GameWork *obj)
{
    unsigned char key = kbd_key;

    if (debug_flags_test(31)) {
        if ((*(Pad * *)&pad_main)->menu.trig & PAD_L1) { /* steer(cast) */
            if (obj->paused)
                game_debug_resume(obj);
            else
                game_debug_pause(obj);
        }
    }
    if (key == 'D') {
        if (pad_keyboard_modifiers_match(17))
            game_flow_queue_back();
        else if (pad_keyboard_modifiers_match(68))
            game_flow_queue_forward();
        else
            game_times_clear(game_times);
        game_times[4] = 0;
        flow_enemy_kill_all();
    }
}

/* 0x0011a800 */
void game_state_normal(void)
{
}

/* Leaves demo mode once the demo task is gone or ending. */
/* 0x0011a808 */
void game_demo_check_end(void)
{
    if (game_demo_obj == 0 || (TASK_OF(game_demo_obj)->flags & TASK_F_END)) {
        game_demo_end(0);
        game_demo_obj = 0;
        game_state_req = 0;
    }
}

/* Counts the play unless a replay is playing. */
/* steer(cast) */
/* 0x0011a858 */
void GameStart(GameWork *w)
{
    game_times_clear(game_times);
    count_time = 0;
    flow_count_time = 0;
    pass_count_time = 0;
    w->unk_00 = 0;
    game_work = w;
    DEBUG_PRINT(str_game_start);
    system_time.cur_play_time = 0;
    if (replay_is_playing() == 0) {
        system_time.play_num++;
        system_time.cur_play_num++;
        do {
            DEBUG_PRINT(str_game_mode_play);
            DEBUG_PRINTF(str_game_play_num, system_time.play_num);
        } while (0);
    } else {
        DEBUG_PRINT(str_game_mode_replay);
    }
    DEBUG_PRINT(str_game_start_rule);
    TASK_OF(w)->flags |= TASK_F_RUN_PAUSED;
    *(GState *)&w->state = 0; /* steer(type) */
    w->unk_08 = 0;
    (*(GState *)&game_state_req) = -1; /* steer(cast) */
    w->player_status = player_status_man_create(w);
    w->enemy_status = enemy_status_man_create(w);
    w->formation = enemy_formation_create(w);
    w->stage = stage_task_create(w);
    w->enemy_status2 = enemy_status_man2_create(w);
    w->console = console_task_create(w);
    w->sound_task = ((int (*)(GameWork *, int))sound_stg_create)(w, 0);
    w->paused = 0;
}

/* Logs play_time_frames (60 per second); clears system_flags bit 12. */
/* 0x0011a9a8 */
void GameEnd(GameWork *w)
{
    int frames, mins, secs, centis;

    game_work = 0;
    DEBUG_PRINT(str_game_end);
    if (w->paused)
        do { game_debug_resume(w); } while (0);
    if (w->console)
        TASK_OF(w->console)->flags |= TASK_F_END;
    if (w->sound_task)
        TASK_OF(w->sound_task)->flags |= TASK_F_END;
    do {
        frames = play_time_frames;
        mins = frames / 3600;
        secs = frames / 60 % 60;
        centis = frames * 100 / 60 % 100;
    } while (0);
    DEBUG_PRINTF(str_game_play_time, mins, secs, centis);
    system_flags &= ~SYS_PLAY_NOW;
    DEBUG_PRINT(str_game_end_rule);
}

/*
 * system_flags bit 0/1 skips it, bit 15 runs debug keys. Flow time stops and flow enemies go while
 * the stage child's map_manager_scene_flag is clear. game_flags 0-2 run, 3-5 reset times; 16 demo.
 */
/* 0x0011aab0 */
void game_task_exec(GameWork *w)
{
    float cur;
    int advance;
    int child;
    unsigned int fl;
    float *t;

    if ((system_flags & (SYS_PAUSE_KBD | SYS_PAUSED)) == 0) {
        if (game_state_req != -1) {
            w->state = game_state_req;
            game_state_req = -1;
        }
        t = game_times;
        cur = t[0];
        t[1] = cur;
        fl = game_flags;
        if (fl & 8) {
            t[0] = 0;
            game_flags = fl & ~8;
        } else if (fl & 1) {
            t[0] = cur + count_spd;
            t[2] = t[2] + count_spd;
        }
        fl = game_flags;
        if (fl & 0x20) {
            t[4] = 0;
            game_flags = fl & ~0x20;
        } else if (fl & 4) {
            t[4] = t[4] + pass_count_spd;
        }
        fl = game_flags;
        if (fl & 0x10) {
            t[3] = 0;
            game_flags = fl & ~0x10;
        } else {
            advance = 1;
            child = stage_get_child();
            if (child != 0) {
                if (map_manager_scene_flag(child) == 0) {
                    flow_enemy_kill_all();
                    advance = 0;
                }
            }
            if (advance != 0 && (game_flags & 2))
                t[3] = t[3] + flow_count_spd;
        }
        replay_frame(pad_main + 2);
        switch (w->state) {
        case 0:
            ((void (*)(GameWork *))game_state_normal)(w); /* steer(cast) */
            break;
        case 1:
            ((void (*)(GameWork *))game_demo_check_end)(w); /* steer(cast) */
            break;
        }
    }
    if (system_flags & SYS_DEBUG)
        game_debug_keys(w);
}

/* 0x0011aca0 */
void game_task_disp(void)
{
}

/* 0x0011aca8 */
void *game_task_create(void *parent)
{
    return task_create(parent, &game_task_desc, sizeof(GameWork));
}

/* Flow time too while flow_count_link is 0. */
/* 0x0011acd0 */
void game_time_set(float v)
{
    game_times[0] = v;
    if (flow_count_link == 0)
        game_times[3] = v;
}

/* 0x0011acf8 */
float game_time_get(void)
{
    return game_times[0];
}

/* 0x0011ad08 */
float game_prev_time_get(void)
{
    return game_prev_time;
}

/* Never reset by reset requests. */
/* 0x0011ad18 */
float game_total_time_get(void)
{
    return game_total_time;
}

/*
 * Despite the name, stops the game, flow and pass times. Nests: each needs its own GameCountStop,
 * which lets them run.
 */
/* 0x0011ad28 */
void GameCountStart(void)
{
    count_time++;
    game_flags &= ~1;
    GameFlowCountStart();
    GamePassCountStart();
}

/* Runs again when the last start is undone; an unmatched call is reported. Flow and pass too. */
/* 0x0011ad68 */
void GameCountStop(void)
{
    if (count_time <= 0) {
        ERROR_PRINTF(str_game_count_stop_error);
    } else {
        if (--count_time == 0)
            game_flags |= 1;
    }
    GameFlowCountStop();
    GamePassCountStop();
}

/* Next frame. */
/* 0x0011adc0 */
void game_flow_time_reset(void)
{
    game_flags |= 0x10;
}

/* Game time too while linked directly. */
/* 0x0011add8 */
void game_flow_time_set(float v)
{
    game_times[3] = v;
    if (flow_count_link == 0)
        game_times[0] = v;
}

/* 0x0011ae00 */
float game_flow_time_get(void)
{
    return game_flow_time;
}

/* Stops the flow time; nests like GameCountStart. */
/* 0x0011ae10 */
void GameFlowCountStart(void)
{
    flow_count_time++;
    game_flags &= ~2;
}

/* 0x0011ae40 */
void GameFlowCountStop(void)
{
    if (flow_count_time <= 0) {
        ERROR_PRINTF(str_game_flow_count_stop_error);
        return;
    }
    if (--flow_count_time == 0)
        game_flags |= 2;
}

/* Next frame. */
/* 0x0011ae80 */
void game_pass_time_reset(void)
{
    game_flags |= 0x20;
}

/* 0x0011ae98 */
void game_pass_time_set(float v)
{
    game_pass_time = v;
}

/* 0x0011aea8 */
float game_pass_time_get(void)
{
    return game_pass_time;
}

/* 0x0011aeb8 */
void game_pass_count_spd_set(float v)
{
    pass_count_spd = v;
}

/* 0x0011aec8 */
float game_pass_count_spd_get(void)
{
    return pass_count_spd;
}

/* Stops the pass time; nests like GameCountStart. */
/* 0x0011aed8 */
void GamePassCountStart(void)
{
    pass_count_time++;
    game_flags &= ~4;
}

/* 0x0011af08 */
void GamePassCountStop(void)
{
    if (pass_count_time <= 0) {
        ERROR_PRINTF(str_game_pass_count_stop_error);
        return;
    }
    if (--pass_count_time == 0)
        game_flags |= 4;
}

/* 0x0011af48 */
void game_times_reset(void)
{
    game_count_speed_reset(game_times);
}

/* Labels: game_info_flag_labels; clear bits leave their row empty. Returns y below the last row. */
/* 0x0011af58 */
int game_info_draw_flags(void *w, int x, int y, int dy, int mask)
{
    int i;
    int *ent = game_info_flag_labels;

    for (i = 4; i >= 0; i--) {
        if (mask & ent[1])
            gw_win_printf(w, x, y, str_game_info_label, ent[0]);
        y += dy;
        ent += 2;
    }
    return y;
}

/* Replay frame; total, game, flow, pass times; stage phase; game_flags with set bits named. */
/* 0x0011b010 */
void game_info_window_draw(void *w)
{
    int x, y;
    int ofs;
    float frames;
    int min, sec, cs, fr;

    if (!game_work)
        return;
    if (TASK_OF(game_work)->flags & TASK_F_END)
        return;
    gw_get_cell_size(&x, &y);
    gw_win_printf(w, 4, 0, str_game_info_replay, replay_get_frame());
    ofs = y;
    GI_TIME_LINE(str_game_info_time, 2);
    ofs += y;
    GI_TIME_LINE(str_game_info_game_time, 0);
    ofs += y;
    GI_TIME_LINE(str_game_info_flow_time, 3);
    ofs += y;
    GI_TIME_LINE(str_game_info_pass_time, 4);
    ofs += y;
    gw_win_printf(w, 4, ofs, str_game_info_phase, stage_get_phase());
    ofs += y;
    gw_win_printf(w, 4, ofs, str_game_info_status, game_flags);
    game_info_draw_flags(w, 4, ofs + y, y, game_flags);
}

/* Opens top right, 26 x 12 cells; menu tick follows the window. */
/* 0x0011b378 */
void game_info_window_toggle(void)
{
    int cw, ch;
    int win;

    if (game_info_window != 0) {
        gw_window_close(game_info_window);
        game_info_window = 0;
        menu_item_set_value(info_menu_name, str_game_info, 0);
        return;
    }
    gw_get_cell_size(&cw, &ch);
    win = gw_window_open(str_game_info, 95,
                       (int)(screen_layout.right - (float)(cw * 26) - 4.0f),
                       (int)(screen_layout.top + 32.0f),
                       cw * 26, ch * 12, 0);
    game_info_window = win;
    if (win != 0) {
        gw_window_set_handlers(win, 0, game_info_window_draw, 0);
        menu_item_set_value(info_menu_name, str_game_info, 1);
    }
}

/* 0x0011b490 */
void game_info_menu_register(void)
{
    menu_item_add(info_menu_name, str_game_info, game_info_window_toggle, 0, 0);
}
