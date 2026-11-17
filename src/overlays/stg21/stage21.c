/* Boss rush stage setup and teardown: boss rush manager, two BPM tables, stage frame rate. */

#include "types_gw.h"
#include "stg21.h"
#include "debug.h"
#include "game.h"
#include "gw_varedit.h"
#include "lockon.h"
#include "pad.h"
#include "sound_bank.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "stage.h"
#include "system.h"
#include "br_manager.h"

extern char lockon_types[];

extern GwVarDef stage_vars_group_def[];
extern GwVarDef stage_vars_table[];
extern char stage_desc[];
extern struct TaskDesc stage_mng_desc;
extern int stage_vars_group;
void module_entry(void);
void *stage_mng_create(void *parent);
void stage_vars_add(void);
extern char bpm_files[2][8];
extern int stage_work_a;
void stage_mng_exec(void);
int stage_mng_debug_keys(void);
void stage_vars_remove(void);

/* 0x00af6700 */
void module_entry(void)
{
    void *prm[4];

    lockon_set_interval_table(lockon_types);
    prm[0] = stage_desc;
    game_stage_start(prm);
}

/* 0x00af6738 */
void stage_init(void)
{
    int *w = (int *)stage_get_buffer();
    int *sw = (int *)stage_get_work();

    stage_get_lockon();
    stage_work_a = sw[0];
    stage_textures = sw[1];
    lockon_set_far(2000.0f);
    br_manager_create(sw);
    w[0] = (int)stage_mng_create(sw);
    sound_mpb_load_create_bpms(bpm_files[0], 1);
    sound_mpb_load_create_bpms(bpm_files[1], 2);
    system_set_frame_rate(2);
    stage_vars_add();
}

/* 0x00af67d8 */
void stage_release(void)
{
    int *w = (int *)stage_get_buffer();

    if (w) {
        stage_vars_remove();
        stage_work_a = 0;
        stage_textures = 0;
        se_stg_common_clear();
        music_change(0);
        lockon_clear_interval_table();
        FINISH_IF((void *)w[0]);
    }
}

/* 0x00af6850 */
void stage_mng_exec(void)
{
    stage_get_buffer();
    if (debug_flags_test(1))
        stage_mng_debug_keys();
}

/* 0x00af6880 */
void *stage_mng_create(void *parent)
{
    return task_create(parent, &stage_mng_desc, 0);
}

/* 0x00af68a8 */
int stage_mng_debug_keys(void)
{
    return pad_keyboard_modifiers_match(68);
}

/* 0x00af68c8 */
void stage_vars_add(void)
{
    stage_vars_group = gw_varedit_add(&gw_varedit_root, 0, stage_vars_group_def);
    gw_varedit_add_table(&gw_varedit_root, stage_vars_group, stage_vars_table);
}

/* 0x00af6918 */
void stage_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, stage_vars_group);
}
