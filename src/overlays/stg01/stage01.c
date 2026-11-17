/*
 * Area 1 stage module: start and end (texture-force, SE files, BPM banks, models), the phase task
 * through four phases (boss in the last), map draw callbacks, debug keys.
 */

#include "types_gw.h"
#include "color.h"
#include "stg01.h"
#include "debug.h"
#include "camera_demo.h"
#include "file.h"
#include "game.h"
#include "game_flow_editor.h"
#include "gw_varedit.h"
#include "heap.h"
#include "item.h"
#include "lockon.h"
#include "map.h"
#include "pad.h"
#include "pad_vib.h"
#include "player.h"
#include "player_status.h"
#include "render.h"
#include "sound_bank.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "stage.h"
#include "system.h"
#include "texforce.h"
#include "ul_vu0.h"
#include "stage01.h"
#include "attack_ray.h"
#include "stage_base.h"

extern unsigned char kbd_state[];
extern int stage_work_a;
extern int stage_work_b;
extern char load_files[3][8];
extern char change_symbol_se_table[];
extern char adp_file[];
extern char bpm_files[2][8];
extern char str_test_bonus[];
extern unsigned int stage_exit_flags;
extern int phase_requested;
extern int phase_flags[4];
extern struct TaskDesc phase_desc;
extern char str_start_phase[];
extern char str_end_phase[];
extern char lockon_types[];
extern GwVarDef stage_vars_group_def[];
extern GwVarDef stage_vars_table[];
extern char stage_desc[];
extern struct TaskDesc stage_mng_desc;
extern int stage_vars_group;

/* 0x00af9410 */
void draw_cb_list3(DrawObj *act)
{
    int dl = RenderSetCurrentDisplayList(3);

    act->unk_20 = 0;
    map_hook_draw(act);
    RenderSetCurrentDisplayList(dl);
}

/* 0x00af9458 */
void draw_cb_red(DrawObj *obj)
{
    obj->color = RGBA(0x1e, 0x0f, 0x05, 0x80);
    map_hook_draw(obj);
}

/* 0x00af9470 */
void draw_cb_by_map(DrawObj *act)
{
    act->color = (unsigned int)map_get_current() < 3 ? RGBA(0x1e, 0x1e, 0x1e, 0x80) : RGBA(0x32, 0x32, 0x32, 0x80);
    map_hook_draw(act);
}

/* 0x00af94b8 */
void draw_cb_plain(DrawObj *act)
{
    map_hook_draw(act);
}

/* 0x00af94c0 */
void phase_init(PhaseWork *w)
{
    w->unk_04 = -1;
    w->phase = 0;
    start_phase(w, 0);
    phase_requested = -1;
}

/* 0x00af9500 */
void phase_release(PhaseWork *w)
{
    end_phase(w, w->phase);
}

/* 0x00af9508 */
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
            next = 3;
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

/* 0x00af95c8 */
void phase_disp(void)
{
}

/* 0x00af95d0 */
void *phase_create(void *parent)
{
    return task_create(parent, &phase_desc, 8);
}

/* 0x00af95f8 */
void start_phase(PhaseWork *w, int phase)
{
    int *on = &phase_flags[phase];

    DEBUG_PRINTF(str_start_phase, phase);
    stage_phase_set(phase);
    game_flow_time_reset();
    switch (phase) {
    case 0:
    case 1:
    case 2:
        break;
    case 3:
        if (!(stage_exit_flags & 8)) {
            peri_act_select_boss();
            filter_fade_last();
            boss_boot_create(w);
        }
        break;
    }
    if (*on)
        GameFlowLoadFile();
    else
        GameFlowClear();
}

/* 0x00af96c0 */
void end_phase(PhaseWork *w, int phase)
{
    do {
        DEBUG_PRINTF(str_end_phase, phase);
    } while (0);
}

/* 0x00af96e0 */
void phase_request(int phase)
{
    phase_requested = phase;
}

/* 0x00af96f0 */
void phase01_request(void)
{
    phase_request(0);
}

/* 0x00af96f8 */
void phase02_request(void)
{
    phase_request(1);
}

/* 0x00af9700 */
void phase03_request(void)
{
    phase_request(2);
}

/* 0x00af9708 */
void phase04_request(void)
{
    phase_request(3);
}

/* 0x00af9710 */
void module_entry(void)
{
    void *prm[4];

    lockon_set_interval_table(lockon_types);
    prm[0] = stage_desc;
    game_stage_start(prm);
}

/* 0x00af9748 */
void module_exit(void)
{
    stars_off();
    filter_fade_last();
}

/* 0x00af9768 */
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
    w[5] = file_load_sync(load_files[2], 0, 0);
    change_symbol_se_set_data(change_symbol_se_table, adp_file, w[5]);
    sound_mpb_load_create_bpms(bpm_files[0], 1);
    sound_mpb_load_create_bpms(bpm_files[1], 2);
    midiseq_play();
    system_set_frame_rate(2);
    stage_vars_add();
    filter_vars_add();
    filter_set_area(0);
    peri_act_start(1);
}

/* 0x00af98a8 */
void stage_release(void)
{
    int *w = (int *)stage_get_buffer();

    if (w) {
        peri_act_setup();
        filter_vars_remove();
        stage_vars_remove();
        stage_work_a = 0;
        stage_work_b = 0;
        se_laser_bank_clear();
        heap_free((void *)w[4], 0);
        texforce_unload_ktf();
        heap_free((void *)w[3], 0);
        change_symbol_se_clear_data();
        midiseq_pcm_table_clear();
        heap_free((void *)w[5], 0);
        se_stg_common_clear();
        music_change(0);
        lockon_clear_interval_table();
        listeners_remove();
        models_free();
        FINISH_IF((void *)w[0]);
    }
}

/* 0x00af9980 */
void stage_mng_exec(void)
{
    stage_get_buffer();
    if (debug_flags_test(31))
        stage_mng_debug_keys();
}

/* 0x00af99b0 */
void *stage_mng_create(void *parent)
{
    return task_create(parent, &stage_mng_desc, 0);
}

/* 0x00af99d8 */
void debug_item_create(int kind)
{
    Spawn spawn;
    Vec3 cpos;
    Vec3 fwdv;

    player_get_pos(&cpos);
    player_get_accel(&fwdv);
    if (!v3_is_zero(&fwdv)) {
        vec3_normalize(&fwdv, &fwdv);
        v3_scale(&fwdv, 200.0f);
        v3_add(&cpos, &fwdv);
    }
    spawn.kind = kind;
    spawn.x = cpos.x;
    spawn.y = cpos.y;
    spawn.z = cpos.z;
    spawn.vel_x = 0;
    spawn.vel_y = 0;
    spawn.vel_z = 0;
    spawn.scale = 0.9f;
    item_create(&spawn);
}

/* 0x00af9b08 */
void stage_mng_debug_keys(void)
{
    int key = kbd_state[5];
    int alt = kbd_state[6];
    int mod = kbd_state[4];

    if (!pad_keyboard_modifiers_match(68)) {
        key = 0;
        alt = 0;
        mod = 0;
    }
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
    if (debug_flags_test(1)) {
        switch (alt) {
        case 24:
            debug_item_create(0);
            break;
        case 12:
            debug_item_create(1);
            break;
        case 18:
            debug_item_create(2);
            break;
        case 19:
            debug_item_create(3);
            break;
        }
        switch (key) {
        case 5:
            player_status_add_score_event(str_test_bonus, 10000);
            break;
        case 22:
            stars_on();
            break;
        case 7:
            stars_off();
            break;
        case 4:
            player_command_start(0);
            break;
        case 20:
            oscilate_create(0.05f, 0.1f);
            break;
        }
        switch (mod) {
        case 4:
            player_status_add_score(100);
            break;
        case 17:
            player_status_add_damage(1.0f);
            break;
        }
    }
}

/* 0x00af9d38 */
int stage_vars_group_get(void)
{
    return stage_vars_group;
}

/* 0x00af9d48 */
void stage_vars_add(void)
{
    stage_vars_group = gw_varedit_add(&gw_varedit_root, 0, stage_vars_group_def);
    gw_varedit_add_table(&gw_varedit_root, stage_vars_group, stage_vars_table);
}

/* 0x00af9d98 */
void stage_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, stage_vars_group);
}
