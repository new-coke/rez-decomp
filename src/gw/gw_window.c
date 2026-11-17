/*
 * Debug GUI windows: tree (siblings in draw order), frame windows with title bar, border and
 * boxes, move/resize hit testing, active and grabbed windows, per-frame update and draw passes.
 */

#include "types_gw.h"
#include "libc.h"
#include "gw_window.h"
#include "gw_button.h"
#include "gw_draw.h"
#include "gw_font.h"
#include "gw_heap.h"
#include "gw_menu.h"
#include "gw_mouse.h"
#include "gw_scrollbar.h"
#include "gw_system.h"
#include "ul_gx.h"

static int gw_point_to_local(GwWin *win, int *x, int *y);
static int gw_window_list_has_active(GwWin *win);
static void gw_window_get_hit_rect(GwWin *win, int *rect);
static int gw_window_add_abs_pos(GwWin *win, int *x, int *y);
static void gw_window_popup_menu_on_destroy(GwWin *pop);

extern int gw_default_draw_color;
extern int gw_default_colors[4];
extern int gw_desktop_win;
extern int gw_active_win;
extern int gw_root_win;
extern int gw_window_sizes[];

/* Plus a 2-pixel gap. */
/* 0x0018c730 */
static int gw_window_right(GwWin *win)
{
    return win->x + win->w + 2;
}

/* Plus a 2-pixel gap. */
/* 0x0018c748 */
static int gw_window_bottom(GwWin *win)
{
    return win->y + win->h + 2;
}

/* 0x0018c760 */
int gw_get_default_color(void)
{
    return gw_default_draw_color;
}

/* Four corner colours plus the text colour. */
/* 0x0018c768 */
void gw_set_default_colors(int *src, int draw_color)
{
    int i;

    for (i = 0; i < 4; i++)
        gw_default_colors[i] = src[i];
    gw_default_draw_color = draw_color;
}

/* Right and bottom edges stop dx, dy short of ref's. */
/* 0x0018c7a0 */
void gw_window_size_to_window(GwWin *win, GwWin *ref, int dx, int dy)
{
    int coords[4];

    gw_window_get_abs_pos(win, &coords[0], &coords[1]);
    gw_window_get_abs_pos(ref, &coords[2], &coords[3]);
    win->w = coords[2] + ref->w - coords[0] - dx;
    win->h = coords[3] + ref->h - coords[1] - dy;
}

/* Right and bottom edges stop dx, dy short of the parent's. */
/* 0x0018c838 */
void gw_window_size_to_parent(GwWin *win, int dx, int dy)
{
    GwWin *parent = win->parent;
    int coords[4];

    if (parent != 0) {
        gw_window_get_abs_pos(win, &coords[0], &coords[1]);
        gw_window_get_abs_pos(parent, &coords[2], &coords[3]);
        win->w = coords[2] + parent->w - coords[0] - dx;
        win->h = coords[3] + parent->h - coords[1] - dy;
    }
}

/* Either output may be null. */
/* 0x0018c8d8 */
void gw_window_get_cell_size(GwWin *win, int *cw, int *ch)
{
    if (cw != 0)
        *cw = win->cw;
    if (ch != 0)
        *ch = win->ch;
}

/* 0x0018c900 */
static void gw_window_set_font(GwWin *win, int font)
{
    win->font = font;
}

/* 0x0018c908 */
static int gw_window_get_font(GwWin *win)
{
    return win->font;
}

/* 0x0018c910 */
int *gw_get_default_colors(void)
{
    return gw_default_colors;
}

/* 0x0018c920 */
int gw_window_is_visible(GwWin *win)
{
    int hidden = win->flags & 0x80;

    return hidden == 0;
}

/* 0x0018c930 */
void gw_window_show(GwWin *win)
{
    win->flags &= ~0x80;
}

/* 0x0018c948 */
void gw_window_hide(GwWin *win)
{
    win->flags |= 0x80;
}

/* 0x0018c958 */
static int gw_window_is_kind8(GwWin *win)
{
    return win->kind == 8;
}

/* 0x0018c968 */
static int gw_window_is_text_field(GwWin *win)
{
    return win->kind == 7;
}

/* 0x0018c978 */
int gw_window_is_slider(GwWin *win)
{
    return win->kind == 6;
}

/* 0x0018c988 */
int gw_window_is_scrollbar(GwWin *win)
{
    return win->kind == 3;
}

/* Top-level window with a frame. */
/* 0x0018c998 */
int gw_window_is_frame(GwWin *win)
{
    return win->kind == 1;
}

/* Screen (x, y) in win's coordinates; returns whether inside. */
/* 0x0018c9a8 */
int gw_window_hit_local(GwWin *win, int x, int y, int *rx, int *ry)
{
    *rx = x;
    *ry = y;
    gw_point_to_local(win, rx, ry);
    if (*rx < 0)
        return 0;
    if (*ry < 0)
        return 0;
    if (win->w < *rx)
        return 0;
    return !(win->h < *ry);
}

/* Subtracts its position and, when parent-relative, its parents' too. */
/* 0x0018ca28 */
static int gw_point_to_local(GwWin *win, int *x, int *y)
{
    GwWin *parent;
    int ret;

    if (win != 0) {
        *x = *x - win->x;
        parent = win->parent;
        *y = *y - win->y;
        if (parent != 0 && (win->flags & 0x20) != 0) {
            ret = gw_point_to_local(parent, x, y);
            return ret;
        }
    }
}

/* Pointer in win's coordinates; returns whether inside. */
/* 0x0018ca80 */
int gw_window_hit_mouse(GwWin *win, int *lx, int *ly)
{
    int x;
    int y;

    gw_mouse_get_pos(&x, &y);
    return gw_window_hit_local(win, x, y, lx, ly);
}

/* Drawing depth. */
/* 0x0018cae0 */
float gw_window_get_scale(GwWin *win)
{
    return win->scale;
}

/* Returns the previous flags. */
/* 0x0018cae8 */
static int gw_window_set_flags(GwWin *win, int bits)
{
    int old = win->flags;

    win->flags = old | bits;
    return old;
}

/* Returns the previous flags. */
/* 0x0018caf8 */
static int gw_window_clear_flags(GwWin *win, int bits)
{
    int old = win->flags;

    win->flags = old & ~bits;
    return old;
}

/* Returns the previous flags. */
/* 0x0018cb10 */
int gw_window_replace_flags(GwWin *win, int flags)
{
    int old = win->flags;

    win->flags = flags;
    return old;
}

/* 0x0018cb20 */
int gw_window_get_flags(GwWin *win)
{
    return win->flags;
}

/* 0x0018cb28 */
void gw_window_set_user(GwWin *win, int user)
{
    win->user = user;
}

/* 0x0018cb30 */
int gw_window_get_user(GwWin *win)
{
    return win->user;
}

/* 0x0018cb38 */
void gw_window_set_slot(GwWin *win, int i, int val)
{
    win->slot[i] = val;
}

/* 0x0018cb48 */
int gw_window_get_slot(GwWin *win, int i)
{
    return win->slot[i];
}

/* 1 pixel. */
/* 0x0018cb58 */
int gw_get_border_width(void)
{
    return 1;
}

/* One cell plus 4 pixels. */
/* 0x0018cb60 */
int gw_get_title_bar_height(void)
{
    int sz[2];

    gw_get_cell_size(&sz[0], &sz[1]);
    sz[1] += 4;
    return sz[1];
}

/* 0x0018cb90 */
GwWin *gw_window_get_parent(GwWin *win)
{
    return win->parent;
}

/*
 * Frame windows: 9 inside, 10 title bar, 1..8 border (top, top-left, left, bottom-left, bottom,
 * bottom-right, right, top-right); 0 elsewhere or for other kinds.
 */
/* 0x0018cb98 */
int gw_window_hit_area(GwWin *win, int px, int py)
{
    int rect[4];
    int edge[4];
    int tw;
    int th;
    int border;

    if (gw_window_is_frame(win) == 0)
        return 0;
    gw_window_get_abs_rect(win, rect);
    if (win->flags & 1) {
        tw = win->w;
        th = gw_get_title_bar_height();
    } else {
        th = 0;
        tw = 0;
    }
    border = (win->flags & 2) ? 6 : 0;
    if (gw_rect_contains_point(rect, px, py) != 0)
        return 9;
    edge[2] = rect[0] + tw;
    edge[0] = rect[0];
    edge[3] = rect[1];
    edge[1] = rect[1] - th;
    if (gw_rect_contains_point(edge, px, py) != 0)
        return 10;
    edge[0] = rect[0];
    edge[2] = rect[2];
    edge[3] = rect[1] - th;
    edge[1] = rect[1] - th - border;
    if (gw_rect_contains_point(edge, px, py) != 0)
        return 1;
    edge[0] = rect[0] - border;
    edge[2] = rect[0];
    edge[1] = rect[1] - th - border;
    edge[3] = rect[1] - th;
    if (gw_rect_contains_point(edge, px, py) != 0)
        return 2;
    edge[0] = rect[0] - border;
    edge[2] = rect[0];
    edge[1] = rect[1] - th;
    edge[3] = rect[3];
    if (gw_rect_contains_point(edge, px, py) != 0)
        return 3;
    edge[2] = rect[0];
    edge[3] = rect[3] + border;
    edge[0] = rect[0] - border;
    edge[1] = rect[3];
    if (gw_rect_contains_point(edge, px, py) != 0)
        return 4;
    edge[0] = rect[0];
    edge[2] = rect[2];
    edge[1] = rect[3];
    edge[3] = rect[3] + border;
    if (gw_rect_contains_point(edge, px, py) != 0)
        return 5;
    edge[0] = rect[2];
    edge[1] = rect[3];
    edge[2] = rect[2] + border;
    edge[3] = rect[3] + border;
    if (gw_rect_contains_point(edge, px, py) != 0)
        return 6;
    edge[1] = rect[1] - th;
    edge[2] = rect[2] + border;
    edge[3] = rect[3];
    edge[0] = rect[2];
    if (gw_rect_contains_point(edge, px, py) != 0)
        return 7;
    edge[0] = rect[2];
    edge[2] = rect[2] + border;
    edge[1] = rect[1] - th - border;
    edge[3] = rect[1] - th;
    if (gw_rect_contains_point(edge, px, py) != 0)
        return 8;
    return 0;
}

/* 0x0018ce68 */
char *gw_window_get_title(GwWin *win)
{
    return win->title;
}

/* A copy, or none. */
/* 0x0018ce70 */
void gw_window_set_title(GwWin *win, char *title)
{
    if (win->title != 0) {
        gw_dc_free(win->title, "gw_window.c", 799);
        win->title = 0;
    }
    if (title != 0) {
        win->title = gw_dc_malloc(strlen(title) + 1, "gw_window.c", 803);
        if (win->title != 0)
            strcpy(win->title, title);
    }
}

/* In its parent; either output may be null. */
/* 0x0018cf00 */
void gw_window_get_pos(GwWin *win, int *x, int *y)
{
    if (x != 0)
        *x = win->x;
    if (y != 0)
        *y = win->y;
}

/* 0x0018cf28 */
void gw_window_set_pos(GwWin *win, int x, int y)
{
    win->x = x;
    win->y = y;
}

/* Either output may be null. */
/* 0x0018cf38 */
void gw_window_get_size(GwWin *win, int *width, int *height)
{
    if (width != 0)
        *width = win->w;
    if (height != 0)
        *height = win->h;
}

/* 0x0018cf60 */
void gw_window_set_size(GwWin *win, int width, int height)
{
    win->w = width;
    win->h = height;
}

/*
 * It and each frame window or pop-up above it move to the end of their sibling lists (drawn last).
 */
/* 0x0018cf70 */
static void gw_window_bring_to_front(GwWin *win)
{
    GwWin *root = (GwWin *)gw_desktop_win;
    GwWin *last;

again:
    if (win->kind == 1 || win->kind == 4) {
        last = win->next;
        if (last != 0) {
            last->prev = win->prev;
            if (win->prev != 0)
                win->prev->next = win->next;
            else if (win->parent != 0)
                win->parent->child = win->next;
            while (last->next != 0)
                last = last->next;
            last->next = win;
            win->prev = last;
            win->next = 0;
        }
    }
    win = win->parent;
    if (win != root && win != 0)
        goto again;
}

/* Frontmost, deepest shown window among win, its siblings and their children. */
/* 0x0018d020 */
void gw_window_find_at(GwWin *win, int x, int y, GwWin **out)
{
    int rect[4];

    while (win != 0) {
        if (gw_window_is_visible(win) != 0) {
            gw_window_get_hit_rect(win, rect);
            if (gw_rect_contains_point(rect, x, y) != 0)
                *out = win;
            if (win->child != 0)
                gw_window_find_at(win->child, x, y, out);
        }
        win = win->next;
    }
}

/* Nothing for a null window. */
/* 0x0018d0c0 */
void gw_window_set_handlers(GwWin *win, GwFn update, GwFn draw, GwFn close)
{
    if (win != 0) {
        win->update_handler = update;
        win->draw_handler = draw;
        win->close_handler = close;
    }
}

/* 0x0018d0e0 */
void gw_window_set_close_handler(GwWin *win, GwFn fn)
{
    if (win != 0)
        win->close_handler = fn;
}

/* 0x0018d0f0 */
static void gw_window_set_update_handler(GwWin *win, GwFn fn)
{
    if (win != 0)
        win->update_handler = fn;
}

/* 0x0018d100 */
void gw_window_set_draw_handler(GwWin *win, GwFn fn)
{
    if (win != 0)
        win->draw_handler = fn;
}

/* The window under the pointer. */
/* 0x0018d110 */
int gw_window_is_grabbed(int win)
{
    return win == gw_grabbed_win;
}

/* 0x0018d120 */
static int gw_get_grabbed_window(void)
{
    return gw_grabbed_win;
}

/* Window under the pointer; returns the previous one. */
/* 0x0018d128 */
static int gw_set_grabbed_window(int win)
{
    int old = gw_grabbed_win;

    gw_grabbed_win = win;
    return old;
}

/* It or any descendant. */
/* 0x0018d138 */
int gw_window_has_active(GwWin *win)
{
    int found;

    if (gw_window_is_active(win) != 0)
        return 1;
    if (win->child != 0) {
        found = gw_window_list_has_active(win->child);
        if (found != 0)
            return 1;
    }
    return 0;
}

/* In the list from win or below one of its windows. */
/* 0x0018d188 */
static int gw_window_list_has_active(GwWin *win)
{
    while (win != 0) {
        if (gw_window_is_active(win) != 0)
            return 1;
        if (win->child != 0 && gw_window_list_has_active(win->child) != 0)
            return 1;
        win = win->next;
    }
    return 0;
}

/* 0x0018d1f0 */
int gw_window_is_active(int win)
{
    return win == gw_active_win;
}

/* 0x0018d200 */
static int gw_get_active_window(void)
{
    return gw_active_win;
}

/* Brings it to the front; returns the previous one. */
/* 0x0018d208 */
int gw_window_activate(GwWin *win)
{
    int old = gw_active_win;

    gw_window_bring_to_front(win);
    gw_active_win = (int)win;
    return old;
}

/* 0x0018d240 */
static GwWin *gw_window_get_owner(GwWin *win)
{
    return win->owner;
}

/* 0x0018d248 */
void gw_window_set_owner(GwWin *win, GwWin *owner)
{
    win->owner = owner;
}

/* 1 without an owner. */
/* 0x0018d250 */
int gw_window_inside_owner(GwWin *win)
{
    int rect[4];
    int owner_rect[4];

    if (win->owner == 0)
        return 1;
    gw_window_get_abs_rect(win, rect);
    gw_window_get_abs_rect(win->owner, owner_rect);
    return gw_rect_contains_rect(rect, owner_rect);
}

/* {left, top, right, bottom}. */
/* 0x0018d2a8 */
int gw_rect_contains_rect(int *inner, int *outer)
{
    if (inner[0] < outer[0] || inner[2] > outer[2])
        return 0;
    if (inner[1] < outer[1])
        return 0;
    return !(inner[3] > outer[3]);
}

/* {left, top, right, bottom}, edges inclusive. */
/* 0x0018d300 */
int gw_rect_contains_point(int *rect, int x, int y)
{
    if (x < rect[0])
        return 0;
    if (rect[2] < x)
        return 0;
    if (y < rect[1])
        return 0;
    return !(rect[3] < y);
}

/* Title bar and border included. */
/* 0x0018d348 */
static int gw_window_hit_point(GwWin *win, int x, int y)
{
    int rect[4];

    gw_window_get_hit_rect(win, rect);
    return gw_rect_contains_point(rect, x, y);
}

/* Screen rectangle plus title bar and 6-pixel resize border when present. */
/* 0x0018d390 */
static void gw_window_get_hit_rect(GwWin *win, int *rect)
{
    gw_window_get_abs_rect(win, rect);
    if (win->flags & 1)
        rect[1] -= gw_get_title_bar_height();
    if (win->flags & 2) {
        rect[0] -= 6;
        rect[2] += 6;
        rect[1] -= 6;
        rect[3] += 6;
    }
}

/* Empty for other kinds. */
/* 0x0018d420 */
static void gw_frame_get_abs_rect(GwWin *win, int *rect)
{
    if (win->kind == 1) {
        gw_window_get_abs_pos(win, &rect[0], &rect[1]);
        rect[2] = rect[0] + win->w;
        rect[3] = rect[1] + win->h;
    } else {
        rect[2] = 0;
        rect[0] = 0;
        rect[3] = 0;
        rect[1] = 0;
    }
}

/* 0x0018d498 */
static void gw_window_get_screen_rect(GwWin *win, int *rect)
{
    gw_window_get_abs_pos(win, &rect[0], &rect[1]);
    rect[2] = rect[0] + win->w;
    rect[3] = rect[1] + win->h;
}

/* {left, top, right, bottom}. */
/* 0x0018d4f0 */
void gw_window_get_abs_rect(GwWin *win, int *rect)
{
    gw_window_get_abs_pos(win, &rect[0], &rect[1]);
    rect[2] = rect[0] + win->w;
    rect[3] = rect[1] + win->h;
}

/* 0x0018d548 */
void gw_window_get_abs_pos(GwWin *win, int *x, int *y)
{
    *y = 0;
    *x = 0;
    gw_window_add_abs_pos(win, x, y);
}

/* Adds its position and, when parent-relative, its parents' too. */
/* 0x0018d558 */
static int gw_window_add_abs_pos(GwWin *win, int *x, int *y)
{
    GwWin *parent;
    int ret;

    if (win != 0) {
        *x = *x + win->x;
        parent = win->parent;
        *y = *y + win->y;
        if (parent != 0 && (win->flags & 0x20) != 0) {
            ret = gw_window_add_abs_pos(parent, x, y);
            return ret;
        }
    }
}

/* All four; translucent unless alpha is 0xff. */
/* 0x0018d5b0 */
void gw_window_set_color(GwWin *win, unsigned col)
{
    win->col[3] = col;
    win->col[2] = col;
    win->col[1] = col;
    win->col[0] = col;
    if ((col & 0xff000000) == 0xff000000)
        win->translucent = 0;
    else
        win->translucent = 1;
}

/* Translucent unless every alpha is 0xff. */
/* 0x0018d5e0 */
void gw_window_set_colors(GwWin *win, unsigned *cols)
{
    int i;
    unsigned common;

    for (i = 0; i < 4; i++)
        win->col[i] = cols[i];
    common = cols[0] & cols[1] & cols[2] & cols[3];
    if ((common & 0xff000000) == 0xff000000)
        win->translucent = 0;
    else
        win->translucent = 1;
}

/* Adds to *n; children included. */
/* 0x0018d650 */
void gw_window_count(GwWin *win, int *n)
{
    while (win != 0) {
        *n = *n + 1;
        if (win->child != 0)
            gw_window_count(win->child, n);
        win = win->next;
    }
}

/* Last (frontmost) child; parent null = the desktop. */
/* 0x0018d6a8 */
static void gw_window_link(GwWin *win, GwWin *parent)
{
    GwWin *dst = (GwWin *)gw_desktop_win;
    GwWin *last;

    if (parent != 0)
        dst = parent;
    last = dst->child;
    if (last != 0) {
        while (last->next != 0)
            last = last->next;
        last->next = win;
        win->prev = last;
    } else {
        dst->child = win;
    }
    win->parent = dst;
}

/* Children too. Bug: reads each window's children after freeing it. */
/* 0x0018d6f8 */
static void gw_window_free_list(GwWin *win)
{
    GwWin *next;

    while (win != 0) {
        next = win->next;
        gw_window_set_title(win, 0);
        gw_dc_free(win, "gw_window.c", 1531);
        if (win->child != 0)
            gw_window_free_list(win->child);
        win = next;
    }
}

/* Children, sibling link, title, itself. */
/* 0x0018d770 */
static void gw_window_destroy(GwWin *win)
{
    if (win->child != 0)
        gw_window_free_list(win->child);
    if (win->prev != 0)
        win->prev->next = win->next;
    else if (win->parent != 0)
        win->parent->child = win->next;
    if (win->next != 0)
        win->next->prev = win->prev;
    if (win == (GwWin *)gw_root_win)
        gw_root_win = (int)win->next;
    gw_window_set_title(win, 0);
    gw_dc_free(win, "gw_window.c", 1575);
}

/* Top-level window behind the one holding win (next to activate on close), else the desktop. */
/* 0x0018d810 */
static GwWin *gw_window_prev_toplevel(GwWin *win)
{
    GwWin *prev;

    while (win->parent != (GwWin *)gw_desktop_win)
        win = win->parent;
    prev = win->prev;
    if (prev != 0)
        return prev;
    return (GwWin *)gw_desktop_win;
}

/* Children too: destroy and close handlers run once, others removed; freed by the next update. */
/* 0x0018d858 */
static void gw_window_close_list(GwWin *win)
{
    while (win != 0) {
        if (win->child != 0)
            gw_window_close_list(win->child);
        if (win->destroy != 0) {
            win->destroy(win);
            win->destroy = 0;
        }
        if (win->close_handler != 0) {
            win->close_handler(win);
            win->close_handler = 0;
        }
        win->update = 0;
        win->flags |= 0x80000000;
        win->draw = 0;
        win->update_handler = 0;
        win->draw_handler = 0;
        win = win->next;
    }
}

/* Freed by the next update pass. Returns its next sibling, or 0 if already closed. */
/* 0x0018d8f8 */
GwWin *gw_window_close(GwWin *win)
{
    GwWin *next;

    if (win == 0)
        return 0;
    if (win->flags & 0x80000000)
        return 0;
    next = win->next;
    if (win->child != 0)
        gw_window_close_list(win->child);
    if (win->destroy != 0) {
        win->destroy(win);
        win->destroy = 0;
    }
    if (win->close_handler != 0) {
        win->close_handler(win);
        win->close_handler = 0;
    }
    win->update = 0;
    win->draw = 0;
    win->update_handler = 0;
    win->draw_handler = 0;
    win->flags |= 0x80000000;
    return next;
}

/* Negative x and y cascade from the last new window; default colours, GUI font; frontmost child. */
/* 0x0018d9a8 */
GwWin *gw_window_new(int kind, char *name, int flags, int x, int y, int width, int height, GwWin *parent)
{
    GwWin *win;
    int rect[4];

    win = gw_dc_calloc(1, gw_window_sizes[kind], "gw_window.c", 1721);
    if (win == 0)
        return 0;
    win->flags = flags;
    win->kind = kind;
    gw_window_set_title(win, name);
    win->parent = parent;
    if (x < 0 && y < 0) {
        gw_get_desktop_rect(rect);
        gw_next_window_pos(&win->x, &win->y);
        if (rect[2] < win->x + win->w || rect[3] < win->y + win->h) {
            gw_next_window_pos(0, 0);
            gw_next_window_pos(&win->x, &win->y);
        }
    } else {
        win->x = x;
        win->y = y;
    }
    win->w = width;
    win->h = height;
    gw_window_set_colors(win, gw_default_colors);
    gw_window_set_draw_color(win, gw_default_draw_color);
    win->font = gw_get_font();
    gw_get_cell_size(&win->cw, &win->ch);
    win->owner = parent;
    gw_window_link(win, parent);
    return win;
}

/* Adds n << 16 to the bits of x; gives each window layer its own depth. */
/* 0x0018db50 */
float gw_float_bits_add(float x, int n)
{
    *(int *)&x = *(int *)&x + (n << 16);
    return x;
}

/* Own draw function, then the draw handler. */
/* 0x0018db70 */
static void gw_window_apply_scale(GwWin *win, float scale)
{
    float old_depth;
    unsigned col;

    win->scale = scale;
    old_depth = gw_font_set_depth(gw_float_bits_add(scale, 2));
    col = gw_font_set_color(win->color);
    if (win->draw != 0)
        win->draw(win, scale);
    if (win->draw_handler != 0)
        win->draw_handler(win);
    gw_font_set_color(col);
    gw_font_set_depth(old_depth);
}

/*
 * Closed windows are freed; others run update function and handler, get marked for drawing,
 * recurse.
 */
/* 0x0018dc08 */
void gw_window_update_all(GwWin *win)
{
    GwWin *next;

    while (win != 0) {
        next = win->next;
        if (gw_window_is_visible(win) != 0) {
            if ((win->flags & 0x80000000) != 0) {
                gw_window_destroy(win);
            } else {
                if (win->update != 0)
                    win->update(win);
                if (win->update_handler != 0)
                    win->update_handler(win);
                win->flags |= 0x40000000;
                if (win->child != 0)
                    gw_window_update_all(win->child);
            }
        }
        win = next;
    }
}

/* Shown windows inside their owner and updated this frame, each a little further forward. */
/* 0x0018dcd0 */
void gw_window_scale_pass(GwWin *win, float *scale)
{
    ulGxSetAlphaBlend(4, 5);
    ulGxSetTexFilter(0);
    ulGxSetTexFunc(0);
    while (win != 0) {
        if (gw_window_is_visible(win) != 0 && gw_window_inside_owner(win) != 0) {
            if ((win->flags & 0x40000000) != 0) {
                gw_window_apply_scale(win, *scale);
                win->flags &= 0xbfffffff;
            }
            *scale = gw_float_bits_add(*scale, 4);
            if (win->child != 0)
                gw_window_scale_pass(win->child, scale);
        }
        win = win->next;
    }
}

/* Close, maximize and minimize boxes. */
/* 0x0018ddb0 */
static void gw_window_show_title_buttons(GwWin *win, int show)
{
    if (show) {
        if (win->btn[0] != 0)
            gw_button_set_enabled(win->btn[0], 1);
        if (win->btn[1] != 0)
            gw_button_set_enabled(win->btn[1], 1);
        if (win->btn[2] != 0)
            gw_button_set_enabled(win->btn[2], 1);
    } else {
        if (win->btn[0] != 0)
            gw_button_set_enabled(win->btn[0], 0);
        if (win->btn[1] != 0)
            gw_button_set_enabled(win->btn[1], 0);
        if (win->btn[2] != 0)
            gw_button_set_enabled(win->btn[2], 0);
    }
}

/* 0x0018de58 */
static GwWin *gw_window_get_close_box(GwWin *win)
{
    return win->btn[0];
}

/* 0x0018de60 */
GwWin *gw_window_get_maximize_box(GwWin *win)
{
    return win->btn[1];
}

/* 0x0018de68 */
GwWin *gw_window_get_minimize_box(GwWin *win)
{
    return win->btn[2];
}

/* Unless win already has one. */
/* 0x0018de70 */
static GwWin *gw_window_popup_menu(GwWin *win, int list, int name, int x, int y)
{
    GwWin *pop;

    if (win->has_popup != 0)
        return 0;
    pop = gw_menu_popup_create(list, name, x, y, win);
    if (pop == 0)
        return 0;
    pop->destroy = gw_window_popup_menu_on_destroy;
    return pop;
}

/* Notes the window no longer has one. */
/* 0x0018dec8 */
static void gw_window_popup_menu_on_destroy(GwWin *pop)
{
    pop->parent->has_popup = 0;
}

/* Resize minimum. */
/* 0x0018ded8 */
static void gw_window_set_min_size(GwWin *win, int minw, int minh)
{
    win->minw = minw;
    win->minh = minh;
}

/* Resize minimum; either output may be null. */
/* 0x0018dee8 */
static void gw_window_get_min_size(GwWin *win, int *minw, int *minh)
{
    if (minw != 0)
        *minw = win->minw;
    if (minh != 0)
        *minh = win->minh;
}

/* From its scroll bars; either output may be null. */
/* 0x0018df10 */
static void gw_window_get_scroll(GwWin *win, int *sbw, int *sbh)
{
    if (sbw != 0)
        *sbw = win->scroll_x;
    if (sbh != 0)
        *sbh = win->scroll_y;
}

/* 0x0018df38 */
static GwWin *gw_window_get_hscrollbar(GwWin *win)
{
    return win->hscroll;
}

/* 0x0018df40 */
GwWin *gw_window_get_vscrollbar(GwWin *win)
{
    return win->vscroll;
}

/* Only a scroll bar, and only if none is set. */
/* 0x0018df48 */
static void gw_window_set_hscrollbar(GwWin *win, GwWin *sb)
{
    if (win->hscroll == 0 && sb != 0 && sb->kind == 3)
        win->hscroll = sb;
}

/* Only a scroll bar, and only if none is set. */
/* 0x0018df70 */
void gw_window_set_vscrollbar(GwWin *win, GwWin *sb)
{
    if (win->vscroll == 0 && sb != 0 && sb->kind == 3)
        win->vscroll = sb;
}

/* By its orientation; free bars only. */
/* 0x0018df98 */
void gw_window_add_scrollbar(GwWin *win, GwWin *sb)
{
    int orient = gw_scrollbar_get_orient(sb);

    switch (orient) {
    case 0:
        gw_window_set_hscrollbar(win, sb);
        break;
    case 1:
        gw_window_set_vscrollbar(win, sb);
        break;
    }
}

/* Body, then title bar if any. */
/* 0x0018e018 */
static void gw_window_draw(GwWin *win, float scale)
{
    gw_draw_window_body(win, scale);
    if (win->flags & 1)
        gw_draw_title_bar(win, scale);
}

/* Keeps the minimum height. */
/* 0x0018e070 */
static void gw_window_resize_top(GwWin *win, int delta)
{
    int height = win->h;
    int min = win->minh;
    int new_h = height - delta;

    if (new_h < min) {
        delta = height - min;
        win->h = min;
        win->y += delta;
    } else {
        win->h = new_h;
        win->y += delta;
    }
}

/* Keeps the minimum height. */
/* 0x0018e0a8 */
static void gw_window_resize_bottom(GwWin *win, int delta)
{
    int new_h = win->h + delta;

    if (new_h < win->minh)
        win->h = win->minh;
    else
        win->h = new_h;
}

/* Keeps the minimum width. */
/* 0x0018e0d0 */
static void gw_window_resize_left(GwWin *win, int delta)
{
    int width = win->w;
    int min = win->minw;
    int new_w = width - delta;

    if (new_w < min) {
        delta = width - min;
        win->w = min;
        win->x += delta;
    } else {
        win->w = new_w;
        win->x += delta;
    }
}

/* Keeps the minimum width. */
/* 0x0018e108 */
static void gw_window_resize_right(GwWin *win, int delta)
{
    int new_w = win->w + delta;

    if (new_w < win->minw)
        win->w = win->minw;
    else
        win->w = new_w;
}

/* edge 1..8 as gw_window_hit_area returns. */
/* 0x0018e130 */
static void gw_window_resize_by_edge(GwWin *win, int dx, int dy, int edge)
{
    if (win == 0)
        return;
    switch (edge) {
    case 2:
        gw_window_resize_left(win, dx);
    case 1:
        gw_window_resize_top(win, dy);
        break;
    case 3:
        gw_window_resize_left(win, dx);
        break;
    case 4:
        gw_window_resize_left(win, dx);
        gw_window_resize_bottom(win, dy);
        break;
    case 6:
        gw_window_resize_right(win, dx);
    case 5:
        gw_window_resize_bottom(win, dy);
        break;
    case 7:
        gw_window_resize_right(win, dx);
        break;
    case 8:
        gw_window_resize_right(win, dx);
        gw_window_resize_top(win, dy);
        break;
    }
}

/* Unless fixed in place. */
/* 0x0018e208 */
static void gw_window_move_by(GwWin *win, int dx, int dy)
{
    if ((win->flags & 0x100) == 0) {
        win->x += dx;
        win->y += dy;
    }
}

/*
 * 12-pixel strips along bottom and right, shortened when both exist; records the scroll position.
 */
/* 0x0018e238 */
static void gw_window_layout_scrollbars(GwWin *win)
{
    GwWin *hsb = gw_window_get_hscrollbar(win);
    GwWin *vsb = gw_window_get_vscrollbar(win);

    if (hsb != 0) {
        if (vsb != 0) {
            hsb->w = win->w - 14;
            hsb->h = 12;
            hsb->x = 1;
            hsb->y = win->h - 13;
            vsb->w = 12;
            vsb->h = win->h - 14;
            vsb->x = win->w - 13;
            vsb->y = 1;
        } else {
            hsb->w = win->w - 2;
            hsb->h = 12;
            hsb->x = 1;
            hsb->y = win->h - 13;
        }
    } else if (vsb != 0) {
        vsb->w = 12;
        vsb->h = win->h - 2;
        vsb->x = win->w - 13;
        vsb->y = 1;
    }
    if (hsb != 0)
        win->scroll_x = (int)gw_scrollbar_get_val(hsb);
    if (vsb != 0)
        win->scroll_y = (int)gw_scrollbar_get_val(vsb);
}

/* Under the pointer: left drag moves (title bar, or anywhere without one) or resizes (border). */
/* 0x0018e350 */
static void gw_window_update_drag(GwWin *win)
{
    int coords[8];
    unsigned btn;
    unsigned area;

    if (gw_window_is_grabbed((int)win) != 0) {
        btn = gw_mouse_get_stat();
        gw_mouse_get_pos(&coords[4], &coords[5]);
        gw_window_get_hit_rect(win, &coords[0]);
        switch (win->drag) {
        case 0:
        case 2:
        case 3:
            if (gw_rect_contains_point(&coords[0], coords[4], coords[5]) == 0)
                break;
            if ((btn & 0x10) == 0)
                break;
            win->drag = 1;
        case 1:
            if ((btn & 0x1000001) == 0x1000001) {
                area = gw_mouse_get_cursor() & 0xffff;
                gw_mouse_get_move(&coords[6], &coords[7]);
                if ((area == 9 && (win->flags & 1) == 0) || area == 10)
                    gw_window_move_by(win, coords[6], coords[7]);
                else if (area - 1 < 8)
                    gw_window_resize_by_edge(win, coords[6], coords[7], area);
            }
            if ((btn & 1) == 0)
                win->drag = 0;
            break;
        }
    }
    if (win->hscroll != 0 || win->vscroll != 0)
        gw_window_layout_scrollbars(win);
}

/* 0x0018e4b0 */
static void gw_frame_close(GwWin *win)
{
    gw_window_close(win);
}

/* Activates the window behind it if it was active. */
/* 0x0018e4b8 */
static void gw_frame_on_destroy(GwWin *win)
{
    if (gw_window_has_active(win) != 0)
        gw_window_activate(gw_window_prev_toplevel(win));
}

/*
 * Draggable, resizable down to 48 x 48; boxes when it has a title bar and boxes in flags;
 * activated.
 */
/* 0x0018e500 */
GwWin *gw_window_open(char *title, int flags, int x, int y, int width, int height, int parent)
{
    GwWin *win;

    win = gw_window_new(1, title, flags, x, y, width, height, parent);
    if (win == 0)
        return 0;
    win->update = gw_window_update_drag;
    win->draw = gw_window_draw;
    win->destroy = gw_frame_on_destroy;
    if ((flags & 0x11) == 0x11) {
        win->btn[0] = gw_close_box_create(win);
        win->btn[1] = gw_maximize_box_create(win);
        win->btn[2] = gw_minimize_box_create(win);
    }
    win->minw = 48;
    win->minh = 48;
    gw_window_activate(win);
    return win;
}
