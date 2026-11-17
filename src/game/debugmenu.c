/* Debug menu tree: items of the "Kpro Menu" by submenu and item name; popup at the mouse. */

#include "gw_desktop.h"
#include "gw_menu.h"
#include "gw_mouse.h"
#include "gw_window.h"
#include "debugmenu.h"

extern struct GwItem *menu_root;
extern int kpro_menu_window;

/* Empty "Info" submenu at the top. */
/* 0x00111608 */
void info_default(void)
{
    menu_item_add(0, "Info", 0, 0, 1);
}

/* name 0: top menu. */
/* 0x00111628 */
void menu_item_set_value(char *name, char *item_name, int val)
{
    struct GwItem *item = 0;
    struct GwItem *found;

    if (name != 0) {
        found = gw_menu_find_path(menu_root, name);
        if (found != 0)
            item = gw_item_get_submenu(found);
    } else {
        item = menu_root;
    }
    if (item != 0) {
        found = gw_menu_find_path(item, item_name);
        if (found != 0)
            gw_item_set_value(found, val);
    }
}

/* parent 0: top. Mode 0: item calling cb with data; 1: submenu, hidden until it has items. */
/* 0x001116c8 */
void menu_item_add(char *parent, char *name, void (*cb)(), int data, int mode)
{
    struct GwItem *item = 0;
    struct GwItem *menu;

    switch (mode) {
    case 0:
        item = gw_item_alloc(name, 0, 0, cb, data);
        break;
    case 1:
        item = gw_menu_create_submenu(name, 0);
        gw_item_set_visible(item, 0);
        break;
    }
    if (parent != 0) {
        menu = gw_menu_find_path(menu_root, parent);
        if (menu != 0) {
            gw_item_set_submenu(menu, item);
            gw_item_set_visible(menu, 1);
        }
    } else if (menu_root == 0) {
        menu_root = item;
    } else {
        gw_menu_append(menu_root, item);
    }
}

/* name 0: top menu. */
/* 0x001117b8 */
void menu_item_remove(char *name, char *path)
{
    struct GwItem *item = 0;
    struct GwItem *found;

    if (name != 0) {
        found = gw_menu_find_path(menu_root, name);
        if (found != 0)
            item = gw_item_get_submenu(found);
    } else {
        item = menu_root;
    }
    if (item != 0)
        gw_menu_remove_path(item, path);
}

/* 0x00111838 */
static void kpro_menu_closed(void)
{
    kpro_menu_window = 0;
}

/* Returns 0. root unused: always opens on the desktop. */
/* 0x00111848 */
int kpro_menu_toggle(int root)
{
    int mouse[2];
    int win;
    int y;

    if (kpro_menu_window != 0) {
        gw_menu_close(kpro_menu_window);
    } else {
        gw_mouse_get_pos(&mouse[0], &mouse[1]);
        y = mouse[1];
        win = gw_popup_menu_open(menu_root, "Kpro Menu", mouse[0], y);
        kpro_menu_window = win;
        gw_window_set_handlers(win, 0, 0, kpro_menu_closed);
    }
    return 0;
}
