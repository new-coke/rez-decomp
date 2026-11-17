/*
 * Area 5 stage module: start and end, map scene manager and map blur, centred message window
 * (typed in the player's language), the phase task, debug keys, ground height for walls and
 * falling parts.
 */

#include "types_gw.h"
#include "stg05.h"
#include "debug.h"
#include "console_com.h"
#include "file.h"
#include "fog.h"
#include "game.h"
#include "game_flow_editor.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "map.h"
#include "map_manager.h"
#include "model.h"
#include "pad.h"
#include "pad_vib.h"
#include "screen.h"
#include "sound_bank.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "stage.h"
#include "system.h"
#include "task.h"
#include "stage05.h"
#include "dominions_bomb.h"
#include "en_seraphim_dmg.h"
#include "en_virtues_base.h"
#include "en_vts_cmd_dmg_eff.h"
#include "seraphim.h"
#include "stage_base.h"
#include "virtues_wall.h"
#include "../common/attack_ray.h"
#include "../common/en_backfire.h"
#include "../common/en_shatter.h"
#include "../common/en_thrones_armor_die.h"
#include "../common/enemy_shatter.h"

extern GameState game_state;

extern unsigned int stage_exit_flags;
extern struct TaskDesc map_blur_desc;
extern struct TaskDesc phase_desc;
extern char str_end_phase[];
extern char lockon_types[];
extern GwVarDef stage_vars_group_def[];
extern GwVarDef stage_vars_table[];
extern char stage_desc[];
extern struct TaskDesc stage_mng_desc;
extern int phase_requested;
extern int stage_vars_group;
extern void *map_menu;
extern char str_start_map_shape[];
extern int *map_work;
extern struct TaskDesc stage_message_desc;
extern int phase_flags[];
extern char str_start_phase[];
extern void *map_child;
extern struct TaskDesc map_desc;
extern char map_menu_table[];

/* 28 bytes */
typedef struct MapWork {
    char pad_00[8];
    int sel;                    /* 0x08: the map menu selection */
    char pad_0c[12];
    void *sub;                  /* 0x18 */
} MapWork;

extern int stage_work_a;
extern int stage_work_b;
extern char load_files[2][8];
extern char change_symbol_se_table[];
extern char adp_file[];
extern char bpm_files[2][8];
extern float *start_map_points[];
extern char str_start_map[];
extern struct TaskDesc map_child_desc;
extern char *stage_message_lines_en[];
extern char *stage_message_lines_es[];
extern char *stage_message_lines_fr[];
extern char *stage_message_lines_de[];
extern char *stage_message_lines_it[];

/* 28 bytes: opens, types its lines, closes */
typedef struct StageMessageWork {
    void *con;
    int cols;                      /* 0x04: window columns */
    int h;                      /* 0x08: window rows */
    int line;                   /* 0x0c: the next line */
    int delay;                  /* 0x10: frames */
    char **lines;               /* 0x14: the message table of the language, 0-separated */
    int n;                      /* 0x18: lines of this message */
} StageMessageWork;

extern char stage_textures_file[];
extern void *sequence_load(char *name);
extern void sequence_free(void *h);

/* 0x00af7d50 */
void cmd_change_map(void)
{
    map_menu_next();
}

/* 0x00af7d58 */
void cmd_clear(void)
{
    stage_exit_flags |= 2;
}

/* 0x00af7d70 */
void map_blur_release(void)
{
    screen_set_fade_alpha(0);
    screen_set_margin(0);
}

/* 0x00af7d90 */
void map_blur_exec(void)
{
    screen_set_fade_alpha(180);
    screen_set_margin(10);
}

/* 0x00af7db0 */
void *map_blur_create(void *parent)
{
    return task_create_sibling_inherit(parent, &map_blur_desc, 8);
}

/* 0x00af7dd8 */
void start_map(void *menu)
{
    MapWork *w = (MapWork *)map_manager_get_work(menu);

    w->sel = map_manager_get_scene(menu);
    DEBUG_PRINTF(str_start_map, w->sel);
    if (!map_manager_scene_flag((struct MapManager *)menu)) {
        w->sub = map_blur_create(scr_root);
        stage_message_create(w);
    } else if (w->sub) {
        screen_set_fade_alpha(0);
        screen_set_margin(0);
        FINISH_IF(w->sub);
        w->sub = 0;
    }
    map_set_part_radii(map_manager_get_map(menu), start_map_points[w->sel]);
}

/* 0x00af7eb0 */
void start_map_shape(void *menu)
{
    int scene = map_manager_get_scene(menu);

    DEBUG_PRINTF(str_start_map_shape, scene);
}

/* 0x00af7ed8 */
void map_init(MapWork *w)
{
    {
        void **child = &map_child;
        *child = task_create_inherit(w, &map_child_desc, 8);
    }
    en_sort_create(w);
    map_work = (int *)w;
    map_manager_set_work(map_menu, w);
    map_manager_swap_start_cb(map_menu, start_map_shape);
    map_manager_swap_change_cb(map_menu, start_map);
    map_manager_set_sub_task(map_menu, w);
    map_manager_start_scene(map_menu, 0);
    map_obj_set_params(map_menu, 0, 0);
    w->sub = 0;
}

/* 0x00af7f90 */
void map_release(MapWork *w)
{
    FINISH_IF(map_menu);
    FINISH_IF(w->sub);
    map_work = 0;
}

/* 0x00af7fd0 */
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

/* 0x00af8040 */
void *map_create(void *w)
{
    task_create_sibling_inherit(w, &map_desc, 28);
    return map_child;
}

/* 0x00af8070 */
void map_menu_create(void *parent)
{
    map_menu = map_manager_task_create_select(parent, map_menu_table, 6, 29, 1, map_create);
}

/* 0x00af80b0 */
void map_menu_next(void)
{
    map_manager_set_next_scene(map_menu, (map_work[2] + 1) % 29);
}

/* 0x00af80e0 */
void map_menu_set_page(int page)
{
    map_manager_set_next_scene(map_menu, page);
}

/* 0x00af80f0 */
void map_menu_set_value(int scene)
{
    map_manager_request_scene(map_menu, scene);
}

/* 0x00af8100 */
int map_get_current(void)
{
    return map_work[2];
}

/* 0x00af8110 */
void *map_menu_get(void)
{
    return map_menu;
}

/* 0x00af8120 */
int *map_get_work(void)
{
    return map_work;
}

/* 0x00af8130 */
void draw_cb_no_fog(DrawObj *w)
{
    void *fog = fog_set_enable(0);

    map_hook_draw(w);
    fog_set_enable(fog);
}

/* 0x00af8170 */
void stage_message_init(StageMessageWork *w)
{
    int n = map_manager_get_scene_id((int)map_menu_get());
    char **lines;

    switch (game_state.lang) {
    case 0:
    case 1:
        lines = w->lines = stage_message_lines_en;
        break;
    case 2:
        lines = w->lines = stage_message_lines_fr;
        break;
    case 3:
        lines = w->lines = stage_message_lines_es;
        break;
    case 4:
        lines = w->lines = stage_message_lines_de;
        break;
    case 5:
        lines = w->lines = stage_message_lines_it;
        break;
    default:
        lines = w->lines = stage_message_lines_en;
        break;
    }
    w->line = 0;
    for (; n > 0; n--) {
        while (lines[w->line])
            w->line++;
        w->line++;
    }
    w->n = w->line;
    while (lines[w->n])
        w->n++;
    {
        unsigned int lang = game_state.lang;

        w->n -= w->line;
        if (lang < 2 && map_get_current() == 27)
            w->n = 2;
    }
    w->con = console_create_fixed(40, w->n + 2);
    console_set_font_mode(w->con, 2);
    console_set_window(w->con, 0, 0);
    w->delay = 0;
    w->cols = 0;
    w->h = 0;
}

/* 0x00af8340 */
void stage_message_release(void **con)
{
    console_destroy_fixed(*con);
}

/* 0x00af8348 */
void stage_message_center(void **con)
{
    int sz[2];

    console_get_pixel_size(*con, &sz[0], &sz[1]);
    console_set_pos(*con, screen_cfg.cx - sz[0] * 0.5f, screen_cfg.cy - sz[1] * 0.5f + -60.0f);
}

/* 0x00af83c8 */
void stage_message_exec_close(StageMessageWork *w)
{
    if (--w->delay <= 0) {
        console_clear_home(w->con);
        w->delay = 0;
        if (w->h > 0)
            w->h = MAX_GE(w->h - 1, 0);
        else if (w->cols > 0)
            w->cols = MAX_GE(w->cols - 1, 0);
        else
            FINISH(w);
        console_set_window(w->con, w->cols, w->h);
        stage_message_center((void **)w);
    }
}

/* 0x00af8478 */
void stage_message_exec_type(StageMessageWork *w)
{
    int xy[2];
    char **lines;
    char *str;
    int n;

    if (w->delay-- > 0)
        return;
    lines = w->lines;
    str = lines[w->line];
    n = strlen(str) - 1;
    console_get_cursor(w->con, &xy[0], &xy[1]);
    xy[0] = (40 - n) / 2;
    console_set_cursor(w->con, xy[0], xy[1]);
    w->delay = console_puts(w->con, str, 2);
    if (lines[++w->line] == 0) {
        TASK_OF(w)->exec = (void (*)(void *))stage_message_exec_close;
        w->delay += 120.0f;
    }
}

/* 0x00af8580 */
void stage_message_exec(StageMessageWork *w)
{
    if (w->cols < 40) {
        w->cols = MIN(w->cols + 2, 40);
    } else if (w->h < w->n + 2) {
        w->h = MIN(w->n + 2, w->h + 2);
    } else {
        TASK_OF(w)->exec = (void (*)(void *))stage_message_exec_type;
        console_newline(w->con);
    }
    console_set_window(w->con, w->cols, w->h);
    stage_message_center((void **)w);
}

/* 0x00af8618 */
void stage_message_disp(void)
{
}

/* 0x00af8620 */
void *stage_message_create(void *parent)
{
    return task_create(parent, &stage_message_desc, 28);
}

/* 0x00af8648 */
void phase_init(PhaseWork *w)
{
    w->unk_04 = -1;
    w->phase = 0;
    start_phase(w, 0);
    phase_requested = -1;
}

/* 0x00af8688 */
void phase_release(PhaseWork *w)
{
    end_phase(w, w->phase);
}

/* 0x00af8690 */
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

/* 0x00af8750 */
void phase_disp(void)
{
}

/* 0x00af8758 */
void *phase_create(void *parent)
{
    return task_create(parent, &phase_desc, 8);
}

/* 0x00af8780 */
void start_phase(PhaseWork *w, int phase)
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

/* 0x00af87f0 */
void end_phase(PhaseWork *w, int phase)
{
    do {
        DEBUG_PRINTF(str_end_phase, phase);
    } while (0);
}

/* 0x00af8810 */
void phase_request(int phase)
{
    phase_requested = phase;
}

/* 0x00af8820 */
void module_entry(void)
{
    void *prm[4];

    lockon_set_interval_table(lockon_types);
    prm[0] = stage_desc;
    game_stage_start(prm);
}

/* 0x00af8858 */
float module_exit(void)
{
    return beat_clock_get_interval();
}

/* 0x00af8878 */
void stage_init(void)
{
    int *w = (int *)stage_get_buffer();
    int *sw = (int *)stage_get_work();

    stage_get_lockon();
    stage_work_a = sw[0];
    stage_work_b = sw[1];
    lockon_set_far(4000.0f);
    models_load();
    listeners_add();
    w[0] = (int)stage_mng_create(sw);
    w[2] = file_load_sync(load_files[0], 0, 0);
    se_laser_bank_set(w[2]);
    w[3] = file_load_sync(load_files[1], 0, 0);
    change_symbol_se_set_data(change_symbol_se_table, adp_file, 0);
    sound_mpb_load_create_bpms(bpm_files[0], 1);
    sound_mpb_load_create_bpms(bpm_files[1], 2);
    midiseq_play();
    system_set_frame_rate(2);
    stage_vars_add();
    filter_vars_add();
    filter_set_area(0);
    peri_act_start(5);
}

/* 0x00af8990 */
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
        heap_free((void *)w[2], 0);
        change_symbol_se_clear_data();
        midiseq_pcm_table_clear();
        heap_free((void *)w[3], 0);
        se_stg_common_clear();
        music_change(0);
        lockon_clear_interval_table();
        listeners_remove();
        models_free();
        FINISH_IF((void *)w[0]);
    }
}

/* 0x00af8a58 */
void stage_mng_exec(void)
{
    stage_get_buffer();
    if (debug_flags_test(31))
        stage_mng_debug_keys();
}

/* 0x00af8a88 */
void *stage_mng_create(void *parent)
{
    return task_create(parent, &stage_mng_desc, 0);
}

/* 0x00af8ab0 */
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

/* 0x00af8b60 */
int stage_vars_group_get(void)
{
    return stage_vars_group;
}

/* 0x00af8b70 */
void stage_vars_add(void)
{
    stage_vars_group = gw_varedit_add(&gw_varedit_root, 0, stage_vars_group_def);
    gw_varedit_add_table(&gw_varedit_root, stage_vars_group, stage_vars_table);
}

/* 0x00af8bc0 */
void stage_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, stage_vars_group);
}

/* 0x00af8bd8 */
void models_load(void)
{
    stage_textures = model_load_timed(stage_textures_file, 0x53000);
    stage_models = model_pnb_load(stage_textures_file - 40);
    stage_sequence = sequence_load(stage_textures_file - 32);
    attack_ray_set_model((int)stage_models + 0xd78, (int)stage_models + 0xf98, stage_textures);
    attack_ray_set_radius(50.0f);
    en_backfire_set_texture(2);
    enemy_shatter_vars_register();
    seraphim_models_load();
    en_seraphim_dmg_vars_register();
    en_shatter_vars_register();
    en_sort_vars_register();
    en_thrones_armor_die_vars_register();
    en_virtues_base_vars_register();
    en_vts_cmd_dmg_eff_vars_register();
    en_virtues_laser_vars_register();
    virtues_wall_models_load();
    dominions_bomb_models_load();
}

/* 0x00af8cc0 */
void models_free(void)
{
    dominions_bomb_models_free();
    virtues_wall_models_free();
    en_virtues_laser_vars_unregister();
    en_vts_cmd_dmg_eff_vars_unregister();
    en_virtues_base_vars_unregister();
    en_thrones_armor_die_vars_unregister();
    en_sort_vars_unregister();
    en_shatter_vars_unregister();
    en_seraphim_dmg_vars_unregister();
    seraphim_models_free();
    enemy_shatter_vars_unregister();
    sequence_free(stage_sequence);
    model_pnb_unload(stage_models);
    model_unload_timed(stage_textures);
}

/* 0x00af8d50 */
float stage_ground_height(Vec3 *p)
{
    int n = map_get_current();
    float height = 0.0f;

    if ((unsigned int)(n - 3) >= 17)
        height = -10000.0f;
    return height;
}
