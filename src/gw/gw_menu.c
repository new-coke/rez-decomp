/* Debug GUI menus: item lists from static tables, found by dotted paths ("Font.Large"); pop-ups. */

#include "types_gw.h"
#include "libc.h"
#include "gw_menu.h"
#include "gw_draw.h"
#include "gw_font.h"
#include "gw_heap.h"
#include "gw_mouse.h"
#include "gw_system.h"
#include "gw_window.h"
#include "color.h"

static GwItem *gw_menu_find(GwItem *list, char *name);
static int gw_item_is_visible(GwItem *item);
static int gw_item_is_submenu(GwItem *item);
static void gw_item_free(GwItem *item);
static int gw_menu_read_input(void);

extern GwPop *gw_menu_current;
extern int gw_menu_value_icons[];

/* Every item of the list list belongs to. */
/* 0x00191850 */
void gw_menu_set_all_values(GwItem *list, int val)
{
    GwItem *it = list;

    if (it->prev != 0) {
        do {
            it = it->prev;
        } while (it->prev != 0);
    }
    if (it != 0) {
        do {
            gw_item_set_value(it, val);
            it = it->next;
        } while (it != 0);
    }
}

/* 0x001918c0 */
static int gw_menu_has_submenu(GwItem *list)
{
    GwItem *it = list;

    if (it->prev != 0) {
        do {
            it = it->prev;
        } while (it->prev != 0);
    }
    if (it != 0) {
        do {
            if (gw_item_is_submenu(it))
                return 1;
            it = it->next;
        } while (it != 0);
    }
    return 0;
}

/* 0x00191938 */
static int gw_menu_count(GwItem *list)
{
    GwItem *it = list;
    int n = 0;

    if (it->prev != 0) {
        do {
            it = it->prev;
        } while (it->prev != 0);
    }
    if (it != 0) {
        do {
            it = it->next;
            n++;
        } while (it != 0);
    }
    return n;
}

/* Named non-separator items. */
/* 0x00191998 */
static int gw_menu_max_label_len(GwItem *list)
{
    GwItem *it = list;
    int max = 0;
    int len;

    if (it->prev != 0) {
        do {
            it = it->prev;
        } while (it->prev != 0);
    }
    while (it != 0) {
        if (it->type != 2 && it->name != 0) {
            len = strlen(it->name);
            if (max < len)
                max = len;
        }
        it = it->next;
    }
    return max;
}

/* Separators aside; either output may be null. */
/* 0x00191a38 */
static void gw_menu_measure(GwItem *list, int *maxlen, int *count)
{
    GwItem *it = list;
    int n = 0;
    int max = 0;
    int len;

    if (it->prev != 0) {
        do {
            it = it->prev;
        } while (it->prev != 0);
    }
    while (it != 0) {
        if (it->type != 2) {
            len = strlen(it->name);
            if (max < len)
                max = len;
        }
        it = it->next;
        n++;
    }
    if (maxlen != 0)
        *maxlen = max;
    if (count != 0)
        *count = n;
}

/* 0 based; 0 if none. */
/* 0x00191b00 */
GwItem *gw_menu_get_nth(GwItem *list, int n)
{
    GwItem *it = list;
    int i = 0;

    if (it->prev != 0) {
        do {
            it = it->prev;
        } while (it->prev != 0);
    }
    while (it != 0) {
        if (i == n)
            return it;
        it = it->next;
        i++;
    }
    return 0;
}

/* At most 255 chars, through submenus; list for an empty path, 0 when a part is missing. */
/* 0x00191b58 */
GwItem *gw_menu_find_path(GwItem *list, char *path)
{
    char buf[256];
    char *tok;

    if (strlen(path) >= 256)
        return 0;
    strcpy(buf, path);
    tok = strtok(buf, ".");
    if (tok != 0) {
        do {
            list = gw_menu_find(list, tok);
            if (list == 0)
                return 0;
            tok = strtok(0, ".");
            if (tok == 0)
                return list;
            if (!gw_item_is_submenu(list))
                return 0;
            list = gw_item_get_submenu(list);
        } while (list != 0);
        return 0;
    }
    return list;
}

/* 0x00191c18 */
static GwItem *gw_menu_find(GwItem *list, char *name)
{
    GwItem *it = list;

    if (it->prev != 0) {
        do {
            it = it->prev;
        } while (it->prev != 0);
    }
    while (it != 0) {
        if (it->name != 0 && strcmp(it->name, name) == 0)
            return it;
        it = it->next;
    }
    return 0;
}

/* Submenus too. Bug: reads each next link after freeing the item. */
/* 0x00191ca0 */
void gw_menu_free_all(GwItem *list)
{
    GwItem *it = list;
    GwItem *sub;

    if (it->prev != 0) {
        do {
            it = it->prev;
        } while (it->prev != 0);
    }
    while (it != 0) {
        sub = gw_item_get_submenu(it);
        if (sub != 0) {
            gw_menu_free_all(sub);
            it->val.data = 0;
        }
        gw_item_free(it);
        it = it->next;
    }
}

/* Frees its submenu too; returns the new head. */
/* 0x00191d28 */
static GwItem *gw_menu_remove(GwItem *list, GwItem *item)
{
    GwItem *sub;
    GwItem *next;

    sub = gw_item_get_submenu(item);
    if (sub != 0)
        gw_menu_free_all(sub);
    if (item->prev != 0) {
        item->prev->next = item->next;
        next = item->next;
    } else {
        list = item->next;
        next = list;
    }
    if (next != 0)
        next->prev = item->prev;
    gw_item_free(item);
    return list;
}

/* Returns the new head, or 0 when not found. */
/* 0x00191da8 */
GwItem *gw_menu_remove_path(GwItem *list, char *path)
{
    GwItem *item = gw_menu_find_path(list, path);

    if (item == 0)
        return 0;
    return gw_menu_remove(list, item);
}

/* Returns the head (item itself for an empty list). */
/* 0x00191de8 */
GwItem *gw_menu_append(GwItem *list, GwItem *item)
{
    GwItem *tail;

    if (list != 0) {
        tail = list;
        while (tail->next != 0)
            tail = tail->next;
        tail->next = item;
        item->prev = tail;
    } else {
        list = item;
    }
    return list;
}

/* Returns the new head. */
/* 0x00191e30 */
static GwItem *gw_menu_remove_table(GwItem *list, GwItemDef *def)
{
    GwItem *item;

    if (def->type != 4 && list != 0) {
        do {
            item = gw_menu_find_path(list, def->name);
            def++;
            list = gw_menu_remove(list, item);
        } while (def->type != 4 && list != 0);
    }
    return list;
}

/* Table ends at a type-4 row; submenu rows build their own. 0 for a null table. */
/* 0x00191eb0 */
GwItem *gw_menu_build(GwItem *list, GwItemDef *def)
{
    GwItem *tail;
    GwItem *item;

    if (def == 0)
        return 0;
    tail = list;
    while (def->type != 4) {
        if (def->type == 1) {
            item = gw_item_alloc(def->name, 1, def->val, def->cb, 0);
            gw_item_set_submenu(item, gw_menu_build(0, (GwItemDef *)def->data));
            tail = gw_menu_append(tail, item);
        } else {
            tail = gw_menu_append(tail, gw_item_alloc(def->name, def->type, def->val, def->cb, def->data));
        }
        def++;
    }
    while (tail->prev != 0)
        tail = tail->prev;
    return tail;
}

/* 0x00191fc8 */
static int gw_item_get_data(GwItem *item)
{
    return item->val.data;
}

/* Returns the previous data. */
/* 0x00191fd0 */
static int gw_item_set_data(GwItem *item, int data)
{
    int old = item->val.data;

    item->val.data = data;
    return old;
}

/* 0x00191fe0 */
static void gw_item_set_callback(GwItem *item, GwItemFn cb)
{
    item->cb = cb;
}

/* 0 for non-submenu items. */
/* 0x00191fe8 */
GwItem *gw_item_set_submenu(GwItem *item, GwItem *sub)
{
    if (item->type == 1)
        return item->val.sub = gw_menu_append(item->val.sub, sub);
    return 0;
}

/* Shown and ticked. */
/* 0x00192028 */
int gw_item_is_checked(GwItem *item)
{
    if (!gw_item_is_visible(item))
        return 0;
    return gw_item_get_value(item) != 0;
}

/* Shown items only. */
/* 0x00192060 */
static int gw_item_set_checked(GwItem *item, int on)
{
    if (gw_item_is_visible(item)) {
        if (on)
            return gw_item_set_value(item, 1);
        return gw_item_set_value(item, 0);
    }
}

/* 0x001920b8 */
static int gw_item_is_visible(GwItem *item)
{
    int vis = 0;

    if (!(item->flags & 0x8000))
        vis = 1;
    return vis;
}

/* Hidden items are greyed out and cannot be picked. */
/* 0x001920c8 */
void gw_item_set_visible(GwItem *item, int show)
{
    if (show)
        item->flags &= 0x7fff;
    else
        item->flags |= 0x8000;
}

/* 0..15; 1..3 show a tick. Returns the previous value. */
/* 0x001920e8 */
int gw_item_set_value(GwItem *item, int val)
{
    unsigned short flags = item->flags;

    item->flags = (flags & 0xfff0) | val;
    return flags & 0xf;
}

/* 0x00192100 */
int gw_item_get_value(GwItem *item)
{
    return item->flags & 0xf;
}

/* 0 for other items. */
/* 0x00192110 */
GwItem *gw_item_get_submenu(GwItem *item)
{
    if (gw_item_is_submenu(item))
        return item->val.sub;
    return 0;
}

/* 0x00192140 */
static int gw_item_is_submenu(GwItem *item)
{
    return item->type == 1;
}

/* 0x00192150 */
static void gw_item_free(GwItem *item)
{
    gw_dc_free(item, "gw_menu.c", 700);
}

/* 0x00192160 */
GwItem *gw_menu_create_submenu(char *name, GwItem *sub)
{
    GwItem *item = gw_item_alloc(name, 1, 0, 0, 0);

    if (item != 0)
        gw_item_set_submenu(item, sub);
    return item;
}

/* 0x001921b8 */
static GwItem *gw_menu_create_separator(void)
{
    return gw_item_alloc("-", 2, 0, 0, 0);
}

/* Tickable. */
/* 0x001921e8 */
static GwItem *gw_menu_create_value_item(char *name, short val, GwItemFn cb, int data)
{
    return gw_item_alloc(name, 3, val, cb, data);
}

/* 0x00192218 */
static GwItem *gw_menu_create_item(char *name, GwItemFn cb, int data)
{
    return gw_item_alloc(name, 0, 0, cb, data);
}

/* 0 without memory. */
/* 0x00192240 */
GwItem *gw_item_alloc(char *name, short type, short flags, GwItemFn cb, int data)
{
    GwItem *item = gw_dc_calloc(1, 24, "gw_menu.c", 802);

    if (item == 0)
        return 0;
    item->name = name;
    item->type = type;
    item->flags = flags;
    item->cb = cb;
    item->val.data = data;
    return item;
}

/* 0x001922d8 */
static int gw_menu_get_current(void)
{
    return gw_menu_current;
}

/* Right of the selected row. */
/* 0x001922e0 */
static void gw_menu_selected_pos(GwPop *menu, int *px, int *py)
{
    int rect[4];
    int sz[2];

    gw_get_cell_size(&sz[0], &sz[1]);
    gw_window_get_abs_rect((GwWin *)menu, rect);
    if (px != 0)
        *px = rect[2];
    if (py != 0)
        *py = rect[1] + menu->sel * sz[1];
}

/* Beside the selected row, flipped if it would leave the desktop. 0 when item has no list. */
/* 0x00192358 */
static GwPop *gw_menu_open_submenu(GwPop *menu, GwItem *item)
{
    int rect[4];
    int px, py;
    int x, y;
    int width, height;
    GwPop *sub;

    gw_menu_selected_pos(menu, &px, &py);
    if (item->val.sub != 0) {
        sub = gw_menu_popup_create(item->val.sub, item->name, px, py, menu);
        gw_get_desktop_rect(rect);
        gw_window_get_abs_pos((GwWin *)sub, &x, &y);
        gw_window_get_size((GwWin *)sub, &width, &height);
        if (rect[2] < x + width)
            x = menu->x - width;
        if (rect[3] < y + height)
            y = menu->y - height;
        gw_window_set_pos((GwWin *)sub, x, y);
    } else {
        sub = 0;
    }
    return sub;
}

/* Plain: callback, close menus. Value: flips the tick first. Submenus and separators: nothing. */
/* 0x00192448 */
static void gw_menu_activate_item(GwPop *menu, GwItem *item)
{
    if (item == 0)
        return;
    switch (item->type) {
    case 0:
        if (item->cb != 0)
            item->cb(item, menu->sel, item->val.data);
        gw_menu_close(menu);
        break;
    case 1:
    case 2:
        break;
    case 3:
        if (!gw_item_is_checked(item))
            gw_item_set_checked(item, 1);
        else
            gw_item_set_checked(item, 0);
        if (item->cb != 0)
            item->cb(item, menu->sel, item->val.data);
        gw_menu_close(menu);
        break;
    }
}

/* Bits 1 up, 2 down (skipping separators); stops at the ends, or wraps with 0x10 up / 0x20 down. */
/* 0x00192520 */
static int gw_menu_next_row(GwPop *menu, int bits)
{
    int count = gw_menu_count(menu->list);
    int sel;
    GwItem *item;

    sel = menu->sel;
    if (bits & 1) {
        sel = menu->sel - 1;
        item = gw_menu_get_nth(menu->list, sel);
        if (item != 0 && item->type == 2)
            sel--;
        if (sel < 0) {
            sel = 0;
            if (bits & 0x10)
                sel = count - 1;
        }
    } else if (bits & 2) {
        sel = menu->sel + 1;
        item = gw_menu_get_nth(menu->list, sel);
        if (item != 0 && item->type == 2)
            sel++;
        if (sel > count - 1) {
            sel = 0;
            if (!(bits & 0x20))
                sel = count - 1;
        }
    }
    return sel;
}

/*
 * 8 opens the submenu, 4 back to the parent, 0x100 picks, 0x200 closes. No input read in this
 * build.
 */
/* 0x00192600 */
void gw_menu_update_input(void)
{
    GwPop *menu = gw_menu_current;
    int bits;
    GwItem *item;

    if (menu == 0)
        return;
    bits = gw_menu_read_input();
    menu->sel = gw_menu_next_row(menu, bits);
    item = gw_menu_get_nth(menu->list, menu->sel);
    if (bits & 8) {
        if (item != 0 && item->type == 1 && item->val.sub != 0) {
            if (gw_item_is_visible(item))
                gw_menu_open_submenu(menu, item)->sel = 0;
        }
    }
    if (bits & 4) {
        if (menu->parent != 0 && menu->parent->kind == 4) {
            gw_window_close(menu);
            {
                GwPop *parent = menu->parent;

                gw_menu_current = parent;
            }
        }
    }
    if (bits & 0x100) {
        if (item != 0) {
            if (gw_item_is_visible(item))
                gw_menu_activate_item(menu, item);
        }
    }
    if (!(bits & 0x200))
        return;
    gw_menu_close(menu);
}

/* Always 0 in this build. */
/* 0x00192730 */
static int gw_menu_read_input(void)
{
    return 0;
}

/* 0 for a null or other window. */
/* 0x00192738 */
static GwItem *gw_menu_get_list(GwPop *menu)
{
    if (menu == 0 || menu->kind != 4)
        return 0;
    return menu->list;
}

/* -1 if none. */
/* 0x00192760 */
static int gw_menu_row_at_pointer(GwPop *menu)
{
    int rect[4];
    int px, py;
    int maxlen, count;
    int cw, ch;
    int i = 0;
    GwItem *it = menu->list;

    gw_mouse_get_pos(&px, &py);
    gw_menu_measure(menu->list, &maxlen, &count);
    gw_get_cell_size(&cw, &ch);
    gw_window_get_abs_pos((GwWin *)menu, &rect[0], &rect[1]);
    rect[3] = rect[1] + ch;
    rect[2] = rect[0] + (maxlen + 6) * cw;
    if (it != 0) {
        do {
            if (gw_rect_contains_point(rect, px, py) != 0)
                return i;
            i++;
            rect[1] += ch;
            rect[3] += ch;
            it = it->next;
        } while (it != 0);
    }
    return -1;
}

/*
 * Longest label + 2 cells (+4 more with submenus) wide, a cell per row high. Outputs may be null.
 */
/* 0x00192848 */
static void gw_menu_measure_size(GwItem *list, int *pw, int *ph)
{
    int maxlen, count;
    int cw, ch;
    int has;
    int extra;

    gw_menu_measure(list, &maxlen, &count);
    gw_get_cell_size(&cw, &ch);
    has = gw_menu_has_submenu(list);
    extra = 4;
    if (has == 0)
        extra = 0;
    if (pw != 0) {
        int margin = extra + 2;

        *pw = (maxlen + margin) * cw;
    }
    if (ph != 0)
        *ph = count * ch;
}

/* Dark line over a light one. */
/* 0x001928d8 */
static void gw_menu_draw_separator(float x0, float x1, float y, float z)
{
    gw_draw_rect(x0, y, x1, y + 1.0f, z, 0x80404040);
    gw_draw_rect(x0, y + 1.0f, x1, y + 2.0f, z, 0x80c0c0c0);
}

/* Highlight behind the selection, label (grey when hidden), submenu arrow, separator line, tick. */
/* 0x00192970 */
static void gw_menu_draw(GwPop *menu, float scale)
{
    int rect[4];
    int cw, ch;
    float iw, ih;
    float left, y, bot, right, step;
    unsigned prev;
    GwItem *it;
    int row;
    int val;

    if (menu->kind != 4)
        return;
    gw_get_cell_size(&cw, &ch);
    row = 0;
    gw_draw_window_body((GwWin *)menu, scale);
    gw_window_get_abs_rect((GwWin *)menu, rect);
    left = (float)rect[0];
    y = (float)rect[1];
    right = (float)rect[2];
    bot = y + (float)ch;
    prev = gw_font_set_color(ARGB(0x80, 0, 0, 0));
    it = menu->list;
    step = (float)ch;
    if (it != 0) {
        do {
            if (row == menu->sel) {
                if (it->type != 2) {
                    float hl_left = (float)rect[0];
                    float hl_right = (float)rect[2];

                    gw_draw_rect_gradient(hl_left, y, hl_right, bot, gw_float_bits_add(scale, 1), 0x80800000, 0);
                }
                gw_font_set_color(ARGB(0x80, 0x80, 0x80, 0x80));
            } else {
                gw_font_set_color(ARGB(0x80, 0, 0, 0));
            }
            if (it->flags & 0x8000)
                gw_font_set_color(ARGB(0x80, 0x40, 0x40, 0x40));
            if (it->name != 0) {
                if (it->type != 2)
                    gw_font_text_clip(it->name, left + (float)cw * 2.0f, y, right, bot);
            }
            if (it->type == 1)
                gw_font_putc(right - (float)cw - 0.0f, y, 127);
            if (it->type == 2) {
                float sep_left = (float)rect[0];
                float sep_right = (float)rect[2];
                float sep_y = y + (float)(ch / 2);

                gw_menu_draw_separator(sep_left, sep_right, sep_y, gw_float_bits_add(scale, 1));
            }
            val = gw_item_get_value(it);
            if (val < 4) {
                if (val > 0) {
                    gw_get_cell_scale(&iw, &ih);
                    gw_font_icon_scaled(left, y, ih, gw_menu_value_icons[val]);
                }
            }
            it = it->next;
            y += step;
            bot += step;
            row++;
        } while (it != 0);
    }
    gw_font_set_color(prev);
}

/* And every pop-up it was opened from. */
/* 0x00192c50 */
int gw_menu_close(GwPop *menu)
{
    int ret;

    if (menu->kind == 4) {
        gw_window_close(menu);
        if (menu == gw_menu_current)
            gw_menu_current = 0;
        if (menu->parent != 0) {
            ret = gw_menu_close(menu->parent);
            return ret;
        }
    }
}

/* On win or a submenu opened from it. */
/* 0x00192ca8 */
static int gw_menu_chain_has_pointer(GwWin *win)
{
    for (;;) {
        if (win == 0)
            return 0;
        if (gw_window_is_grabbed(win))
            return 1;
        win = win->child;
    }
}

/*
 * Moving pointer selects the row under it (none outside with no submenu open); a new selection
 * swaps the open submenu; release over a shown row picks; a press outside closes all.
 */
/* 0x00192ce8 */
static void gw_menu_update_pointer(GwPop *menu)
{
    int rect[4];
    int px, py;
    int stat;
    int old;
    GwItem *item;

    gw_window_get_abs_rect((GwWin *)menu, rect);
    gw_mouse_get_pos(&px, &py);
    stat = gw_mouse_get_stat();
    old = menu->sel;
    if (gw_mouse_moved()) {
        if (gw_rect_contains_point(rect, px, py))
            menu->sel = gw_menu_row_at_pointer(menu);
        else if (menu->child == 0)
            menu->sel = -1;
    }
    item = gw_menu_get_nth(menu->list, menu->sel);
    if (old != menu->sel) {
        if (menu->child != 0) {
            gw_window_close(menu->child);
            gw_menu_current = menu;
        }
        if (item != 0 && item->type == 1) {
            if (item->val.sub != 0 && gw_item_is_visible(item))
                gw_menu_open_submenu(menu, item);
        }
    }
    if (gw_rect_contains_point(rect, px, py)) {
        if (item != 0 && (stat & 0x100)) {
            if (gw_item_is_visible(item))
                gw_menu_activate_item(menu, item);
        }
    } else if (stat & 0x30) {
        if (!gw_menu_chain_has_pointer(menu->child))
            gw_menu_close(menu);
    }
}

/* Forgets it as the current menu. */
/* 0x00192e60 */
static void gw_menu_on_destroy(GwPop *menu)
{
    if (menu == gw_menu_current)
        gw_menu_current = 0;
}

/* Sized to fit, nothing selected; becomes the current menu. */
/* 0x00192e78 */
GwPop *gw_menu_popup_create(GwItem *list, char *name, int x, int y, GwPop *parent)
{
    int width, height;
    GwPop *menu;
    GwPop *pop;

    gw_menu_measure_size(list, &width, &height);
    menu = gw_window_new(4, name, 332, x, y, width, height, parent);
    pop = 0;
    if (menu != 0) {
        menu->update = gw_menu_update_pointer;
        menu->draw = gw_menu_draw;
        menu->destroy = gw_menu_on_destroy;
        menu->list = list;
        menu->sel = -1;
        gw_menu_current = menu;
        menu->owner = 0;
        pop = menu;
    }
    return pop;
}
