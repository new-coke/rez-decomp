/* Debug GUI choice box: shows the selected string; an arrow button drops down the list. */

#include "types_gw.h"
#include "color.h"
#include "libc.h"
#include "gw_choicebox.h"
#include "gw_button.h"
#include "gw_console.h"
#include "gw_draw.h"
#include "gw_heap.h"
#include "gw_mouse.h"
#include "gw_system.h"
#include "gw_window.h"

static void gw_choicebox_open_popup(GwChoice *box);

#define GW_F_DISABLED 0x20000000

#define GW_F_PRESSED 0x40000
#define GW_F_RELEASED 0x40


/* Out of range 0..count-1 is reported on the console. */
/* 0x001947e0 */
void gw_choicebox_set_sel(GwChoiceMenu *box, int i)
{
    if (i < 0 || i >= box->count) {
        gw_console_printf("Illegal index [%d]\n", i);
        return;
    }
    box->sel = i;
}

/* 0x00194810 */
static int gw_choicebox_get_sel(GwChoiceMenu *box)
{
    return box->sel;
}

/* 0x00194818 */
static void gw_choicebox_button_cb(GwWin *btn, int ev)
{
    GwWin *box = btn->parent;

    if (ev == 0) {
        if (gw_mouse_get_press() & 1)
            gw_choicebox_open_popup(box);
    }
}

/* Row under the pointer grey over a gradient bar, others black. */
/* 0x00194860 */
static void gw_choicebox_popup_draw(GwChoiceMenu *menu, float scale)
{
    int y = 0;
    int row = 0;
    GwChoiceMenu *box;
    int hot;
    unsigned prev;
    GwChoiceItem *it;
    int rect[4];

    box = menu->owner;
    gw_draw_window_body((GwWin *)menu, scale);
    hot = menu->hot;
    prev = gw_window_set_draw_color((GwWin *)menu, ARGB(0x80, 0, 0, 0));
    it = box->items;
    for (; row < box->count; row++) {
        if (row == hot)
            gw_window_set_draw_color((GwWin *)menu, ARGB(0x80, 0x80, 0x80, 0x80));
        else
            gw_window_set_draw_color((GwWin *)menu, ARGB(0x80, 0, 0, 0));
        gw_win_text((GwWin *)menu, 0, y, it->str);
        y += menu->ch;
        it = it->next;
    }
    gw_window_set_draw_color((GwWin *)menu, prev);
    if (hot >= 0) {
        float left, right, top, row_h;

        gw_window_get_abs_rect((GwWin *)menu, rect);
        left = (float)rect[0];
        row_h = (float)menu->ch;
        right = (float)rect[2];
        top = (float)(rect[1] + menu->ch * hot);
        gw_draw_rect_gradient(left, top, right, top + row_h, gw_float_bits_add(scale, 1), 0x80800000, 0);
    }
}

/*
 * Placed under the owner; a press outside closes it; a click selects the row and calls the owner's
 * cb.
 */
/* 0x001949e0 */
static void gw_choicebox_popup_update(GwChoiceMenu *menu)
{
    GwChoiceMenu *box = menu->owner;
    GwWin *pop = box->popup;
    int lx, ly;
    int row;

    menu->w = box->w;
    menu->h = box->count * box->ch;
    menu->x = box->x;
    menu->y = box->y + box->ch + 2;
    if (!gw_window_is_grabbed((GwWin *)menu) && !gw_window_is_grabbed((GwWin *)box) && !gw_window_is_grabbed(pop)
        && gw_mouse_get_press())
        gw_window_close((GwWin *)menu);
    if (gw_win_mouse_local((GwWin *)menu, &lx, &ly)) {
        row = ly / menu->ch;
        do {
            if (row >= 0) {
                if (row < box->count)
                    menu->hot = row;
            }
        } while (0);
        if (gw_mouse_get_press() & 1) {
            int sel = menu->hot;

            box->sel = sel;
            if (box->cb != 0)
                box->cb(menu, sel);
            gw_window_close((GwWin *)menu);
        }
    }
}

/* Marks the owner closed and makes it active. */
/* 0x00194b10 */
static void gw_choicebox_popup_on_destroy(GwChoiceMenu *menu)
{
    GwChoiceMenu *box = menu->owner;

    box->open = 0;
    gw_window_activate((GwWin *)box);
}

/* In the owner's parent. */
/* 0x00194b20 */
static GwChoiceMenu *gw_choicebox_popup_create(int x, int y, GwChoiceMenu *owner)
{
    GwChoiceMenu *menu = gw_window_new(0, "choice menu", 104, x, y, 0, 0, owner->parent);

    if (menu == 0)
        return 0;
    menu->owner = owner;
    menu->update = gw_choicebox_popup_update;
    menu->draw = gw_choicebox_popup_draw;
    menu->destroy = gw_choicebox_popup_on_destroy;
    gw_window_set_owner((GwWin *)menu, 0);
    return menu;
}

/* Returns the head (node itself for an empty list). */
/* 0x00194bb8 */
static GwListNode *gw_choicebox_list_append(GwListNode *head, GwListNode *node)
{
    GwListNode *tail;

    if (head) {
        tail = head;
        while (tail->next)
            tail = tail->next;
        tail->next = node;
        node->prev = tail;
    } else
        head = node;
    return head;
}

/* 0x00194c00 */
static void gw_choicebox_item_free(GwListNode *node)
{
    if (node) {
        if (node->str) {
            gw_dc_free(node->str, "gw_choicebox.c", 328);
            node->str = 0;
        }
        gw_dc_free(node, "gw_choicebox.c", 331);
    }
}

/* With its own copy of str. */
/* 0x00194c68 */
static GwListNode *gw_choicebox_item_new(char *str)
{
    GwListNode *node;

    node = gw_dc_calloc(1, 12, "gw_choicebox.c", 346);
    node->str = gw_dc_malloc(strlen(str) + 1, "gw_choicebox.c", 347);
    strcpy(node->str, str);
    return node;
}

/* Called when a row is picked. */
/* 0x00194ce8 */
void gw_choicebox_set_callback(GwChoice *box, int cb)
{
    box->cb = cb;
}

/* 0x00194cf0 */
void gw_choicebox_add_item(GwChoice *box, char *str)
{
    GwListNode *node;

    node = gw_choicebox_item_new(str);
    if (node) {
        box->list = gw_choicebox_list_append(box->list, node);
        box->count++;
    }
}

/* Only with items and not already open; starts on the selected row. */
/* 0x00194d40 */
static void gw_choicebox_open_popup(GwChoice *box)
{
    if (box->count > 0) {
        if (box->popup == 0) {
            box->popup = gw_choicebox_popup_create(0, box->ch, box);
            gw_window_activate(box->popup);
            do { } while (0);
            box->popup->slot[1] = box->sel;
            do { } while (0);
        }
    }
}

/* "NO SELECTED" without a selection. Bug: the range check lets sel == count through. */
/* 0x00194da8 */
static void gw_choicebox_draw(GwChoice *box, float scale)
{
    int cw;
    int ch;
    int y;
    int i;
    GwListNode *node;

    gw_draw_window_body(box, scale);
    gw_window_get_cell_size(box, &cw, &ch);
    y = (box->h - ch) / 2;
    if (box->sel < 0 || box->count < box->sel)
        gw_win_text(box, 0, y, "NO SELECTED");
    else {
        node = box->list;
        for (i = 0; i < box->sel; i++)
            node = node->next;
        gw_win_text(box, 0, y, node->str);
    }
}

/*
 * One cell high plus border, square arrow at the right; a click on the active box opens the list.
 */
/* 0x00194e68 */
static void gw_choicebox_layout(GwChoice *box)
{
    GwBtn *arrow;
    int cw;
    int ch;
    int sz;
    int right;

    gw_window_get_cell_size(box, &cw, &ch);
    sz = ch;
    box->h = sz + 2;
    arrow = box->arrow;
    if (arrow) {
        arrow->h = sz;
        arrow->w = sz;
        right = box->w - 1;
        arrow->y = 1;
        arrow->x = right - sz;
    }
    if (gw_window_is_active(box) && (gw_mouse_get_press() & 1))
        gw_choicebox_open_popup(box);
}

/* Bug: reads each node's next link after freeing it. */
/* 0x00194ef8 */
static void gw_choicebox_destroy(GwChoice *box)
{
    GwListNode *node;

    if (box->list) {
        node = box->list;
        do {
            gw_choicebox_item_free(node);
            node = node->next;
        } while (node);
        box->list = 0;
    }
}

/* parent must be a frame window, else 0. Nothing selected. */
/* 0x00194f48 */
GwChoice *gw_choicebox_create(char *name, int x, int y, int width, int height, void *parent)
{
    GwChoice *box;
    int cw;
    int ch;

    if (parent == 0)
        return 0;
    if (gw_window_is_frame(parent) == 0)
        return 0;
    gw_get_cell_size(&cw, &ch);
    box = gw_window_new(9, name, 0x4012c, x, y, width, ch, parent);
    if (box == 0)
        return 0;
    box->update = (GwCb)gw_choicebox_layout;
    box->draw = (GwCbF)gw_choicebox_draw;
    box->destroy = (GwCb)gw_choicebox_destroy;
    box->sel = -1;
    box->arrow = gw_icon_button_create(gw_choicebox_button_cb, 17, 1.0f, 0, 0, 0, 0, box);
    return box;
}

/* 0x00195050 */
static void gw_choicebox_nop(void)
{
}
