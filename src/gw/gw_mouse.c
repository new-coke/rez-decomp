/*
 * Debug GUI pointer driven by the pad (d-pad and left stick move; Cross left, Circle right):
 * press/release/repeat/double-click, cursor shape per window area, idle fade, 'Mouse Info'.
 */

#include "types_gw.h"
#include "gw_mouse.h"
#include "libc.h"
#include "gw_draw.h"
#include "gw_font.h"
#include "gw_menu.h"
#include "gw_system.h"
#include "gw_window.h"
#include "ps_pad.h"
#include "ul_vu0.h"

extern int gw_pointer_fade_time;
extern int gw_mouse_wheel;
extern int gw_mouse_dclick;
extern int gw_mouse_repeat;
extern int gw_mouse_release;
extern int gw_mouse_press;
extern int gw_mouse_on;
extern int gw_mouse_cursor;
extern int gw_mouse_prev_x;
extern int gw_mouse_prev_y;
extern float gw_mouse_fx;
extern float gw_mouse_fy;
extern float gw_mouse_speed;
extern int gw_mouse_enabled;
extern int gw_desktop_win;
extern int gw_mouse_cursor_icons[];

/* Idle frames before fading; 0 never. */
/* 0x00190a40 */
static void gw_set_pointer_fade_time(int frames)
{
    gw_pointer_fade_time = frames;
}

/* Position in win's coordinates; returns whether inside. */
/* 0x00190a48 */
int gw_win_mouse_local(GwWin *win, int *rx, int *ry)
{
    int x, y;

    gw_mouse_get_pos(&x, &y);
    return gw_window_hit_local(win, x, y, rx, ry);
}

/* 0x00190aa8 */
static int gw_win_mouse_inside(GwWin *win)
{
    int x, y, rx, ry;

    gw_mouse_get_pos(&x, &y);
    return gw_window_hit_local(win, x, y, &rx, &ry) != 0;
}

/* 0x00190af0 */
int gw_mouse_get_wheel(void)
{
    return gw_mouse_wheel;
}

/* 1 left, 2 right. */
/* 0x00190b00 */
int gw_mouse_get_dclick(void)
{
    return gw_mouse_dclick;
}

/* 1 left, 2 right. */
/* 0x00190b10 */
int gw_mouse_get_repeat(void)
{
    return gw_mouse_repeat;
}

/* 1 left, 2 right. */
/* 0x00190b20 */
int gw_mouse_get_release(void)
{
    return gw_mouse_release;
}

/* 1 left, 2 right. */
/* 0x00190b30 */
int gw_mouse_get_press(void)
{
    return gw_mouse_press;
}

/* 1 left, 2 right. */
/* 0x00190b40 */
int gw_mouse_get_on(void)
{
    return gw_mouse_on;
}

/* 0x00190b50 */
int gw_mouse_get_cursor(void)
{
    return gw_mouse_cursor;
}

/*
 * Held 0x1/0x2, pressed 0x10/0x20, released 0x100/0x200, repeat from bit 12, dclick from 16,
 * moving.
 */
/* 0x00190b60 */
int gw_mouse_get_stat(void)
{
    return gw_mouse_stat;
}

/* 0x00190b70 */
int gw_mouse_moved(void)
{
    GwMouse *ms = &gw_mouse_state;

    if (ms->dx != 0 || ms->dy != 0)
        return 1;
    return 0;
}

/* Either output may be null; returns whether it moved. */
/* 0x00190ba0 */
int gw_mouse_get_move(int *dx, int *dy)
{
    GwMouse *ms = &gw_mouse_state;

    if (dx != 0)
        *dx = ms->dx;
    if (dy != 0)
        *dy = ms->dy;
    if (ms->dx != 0 || ms->dy != 0)
        return 1;
    return 0;
}

/* Either output may be null. */
/* 0x00190bf0 */
void gw_mouse_get_prev_pos(int *x, int *y)
{
    if (x != 0)
        *x = gw_mouse_prev_x;
    if (y != 0)
        *y = gw_mouse_prev_y;
}

/* Whole pixels; either output may be null. */
/* 0x00190c18 */
void gw_mouse_get_pos(int *x, int *y)
{
    if (x != 0)
        *x = (int)gw_mouse_fx;
    if (y != 0)
        *y = (int)gw_mouse_fy;
}

/* Kept inside the desktop; no movement this frame. */
/* 0x00190c48 */
static void gw_mouse_set_pos(int x, int y)
{
    GwMouse *ms = &gw_mouse_state;
    int rect[4];

    gw_get_desktop_rect(rect);
    if (x < rect[0])
        x = rect[0];
    else if (x > rect[2] - 1)
        x = rect[2] - 1;
    if (y < rect[1])
        y = rect[1];
    else if (rect[3] < y)
        y = rect[3] - 1;
    ms->x = ms->prev_x = x;
    ms->y = ms->prev_y = y;
    ms->fx = (float)x;
    ms->fy = (float)y;
}

/* Frames held before repeating; returns the previous delay. */
/* 0x00190d00 */
static int gw_mouse_set_repeat_delay(int frames)
{
    GwMouse *ms = &gw_mouse_state;
    int old = ms->repeat_delay;

    ms->repeat_delay = frames;
    return old;
}

/* Frames between repeats; returns the previous interval. */
/* 0x00190d18 */
static int gw_mouse_set_repeat_rate(int frames)
{
    GwMouse *ms = &gw_mouse_state;
    int old = ms->repeat_rate;

    ms->repeat_rate = frames;
    return old;
}

/* Pixels per frame; returns the previous speed. */
/* 0x00190d30 */
static float gw_mouse_set_speed(float speed)
{
    float old = gw_mouse_speed;

    gw_mouse_speed = speed;
    return old;
}

/* Cross = left, Circle = right. */
/* 0x00190d40 */
static void gw_mouse_read_buttons(GwMouse *ms, int *pad)
{
    ms->stat &= ~0x333;
    if (pad[10] & 0x40) {
        ms->stat |= 0x1;
        ms->on |= 1;
    }
    if (pad[10] & 0x20) {
        ms->stat |= 0x2;
        ms->on |= 2;
    }
    if (pad[12] & 0x40) {
        ms->stat |= 0x10;
        ms->press |= 1;
    }
    if (pad[12] & 0x20) {
        ms->stat |= 0x20;
        ms->press |= 2;
    }
    if (pad[13] & 0x40) {
        ms->stat |= 0x100;
        ms->release |= 1;
    }
    if (pad[13] & 0x20) {
        ms->stat |= 0x200;
        ms->release |= 2;
    }
}

/* Digital or analog pad; double click = second press of a button within dclick_time frames. */
/* 0x00190e40 */
static void gw_mouse_update_buttons(GwMouse *ms, unsigned char *pad)
{
    int press;

    ms->release = 0;
    ms->press = 0;
    ms->on = 0;
    ms->dclick = 0;
    ms->repeat = 0;
    if (pad[37] == 4 || pad[37] == 7)
        gw_mouse_read_buttons(ms, pad);
    press = ms->press;
    if (press & 0xf) {
        ms->repeat_cnt = 0;
        ms->repeat = press;
    } else {
        int held = ms->on;

        if (held & 0xf) {
            ms->repeat_cnt++;
            if (ms->repeat_delay < ms->repeat_cnt) {
                ms->repeat = held;
                ms->repeat_cnt = ms->repeat_cnt - ms->repeat_rate;
            }
        } else {
            ms->repeat_cnt = 0;
        }
    }
    if (ms->press & 0xf) {
        press = ms->press;
        if (ms->dclick_win == press) {
            if (ms->dclick_cnt <= ms->dclick_time) {
                ms->dclick = press;
                ms->dclick_win = 0;
            } else {
                ms->dclick_cnt = 0;
            }
        } else {
            ms->dclick_win = press;
            ms->dclick_cnt = 0;
        }
    } else {
        ms->dclick_cnt++;
    }
    ms->stat = ms->stat | (ms->repeat << 12) | (ms->dclick << 16);
}

/*
 * D-pad at gw_mouse_speed (unless gw_input_flags bit 2), left stick adds up to 3x; sets the moving
 * bit.
 */
/* 0x00190f50 */
void gw_mouse_read_pad_move(GwMouse *ms, unsigned char *pad)
{
    float vy = 0.0f;
    float vx = vy;
    float dir[4];
    float stick[2];
    float k;

    if (!(gw_input_flags & 2)) {
        if (((int *)pad)[10] & 0x8000)
            vx = -gw_mouse_speed;
        else if (((int *)pad)[10] & 0x2000)
            vx = gw_mouse_speed;
        if (((int *)pad)[10] & 0x1000)
            vy = -gw_mouse_speed;
        else if (((int *)pad)[10] & 0x4000)
            vy = gw_mouse_speed;
    }
    psPadStickToFloat(pad[66], pad[67], 32, &stick[0], &stick[1]);
    dir[0] = stick[0];
    dir[1] = stick[1];
    dir[2] = 0.0f;
    dir[3] = 1.0f;
    vu0_vec3_normalize(dir, dir);
    k = sqrtf(stick[0] * stick[0] + stick[1] * stick[1]) * gw_mouse_speed * 3.0f;
    vx += k * dir[0];
    vy += k * dir[1];
    ms->vx = vx;
    ms->vy = vy;
    if (vx != 0.0f || vy != 0.0f)
        ms->stat |= 0x1000000;
    else
        ms->stat &= ~0x1000000;
    ms->wheel = 0;
}

/* Kept inside the desktop; records the previous position and movement. */
/* 0x001910d8 */
static void gw_mouse_update_pos(GwMouse *ms, unsigned char *pad)
{
    int rect[4];
    int nx, ny, ox, oy;

    ms->vx = ms->vy = 0.0f;
    if (pad[37] == 4 || pad[37] == 7)
        gw_mouse_read_pad_move(ms, pad);
    ms->fx = ms->fx + ms->vx;
    ms->fy = ms->fy + ms->vy;
    gw_get_desktop_rect(rect);
    if ((float)rect[2] - 1.0f < ms->fx)
        ms->fx = (float)rect[2] - 1.0f;
    else if (ms->fx < (float)rect[0])
        ms->fx = (float)rect[0];
    if ((float)rect[3] - 1.0f < ms->fy)
        ms->fy = (float)rect[3] - 1.0f;
    else if (ms->fy < (float)rect[1])
        ms->fy = (float)rect[1];
    nx = (int)ms->fx;
    ny = (int)ms->fy;
    ox = ms->x;
    oy = ms->y;
    ms->x = nx;
    ms->y = ny;
    ms->prev_x = ox;
    ms->dx = nx - ox;
    ms->dy = ny - oy;
    ms->prev_y = oy;
}

/* 0x00191230 */
static void gw_mouse_set_enabled(int on)
{
    gw_mouse_enabled = on;
}

/*
 * Window and area under the pointer (kept while left is held, so drags stay on their window),
 * press origin, cursor shape (move/resize), move, activate on click, idle count.
 */
/* 0x00191240 */
void gw_mouse_update(GwMouse *ms)
{
    unsigned char *pad;
    GwWin *win;

    if (ms->enabled == 0)
        return;
    pad = psPadGet(gw_get_pad_port());
    gw_mouse_update_buttons(ms, pad);
    gw_window_find_at(gw_desktop_win, ms->x, ms->y, &gw_grabbed_win);
    if (!(ms->stat & 1)) {
        if (gw_grabbed_win != 0) {
            ms->under = (GwWin *)gw_grabbed_win;
            ms->area = gw_window_hit_area(gw_grabbed_win, ms->x, ms->y);
        } else {
            ms->under = 0;
        }
    }
    if (ms->stat & 0x30) {
        ms->drag_x = ms->x;
        ms->drag_y = ms->y;
    }
    win = ms->under;
    ms->cursor = ms->area;
    if (win != 0) {
        if ((ms->area == 9 && !(win->flags & 1)) || ms->area == 10)
            ms->cursor = ms->cursor | 0x40000000;
        else if ((unsigned)(ms->area - 1) < 8)
            ms->cursor = ms->area | 0x20000000;
    }
    gw_mouse_update_pos(ms, pad);
    if ((ms->stat & 0x30) && ms->under != 0)
        gw_window_activate(win);
    if (!gw_mouse_moved() && !gw_mouse_get_on())
        ms->idle++;
    else
        ms->idle = 0;
}

/* After gw_pointer_fade_time idle frames fades over about two seconds, then is not drawn. */
/* 0x001913b8 */
void gw_mouse_draw(GwMouse *ms, float scale)
{
    unsigned a;
    unsigned col;
    unsigned prev;
    unsigned short cur;
    float old;

    if (ms->enabled == 0)
        return;
    if (gw_pointer_fade_time == 0 || ms->idle < gw_pointer_fade_time) {
        a = 0xff000000;
    } else {
        a = (ms->idle - gw_pointer_fade_time) << 1;
        if (a >= 255)
            return;
        a = (255 - a) << 24;
    }
    cur = ms->cursor;
    old = gw_font_set_depth(scale);
    col = a | 0xffffff;
    prev = gw_font_set_color((((((col >> 24) + 1) >> 1) << 24) | (((((col >> 8) & 0xff) + 1) >> 1) << 8))
                        | (((((col >> 16) & 0xff) + 1) >> 1) | (((((col & 0xff)) + 1) >> 1) << 16)));
    gw_font_icon(gw_mouse_cursor_icons[cur], (float)ms->x, (float)ms->y);
    gw_font_set_color(prev);
    gw_font_set_depth(old);
}

/*
 * Centred, on; pad_port unused. Speed 3, repeat 30 then 6, dclick 15, fade after 300 idle frames.
 */
/* 0x001914f8 */
void gw_mouse_init(int pad_port, int width, int height)
{
    GwMouse *ms = &gw_mouse_state;
    int cx, cy;

    memset(ms, 0, 136);
    cx = width / 2;
    cy = height / 2;
    gw_mouse_set_pos(cx, cy);
    ms->pad_port = gw_get_pad_port();
    gw_mouse_set_enabled(1);
    gw_mouse_set_speed(3.0f);
    ms->repeat_delay = 30;
    ms->repeat_rate = 6;
    ms->dclick_time = 15;
    gw_pointer_fade_time = 300;
}

/* 0x001915a8 */
static void gw_mouse_info_draw(GwWin *win)
{
    GwMouse *ms = &gw_mouse_state;
    int lh = gw_font_cell_h;
    int y;

    gw_win_printf(win, 0, 0, "pos        = (%3d,%3d)", ms->x, ms->y);
    y = lh;
    gw_win_printf(win, 0, y, "move speed = (%3d,%3d)", ms->dx, ms->dy);
    y += lh;
    gw_win_printf(win, 0, y, "drag pos   = (%3d,%3d)", ms->drag_x, ms->drag_y);
    y += lh;
    gw_win_printf(win, 0, y, "stat = %08X", ms->stat);
    y += lh;
    gw_win_printf(win, 0, y, "cursor = %2d", (unsigned short)ms->cursor);
    y += lh;
    gw_win_printf(win, 0, y, "on      : %08X", ms->on);
    y += lh;
    gw_win_printf(win, 0, y, "press   : %08X", ms->press);
    y += lh;
    gw_win_printf(win, 0, y, "release : %08X", ms->release);
    y += lh;
    gw_win_printf(win, 0, y, "repeat  : %08X", ms->repeat);
    y += lh;
    gw_win_printf(win, 0, y, "dclick  : %08X", ms->dclick);
    y += lh;
    gw_win_printf(win, 0, y, "wheel   : %3d", ms->wheel);
    y += lh;
    if (ms->under != 0) {
        if (ms->under->title != 0) {
            y += lh;
            gw_win_printf(win, 0, y, "window = %08x(%s)", ms->under, ms->under->title);
        } else {
            y += lh;
            gw_win_printf(win, 0, y, "window = %08x", ms->under);
        }
    }
    y += lh;
    gw_win_printf(win, 0, y, "area = %d", ms->area);
}

/* Unticks the menu entry. */
/* 0x001917c0 */
static void gw_mouse_info_on_destroy(GwWin *win)
{
    gw_item_set_value(gw_window_get_slot(win, 0), 0);
}

/* 200 x 100. */
/* 0x001917e8 */
GwWin *gw_mouse_info_create(int x, int y)
{
    GwWin *win = gw_window_open("Mouse Info", 0x1005f, x, y, 200, 100, 0);

    gw_window_set_handlers(win, 0, gw_mouse_info_draw, gw_mouse_info_on_destroy);
    return win;
}
