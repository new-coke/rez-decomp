/*
 * Built-in debug commands. Most act only in debug mode (system flag 0x8000) and report in a
 * message window.
 */

#include "debug.h"
#include "color.h"
#include "gw_command.h"
#include "command_set.h"
#include "command.h"
#include "console.h"
#include "enemy_status.h"
#include "heap.h"
#include "pcfile.h"
#include "player.h"
#include "player_status.h"
#include "replay.h"
#include "system.h"
#include "texforce.h"

extern char command_table[];
extern char command_gw_table[];
extern unsigned int system_flags;
extern char str_command_muteking_off[];
extern char str_command_muteking_on[];
extern int command_console_level;
extern char str_command_machagogo_off[];
extern char str_command_machagogo_on[];
extern char str_command_fullrange_on[];
extern char str_command_fullrange_off[];
extern char str_command_r_save_usage[];
extern void *wizard_work;
extern char str_wizard[];
extern char str_wizard_off[];
extern int scr_root;
extern struct TaskDesc wizard_desc;

static int wizard_task_create(void);

/* Command line and debug window system. */
/* 0x00109c10 */
void command_tables_register(void)
{
    command_register_table(command_table);
    gw_command_add_table(command_gw_table);
}

/* 0x00109c38 */
void command_tables_unregister(void)
{
    gw_command_remove_table(command_gw_table);
    command_unregister_table(command_table);
}

/* Toggles player status flag 0x100000. */
/* steer(cast) */
/* 0x00109c60 */
static int command_muteking(void)
{
    if (system_flags & SYS_DEBUG) {
        if (player_status_test_flags(0x100000)) {
            ((void (*)(int, int, int, unsigned int, void *))debug_message_window)(1, 0, 120, 0x80800000, str_command_muteking_off);
            player_status_clear_flags(0x100000);
        } else {
            ((void (*)(int, int, int, unsigned int, void *))debug_message_window)(1, 0, 120, 0x80800000, str_command_muteking_on);
            player_status_set_flags(0x100000);
        }
    }
    return 0;
}

/* Hides the console or restores its level; works in any mode. */
/* 0x00109ce8 */
static int command_console(void)
{
    int level = console_get_disp_level();

    if (level == 0)
        console_set_disp_level(command_console_level);
    else
        console_set_disp_level(0);
    command_console_level = level;
    return 0;
}

/* Enemy action time scale 1.0 <-> 1.5. */
/* steer(cast) */
/* 0x00109d40 */
static int command_machagogo(void)
{
    if (system_flags & SYS_DEBUG) {
        float v = 1.5f;

        if (enemy_status_get_action_time() == v) {
            ((void (*)(int, int, int, unsigned int, void *))debug_message_window)(1, 0, 120, 0x80800000, str_command_machagogo_off);
            enemy_status_set_action_time(1.0f);
        } else {
            ((void (*)(int, int, int, unsigned int, void *))debug_message_window)(1, 0, 120, 0x80800000, str_command_machagogo_on);
            enemy_status_set_action_time(v);
        }
    }
    return 0;
}

/* 0x00109de8 */
static int command_wizard(void)
{
    if (system_flags & SYS_DEBUG)
        wizard_task_create();
    return 0;
}

/* 0x00109e18 */
static int command_debugoff(void)
{
    if (system_flags & SYS_DEBUG)
        debug_off(-1);
    return 0;
}

/* steer(cast) */
/* 0x00109e48 */
static int command_fullrange(void)
{
    int on;

    if (system_flags & SYS_DEBUG) {
        on = player_swap_fullrange(1) == 0;
        player_swap_fullrange(on);
        if (on)
            ((void (*)(int, int, int, unsigned int, void *))debug_message_window)(1, 0, 120, 0x80800000, str_command_fullrange_on);
        else
            ((void (*)(int, int, int, unsigned int, void *))debug_message_window)(1, 0, 120, 0x80800000, str_command_fullrange_off);
    }
    return 0;
}

/* "r_save <file>": current replay data to the host PC. */
/* 0x00109ed0 */
static int command_r_save(int argc, char **argv)
{
    int n;
    void *buf;

    if (system_flags & SYS_DEBUG) {
        if (argc < 2) {
            DEBUG_PRINTF(str_command_r_save_usage);
            return 0;
        }
        argv++;
        n = replay_calc_size();
        buf = heap_block_tag(game_malloc(n), "command_set.c", 279);
        ReplayMakeData(buf);
        pcfile_save_set(argv[0], buf, n);
    }
    return 0;
}

/* 0x00109f78 */
static int command_r_load(void)
{
    return 0;
}

/* Sets task flag 8. */
/* 0x00109f80 */
static void wizard_init(void *w)
{
    char *base = (char *)w;
    wizard_work = w;
    TASK_OF(base)->flags |= TASK_F_RUN_PAUSED;
    debug_message_window(1, 1, 24, RGBA(0x80, 0, 0x80, 0x80), str_wizard);
}

/* 0x00109fb8 */
static void wizard_release(void)
{
    debug_message_window(1, 1, 24, RGBA(0x80, 0, 0x80, 0x80), str_wizard_off);
    wizard_work = 0;
}

/* Every frame: frame rate mode 1, texforce off. */
/* 0x00109ff8 */
static void wizard_exec(void)
{
    system_set_frame_rate(1);
    texforce_disable();
}

/* 0x0010a018 */
static void wizard_disp(void)
{
}

/* Toggles. */
/* 0x0010a020 */
static int wizard_task_create(void)
{
    char *w = wizard_work;
    if (w) {
        TASK_OF(w)->flags |= TASK_F_END;
        do {
            return 0;
        } while (0);
    }
    return task_create(scr_root, &wizard_desc, 4);
}
