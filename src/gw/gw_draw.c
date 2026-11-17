/*
 * Debug GUI drawing: rectangles, gradients, bevels, window bodies, title bars, colour maths, text.
 */

#include "types_gw.h"
#include "mathdef.h"
#include <stdarg.h>
#include "libc.h"
#include "gw_draw.h"
#include "gw_button.h"
#include "gw_font.h"
#include "gw_system.h"
#include "gw_window.h"
#include "ul_gx_prim.h"
#include "color.h"
#include "gw_mouse.h"

static unsigned gw_color_add(int col, int delta);

#define GW_F_DISABLED 0x20000000

#define GW_F_PRESSED 0x40000
#define GW_F_RELEASED 0x40

/* For windows without a filled background. */
/* 0x0018f008 */
static void gw_draw_window_rect_none(void)
{
}

/* 1 pixel: left/top darker, right/bottom brighter. */
/* 0x0018f010 */
static void gw_draw_bevel_raised(float x0, float y0, float x1, float y1, float z, unsigned col)
{
    unsigned bright;
    unsigned dark;
    float xl;
    float yt;
    float yb;
    float xr;

    bright = gw_color_add(col, 0x404040);
    xl = x0 - 1.0f;
    dark = gw_color_scale(col, 0.2f);
    yb = y1 + 1.0f;
    yt = y0 - 1.0f;
    gw_draw_rect(xl, y0, x0, yb, z, dark);
    xr = x1 + 1.0f;
    gw_draw_rect(xl, yt, x1, y0, z, dark);
    gw_draw_rect(x1, yt, xr, y1, z, bright);
    gw_draw_rect(x0, y1, xr, yb, z, bright);
}

/* 1 pixel: left/top brighter, right/bottom darker. */
/* 0x0018f138 */
static void gw_draw_bevel_sunken(float x0, float y0, float x1, float y1, float z, unsigned col)
{
    unsigned bright;
    unsigned dark;
    float xl;
    float yt;
    float yb;
    float xr;

    bright = gw_color_add(col, 0x404040);
    xl = x0 - 1.0f;
    dark = gw_color_scale(col, 0.2f);
    yb = y1 + 1.0f;
    yt = y0 - 1.0f;
    gw_draw_rect(xl, y0, x0, yb, z, bright);
    xr = x1 + 1.0f;
    gw_draw_rect(xl, yt, x1, y0, z, bright);
    gw_draw_rect(x1, yt, xr, y1, z, dark);
    gw_draw_rect(x0, y1, xr, yb, z, dark);
}

/* Horizontal, col to a brighter col. mode unused. */
/* 0x0018f260 */
void gw_draw_rect_gradient(float x0, float y0, float x1, float y1, float z, unsigned col, int mode)
{
    GwVtx vtx[4];
    unsigned bright = gw_color_add(col, 0x404040);

    vtx[0].x = x0;
    vtx[0].y = y0;
    vtx[1].x = x0;
    vtx[0].z = z;
    vtx[1].y = y1;
    vtx[1].z = z;
    vtx[2].x = x1;
    vtx[2].y = y0;
    vtx[2].z = z;
    vtx[3].x = x1;
    vtx[3].y = y1;
    vtx[3].z = z;
    vtx[1].col = col;
    vtx[0].col = col;
    vtx[3].col = bright;
    vtx[2].col = bright;
    ulGxBeginStrip(0);
    ulGxAddStrip(vtx, 4);
    ulGxEndStrip();
}

/* 0x0018f328 */
void gw_draw_rect(float x0, float y0, float x1, float y1, float z, unsigned col)
{
    GwVtx vtx[4];

    vtx[0].x = x0;
    vtx[0].y = y0;
    vtx[0].z = z;
    vtx[1].y = y1;
    vtx[1].x = x0;
    vtx[2].y = y0;
    vtx[1].z = z;
    vtx[2].x = x1;
    vtx[3].x = x1;
    vtx[2].z = z;
    vtx[3].col = col;
    vtx[2].col = col;
    vtx[1].col = col;
    vtx[3].y = y1;
    vtx[3].z = z;
    vtx[0].col = col;
    ulGxBeginStrip(0);
    ulGxAddStrip(vtx, 4);
    ulGxEndStrip();
}

/* Corner colours if the window has them, else its first colour. */
/* 0x0018f3a0 */
static void gw_draw_window_rect(GwWin *win, float x0, float y0, float x1, float y1, float z)
{
    GwVtx vtx[4];

    vtx[0].x = x0;
    vtx[0].y = y0;
    vtx[0].z = z;
    vtx[1].y = y1;
    vtx[1].x = x0;
    vtx[2].y = y0;
    vtx[1].z = z;
    vtx[2].x = x1;
    vtx[3].x = x1;
    vtx[2].z = z;
    vtx[3].y = y1;
    vtx[3].z = z;
    if (win->flags & 8) {
        vtx[0].col = win->col[0];
        vtx[1].col = win->col[1];
        vtx[2].col = win->col[2];
        vtx[3].col = win->col[3];
    } else {
        vtx[3].col = win->col[0];
        vtx[2].col = win->col[0];
        vtx[1].col = win->col[0];
        vtx[0].col = win->col[0];
    }
    ulGxBeginStrip(0);
    ulGxAddStrip(vtx, 4);
    ulGxEndStrip();
}

/* Per channel, saturating at 255. */
/* 0x0018f448 */
static unsigned gw_color_add(int col, int delta)
{
    int al = (int)((unsigned)col >> 24) + (int)((unsigned)delta >> 24);
    int r = (col & 0xff) + (delta & 0xff);
    int bl = ((col >> 16) & 0xff) + ((delta >> 16) & 0xff);
    int g = ((col >> 8) & 0xff) + ((delta >> 8) & 0xff);

    if (al > 255)
        al = 255;
    if (bl > 255)
        bl = 255;
    if (g > 255)
        g = 255;
    if (r > 255)
        r = 255;
    return (al << 24) | (bl << 16) | (g << 8) | r;
}

/* RGB clamped to 255; alpha kept. */
/* 0x0018f4c8 */
unsigned gw_color_scale(int col, float scale)
{
    float r = (float)((col >> 16) & 0xff) / 255.0f;
    float g = (float)((col >> 8) & 0xff) / 255.0f;
    float b = (float)(col & 0xff) / 255.0f;

    r = r * scale;
    g = g * scale;
    b = b * scale;
    if (r > 1.0f)
        r = 1.0f;
    if (g > 1.0f)
        g = 1.0f;
    if (b > 1.0f)
        b = 1.0f;
    return ((col & 0xff000000) | (int)(b * 255.0f)) | (((int)(g * 255.0f) << 8) | ((int)(r * 255.0f) << 16));
}

/* Per channel, clamped at 0. */
/* 0x0018f5b0 */
static unsigned gw_color_sub(int col, int delta)
{
    int col_a = (int)((unsigned)col >> 24), delta_a = (int)((unsigned)delta >> 24);
    int col_b = (col >> 16) & 0xff, col_g = (col >> 8) & 0xff, col_r = col & 0xff;
    int delta_b = (delta >> 16) & 0xff, delta_g = (delta >> 8) & 0xff, delta_r = delta & 0xff;
    int al = col_a - delta_a, bl = col_b - delta_b, g = col_g - delta_g, r = col_r - delta_r;

    if (al < 0)
        al = 0;
    if (bl < 0)
        bl = 0;
    if (g < 0)
        g = 0;
    if (r < 0)
        r = 0;
    return (al << 24) | (bl << 16) | (g << 8) | r;
}

/*
 * Gradient strip above the window (brighter when it or a child is active), title between the
 * buttons.
 */
/* 0x0018f630 */
void gw_draw_title_bar(GwWin *win, float z)
{
    unsigned text_col;
    unsigned bar_col;
    unsigned old;
    float val;
    int pos[6];
    int gap;
    float top;
    float right;
    float bottom;

    if (gw_window_has_active(win) != 0) {
        text_col = ARGB(0x80, 0x80, 0x80, 0x80);
        bar_col = ARGB(0x80, 0x40, 0, 0);
    } else {
        text_col = ARGB(0x80, 0x40, 0x40, 0x40);
        bar_col = ARGB(0x80, 0x40, 0x40, 0x40);
    }
    gw_window_get_abs_pos(win, &pos[0], &pos[1]);
    val = (float)pos[0];
    top = (float)(pos[1] - gw_get_title_bar_height());
    right = (float)(pos[0] + win->w);
    bottom = (float)pos[1];
    gw_draw_rect_gradient(val, top, right, bottom, z, bar_col, 0);
    if (win->title != 0) {
        gw_get_cell_size(&pos[2], &pos[3]);
        if (win->flags & 0x10)
            gw_titlebar_button_pos(win->btn[2], win, &pos[4], &pos[5]);
        else
            pos[4] = win->w;
        old = gw_font_set_color(text_col);
        val = gw_font_set_depth(gw_float_bits_add(z, 1));
        gap = gw_get_title_bar_height() - pos[3];
        gw_win_text_clip(win, 2, -(gap / 2) - pos[3], pos[4], win->h, win->title);
        gw_font_set_depth(val);
        gw_font_set_color(old);
    }
}

/* Background unless none; released/pressed windows get a bevel including the title bar. */
/* 0x0018f7b8 */
void gw_draw_window_body(GwWin *win, float z)
{
    int pos[2];
    float x0;
    float y0;
    float x1;
    float y1;
    int cell;
    unsigned col;

    gw_window_get_abs_pos(win, &pos[0], &pos[1]);
    x0 = (float)pos[0];
    y0 = (float)pos[1];
    x1 = (float)(pos[0] + win->w);
    y1 = (float)(pos[1] + win->h);
    if (win->flags & 0x20000)
        ((void (*)(GwWin *, float, float, float, float, float))gw_draw_window_rect_none)(win, x0, y0, x1, y1, z); /* steer(cast) */
    else
        gw_draw_window_rect(win, x0, y0, x1, y1, z);
    cell = (win->flags & 1) ? gw_get_title_bar_height() : 0;
    col = win->col[0];
    if (win->flags & 0x40)
        gw_draw_bevel_sunken(x0, y0 - (float)cell, x1, y1, z, col);
    else if (win->flags & 0x40000)
        gw_draw_bevel_raised(x0, y0 - (float)cell, x1, y1, z, col);
}

/* 0x0018f918 */
unsigned gw_window_get_draw_color(GwWin *win)
{
    return win->color;
}

/* Also the text colour; returns the previous text colour. */
/* 0x0018f920 */
unsigned gw_window_set_draw_color(GwWin *win, unsigned col)
{
    win->color = col;
    return gw_font_set_color(col);
}

/* Draws an icon: text carries the icon number, z the scale. */
/* 0x0018f940 */
void gw_draw_text_at(GwWin *win, int x, int y, char *text, float z)
{
    int pos[2];
    float fx;
    float fy;

    gw_window_get_abs_pos(win, &pos[0], &pos[1]);
    fx = (float)(pos[0] + x);
    fy = (float)(pos[1] + y);
    gw_font_icon_scaled(fx, fy, z, text);
}

/* Clipped right and bottom to the smaller of cw x ch and the window. */
/* 0x0018f9c0 */
void gw_win_text_clip(GwWin *win, int x, int y, int cw, int ch, char *s)
{
    int ox, oy;

    cw = MIN(win->w, cw);
    ch = MIN(win->h, ch);
    gw_window_get_abs_pos(win, &ox, &oy);
    gw_font_text_clip(s, (float)(ox + x), (float)(oy + y), (float)(ox + cw), (float)(oy + ch));
}

/* Up to 1024 bytes. */
/* 0x0018fa78 */
void gw_win_printf_clip(GwWin *win, int x, int y, int cw, int ch, char *fmt, ...)
{
    char buf[1024];
    int ox, oy;
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    if (win->w < cw)
        cw = win->w;
    if (win->h < ch)
        ch = win->h;
    gw_window_get_abs_pos(win, &ox, &oy);
    gw_font_text_clip(buf, (float)(ox + x), (float)(oy + y), (float)(ox + cw), (float)(oy + ch));
}

/* 0x0018fb48 */
static void gw_win_putc(GwWin *win, int x, int y, char code)
{
    int ox, oy;

    gw_window_get_abs_pos(win, &ox, &oy);
    gw_font_putc((float)(ox + x), (float)(oy + y), code);
}

/* Clipped to the window. */
/* 0x0018fbb8 */
void gw_win_text(GwWin *win, int x, int y, char *s)
{
    int ox, oy;

    gw_window_get_abs_pos(win, &ox, &oy);
    gw_font_text_clip(s, (float)(ox + x), (float)(oy + y), (float)(ox + win->w), (float)(oy + win->h));
}

/* Up to 1024 bytes. */
/* 0x0018fc50 */
void gw_win_printf(GwWin *win, int x, int y, char *fmt, ...)
{
    char buf[1024];
    int ox, oy;
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    gw_window_get_abs_pos(win, &ox, &oy);
    gw_font_text_clip(buf, (float)(ox + x), (float)(oy + y), (float)(ox + win->w), (float)(oy + win->h));
}
