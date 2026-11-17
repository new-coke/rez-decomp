/*
 * Trance Mission stage base: 11 maps through the map manager; on area change a texture-force
 * scene sets the map's tempo, music mute and voice sample, others restore the star field;
 * lock-on hit rings; screen filter and star field.
 */

#include "types_gw.h"
#include "color.h"
#include "stg23.h"
#include "debug.h"
#include "callback.h"
#include "console.h"
#include "game.h"
#include "game_flow_editor.h"
#include "gw_varedit.h"
#include "map.h"
#include "map_manager.h"
#include "player.h"
#include "sound_beat.h"
#include "sound_midiseq.h"
#include "sound_pcmstream.h"
#include "stage.h"
#include "task.h"
#include "texforce.h"
#include "stage_base.h"
#include "stage23.h"
#include "../common/eff_circle.h"
#include "../common/eff_star.h"
#include "../common/land_wave.h"
#include "../common/scr_filter.h"

extern unsigned int stage_exit_flags;

extern ListenerDef listener_defs[5];

extern char filter_area_colors[12][20];
extern char filter_last_color[];
extern char filter_vardef[];
extern GwVarDef filter_vardefs[];
extern int *map_work;
extern void *map_menu;
extern void *listener_handles[5];
extern void *map_child;
extern struct TaskDesc map_desc;
extern char str_start_map_shape[];
extern char map_menu_table[];
extern struct TaskDesc map_child_desc;
extern float map_tempos[];
extern char pcm_file[];
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
void listener_lock_hit(HitEv *ev)
{
    EffCircleArg prm;
    HitObj *hit = ev->obj;
    int hi, lo;

    if (hit->hits <= 0 && ((hit->flags & 0x38) >> 3) == 1)
        TextureForce(hit->id, hit->pos);
    prm.pos = hit->pos;
    lo = ev->span->lo;
    hi = ev->span->hi;
    prm.color = console_get_pcolp();
    prm.radius = (hi - lo + 1.0f) * 6.0f + 10.0f;
    prm.width = 0.5f;
    prm.frames = 70;
    prm.fade = 70;
    prm.n = 16;
    eff_circle_create(game_flow_get_current(), &prm);
}

/* 0x00af68c8 */
void listener_area_change(int *ev)
{
    int scene;

    stage_get_buffer();
    scene = map_get_current();
    tfscreen_select(map_manager_get_scene_id(ev[0]));
    game_pass_time_reset();
    if (map_manager_scene_flag(ev[0])) {
        texforce_enable();
        stars_off();
        beat_clock_request_tempo(map_tempos[scene]);
        midiseq_mute_request(scene);
        pcm_play_index(pcm_file, 1, scene);
    } else {
        texforce_disable();
        stars_on();
        player_command_start(0);
    }
}

/* 0x00af6990 */
void filter_vars_add(void)
{
    int group = stage_vars_group_get();

    gw_varedit_add_table(&gw_varedit_root, gw_varedit_add(&gw_varedit_root, group, filter_vardef),
                         filter_vardefs);
}

/* 0x00af69e0 */
void filter_vars_remove(void)
{
    int group = stage_vars_group_get();

    gw_varedit_remove(&gw_varedit_root, group, (struct GwVarDef *)filter_vardef);
}

/* 0x00af6a10 */
void filter_reset(void)
{
    int *w = (int *)stage_get_buffer();
    void *filt = (void *)w[2];

    if (filt && !(TASK_OF(filt)->flags & TASK_F_END))
        scr_filter_fade(filt, filter_area_colors, 0);
}

/* 0x00af6a58 */
void filter_set_area(unsigned int area)
{
    int *w = (int *)stage_get_buffer();

    if (w[2] == 0)
        w[2] = (int)scr_filter_mng_create((void *)w[0]);
    if (area > 11)
        area = 11;
    scr_filter_fade((void *)w[2], filter_area_colors[area], 60);
}

/* 0x00af6ac8 */
void filter_fade_last(void)
{
    int *w = (int *)stage_get_buffer();

    if (w && w[2])
        scr_filter_fade((void *)w[2], filter_last_color, 60);
}

/* 0x00af6b08 */
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

/* 0x00af6b88 */
void stars_off(void)
{
    int *w = (int *)stage_get_buffer();

    if (w && w[1]) {
        do {
            eff_star_col_create((void *)w[1], 0, 30);
        } while (0);
    }
}

/* 0x00af6bc0 */
void change_map(void)
{
    map_menu_next();
}

/* 0x00af6bc8 */
void clear(void)
{
    stage_exit_flags |= 2;
}

/* 0x00af6be0 */
void start_map(void *menu)
{
    int first = map_manager_get_scene(menu);
    int *w = (int *)map_manager_get_work(menu);

    w[2] = map_manager_get_scene(menu);
    map_manager_get_scenes(menu);
    DEBUG_PRINTF(str_start_map, w[2]);
    if (!first && w[3] == 0)
        w[3] = (int)land_wave_mng_create(w);
    map_manager_scene_flag((struct MapManager *)menu);
    map_set_part_radii(map_manager_get_map(menu), map_shapes[w[2]]);
}

/* 0x00af6c98 */
void start_map_shape(void *menu)
{
    int scene = map_manager_get_scene(menu);

    DEBUG_PRINTF(str_start_map_shape, scene);
}

/* 0x00af6cc0 */
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

/* 0x00af6d50 */
void map_release(void)
{
    FINISH_IF(map_menu);
    map_work = 0;
}

/* 0x00af6d78 */
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

/* 0x00af6de8 */
void *map_create(void *w)
{
    task_create_sibling_inherit(w, &map_desc, 16);
    return map_child;
}

/* steer(cast) */
/* 0x00af6e18 */
void map_menu_create(void *parent)
{
    map_menu = ((void *(*)(void *, void *, int, int, void *(*)(void *)))map_manager_task_create_mode2)(parent, map_menu_table, 11, 1, map_create);
}

/* 0x00af6e50 */
void map_menu_next(void)
{
    map_manager_set_next_scene(map_menu, (map_work[2] + 1) % 11);
}

/* 0x00af6e80 */
void map_menu_set_page(int page)
{
    map_manager_set_next_scene(map_menu, page);
}

/* 0x00af6e90 */
void map_menu_set_value(int scene)
{
    map_manager_request_scene(map_menu, scene);
}

/* 0x00af6ea0 */
int map_get_current(void)
{
    return map_work[2];
}

/* 0x00af6eb0 */
void *map_menu_get(void)
{
    return map_menu;
}

/* 0x00af6ec0 */
int *map_get_work(void)
{
    return map_work;
}
