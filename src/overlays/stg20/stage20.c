/*
 * Tutorial stage setup and teardown: manager task, message box, button hint, laser sound bank,
 * two BPM tables; starts each tutorial phase from the game flow.
 */

#include "types_gw.h"
#include "stg20.h"
#include "debug.h"
#include "file.h"
#include "game.h"
#include "game_flow_editor.h"
#include "gw_varedit.h"
#include "heap.h"
#include "lockon.h"
#include "pad.h"
#include "player_camera.h"
#include "player_status.h"
#include "sound_bank.h"
#include "sound_midise.h"
#include "stage.h"
#include "system.h"
#include "texforce.h"
#include "stage_base.h"

extern void *scr_root;
extern int phase_flags[];
extern struct TaskDesc phase_desc;
extern char str_start_phase[];

/* file request: the laser sound bank */
extern char load_files[];

extern char bpm_files[2][8];

extern char lockon_types[];

extern char camera_start[];
extern GwVarDef stage_vars_group_def[];
extern GwVarDef stage_vars_table[];
extern char stage_desc[];
extern struct TaskDesc stage_mng_desc;
extern int stage_work_a;
extern int stage_work_b;
extern int stage_vars_group;
void start_phase(void *w, int page);
void *stage_mng_create(void *parent);
int stage_mng_debug_keys(void);
void stage_vars_add(void);
void stage_vars_remove(void);

/* 0x00af8200 */
void phase_init(void *w)
{
    start_phase(w, 0);
}

/* 0x00af8208 */
void phase_release(void)
{
}

/* 0x00af8210 */
void phase_exec(void)
{
}

/* 0x00af8218 */
void phase_disp(void)
{
}

/* 0x00af8220 */
void *phase_create(void *parent)
{
    return task_create(parent, &phase_desc, 4);
}

/* Restarts the game flow timer, then loads or clears the phase's game flow per phase_flags. */
/* 0x00af8248 */
void start_phase(void *w, int phase)
{
    int *flag = &phase_flags[phase];

    DEBUG_PRINTF(str_start_phase, phase);
    stage_phase_set(phase);
    game_flow_time_reset();
    if (*flag)
        GameFlowLoadFile();
    else
        GameFlowClear();
}

/* 0x00af82b8 */
void module_entry(void)
{
    void *prm[4];

    lockon_set_interval_table(lockon_types);
    prm[0] = stage_desc;
    game_stage_start(prm);
}

/* The stage buffer keeps the manager, message box and button hint tasks and the sound bank. */
/* 0x00af82f0 */
void stage_init(void)
{
    int *w = (int *)stage_get_buffer();
    int *sw = (int *)stage_get_work();

    stage_get_lockon();
    stage_work_a = sw[0];
    stage_work_b = sw[1];
    lockon_set_far(2000.0f);
    listeners_add();
    w[0] = (int)stage_mng_create(sw);
    texforce_disable();
    w[3] = file_load_sync(load_files, 0, 0);
    se_laser_bank_set(w[3]);
    sound_mpb_load_create_bpms(bpm_files[0], 1);
    sound_mpb_load_create_bpms(bpm_files[1], 2);
    system_set_frame_rate(1);
    w[1] = (int)explain_create(scr_root);
    w[2] = (int)stg20_button_create(sw);
    player_camera_set_data(camera_start);
    player_status_add_items(7);
    stage_vars_add();
}

/* 0x00af83e0 */
void stage_release(void)
{
    int *w = (int *)stage_get_buffer();

    if (w) {
        stage_vars_remove();
        stage_work_a = 0;
        stage_work_b = 0;
        se_laser_bank_clear();
        heap_free((void *)w[3], 0);
        lockon_clear_interval_table();
        listeners_remove();
        se_stg_common_clear();
        FINISH_IF((void *)w[1]);
        FINISH_IF((void *)w[0]);
    }
}

/* 0x00af8480 */
void stage_mng_exec(void)
{
    stage_get_buffer();
    if (debug_flags_test(1))
        stage_mng_debug_keys();
}

/* 0x00af84b0 */
void *stage_mng_create(void *parent)
{
    return task_create(parent, &stage_mng_desc, 0);
}

/* 0x00af84d8 */
int stage_mng_debug_keys(void)
{
    return pad_keyboard_modifiers_match(68);
}

/* 0x00af84f8 */
void stage_vars_add(void)
{
    stage_vars_group = gw_varedit_add(&gw_varedit_root, 0, stage_vars_group_def);
    gw_varedit_add_table(&gw_varedit_root, stage_vars_group, stage_vars_table);
}

/* 0x00af8548 */
void stage_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, stage_vars_group);
}
