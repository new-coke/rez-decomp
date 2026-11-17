/*
 * Area 3 stage module: start and end (models, stage sequence, SE files, BPM banks), map draw
 * callbacks (pulsing light), the phase task (boss in the last phase), debug keys.
 */

#include "types_gw.h"
#include "color.h"
#include "stg03.h"
#include "debug.h"
#include "file.h"
#include "game.h"
#include "game_flow_editor.h"
#include "gw_varedit.h"
#include "heap.h"
#include "lockon.h"
#include "map.h"
#include "math_util.h"
#include "model.h"
#include "pad.h"
#include "pad_vib.h"
#include "sound_bank.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "stage.h"
#include "system.h"
#include "texforce.h"
#include "ul_math.h"
#include "yuki_tool.h"
#include "stage03.h"
#include "stage_base.h"
#include "../common/attack_ray.h"
#include "../common/en_backfire.h"
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
extern int stage_work_a;
extern int stage_work_b;
extern int phase_requested;
extern int stage_vars_group;

extern MapPulse draw_cb_pulse_table[];
extern char change_symbol_se_table[];
extern char load_files[3][8];
extern char adp_file[];
extern char bpm_files[2][8];
extern void sequence_free(void *s);
extern char stage_textures_file[];
extern void *sequence_load(char *name);

/* 0x00af7e08 */
void draw_cb_pulse_attach(MapDrawObj *obj, int idx)
{
    MapPulse *ent;
    float *src;
    float *p;
    int i;

    obj->idx = idx;
    if (!obj->busy) {
        ent = &draw_cb_pulse_table[idx];
        if (++ent->ref == 1) {
            src = obj->part->link->next->data;
            ent->buf = yuki_alloc(8);
            ent->buf->n = ((short *)src)[3];
            src += 2;
            ent->buf->v = yuki_alloc(ent->buf->n * 4);
            p = ent->buf->v;
            src += ent->ofs;
            for (i = 0; i < ent->buf->n; src += 4, i++, p++) {
                *p = -*src;
                while (*p < 0.0f)
                    *p += ent->step;
                while (*p > ent->step)
                    *p = *p - ent->step;
                *p /= ent->step;
            }
            ent->frame = 0;
            dc_object_free_handles((struct MapPart *)obj->part);
        }
    }
}

/* 0x00af7f98 */
void draw_cb_pulse_release(MapDrawObj *obj)
{
    MapPulse *ent;

    if (!obj->busy) {
        ent = &draw_cb_pulse_table[obj->idx];
        if (--ent->ref == 0) {
            yuki_free(ent->buf->v);
            do { ent->buf->v = 0; } while (0);
            yuki_free(ent->buf);
            ent->buf = 0;
        }
    }
}

/* steer(cast) */
/* 0x00af8008 */
void draw_cb_pulse_exec(MapDrawObj *obj)
{
    MapPulse *ent;
    MapPulseBuf *buf;
    float *ph;
    unsigned int *dst;
    float t;
    float s, c;
    int i, lum;

    if (!obj->busy) {
        ent = &draw_cb_pulse_table[obj->idx];
        ent->frame = (ent->frame + 1) % ent->period;
        buf = ent->buf;
        ph = buf->v;
        dst = obj->part->link->next->data;
        t = (float)ent->frame / (float)ent->period;
        for (i = 0, dst += 5; i < buf->n; i++, dst += 4) {
            ulSinCos((int)(((float (*)(float))float_frac)(*ph++ + t) * ANG_TURN), &s, &c);
            lum = (int)((s + 1.0f) * 255.0f * 0.5f);
            *dst = lum << 24 | lum << 16 | lum << 8 | lum;
        }
    }
}

/* 0x00af8170 */
void draw_cb_pulse_disp(MapDrawObj *obj)
{
    map_draw_part_rgba(obj, obj->part->time, 1.0f, 1.0f, 1.0f, 1.0f);
}

/* 0x00af8190 */
void draw_cb_pulse_init(MapDrawObj *obj)
{
    draw_cb_pulse_attach(obj, 0);
}

/* 0x00af8198 */
void draw_cb_red(DrawObj *obj)
{
    obj->color = RGBA(0x34, 0x10, 0, 0x80);
    map_hook_draw(obj);
}

/* 0x00af81b0 */
void draw_cb_red2(DrawObj *obj)
{
    obj->color = RGBA(0x34, 0x10, 0, 0x80);
    map_hook_draw(obj);
}

/* 0x00af81c8 */
void phase_init(PhaseWork *w)
{
    w->unk_04 = -1;
    w->phase = 0;
    start_phase(w, 0);
    phase_requested = -1;
}

/* 0x00af8208 */
void phase_release(PhaseWork *w)
{
    end_phase(w, w->phase);
}

/* 0x00af8210 */
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

/* 0x00af82d0 */
void phase_disp(void *w)
{
}

/* 0x00af82d8 */
void *phase_create(void *parent)
{
    return task_create(parent, &phase_desc, 8);
}

/* 0x00af8300 */
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

/* 0x00af83c8 */
void end_phase(PhaseWork *w, int phase)
{
    do {
        DEBUG_PRINTF(str_end_phase, phase);
    } while (0);
}

/* 0x00af83e8 */
void phase_request(int phase)
{
    phase_requested = phase;
}

/* 0x00af83f8 */
void phase01_request(void)
{
    phase_request(0);
}

/* 0x00af8400 */
void phase02_request(void)
{
    phase_request(1);
}

/* 0x00af8408 */
void phase03_request(void)
{
    phase_request(2);
}

/* 0x00af8410 */
void phase04_request(void)
{
    phase_request(3);
}

/* 0x00af8418 */
void module_entry(void)
{
    void *prm[4];

    lockon_set_interval_table(lockon_types);
    prm[0] = stage_desc;
    game_stage_start(prm);
}

/* 0x00af8450 */
void module_exit(void *w)
{
    stars_off();
    filter_fade_last();
}

/* 0x00af8470 */
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
    peri_act_start(3);
}

/* 0x00af85b0 */
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

/* 0x00af8688 */
void stage_mng_exec(void)
{
    stage_get_buffer();
    if (debug_flags_test(31))
        stage_mng_debug_keys();
}

/* 0x00af86b8 */
void *stage_mng_create(void *parent)
{
    return task_create(parent, &stage_mng_desc, 0);
}

/* 0x00af86e0 */
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

/* 0x00af8790 */
int stage_vars_group_get(void)
{
    return stage_vars_group;
}

/* 0x00af87a0 */
void stage_vars_add(void)
{
    stage_vars_group = gw_varedit_add(&gw_varedit_root, 0, stage_vars_group_def);
    gw_varedit_add_table(&gw_varedit_root, stage_vars_group, stage_vars_table);
}

/* 0x00af87f0 */
void stage_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, stage_vars_group);
}

/* 0x00b0fa88 */
void models_load(void)
{
    stage_textures = model_load_timed(stage_textures_file, 0x33000);
    stage_models = model_pnb_load(stage_textures_file - 48);
    stage_sequence = sequence_load(stage_textures_file - 40);
    attack_ray_set_model((int)stage_models + 0xad50, (int)stage_models + 0xaf70, stage_textures);
    attack_ray_set_radius(50.0f);
    en_backfire_set_texture(2);
    en_backfire_beam_set_texture(1);
    enemy_shatter_vars_register();
}

/* 0x00b0fb30 */
void models_free(void)
{
    enemy_shatter_vars_unregister();
    sequence_free(stage_sequence);
    model_pnb_unload(stage_models);
    model_unload_timed(stage_textures);
}
