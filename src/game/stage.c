/*
 * Stage: current stage number, phase and table; the "Stage" task loads a stage from its
 * definition, fades it in, runs it until an exit flag (result, continue, quit) and tears it down.
 * Also the "Stage" debug menu and the "LogIO" typed-text task.
 */

#include "types_game.h"
#include "task.h"
#include "debug.h"
#include "libc.h"
#include "gw_varedit.h"
#include "stage.h"
#include "boss_start.h"
#include "config.h"
#include "console.h"
#include "console_com.h"
#include "continue.h"
#include "debugmenu.h"
#include "enemy_status.h"
#include "fade.h"
#include "fog.h"
#include "game.h"
#include "game_flow_editor.h"
#include "heap.h"
#include "lockon.h"
#include "model.h"
#include "node_draw_state.h"
#include "pad.h"
#include "player.h"
#include "player_status.h"
#include "replay.h"
#include "result.h"
#include "sequence.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "system.h"
#include "texforce.h"
#include "screen.h"

typedef struct StageWork {
    void *model;                /* 0x00 */
    void *timed_model;               /* 0x04 */
    int step;                   /* 0x08: load step (stage_task_load) / run step (stage_task_exit) */
    int child;                  /* 0x0c: task work from def->create */
    void *child2;              /* 0x10: task work from def->create2 */
    void *flow;               /* 0x14: from GameFlowInit */
    void *player;               /* 0x18: from player_init */
    void *lockon;               /* 0x1c: "LockonTarget" task work */
    void *lock_mgr;               /* 0x20: from LockonInit */
    void *tfctrl;              /* 0x24: task work from tfctrl_create */
    void *buf;                  /* 0x28: "StageWork" buffer */
    StageDef *def;              /* 0x2c */
    int frames;                 /* 0x30 */
} StageWork;
/* 52 bytes */

/* Ends p's task, if any, and clears p. */
#define TASK_KILL(p) do { if (p) TASK_OF((void *)(p))->flags |= 1; (p) = 0; } while (0)

typedef struct LogIoArg {
    char *text;
    int font;
    int t_in;
    int t_hold;
    int t_out;
} LogIoArg;

typedef struct Screen {
    char pad_00[8];
    float cx;                   /* 0x08 */
    float cy;                   /* 0x0c */
} Screen;

/* stage number, followed by two more words */
extern int stage_num;

extern int stage_varedit;

extern struct GwVarNode *gw_varedit_root;
extern int stage_vardef;
extern int stage_vardefs;
extern char *stage_names[];
extern void soft_reset(const char *name);
extern int stage_next;
extern int stage_phase;
extern int stage_modules[];
extern StageInfo stage_table[];
extern unsigned int game_flags;
extern TaskDesc lockon_target_desc;

extern unsigned int system_flags;
extern unsigned int stage_exit_flags;

extern Pad *pad_main;

extern GameState game_state;
extern char *press_start_texts[];

extern void font_set(int a);
/* "Stage" */
extern TaskDesc stage_task_desc;

extern int *stage_work;
extern Screen screen_cfg;
/* "LogIO" */
extern TaskDesc logio_desc;

/* Stage n with replay recording (replay mode 1), via sequence 9. */
/* 0x0016bfa8 */
void stage_menu_select(void *menu, void *item, int n)
{
    stage_next_set(n);
    replay_set_mode(1);
    sound_sequence_set_next(9);
}

/* One item per stage (11); "Stage" soft reset; stage variables. */
/* 0x0016bfd0 */
void stage_debug_menu_init(void)
{
    int *sel = &stage_num;
    char **name;
    int i;
    int node;

    soft_reset("Stage");
    stage_num = 0;
    sel[1] = 0;
    sel[2] = 0;
    node = gw_varedit_add(&gw_varedit_root, 0, &stage_vardef);
    stage_varedit = node;
    gw_varedit_add_table(&gw_varedit_root, node, &stage_vardefs);
    menu_item_add(0, "Stage", 0, 0, 1);
    name = stage_names;
    for (i = 0; i < 11; i++) {
        menu_item_add("Stage", *name, stage_menu_select, i, 0);
        name++;
    }
}

/* Stage the next stage sequence starts. */
/* 0x0016c0c8 */
void stage_next_set(int stage)
{
    stage_next = stage;
}

/* 0x0016c0d8 */
int stage_get_next(void)
{
    return stage_next;
}

/* 0x0016c0e8 */
void stage_num_set(int n)
{
    DEBUG_PRINTF("StageNumSet %d\n", n);
    stage_num = n;
}

/* 0x0016c120 */
int stage_get_num(void)
{
    return stage_num;
}

/* 0x0016c130 */
void stage_phase_set(int phase)
{
    stage_phase = phase;
}

/* 0x0016c140 */
int stage_get_phase(void)
{
    return stage_phase;
}

/* "01", ... */
/* 0x0016c150 */
char *stage_get_name(void)
{
    return stage_names[stage_num];
}

/* 0x0016c170 */
int stage_get_module(void)
{
    return stage_modules[stage_num];
}

/* Also requests its tempo from the beat clock. */
/* 0x0016c190 */
void stage_set(int n)
{
    StageInfo *info = &stage_table[n];

    stage_num_set(n);
    beat_clock_request_tempo(info->tempo);
}

/* 0x0016c1d0 */
StageInfo *stage_info(int n)
{
    return &stage_table[n];
}

/* 0x0016c1e8 */
int stage_get_analyze_total(int n)
{
    StageInfo *info = &stage_table[n];

    return info->analyze_total;
}

/* Console colour, also the map colour. */
/* 0x0016c208 */
int stage_get_map_col(int n)
{
    StageInfo *info = &stage_table[n];

    return info->console_col;
}

/* Music number. */
/* 0x0016c228 */
int stage_get_setup_arg(int n)
{
    StageInfo *info = &stage_table[n];

    return info->music_no;
}

/*
 * Clears exit flags, resets status; game flag 0x40 for game type 4, and type 5 with extra type
 * 0, 1 or 3.
 */
/* steer(cast) */
/* 0x0016c248 */
void stage_init_print(void)
{
    int on = 0;

    DEBUG_PRINT("-----[ StageInit ]-----\n");
    stage_exit_flags = 0;
    player_status_init_stage();
    EnemyStatusInitStage();
    player_swap_fullrange(1);
    switch (game_type_get()) {
    case 0:
    case 1:
    case 2:
    case 3:
        break;
    case 4:
        on = 1;
        break;
    case 5:
        switch (((unsigned int (*)(void))ConfigGetExtraType)()) {
        case 0:
        case 1:
            on = 1;
            break;
        case 2:
            break;
        case 3:
            on = 1;
            break;
        }
        break;
    case 6:
        break;
    }
    if (on)
        game_flags |= 0x40;
    else
        game_flags &= ~0x40;
}

/* 0x0016c320 */
void stage_exit_print(void)
{
    DEBUG_PRINT("-----[ StageExit ]-----\n");
}

/* 0x0016c330 */
void stage_formation_hook(void)
{
}

/* 0x0016c338 */
void *lockon_target_create(void *parent)
{
    return task_create_inherit(parent, &lockon_target_desc, 8);
}

/*
 * Ends the replay, silences SE; with a stage loaded: its exit hook, frees its buffer, ends what
 * stage_task_load created.
 */
/* 0x0016c360 */
void stage_unload(StageWork *w)
{
    StageDef *def = w->def;

    ReplayEnd();
    se_all_sound_off();
    if (def) {
        system_flags &= ~SYS_PLAY_NOW;
        if (def->exit)
            def->exit();
        heap_free(w->buf, 0);
        w->buf = 0;
        fog_set_enable(0);
        TASK_KILL(w->tfctrl);
        texforce_disable();
        TASK_KILL(w->child2);
        lockon_clear_interval_table();
        LockonExit(w->lock_mgr);
        w->lock_mgr = 0;
        TASK_KILL(w->lockon);
        game_flow_regist_release(w->flow);
        GameFlowExit(w->flow);
        w->flow = 0;
        player_exit(w->player);
        w->player = 0;
        if (w->child) {
            TASK_OF((void *)w->child)->flags |= TASK_F_END;
            w->child = 0;
        }
        model_unload_timed(w->timed_model);
        model_pnb_unload(w->model);
        do {
            w->timed_model = 0;
            w->def = 0;
            w->model = 0;
        } while (0);
    }
}

/* No stage yet. */
/* 0x0016c4a0 */
void stage_task_init(StageWork *w)
{
    stage_work = w;
    w->def = 0;
    boss_start_stage_init();
    w->step = 0;
}

/* 0x0016c4d8 */
void stage_task_release(void *w)
{
    stage_unload(w);
    boss_start_stage_release();
    ConfigStageRelease();
    stage_work = 0;
}

/* Idle: once stage_start gives a definition, blacks out and starts stage_task_load. */
/* 0x0016c510 */
void stage_task_exec(StageWork *w)
{
    if (w->def) {
        w->step = 0;
        fade_start(0, 1, 0);
        TASK_OF(w)->exec = stage_task_load;
    }
}

/*
 * Loading, a step a frame: buffer, models; start, task, player, flow, lock-on; second task, texture
 * force, start hook; after the music, fade-in, replay, flow; after the fade, input -> run.
 */
/* 0x0016c558 */
void stage_task_load(StageWork *w)
{
    void *timed_model;
    StageDef *def;
    unsigned int step;

    w->frames = 0;
    step = w->step;
    def = w->def;
    switch (step) {
    case 0:
        if (def->work_size)
            w->buf = heap_block_tag(game_calloc(def->work_size), "StageWork", -1);
        if (def->tex_file)
            timed_model = model_load_timed(def->tex_file, def->tex_vram);
        else
            timed_model = 0;
        w->model = def->model ? model_pnb_load(def->model) : 0;
        do {
            w->timed_model = timed_model;
        } while (0);
        w->step = 1;
        break;
    case 1:
        stage_init_print();
        if (def->create)
            w->child = def->create(w);
        fog_set_enable(1);
        w->player = player_init(w);
        w->flow = GameFlowInit(w, def->unk_0c, def->enemy_table, def->func_table);
        w->lockon = lockon_target_create(w);
        w->lock_mgr = LockonInit(w);
        ConfigStageLoad();
        w->step = 2;
        break;
    case 2:
        if (def->create2)
            w->child2 = def->create2(w);
        w->tfctrl = tfctrl_create(w);
        game_flow_regist_init(w->flow);
        GameFlowStop();
        if (!replay_is_playing())
            system_flags |= SYS_PLAY_NOW;
        if (def->start)
            def->start();
        w->step = 3;
        break;
    case 3:
        if (seqload_idle()) {
            fade_start_out(60);
            ReplayStart();
            GameFlowStart();
            w->step = 4;
        }
        break;
    case 4:
        if (fade_is_idle() == 1) {
            system_pause_enable();
            pad_input_enable(0);
            TASK_OF(w)->exec = stage_task_run;
            system_flags |= SYS_STABILIZER;
            w->step = 0;
        }
        break;
    }
}

/*
 * Running. Game type 0 (attract): counts frames, ends on start (exit 0x10) or replay end (1).
 * Exit 0x11 -> stage_task_exit; 2 opens the result, 4 the continue screen unless 0x20.
 */
/* 0x0016c790 */
void stage_task_run(StageWork *w)
{
    unsigned int flags;

    if (game_type_get() == 0) {
        w->frames++;
        if (pad_main->raw.trig & PAD_START)
            stage_exit_flags |= 0x10;
        if (!replay_is_playing())
            stage_exit_flags |= 1;
    }
    flags = stage_exit_flags;
    if (flags & 0x11) {
        TASK_OF(w)->exec = stage_task_exit;
        w->step = 0;
    } else {
        if (flags & 2) {
            result_create();
            flags = stage_exit_flags;
        }
        if (flags & 4) {
            if (flags & 0x20)
                return;
            continue_task_create();
        }
    }
}

/*
 * 60-frame fade-out, tear-down. Attract: exits (sequence 2 if start pressed). Play: exit 0x10
 * reloads at once, else exits (2 after the continue screen, else 0).
 */
/* 0x0016c860 */
void stage_task_exit(StageWork *w)
{
    w->frames = 0;
    system_pause_disable();
    if (game_type_get() == 0) {
        switch (w->step) {
        case 0:
            fade_start(0, 1, 60);
            w->step++;
            break;
        case 1:
            if (fade_is_idle() == 1)
                w->step = 2;
            break;
        case 2:
            stage_unload(w);
            if (stage_exit_flags & 0x10) {
                GameExit(0);
                sound_sequence_set_next(2);
            } else {
                GameExit(0);
            }
            break;
        }
    } else {
        switch (w->step) {
        case 0:
            if (stage_exit_flags & 0x10) {
                w->step = 2;
            } else {
                fade_start(0, 1, 60);
                w->step++;
            }
            break;
        case 1:
            if (fade_is_idle() == 1)
                w->step = 2;
            break;
        case 2:
            stage_unload(w);
            if (stage_exit_flags & 0x10) {
                TASK_OF(w)->exec = stage_task_exec;
                w->step = 0;
            } else {
                if (stage_exit_flags & 4) {
                    GameExit(2);
                    w->step++;
                } else {
                    GameExit(0);
                    w->step++;
                }
            }
            break;
        }
    }
}

/* Attract demo: blinks "press start" at the top centre. */
/* 0x0016ca20 */
void stage_task_disp(StageWork *w)
{
    char *text;
    int font;
    int len;
    float x;
    float y;

    if (game_type_get() == 0 && ((w->frames >> 5) & 1)) {
        text = press_start_texts[game_state.lang];
        font = game_state.menu_color;
        len = strlen(text);
        x = screen_cx - len * 12 * 0.5f;
        y = screen_top + 42.0f - 20.0f;
        draw_set_blend(4, 5);
        font_set(2);
        font_print(x, y, 3.3333333f, font, text);
    }
}

/* 0x0016cb38 */
void *stage_task_create(void *parent)
{
    return task_create(parent, &stage_task_desc, 52);
}

/* 0x0016cb60 */
int stage_get_work(void)
{
    return stage_work;
}

/* 0x0016cb70 */
int stage_get_varedit(void)
{
    return stage_varedit;
}

/* Getters below: parts of the loaded stage, 0 without a stage task. */
/* 0x0016cb80 */
int stage_get_player(void)
{
    if (stage_work)
        return stage_work[6];
    return 0;
}

/* 0x0016cba0 */
int stage_get_game_flow(void)
{
    if (stage_work)
        return stage_work[5];
    return 0;
}

/* 0x0016cbc0 */
int stage_get_lockon(void)
{
    if (stage_work)
        return stage_work[7];
    return 0;
}

/* 0x0016cbe0 */
int stage_get_child(void)
{
    if (stage_work)
        return stage_work[3];
    return 0;
}

/* 0x0016cc00 */
void stage_end_child(void)
{
    StageWork *w = stage_work;

    if (w) {
        do {
            if (w->child)
                TASK_OF(w->child)->flags |= TASK_F_END;
            w->child = 0;
        } while (0);
    }
}

/* 0x0016cc38 */
int stage_get_child2(void)
{
    if (stage_work)
        return stage_work[4];
    return 0;
}

/* 0x0016cc58 */
int stage_get_tfctrl(void)
{
    if (stage_work)
        return stage_work[9];
    return 0;
}

/* 0x0016cc78 */
int stage_get_buffer(void)
{
    if (stage_work)
        return stage_work[10];
    return 0;
}

/* Unless one runs; applies the stage's console and laser colours. 0 without a task. */
/* 0x0016cc98 */
StageWork *stage_start(StageDef *def)
{
    StageWork *w = stage_work;
    StageInfo *info;

    if (w) {
        if (TASK_OF(w)->flags & TASK_F_END)
            return w;
        info = &stage_table[stage_get_num()];
        if (w->def == 0)
            w->def = def;
        console_set_stage_colors(info->frame_col, info->console_col);
        console_set_color(0, info->console_col);
        lockon_stage_setup(info->laser_col0, info->laser_col1, info->laser_col2);
    }
    return w;
}

/* No running "Stage" task with a stage. */
/* 0x0016cd38 */
int stage_is_not_ready(void)
{
    StageWork *w = stage_work;

    if (w != 0 && !(TASK_OF(w)->flags & TASK_F_END) && w->def != 0)
        return 0;
    return 1;
}

/* Text, font, timings; display list 13. */
/* 0x0016cd68 */
void logio_task_init(LogIo *w, LogIoArg *prm)
{
    w->font = prm->font;
    w->text = prm->text;
    w->t_in = prm->t_in;
    w->t_hold = prm->t_hold;
    w->t_out = prm->t_out;
    w->shown = 0;
    w->len = strlen(w->text);
    w->blink = 0;
    task_set_dlist(w, 13);
}

/* 0x0016cdd0 */
void logio_task_release(void)
{
}

/* Types over t_in frames, holds t_hold, erases over t_out, ends. */
/* 0x0016cdd8 */
void logio_task_exec(LogIo *w)
{
    w->blink++;
    switch (w->state) {
    case 0:
        if (++w->count < w->t_in) {
            w->blink = 0;
            w->shown = w->len * ((float)w->count / (float)w->t_in);
        } else {
            w->state = 1;
            w->shown = w->len;
            w->count = 0;
        }
        break;
    case 1:
        if (++w->count >= w->t_hold) {
            w->count = 0;
            w->state = 2;
        }
        break;
    case 2:
        if (++w->count < w->t_out) {
            w->blink = 0;
            w->shown = w->len * (1.0f - (float)w->count / (float)w->t_out);
        } else {
            do {
                if (w)
                    TASK_OF(w)->flags |= TASK_F_END;
            } while (0);
        }
        break;
    }
}

/* Centred, 36 pixels apart; the cursor blinks once the text stops changing. */
/* 0x0016cf18 */
void logio_task_disp(LogIo *w)
{
    float x;
    float y;
    void *font;
    char *p;
    int i;
    char ch;

    draw_set_blend(4, 5);
    font_set(3);
    y = screen_cfg.cy - 21.0f;
    x = screen_cfg.cx - w->len * 36.0f * 0.5f;
    font = font_begin_current(w->font);
    i = w->shown;
    p = w->text;
    if (i > 0) do {
        ch = *p++;
        font = font_put_char_scaled(font, x, y, 3.3333333f, ch, 1.0f);
        x += 36.0f;
    } while (--i);
    if (!(w->blink & 0x10))
        font = font_put_char_scaled(font, x, y, 3.3333333f, 127, 1.0f);
    font_end_current(font);
}

/* 0x0016d040 */
void *logio_create(void *parent, void *prm)
{
    return task_create_arg(parent, &logio_desc, 40, prm);
}
