/*
 * Tutorial stage: message box for the current step (image in English, text otherwise), button
 * hint that lights while held, event listeners (map change, item drops), map manager.
 */

#include "stg20.h"
#include "color.h"
#include "debug.h"
#include "billboard.h"
#include "callback.h"
#include "console.h"
#include "float_bits.h"
#include "item.h"
#include "map.h"
#include "map_manager.h"
#include "model.h"
#include "node_draw_state.h"
#include "stage.h"
#include "system.h"
#include "task.h"
#include "ul_gx.h"
#include "stage_base.h"
#include "game_flow_editor.h"
#include "screen.h"

/* current step's label */
typedef struct ExplainWork {
    void *list;                 /* texture list */
    int idx;                    /* message, 0 = none */
} ExplainWork;

typedef struct BoxSize {
    float width, h;
} BoxSize;

/* 20 bytes: mode 1 follows held button bit 2, mode 2 bit 10; shown state fades over 0..4 */
typedef struct Stg20ButtonWork {
    int mode;
    int shown;
    int next;                   /* mode for the next frame */
    int on;
    int level;
} Stg20ButtonWork;

extern Pad *pad_main;
extern ScreenCfg screen_cfg;
extern GameState game_state;

extern void font_set(int id);
extern int fptoui(float f);
extern char stg20_button_mode_uvs[][4];
extern float stg20_button_shown_uvs[][4];
extern struct TaskDesc stg20_button_desc;
extern ListenerDef listener_defs[4];
extern char explain_ptm[];
extern void *listener_handles[4];
extern BoxSize explain_box_sizes[];
extern char **explain_texts[];

/* map manager task for the tutorial's maps, and its owner's work */
extern void *map_menu;

extern int *map_work;

extern void *map_child;

/* model part radii per map */
extern float *map_shapes[];

extern char explain_uvs[];

extern struct TaskDesc explain_desc;
extern char map_menu_table[];
extern char str_start_map[];
extern char str_start_map_shape[];
extern struct TaskDesc map_child_desc;
extern struct TaskDesc map_desc;

/* 0x00af6700 */
void stg20_button_init(void)
{
}

/* 0x00af6708 */
void stg20_button_release(void)
{
}

/* 0x00af6710 */
void stg20_button_exec(Stg20ButtonWork *btn)
{
    int mode = btn->mode;
    int target;

    switch (mode) {
    case 0:
        btn->on = 0;
        break;
    case 1: /* steer(type): the button word is shifted as an int, as the original */
        btn->on = (int)pad_main->player.on >> 2 & 1;
        break;
    case 2:
        btn->on = (int)pad_main->player.on >> 10 & 1;
        break;
    }
    if (btn->shown != btn->on) {
        target = btn->on ? 4 : 0;
        if (btn->level == target || mode == 0) {
            btn->shown = btn->on;
            btn->level = target;
        } else if (btn->level < target) {
            btn->level++;
        } else {
            btn->level--;
        }
    }
    btn->mode = btn->next;
}

/* steer(cast) */
/* 0x00af67c0 */
int stg20_button_disp(Stg20ButtonWork *btn)
{
    void *tlist = (void *)stage_get_buffer();
    float x, y, z;

    if (tlist && (tlist = word0_or_null(((void **)tlist)[1])) != 0) {
        ulGxSetTextureList(tlist);
        if (btn->mode > 0) {
            z = float_add_bits16(3, 2.2222223f);
            x = screen_cfg.cx + 278.0f - 96.0f;
            y = screen_top + 42.0f + 66.0f + 44.0f;
            draw_set_blend(4, 5);
            ((int (*)(int, int, float *, float, float, float, float, float))tex_rect_draw_uv)(1, COL_WHITE, stg20_button_shown_uvs[btn->shown],
                          x, y, z, 64.0f, 32.0f);
            return ((int (*)(int, int, float *, float, float, float, float, float))tex_rect_draw_uv)(1, COL_WHITE,
                          (float *)(*(char **)stg20_button_mode_uvs[btn->mode] + (btn->level >> 1) * 16),
                          x, y + 32.0f, z, 64.0f, 32.0f);
        }
    }
}

/* 0x00af6940 */
void *stg20_button_create(void *parent)
{
    return task_create(parent, &stg20_button_desc, 20);
}

/* 0x00af6968 */
void button_command(void *w, float *args)
{
    int *buf = (int *)stage_get_buffer();
    int *btn;
    int next_mode;

    if (buf && (btn = (int *)buf[2]) != 0) {
        next_mode = 2;
        if (args[1] < 2.0f)
            next_mode = (int)args[1];
        btn[2] = next_mode;
        btn[3] = 0;
    }
}

/* 0x00af69d0 */
void listeners_add(void)
{
    unsigned int i;

    for (i = 0; i < 4; i++)
        listener_handles[i] = listener_add(listener_defs[i].idx, listener_defs[i].fn);
}

/* 0x00af6a38 */
void listeners_remove(void)
{
    unsigned int i;

    for (i = 0; i < 4; i++) {
        listener_remove(listener_defs[i].idx, listener_handles[i]);
        listener_handles[i] = 0;
    }
}

/* 0x00af6aa0 */
void listener_symbol_effect(void)
{
}

/* 0x00af6aa8 */
void listener_next_map(void)
{
    map_menu_next();
}

/* 0x00af6ab0 */
void listener_symbol_end(void)
{
}

/* 0x00af6ab8 */
void listener_item_spawn(float *ev)
{
    Spawn spawn;

    spawn.x = ev[0];
    spawn.y = ev[1];
    spawn.z = ev[2];
    spawn.scale = 0.9f;
    spawn.vel_x = 0;
    spawn.vel_y = 0;
    spawn.vel_z = 0;
    if (((int *)ev)[4] == 1) {
        spawn.kind = 2;
        item_state_if_alive(item_create(&spawn), 4);
    } else if (((int *)ev)[4] == 2) {
        spawn.kind = 0;
        item_state_if_alive(item_create(&spawn), 4);
    }
}

/* 0x00af6b58 */
void explain_init(void **w)
{
    *w = model_create_timed(explain_ptm, 0x201000);
    task_set_dlist(w, 13);
}

/* 0x00af6b98 */
void explain_release(void **w)
{
    model_list_remove(*w);
    *w = 0;
}

/* 0x00af6bc8 */
void explain_exec(int *w)
{
    if (w[1] != w[2])
        w[1] = w[2];
}

/* 0x00af6be0 */
void explain_disp(ExplainWork *ex)
{
    int lang;
    int color;
    float z, box_w, x, y;

    if (ex->idx != 0) {
        lang = game_state.lang;
        z = float_add_bits16(2, 2.2222223f);
        draw_set_blend(4, 5);
        box_w = explain_box_sizes[ex->idx].width + 44.0f;
        y = screen_top + 42.0f;
        x = screen_cfg.cx - box_w * 0.5f;
        rect_draw_outline(x, y, z, box_w, 110.0f, console_get_pcolp(), RGBA(0, 0, 0, 0x5a));
        z = float_add_bits16(2, z);
        ulGxSetTextureList(ex->list);
        if (lang == 0) {
            box_w = explain_box_sizes[ex->idx].width;
            tex_rect_draw_uv(0, COL_WHITE, (float *)(explain_uvs + ex->idx * 16), screen_cfg.cx - box_w * 0.5f, y + 22.0f, z,
                          box_w, explain_box_sizes[ex->idx].h);
        } else {
            color = game_state.menu_color;
            y += 22.0f;
            x += 22.0f;
            font_set(2);
            font_print(x, y, z, color, explain_texts[lang][ex->idx]);
        }
    }
}

/* 0x00af6e28 */
void *explain_create(void *parent)
{
    return task_create(parent, &explain_desc, 12);
}

/* 0x00af6e50 */
void *word0_or_null(void *ptr)
{
    if (!ptr)
        return 0;
    return *(void **)ptr;
}

/* 0x00af6e68 */
void explain_set_message(int *w, int msg)
{
    w[2] = msg;
}

/* 0x00af6e70 */
void explain_command(void *w, float *args)
{
    int *buf = (int *)stage_get_buffer();
    unsigned int msg_no;

    if (buf && (msg_no = fptoui(args[1])) < 11)
        explain_set_message((int *)buf[1], msg_no);
}

/* 0x00af6ed0 */
void start_map(void *menu)
{
    int *mw = (int *)map_manager_get_work(menu);

    mw[2] = map_manager_get_scene(menu);
    DEBUG_PRINTF(str_start_map, mw[2]);
    map_set_part_radii(map_manager_get_map(menu), map_shapes[mw[2]]);
}

/* 0x00af6f40 */
void start_map_shape(void *menu)
{
    int scene = map_manager_get_scene(menu);

    DEBUG_PRINTF(str_start_map_shape, scene);
}

/* 0x00af6f68 */
void map_init(int *w)
{
    map_child = task_create_inherit(w, &map_child_desc, 8);
    map_work = w;
    map_manager_set_work(map_menu, w);
    map_manager_swap_start_cb(map_menu, start_map_shape);
    map_manager_swap_change_cb(map_menu, start_map);
    map_manager_set_sub_task(map_menu, w);
    map_manager_start_scene(map_menu, 0);
}

/* 0x00af6ff8 */
void map_release(void)
{
    FINISH_IF(map_menu);
    map_work = 0;
}

/* 0x00af7020 */
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

/* 0x00af7090 */
void *map_create(void *w)
{
    task_create_sibling_inherit(w, &map_desc, 12);
    return map_child;
}

/* steer(cast) */
/* 0x00af70c0 */
void map_menu_create(void *parent)
{
    map_menu = ((void *(*)(void *, void *, int, int, void *(*)(void *)))map_manager_task_create_mode2)(parent, map_menu_table, 3, 1, map_create);
}

/* 0x00af70f8 */
void map_menu_next(void)
{
    map_manager_set_next_scene(map_menu, (map_work[2] + 1) % 3);
}

/* 0x00af7128 */
void map_menu_set_page(int page)
{
    map_manager_set_next_scene(map_menu, page);
}

/* 0x00af7138 */
void map_menu_set_value(int scene)
{
    map_manager_request_scene(map_menu, scene);
}

/* 0x00af7148 */
int map_get_current(void)
{
    return map_work[2];
}

/* 0x00af7158 */
void *map_menu_get(void)
{
    return map_menu;
}

/* 0x00af7168 */
int *map_get_work(void)
{
    return map_work;
}
