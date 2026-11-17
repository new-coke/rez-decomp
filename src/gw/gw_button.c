/*
 * Debug GUI buttons: push, toggle and icon buttons, and the title-bar close/maximize/minimize
 * boxes.
 */

#include "types_gw.h"
#include "libc.h"
#include "gw_button.h"
#include "gw_draw.h"
#include "gw_font.h"
#include "gw_mouse.h"
#include "gw_system.h"
#include "gw_window.h"
#include "color.h"

static void gw_toggle_button_draw(GwBtn *btn, float z);

/* The window title. */
/* 0x0018fd00 */
static char *gw_button_get_title(GwWin *win)
{
    return gw_window_get_title(win);
}

/* The window title. */
/* 0x0018fd20 */
static void gw_button_set_title(GwWin *win, char *title)
{
    gw_window_set_title(win, title);
}

/* Ignored while disabled. */
/* 0x0018fd28 */
void gw_toggle_button_set(GwBtn *btn, int on)
{
    if (!(btn->flags & GW_F_DISABLED)) {
        btn->checked = on;
        if (on != 0) {
            btn->state = 1;
            btn->flags = (btn->flags & ~GW_F_RELEASED) | GW_F_PRESSED;
        } else {
            btn->state = 0;
            btn->flags = (btn->flags & ~GW_F_PRESSED) | GW_F_RELEASED;
        }
    }
}

/* 0x0018fd88 */
int gw_toggle_button_get(GwBtn *btn)
{
    return btn->checked;
}

/*
 * Active, inside its owner and enabled: drawn by value; a click flips it and calls cb with the old
 * value.
 */
/* 0x0018fd90 */
static void gw_toggle_button_update(GwBtn *btn)
{
    if (gw_window_is_active((GwWin *)btn) == 0)
        return;
    if (gw_window_inside_owner((GwWin *)btn) == 0)
        return;
    if (btn->flags & GW_F_DISABLED)
        return;
    if (btn->checked != 0) {
        btn->flags = (btn->flags & ~GW_F_RELEASED) | GW_F_PRESSED;
        if (gw_mouse_get_press() & 1) {
            btn->checked = 0;
            btn->state = 0;
            if (btn->cb != 0)
                btn->cb((GwWin *)btn, 1);
        }
    } else {
        btn->flags = (btn->flags & ~GW_F_PRESSED) | GW_F_RELEASED;
        if (gw_mouse_get_press() & 1) {
            btn->checked = 1;
            btn->state = 1;
            if (btn->cb != 0)
                btn->cb((GwWin *)btn, 0);
        }
    }
}

/* Starts off. Width or height 0 fits the label. */
/* 0x0018fe70 */
GwBtn *gw_toggle_button_create(GwBtnFn cb, char *name, int x, int y, int width, int height, GwWin *parent)
{
    int sz[2];
    GwBtn *btn;

    gw_get_cell_size(&sz[0], &sz[1]);
    if (width <= 0) {
        width = 0;
        if (name != 0)
            width = strlen(name) * sz[0] + 2;
    }
    if (height <= 0)
        height = sz[1] + 2;
    btn = gw_window_new(2, name, 364, x, y, width, height, parent);
    if (btn == 0)
        return 0;
    btn->update = gw_toggle_button_update;
    btn->draw = gw_toggle_button_draw;
    btn->cb = cb;
    btn->state = 0;
    btn->toggle = 0;
    btn->checked = 0;
    return btn;
}

/* Icon centred, grey when disabled, shifted one pixel down-right while pressed. */
/* 0x0018ff70 */
static void gw_icon_button_draw(GwBtn *btn, float z)
{
    int x, y;
    unsigned prev;

    gw_draw_window_body((GwWin *)btn, z);
    x = (btn->w - 8) / 2;
    y = (btn->h - 8) / 2;
    if (btn->flags & GW_F_DISABLED) {
        prev = gw_font_set_color(ARGB(0x80, 0x40, 0x40, 0x40));
        if (btn->title != 0)
            gw_draw_text_at((GwWin *)btn, x, y, btn->icon, btn->icon_scale);
    } else {
        switch (btn->state) {
        case 0:
            prev = gw_font_set_color(ARGB(0x80, 0, 0, 0));
            if (btn->title != 0)
                gw_draw_text_at((GwWin *)btn, x, y, btn->icon, btn->icon_scale);
            break;
        case 1:
            prev = gw_font_set_color(ARGB(0x80, 0, 0, 0));
            if (btn->title != 0)
                gw_draw_text_at((GwWin *)btn, x + 1, y + 1, btn->icon, btn->icon_scale);
            break;
        }
    }
    gw_font_set_color(prev);
}

/* icon 8..30, else 0. Width or height 0 gives 10. */
/* 0x00190078 */
GwBtn *gw_icon_button_create(GwBtnFn cb, int icon, float scale, int x, int y, int width, int height, GwWin *parent)
{
    GwBtn *btn;

    if ((unsigned)(icon - 8) > 22)
        return 0;
    if (width <= 0)
        width = 10;
    if (height <= 0)
        height = 10;
    btn = gw_window_new(2, "Icon Button", 364, x, y, width, height, parent);
    if (btn == 0)
        return 0;
    btn->update = gw_icon_button_update;
    btn->draw = gw_icon_button_draw;
    btn->cb = cb;
    btn->toggle = 1;
    btn->icon = icon;
    btn->icon_scale = scale;
    btn->state = 0;
    return btn;
}

/* 0x00190148 */
void gw_button_set_callback(GwBtn *btn, GwBtnFn cb)
{
    btn->cb = cb;
}

/* Other window kinds are left alone. */
/* 0x00190150 */
void gw_button_set_enabled(GwBtn *btn, int enable)
{
    if (btn->kind == 2) {
        if (enable)
            btn->flags &= ~GW_F_DISABLED;
        else
            btn->flags |= GW_F_DISABLED;
    }
}

/* Label centred, grey when disabled, shifted one pixel down-right while pressed. */
/* 0x00190198 */
static void gw_toggle_button_draw(GwBtn *btn, float z)
{
    int sz[6];
    int len;
    int x, y;
    unsigned prev;

    gw_draw_window_body((GwWin *)btn, z);
    gw_get_cell_size(&sz[4], &sz[5]);
    len = btn->title != 0 ? strlen(btn->title) : 0;
    x = (btn->w - len * sz[4]) / 2;
    y = (btn->h - sz[5]) / 2;
    if (btn->flags & GW_F_DISABLED) {
        prev = gw_font_set_color(ARGB(0x80, 0x40, 0x40, 0x40));
        if (btn->title != 0)
            gw_win_text_clip((GwWin *)btn, x, y, btn->w, btn->h, btn->title);
    } else {
        switch (btn->state) {
        case 0:
            prev = gw_font_set_color(ARGB(0x80, 0, 0, 0));
            if (btn->title != 0)
                gw_win_text_clip((GwWin *)btn, x, y, btn->w, btn->h, btn->title);
            break;
        case 1:
            prev = gw_font_set_color(ARGB(0x80, 0, 0, 0));
            if (btn->title != 0)
                gw_win_text_clip((GwWin *)btn, x + 1, y + 1, btn->w + 1, btn->h + 1, btn->title);
            break;
        }
    }
    gw_font_set_color(prev);
}

/* While held, cb gets 0 per frame and 1 on release inside. Leaving the button lets go silently. */
/* 0x001902e0 */
void gw_icon_button_update(GwBtn *btn)
{
    int rect[4];
    int pos[2];

    btn->flags = (btn->flags & ~GW_F_PRESSED) | GW_F_RELEASED;
    if (gw_window_is_active((GwWin *)btn) == 0)
        return;
    if (gw_window_inside_owner((GwWin *)btn) == 0)
        return;
    if (btn->flags & GW_F_DISABLED)
        return;
    gw_window_get_abs_rect((GwWin *)btn, rect);
    gw_mouse_get_pos(&pos[0], &pos[1]);
    switch (btn->state) {
    case 0:
        if (gw_rect_contains_point(rect, pos[0], pos[1]) == 0)
            return;
        if (!(gw_mouse_stat & 0x10))
            return;
        btn->state = 1;
    case 1:
        btn->flags = (btn->flags & ~GW_F_RELEASED) | GW_F_PRESSED;
        if (gw_rect_contains_point(rect, pos[0], pos[1]) != 0) {
            if (gw_mouse_stat & 0x100) {
                btn->state = 0;
                if (btn->cb != 0)
                    btn->cb((GwWin *)btn, 1);
            } else {
                if (btn->cb != 0)
                    btn->cb((GwWin *)btn, 0);
            }
        } else {
            btn->state = 0;
        }
        break;
    }
}

/* 0x00190420 */
static void gw_button_close(GwWin *win)
{
    gw_window_close(win);
}

/* Width or height 0 fits the label. */
/* 0x00190428 */
GwBtn *gw_push_button_create(GwBtnFn cb, char *name, int x, int y, int width, int height, GwWin *parent)
{
    int sz[2];
    GwBtn *btn;

    gw_get_cell_size(&sz[0], &sz[1]);
    if (width <= 0) {
        width = 0;
        if (name != 0)
            width = strlen(name) * sz[0] + 2;
    }
    if (height <= 0)
        height = sz[1] + 2;
    btn = gw_window_new(2, name, 364, x, y, width, height, parent);
    if (btn == 0)
        return 0;
    btn->update = gw_icon_button_update;
    btn->draw = gw_toggle_button_draw;
    btn->cb = cb;
    btn->state = 0;
    btn->toggle = 0;
    return btn;
}

/* Third from the right edge, centred in the title bar above the window. */
/* 0x00190528 */
void gw_titlebar_button_pos(GwWin *btn, GwWin *parent, int *px, int *py)
{
    int th = gw_get_title_bar_height();
    int bd = gw_get_border_width();
    int ofs, pair_ofs;

    ofs = btn->w + bd + 2;
    pair_ofs = (btn->w + bd * 2) << 1;
    *px = parent->w - ofs - pair_ofs;
    *py = -((th - btn->h) / 2) - btn->h;
}

/*
 * Square box one cell high, centred in the title bar, from the right by btn->box (0 close, 1 max, 2
 * min).
 */
/* 0x001905c8 */
static void gw_titlebar_button_layout(GwBtn *btn)
{
    int sz[2];
    GwWin *parent;

    gw_get_cell_size(&sz[0], &sz[1]);
    btn->h = btn->w = sz[1];
    btn->y = -((gw_get_title_bar_height() - btn->h) / 2) - btn->h;
    parent = btn->parent;
    if (parent != 0) {
        switch (btn->box) {
        case 0: {
            int ofs = btn->w + 3;
            btn->x = parent->w - ofs;
            break;
        }
        case 1: {
            int ofs = parent->w - btn->w;
            ofs -= 5;
            btn->x = ofs - btn->w;
            break;
        }
        case 2: {
            int ofs = btn->w + 3;
            int pair_ofs = (btn->w + 2) << 1;
            btn->x = parent->w - ofs - pair_ofs;
            break;
        }
        }
    }
    gw_icon_button_update(btn);
}

/*
 * On release. Normal: saves the rectangle in frame->saved, shrinks to a 100 x 1 bar (state 2).
 * Minimized: restores (state 0). Maximized (3): saved position as a 100 x 1 bar.
 */
/* 0x001906b8 */
static void gw_minimize_button_callback(GwWin *btn, int ev)
{
    GwFrame *frame;

    switch (ev) {
    case 1:
        frame = (GwFrame *)btn->parent;
        switch (frame->state) {
        case 0:
            {
                int sh = frame->h;
                frame->state = 2;
                frame->saved[0] = frame->x;
                frame->saved[1] = frame->y;
                frame->saved[2] = frame->w;
                frame->saved[3] = sh;
            }
            frame->w = 100;
            do { } while (0);
            frame->h = ev;
            break;
        case 3:
            frame->state = 2;
            frame->x = frame->saved[0];
            frame->y = frame->saved[1];
            frame->w = 100;
            do { } while (0);
            frame->h = ev;
            break;
        case 2:
            frame->x = frame->saved[0];
            frame->y = frame->saved[1];
            frame->w = frame->saved[2];
            frame->h = frame->saved[3];
            frame->state = 0;
            break;
        }
        break;
    }
}

/*
 * On release. Normal saves its rectangle; normal or minimized fills the desktop (3); maximized
 * restores (0).
 */
/* 0x00190780 */
static void gw_maximize_button_callback(GwWin *btn, int ev)
{
    int rect[4];
    GwFrame *frame;

    if (ev == 1) {
        frame = (GwFrame *)btn->parent;
        gw_get_desktop_rect(rect);
        switch (frame->state) {
        case 0:
            frame->saved[0] = frame->x;
            frame->saved[1] = frame->y;
            frame->saved[2] = frame->w;
            frame->saved[3] = frame->h;
        case 2:
            frame->x = rect[0];
            frame->y = rect[1] + gw_get_title_bar_height();
            frame->w = rect[2] - rect[0];
            frame->h = rect[3] - rect[1] - gw_get_title_bar_height();
            frame->state = 3;
            break;
        case 3:
            frame->x = frame->saved[0];
            frame->y = frame->saved[1];
            frame->w = frame->saved[2];
            frame->h = frame->saved[3];
            frame->state = 0;
            break;
        }
    }
}

/* Closes the parent on release. */
/* 0x00190878 */
static int gw_close_button_callback(GwWin *btn, int ev)
{
    if (ev == 1) {
        if (btn->parent != 0)
            return gw_window_close(btn->parent);
    }
}

/* Right end of the title bar. */
/* 0x001908a8 */
GwBtn *gw_close_box_create(GwWin *parent)
{
    GwBtn *btn;

    btn = gw_icon_button_create(gw_close_button_callback, 8, 1.0f, 0, 0, 8, 8, parent);
    if (btn == 0)
        return 0;
    gw_window_set_title((GwWin *)btn, "Close Box");
    ((GwFrame *)btn)->owner = 0;
    btn->box = 0;
    btn->update = gw_titlebar_button_layout;
    btn->state = 0;
    return btn;
}

/* Second from the right of the title bar. */
/* 0x00190930 */
GwBtn *gw_maximize_box_create(GwWin *parent)
{
    GwBtn *btn;

    btn = gw_icon_button_create(gw_maximize_button_callback, 9, 1.0f, 0, 0, 8, 8, parent);
    if (btn == 0)
        return 0;
    gw_window_set_title((GwWin *)btn, "Maximize");
    ((GwFrame *)btn)->owner = 0;
    btn->box = 1;
    btn->update = gw_titlebar_button_layout;
    btn->state = 0;
    return btn;
}

/* Third from the right of the title bar. */
/* 0x001909b8 */
GwBtn *gw_minimize_box_create(GwWin *parent)
{
    GwBtn *btn;

    btn = gw_icon_button_create(gw_minimize_button_callback, 10, 1.0f, 0, 0, 8, 8, parent);
    if (btn == 0)
        return 0;
    gw_window_set_title((GwWin *)btn, "Minimize");
    ((GwFrame *)btn)->owner = 0;
    btn->box = 2;
    btn->update = gw_titlebar_button_layout;
    btn->state = 0;
    return btn;
}
