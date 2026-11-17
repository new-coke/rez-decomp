/* Debug GUI colour editor: text field and 0..255 slider per ARGB channel, preview, OK button. */

#include "types_gw.h"
#include "libc.h"
#include "gw_coloredit_win.h"
#include "gw_button.h"
#include "gw_draw.h"
#include "gw_dump.h"
#include "gw_heap.h"
#include "gw_slider.h"
#include "gw_system.h"
#include "gw_textfield.h"
#include "gw_window.h"
#include "color.h"

extern int fptoui(float f);
extern char *gw_coloredit_chan_names[4];
extern char gw_coloredit_text_buf[];

/* 'coloredit [hex colour]', opaque black by default. 0 ok, -1 failed. */
/* 0x0019d0d8 */
static int gw_coloredit_command(int argc, char **argv)
{
    unsigned color = ARGB(0xff, 0, 0, 0);
    GwWin *win;
    int ret = -1;

    if (argc == 2)
        color = gw_parse_hex(argv[1]);
    win = gw_coloredit_window_create(0, color, -1, -1, 0, 0, 0);
    if (win)
        ret = 0;
    return ret;
}

/* 0x0019d130 */
void gw_coloredit_set_change_callback(GwWin *win, GwColorFn cb)
{
    GwColorEdit *cedit = gw_window_get_user(win);

    cedit->on_change = cb;
}

/* 0x0019d158 */
unsigned gw_coloredit_get_color(GwWin *win)
{
    GwColorEdit *cedit = gw_window_get_user(win);

    return cedit->color;
}

/* 0x0019d178 */
static void gw_coloredit_set_close_callback(GwWin *win, GwColorFn cb)
{
    GwColorEdit *cedit = gw_window_get_user(win);

    cedit->on_close = cb;
}

/* 0x0019d1a0 */
static void gw_coloredit_close(void * win)
{
    gw_window_close(win);
}

/* Closes on release. */
/* 0x0019d1a8 */
static void gw_coloredit_ok_button(GwWin *btn, int ev)
{
    if (ev == 1)
        gw_window_close(btn->parent);
}

/* A in the top byte, then R, G, B. */
/* 0x0019d1c8 */
static unsigned gw_coloredit_pack_color(GwWin *win)
{
    GwColorEdit *cedit = gw_window_get_user(win);
    unsigned a, r, g, b;

    a = fptoui(cedit->rec[0].val) & 0xff;
    r = fptoui(cedit->rec[1].val) & 0xff;
    g = fptoui(cedit->rec[2].val) & 0xff;
    b = fptoui(cedit->rec[3].val) & 0xff;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

/* Value into the channel and the text field; repacks, calls on_change. */
/* 0x0019d250 */
static void gw_coloredit_slider_changed(GwWin *slider)
{
    GwColorEdit *cedit;
    GwColorChan *rec;
    char buf[128];

    cedit = gw_window_get_user(slider->parent);
    rec = &cedit->rec[gw_window_get_slot(slider, 0)];
    rec->val = gw_slider_get_value((GwSlider *)slider);
    sprintf(buf, "%d", (int)rec->val);
    gwTextFieldSetText(rec->edit, buf);
    cedit->color = gw_coloredit_pack_color(slider->parent);
    if (cedit->on_change)
        cedit->on_change(slider->parent);
}

/* When editing is done: parses, clamps to the slider range, updates, calls on_change. */
/* 0x0019d2f8 */
static void gw_coloredit_text_done(GwWin *tf)
{
    GwColorEdit *cedit;
    GwColorChan *rec;
    char buf[128];
    float lo, hi;

    if (gwTextFieldIsDone(tf)) {
        cedit = gw_window_get_user(tf->parent);
        rec = &cedit->rec[gw_window_get_slot(tf, 0)];
        rec->val = (float)atof(gwTextFieldGetText(rec->edit));
        lo = gw_slider_get_min(rec->slider);
        hi = gw_slider_get_max(rec->slider);
        if (rec->val < lo)
            rec->val = lo;
        if (hi < rec->val)
            rec->val = hi;
        gw_slider_set_value(rec->slider, rec->val);
        sprintf(buf, "%d", (int)rec->val);
        gwTextFieldSetText(tf, buf);
        cedit->color = gw_coloredit_pack_color(tf->parent);
        if (cedit->on_change)
            cedit->on_change(tf->parent);
    }
}

/* Hex value by the last slider; preview with alpha halved and R/B swapped to GS order; labels. */
/* 0x0019d410 */
static void gw_coloredit_window_draw(GwWin *win)
{
    GwColorEdit *cedit;
    GwWin **pe;
    GwWin *sl;
    int coords[6];
    unsigned col;
    char **label;
    int i;

    cedit = gw_window_get_user(win);
    i = 3;
    sl = (GwWin *)cedit->rec[3].slider;
    gw_get_cell_size(&coords[0], &coords[1]);
    pe = &cedit->rec[0].edit;
    gw_window_get_pos(sl, &coords[2], &coords[3]);
    gw_window_get_size(sl, &coords[4], &coords[5]);
    coords[2] = coords[2] + (coords[4] + coords[0] * 2);
    coords[3] = coords[3] + (coords[5] - coords[1]) / 2;
    gw_win_printf(win, coords[2], coords[3], "%08X", cedit->color);
    col = cedit->color;
    gw_window_set_color(cedit->preview,
                 ((((col >> 24) + 1) >> 1) << 24) | ((col & 0xff) << 16) | (col & 0xff00) | ((col >> 16) & 0xff));
    label = gw_coloredit_chan_names;
    do {
        gw_window_get_pos(*pe, &coords[2], &coords[3]);
        i--;
        gw_window_get_size(*pe, &coords[4], &coords[5]);
        pe = (GwWin **)((char *)pe + 12);
        coords[2] = coords[2] - (coords[0] * 2 - coords[0] / 2);
        coords[3] = coords[3] + (coords[5] - coords[1]) / 2;
        gw_win_text(win, coords[2], coords[3], *label);
        label++;
    } while (i >= 0);
}

/* 0x0019d5f0 */
static void gw_coloredit_window_update(void)
{
}

/* Calls the close hook, frees the editor state. */
/* 0x0019d5f8 */
static void gw_coloredit_window_destroyed(GwWin *win)
{
    GwColorEdit *cedit = gw_window_get_user(win);

    if (cedit->on_close)
        cedit->on_close(win);
    gw_dc_free(cedit, "gw_coloredit_win.c", 330);
    gw_window_set_user(win, 0);
}

/*
 * 'Color Edit Window': four channel rows, 'Color Panel' preview, 'OK'; sized to fit. 0 on failure.
 */
/* 0x0019d658 */
GwWin *gw_coloredit_window_create(int mode, unsigned color, int x, int y, int width, int height, GwWin *parent)
{
    GwWin *win;
    GwColorEdit *cedit;
    GwColorChan *rec;
    GwSliderVals vals;
    GwWin *ctl;
    int cw, ch, sx, sy, px, py;
    int i;
    unsigned chan;

    win = gw_window_open("Color Edit Window", 0x1005d, x, y, width, height, parent);
    if (!win)
        return 0;
    i = 0;
    gw_button_set_enabled(gw_window_get_maximize_box(win), 0);
    gw_button_set_enabled(gw_window_get_minimize_box(win), 0);
    gw_window_set_owner(win, 0);
    gw_get_cell_size(&cw, &ch);
    gw_window_set_handlers(win, gw_coloredit_window_update, gw_coloredit_window_draw, gw_coloredit_window_destroyed);
    cedit = gw_dc_calloc(1, 80, "gw_coloredit_win.c", 386);
    gw_window_set_user(win, cedit);
    cedit->color = color;
    vals.max = 255.0f;
    vals.fine_step = 1.0f;
    vals.step = 10.0f;
    px = cw * 2;
    py = ch * 2;
    vals.val = 0;
    vals.min = 0;
    do {
        chan = (color >> ((3 - i) * 8)) & 0xff;
        rec = &cedit->rec[i];
        rec->val = (float)chan;
        sprintf(gw_coloredit_text_buf, "%d", (int)rec->val);
        ctl = gwTextFieldCreate(gw_coloredit_text_buf, 0, px, py, cw * 8, ch, win);
        rec->edit = ctl;
        gw_window_get_size(ctl, &sx, &sy);
        gwTextFieldSetEnterCallback(ctl, gw_coloredit_text_done);
        gw_window_set_slot(ctl, 0, i);
        ctl = (GwWin *)gw_slider_create(0, &vals, px + sx + 2, py, cw * 10, sy, win);
        rec->slider = (GwSlider *)ctl;
        gw_window_get_size(ctl, &sx, &sy);
        gw_slider_set_value((GwSlider *)ctl, rec->val);
        gw_slider_set_callback((GwSlider *)ctl, gw_coloredit_slider_changed);
        gw_window_set_slot(ctl, 0, i);
        i++;
        py += 3;
        py += sy;
    } while (i < 4);
    gw_window_get_pos(cedit->rec[0].slider, &px, &py);
    gw_window_get_size(cedit->rec[0].slider, &sx, &sy);
    px += sx + cw * 2;
    gw_window_get_pos(cedit->rec[2].slider, &x, &y);
    gw_window_get_size(cedit->rec[2].slider, &width, &height);
    sy = y + height - py;
    cedit->preview = gw_window_open("Color Panel", 0x40124, px, py, cw * 8, sy, win);
    gw_window_set_color(cedit->preview, color);
    gw_window_get_pos(cedit->rec[3].edit, &px, &py);
    gw_window_get_size(cedit->rec[3].edit, &sx, &sy);
    height = py + sy + ch * 2;
    gw_window_get_pos(cedit->preview, &px, &py);
    gw_window_get_size(cedit->preview, &sx, &sy);
    width = px + sx + cw * 2;
    gw_window_set_size(win, width, height);
    gw_window_get_pos(cedit->rec[3].edit, &px, &py);
    gw_window_get_size(cedit->rec[3].edit, &sx, &sy);
    cedit->ok = gw_push_button_create(gw_coloredit_ok_button, "OK", (width - cw * 4) / 2, py + sy + ch / 2, cw * 4, ch + 2, win);
    return win;
}
