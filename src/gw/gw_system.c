/*
 * Debug GUI ('Gindows') system: init and exit, per-frame input and draw, configuration (screen,
 * font, pad port), the Alt+Tab 'Window Manager' and the 'Window Info' window.
 */

#include "types_gw.h"
#include "color.h"
#include "libc.h"
#include "gw_system.h"
#include "gw_command.h"
#include "gw_console.h"
#include "gw_desktop.h"
#include "gw_draw.h"
#include "gw_font.h"
#include "gw_heap.h"
#include "gw_keyboard.h"
#include "gw_menu.h"
#include "gw_mouse.h"
#include "gw_rect.h"
#include "gw_scrollbar.h"
#include "gw_varedit.h"
#include "gw_window.h"
#include "gwu_texture.h"
#include "ps_pad.h"
#include "ps_timer.h"
#include "ul_gx.h"

static int gw_set_ss_mode(int mode);
static int gw_set_tex_filter(int filter);

extern int gw_next_x;
extern int gw_next_y;
extern int gw_sys_mode;
extern unsigned char gw_keyboard_state[];
extern int gw_manager_bar_colors[];
extern int gw_manager_win;
extern int gw_desktop_win;
extern int gw_blend_src;
extern int gw_blend_dst;
extern int gw_ss_mode;
extern int gw_tex_filter;
extern GwConf gw_config;
extern int gw_desktop_margin;
extern int gw_root_win;
extern float gw_scale;
extern float gw_draw_scale;
extern int gw_init_colors[];

/*
 * Cascades: each call moves the place one cell down-right. A null output resets it near top left.
 */
/* 0x0018b6d8 */
void gw_next_window_pos(int *x, int *y)
{
    int org[2];
    int cw;
    int ch;
    int vx;

    gw_get_cell_size(&cw, &ch);
    vx = 0;
    if (x == 0 || y == 0) {
        gw_get_desktop_rect(org);
        gw_next_x = org[0] + cw * 2;
        gw_next_y = org[1] + ch * 2;
    } else {
        vx = gw_next_x;
        *x = vx;
        *y = gw_next_y;
        gw_next_x += cw;
        gw_next_y += ch;
    }
}

/* Returns the previous mode. */
/* 0x0018b780 */
static int gw_set_sys_mode(int mode)
{
    int old = gw_sys_mode;

    gw_sys_mode = mode;
    return old;
}

/* 'cd'; does nothing. */
/* 0x0018b790 */
static int gw_cmd_cd(void)
{
    return 0;
}

/* 'ls'; does nothing. */
/* 0x0018b798 */
static int gw_cmd_ls(void)
{
    return 0;
}

/* Into gw_input_keys: Up 1, Down 2, Left 4, Right 8, PgUp 0x10, PgDn 0x20, Home 0x40, End 0x80. */
/* 0x0018b7a0 */
static void gw_input_map_keys(void)
{
    unsigned char k = gw_keyboard_state[14];

    gw_input_keys[0] = 0;
    gw_input_keys[1] = 0;
    gw_input_keys[2] = 0;
    if (k == 82)
        gw_input_keys[2] = 1;
    if (k == 81)
        gw_input_keys[2] |= 2;
    if (k == 80)
        gw_input_keys[2] |= 4;
    if (k == 79)
        gw_input_keys[2] |= 8;
    if (k == 75)
        gw_input_keys[2] |= 0x10;
    if (k == 78)
        gw_input_keys[2] |= 0x20;
    if (k == 74)
        gw_input_keys[2] |= 0x40;
    if (k == 77)
        gw_input_keys[2] |= 0x80;
}

/* A line per window title, the selected one on a bar. */
/* 0x0018b880 */
static void gw_manager_window_draw(GwWin *win)
{
    int y = 4;
    int i = 0;
    int cnt = win->slot[0];
    GwWin **list = (GwWin **)win->slot[1];
    int cur = win->slot[2];
    unsigned old;
    int sz[2];
    GwWin *it;

    old = gw_window_set_draw_color(win, ARGB(0x80, 0, 0, 0));
    gw_get_cell_size(&sz[0], &sz[1]);
    for (; i < cnt; i++) {
        if (i == cur) {
            gw_window_set_draw_color(win, ARGB(0x80, 0x80, 0x80, 0x80));
            gw_draw_rect_fill_gradient(win, 0, y, win->w, y + sz[1], gw_manager_bar_colors, 1.0f);
        } else {
            gw_window_set_draw_color(win, ARGB(0x80, 0, 0, 0));
        }
        it = list[i];
        if (it != 0 && it->title != 0)
            gw_win_text(win, 4, y, it->title);
        y += sz[1];
    }
    gw_window_set_draw_color(win, old);
}

/*
 * Alt+Tab moves the selection (Shift backwards); releasing Alt activates it. Mouse, keys, Ctrl
 * close.
 */
/* 0x0018b9a8 */
static int gw_manager_window_update(GwWin *win)
{
    int cnt = win->slot[0];
    GwWin **list = (GwWin **)win->slot[1];
    int cur = win->slot[2];

    if ((gw_keyboard_state[16] & 0x44) != 0) {
        if (gw_keyboard_state[14] == 0x2b) {
            if ((gw_keyboard_state[16] & 0x22) != 0) {
                cur--;
                if (cur < 0)
                    cur = cnt - 1;
            } else {
                cur++;
                if (cur >= cnt)
                    cur = 0;
            }
        }
        win->slot[2] = cur;
    }
    if ((gw_keyboard_state[18] & 0x44) != 0) {
        if (list[cur] != 0)
            gw_window_activate(list[cur]);
        gw_window_close(win);
        return;
    }
    if ((gw_mouse_get_on() & 0xf) == 0 && (gw_keyboard_state[12] == 0 || gw_keyboard_state[12] == 0x2b)
        && (gw_keyboard_state[16] & 0x99) == 0)
        return;
    return gw_window_close(win);
}

/* Frees its window list. */
/* 0x0018bab8 */
static void gw_manager_window_close(GwWin *win)
{
    if (win->slot[1] != 0) {
        gw_dc_free((void *)win->slot[1], "gw_system.c", 512);
        win->slot[1] = 0;
    }
    gw_manager_win = 0;
}

/* Every shown frame window, centred, sized to the titles. 0 when there are none. */
/* 0x0018baf8 */
static GwWin *gw_manager_window_create(void)
{
    GwWin *win;
    GwWin **list;
    int count = 0;
    unsigned maxlen = 0;
    int idx;
    int coords[4];
    int width;
    int height;
    char *title;

    for (win = ((GwWin *)gw_desktop_win)->child; win != 0; win = win->next) {
        if (gw_window_is_frame(win) == 0)
            continue;
        if (gw_window_is_visible(win) == 0)
            continue;
        count++;
        title = gw_window_get_title(win);
        if (title == 0)
            continue;
        if (maxlen < strlen(title))
            maxlen = strlen(title);
    }
    if (count == 0)
        return 0;
    list = gw_dc_malloc(count * 4, "gw_system.c", 567);
    idx = count - 1;
    for (win = ((GwWin *)gw_desktop_win)->child; win != 0; win = win->next) {
        if (gw_window_is_frame(win) == 0)
            continue;
        if (gw_window_is_visible(win) == 0)
            continue;
        list[idx--] = win;
    }
    gw_get_cell_size(&coords[0], &coords[1]);
    gw_get_screen_size(&coords[2], &coords[3]);
    width = coords[0] * maxlen + 8;
    height = coords[1] * count + 8;
    win = gw_window_open("Window Manager", 332, (coords[2] - width) / 2, (coords[3] - height) / 2, width, height, 0);
    if (win == 0)
        return 0;
    gw_window_set_handlers(win, gw_manager_window_update, gw_manager_window_draw, gw_manager_window_close);
    win->slot[0] = count;
    win->slot[1] = (int)list;
    win->slot[2] = 0;
    return win;
}

/* Alt+Tab, unless already open. */
/* 0x0018bd00 */
static void gw_manager_window_hotkey(void)
{
    if (gw_manager_win == 0) {
        if ((gw_keyboard_state[16] & 0x44) != 0 && gw_keyboard_state[13] == 0x2b)
            gw_manager_win = gw_manager_window_create();
    }
}

/* Either output may be null. */
/* 0x0018bd50 */
static void gw_get_blend(int *src, int *dst)
{
    if (src != 0)
        *src = gw_blend_src;
    if (dst != 0)
        *dst = gw_blend_dst;
}

/* 0x0018bd70 */
static void gw_set_blend(int src, int dst)
{
}

/* 0x0018bd78 */
static void gw_change_blend(int src, int dst)
{
    gw_set_blend(src, dst);
}

/* 0x0018bd80 */
int gw_change_ss_mode(int mode)
{
    return gw_set_ss_mode(mode);
}

/* 0x0018bda0 */
static int gw_get_ss_mode(void)
{
    return gw_ss_mode;
}

/* Returns the previous mode. */
/* 0x0018bda8 */
static int gw_set_ss_mode(int mode)
{
    int old = gw_ss_mode;

    gw_ss_mode = mode;
    return old;
}

/* 0x0018bdb8 */
int gw_change_tex_filter(int filter)
{
    return gw_set_tex_filter(filter);
}

/* 0x0018bdd8 */
static int gw_get_tex_filter(void)
{
    return gw_tex_filter;
}

/* Returns the previous one. */
/* 0x0018bde0 */
static int gw_set_tex_filter(int filter)
{
    int old = gw_tex_filter;

    gw_tex_filter = filter;
    ulGxSetTexFilter(filter);
    return old;
}

/* on stops the d-pad moving the pointer; returns on. */
/* 0x0018be10 */
static int gw_mouse_lock_dpad(int on)
{
    int cur = gw_input_flags;
    int flags;

    if (on)
        flags = cur | 2;
    else
        flags = cur & ~2;
    gw_input_flags = flags;
    return on;
}

/* Units of 8 pixels; either output may be null. */
/* 0x0018be38 */
void gw_get_cell_scale(float *width, float *height)
{
    int sz[2];
    int eight;

    gw_get_cell_size(&sz[0], &sz[1]);
    eight = 8;
    if (width != 0)
        *width = (float)sz[0] / (float)eight;
    if (height != 0)
        *height = (float)sz[1] / (float)eight;
}

/* 0x0018bec0 */
int gw_get_pad_port(void)
{
    return gw_config.pad_port;
}

/* Returns the previous one. */
/* 0x0018bed0 */
static int gw_set_pad_port(int port)
{
    int old = gw_config.pad_port;

    gw_config.pad_port = port;
    return old;
}

/* 0x0018bee8 */
static int gw_get_font_mode(void)
{
    return gw_config.font_mode;
}

/*
 * {left, top, right, bottom}: the screen, or inset 8 at the sides and 32 top/bottom with
 * gw_desktop_margin.
 */
/* 0x0018bef8 */
void gw_get_desktop_rect(int *rect)
{
    int sz[2];

    gw_get_screen_size(&sz[0], &sz[1]);
    if (gw_desktop_margin == 0) {
        rect[2] = sz[0];
        rect[3] = sz[1];
        rect[1] = 0;
        rect[0] = 0;
    } else {
        rect[0] = 8;
        rect[1] = 32;
        rect[2] = sz[0] - 8;
        rect[3] = sz[1] - 32;
    }
}

/* Either output may be null. */
/* 0x0018bf70 */
void gw_get_screen_size(int *width, int *height)
{
    if (width != 0)
        *width = gw_config.w;
    if (height != 0)
        *height = gw_config.h;
}

/* 0x0018bf98 */
int gw_get_font(void)
{
    return gw_config.font;
}

/* Returns the previous one. */
/* 0x0018bfa8 */
int gw_set_font(int font)
{
    int old = gw_config.font;

    gw_config.font = font;
    gw_font_set(font);
    return old;
}

/* Cell size into *cw, *ch (when both given); returns the font. */
/* 0x0018bfe0 */
int gw_get_cell_size(int *cw, int *ch)
{
    if (cw != 0 && ch != 0)
        gw_font_get_cell_size(gw_config.font, cw, ch);
    return gw_config.font;
}

/*
 * Row per window indented by depth from the scroll position: 'A' active, 'F' under pointer,
 * address, title.
 */
/* 0x0018c030 */
static void gw_info_tree_draw(GwWin *win, GwWin *node, int *px, int *py)
{
    int sz[2];
    int rows;
    int y;
    unsigned old;

    gw_get_cell_size(&sz[0], &sz[1]);
    rows = sz[1] * 2;
    while (node != 0) {
        y = *py - (int)gw_scrollbar_get_val(gw_window_get_vscrollbar(win));
        if (y >= rows) {
            old = gw_window_set_draw_color(win, ARGB(0x80, 0, 0, 0x80));
            if (gw_window_is_active(node) != 0)
                gw_win_text(win, 0, y, "A");
            if (gw_window_is_grabbed(node) != 0)
                gw_win_text(win, sz[0], y, "F");
            gw_window_set_draw_color(win, old);
            if (win->title != 0)
                gw_win_printf(win, *px, y, "%08X(\"%s\")", node, node->title);
            else
                gw_win_printf(win, *px, y, "%08X", node);
        }
        *py += sz[1];
        if (node->child != 0) {
            *px += sz[0];
            gw_info_tree_draw(win, node->child, px, py);
            *px -= sz[0];
        }
        node = node->next;
    }
}

/* Window count, then the tree. */
/* 0x0018c1e8 */
static void gw_info_window_draw(GwWin *win)
{
    int coords[5];

    gw_get_cell_size(&coords[0], &coords[1]);
    coords[2] = 0;
    gw_window_count((GwWin *)gw_desktop_win, &coords[2]);
    gw_win_printf(win, 0, 0, "nbWindow : %3d", coords[2]);
    coords[3] = coords[0] * 2;
    coords[4] = coords[1] * 2;
    gw_info_tree_draw(win, (GwWin *)gw_root_win, &coords[3], &coords[4]);
}

/* Scroll bar sized from the window count; keys and wheel scroll while active. */
/* 0x0018c270 */
static void gw_info_window_update(GwWin *win)
{
    int coords[3];
    int total;
    int thumb;
    int cell_h;

    coords[0] = 0;
    gw_window_count((GwWin *)gw_desktop_win, &coords[0]);
    gw_get_cell_size(&coords[1], &coords[2]);
    total = gw_scrollbar_get_track(win->vscroll);
    thumb = (int)((float)win->h / (float)(coords[0] * coords[2]) * (float)total);
    if (thumb > total)
        thumb = total;
    if (thumb <= 3)
        thumb = 4;
    gw_scrollbar_set_pos(win->vscroll, thumb);
    cell_h = coords[2];
    gw_scrollbar_set_max(win->vscroll, (float)((coords[0] - 1) * cell_h));
    if (gw_window_has_active(win) != 0) {
        gw_scrollbar_keys(gw_window_get_vscrollbar(win));
        gw_scrollbar_wheel(gw_window_get_vscrollbar(win));
    }
}

/* Unticks the menu entry. */
/* 0x0018c360 */
static void gw_info_window_close(GwWin *win)
{
    int obj = gw_window_get_slot(win, 0);

    if (obj != 0)
        gw_item_set_value(obj, 0);
}

/* 150 x 300, vertical scroll bar. */
/* 0x0018c390 */
GwWin *gw_info_window_create(int x, int y)
{
    GwWin *win;
    float cfg[5];
    int sz[2];

    gw_get_cell_size(&sz[0], &sz[1]);
    win = gw_window_open("Window Info", 0x1005f, x, y, 150, 300, 0);
    gw_window_set_handlers(win, gw_info_window_update, gw_info_window_draw, gw_info_window_close);
    cfg[0] = 0;
    cfg[1] = 0;
    cfg[2] = 100.0f;
    cfg[3] = (float)sz[1];
    cfg[4] = (float)sz[1] * 5.0f;
    gw_window_set_vscrollbar(win, gw_scrollbar_create(1, cfg, 0, 0, 0, 0, win));
    return win;
}

/*
 * Windows and pointer at depths from gw_scale; restores font, scale, colour. Times itself, discards
 * it.
 */
/* 0x0018c478 */
int gw_update(void)
{
    int start;
    int font;
    unsigned col;
    float old_scale;

    start = psTimerGetCount();
    gw_set_blend(8, 6);
    gw_change_ss_mode(0);
    gw_change_tex_filter(0);
    font = gw_font_set(gw_config.font);
    old_scale = gw_font_set_scale(0.0f);
    col = gw_font_set_color(ARGB(0x80, 0, 0, 0));
    gw_draw_scale = 1.0f / (gw_scale + 0.025f);
    gw_window_scale_pass((GwWin *)gw_root_win, &gw_draw_scale);
    gw_mouse_draw(&gw_mouse_state, gw_draw_scale);
    gw_font_set_color(col);
    gw_font_set_scale(old_scale);
    gw_font_set(font);
    psTimerDiff(start, psTimerGetCount());
    return 0;
}

/* Keyboard, navigation keys, pointer, Window Manager hot key, every window, then menu keys. */
/* steer(cast) */
/* 0x0018c570 */
int gw_input_update(void)
{
    ((void (*)(unsigned char *))gw_keyboard_poll)(gw_keyboard_state);
    gw_input_map_keys();
    gw_mouse_update(&gw_mouse_state);
    gw_manager_window_hotkey();
    gw_console_poll();
    gw_window_update_all((GwWin *)gw_root_win);
    gw_menu_update_input();
    return 0;
}

/* Console, then desktop. */
/* 0x0018c5c8 */
void gw_exit(void)
{
    gw_console_exit();
    gw_desktop_shutdown();
}

/*
 * Heap, font, pointer, desktop, scale, colours, commands, variable editor, console, texture
 * viewer, pad analog mode. A null user_conf is still read later, so cannot really be null.
 */
/* steer(cast) */
/* 0x0018c5e8 */
void gw_init(int *user_conf)
{
    GwConf *conf;

    if (user_conf != 0) {
        gw_config = *(GwConf *)user_conf;
    } else {
        gw_config.font = 0;
        gw_config.font_mode = 2;
        gw_config.pad_port = 18;
    }
    conf = &gw_config;
    gw_heap_init(gw_config.heap, conf->heap_size);
    ((void (*)(int, int))gw_font_init)(conf->font, conf->font_mode);
    gw_mouse_init(conf->pad_port, conf->w, conf->h);
    gw_desktop_init(conf->w, conf->h);
    gw_draw_scale = conf->scale;
    gw_scale = conf->scale;
    gw_next_window_pos(0, 0);
    gw_set_default_colors(gw_init_colors, ARGB(0x80, 0, 0, 0));
    gw_command_init();
    gw_varedit_init();
    gw_console_init(user_conf + 8);
    gw_console_set_serial_out(user_conf[11]);
    gw_texview_install();
    psPadChangeAnalogMode(user_conf[6], 0);
}
