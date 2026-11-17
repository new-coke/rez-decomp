/*
 * Trance Mission stage setup, teardown and phases: models, texture force, laser sounds, BPM
 * tables; plays as a Beyond game, camera fixed and console hidden (restored on exit); loops
 * three game flow phases.
 */

#include "types_gw.h"
#include "stg23.h"
#include "debug.h"
#include "config.h"
#include "console.h"
#include "file.h"
#include "game.h"
#include "game_flow_editor.h"
#include "gw_varedit.h"
#include "heap.h"
#include "lockon.h"
#include "model.h"
#include "pad.h"
#include "pad_vib.h"
#include "sound_bank.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "sound_pcmstream.h"
#include "stage.h"
#include "system.h"
#include "texforce.h"
#include "stage23.h"
#include "en_check.h"
#include "en_color.h"
#include "stage_base.h"
#include "../common/enemy_shatter.h"

extern struct TaskDesc phase_desc;
extern char str_end_phase[];
extern char lockon_types[];
extern GwVarDef stage_vars_group_def[];
extern GwVarDef stage_vars_table[];
extern char stage_desc[];
extern struct TaskDesc stage_mng_desc;
extern int phase_requested;
extern int stage_vars_group;
extern int phase_load_flags[];
extern char str_start_phase[];
extern int stage_work_b;
extern int stage_work_a;
extern int saved_config_select;
extern int saved_disp_level;
extern int saved_game_type;
extern char load_files[2][8];
extern char bpm_files[2][8];
extern char stage_models_file[];

/* 0x00af7c28 */
void phase_init(PhaseWork *w)
{
    w->unk_04 = -1;
    w->phase = 0;
    start_phase(w, 0);
    phase_requested = -1;
}

/* 0x00af7c68 */
void phase_release(PhaseWork *w)
{
    end_phase(w, w->phase);
}

/* 0x00af7c70 */
void phase_exec(PhaseWork *w)
{
    if (game_flow_is_finished()) {
        unsigned int next;

        switch (w->phase) {
        case 0:
            next = 1;
            break;
        case 1:
            next = 2;
            break;
        case 2:
            next = 0;
            break;
        default:
            next = 0xffffffff;
            break;
        }
        phase_request(next);
    }
    if (phase_requested >= 0) {
        if (w->phase != phase_requested) {
            end_phase(w, w->phase);
            start_phase(w, phase_requested);
            w->phase = phase_requested;
        }
        phase_requested = -1;
    }
}

/* 0x00af7d30 */
void phase_disp(void)
{
}

/* 0x00af7d38 */
void *phase_create(void *parent)
{
    return task_create(parent, &phase_desc, 8);
}

/* 0x00af7d60 */
void start_phase(PhaseWork *w, int phase)
{
    int *on = &phase_load_flags[phase];

    DEBUG_PRINTF(str_start_phase, phase);
    stage_phase_set(phase);
    game_flow_time_reset();
    if (*on)
        GameFlowLoadFile();
    else
        GameFlowClear();
}

/* 0x00af7dd0 */
void end_phase(PhaseWork *w, int phase)
{
    do {
        DEBUG_PRINTF(str_end_phase, phase);
    } while (0);
}

/* 0x00af7df0 */
void phase_request(int phase)
{
    phase_requested = phase;
}

/* 0x00af7e00 */
void module_entry(void)
{
    void *prm[4];

    lockon_set_interval_table(lockon_types);
    prm[0] = stage_desc;
    game_stage_start(prm);
}

/* 0x00af7e38 */
void module_exit(void)
{
    filter_fade_last();
}

/* 0x00af7e40 */
void stage_init(void)
{
    int *w = (int *)stage_get_buffer();
    int *sw = (int *)stage_get_work();

    stage_get_lockon();
    stage_work_a = sw[0];
    stage_work_b = sw[1];
    lockon_set_far(2000.0f);
    models_load();
    listeners_add();
    w[0] = (int)stage_mng_create(sw);
    w[3] = file_load_sync(load_files[0], 0, 0);
    texforce_load_ktf((char *)w[3]);
    texforce_enable();
    w[4] = file_load_sync(load_files[1], 0, 0);
    se_laser_bank_set(w[4]);
    sound_mpb_load_create_bpms(bpm_files[0], 1);
    sound_mpb_load_create_bpms(bpm_files[1], 2);
    midiseq_play();
    system_set_frame_rate(2);
    stage_vars_add();
    filter_vars_add();
    filter_set_area(0);
    peri_act_start(9);
}

/* 0x00af7f50 */
void stage_release(void)
{
    int *w = (int *)stage_get_buffer();

    if (w) {
        pcm_stop();
        peri_act_setup();
        stage_vars_remove();
        stage_work_a = 0;
        stage_work_b = 0;
        se_laser_bank_clear();
        heap_free((void *)w[4], 0);
        texforce_unload_ktf();
        heap_free((void *)w[3], 0);
        change_symbol_se_clear_data();
        heap_free((void *)w[5], 0);
        se_stg_common_clear();
        music_change(0);
        lockon_clear_interval_table();
        listeners_remove();
        models_free();
        FINISH_IF((void *)w[0]);
    }
}

/* 0x00af8020 */
void stage_mng_exec(void)
{
    stage_get_buffer();
    if (debug_flags_test(31))
        stage_mng_debug_keys();
}

/* 0x00af8050 */
void *stage_mng_create(void *parent)
{
    return task_create(parent, &stage_mng_desc, 0);
}

/* 0x00af8078 */
void stage_mng_debug_keys(void)
{
    int key = kbd_key;

    if (!pad_keyboard_modifiers_match(68))
        key = 0;
    switch (key) {
    case 30:
        phase_request(0);
        break;
    case 31:
        phase_request(1);
        break;
    case 32:
        phase_request(2);
        break;
    case 33:
        phase_request(3);
        break;
    }
}

/* 0x00af8128 */
int stage_vars_group_get(void)
{
    return stage_vars_group;
}

/* 0x00af8138 */
void stage_vars_add(void)
{
    stage_vars_group = gw_varedit_add(&gw_varedit_root, 0, stage_vars_group_def);
    gw_varedit_add_table(&gw_varedit_root, stage_vars_group, stage_vars_table);
}

/* 0x00af8188 */
void stage_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, stage_vars_group);
}

/* steer(cast) */
/* 0x00af81a0 */
void models_load(void)
{
    stage_textures = ((void *(*)(char *, int))model_load_timed)(stage_models_file + 32, 0x233000);
    stage_models = model_pnb_load(stage_models_file);
    enemy_shatter_vars_register();
    en_check_create((void *)stage_get_lockon());
    en_color_vars_register();
    saved_game_type = game_type_get();
    game_type_set(5);
    saved_config_select = ConfigGetSelect(8);
    ConfigSetSelect(8, 4);
    saved_disp_level = console_set_disp_level(0);
}

/* 0x00af8250 */
void models_free(void)
{
    console_set_disp_level(saved_disp_level);
    ConfigSetSelect(8, saved_config_select);
    game_type_set(saved_game_type);
    en_color_vars_unregister();
    enemy_shatter_vars_unregister();
    model_pnb_unload(stage_models);
    model_unload_timed(stage_textures);
}
