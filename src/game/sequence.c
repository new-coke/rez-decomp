/*
 * Game flow sequencer: "Sequence" runs the scripts of sound_sequence_table (modules in turn, game
 * type and status resets between them). Also the "Sequence" and "Module" jump menus.
 */

#include "types_game.h"
#include "gw_draw.h"
#include "gw_system.h"
#include "gw_window.h"
#include "sequence.h"
#include "camera.h"
#include "config.h"
#include "debugmenu.h"
#include "enemy_status.h"
#include "game.h"
#include "heap.h"
#include "module.h"
#include "pad.h"
#include "player_status.h"
#include "render.h"
#include "screen.h"
#include "system.h"
#include "task.h"
#include "texforce.h"

#define C15_FLAGS(p) (*(unsigned *)((char *)(p) - 36))

extern void soft_reset(char *);
extern int system_flags;
extern int game_flags;
extern SoundSequenceEntry sound_sequence_table[];
extern void *system1_work;
extern struct TaskDesc sound_sequence_desc;
extern int *sound_sequence_work;
extern int sound_sequence_info;

extern struct {
    int pad[6];
    float x;
    int pad1c[2];
    float y;
} screen_layout;

extern int sound_sequence_modules[][2];
extern int sound_sequence_module;

static void sound_sequence_create(void);
static void sound_sequence_menu_init(void);

/* 0x001635f8 */
void sound_sequence_start(void)
{
    soft_reset("Sequence");
    sound_sequence_create();
    sound_sequence_set_next(1);
    sound_sequence_menu_init();
}

/* Condition: system flag 0x2000. */
/* 0x00163628 */
static int sound_sequence_cond_warning_skip(void)
{
    int flag = system_flags & SYS_WARNING_PENDING;

    return flag != 0;
}

/* Condition: game flag 0x40, except in game type 5 with config selection 5 or 7 on. */
/* 0x00163640 */
static int sound_sequence_cond_game_flag(void)
{
    int flag;

    if (game_type_get() == 5) {
        if (ConfigGetSelect(5))
            return 0;
        if (ConfigGetSelect(7))
            return 0;
    }
    flag = game_flags & 0x40;
    return flag != 0;
}

/* Condition: game type 3, or 5 with extra type 0. */
/* 0x00163698 */
static int sound_sequence_cond_mode(void)
{
    switch (((unsigned (*)(void))game_type_get)()) { /* steer(cast) */
    case 3:
        return 1;
    case 2:
        return 0;
    case 4:
        return 0;
    case 5:
        if (ConfigGetExtraType() == 0)
            return 1;
    }
    return 0;
}

/* 0x00163708 */
static int sound_sequence_cond_true(void)
{
    return 1;
}

/* Unpauses, frees the module heap, checks the default heap, disables pad input. */
/* 0x00163710 */
static void sound_sequence_module_release(void)
{
    system_pause_disable();
    (*(unsigned *)&system_flags) &= ~SYS_PAUSED; /* steer(cast) */
    module_heap_release();
    heap_check_default();
    pad_input_disable(0);
    (*(unsigned *)&system_flags) |= SYS_NO_SCREEN_SAVER; /* steer(cast) */
    (*(unsigned *)&system_flags) &= ~SYS_STABILIZER; /* steer(cast) */
}

/* Queued sequence becomes current from its first command; ends the old module task. */
/* 0x00163778 */
static void sound_sequence_advance(SoundSequenceWork *w)
{
    if (w->next) {
        w->cur = w->next;
        w->next = 0;
        w->step = 0;
        do {
            if (w->child)
                TASK_OF(w->child)->flags |= TASK_F_END;
        } while (0);
        w->child = 0;
        w->count = w->count + 1;
    }
}

/* Runs while paused (task flag 8). */
/* 0x001637c8 */
static void sound_sequence_init(SoundSequenceWork *w)
{
    sound_sequence_work = w;
    TASK_OF(w)->flags |= TASK_F_RUN_PAUSED;
    w->heap_count = -1;
    w->child = 0;
    w->count = 0;
}

/* 0x001637f0 */
static void sound_sequence_release(void)
{
    sound_sequence_work = 0;
}

/*
 * Commands {op, arg, cond}, skipped when cond fails. Ops: 1 start module, 2 queue sequence, 3 game
 * type, 4/5 reset status (new game / new score), 6 set pending, 255 queue pending (reset to 1).
 */
/* 0x00163800 */
static void sound_sequence_exec(SoundSequenceWork *w)
{
    SoundSequenceCmd *cmd;
    int done;
    int pending;
    int one;

    if (w->child == 0 || (C15_FLAGS(w->child) & 1))
        w->child = 0;
    switch (w->count) {
    case 0:
        sound_sequence_advance(w);
        if (w->child == 0 || (C15_FLAGS(w->child) & 1))
            w->count = w->count + 1;
        return;
    case 1:
    case 2:
        w->count = w->count + 1;
        return;
    case 3:
        break;
    default:
        return;
    }
    for (;;) {
        one = 1;
        cmd = sound_sequence_table[w->cur].cmds;
        cmd += w->step;
        done = 0;
        if (cmd->cond && !cmd->cond()) {
            w->step = w->step + 1;
            continue;
        }
        switch (cmd->op) {
        case 1:
            sound_sequence_module_release();
            done = 1;
            w->heap_count = heap_alloc_count();
            w->child = module_call_entry(w, cmd->arg);
            w->step = w->step + 1;
            break;
        case 2:
            done = 1;
            w->next = cmd->arg;
            break;
        case 3:
            game_type_set(cmd->arg);
            w->step = w->step + 1;
            break;
        case 4:
            player_status_init_game();
            EnemyStatusInitGame();
            w->step = w->step + 1;
            break;
        case 5:
            player_status_init_score();
            EnemyStatusInitGame();
            w->step = w->step + 1;
            break;
        case 6:
            w->pending = cmd->arg;
            w->step = w->step + 1;
            break;
        case 255:
            pending = w->pending;
            done = 1;
            w->pending = one;
            w->next = pending;
            break;
        }
        if (done)
            break;
    }
    w->count = 0;
}

/*
 * Submits the frame (mode per screen effect), updates the camera, draws screen scaling and
 * texture-force passes.
 */
/* 0x00163a38 */
static void sound_sequence_disp(void)
{
    if (screen_get_effect_mode())
        render_submit_frame(1, 4096);
    else
        render_submit_frame(2, -1);
    camera_update();
    screen_apply_scale_frame();
    texforce_draw_screen();
}

/* Sibling of the system task. */
/* 0x00163a88 */
static void sound_sequence_create(void)
{
    task_create_sibling(system1_work, &sound_sequence_desc, 28);
}

/* If the task exists and is not ending. */
/* 0x00163aa0 */
void sound_sequence_set_next(int seq)
{
    int *w = sound_sequence_work;

    if (w != 0 && !(TASK_OF(w)->flags & TASK_F_END))
        w[3] = seq;
}

/* Queued by op 255; only if the task exists and is not ending. */
/* 0x00163ac8 */
static void sound_sequence_set_pending(int seq)
{
    int *w = sound_sequence_work;

    if (w != 0 && !(TASK_OF(w)->flags & TASK_F_END))
        w[4] = seq;
}

/* Current and pending sequence. */
/* 0x00163af0 */
static void sound_sequence_info_draw(void *win)
{
    int cw;
    int ch;
    int *w = sound_sequence_work;

    gw_get_cell_size(&cw, &ch);
    if (w) {
        if (!(TASK_OF(w)->flags & TASK_F_END)) {
            gw_win_printf(win, 4, 2, "Seq  %2d:[%s]", w[2], sound_sequence_table[w[2]].name);
            gw_win_printf(win, 4, ch + 2, "Next %2d:[%s]", w[4], sound_sequence_table[w[4]].name);
        }
    }
}

/* 0x00163ba8 */
static void sound_sequence_info_close(void)
{
    sound_sequence_info = 0;
}

/* 0x00163bb8 */
static void sound_sequence_info_toggle(void)
{
    int cw;
    int ch;
    void *w;

    if (sound_sequence_info) {
        gw_window_close(sound_sequence_info);
    } else {
        gw_get_cell_size(&cw, &ch);
        w = gw_window_open("Sequence", 95, screen_layout.x + 4.0f, screen_layout.y - 16.0f - ch * 3, cw * 31, ch * 3, 0);
        sound_sequence_info = w;
        if (w)
            gw_window_set_handlers(w, 0, sound_sequence_info_draw, sound_sequence_info_close);
    }
    menu_item_set_value("Sequence", "Infomation", sound_sequence_info != 0);
}

/* 0x00163cb0 */
static void sound_sequence_menu_select(int item, int val, int idx)
{
    sound_sequence_set_next(idx);
}

/* Selects module idx, jumps to sequence 15. */
/* 0x00163cb8 */
static void sound_sequence_module_select(int item, int val, int idx)
{
    sound_sequence_module = sound_sequence_modules[idx][1];
    sound_sequence_set_next(15);
}

/* "Sequence": info window and 15 sequences; "Module": 17 modules. */
/* 0x00163ce0 */
static void sound_sequence_menu_init(void)
{
    int i;
    int j;
    SoundSequenceEntry *ent;

    menu_item_add(0, "Sequence", 0, 0, 1);
    menu_item_add("Sequence", "Infomation", sound_sequence_info_toggle, 0, 0);
    for (i = 0; i < 15; i++)
        menu_item_add("Sequence", sound_sequence_table[i].name, sound_sequence_menu_select, i, 0);
    menu_item_add(0, "Module", 0, 0, 1);
    ent = sound_sequence_modules;
    for (j = 0; j < 17; j++, ent++)
        menu_item_add("Module", ent->name, sound_sequence_module_select, j, 0);
}
