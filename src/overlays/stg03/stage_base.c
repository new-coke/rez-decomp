/*
 * Area 3 stage support: rank, boss boot on a bar start, event listeners, screen filter, star
 * field, map scene manager (starts ground waves).
 */

#include "types_gw.h"
#include "color.h"
#include "stg03.h"
#include "debug.h"
#include "boss_start.h"
#include "callback.h"
#include "config.h"
#include "console.h"
#include "game_flow_editor.h"
#include "gw_varedit.h"
#include "heap.h"
#include "map.h"
#include "map_manager.h"
#include "player.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "stage.h"
#include "task.h"
#include "texforce.h"
#include "stage_base.h"
#include "stage03.h"
#include "../common/eff_circle.h"
#include "../common/eff_star.h"
#include "../common/land_wave.h"
#include "../common/scr_filter.h"

extern ListenerDef listener_defs[5];
extern char filter_area_colors[12][20];
extern char filter_last_color[];
extern char map_menu_table[];
extern char filter_vardef[];
extern GwVarDef filter_vardefs[];
extern int *map_work;
extern void *map_menu;
extern void *listener_handles[5];
extern char str_start_map_shape[];
extern void *map_child;
extern struct TaskDesc map_child_desc;
extern struct TaskDesc map_desc;
extern struct TaskDesc boss_boot_desc;
extern char str_start_map[];
extern float *map_shapes[];

/* 0x00af6700 */
int stage_rating(void)
{
    float rate = ConfigStageShotdownRate();
    int rank = 0;

    if (!(rate < 90.0f)) {
        rank = 2;
        if (rate < 98.0f)
            rank = 1;
    }
    return rank;
}

/* 0x00af6758 */
void stage_data_free(void)
{
    int *w = (int *)stage_get_buffer();

    se_laser_bank_clear();
    heap_free((void *)w[4], 0);
    w[4] = 0;
    stars_off();
}

/* 0x00af6798 */
void boss_boot_exec(int *w)
{
    int prm[2];

    if (w[1] == 0) {
        if (beat_grid_hit(0) && (beat_clock_get_haku() & 3) == 0) {
            map_menu_set_page(21);
            music_change(4);
            beat_clock_request_tempo(138.0f);
            midiseq_mute_request(40);
            midiseq_play();
            texforce_disable();
            w[1]++;
        }
    } else if (--w[0] <= 0) {
        stage_data_free();
        prm[0] = 2;
        prm[1] = stage_rating();
        do {
            boss_start_create(prm);
        } while (0);
        FINISH(w);
    }
}

/* 0x00af6868 */
int *boss_boot_create(void *parent)
{
    int *w = task_create(parent, &boss_boot_desc, 8);

    w[1] = 0;
    w[0] = 280;
    return w;
}

/* 0x00af68a0 */
void listeners_add(void)
{
    unsigned int i;

    for (i = 0; i < 5; i++)
        listener_handles[i] = listener_add(listener_defs[i].idx, listener_defs[i].fn);
}

/* 0x00af6908 */
void listeners_remove(void)
{
    unsigned int i;

    for (i = 0; i < 5; i++)
        listener_remove(listener_defs[i].idx, listener_handles[i]);
}

/* 0x00af6970 */
void listener_stars_on(void)
{
    stars_on();
}

/* 0x00af6978 */
void listener_next_map(void)
{
    map_menu_next();
}

/* 0x00af6980 */
void listener_mute(int *mute)
{
    midiseq_mute_request(mute[0]);
}

/* 0x00af6988 */
void listener_lock_hit(HitEv *ev)
{
    EffCircleArg circ;
    HitObj *hit = ev->obj;
    int hi, lo;

    if (hit->hits <= 0 && ((hit->flags & 0x38) >> 3) == 1)
        TextureForce(hit->id, hit->pos);
    circ.pos = hit->pos;
    lo = ev->span->lo;
    hi = ev->span->hi;
    circ.color = console_get_pcolp();
    circ.radius = (hi - lo + 1.0f) * 6.0f + 10.0f;
    circ.width = 0.5f;
    circ.frames = 70;
    circ.fade = 70;
    circ.n = 16;
    eff_circle_create(game_flow_get_current(), &circ);
}

/* 0x00af6a68 */
void listener_area_change(int *ev)
{
    stage_get_buffer();
    map_get_current();
    tfscreen_select(map_manager_get_scene_id(ev[0]));
    if (map_manager_scene_flag(ev[0])) {
        texforce_enable();
        stars_off();
    } else {
        texforce_disable();
        stars_on();
        player_command_start(0);
    }
}

/* 0x00af6ae0 */
void filter_vars_add(void)
{
    int group = stage_vars_group_get();

    gw_varedit_add_table(&gw_varedit_root, gw_varedit_add(&gw_varedit_root, group, filter_vardef),
                         filter_vardefs);
}

/* 0x00af6b30 */
void filter_vars_remove(void)
{
    int group = stage_vars_group_get();

    gw_varedit_remove(&gw_varedit_root, group, (struct GwVarDef *)filter_vardef);
}

/* 0x00af6b60 */
void filter_reset(void)
{
    int *w = (int *)stage_get_buffer();
    void *filt = (void *)w[2];

    if (filt && !(TASK_OF(filt)->flags & TASK_F_END))
        scr_filter_fade(filt, filter_area_colors, 0);
}

/* 0x00af6ba8 */
void filter_set_area(unsigned int area)
{
    int *w = (int *)stage_get_buffer();

    if (w[2] == 0)
        w[2] = (int)scr_filter_mng_create((void *)w[0]);
    if (area > 11)
        area = 11;
    scr_filter_fade((void *)w[2], filter_area_colors[area], 60);
}

/* 0x00af6c18 */
void filter_fade_last(void)
{
    int *w = (int *)stage_get_buffer();

    if (w && w[2])
        scr_filter_fade((void *)w[2], filter_last_color, 60);
}

/* 0x00af6c58 */
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

/* 0x00af6cd8 */
int stars_off(void)
{
    int *w = (int *)stage_get_buffer();

    if (w && w[1])
        return eff_star_col_create((void *)w[1], 0, 30);
}

/* 0x00af6d10 */
void start_map(void *menu)
{
    int *w = (int *)map_manager_get_work(menu);

    w[2] = map_manager_get_scene(menu);
    map_manager_get_scenes(menu);
    DEBUG_PRINTF(str_start_map, w[2]);
    switch (w[2]) {
    case 0:
        if (w[3] == 0)
            w[3] = (int)land_wave_mng_create(w);
        break;
    case 21:
        map_obj_set_params(menu, 0, 0);
        break;
    }
    map_manager_scene_flag((struct MapManager *)menu);
    map_set_part_radii(map_manager_get_map(menu), map_shapes[w[2]]);
}

/* 0x00af6dd8 */
void start_map_shape(void *menu)
{
    int scene = map_manager_get_scene(menu);

    DEBUG_PRINTF(str_start_map_shape, scene);
}

/* 0x00af6e00 */
void map_init(int *w)
{
    void *child = task_create_inherit(w, &map_child_desc, 8);

    map_child = child;
    map_work = w;
    w[3] = 0;
    map_manager_set_work(map_menu, w);
    map_manager_swap_start_cb(map_menu, start_map_shape);
    map_manager_swap_change_cb(map_menu, start_map);
    map_manager_set_sub_task(map_menu, w);
    map_manager_start_scene(map_menu, 0);
}

/* 0x00af6e90 */
void map_release(void)
{
    FINISH_IF(map_menu);
    map_work = 0;
}

/* 0x00af6eb8 */
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

/* 0x00af6f28 */
void *map_create(void *w)
{
    task_create_sibling_inherit(w, &map_desc, 16);
    return map_child;
}

/* 0x00af6f58 */
void map_menu_create(void *parent)
{
    map_menu = map_manager_task_create_select(parent, map_menu_table, 6, 22, 1, map_create);
}

/* 0x00af6f98 */
void map_menu_next(void)
{
    map_manager_set_next_scene(map_menu, (map_work[2] + 1) % 22);
}

/* 0x00af6fc8 */
void map_menu_set_page(int page)
{
    map_manager_set_next_scene(map_menu, page);
}

/* 0x00af6fd8 */
void map_menu_set_value(int scene)
{
    map_manager_request_scene(map_menu, scene);
}

/* 0x00af6fe8 */
int map_get_current(void)
{
    return map_work[2];
}

/* 0x00af6ff8 */
void *map_menu_get(void)
{
    return map_menu;
}

/* 0x00af7008 */
int *map_get_work(void)
{
    return map_work;
}
