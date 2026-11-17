/*
 * Lost Area stage base: loads models, texture force, laser and symbol sounds and BPM tables; runs
 * the 22 maps and four game flow phases; stage events (star field, music mutes, lock-on ring,
 * screen filter and texture force screen on area change).
 */

#include "types_gw.h"
#include "color.h"
#include "stg22.h"
#include "debug.h"
#include "callback.h"
#include "console.h"
#include "file.h"
#include "game.h"
#include "game_flow_editor.h"
#include "gw_varedit.h"
#include "heap.h"
#include "lockon.h"
#include "map.h"
#include "map_manager.h"
#include "pad.h"
#include "pad_vib.h"
#include "player.h"
#include "sound_bank.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "stage.h"
#include "system.h"
#include "task.h"
#include "texforce.h"
#include "e_shot.h"
#include "../common/eff_circle.h"
#include "../common/eff_star.h"
#include "../common/en_shatter.h"
#include "../common/scr_filter.h"

extern unsigned int stage_exit_flags;

extern ListenerDef listener_defs[5];
extern char filter_area_colors[12][20];
extern char filter_last_color[];
extern char filter_vardef[];
extern GwVarDef filter_vardefs[];
extern struct TaskDesc phase_desc;
extern char str_end_phase[];
extern char lockon_types[];
extern GwVarDef stage_vars_group_def[];
extern GwVarDef stage_vars_table[];
extern char stage_desc[];
extern struct TaskDesc stage_mng_desc;
extern int *map_work;
extern int phase_requested;
extern int stage_vars_group;
extern void *map_menu;
extern void *listener_handles[5];
int stage_vars_group_get(void);
void start_phase(PhaseWork *w, int phase);
void phase_request(int phase);
void listeners_add(void);
void listener_lock_hit(HitEv *p);
void filter_vars_add(void);
void filter_vars_remove(void);
void filter_reset(void);
void filter_set_area(unsigned int area);
void filter_fade_last(void);
int stars_on(void);
int stars_off(void);
void map_release(void);
void map_exec(void);
void map_menu_next(void);
void phase_init(PhaseWork *w);
void phase_exec(PhaseWork *w);
void *phase_create(void *parent);
void end_phase(PhaseWork *w, int phase);
void module_entry(void);
void *stage_mng_create(void *parent);
void stage_vars_add(void);
extern char str_start_map_shape[];
extern int phase_flags[];
extern char str_start_phase[];
extern void *map_child;
extern struct TaskDesc map_desc;
extern char map_menu_table[];
void stage_mng_debug_keys(void);
extern char change_symbol_se_table[];
extern char load_files[3][8];
extern char adp_file[];
extern char bpm_files[2][8];
extern int stage_work_a;
extern int stage_work_b;
void listeners_remove(void);
void stage_vars_remove(void);
extern struct TaskDesc map_child_desc;
void start_map(void *s);
int map_get_current(void);
extern char str_start_map[];
extern float *map_shapes[];

/* 0x00af6700 */
void listeners_add(void)
{
    unsigned int i;

    for (i = 0; i < 5; i++)
        listener_handles[i] = listener_add(listener_defs[i].idx, listener_defs[i].fn);
}

/* 0x00af6768 */
void listeners_remove(void)
{
    unsigned int i;

    for (i = 0; i < 5; i++)
        listener_remove(listener_defs[i].idx, listener_handles[i]);
}

/* 0x00af67d0 */
void listener_stars_on(void)
{
    stars_on();
}

/* 0x00af67d8 */
void listener_next_map(void)
{
    map_menu_next();
}

/* 0x00af67e0 */
void listener_mute(int *mute)
{
    midiseq_mute_request(*mute);
}

/* Ring sized by the locks taken; a destroyed texture force kind 1 object fires its force. */
/* 0x00af67e8 */
void listener_lock_hit(HitEv *p)
{
    EffCircleArg circ;
    HitObj *hit = p->obj;
    int hi, lo;

    if (hit->hits <= 0 && ((hit->flags & 0x38) >> 3) == 1)
        TextureForce(hit->id, hit->pos);
    circ.pos = hit->pos;
    lo = p->span->lo;
    hi = p->span->hi;
    circ.color = console_get_pcolp();
    circ.radius = (hi - lo + 1.0f) * 6.0f + 10.0f;
    circ.width = 0.5f;
    circ.frames = 70;
    circ.fade = 70;
    circ.n = 16;
    eff_circle_create(game_flow_get_current(), &circ);
}

/*
 * Selects the area's texture force screen; flagged scenes turn texture force on (stars off),
 * others off (stars on); fades the screen filter to the area's colour.
 */
/* 0x00af68c8 */
void listener_area_change(int *ev)
{
    int area;

    stage_get_buffer();
    map_get_current();
    area = map_manager_get_scene_id(ev[0]);
    tfscreen_select(area);
    if (map_manager_scene_flag(ev[0])) {
        texforce_enable();
        stars_off();
    } else {
        texforce_disable();
        stars_on();
        player_command_start(0);
    }
    filter_set_area(area);
}

/* 0x00af6958 */
void filter_vars_add(void)
{
    int group = stage_vars_group_get();

    gw_varedit_add_table(&gw_varedit_root, gw_varedit_add(&gw_varedit_root, group, filter_vardef),
                         filter_vardefs);
}

/* 0x00af69a8 */
void filter_vars_remove(void)
{
    int group = stage_vars_group_get();

    gw_varedit_remove(&gw_varedit_root, group, (struct GwVarDef *)filter_vardef);
}

/* 0x00af69d8 */
void filter_reset(void)
{
    int *w = (int *)stage_get_buffer();
    void *filt = (void *)w[2];

    if (filt && !(TASK_OF(filt)->flags & TASK_F_END))
        scr_filter_fade(filt, filter_area_colors, 0);
}

/* 0x00af6a20 */
void filter_set_area(unsigned int area)
{
    int *w = (int *)stage_get_buffer();

    if (w[2] == 0)
        w[2] = (int)scr_filter_mng_create((void *)w[0]);
    if (area > 11)
        area = 11;
    scr_filter_fade((void *)w[2], filter_area_colors[area], 60);
}

/* 0x00af6a90 */
void filter_fade_last(void)
{
    int *w = (int *)stage_get_buffer();

    if (w && w[2])
        scr_filter_fade((void *)w[2], filter_last_color, 60);
}

/* 0x00af6ad0 */
int stars_on(void)
{
    int *w = (int *)stage_get_buffer();
    FogArg prm;

    if (w[1] == 0) {
        prm.range = 1000.0f;
        prm.k = 1.0f;
        prm.count = 500;
        prm.color = 0;
        w[1] = (int)eff_star_create(*(void **)w, &prm);
    }
    return eff_star_col_create((void *)w[1], RGBA(0x64, 0x64, 0x64, 0x64), 30);
}

/* 0x00af6b50 */
int stars_off(void)
{
    int *w = (int *)stage_get_buffer();

    if (w && w[1])
        return eff_star_col_create((void *)w[1], 0, 30);
}

/* 0x00af6b88 */
void cmd_change_map(void)
{
    map_menu_next();
}

/* 0x00af6b90 */
void cmd_clear(void)
{
    stage_exit_flags |= 2;
}

/* 0x00af6ba8 */
void start_map(void *menu)
{
    int *w = (int *)map_manager_get_work(menu);

    w[2] = map_manager_get_scene(menu);
    map_manager_get_scenes(menu);
    DEBUG_PRINTF(str_start_map, w[2]);
    switch (w[2]) {
    case 21:
        map_obj_set_params(menu, 0, 0);
        break;
    }
    map_manager_scene_flag((struct MapManager *)menu);
    map_set_part_radii(map_manager_get_map(menu), map_shapes[w[2]]);
}

/* 0x00af6c48 */
void start_map_shape(void *menu)
{
    int scene = map_manager_get_scene(menu);

    DEBUG_PRINTF(str_start_map_shape, scene);
}

/* 0x00af6c70 */
void map_init(int *w)
{
    void **child = &map_child;

    *child = task_create_inherit(w, &map_child_desc, 8);
    en_sort_create(w);
    map_work = w;
    map_manager_set_work(map_menu, w);
    map_manager_swap_start_cb(map_menu, start_map_shape);
    map_manager_swap_change_cb(map_menu, start_map);
    map_manager_set_sub_task(map_menu, w);
    map_manager_start_scene(map_menu, 0);
}

/* 0x00af6d10 */
void map_release(void)
{
    FINISH_IF(map_menu);
    map_work = 0;
}

/* 0x00af6d38 */
void map_exec(void)
{
    if (debug_flags_test(8)) {
        switch (kbd_key) {
        case ',':
            map_menu_next();
            break;
        case 'L':
            FINISH_IF(map_menu);
            break;
        }
    }
}

/* 0x00af6da8 */
void *map_create(void *w)
{
    task_create_sibling_inherit(w, &map_desc, 12);
    return map_child;
}

/* steer(cast) */
/* 0x00af6dd8 */
void map_menu_create(void *parent)
{
    map_menu = ((void *(*)(void *, void *, int, int, void *(*)(void *)))map_manager_task_create_mode2)(parent, map_menu_table, 22, 1, map_create);
}

/* 0x00af6e10 */
void map_menu_next(void)
{
    map_manager_set_next_scene(map_menu, (map_work[2] + 1) % 22);
}

/* 0x00af6e40 */
void map_menu_set_page(int page)
{
    map_manager_set_next_scene(map_menu, page);
}

/* 0x00af6e50 */
void map_menu_set_value(int scene)
{
    map_manager_request_scene(map_menu, scene);
}

/* 0x00af6e60 */
int map_get_current(void)
{
    return map_work[2];
}

/* 0x00af6e70 */
void *map_menu_get(void)
{
    return map_menu;
}

/* 0x00af6e80 */
void *map_get_work(void)
{
    return map_work;
}

/* 0x00af6e90 */
void phase_init(PhaseWork *w)
{
    w->unk_04 = -1;
    w->phase = 0;
    start_phase(w, 0);
    phase_requested = -1;
}

/* 0x00af6ed0 */
void phase_release(PhaseWork *w)
{
    end_phase(w, w->phase);
}

/* 0x00af6ed8 */
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

/* 0x00af6f98 */
void phase_disp(void)
{
}

/* 0x00af6fa0 */
void *phase_create(void *parent)
{
    return task_create(parent, &phase_desc, 8);
}

/* 0x00af6fc8 */
void start_phase(PhaseWork *w, int page)
{
    int *flag = &phase_flags[page];

    DEBUG_PRINTF(str_start_phase, page);
    stage_phase_set(page);
    game_flow_time_reset();
    if (*flag)
        GameFlowLoadFile();
    else
        GameFlowClear();
}

/* 0x00af7038 */
void end_phase(PhaseWork *w, int phase)
{
    do {
        DEBUG_PRINTF(str_end_phase, phase);
    } while (0);
}

/* 0x00af7058 */
void phase_request(int phase)
{
    phase_requested = phase;
}

/* 0x00af7068 */
void module_entry(void)
{
    void *prm[4];

    lockon_set_interval_table(lockon_types);
    prm[0] = stage_desc;
    game_stage_start(prm);
}

/* 0x00af70a0 */
void module_exit(void)
{
    stars_off();
    filter_fade_last();
}

/* 0x00af70c0 */
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
    peri_act_start(8);
}

/* 0x00af7200 */
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
        heap_free((void *)w[5], 0);
        se_stg_common_clear();
        music_change(0);
        lockon_clear_interval_table();
        listeners_remove();
        models_free();
        FINISH_IF((void *)w[0]);
    }
}

/* 0x00af72d0 */
void stage_mng_exec(void)
{
    stage_get_buffer();
    if (debug_flags_test(1))
        stage_mng_debug_keys();
}

/* 0x00af7300 */
void *stage_mng_create(void *parent)
{
    return task_create(parent, &stage_mng_desc, 0);
}

/* 0x00af7328 */
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

/* 0x00af73d8 */
int stage_vars_group_get(void)
{
    return stage_vars_group;
}

/* 0x00af73e8 */
void stage_vars_add(void)
{
    stage_vars_group = gw_varedit_add(&gw_varedit_root, 0, stage_vars_group_def);
    gw_varedit_add_table(&gw_varedit_root, stage_vars_group, stage_vars_table);
}

/* 0x00af7438 */
void stage_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, stage_vars_group);
}
