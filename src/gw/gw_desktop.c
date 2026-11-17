/*
 * Debug GUI desktop: root window, 'Desktop Menu' (System, Font, User) and the System info windows.
 */

#include "types_gw.h"
#include "libc.h"
#include "gw_desktop.h"
#include "gw_heap.h"
#include "gw_menu.h"
#include "gw_mouse.h"
#include "gw_system.h"
#include "gw_window.h"
#include "gwu_periinfo.h"
#include "gwu_texview.h"

static void gw_desktop_raise(void);

extern GwWin *gw_periinfo_win;
extern GwWin *gw_texview_win;
extern int gw_desktop_popup;
extern GwWin *gw_desktop_win;
extern int gw_desktop_fonts[];
extern GwWin *gw_heap_info_win;
extern GwWin *gw_mouse_info_win;
extern GwWin *gw_window_info_win;
extern GwItem *gw_desktop_menu;
extern GwFrame *gw_root_win;
extern GwFrame *gw_active_win;
extern char gw_desktop_block[];
extern char gw_desktop_menu_defs[];

/* 'Peripheral Info'; toggles. */
/* 0x00192f40 */
static void gw_menu_toggle_peripheral_info(GwItem *item)
{
    if (gw_item_get_value(item) == 1) {
        if (gw_periinfo_win != 0) {
            gw_window_close(gw_periinfo_win);
            gw_periinfo_win = 0;
        }
        gw_item_set_value(item, 0);
    } else {
        gw_periinfo_win = gw_periinfo_window_create(16, 32, 0);
        gw_item_set_value(item, 1);
        gw_window_set_slot(gw_periinfo_win, 0, item);
    }
}

/* Toggles the texture viewer. */
/* 0x00192fc8 */
static void gw_menu_toggle_texview(GwItem *item)
{
    if (gw_item_get_value(item) == 1) {
        if (gw_texview_win != 0) {
            gw_window_close(gw_texview_win);
            gw_texview_win = 0;
        }
        gw_item_set_value(item, 0);
    } else {
        gw_texview_win = gw_texview_open(0, 200, 0);
        gw_item_set_value(item, 1);
        gw_window_set_slot(gw_texview_win, 0, item);
    }
}

/* Notes that no pop-up is open. */
/* 0x00193050 */
static void gw_popup_menu_on_destroy(void)
{
    gw_desktop_popup = 0;
}

/* Unless one is open; makes the desktop active. Returns the pop-up or 0. */
/* 0x00193058 */
GwPop *gw_popup_menu_open(GwItem *list, char *name, int x, int y)
{
    GwPop *pop;

    if (gw_desktop_popup == 0) {
        pop = gw_menu_popup_create(list, name, x, y, gw_desktop_win);
        if (pop != 0) {
            gw_desktop_popup = pop;
            pop->destroy = gw_popup_menu_on_destroy;
            gw_desktop_raise();
            return pop;
        }
    }
    return 0;
}

/* 0x001930b0 */
static void gw_desktop_raise(void)
{
    gw_window_activate(gw_desktop_win);
}

/* 'Large', 'Middle', 'Small': ticks only this entry. */
/* 0x001930b8 */
static void gw_menu_select_font(GwItem *item, int row)
{
    gw_set_font(gw_desktop_fonts[row]);
    gw_menu_set_all_values(item, 0);
    gw_item_set_value(item, 2);
}

/* 'Heap Info'; toggles. */
/* 0x00193108 */
static void gw_menu_toggle_heap_info(GwItem *item)
{
    if (gw_item_get_value(item) == 1) {
        if (gw_heap_info_win != 0) {
            gw_window_close(gw_heap_info_win);
            gw_heap_info_win = 0;
        }
        gw_item_set_value(item, 0);
    } else {
        gw_heap_info_win = gw_heap_window_create(-1, -1);
        gw_item_set_value(item, 1);
        gw_window_set_slot(gw_heap_info_win, 0, item);
    }
}

/* 'Mouse Info'; opens when ticked, else closes. */
/* 0x00193190 */
static void gw_menu_toggle_mouse_info(GwItem *item)
{
    if (gw_item_is_checked(item)) {
        gw_mouse_info_win = gw_mouse_info_create(-1, -1);
        gw_window_set_slot(gw_mouse_info_win, 0, item);
    } else if (gw_mouse_info_win != 0) {
        gw_window_close(gw_mouse_info_win);
        gw_mouse_info_win = 0;
    }
}

/* 'Window Info'; toggles. */
/* 0x00193200 */
static void gw_menu_toggle_window_info(GwItem *item)
{
    if (gw_item_get_value(item) == 1) {
        if (gw_window_info_win != 0) {
            gw_window_close(gw_window_info_win);
            gw_window_info_win = 0;
        }
        gw_item_set_value(item, 0);
    } else {
        gw_window_info_win = gw_info_window_create(-1, -1);
        gw_item_set_value(item, 1);
        gw_window_set_slot(gw_window_info_win, 0, item);
    }
}

/* 0x00193288 */
GwItem *gw_desktop_get_menu(void)
{
    return gw_desktop_menu;
}

/* Called on a left press on the desktop. */
/* steer(cast) */
/* 0x00193290 */
void gw_desktop_set_left_click(GwFn cb)
{
    *((GwFn *(*)(GwWin *))gw_window_get_user)(gw_desktop_win) = cb;
}

/* Called on a left release on the desktop. */
/* steer(cast) */
/* 0x001932c0 */
static void gw_desktop_set_release_click(GwFn cb)
{
    ((GwFn *(*)(GwWin *))gw_window_get_user)(gw_desktop_win)[1] = cb;
}

/*
 * Right click: 'Desktop Menu' at the pointer (flipped if it would leave); left press/release call
 * cbs.
 */
/* 0x001932f0 */
static void gw_desktop_update(GwWin *win)
{
    int x, y;
    int rect[4];
    int px, py;
    int sw, sh;
    GwFn *hooks;
    int stat;
    GwWin *pop;

    if (gw_window_is_active(win) == 0)
        return;
    hooks = gw_window_get_user(win);
    stat = gw_mouse_get_stat();
    if ((stat & 0x200) && gw_desktop_popup == 0) {
        gw_mouse_get_pos(&x, &y);
        pop = gw_popup_menu_open(gw_desktop_menu, "Desktop Menu", x, y);
        gw_get_desktop_rect(rect);
        gw_window_get_abs_pos(pop, &px, &py);
        gw_window_get_size(pop, &sw, &sh);
        if (rect[2] < px + sw)
            px = px - sw;
        if (rect[3] < py + sh)
            py = py - sh;
        gw_window_set_pos(pop, px, py);
    }
    if (gw_mouse_get_press() & 1) {
        if (hooks[0] != 0)
            hooks[0](win);
    }
    if (gw_mouse_get_release() & 1) {
        if (hooks[1] != 0)
            hooks[1](win);
    }
}

/* 'Desk Top' root in a static block, root and active; builds the menu, ticks the current font. */
/* 0x00193440 */
void gw_desktop_init(int width, int height)
{
    int font;

    gw_desktop_win = (GwFrame *)gw_desktop_block;
    memset(gw_desktop_block, 0, 164);
    gw_desktop_win->kind = 1;
    gw_desktop_win->flags = 0x10100;
    gw_window_set_pos(gw_desktop_win, 0, 0);
    gw_window_set_size(gw_desktop_win, width, height);
    gw_window_set_title(gw_desktop_win, "Desk Top");
    gw_desktop_win->next = 0;
    gw_desktop_win->prev = 0;
    gw_desktop_win->child = 0;
    gw_desktop_win->parent = 0;
    gw_desktop_win->update = gw_desktop_update;
    gw_desktop_win->draw = 0;
    gw_window_set_user(gw_desktop_win, gw_dc_calloc(1, 8, "gw_desktop.c", 437));
    gw_active_win = gw_desktop_win;
    gw_root_win = gw_desktop_win;
    gw_desktop_menu = gw_menu_build(0, gw_desktop_menu_defs);
    font = gw_get_font();
    gw_item_set_value(gw_menu_get_nth(gw_menu_find_path(gw_desktop_menu, "Font.Large"), font), 2);
}

/* 0x00193568 */
void gw_desktop_shutdown(void)
{
    GwWin *win = gw_desktop_win;
    void *user = gw_window_get_user(win);

    if (user != 0) {
        gw_dc_free(user, "gw_desktop.c", 493);
        gw_window_set_user(win, 0);
    }
    if (gw_desktop_menu != 0) {
        gw_menu_free_all(gw_desktop_menu);
        gw_desktop_menu = 0;
    }
}
