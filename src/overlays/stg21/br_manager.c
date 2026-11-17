/*
 * Boss rush manager: per boss, starts its vibration pattern and starter, waits for the clear,
 * then opens the score screen and waits for it to close.
 */

#include "types_gw.h"
#include "stg21.h"
#include "boss_start.h"
#include "debug.h"
#include "game.h"
#include "gw_varedit.h"
#include "pad.h"
#include "pad_vib.h"
#include "sound_midiseq.h"
#include "system.h"
#include "br_manager.h"
#include "br_score.h"

/* 12 bytes */
typedef struct BrManager {
    int step;                   /* three steps per boss */
    struct BrScore *score;      /* 0x04 */
    int peri;                   /* 0x08 */
} BrManager;

extern int boss_levels[];
extern void *(*boss_boot_creates[])(void *parent);
extern char br_manager_vars_group_def[];
extern GwVarDef br_manager_vars_table[];
extern int boss_peri_acts[];
extern struct TaskDesc br_manager_desc;
extern int br_manager_vars_group;

/* steer(cast) */
/* 0x00af6f90 */
void br_manager_vars_add(void)
{
    br_manager_vars_group = ((int (*)(struct GwVarNode **, int, char *))gw_varedit_add)(&gw_varedit_root, game_varedit_get(), br_manager_vars_group_def);
    gw_varedit_add_table(&gw_varedit_root, br_manager_vars_group, br_manager_vars_table);
}

/* 0x00af6fe8 */
void br_manager_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, game_varedit_get(), (struct GwVarDef *)br_manager_vars_group_def);
}

/* 0x00af7020 */
void br_manager_exec(BrManager *w)
{
    int boss;

    if (debug_flags_test(16) && kbd_key == 6)
        stage_exit_flags |= 2;
    switch (w->step % 3) {
    case 0:
        boss = w->step / 3;
        peri_act_start(boss_peri_acts[boss]);
        peri_act_set_selection(16);
        w->peri = 1;
        boss_boot_creates[boss](w);
        w->step++;
        break;
    case 1:
        if (stage_exit_flags & 2) {
            stage_exit_flags &= ~8;
            stage_exit_flags &= ~2;
            boss_start_kill();
            w->peri = 0;
            peri_act_setup();
            music_change(0);
            pad_trigger_disable(0);
            system_pause_disable();
            br_score_open(w->score, w->step / 3);
            w->step++;
        }
        break;
    case 2:
        pad_trigger_disable(0);
        system_pause_disable();
        if (br_score_is_idle(w->score)) {
            system_pause_enable();
            w->step++;
        }
        break;
    }
}

/* 0x00af71e0 */
void br_manager_init(BrManager *w)
{
    br_manager_vars_add();
    w->score = br_score_create(w);
    pad_trigger_disable(0);
}

/* 0x00af7218 */
void br_manager_release(BrManager *w)
{
    br_manager_vars_remove();
    if (w->peri)
        peri_act_setup();
    pad_trigger_enable(0);
}

/* 0x00af7258 */
void *br_manager_create(void *parent)
{
    return task_create(parent, &br_manager_desc, 12);
}

/* 0x00af7280 */
int boss_level_get(int boss)
{
    return boss_levels[boss];
}
