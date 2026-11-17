/*
 * Performance meter: task timer helpers and the "Performance" debug window (timed tasks as a
 * tree: name, display list, two times; total; levels below a chosen depth folded).
 */

#include "gw_button.h"
#include "gw_draw.h"
#include "gw_mouse.h"
#include "gw_system.h"
#include "gw_window.h"
#include "nj_system.h"
#include "perform.h"
#include "debug.h"
#include "debugmenu.h"

typedef struct PerformRec {
    int depth;
    unsigned int key;
    float x;
    float y;
} PerformRec;

#include "types_game.h"
#include "color.h"

typedef struct PerformNode {
    char pad_00[28];
    unsigned int flags;     /* +28 */
    int name;
    char pad_24[8];
    int dlist;
} PerformNode;

extern char str_perform[];
extern void soft_reset(char *name);
extern PerformRec perform_recs[];
extern int perform_value;
extern int perform_cursor;

extern unsigned int perform_key;

extern int perform_window;
extern char str_performance[];
extern int perform_window1;
extern int perform_window2;
extern int perform_window3;
extern int perform_window4;
extern PadKeyRecView kbd_state;
extern ScreenView screen_layout;

static void perform_menu_register(void);

/* 0x0014e5c0 */
void perform_hard_reset(void)
{
    hard_reset(str_perform);
}

/* 0x0014e5d0 */
void perform_soft_reset(void)
{
    soft_reset(str_perform);
    perform_menu_register();
}

/* Start value for perform_elapsed. */
/* 0x0014e5f0 */
int perform_timer_read(void)
{
    return nj_timer_get_count();
}

/* Microseconds since start. */
/* steer(cast) */
/* 0x0014e610 */
float perform_elapsed(int start)
{
    return (float)((unsigned int (*)(int))nj_timer_to_usec)(nj_timer_diff(start, nj_timer_get_count()));
}

/* Microseconds. */
/* steer(cast) */
/* 0x0014e678 */
float perform_elapsed_cur(int start, int end)
{
    return (float)((unsigned int (*)(int))nj_timer_to_usec)(nj_timer_diff(start, end)); /* steer(cast) */
}

/* List ends at key 0xffffffff. */
/* 0x0014e6c8 */
static void perform_points_sum(float *outx, float *outy)
{
    PerformRec *p = perform_recs;
    float sx = 0.0f;
    float sy = 0.0f;

    while (p->key != 0xffffffff) {
        sx += p->x;
        sy += p->y;
        p++;
    }
    *outx = sx;
    *outy = sy;
}

/* One more tree level; repeats while the mouse button is held. */
/* 0x0014e720 */
static void perform_value_inc(int item, int val)
{
    if (val == 0) {
        if (gw_mouse_get_repeat() & 1)
            perform_value++;
    }
}

/* Not below 0. */
/* 0x0014e758 */
static void perform_value_dec(int item, int val)
{
    if (val == 0) {
        if (gw_mouse_get_repeat() & 1) {
            perform_value--;
            if (perform_value < 0)
                perform_value = 0;
        }
    }
}

/* To the previous shown (unfolded) record; remembers its task so the cursor follows it. */
/* 0x0014e798 */
static void perform_cursor_up(int item, int val)
{
    if (val == 0) {
        if (gw_mouse_get_repeat() & 1) {
            int old = perform_cursor;
            PerformRec *p = &perform_recs[old];
            int depth = perform_value;
            int k = old;
            int n;

            for (;;) {
                n = k - 1;
                k = n;
                if (n == 0)
                    break;
                p--;
                if (!(depth < p->depth))
                    break;
            }
            perform_cursor = n;
            if (n < 0)
                perform_cursor = 0;
            if (old != perform_cursor) {
                p = &perform_recs[perform_cursor];
                perform_key = p->key;
            }
        }
    }
}

/* To the next shown record. */
/* 0x0014e838 */
static void perform_cursor_down(int item, int val)
{
    if (val == 0) {
        if (gw_mouse_get_repeat() & 1) {
            int old = perform_cursor;
            PerformRec *p = &perform_recs[old];
            int j;

            perform_cursor = old + 1;
            p++;
            if (p->key != 0xffffffff)
                goto loop;
            perform_cursor = old;
            goto done;
            for (;;) {
            loop:
                if (p->depth <= perform_value)
                    goto done;
                j = perform_cursor;
                p++;
                perform_cursor = j + 1;
                if (p->key == 0xffffffff)
                    break;
            }
            perform_cursor = j;
        done:
            if (old != perform_cursor) {
                p = &perform_recs[perform_cursor];
                perform_key = p->key;
            }
        }
    }
}

/* 0x0014e8f8 */
static void perform_window_close(void)
{
    perform_window = 0;
    menu_item_set_value(info_menu_name, str_performance, 0);
}

/*
 * While active: up/down cursor, left/right depth. Then lines up the four buttons from the right
 * along the top edge.
 */
/* 0x0014e918 */
static void perform_window_key(int win)
{
    int dim[8];

    if (gw_window_has_active(win)) {
        int old = perform_cursor;
        PerformRec *p = &perform_recs[old];

        if (kbd_state.k6 == 82) {
            do {
                if (--perform_cursor == 0)
                    break;
                p--;
            } while (perform_value < p->depth);
            if (perform_cursor < 0)
                perform_cursor = 0;
        }
        if (kbd_state.k6 == 81) {
            int j = perform_cursor;

            p++;
            perform_cursor = j + 1;
            do {
                while (p->key != 0xffffffff) {
                    if (p->depth <= perform_value)
                        goto qdone;
                    j = perform_cursor;
                    p++;
                    perform_cursor = j + 1;
                }
                perform_cursor = j;
            qdone:;
            } while (0);
        }
        if (kbd_state.k5 == 80) {
            perform_value = perform_value - 1;
            if (perform_value < 0)
                perform_value = 0;
        }
        if (kbd_state.k5 == 79)
            perform_value = perform_value + 1;
        if (old != perform_cursor) {
            p = &perform_recs[perform_cursor];
            perform_key = p->key;
        }
    }
    gw_get_cell_size(&dim[0], &dim[1]);
    gw_window_get_pos(win, &dim[2], &dim[3]);
    gw_window_get_size(win, &dim[4], &dim[5]);
    dim[2] = dim[0] * 13 + 2;
    dim[3] = 1;
    gw_window_get_size(perform_window3, &dim[6], &dim[7]);
    { int right = dim[4] - 4; dim[2] = right - dim[6]; }
    gw_window_set_pos(perform_window3, dim[2], dim[3]);
    gw_window_get_size(perform_window4, &dim[6], &dim[7]);
    { int right = dim[2] - 4; dim[2] = right - dim[6]; }
    gw_window_set_pos(perform_window4, dim[2], dim[3]);
    gw_window_get_size(perform_window1, &dim[6], &dim[7]);
    { int right = dim[2] - 4; dim[2] = right - dim[6]; }
    gw_window_set_pos(perform_window1, dim[2], dim[3]);
    gw_window_get_size(perform_window2, &dim[6], &dim[7]);
    { int right = dim[2] - 4; dim[2] = right - dim[6]; }
    gw_window_set_pos(perform_window2, dim[2], dim[3]);
}

/*
 * Cursor, count, depth, totals, then records from the cursor: indented by depth, display list,
 * name (or address), two times, coloured by task flags. Deeper records fold into a count/sum row.
 */
/* 0x0014ec50 */
static void perform_window_draw(int win)
{
    struct { int cell[2]; int sz[2]; float tot[2]; } layout;
    int cur;
    int idx;
    int n;
    PerformRec *p;
    PerformRec *rec;
    int rows;
    int y;
    int i;
    int cnt;
    float sy, sx;
    int *pw = &layout.sz[0];
    PerformNode *node;

    n = perf_record_cnt;
    cur = perform_cursor;
    idx = 0;
    rec = perform_recs;
    do {
        if (rec->key == perform_key) {
            cur = idx;
            break;
        }
        idx++;
        rec++;
        n--;
    } while (n != 0);
    perform_cursor = cur;
    if ((cur & 1) >> 1) layout.cell[0] = 0;
    gw_get_cell_size(&layout.cell[0], &layout.cell[1]);
    gw_window_get_size(win, pw, &layout.sz[1]);
    rows = layout.sz[1] / layout.cell[1];
    gw_win_printf(win, 4, 0, "%3d/%3d [%d]", perform_cursor, perf_record_cnt, perform_value);
    y = layout.cell[1];
    perform_points_sum(&layout.tot[0], &layout.tot[1]);
    gw_win_printf(win, 4, y, "Total %4d", perf_record_cnt);
    gw_win_printf(win, layout.sz[0] - layout.cell[0] * 12, y, "%6d%6d", (int)layout.tot[0], (int)layout.tot[1]);
    y += layout.cell[1];
    i = rows - 2;
    if (!(perform_cursor < perf_record_cnt - 1))
        perform_cursor = perf_record_cnt - 1;
    p = &perform_recs[perform_cursor];
    cnt = 0;
    sy = 0.0f;
    sx = sy;
    while (i-- > 0) {
        node = p->key;
        if (node == (PerformNode *)0xffffffff) {
            i = 0;
        } else {
            if (p->depth > perform_value) {
                sx += p->x;
                sy += p->y;
                i++;
                cnt++;
            } else {
                if (node->flags & 1)
                    gw_window_set_draw_color(win, ARGB(0x80, 0, 0, 0x80));
                else if (node->flags & 2)
                    gw_window_set_draw_color(win, ARGB(0x80, 0, 0x80, 0x80));
                else if (node->flags & 8)
                    gw_window_set_draw_color(win, ARGB(0x80, 0x78, 0x20, 0x10));
                else
                    gw_window_set_draw_color(win, ARGB(0x80, 0, 0, 0));
                gw_win_printf(win, 4, y, "%1d", node->dlist);
                {
                    int col = p->depth + 2;
                    if (node->name != 0)
                        gw_win_printf(win, layout.cell[0] * col + 4, y, "%s", node->name);
                    else
                        gw_win_printf(win, layout.cell[0] * col + 4, y, "%08x", node);
                }
                gw_win_printf(win, layout.sz[0] - layout.cell[0] * 12, y, "%6d%6d", (int)p->x, (int)p->y);
                y += layout.cell[1];
            }
            p++;
        }
        if (cnt > 0) {
            if (!(perform_value < p->depth)) {
                gw_win_printf(win, 4, y, "%6d>", cnt);
                cnt = 0;
                gw_win_printf(win, layout.sz[0] - layout.cell[0] * 12, y, "%6u%6u", (int)sx, (int)sy);
                sy = 0.0f;
                sx = sy;
                y += layout.cell[1];
            }
        }
    }
    gw_window_set_draw_color(win, ARGB(0x80, 0, 0, 0));
}

/* Window 30 wide down the left, four arrow buttons for depth and cursor. */
/* 0x0014f040 */
void perform_window_toggle(void)
{
    int sz[2];
    float one;

    if (perform_window != 0) {
        gw_window_close(perform_window);
        return;
    }
    gw_get_cell_size(&sz[0], &sz[1]);
    one = 1.0f;
    perform_window = gw_window_open(str_performance, 95, (int)(screen_layout.left + 4.0f),
                              (int)(screen_layout.top + 23.0f + 32.0f), sz[0] * 30,
                              (int)(screen_layout.scr_h - 100.0f), 0);
    gw_window_set_handlers(perform_window, perform_window_key, perform_window_draw, perform_window_close);
    menu_item_set_value(info_menu_name, str_performance, 1);
    perform_window1 = ((int (*)(void (*)(void), int, float, int, int, int, int, int))gw_icon_button_create)(perform_value_inc, 19, one, 0, 0, 12, 8, perform_window);
    perform_window2 = ((int (*)(void (*)(void), int, float, int, int, int, int, int))gw_icon_button_create)(perform_value_dec, 18, one, 0, 0, 12, 8, perform_window);
    perform_window3 = ((int (*)(void (*)(void), int, float, int, int, int, int, int))gw_icon_button_create)(perform_cursor_up, 16, one, 0, 0, 12, 8, perform_window);
    perform_window4 = ((int (*)(void (*)(void), int, float, int, int, int, int, int))gw_icon_button_create)(perform_cursor_down, 17, one, 0, 0, 12, 8, perform_window);
}

/* 0x0014f218 */
static void perform_menu_register(void)
{
    menu_item_add(info_menu_name, str_performance, perform_window_toggle, 0, 0);
}
