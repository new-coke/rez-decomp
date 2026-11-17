/*
 * Stage 6 module: no enemies of its own. Sets fog, lock-on range and BPM banks, then boots the
 * fifth boss at once (stage_base.c); variable group and debug-key hook.
 */

#include "types_gw.h"
#include "color.h"
#include "stg06.h"
#include "debug.h"
#include "fog.h"
#include "game.h"
#include "gw_varedit.h"
#include "lockon.h"
#include "pad.h"
#include "pad_vib.h"
#include "sound_bank.h"
#include "sound_midiseq.h"
#include "stage.h"
#include "system.h"
#include "stage_base.h"

extern struct GwVarNode *gw_varedit_root;

extern GwVarDef stage_vars_group_def[];

extern GwVarDef stage_vars_table[];
extern char lockon_types[];
extern char stage_desc[];
extern struct TaskDesc stage_mng_desc;
extern int stage_vars_group;
void module_entry(void);
void *stage_mng_create(void *parent);
void stage_vars_add(void);

extern char bpm_files[2][8];

extern int stage_work_a;
extern int stage_work_b;
void stage_mng_debug_keys(void);
void stage_vars_remove(void);

/* 0x00af6ab8 */
void module_entry(void)
{
    void *prm[4];

    lockon_set_interval_table(lockon_types);
    prm[0] = stage_desc;
    game_stage_start(prm);
}

/* 0x00af6af0 */
void stage_init(void)
{
    int *w = (int *)stage_get_buffer();
    float far = 2000.0f;
    int *sw = (int *)stage_get_work();

    stage_get_lockon();
    fog_set(RGBA(0x05, 0, 0, 0x80), 4, 200.0f, far);
    BackColReq(RGBA(0x05, 0, 0, 0x80));
    stage_work_a = sw[0];
    stage_work_b = sw[1];
    lockon_set_far(far);
    w[0] = (int)stage_mng_create(sw);
    sound_mpb_load_create_bpms(bpm_files[0], 1);
    sound_mpb_load_create_bpms(bpm_files[1], 2);
    system_set_frame_rate(2);
    stage_vars_add();
    boss_boot_create((void *)w[0]);
    peri_act_start(6);
}

/* 0x00af6bd0 */
void stage_release(void)
{
    int *w = (int *)stage_get_buffer();

    if (w) {
        peri_act_setup();
        stage_vars_remove();
        stage_work_a = 0;
        stage_work_b = 0;
        music_change(0);
        lockon_clear_interval_table();
        FINISH_IF((void *)w[0]);
    }
}

/* 0x00af6c48 */
void stage_mng_exec(void)
{
    stage_get_buffer();
    if (debug_flags_test(1))
        stage_mng_debug_keys();
}

/* 0x00af6c78 */
void *stage_mng_create(void *parent)
{
    return task_create(parent, &stage_mng_desc, 0);
}

/* 0x00af6ca0 */
void stage_mng_debug_keys(void)
{
    int key = kbd_key;

    if (!pad_keyboard_modifiers_match(68))
        key = 0;
    switch (key) {
    }
}

/* 0x00af6cc0 */
void stage_vars_add(void)
{
    stage_vars_group = gw_varedit_add(&gw_varedit_root, 0, stage_vars_group_def);
    gw_varedit_add_table(&gw_varedit_root, stage_vars_group, stage_vars_table);
}

/* 0x00af6d10 */
void stage_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, stage_vars_group);
}
