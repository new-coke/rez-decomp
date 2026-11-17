/*
 * Area 2 stage support: rank, boss boot on a bar start, event listeners, screen filter, star
 * field, map scene manager, and ring emitters at stage model nodes.
 */

#include "types_gw.h"
#include "stg02.h"
#include "mathdef.h"
#include "debug.h"
#include "anim.h"
#include "boss_start.h"
#include "callback.h"
#include "color.h"
#include "config.h"
#include "console.h"
#include "game_flow_editor.h"
#include "gw_varedit.h"
#include "heap.h"
#include "map.h"
#include "map_manager.h"
#include "math_util.h"
#include "node_draw_state.h"
#include "player.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "stage.h"
#include "task.h"
#include "texforce.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "stage_base.h"
#include "stage02.h"
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
extern struct TaskDesc circle_desc;
extern void *map_child;
extern struct TaskDesc map_desc;

/* 32 bytes: count cycles over 450 frames, a new ring every 30 */
typedef struct CircleRing {
    Vec3 pos;
    int count;                  /* 0x0c */
    float scale;                /* 0x10: of the ring radius */
    int scene;                  /* 0x14: not drawn while the map scene flag equals it */
} CircleRing;

typedef struct CircleWork {
    char pad_00[8];
    CircleRing ring;                   /* 0x08 */
} CircleWork;

extern struct TaskDesc boss_boot_desc;
extern void *circle_mng_work;
extern struct TaskDesc circle_mng_desc;
extern struct TaskDesc map_child_desc;

/* per map scene (circle_places), 32 bytes */
typedef struct CircleMngPlace {
    int node;                   /* byte offset of the node list in the stage model */
    int n;                      /* 0x04: rings per node */
    Vec3 step;                  /* 0x08: between the rings */
    float scale;                /* 0x14 */
    char pad_18[4];
    int scene;                  /* 0x1c */
} CircleMngPlace;

extern CircleMngPlace circle_places[];
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
}

/* 0x00af6798 */
void boss_boot_exec(int *w)
{
    int prm[2];

    if (w[1] == 0) {
        if (beat_grid_hit(0) && (beat_clock_get_haku() & 3) == 0) {
            music_change(3);
            beat_clock_request_tempo(145.0f);
            midiseq_mute_request(40);
            midiseq_play();
            map_menu_set_page(21);
            texforce_disable();
            w[1]++;
        }
    } else if (--w[0] <= 0) {
        stage_data_free();
        stars_off();
        prm[0] = 1;
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
    w[0] = 320;
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

/* 0x00af6a70 */
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

/* 0x00af6ae8 */
void filter_vars_add(void)
{
    int group = stage_vars_group_get();

    gw_varedit_add_table(&gw_varedit_root, gw_varedit_add(&gw_varedit_root, group, filter_vardef),
                         filter_vardefs);
}

/* 0x00af6b38 */
void filter_vars_remove(void)
{
    int group = stage_vars_group_get();

    gw_varedit_remove(&gw_varedit_root, group, (struct GwVarDef *)filter_vardef);
}

/* 0x00af6b68 */
void filter_reset(void)
{
    int *w = (int *)stage_get_buffer();
    void *filt = (void *)w[2];

    if (filt && !(TASK_OF(filt)->flags & TASK_F_END))
        scr_filter_fade(filt, filter_area_colors, 0);
}

/* 0x00af6bb0 */
void filter_set_area(unsigned int area)
{
    int *w = (int *)stage_get_buffer();

    if (w[2] == 0)
        w[2] = (int)scr_filter_mng_create((void *)w[0]);
    if (area > 11)
        area = 11;
    scr_filter_fade((void *)w[2], filter_area_colors[area], 60);
}

/* 0x00af6c20 */
void filter_fade_last(void)
{
    int *w = (int *)stage_get_buffer();

    if (w && w[2])
        scr_filter_fade((void *)w[2], filter_last_color, 60);
}

/* 0x00af6c60 */
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

/* 0x00af6ce0 */
void stars_off(void)
{
    int *w = (int *)stage_get_buffer();

    if (w && w[1]) {
        do {
            eff_star_col_create((void *)w[1], 0, 30);
        } while (0);
    }
}

/* 0x00af6d18 */
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
    case 8:
    case 9:
        map_set_sort_type(map_manager_get_map(menu), 3);
        break;
    case 21:
        map_set_sort_type(map_manager_get_map(menu), 1);
        map_obj_set_params(menu, 0, 0);
        break;
    }
    map_manager_scene_flag((struct MapManager *)menu);
    map_set_part_radii(map_manager_get_map(menu), map_shapes[w[2]]);
}

/* 0x00af6e30 */
void start_map_shape(void *menu)
{
    int scene = map_manager_get_scene(menu);

    DEBUG_PRINTF(str_start_map_shape, scene);
}

/* 0x00af6e58 */
void map_init(int *w)
{
    void **child = &map_child;

    *child = task_create_inherit(w, &map_child_desc, 8);
    map_work = w;
    circle_mng_create(w);
    w[3] = 0;
    map_manager_set_work(map_menu, w);
    map_manager_swap_start_cb(map_menu, start_map_shape);
    map_manager_swap_change_cb(map_menu, start_map);
    map_manager_set_sub_task(map_menu, w);
    map_manager_start_scene(map_menu, 0);
}

/* 0x00af6ef8 */
void map_release(void)
{
    FINISH_IF(map_menu);
    map_work = 0;
}

/* 0x00af6f20 */
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

/* 0x00af6f90 */
void *map_create(void *w)
{
    task_create_sibling_inherit(w, &map_desc, 16);
    return map_child;
}

/* 0x00af6fc0 */
void map_menu_create(void *parent)
{
    map_menu = map_manager_task_create_select(parent, map_menu_table, 6, 22, 1, map_create);
}

/* 0x00af7000 */
void map_menu_next(void)
{
    map_manager_set_next_scene(map_menu, (map_work[2] + 1) % 22);
}

/* 0x00af7030 */
void map_menu_set_page(int page)
{
    map_manager_set_next_scene(map_menu, page);
}

/* 0x00af7040 */
void map_menu_set_value(int scene)
{
    map_manager_request_scene(map_menu, scene);
}

/* 0x00af7050 */
int map_get_current(void)
{
    return map_work[2];
}

/* 0x00af7060 */
void *map_menu_get(void)
{
    return map_menu;
}

/* 0x00af7070 */
void *map_get_work(void)
{
    return map_work;
}

/* 0x00af7080 */
void circle_init(CircleWork *w, CircleRing *ring)
{
    w->ring = *ring;
    task_set_dlist(w, 4);
}

/* 0x00af70b8 */
void circle_release(void)
{
}

/* 0x00af70c0 */
void circle_exec(CircleWork *w)
{
    CircleRing *ring = &w->ring;

    ring->count = (ring->count + 1) % 450;
}

/* 0x00af70e8 */
void circle_disp(CircleWork *w)
{
    float sc[2];
    CircleRing *ring = &w->ring;
    void *menu;
    unsigned int col;
    float frm, rad, alpha;
    int i, cnt;

    menu = map_menu_get();
    if (map_manager_scene_flag((struct MapManager *)menu) == ring->scene)
        return;
    for (i = 2, cnt = ring->count; i != -1; i--) {
        frm = cnt;
        if (frm < 360.0f) {
            rad = frm / 360.0f;
            ulSinCos((int)(rad * ANG_QUARTER), &sc[0], &sc[1]);
            ulSinCos((int)(sc[0] * ANG_TURN - ANG_QUARTER), &sc[0], &sc[1]);
            map_manager_get_scenes(menu);
            map_manager_get_scene(menu);
            alpha = (sc[0] + 1.0f) * 0.5f * 255.0f;
            col = ((int)MIN(MAX_GE(alpha, 0.0f), 255.0f) + 1) >> 1 << 24 | 0x9a2f55;
            draw_set_blend(4, 5);
            rad *= ring->scale;
            do {
                ulMatrixPush();
            } while (0);
            vu0_matrix_translate(ulMatrixCur, ring->pos.x, ring->pos.y, ring->pos.z);
            draw_ring_xz(rad, col, 16);
            ulMatrixPop();
        }
        cnt = frm - 30.0f;
        if (cnt < 0)
            break;
    }
}

/* 0x00af72e8 */
void *circle_create(void *parent, void *ring)
{
    return task_create_inherit_arg(parent, &circle_desc, 32, ring);
}

/* 0x00af7310 */
void circle_mng_spawn_nodes(unsigned int *node, int n, Vec3 *step, int scene, float scale)
{
    CircleRing ring;
    int i;

    do {
        ulMatrixPush();
        anim_apply_node(node);
        ulMatrixGetTranslation((float *)ulMatrixCur, (float *)&ring.pos);
        ring.count = (int)(RAND_FLOAT_A() * 450.0f);
        ring.scale = scale;
        ring.scene = scene;
        for (i = 0; i < n; i++) {
            circle_create(circle_mng_work, &ring);
            v3_add(&ring.pos, step);
        }
        if (node[11])
            circle_mng_spawn_nodes((unsigned int *)node[11], n, step, scene, scale);
        ulMatrixPop();
        node = (unsigned int *)node[12];
    } while (node);
}

/* 0x00af7470 */
void circle_mng_set_scene(int ev)
{
    CircleMngPlace *place = &circle_places[map_manager_get_scene(*(void **)ev)];

    if (place->node != ((int *)circle_mng_work)[2]) {
        task_end_children(circle_mng_work);
        ((int *)circle_mng_work)[2] = place->node;
        if (place->node) {
            anim_start(0, 0.0f);
            ulMatrixPush();
            matrix_unit_set(0);
            circle_mng_spawn_nodes(*(unsigned int **)(*(int *)circle_mng_work + place->node + 44), place->n, &place->step, place->scene, place->scale);
            ulMatrixPop();
        }
    }
}

/* 0x00af7538 */
void circle_mng_init(int *w)
{
    w[2] = 0;
    circle_mng_work = w;
    w[3] = (int)listener_add(24, circle_mng_set_scene);
}

/* 0x00af7578 */
void circle_mng_release(int *w)
{
    listener_remove(24, (void *)w[3]);
    circle_mng_work = 0;
}

/* 0x00af75a0 */
void circle_mng_exec(void)
{
}

/* 0x00af75a8 */
void circle_mng_disp(void)
{
}

/* 0x00af75b0 */
void *circle_mng_create(void *parent)
{
    return task_create_inherit(parent, &circle_mng_desc, 16);
}
