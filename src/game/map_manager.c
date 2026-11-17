/*
 * Map manager: steps a stage area through its map scenes (model, fog, background, camera, player
 * cell) in time with the bar, switching behind a wipe or on request. Shares wire data between
 * scene models, draws the backdrop and types the "layer level" title.
 */

#include "types_game.h"
#include "task.h"
#include "mathdef.h"
#include "debug.h"
#include "libc.h"
#include "ul_gx.h"
#include "map_manager.h"
#include "billboard.h"
#include "callback.h"
#include "camera.h"
#include "color.h"
#include "config.h"
#include "fog.h"
#include "game.h"
#include "map.h"
#include "math_util.h"
#include "model.h"
#include "node_draw_state.h"
#include "pad.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "stage.h"
#include "system.h"
#include "yuki_tool.h"
#include "game_flow_editor.h"

typedef struct MapManagerDesc {
    void *scenes;               /* entry table */
    int nscene, use_wires, parent, parent_cb, map_mode;
} MapManagerDesc;

typedef struct MapManagerView {
    int data, tex;
    void *obj;
    int wipe_task;
    void *model;
    int parent;
    void *sub_task;
    void *scenes;
    int nscene;
    int cur;
    int cell_x, cell_y;
    int state;
    float frame;
    int next;
    int auto_wait, auto_change;
    int change_req;
    void (*start_cb)(struct MapManagerView *);
    void (*change_cb)(struct MapManagerView *);
    int parent_cb;
    int work;
    int nwire;
    void *wires;
    void *map_mode;
    int title_on;
    int frames;
    int title_col;
} MapManagerView;

typedef struct MapManagerParentCbView {
    char pad_00[20];
    int parent;
    char pad_18[36];
    int auto_wait;                /* +0x3c */
    char pad_40[16];
    int (*parent_cb)(void *);      /* +0x50: given the task */
} MapManagerParentCbView;

extern struct TaskDesc cam_ang_desc;
extern char map_manager_title[];
extern int map_manager_title_len;
extern int wipe_is_done(int);
extern int wipe_is_covered(int);
extern void wipe_finish(int, int, int);
extern int wipe_start(int, int, int);
extern PadSystem pad_system;
extern int map_manager_sort_type;
extern int debug_disp_level;
extern int map_manager_debug_wait;
extern ScreenGeom screen_layout;
extern void font_set(int font);
extern struct TaskDesc map_manager_desc;
extern struct TaskDesc map_manager_desc2;

static void map_manager_next_scene(MapManager *w);
static int map_manager_get_wire_id(MapManager *w);
static int map_manager_get_real_id(MapManager *w);

/* prm: steps, start fov, end fov. */
/* 0x00134860 */
static void cam_ang_task_init(CamAngWork *w, int *prm)
{
    w->step = 0;
    w->total = prm[0];
    w->start = prm[1];
    w->end = prm[2];
}

/* One step per frame, then ends. */
/* 0x00134880 */
static void cam_ang_task_exec(CamAngWork *w)
{
    camera_set_fov_direct((w->end - w->start) * w->step++ / w->total + w->start);
    if (w->step > w->total) {
        TASK_OF(w)->flags |= TASK_F_END;
    }
}

/* None (0) when steps is 0. */
/* 0x001348f8 */
static int cam_ang_task_create(void *parent, int steps, int start, int end)
{
    int arg[3];

    arg[0] = steps;
    arg[1] = start;
    arg[2] = end;
    if (steps > 0) {
        return task_create_arg(parent, &cam_ang_desc, 112, arg);
    }
    return 0;
}

/* Cell entries from the area data; a negative coordinate clears the cell move. */
/* 0x00134940 */
static void map_manager_set_cell(MapManager *w, int x, int y)
{
    int *cell_x1, *cell_y1, *cell_x2, *cell_y2, *cell_x0, *cell_y0;

    if (x < 0 || y < 0) {
        player_cell_move_clear();
    } else {
        cell_x1 = pnb_get_entry(w->data, x + 1);
        cell_y1 = pnb_get_entry(w->data, y + 1);
        cell_x2 = pnb_get_entry(w->data, x + 2);
        cell_y2 = pnb_get_entry(w->data, y + 2);
        cell_x0 = pnb_get_entry(w->data, x);
        cell_y0 = pnb_get_entry(w->data, y);
        player_cell_move_start(*cell_x1, *cell_y1, *cell_x2, *cell_y2, *cell_x0, *cell_y0);
    }
    w->cell_x = x;
    w->cell_y = y;
}

/* Among the first n scenes. */
/* 0x00134a38 */
static int map_manager_model_seen(MapManager *w, int key, int n)
{
    MapScene *scene = w->scenes;
    int found = 0;
    int i;

    for (i = 0; i < n; i++, scene++) {
        if (key == scene->model) {
            found = 1;
            break;
        }
    }
    return found;
}

/* 0 if none. */
/* 0x00134a78 */
static MapPair *map_manager_find_wire(MapManager *w, int key)
{
    MapPair *wire = w->wires;
    int i;

    for (i = 0; i < w->nwire; i++, wire++) {
        if (key == wire->val) {
            return wire;
        }
    }
    return 0;
}

/* Once per distinct part data, so models sharing part data share wire data. */
/* 0x00134ab8 */
static void map_manager_build_wire_table(MapManager *w)
{
    int i, j;
    MapScene *ent;
    MapModelInfo *model;
    MapAnim *anim;
    MapPair *wire;

    w->nwire = 0;
    ent = w->scenes;
    for (i = 0; i < w->nscene; i++, ent++) {
        MapModelInfo *count_model = ent->model;
        if (count_model) {
            if (!map_manager_model_seen(w, (int)count_model, i)) {
                MapAnim *count_anm = count_model->anm;
                int k;

                for (k = 0; k < count_model->npart; k++, count_anm++) {
                    w->nwire += count_anm->has_wire;
                }
            }
        }
    }
    if (w->nwire == 0) {
        do { w->wires = 0; } while (0);
        return;
    }
    w->wires = yuki_alloc(w->nwire * 8);
    w->nwire = 0;
    ent = w->scenes;
    for (i = 0; i < w->nscene; i++, ent++) {
        MapModelInfo *fill_model = ent->model;
        if (fill_model) {
            if (!map_manager_model_seen(w, (int)fill_model, i)) {
                anim = fill_model->anm;
                for (j = 0; j < fill_model->npart; j++, anim++) {
                    if (anim->has_wire) {
                        if (!map_manager_find_wire(w, anim->node_ofs)) {
                            int base = w->data;
                            wire = &w->wires[w->nwire++];
                            wire->val = anim->node_ofs;
                            wire->key = model_load_wire_data(base + anim->node_ofs);
                        }
                    }
                }
            }
        }
    }
}

/* 0x00134cb0 */
static void map_manager_free_wire_table(MapManager *w)
{
    MapPair *wire;
    int i;

    if (w->nwire) {
        wire = w->wires;
        for (i = 0; i < w->nwire; i++, wire++) {
            model_free_block(wire->key);
        }
        w->nwire = 0;
        do {
            yuki_free(w->wires);
            w->wires = 0;
        } while (0);
    }
}

/* Also the current scene's colour. */
/* 0x00134d30 */
static void map_manager_bind_wires(MapManager *w, MapModelInfo *model)
{
    MapAnim *anim = model->anm;
    int col = w->scenes[w->cur].color;
    int i;

    for (i = 0; i < model->npart; i++, anim++) {
        if (anim->has_wire) {
            map_part_set_wire(w->obj, i, map_manager_find_wire(w, anim->node_ofs)->key);
            map_part_set_color(w->obj, i, col);
        }
    }
}

/* 0x00134df0 */
static void map_manager_unbind_wires(MapManager *own, MapModelInfo *model)
{
    int i;
    MapAnim *anim;

    anim = model->anm;
    for (i = 0; i < model->npart; i++, anim++) {
        if (anim->has_wire) {
            map_part_set_wire(own->obj, i, 0);
        }
    }
}

/*
 * Same model: keeps the map, rebinds animations; else a new map task. Applies the scene settings,
 * message 24, fov. Flag 1 clear: fov sweep and "layer level" title in the stage's map colour.
 */
/* steer(cast) */
/* 0x00134e68 */
static void map_manager_change_scene(MapManager *w)
{
    MapScene *cur;
    MapScene *scene;
    MapModelInfo *model;
    MapModelInfo *old;
    int idx;
    MapManager *arg;

    if (w->cur >= 0) {
        cur = &w->scenes[w->cur];
    } else {
        cur = 0;
    }
    idx = w->next;
    old = w->model;
    w->next = -1;
    w->cur = idx;
    scene = &w->scenes[idx];
    model = scene->model;
    if (model == old) {
        if (model != 0) {
            map_rebind_anims_mode2(w->obj);
        }
    } else {
        if (w->obj) {
            map_manager_unbind_wires(w, old);
            map_parts_reset_time(w->obj);
            do {
                if (w->obj) {
                    TASK_OF(w->obj)->flags |= TASK_F_END;
                }
            } while (0);
            w->obj = 0;
        }
        if (model) {
            if (w->map_mode < 2) {
                if (w->nwire == 0) {
                    w->obj = (MapObj *)((int (*)(void *, MapModelInfo *))map_task_create_mode1)((void *)w->parent, model);
                } else {
                    w->obj = (MapObj *)((int (*)(void *, MapModelInfo *))map_task_create_mode1_wires)((void *)w->parent, model);
                }
            } else {
                if (w->nwire == 0) {
                    w->obj = (MapObj *)map_task_create_mode2((void *)w->parent, model);
                } else {
                    w->obj = (MapObj *)((int (*)(void *, MapModelInfo *))map_task_create_mode2_wires)((void *)w->parent, model);
                }
            }
        }
    }
    if (model) {
        map_manager_bind_wires(w, model);
        if (scene->flags & 2) {
            map_set_far_fade(w->obj, 1);
        } else {
            map_set_far_fade(w->obj, 0);
        }
        if (scene->flags & 4) {
            map_set_far_clip(w->obj, 0);
        } else {
            map_set_far_clip(w->obj, 1);
        }
    }
    w->model = model;
    w->frame = scene->frame_start;
    do { ((void (*)(int, float, float, int))fog_set)(scene->fog_color, scene->fog_near, scene->fog_far, scene->fog_mode); } while (0);
    fog_set_enable(1);
    BackColReq(scene->bg_color);
    camera_set_far(scene->cam_scale);
    draw_set_color(scene->color);
    if (scene->auto_wait > 0) {
        w->auto_wait = scene->auto_wait;
    }
    if (w->cell_x != scene->cell_x || w->cell_y != scene->cell_y) {
        if (scene->cell_mode != 1) {
            if (scene->cell_mode == 0)
                map_manager_set_cell(w, scene->cell_x, scene->cell_y);
        } else {
            player_cell_move_clear();
            map_manager_set_cell(w, scene->cell_x, scene->cell_y);
        }
    }
    w->change_req = 0;
    if (w->change_cb) {
        w->change_cb(w);
    }
    arg = w;
    listener_dispatch(24, &arg);
    camera_set_fov_direct(scene->fov);
    if (map_manager_scene_flag(w) == 0 && cur != 0) {
        int *title_len = &map_manager_title_len;

        cam_ang_task_create(w, scene->auto_wait + scene->wipe_end_len, scene->fov, cur->fov);
        sprintf(map_manager_title, "layer level %02d", map_manager_get_scene_id(w) + 1);
        *title_len = line_length(map_manager_title);
        w->title_col = stage_get_map_col(stage_get_num());
        w->frames = 0;
    }
}

/* Starts at scene 0, auto change on; shared wire data when use_wires. */
/* 0x00135170 */
static void map_manager_task_init(MapManagerView *w, MapManagerDesc *desc)
{
    int has;

    w->nscene = desc->nscene;
    w = (MapManagerView *)((char *)w + (((int)w & 1) >> 1)); /* steer(zero) */
    w->auto_change = 1;
    w->cell_x = -1;
    w->cur = -1;
    w->next = 0;
    w = (MapManagerView *)((char *)w + (((int)w & 1) >> 1)); /* steer(zero) */
    w->cell_y = -1;
    w->state = 0;
    w->auto_wait = 0;
    w->change_req = 0;
    w->obj = 0;
    w->map_mode = (void *)desc->map_mode;
    w->title_on = 1;
    {
        int parent = desc->parent;
        void *scenes = desc->scenes;
        int parent_cb = desc->parent_cb;

        has = desc->use_wires;
        w->scenes = scenes;
        w->parent = parent;
        w->parent_cb = parent_cb;
    }
    w->model = 0;
    w->start_cb = 0;
    w->change_cb = 0;
    w->sub_task = 0;
    if (has) {
        map_manager_build_wire_table(w);
    } else {
        w->nwire = 0;
        w->wires = 0;
    }
}

/* Ends the map, wipe and sub task. */
/* 0x001351f8 */
static void map_manager_task_release(MapManager *w)
{
    if (w->obj) {
        map_manager_unbind_wires(w, w->model);
        TASK_END(w->obj);
        w->obj = 0;
    }
    TASK_END(w->wipe_task);
    TASK_END(w->sub_task);
    do {
    } while (0);
    map_manager_free_wire_table(w);
}

/* State 3: wipe finished -> idle. */
/* 0x00135280 */
static void map_manager_state_wipe_end(MapManagerView *w)
{
    if (wipe_is_done(w->wipe_task)) {
        w->state = 0;
        if (w->wipe_task) {
            TASK_OF(w->wipe_task)->flags |= TASK_F_END;
            w->wipe_task = 0;
        }
    }
}

/* State 2: screen covered -> switch scene, second half of the wipe (state 3). */
/* 0x001352d0 */
static void map_manager_state_wipe_cover(MapManager *w)
{
    MapScene *scene;

    if (wipe_is_covered(w->wipe_task)) {
        scene = &w->scenes[w->cur];
        map_manager_change_scene(w);
        w->state = 3;
        wipe_finish(w->wipe_task, scene->wipe_end_len, scene->wipe_end_arg);
    }
}

/*
 * State 1. Next scene has flag 1: one frame per frame, wipe at the end. Else the frame follows the
 * bar; switch on a bar's first beat, wipe started early enough to cover it.
 */
/* 0x00135348 */
static void map_manager_state_advance(MapManager *w)
{
    MapScene *scene;
    MapScene *next_scene;
    float end, start, t, next_frame, bar;
    int n;

    scene = &w->scenes[w->cur];
    next_scene = &w->scenes[w->next];
    end = scene->frame_end;
    if (next_scene->flags & 1) {
        next_frame = w->frame + 1.0f;
        t = end;
        if (next_frame < end) {
            t = next_frame;
        }
        w->frame = t;
        if (fabsf(t - end) < FLT_EPSILON) {
            w->state = 2;
            w->wipe_task = wipe_start((int)*(void **)&scene->wipe_type, scene->wipe_len, scene->wipe_col);
        }
    } else {
        bar = beat_clock_get_bar();
        start = scene->frame_start;
        if (beat_grid_hit(0)) {
            if ((beat_clock_get_haku() & 3) == 0) {
                w->state = 2;
                bar = 1.0f;
            }
        }
        w->frame = (end - start) * bar + start;
        if (w->wipe_task == 0) {
            n = (int)(beat_clock_get_interval() * 4.0f * (1.0f - bar));
            if (scene->wipe_len >= n) {
                w->wipe_task = wipe_start((int)scene->wipe_type, n + 1, scene->wipe_col);
            }
        }
    }
}

/* State 4: on player status flag 8 -> state 1 and the start callback. */
/* 0x001354c0 */
static void map_manager_state_wait(MapManager *w)
{
    if (player_status_test_flags(8)) {
        w->state = 1;
        if (w->obj) {
            map_rebind_anims(w->obj);
        }
        if (w->start_cb) {
            w->start_cb(w);
        }
    }
}

/*
 * State 0. No next scene: auto-change wait, then the following one. Else state 1, or for cell
 * mode 2 first the scene's cell and state 4.
 */
/* 0x00135518 */
static void map_manager_state_idle(MapManager *w)
{
    MapScene *scene;

    if (w->next < 0) {
        if (w->auto_change != 0) {
            if (w->auto_wait > 0) {
                if (--w->auto_wait <= 0) {
                    map_manager_next_scene(w);
                }
            }
        }
    } else {
        scene = &w->scenes[w->next];
        switch (scene->cell_mode) {
        case 2:
            map_manager_set_cell(w, scene->cell_x, scene->cell_y);
            w->state = 4;
            break;
        case 0:
        case 1:
            w->state = 1;
            if (w->obj) {
                map_rebind_anims(w->obj);
            }
            if (w->start_cb) {
                w->start_cb(w);
            }
            break;
        }
    }
}

/* Debug flag 8: key A toggles auto change; two pad 0 buttons step the scene and cycle sort type. */
/* 0x001355e8 */
static void map_manager_exec_main(MapManager *w)
{
    PadSystem *pads;

    if (w->change_req) {
        map_manager_change_scene(w);
    }
    if (w->wipe_task) {
        if (TASK_OF(w->wipe_task)->flags & TASK_F_END) {
            w->wipe_task = 0;
        }
    }
    w->frames = w->frames + 1;
    switch (w->state) {
    case 0:
        map_manager_state_idle(w);
        break;
    case 4:
        map_manager_state_wait(w);
        break;
    case 1:
        map_manager_state_advance(w);
        break;
    case 2:
        map_manager_state_wipe_cover(w);
        break;
    case 3:
        map_manager_state_wipe_end(w);
        break;
    }
    if (w->obj) {
        map_set_frame(w->obj, w->frame);
    }
    if (debug_flags_test(8)) {
        if (kbd_key == 4) {
            w->auto_change = w->auto_change == 0;
            if (w->auto_change) {
                DEBUG_PRINTF("=== MapManager AutoChange On ===\n");
            } else {
                DEBUG_PRINTF("=== MapManager AutoChange Off ===\n");
            }
        }
    }
    if (debug_flags_test(8)) {
        pads = &pad_system;
        if (pads->pad[0]->menu.trig & PAD_Y) {
            map_manager_next_scene(w);
        }
        if (pads->pad[0]->menu.trig & PAD_X) {
            map_set_sort_type(w->obj, map_manager_sort_type);
            map_manager_sort_type = (map_manager_sort_type + 1) % 4;
        }
    }
}

/* Second frame: stores the parent callback's result; idles until map_manager_start_scene. */
/* 0x00135798 */
static void map_manager_task_exec(MapManagerParentCbView *w)
{
    if (w->auto_wait++ > 0) {
        TASK_OF(w)->exec = 0;
        w->auto_wait = 0;
        w->parent = w->parent_cb(w);
    }
}

/*
 * Bar-synced maps: 640x512 backdrop in five textures, display list 3, no fog (debug key 26 hides).
 * Flag 1 clear: title letter per 5 frames, each shrinking from 8x and fading in over 18 frames.
 */
/* steer(cast) */
/* 0x001357e8 */
static void map_manager_task_disp(MapManager *w)
{
    int n;
    int fc_mode, fog;
    int time, k;
    char *str;
    float x, y, a, b;
    int col, r, g, bl, al;

    if (w->map_mode >= 2 && w->cur >= 0) {
        n = w->scenes[w->cur].bg_tex;
        if (n >= 0 && !(debug_flags_test(8) && pad_keyboard_key_down(26))) {
            int bg_fog = fog_set_enable(0);
            int dl = RenderSetCurrentDisplayList(3);

            ulGxSetTextureList((void *)w->tex);
            draw_set_blend(1, 0);
            tex_rect_draw(n, COL_WHITE, 0.0f, 0.0f, 1e-05f, 512.0f, 512.0f);
            tex_rect_draw(n + 1, COL_WHITE, 512.0f, 0.0f, 1e-05f, 128.0f, 128.0f);
            tex_rect_draw(n + 2, COL_WHITE, 512.0f, 128.0f, 1e-05f, 128.0f, 128.0f);
            tex_rect_draw(n + 3, COL_WHITE, 512.0f, 256.0f, 1e-05f, 128.0f, 128.0f);
            tex_rect_draw(n + 4, COL_WHITE, 512.0f, 384.0f, 1e-05f, 128.0f, 128.0f);
            fog_set_enable(bg_fog);
            RenderSetCurrentDisplayList(dl);
        }
    }
    if (w->title_on && !map_manager_scene_flag(w)) {
        x = (screen_layout.w - map_manager_title_len * 36.0f) * 0.5f;
        y = (screen_layout.h - 42.0f) * 0.5f;
        time = w->frames;
        str = map_manager_title;
        fog = fog_set_enable(0);
        fc_mode = draw_set_fc_mode(0);
        draw_set_blend(4, 5);
        font_set(3);
        for (k = 0; k < map_manager_title_len && time >= 0; k++, time -= 5) {
            if (time >= 18) {
                font_end_current(font_put_char_scaled(font_begin_current(w->title_col), x, y, 1.001001e-05f, *str++,
                                               1.0f));
            } else {
                col = w->title_col;
                a = time / 18.0f;
                r = color_get_a(col);
                g = ((int (*)(int))color_get_r)(col);
                bl = color_get_g(col);
                al = color_get_b(col);
                b = 1.0f - a;
                font_end_current(font_put_char_scaled(
                    font_begin_current(color_blend(col,
                                                   ((((256 - r) >> 1) << 24 |
                                                     (((256 - bl) >> 1) & 0xff) << 8) |
                                                    ((((256 - g) >> 1) & 0xff) |
                                                     (((256 - al) >> 1) & 0xff) << 16)),
                                                   b)),
                    x, y, 1.001001e-05f, *str++, a + b * 8.0f));
            }
            x += 36.0f;
        }
        fog_set_enable(fog);
        draw_set_fc_mode(fc_mode);
    }
    if (debug_flags_test(8) && debug_disp_level == 0) {
        debug_set_origin(0, -5);
        debug_printf("SceneID : %d(%d)", map_manager_get_scene_id(w), map_manager_scene_flag(w));
        debug_set_origin(0, -4);
        debug_printf("WireID : %d, RealID : %d", map_manager_get_wire_id(w), map_manager_get_real_id(w));
        if (map_manager_debug_wait > 0)
            map_manager_debug_wait--;
    }
}

/* Map mode 1. */
/* 0x00135ca0 */
static int map_manager_task_create(void *parent, int scenes, int nscene, int use_wires, int parent_cb)
{
    int arg[6];

    arg[0] = scenes;
    arg[1] = nscene;
    arg[2] = use_wires;
    arg[3] = (int)parent;
    arg[4] = parent_cb;
    arg[5] = 1;
    return task_create_inherit_arg(parent, &map_manager_desc, 112, arg);
}

/* Map mode 2. */
/* 0x00135ce8 */
int map_manager_task_create_mode2(void *parent, int scenes, int nscene, int use_wires, int parent_cb)
{
    int arg[6];

    arg[0] = scenes;
    arg[1] = nscene;
    arg[2] = use_wires;
    arg[3] = (int)parent;
    arg[4] = parent_cb;
    arg[5] = 2;
    return task_create_inherit_arg(parent, &map_manager_desc2, 112, arg);
}

/* Scene list: config choice (ConfigGetSelect) in game type 5, else the first. */
/* 0x00135d30 */
int map_manager_task_create_select(void *parent, int *tables, int ntables, int nscene, int use_wires, int parent_cb)
{
    int i, lo, hi;

    if (game_type_get() != 5) {
        i = 0;
    } else {
        i = ConfigGetSelect(0);
        lo = i > -1 ? i : 0;
        hi = ntables - 1;
        lo = lo < hi ? lo : hi;
        i = lo;
    }
    return map_manager_task_create_mode2(parent, tables[i], nscene, use_wires, parent_cb);
}

/* Installs the main update. */
/* 0x00135de8 */
void map_manager_start_scene(MapManager *w, int idx)
{
    if (w) {
        w->next = idx;
        w->change_req = 1;
        TASK_OF(w)->exec = map_manager_exec_main;
    }
}

/* Wraps. */
/* 0x00135e10 */
static void map_manager_next_scene(MapManager *w)
{
    if (w) {
        w->next = (w->cur + 1) % w->nscene;
    }
}

/* 0x00135e48 */
void map_manager_set_next_scene(MapManager *w, int idx)
{
    if (w) {
        w->next = idx;
    }
}

/* Immediate. */
/* 0x00135e58 */
static void map_manager_request_next_scene(MapManager *w)
{
    if (w) {
        w->next = (w->cur + 1) % w->nscene;
        w->change_req = 1;
    }
}

/* Immediate. */
/* 0x00135e90 */
void map_manager_request_scene(MapManager *w, int idx)
{
    if (w) {
        w->next = idx;
        w->change_req = 1;
    }
}

/* 0x00135ea8 */
int map_manager_get_scene(MapManager *w)
{
    int idx = 0;

    if (w) {
        idx = w->cur;
    }
    return idx;
}

/* Run when a scene starts playing. Returns the previous one. */
/* 0x00135ec0 */
int map_manager_swap_start_cb(MapManager *w, int cb)
{
    int old;

    if (w) {
        old = (int)w->start_cb;
        w->start_cb = (void (*)(MapManager *))cb;
        return old;
    }
    return 0;
}

/* Run on every scene change. Returns the previous one. */
/* 0x00135ee0 */
int map_manager_swap_change_cb(MapManager *w, int cb)
{
    int old;

    if (w) {
        old = (int)w->change_cb;
        w->change_cb = (void (*)(MapManager *))cb;
        return old;
    }
    return 0;
}

/* 0x00135f00 */
void map_manager_set_work(MapManager *w, int work)
{
    if (w) {
        w->work = work;
    }
}

/* 0x00135f10 */
int map_manager_get_work(MapManager *w)
{
    int work = 0;

    if (w) {
        work = w->work;
    }
    return work;
}

/* 0x00135f28 */
MapObj *map_manager_get_map(MapManager *w)
{
    MapObj *obj = 0;

    if (w) {
        obj = w->obj;
    }
    return obj;
}

/* 0x00135f40 */
MapScene *map_manager_get_scenes(MapManager *w)
{
    MapScene *scenes = 0;

    if (w) {
        scenes = w->scenes;
    }
    return scenes;
}

/* 0x00135f58 */
static float map_manager_get_frame(MapManager *w)
{
    float frame = 0;

    if (w) {
        frame = w->frame;
    }
    return frame;
}

/* Ended with the map manager. */
/* 0x00135f70 */
void map_manager_set_sub_task(MapManager *w, void *task)
{
    if (w) {
        w->sub_task = task;
    }
}

/* Flag 1 of the current scene. */
/* 0x00135f80 */
int map_manager_scene_flag(MapManager *w)
{
    if (w) {
        return w->scenes[w->cur].flags & 1;
    }
    return 0;
}

/* Flag-1 scenes up to the current one, from 0; the title shows it + 1. */
/* 0x00135fb0 */
int map_manager_get_scene_id(MapManager *w)
{
    MapScene *scene;
    int n, cnt, i;

    if (w) {
        n = -1;
        scene = w->scenes;
        cnt = w->cur + 1;
        while (!(scene->flags & 1)) {
            scene++;
            cnt--;
        }
        for (i = 0; i < cnt; i++, scene++) {
            n += scene->flags & 1;
        }
        return n;
    }
    return -1;
}

/* Position (from 0) among flag-1 scenes since the model last changed. */
/* 0x00136030 */
static int map_manager_get_wire_id(MapManager *w)
{
    MapScene *scene;
    int n, cnt, i, key;

    if (w) {
        n = -1;
        scene = w->scenes;
        cnt = w->cur + 1;
        while (!(scene->flags & 1)) {
            scene++;
            cnt--;
        }
        key = scene->model;
        for (i = 0; i < cnt; i++) {
            if (scene->flags & 1) {
                n++;
                if (scene->model != key) {
                    key = scene->model;
                    n = 0;
                }
            }
            scene++;
        }
        return n;
    }
    return -1;
}

/* Scenes with flag 1 clear since the model last changed. */
/* 0x001360c8 */
static int map_manager_get_real_id(MapManager *w)
{
    MapScene *scene;
    int n, cnt, i, key;

    if (w) {
        n = -1;
        scene = w->scenes;
        cnt = w->cur + 1;
        while (!(scene->flags & 1)) {
            scene++;
            n++;
            cnt--;
        }
        key = scene->model;
        for (i = 0; i < cnt; i++) {
            if (!(scene->flags & 1)) {
                n++;
            } else if (scene->model != key) {
                key = scene->model;
                n = -1;
            }
            scene++;
        }
        return n;
    }
    return -1;
}

/* 0x00136160 */
void map_obj_set_params(MapObj *w, int first, int second)
{
    if (w) {
        w->unk_64 = first;
        w->unk_6c = second;
    }
}

/* 0x00136178 */
void vec3_clamp_length(float max, float *in, float *out)
{
    float len_sq = vec3_length_squared(in);
    float scale;

    if (max * max <= len_sq) {
        scale = max / sqrtf(len_sq);
        out[0] = in[0] * scale;
        out[1] = in[1] * scale;
        out[2] = in[2] * scale;
    } else {
        out[0] = in[0];
        out[1] = in[1];
        out[2] = in[2];
    }
}

/* 0x00136230 */
int fourcc_equal(signed char *fcc_a, signed char *fcc_b)
{
    if (fcc_a[0] == fcc_b[0] && fcc_a[1] == fcc_b[1] && fcc_a[2] == fcc_b[2] && fcc_a[3] == fcc_b[3])
        return 1;
    return 0;
}
