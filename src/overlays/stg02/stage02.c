/*
 * Area 2 stage module: start and end, map draw callbacks (light pulse, tinted parts), the phase
 * task (boss in the last phase), debug keys. models_load starts the missile manager and recorder.
 */

#include "types_gw.h"
#include "color.h"
#include "stg02.h"
#include "debug.h"
#include "file.h"
#include "game.h"
#include "game_flow_editor.h"
#include "gw_varedit.h"
#include "heap.h"
#include "lockon.h"
#include "map.h"
#include "map_manager.h"
#include "math_util.h"
#include "model.h"
#include "pad.h"
#include "pad_vib.h"
#include "render.h"
#include "sound_bank.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "stage.h"
#include "system.h"
#include "texforce.h"
#include "yuki_tool.h"
#include "stage02.h"
#include "en_brahma.h"
#include "en_brahma_rail_manager.h"
#include "en_garuda.h"
#include "en_garuda_nest.h"
#include "en_shiva.h"
#include "en_shiva_mini.h"
#include "en_vishnu.h"
#include "enb_shiva.h"
#include "enb_shiva_manager.h"
#include "stage_base.h"
#include "../common/attack_ray.h"
#include "../common/en_backfire.h"
#include "../common/en_hanuman.h"
#include "../common/en_hanuman_shield.h"
#include "../common/enemy_shatter.h"

extern unsigned int stage_exit_flags;
extern struct TaskDesc phase_desc;
extern char str_start_phase[];
extern char str_end_phase[];
extern char lockon_types[];
extern GwVarDef stage_vars_group_def[];
extern GwVarDef stage_vars_table[];
extern char stage_desc[];
extern struct TaskDesc stage_mng_desc;
extern int phase_flags[4];
extern int stage_work_b;
extern int stage_work_a;
extern int phase_requested;
extern int stage_vars_group;
extern char change_symbol_se_table[];
extern char load_files[3][8];
extern char adp_file[];
extern char bpm_files[2][8];

extern int draw_cb_pulse_ref;
extern void **draw_cb_pulse_buf;
extern char stage_textures_file[];
extern int draw_cb_pulse_frame;

/* 0x00af84b0 */
void draw_cb_pulse_init(MapDrawObj *obj)
{
    float *src;
    float *p;
    int i;

    if (!obj->busy && ++draw_cb_pulse_ref == 1) {
        src = obj->part->link->next->data;
        draw_cb_pulse_buf = yuki_alloc(8);
        *(int *)draw_cb_pulse_buf = ((short *)src)[3];
        src += 4;
        draw_cb_pulse_buf[1] = yuki_alloc(*(int *)draw_cb_pulse_buf * 4);
        p = draw_cb_pulse_buf[1];
        for (i = 0; i < *(int *)draw_cb_pulse_buf; src += 4, i++, p++) {
            *p = -*src;
            while (*p < 0.0f)
                *p += 2000.0f;
            while (*p > 2000.0f)
                *p = *p - 2000.0f;
            *p /= 2000.0f;
        }
        draw_cb_pulse_frame = 0;
        if (!map_manager_scene_flag((int)map_menu_get()))
            dc_object_free_handles((struct MapPart *)obj->part);
    }
}

/* 0x00af8668 */
void draw_cb_pulse_release(int *obj)
{
    if (obj[16] == 0 && --draw_cb_pulse_ref == 0) {
        yuki_free(draw_cb_pulse_buf[1]);
        do {
            draw_cb_pulse_buf[1] = 0;
        } while (0);
        yuki_free(draw_cb_pulse_buf);
        draw_cb_pulse_buf = 0;
    }
}

/* steer(cast) */
/* 0x00af86c8 */
void draw_cb_pulse_exec(MapDrawObj *obj)
{
    float *ph;
    unsigned int *dst;
    float t, level;
    int i, lum;

    if (!obj->busy) {
        draw_cb_pulse_frame = (draw_cb_pulse_frame + 1) % 180;
        ph = draw_cb_pulse_buf[1];
        dst = obj->part->link->next->data;
        t = (float)draw_cb_pulse_frame / 180.0f;
        for (i = 0, dst += 5; i < ((struct { int n; } *)draw_cb_pulse_buf)->n; i++, dst += 4, ph++) {
            level = ((float (*)(float))float_frac)(*ph + t);
            if (level < 0.3f || level > 0.7f)
                level = 0.0f;
            else if (level < 0.5f)
                level = (level - 0.3f) / 0.2f;
            else
                level = (0.7f - level) / 0.2f;
            lum = (int)(level * 255.0f);
            *dst = lum << 24 | lum << 16 | lum << 8 | lum;
        }
        if (map_manager_scene_flag((int)map_menu_get()))
            dc_object_refresh(obj->part);
        else
            dc_object_free_handles((struct MapPart *)obj->part);
    }
}

/* 0x00af88f0 */
void draw_cb_pulse_disp(MapDrawObj *obj)
{
    map_draw_part_rgba(obj, obj->part->time, 1.0f, 1.0f, 1.0f, 1.0f);
}

/* 0x00af8910 */
void draw_cb_disp_dl5(MapDrawObj *obj)
{
    int prev = RenderSetCurrentDisplayList(5);

    map_draw_part_rgba(obj, obj->part->time, 1.0f, 1.0f, 1.0f, 1.0f);
    RenderSetCurrentDisplayList(prev);
}

/* 0x00af8970 */
void draw_cb_cyan(DrawObj *obj)
{
    obj->color = RGBA(0, 0x36, 0x2e, 0x62);
    map_hook_draw(obj);
}

/* 0x00af8988 */
void draw_cb_blue_dl5(DrawObj *obj)
{
    int prev = RenderSetCurrentDisplayList(5);

    obj->color = RGBA(0x17, 0x19, 0x37, 0x80);
    map_hook_draw(obj);
    RenderSetCurrentDisplayList(prev);
}

/* 0x00af89d8 */
void draw_cb_red(DrawObj *obj)
{
    obj->color = RGBA(0x4a, 0x11, 0x06, 0x48);
    map_hook_draw(obj);
}

/* 0x00af89f0 */
void draw_cb_red2(DrawObj *obj)
{
    obj->color = RGBA(0x4a, 0x11, 0x06, 0x48);
    map_hook_draw(obj);
}

/* 0x00af8a08 */
void phase_init(PhaseWork *w)
{
    w->unk_04 = -1;
    w->phase = 0;
    start_phase(w, 0);
    phase_requested = -1;
}

/* 0x00af8a48 */
void phase_release(PhaseWork *w)
{
    end_phase(w, w->phase);
}

/* 0x00af8a50 */
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

/* 0x00af8b10 */
void phase_disp(void)
{
}

/* 0x00af8b18 */
void *phase_create(void *parent)
{
    return task_create(parent, &phase_desc, 8);
}

/* 0x00af8b40 */
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

/* 0x00af8c08 */
void end_phase(PhaseWork *w, int phase)
{
    do {
        DEBUG_PRINTF(str_end_phase, phase);
    } while (0);
}

/* 0x00af8c28 */
void phase_request(int phase)
{
    phase_requested = phase;
}

/* 0x00af8c38 */
void phase01_request(void)
{
    phase_request(0);
}

/* 0x00af8c40 */
void phase02_request(void)
{
    phase_request(1);
}

/* 0x00af8c48 */
void phase03_request(void)
{
    phase_request(2);
}

/* 0x00af8c50 */
void phase04_request(void)
{
    phase_request(3);
}

/* 0x00af8c58 */
void module_entry(void)
{
    void *prm[4];

    lockon_set_interval_table(lockon_types);
    prm[0] = stage_desc;
    game_stage_start(prm);
}

/* 0x00af8c90 */
void module_exit(void)
{
    stars_off();
    filter_fade_last();
}

/* 0x00af8cb0 */
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
    peri_act_start(2);
}

/* 0x00af8df0 */
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

/* 0x00af8ec8 */
void stage_mng_exec(void)
{
    stage_get_buffer();
    if (debug_flags_test(31))
        stage_mng_debug_keys();
}

/* 0x00af8ef8 */
void *stage_mng_create(void *parent)
{
    return task_create(parent, &stage_mng_desc, 0);
}

/* 0x00af8f20 */
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

/* 0x00af8fd0 */
int stage_vars_group_get(void)
{
    return stage_vars_group;
}

/* 0x00af8fe0 */
void stage_vars_add(void)
{
    stage_vars_group = gw_varedit_add(&gw_varedit_root, 0, stage_vars_group_def);
    gw_varedit_add_table(&gw_varedit_root, stage_vars_group, stage_vars_table);
}

/* 0x00af9030 */
void stage_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, stage_vars_group);
}

/* 0x00af9048 */
void models_load(void)
{
    stage_textures = model_load_timed(stage_textures_file, 0x23000);
    stage_models = model_pnb_load(stage_textures_file - 40);
    attack_ray_set_model((int)((char *)stage_models + 1768), (int)((char *)stage_models + 2312),
                         stage_textures);
    attack_ray_set_radius(50.0f);
    en_backfire_set_texture(2);
    en_backfire_beam_set_texture(5);
    enemy_shatter_vars_register();
    en_shiva_nodes_init();
    en_shiva_mini_nodes_init();
    enb_shiva_nodes_init();
    enb_shiva_manager_create((void *)stage_get_lockon());
    en_brahma_models_init();
    en_brahma_rail_manager_create((void *)stage_get_lockon());
    en_garuda_nodes_init();
    en_garuda_nest_nodes_init();
    en_vishnu_nodes_init();
    en_hanuman_models_init();
    en_hanuman_shield_nodes_init();
}

/* 0x00af9138 */
void models_free(void)
{
    enemy_shatter_vars_unregister();
    model_pnb_unload(stage_models);
    model_unload_timed(stage_textures);
}
