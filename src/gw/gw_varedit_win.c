/*
 * Debug 'Variable Editor' window: browses one group at a time, steps values with arrows or
 * buttons; colours open the colour editor, function entries run when picked.
 */

#include "types_gw.h"
#include "color.h"
#include "gw_varedit_win.h"
#include "mathdef.h"
#include "libc.h"
#include "gw_button.h"
#include "gw_coloredit_win.h"
#include "gw_draw.h"
#include "gw_heap.h"
#include "gw_menu.h"
#include "gw_mouse.h"
#include "gw_rect.h"
#include "gw_scrollbar.h"
#include "gw_system.h"
#include "gw_varedit.h"
#include "gw_window.h"

static void gw_varedit_win_resolve_path(GwVarWin *vw);
static GwWin *gw_varedit_window_create(int *root, char *title);

#define VF(n, i) (((float *)(n))[i])
#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#define F(x) (*(float *)&(x))

extern char str_gw_varedit_win_empty[8];
extern int gw_varedit_window;
extern int gw_varedit_root;
extern unsigned char gw_keyboard_state[];
extern GwItem *gw_varedit_menu;

/* Tree top; window two cells from the desktop corner, 41 x 12 cells. */
/* 0x00199d90 */
static void gw_varedit_win_init_state(GwVarWin *vw)
{
    int rect[4];
    int cw;
    int ch;

    gw_get_cell_size(&cw, &ch);
    gw_get_desktop_rect(rect);
    vw->path[0] = 0;
    vw->list = 0;
    strcat(vw->path, str_gw_varedit_win_empty);
    gw_varedit_win_resolve_path(vw);
    vw->x = rect[0] + cw * 2;
    vw->y = rect[1] + ch * 2;
    vw->w = cw * 41;
    vw->h = ch * 12;
    vw->rows = 0;
    vw->lock = 0;
    vw->trans = 0;
    vw->close = 0;
}

/* Scroll and cursor back to the first entry. */
/* 0x00199e38 */
static void gw_varedit_win_resolve_path(GwVarWin *vw)
{
    GwVarNode *list;

    list = gw_varedit_first_child(vw->root, gw_varedit_find_group(vw->root, 0, vw->path));
    if (list == 0) {
        vw->list = 0;
        vw->count = 0;
    } else {
        vw->list = (int)list;
        vw->count = gw_varedit_count(list);
    }
    vw->top = 0;
    vw->cur = 0;
    vw->hi = 0;
}

/* Keeps the cursor; a vanished path leaves the list empty. */
/* 0x00199ea0 */
static void gw_varedit_win_refresh_path(GwVarWin *vw)
{
    GwVarNode *grp;
    GwVarNode *list;

    grp = 0;
    if (vw->path[0]) {
        grp = gw_varedit_find_group(vw->root, 0, vw->path);
        if (grp == 0) {
            vw->list = 0;
            vw->count = 0;
        } else {
            list = gw_varedit_first_child(vw->root, grp);
            vw->list = (int)list;
            vw->count = gw_varedit_count(list);
        }
        return;
    }
    list = gw_varedit_first_child(vw->root, grp);
    vw->list = (int)list;
    vw->count = gw_varedit_count(list);
}

/* TRANS mode: see-through black, white text; else defaults. */
/* 0x00199f10 */
static void gw_varedit_win_set_colors(GwVarWin *vw)
{
    int colors;
    int text_col;

    if (vw->trans) {
        gw_window_set_color(vw->frame, ARGB(0x60, 0, 0, 0));
        gw_window_set_color(vw->view, ARGB(0x60, 0, 0, 0));
        gw_window_set_draw_color(vw->frame, ARGB(0xff, 0xff, 0xff, 0xff));
        gw_window_set_draw_color(vw->view, ARGB(0xff, 0xff, 0xff, 0xff));
    } else {
        colors = gw_get_default_colors();
        text_col = gw_get_default_color();
        gw_window_set_colors(vw->frame, colors);
        gw_window_set_colors(vw->view, colors);
        gw_window_set_draw_color(vw->frame, text_col);
        gw_window_set_draw_color(vw->view, text_col);
    }
}

/* 0x00199fd8 */
int gw_varedit_win_is_open(void)
{
    int win;
    int open;

    win = gw_varedit_window;
    open = (win != 0);
    if (open)
        open = gw_window_is_visible(win);
    return open;
}

/* Creates it the first time; menu entry ticked while shown. Returns 0. */
/* 0x0019a000 */
int gw_varedit_win_set(int on)
{
    int win;

    win = gw_varedit_window;
    if (on) {
        if (win == 0)
            gw_varedit_window = gw_varedit_window_create(&gw_varedit_root, "Variable Editor");
        else {
            gw_item_set_value(gw_varedit_menu, 1);
            gw_window_show(win);
        }
    } else if (win) {
        gw_item_set_value(gw_varedit_menu, 0);
        gw_window_hide(win);
    }
    return 0;
}

/* '<..>' and TRANS left, up/down right, in a row under the path; the list fills the rest. */
/* 0x0019a080 */
static void gw_varedit_win_layout(GwVarWin *vw)
{
    int cw;
    int ch;
    int x;
    int y;
    int width;
    int height;
    int bw;
    int bh;
    int bottom;

    gw_get_cell_size(&cw, &ch);
    gw_window_get_pos(vw->frame, &x, &y);
    gw_window_get_size(vw->frame, &width, &height);
    vw->x = x;
    vw->y = y;
    vw->w = width;
    vw->h = height;
    x = 2;
    y = ch + 4;
    gw_window_set_pos(vw->parent_btn, x, y);
    gw_window_get_size(vw->parent_btn, &bw, &bh);
    x += 4;
    x += bw;
    gw_window_set_pos(vw->trans_btn, x, y);
    gw_window_get_size(vw->trans_btn, &bw, &bh);
    x += 4;
    x += bw;
    gw_window_get_size(vw->up_btn, &bw, &bh);
    x = width - 4;
    x -= bw;
    gw_window_set_pos(vw->up_btn, x, y);
    gw_window_get_size(vw->down_btn, &bw, &bh);
    x -= 4;
    x -= bw;
    gw_window_set_pos(vw->down_btn, x, y);
    y += 4;
    y += ch;
    gw_window_set_pos(vw->view, 2, y);
    bottom = height - 2;
    gw_window_set_size(vw->view, width - 4, bottom - y);
}

/* 0x0019a230 */
static void gw_varedit_win_draw_int(void *win, int x, int y, GwVarNode *n)
{
    int *val;

    val = (int *)n->child;
    if (val == 0)
        val = (int *)&n->mask;
    gw_win_printf(win, x, y, "%d", *val);
}

/* 0x0019a250 */
static void gw_varedit_win_notify_int(GwVarNode *n)
{
    int *val;

    if (n->cb) {
        val = (int *)n->child;
        if (val == 0)
            val = (int *)&n->mask;
        n->cb(*val);
    }
}

/* Stops at the maximum, or with wrap goes round to the minimum. */
/* 0x0019a280 */
static void gw_varedit_win_step_int_up(GwVarNode *n, int wrap)
{
    int *val;
    int step;
    int value;
    int mx;
    float fmx;
    float fv;

    val = (int *)n->child;
    if (val == 0)
        val = (int *)&n->mask;
    step = (int)((float *)n)[9];
    value = *val;
    if (wrap && value == (int)((float *)n)[8])
        value = (int)((float *)n)[7];
    else {
        fmx = ((float *)n)[8];
        value += step;
        mx = (int)fmx;
        fv = (float)value;
        if (fv < fmx)
            mx = (int)fv;
        value = mx;
    }
    *val = value;
    gw_varedit_win_notify_int(n);
}

/* Stops at the minimum, or with wrap goes round to the maximum. */
/* 0x0019a308 */
static void gw_varedit_win_step_int_down(GwVarNode *n, int wrap)
{
    int *val;
    int step;
    int value;
    int mn;
    float fmn;
    float fv;

    val = (int *)n->child;
    if (val == 0)
        val = (int *)&n->mask;
    step = (int)((float *)n)[9];
    value = *val;
    if (wrap && value == (int)((float *)n)[7])
        value = (int)((float *)n)[8];
    else {
        fmn = ((float *)n)[7];
        value -= step;
        mn = (int)fmn;
        fv = (float)value;
        if (fmn <= fv)
            mn = (int)fv;
        value = mn;
    }
    *val = value;
    gw_varedit_win_notify_int(n);
}

/* 0x0019a390 */
static void gw_varedit_win_draw_float(void *win, int x, int y, GwVarNode *n)
{
    float *val;

    val = (float *)n->child;
    if (val == 0)
        val = (float *)&n->mask;
    gw_win_printf(win, x, y, "%f", *val);
}

/* 0x0019a400 */
static void gw_varedit_win_notify_float(GwVarNode *n)
{
    float *val;

    if (n->cb) {
        val = (float *)n->child;
        if (val == 0)
            val = (float *)&n->mask;
        ((void (*)(float))n->cb)(*val);
    }
}

/* Kept inside its range. */
/* 0x0019a430 */
static void gw_varedit_win_step_float_up(GwVarNode *n)
{
    float *val;
    float value;

    val = (float *)n->child;
    if (val == 0)
        val = (float *)&n->mask;
    value = *val + VF(n, 9);
    value = MIN(MAX(value, VF(n, 7)), VF(n, 8));
    *val = value;
    gw_varedit_win_notify_float(n);
}

/* Kept inside its range. */
/* 0x0019a4a0 */
static void gw_varedit_win_step_float_down(GwVarNode *n)
{
    float *val;
    float value;

    val = (float *)n->child;
    if (val == 0)
        val = (float *)&n->mask;
    value = *val - VF(n, 9);
    value = MIN(MAX(value, VF(n, 7)), VF(n, 8));
    *val = value;
    gw_varedit_win_notify_float(n);
}

/* Four bytes in hex, top byte first. */
/* 0x0019a510 */
static void gw_varedit_win_draw_color(void *win, int x, int y, GwVarNode *n)
{
    unsigned char *val;
    unsigned value;

    val = (unsigned char *)n->child;
    if (val == 0)
        val = (unsigned char *)&n->mask;
    value = *(unsigned *)val;
    gw_win_printf(win, x, y, "%02x %02x %02x %02x\n", value >> 24, val[2], val[1], value & 0xff);
}

/* 0x0019a540 */
static void gw_varedit_win_notify_color(GwVarNode *n)
{
    int *val;

    if (n->cb) {
        val = (int *)n->child;
        if (val == 0)
            val = (int *)&n->mask;
        n->cb(*val);
    }
}

/* Colours are edited in the colour editor. */
/* 0x0019a570 */
static void gw_varedit_win_step_color_up(void)
{
}

/* 0x0019a578 */
static void gw_varedit_win_step_color_down(void)
{
}

/* In degrees. */
/* 0x0019a580 */
static void gw_varedit_win_draw_angle(void *win, int x, int y, GwVarNode *n)
{
    int *val;

    val = (int *)n->child;
    if (val == 0)
        val = (int *)&n->mask;
    gw_win_printf(win, x, y, "%f", (float)*val * DEG_PER_ANG);
}

/* 0x0019a600 */
static void gw_varedit_win_notify_angle(GwVar *it)
{
    if (it->cb)
        it->cb(*(it->ptr ? it->ptr : &it->val));
}

/* Step and range in degrees. */
/* 0x0019a630 */
static void gw_varedit_win_step_angle_up(GwVar *it)
{
    int *val = it->ptr ? it->ptr : &it->val;
    float value = (float)*val * DEG_PER_ANG + F(it->step);

    value = MIN(MAX(value, F(it->min)), F(it->max));
    *val = (int)DEG_TO_ANG(value);
    gw_varedit_win_notify_angle(it);
}

/* Step and range in degrees. */
/* 0x0019a6c8 */
static void gw_varedit_win_step_angle_down(GwVar *it)
{
    int *val = it->ptr ? it->ptr : &it->val;
    float value = (float)*val * DEG_PER_ANG - F(it->step);

    value = MIN(MAX(value, F(it->min)), F(it->max));
    *val = (int)DEG_TO_ANG(value);
    gw_varedit_win_notify_angle(it);
}

/* 0x0019a760 */
static void gw_varedit_win_draw_bool(GwWin *win, int x, int y, GwVar *it)
{
    gw_win_printf(win, x, y, "%s", *(it->ptr ? it->ptr : &it->val) ? "TRUE" : "FALSE");
}

/* 0x0019a798 */
static void gw_varedit_win_notify_bool(GwVar *it)
{
    if (it->cb)
        it->cb(*(it->ptr ? it->ptr : &it->val));
}

/* Flips it. */
/* 0x0019a7c8 */
static void gw_varedit_win_step_bool_up(GwVar *it)
{
    int *val = it->ptr ? it->ptr : &it->val;

    *val = *val ^ 1;
    gw_varedit_win_notify_bool(it);
}

/* Flips it. */
/* 0x0019a7e8 */
static void gw_varedit_win_step_bool_down(GwVar *it)
{
    int *val = it->ptr ? it->ptr : &it->val;

    *val = *val ^ 1;
    gw_varedit_win_notify_bool(it);
}

/* The number in its bits. */
/* 0x0019a808 */
static void gw_varedit_win_draw_bit(GwWin *win, int x, int y, GwVar *it)
{
    gw_win_printf(win, x, y, "%d", (int)(((unsigned)*it->ptr & it->val) >> it->min));
}

/* With the whole word holding the field. */
/* 0x0019a830 */
static void gw_varedit_win_notify_bit(GwVar *it)
{
    if (it->cb)
        it->cb(*it->ptr);
}

/* Wraps inside its bits. */
/* 0x0019a860 */
static void gw_varedit_win_step_bit_up(GwVar *it)
{
    unsigned *val = (unsigned *)it->ptr;
    unsigned mask = it->val;
    unsigned shift = it->min;
    unsigned value;

    value = *val >> shift;
    value = (value + 1) & (mask >> shift);
    value <<= shift;
    value |= *val & ~mask;
    *val = value;
    gw_varedit_win_notify_bit(it);
}

/* Wraps inside its bits. */
/* 0x0019a8a0 */
static void gw_varedit_win_step_bit_down(GwVar *it)
{
    unsigned *val = (unsigned *)it->ptr;
    unsigned mask = it->val;
    int shift = it->min;
    unsigned value;

    value = *val >> shift;
    value = (value - 1) & (mask >> shift);
    value <<= shift;
    value |= *val & ~mask;
    *val = value;
    gw_varedit_win_notify_bit(it);
}

/* "(index)name" from its name table. */
/* 0x0019a8e0 */
static void gw_varedit_win_draw_select(GwWin *win, int x, int y, GwVar *it)
{
    gw_win_printf(win, x, y, "(%d)%s", *it->ptr, ((char **)it->val)[*it->ptr]);
}

/* 0x0019a908 */
static void gw_varedit_win_notify_select(GwVar *it)
{
    if (it->cb)
        it->cb(*it->ptr);
}

/* Stops at the maximum, or with wrap goes round to the minimum. */
/* 0x0019a938 */
static void gw_varedit_win_step_select_up(GwVar *it, int wrap)
{
    int *val = it->ptr;
    int value = *val;

    if (wrap && value == it->max) {
        value = it->min;
    } else {
        value++;
        if (!(value < it->max))
            value = it->max;
    }
    *val = value;
    gw_varedit_win_notify_select(it);
}

/* Stops at the minimum, or with wrap goes round to the maximum. */
/* 0x0019a970 */
static void gw_varedit_win_step_select_down(GwVar *it, int wrap)
{
    int *val = it->ptr;
    int value = *val;

    if (wrap && value == it->min) {
        value = it->max;
    } else {
        value--;
        if ((unsigned)value < (unsigned)it->min)
            value = it->min;
    }
    *val = value;
    gw_varedit_win_notify_select(it);
}

/* Bug: the change callback runs before the copy, so it sees the old colour. */
/* 0x0019a9a8 */
static void gw_varedit_win_color_changed(GwWin *win)
{
    GwVar *it = (GwVar *)gw_window_get_slot(win, 0);
    int *val = it->ptr ? it->ptr : &it->val;
    int col = gw_coloredit_get_color(win);

    if (*val != col) {
        gw_varedit_win_notify_color(it);
        *val = col;
    }
}

/* Titled with the variable's name. */
/* 0x0019aa18 */
static void gw_varedit_win_edit_color(GwVarWin *vw)
{
    GwVar *it = vw->sel;
    GwWin *win;

    if (it) {
        win = gw_coloredit_window_create(2, *(it->ptr ? it->ptr : &it->val), 100, 100, 0, 0, 0);
        gw_coloredit_set_change_callback(win, gw_varedit_win_color_changed);
        gw_window_set_slot(win, 0, it);
        gw_window_set_title(win, it->name);
    }
}

/* 0x0019aab0 */
static GwVar *gw_varedit_win_current(GwVarWin *vw)
{
    return gw_varedit_nth(vw->list, vw->hi);
}

/* Appends its name to the path, shows its entries. */
/* 0x0019aad0 */
static void gw_varedit_win_enter_group(GwVarWin *vw, GwVar *it)
{
    char *dst;

    if (vw->path[0]) {
        dst = vw->path;
        strcat(dst, ".");
    } else {
        dst = vw->path;
    }
    strcat(dst, it->name);
    gw_varedit_win_resolve_path(vw);
}

/* Group: entered; function: run; colour: colour editor. */
/* 0x0019ab40 */
static void gw_varedit_win_activate(GwVarWin *vw)
{
    GwVar *it = gw_varedit_win_current(vw);

    if (!it)
        return;
    switch (it->kind) {
    case 0:
        gw_varedit_win_enter_group(vw, it); break;
    case 1:
        if (it->cb)
            ((void (*)(void))it->cb)();
        break;
    case 4:
        gw_varedit_win_edit_color(vw); break;
    }
}

/* Cursor on the group left. */
/* 0x0019abc8 */
static void gw_varedit_win_leave_group(GwVarWin *vw)
{
    char buf[128];
    char *s = vw->path;
    char *last = s;
    char *dot;
    int n;

    while ((dot = strchr(s, '.')) != 0)
        s = dot + 1;
    strcpy(buf, s);
    if (s != last)
        s = s - 1;
    *s = 0;
    gw_varedit_win_resolve_path(vw);
    if (buf[0]) {
        n = gw_varedit_find_index(vw->root, vw->list, buf);
        vw->top = 0;
        vw->cur = n;
        vw->hi = n;
        if (!(n < vw->rows))
        {
            vw->top = n + 1;
            vw->top -= vw->rows;
        }
        gw_scrollbar_set_val(((GwWin *)vw)->next, (float)vw->top);
    }
}

/* By kind; wrap lets ints and choices go round. */
/* 0x0019ac88 */
static void gw_varedit_win_step_up(GwVarWin *vw, int wrap)
{
    GwVar *it = gw_varedit_win_current(vw);

    if (it == 0)
        return;
    switch (it->kind) {
    case 2:
        gw_varedit_win_step_int_up(it, wrap);
        break;
    case 3:
        gw_varedit_win_step_float_up(it);
        break;
    case 4:
        ((void (*)(GwVar *))gw_varedit_win_step_color_up)(it); /* steer(cast) */
        break;
    case 5:
        gw_varedit_win_step_angle_up(it);
        break;
    case 6:
        gw_varedit_win_step_bool_up(it);
        break;
    case 7:
        gw_varedit_win_step_bit_up(it);
        break;
    case 8:
        gw_varedit_win_step_select_up(it, wrap);
        break;
    }
}

/* By kind; wrap lets ints and choices go round. */
/* 0x0019ad58 */
static void gw_varedit_win_step_down(GwVarWin *vw, int wrap)
{
    GwVar *it = gw_varedit_win_current(vw);

    if (it == 0)
        return;
    switch (it->kind) {
    case 2:
        gw_varedit_win_step_int_down(it, wrap);
        break;
    case 3:
        gw_varedit_win_step_float_down(it);
        break;
    case 4:
        ((void (*)(GwVar *))gw_varedit_win_step_color_down)(it); /* steer(cast) */
        break;
    case 5:
        gw_varedit_win_step_angle_down(it);
        break;
    case 6:
        gw_varedit_win_step_bool_down(it);
        break;
    case 7:
        gw_varedit_win_step_bit_down(it);
        break;
    case 8:
        gw_varedit_win_step_select_down(it, wrap);
        break;
    }
}

/* In the form its kind takes. */
/* 0x0019ae28 */
void gw_varedit_win_notify(GwVar *it)
{
    switch (it->kind) {
    case 2:
        gw_varedit_win_notify_int(it);
        break;
    case 3:
        gw_varedit_win_notify_float(it);
        break;
    case 4:
        gw_varedit_win_notify_color(it);
        break;
    case 5:
        gw_varedit_win_notify_angle(it);
        break;
    case 6:
        gw_varedit_win_notify_bool(it);
        break;
    case 7:
        gw_varedit_win_notify_bit(it);
        break;
    case 8:
        gw_varedit_win_notify_select(it);
        break;
    }
}

/* Cursor bar, two column lines, then per entry: kind, name (groups <>, functions ||), value. */
/* 0x0019ae98 */
static void gw_varedit_win_draw_list(GwWin *view, GwVarWin *vw)
{
    int coords[8];
    int tw;
    int y = 0;
    int rows, xr, xl, i, n, height;
    unsigned col;
    GwVar *it;
    char *fmt;
    float z;

    gw_window_get_abs_rect(view, &coords[0]);
    gw_window_get_size(view, &coords[4], &coords[5]);
    gw_get_cell_size(&coords[6], &coords[7]);
    z = gw_window_get_scale(view);
    xr = coords[4] - coords[6] * 13;
    xl = coords[6] * 6;
    rows = coords[5] / coords[7];
    tw = xr - coords[6];
    vw->rows = rows;
    i = vw->hi - vw->top;
    if (i >= 0 && i < rows) {
        col = vw->trans ? ARGB(0xff, 0, 0, 0) : ARGB(0xff, 0x80, 0x80, 0xf0);
        gw_draw_rect_gradient((float)coords[0], (float)(coords[1] + coords[7] * i), (float)coords[2],
                     (float)(coords[1] + (i + 1) * coords[7]), gw_float_bits_add(z, 1), col, 0);
    }
    height = coords[5];
    i = vw->hi - vw->top;
    gw_draw_bevel_vline(view, 1, xr - 4, 0, height, 0xffe0e0e0, 2);
    gw_draw_bevel_vline(view, 1, xl - 4, 0, height, 0xffe0e0e0, 2);
    gw_varedit_win_refresh_path(vw);
    n = vw->top;
    it = vw->list;
    if (n > 0) {
        do {
            if (it == 0)
                break;
            n--;
            it = it->next;
        } while (n != 0);
    }
    vw->sel = 0;
    if (it == 0)
        return;
    do {
        if (i-- == 0)
            vw->sel = it;
        gw_win_text(view, 0, y, gw_varedit_win_kind_names[it->kind]);
        if (it->kind != 1) {
            if (it->kind != 0) {
                if (it->kind < 9)
                    fmt = "%s";
            } else
                fmt = "<%s>";
        } else
            fmt = "|%s|";
        gw_win_printf_clip(view, xl, y, tw, y + coords[7], fmt, it->name);
        switch (it->kind) {
        case 0:
        case 1:
            break;
        case 2:
            gw_varedit_win_draw_int(view, xr, y, it);
            break;
        case 3:
            gw_varedit_win_draw_float(view, xr, y, it);
            break;
        case 4:
            gw_varedit_win_draw_color(view, xr, y, it);
            break;
        case 5:
            gw_varedit_win_draw_angle(view, xr, y, it);
            break;
        case 6:
            gw_varedit_win_draw_bool(view, xr, y, it);
            break;
        case 7:
            gw_varedit_win_draw_bit(view, xr, y, it);
            break;
        case 8:
            gw_varedit_win_draw_select(view, xr, y, it);
            break;
        }
        y += coords[7];
        coords[5] -= coords[7];
        if (coords[5] < coords[7])
            break;
        it = it->next;
    } while (it);
}

/* zone 0 kind, 1 name, 2 value. Always returns 1. */
/* 0x0019b250 */
static int gw_varedit_win_hit_test(GwWin *view, int x, int y, int *row, int *zone)
{
    int coords[4];

    gw_get_cell_size(&coords[0], &coords[1]);
    gw_window_get_size(view, &coords[2], &coords[3]);
    *row = y / coords[1];
    if (x < coords[0] * 5)
        *zone = 0;
    else if (coords[2] - coords[0] * 12 < x)
        *zone = 2;
    else
        *zone = 1;
    return 1;
}

/* Scrolls as needed; wrap goes round from the first entry. */
/* 0x0019b318 */
static void gw_varedit_win_cursor_up(GwVarWin *vw, int wrap)
{
    int n = vw->hi;

    if (n > 0) {
        n--;
        vw->hi = n;
        if (n < vw->top)
            vw->top = n;
    } else if (wrap) {
        n = vw->count - 1;
        vw->hi = n;
        if (!(n - vw->top < vw->rows))
            vw->top = vw->count - vw->rows;
    }
}

/* Scrolls as needed; wrap goes round from the last entry. */
/* 0x0019b378 */
static void gw_varedit_win_cursor_down(GwVarWin *vw, int wrap)
{
    if (vw->hi < vw->count - 1) {
        vw->hi++;
        if (!(vw->hi - vw->top < vw->rows))
            vw->top++;
    } else if (wrap) {
        vw->hi = 0;
        vw->top = 0;
    }
}

/*
 * Click sets the cursor, double click picks; Enter picks, Escape goes up, Left/Right step, Up/Down
 * move (a fresh press wraps at the ends, a held key stops). The scroll bar follows.
 */
/* 0x0019b3d0 */
static void gw_varedit_win_handle_input(GwVarWin *vw)
{
    GwWin *view = vw->view;
    int coords[6];
    int stat, old, idx, row, i, mx, my;
    unsigned char *k;

    gw_mouse_get_pos(&coords[0], &coords[1]);
    stat = gw_mouse_get_stat();
    old = (int)gw_scrollbar_get_val(vw->sb);
    vw->top = old;
    if (gw_window_is_grabbed(view)) {
        my = coords[1];
        mx = coords[0];
        if (gw_window_hit_local(view, mx, my, &coords[2], &coords[3])) {
            if (gw_varedit_win_hit_test(view, coords[2], coords[3], &coords[4], &coords[5])) {
                row = vw->top + coords[4];
                if (row < vw->count)
                    idx = row;
            } else {
                idx = -1;
            }
            if (idx >= 0) {
                int press = gw_mouse_get_press();
                int dbl = gw_mouse_get_dclick();

                if (dbl & 1) {
                    if (idx == vw->hi)
                        gw_varedit_win_activate(vw);
                }
                if (press & 1) {
                    if (idx == vw->cur)
                        vw->hi = idx;
                }
                if (stat & 0x1000000) {
                    vw->cur = vw->top + coords[4];
                    if (!(vw->cur - vw->top < vw->rows))
                        vw->cur = vw->top + vw->rows - 1;
                    if (!(vw->cur < vw->count))
                        vw->cur = vw->count - 1;
                }
            }
        }
    }
    k = gw_keyboard_state;
    if (k[13] == 40) {
        gw_varedit_win_activate(vw);
    }
    if (k[13] == 41) {
        gw_varedit_win_leave_group(vw);
        old = vw->top;
    }
    if (k[14] == 79)
        gw_varedit_win_step_up(vw, k[13] == 79);
    if (k[14] == 80)
        gw_varedit_win_step_down(vw, k[13] == 80);
    if (k[14] == 82) {
        gw_varedit_win_cursor_up(vw, k[13] == 82);
        vw->cur = vw->hi;
    }
    if (k[14] == 81) {
        gw_varedit_win_cursor_down(vw, k[13] == 81);
        vw->cur = vw->hi;
    }
    if (vw->top < old) {
        i = old - vw->top;
        do {
            gw_scrollbar_step_down(vw->sb);
        } while (--i);
    } else if (old < vw->top) {
        i = vw->top - old;
        do {
            gw_scrollbar_step_up(vw->sb);
        } while (--i);
    }
    gw_scrollbar_wheel(vw->sb);
}

/* While active: layout, input (unless locked), scroll bar sized to the list. */
/* 0x0019b678 */
static void gw_varedit_win_update(GwWin *win)
{
    GwVarWin *vw = gw_window_get_slot(win, 0);
    int cell[2];
    int track, pos;

    if (gw_window_has_active(win)) {
        gw_varedit_win_layout(vw);
        if (vw->lock == 0)
            gw_varedit_win_handle_input(vw);
        gw_get_cell_size(&cell[0], &cell[1]);
        track = gw_scrollbar_get_track(vw->sb);
        pos = (int)((float)track * ((float)vw->rows / (float)vw->count));
        gw_scrollbar_set_pos(vw->sb, pos < track ? pos : track);
        gw_scrollbar_set_max(vw->sb, (float)vw->count);
    }
}

/* The group path in brackets. */
/* 0x0019b740 */
static void gw_varedit_win_draw_title(GwWin *win)
{
    GwVarWin *vw = gw_window_get_slot(win, 0);

    gw_win_printf(win, 0, 0, "[%s]", vw->path);
}

/* Forgets the main editor, unticks its menu entry, calls the close hook, frees the state. */
/* 0x0019b780 */
static void gw_varedit_win_on_close(GwWin *win)
{
    GwVarWin *vw = gw_window_get_slot(win, 0);

    if (gw_varedit_window == win) {
        gw_varedit_window = 0;
        gw_item_set_value(gw_varedit_menu, 0);
    }
    vw->frame = 0;
    if (vw->close) {
        vw->close(win);
        vw->close = 0;
    }
    gw_dc_free(vw, "gw_varedit_win.c", 1589);
}

/* 0x0019b7f8 */
static void gw_varedit_win_view_draw(GwWin *view)
{
    GwVarWin *vw = gw_window_get_slot(view, 0);
    int cell[2];

    gw_window_get_draw_color(view);
    gw_get_cell_size(&cell[0], &cell[1]);
    gw_varedit_win_draw_list(view, vw);
}

/* On release: see-through <-> normal colours. */
/* 0x0019b850 */
static void gw_varedit_win_trans_button(GwWin *btn, int ev)
{
    GwVarWin *vw = gw_window_get_slot(btn, 0);

    if (ev == 1) {
        vw->trans ^= 1;
        gw_varedit_win_set_colors(vw);
    }
}

/* Steps up per left repeat; the first press may wrap at the maximum. */
/* 0x0019b8a0 */
static void gw_varedit_win_up_button(GwWin *btn, int ev)
{
    GwVarWin *vw = gw_window_get_slot(btn, 0);
    int press;

    if (ev == 0) {
        press = gw_mouse_get_press();
        if (gw_mouse_get_repeat() & 1)
            gw_varedit_win_step_up(vw, press & 1);
    }
}

/* Steps down per left repeat; the first press may wrap at the minimum. */
/* 0x0019b910 */
static void gw_varedit_win_down_button(GwWin *btn, int ev)
{
    GwVarWin *vw = gw_window_get_slot(btn, 0);
    int press;

    if (ev == 0) {
        press = gw_mouse_get_press();
        if (gw_mouse_get_repeat() & 1)
            gw_varedit_win_step_down(vw, press & 1);
    }
}

/* Up one group on release. */
/* 0x0019b980 */
static void gw_varedit_win_parent_button(GwWin *btn, int ev)
{
    if (ev == 1)
        gw_varedit_win_leave_group(gw_window_get_slot(btn, 0));
}

/* List with scroll bar, TRANS, up, down and '<..>' buttons. 0 on failure. */
/* 0x0019b9b8 */
static GwWin *gw_varedit_window_create(int *root, char *title)
{
    GwVarWin *vw;
    GwWin *win;
    float sb_range[5];
    int cell[2];

    vw = gw_dc_malloc(216, "gw_varedit_win.c", 1705);
    if (vw == 0)
        return 0;
    vw->root = (int)root;
    gw_varedit_win_init_state(vw);
    win = gw_window_open(title, 0x1005f, vw->x, vw->y, vw->w, vw->h, 0);
    vw->frame = win;
    if (win == 0) {
        gw_dc_free(vw, "gw_varedit_win.c", 1716);
        return 0;
    }
    gw_window_set_slot(win, 0, vw);
    gw_get_cell_size(&cell[0], &cell[1]);
    gw_window_set_handlers(win, gw_varedit_win_update, gw_varedit_win_draw_title, gw_varedit_win_on_close);
    vw->view = gw_window_open("Variable", 0x40128, 0, 0, 0, 0, vw->frame);
    gw_window_set_handlers(vw->view, 0, gw_varedit_win_view_draw, 0);
    gw_window_set_slot(vw->view, 0, vw);
    sb_range[0] = (float)vw->top;
    sb_range[1] = 0.0f;
    sb_range[2] = (float)(vw->count - 1);
    sb_range[3] = 1.0f;
    sb_range[4] = 5.0f;
    vw->sb = gw_scrollbar_create(3, sb_range, 0, 0, 0, 0, vw->view);
    gw_window_set_slot(vw->sb, 0, vw);
    vw->trans_btn = gw_push_button_create(gw_varedit_win_trans_button, "TRANS", 0, 0, cell[0] * 5 + 1, cell[1] + 1, vw->frame);
    vw->up_btn = gw_icon_button_create(gw_varedit_win_up_button, 19, 1.0f, 0, 0, 12, 12, vw->frame);
    vw->down_btn = gw_icon_button_create(gw_varedit_win_down_button, 18, 1.0f, 0, 0, 12, 12, vw->frame);
    vw->parent_btn = gw_push_button_create(gw_varedit_win_parent_button, "<..>", 0, 0, cell[0] * 4 + 4, cell[1] + 1, vw->frame);
    gw_window_set_slot(vw->trans_btn, 0, vw);
    gw_window_set_slot(vw->up_btn, 0, vw);
    gw_window_set_slot(vw->down_btn, 0, vw);
    gw_window_set_slot(vw->parent_btn, 0, vw);
    gw_varedit_win_layout(vw);
    gw_varedit_win_set_colors(vw);
    if (root == &gw_varedit_root)
        gw_item_set_value(gw_varedit_menu, 1);
    return win;
}

/* 0x0019bcb0 */
GwWin *gw_varedit_open(int root, char *title)
{
    return gw_varedit_window_create(root, title);
}

/* 0x0019bcd0 */
void gw_varedit_set_close_callback(GwWin *win, GwFn cb)
{
    GwVarWin *vw = gw_window_get_slot(win, 0);

    vw->close = cb;
}
