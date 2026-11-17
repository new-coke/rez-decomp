/*
 * Stage 6 support: rank from the shot-down rate, boss boot (fifth boss after 20 frames, with its
 * music and tempo), single-scene map manager.
 */

#include "stg06.h"
#include "debug.h"
#include "boss_start.h"
#include "config.h"
#include "map.h"
#include "map_manager.h"
#include "sound_beat.h"
#include "sound_midiseq.h"
#include "task.h"
#include "texforce.h"
#include "stage_base.h"

extern int *map_work;
extern void *map_menu;
extern struct TaskDesc boss_boot_desc;

extern float *map_shapes[];

extern char map_menu_table[];

/* "=====[ StartMap %d ]=====\n" */
extern char str_start_map[];

/* "=====[ StartMapShape %d ]=====\n" */
extern char str_start_map_shape[];

extern struct TaskDesc map_child_desc;

extern struct TaskDesc map_desc;
extern void *map_child;

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
void boss_boot_exec(int *w)
{
    int prm[2];

    if (--w[0] <= 0) {
        prm[0] = 4;
        prm[1] = stage_rating();
        boss_start_create(prm);
        FINISH_IF(w);
    }
}

/* 0x00af67b0 */
int *boss_boot_create(void *parent)
{
    int *w = task_create(parent, &boss_boot_desc, 4);

    w[0] = 20;
    music_change(7);
    beat_clock_request_tempo(140.0f);
    midiseq_mute_request(40);
    midiseq_play();
    texforce_disable();
    return w;
}

/* 0x00af6820 */
void start_map(void *menu)
{
    int *w = (int *)map_manager_get_work(menu);

    w[2] = map_manager_get_scene(menu);
    DEBUG_PRINTF(str_start_map, w[2]);
    map_set_part_radii(map_manager_get_map(menu), map_shapes[w[2]]);
}

/* 0x00af6890 */
void start_map_shape(void *menu)
{
    int scene = map_manager_get_scene(menu);

    DEBUG_PRINTF(str_start_map_shape, scene);
}

/* 0x00af68b8 */
void map_init(int *w)
{
    map_child = task_create_inherit(w, &map_child_desc, 8);
    map_work = w;
    map_manager_set_work(map_menu, w);
    map_manager_swap_start_cb(map_menu, start_map_shape);
    map_manager_swap_change_cb(map_menu, start_map);
    map_manager_set_sub_task(map_menu, w);
    map_manager_start_scene(map_menu, 0);
    map_obj_set_params(map_menu, 0, 0);
}

/* 0x00af6958 */
void map_release(void)
{
    FINISH_IF(map_menu);
    map_work = 0;
}

/* 0x00af6980 */
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

/* 0x00af69f0 */
void *map_create(void *w)
{
    task_create_sibling_inherit(w, &map_desc, 12);
    return map_child;
}

/* steer(cast) */
/* 0x00af6a20 */
void map_menu_create(void *parent)
{
    map_menu = ((void *(*)(void *, void *, int, int, void *(*)(void *)))map_manager_task_create_mode2)(parent, map_menu_table, 1, 1, map_create);
}

/* 0x00af6a58 */
void map_menu_next(void)
{
    map_manager_set_next_scene(map_menu, (map_work[2] + 1) % 1);
}

/* 0x00af6a68 */
void map_menu_set_page(int page)
{
    map_manager_set_next_scene(map_menu, page);
}

/* 0x00af6a78 */
void map_menu_set_value(int scene)
{
    map_manager_request_scene(map_menu, scene);
}

/* 0x00af6a88 */
int map_get_current(void)
{
    return map_work[2];
}

/* 0x00af6a98 */
void *map_menu_get(void)
{
    return map_menu;
}

/* 0x00af6aa8 */
int *map_get_work(void)
{
    return map_work;
}
