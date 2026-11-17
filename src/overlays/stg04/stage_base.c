/*
 * Area 4 stage support: rank, boss boot on a bar start, event listeners, screen filter, star
 * field, map scene manager, and a callback fading far map objects with depth.
 */

#include "types_gw.h"
#include "color.h"
#include "stg04.h"
#include "mathdef.h"
#include "debug.h"
#include "boss_start.h"
#include "callback.h"
#include "camera.h"
#include "config.h"
#include "fog.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "map.h"
#include "map_manager.h"
#include "model.h"
#include "node_draw_state.h"
#include "player.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "stage.h"
#include "task.h"
#include "texforce.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "stage_base.h"
#include "stage04.h"
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
extern struct TaskDesc boss_boot_desc;
extern char str_start_map[];
extern char str_start_map_shape[];
extern struct TaskDesc map_child_desc;
extern struct TaskDesc map_desc;
extern float *map_shapes[];
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
void stage_data_free(void)
{
    int *w = (int *)stage_get_buffer();

    se_laser_bank_clear();
    heap_free((void *)w[4], 0);
    w[4] = 0;
}

/* 0x00af6798 */
void boss_boot_exec(int *w)
{
    int prm[2];

    if (w[1] == 0) {
        if (beat_grid_hit(0) && (beat_clock_get_haku() & 3) == 0) {
            music_change(5);
            beat_clock_request_tempo(140.0f);
            midiseq_mute_request(40);
            midiseq_play();
            map_menu_set_page(21);
            texforce_disable();
            w[1]++;
        }
    } else if (--w[0] <= 0) {
        stage_data_free();
        stars_off();
        prm[0] = 3;
        prm[1] = stage_rating();
        do {
            boss_start_create(prm);
        } while (0);
        FINISH(w);
    }
}

/* 0x00af6870 */
int *boss_boot_create(void *parent)
{
    int *w = task_create(parent, &boss_boot_desc, 8);

    w[1] = 0;
    w[0] = 200;
    return w;
}

/* 0x00af68a8 */
void listeners_add(void)
{
    unsigned int i;

    for (i = 0; i < 5; i++)
        listener_handles[i] = listener_add(listener_defs[i].idx, listener_defs[i].fn);
}

/* 0x00af6910 */
void listeners_remove(void)
{
    unsigned int i;

    for (i = 0; i < 5; i++)
        listener_remove(listener_defs[i].idx, listener_handles[i]);
}

/* 0x00af6978 */
void listener_stars_on(void)
{
    stars_on();
}

/* 0x00af6980 */
void listener_next_map(void)
{
    map_menu_next();
}

/* 0x00af6988 */
void listener_mute(int *mute)
{
    midiseq_mute_request(*mute);
}

/* 0x00af6990 */
void listener_lock_hit(HitEv *ev)
{
    HitObj *hit = ev->obj;

    if (hit->hits <= 0 && ((hit->flags & 0x38) >> 3) == 1)
        TextureForce(hit->id, hit->pos);
}

/* 0x00af69c8 */
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

/* 0x00af6a40 */
void filter_vars_add(void)
{
    int group = stage_vars_group_get();

    gw_varedit_add_table(&gw_varedit_root, gw_varedit_add(&gw_varedit_root, group, filter_vardef),
                         filter_vardefs);
}

/* 0x00af6a90 */
void filter_vars_remove(void)
{
    int group = stage_vars_group_get();

    gw_varedit_remove(&gw_varedit_root, group, (struct GwVarDef *)filter_vardef);
}

/* 0x00af6ac0 */
void filter_reset(void)
{
    int *w = (int *)stage_get_buffer();
    void *filt = (void *)w[2];

    if (filt && !(TASK_OF(filt)->flags & TASK_F_END))
        scr_filter_fade(filt, filter_area_colors, 0);
}

/* 0x00af6b08 */
void filter_set_area(unsigned int area)
{
    int *w = (int *)stage_get_buffer();

    if (w[2] == 0)
        w[2] = (int)scr_filter_mng_create((void *)w[0]);
    if (area > 11)
        area = 11;
    scr_filter_fade((void *)w[2], filter_area_colors[area], 60);
}

/* 0x00af6b78 */
void filter_fade_last(void)
{
    int *w = (int *)stage_get_buffer();

    if (w && w[2])
        scr_filter_fade((void *)w[2], filter_last_color, 60);
}

/* 0x00af6bb8 */
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

/* 0x00af6c38 */
void stars_off(void)
{
    int *w = (int *)stage_get_buffer();

    if (w && w[1]) {
        do {
            eff_star_col_create((void *)w[1], 0, 30);
        } while (0);
    }
}

/* 0x00af6c70 */
void start_map(void *menu)
{
    int *w = (int *)map_manager_get_work(menu);

    w[2] = map_manager_get_scene(menu);
    DEBUG_PRINTF(str_start_map, w[2]);
    switch (w[2]) {
    case 0:
        if (w[3] == 0)
            w[3] = (int)land_wave_mng_create(w);
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 12:
    case 13:
    case 14:
    case 15:
        map_set_sort_type(map_manager_get_map(menu), 1);
        break;
    case 21:
        map_obj_set_params(menu, 0, 0);
        break;
    }
    map_set_part_radii(map_manager_get_map(menu), map_shapes[w[2]]);
}

/* 0x00af6d50 */
void start_map_shape(void *menu)
{
    int scene = map_manager_get_scene(menu);

    DEBUG_PRINTF(str_start_map_shape, scene);
}

/* 0x00af6d78 */
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

/* 0x00af6e08 */
void map_release(void)
{
    FINISH_IF(map_menu);
    map_work = 0;
}

/* 0x00af6e30 */
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

/* 0x00af6ea0 */
void *map_create(void *w)
{
    task_create_sibling_inherit(w, &map_desc, 16);
    return map_child;
}

/* 0x00af6ed0 */
void map_menu_create(void *parent)
{
    map_menu = map_manager_task_create_select(parent, map_menu_table, 6, 22, 1, map_create);
}

/* 0x00af6f10 */
void map_menu_next(void)
{
    map_manager_set_next_scene(map_menu, (map_work[2] + 1) % 22);
}

/* 0x00af6f40 */
void map_menu_set_page(int page)
{
    map_manager_set_next_scene(map_menu, page);
}

/* 0x00af6f50 */
void map_menu_set_value(int scene)
{
    map_manager_request_scene(map_menu, scene);
}

/* 0x00af6f60 */
int map_get_current(void)
{
    return map_work[2];
}

/* 0x00af6f70 */
void *map_menu_get(void)
{
    return map_menu;
}

/* 0x00af6f80 */
int *map_get_work(void)
{
    return map_work;
}

/* steer(cast) */
/* 0x00af6f90 */
void draw_cb_depth_fade(int *w)
{
    ModelPart *gfx = (ModelPart *)w[1];
    float r = gfx->radius;
    Vec3 v;
    Vec3 eye;
    Vec3 *scl;
    float rgba[4];
    float fade, dep;
    int save, blend;

    if (w[13])
        return;
    camera_get_eye(&eye);
    r *= MAX_GE(((Vec3 *)w[12])->z, MAX_GE(((Vec3 *)w[12])->x, ((Vec3 *)w[12])->y));
    if (!view_sphere_visible((Vec3 *)w[10], r))
        return;
    ulMatrixPush();
    do {
        Vec3 *p_ = (Vec3 *)w[10];

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    draw_cb_rotate_zyx((int *)w[11]);
    ulMatrixGetTranslation((float *)ulMatrixCur, &v.x);
    dep = -v.z - r;
    if (dep <= 10000.0f) {
        fade = dep / 10000.0f;
        fade = MIN(MAX_GE(1.0f - fade, 0.0f), 1.0f);
        save = (int)fog_set_enable(0);
        fade -= 1.0f;
        blend = 0;
        if (!(fabsf(fade) < FLT_EPSILON)) {
            blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
            rgba[0] = fade;
            rgba[1] = rgba[2] = rgba[3] = 0.0f;
            draw_push_strip_flags();
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
            draw_push_override_color();
            draw_set_override_color(rgba);
        }
        scl = (Vec3 *)w[12];
        vu0_matrix_scale(ulMatrixCur, scl->x, scl->y, scl->z);
        ulGxSetTextureList((void *)w[18]);
        dc_object_draw(gfx);
        if (!(fabsf(fade) < FLT_EPSILON)) {
            draw_pop_override_color();
            draw_pop_strip_flags();
            draw_set_override_flags(blend);
        }
        fog_set_enable((void *)save);
    }
    ulMatrixPop();
}

/* 0x00af7208 */
void draw_cb_rotate_zyx(int *ang)
{
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
}
