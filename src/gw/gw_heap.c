/* Debug GUI heap: blocks tagged with file and line, and the 'Gindows Heap Infomation' window. */

#include "types_gw.h"
#include "color.h"
#include "libc.h"
#include "gw_heap.h"
#include "gw_button.h"
#include "gw_console.h"
#include "gw_draw.h"
#include "gw_menu.h"
#include "gw_scrollbar.h"
#include "gw_system.h"
#include "gw_window.h"

static int gw_heap_get_stats(unsigned *stats);
static void *gw_heap_alloc(unsigned size);
static void *gw_heap_calloc(int n, int size);
static void gw_heap_free(void *ptr);

extern GwMem gw_heap_list;
extern int gw_heap_size;
extern void *gw_heap_cur;
extern void *gw_heap_base;

/* On release: totals <-> block list (with scroll bar). */
/* 0x0018e5d0 */
static void gw_heap_window_mode_button(GwWin *win, int mode)
{
    GwWin *parent = gw_window_get_parent(win);
    unsigned *state = gw_window_get_user(parent);

    if (mode == 1) {
        *state = *state + 1;
        if (*state >= 2)
            *state = 0;
        switch (*state) {
        case 0:
            gw_scrollbar_set_enabled(gw_window_get_vscrollbar(parent), 0);
            break;
        case 1:
            gw_scrollbar_set_enabled(gw_window_get_vscrollbar(parent), 1);
            break;
        }
    }
}

/*
 * Totals, or one row per block from the scroll position (address, size, file, line), free in red.
 */
/* 0x0018e690 */
static void gw_heap_window_draw(GwWin *win)
{
    int *state = gw_window_get_user(win);
    unsigned col = gw_window_get_draw_color(win);
    unsigned old;
    unsigned st[5];
    int cw;
    int ch;
    int top;
    int y;
    int pos;
    GwMem *blk;

    old = gw_window_set_draw_color(win, col);
    gw_get_cell_size(&cw, &ch);
    top = ch * 2;
    switch (*state) {
    case 0:
        gw_heap_get_stats(st);
        y = top;
        gw_win_printf(win, 0, y, "use size      =%8XH", st[0]);
        y += ch;
        gw_win_printf(win, 0, y, "free size     =%8XH", st[1]);
        y += ch;
        gw_win_printf(win, 0, y, "max free size =%8XH", st[2]);
        y += ch;
        gw_win_printf(win, 0, y, "use block     = %8d", st[3]);
        gw_win_printf(win, 0, y + ch, "free block    = %8d", st[4]);
        break;
    case 1:
        pos = (int)gw_scrollbar_get_val(gw_window_get_vscrollbar(win));
        y = top - pos / ch * ch;
        for (blk = gw_heap_list.next; blk != &gw_heap_list; blk = blk->next) {
            if (y < top) {
                y += ch;
            } else {
                if (blk->used == 1)
                    gw_window_set_draw_color(win, col);
                else
                    gw_window_set_draw_color(win, ARGB(0xff, 0, 0, 0xc0));
                if (blk->file != 0)
                    gw_win_printf(win, 0, y, "%08x %08x %s(%d)", blk, blk->size, blk->file, blk->line);
                else
                    gw_win_printf(win, 0, y, "%08x %08x", blk, blk->size);
                y += ch;
            }
            if (win->h < y + ch)
                break;
        }
        break;
    }
    gw_window_set_draw_color(win, old);
}

/* List mode: scroll bar sized from the block count, keys and wheel scroll while active. */
/* 0x0018e908 */
static void gw_heap_window_update(GwWin *win)
{
    int *state = gw_window_get_user(win);
    GwWin *sb = gw_window_get_vscrollbar(win);
    unsigned st[5];
    int sz[2];
    int n;
    int total;
    int thumb;

    gw_get_cell_size(&sz[0], &sz[1]);
    switch (*state) {
    case 0:
        gw_scrollbar_set_enabled(win->vscroll, 0);
        break;
    case 1:
        gw_scrollbar_set_enabled(win->vscroll, 1);
        gw_heap_get_stats(st);
        n = st[3] + st[4];
        total = gw_scrollbar_get_track(sb);
        thumb = (int)((float)win->h / (float)(n * sz[1]) * (float)total);
        if (thumb > total)
            thumb = total;
        if (thumb <= 3)
            thumb = 4;
        gw_scrollbar_set_pos(sb, thumb);
        gw_scrollbar_set_max(sb, (float)((n - 1) * sz[1]));
        if (gw_window_has_active(win) != 0) {
            gw_scrollbar_keys(gw_window_get_vscrollbar(win));
            gw_scrollbar_wheel(gw_window_get_vscrollbar(win));
        }
        break;
    }
}

/* Unticks the menu entry, frees the state. */
/* 0x0018ea58 */
static void gw_heap_window_close(GwWin *win)
{
    int obj = gw_window_get_slot(win, 0);
    int heap;

    if (obj != 0)
        gw_item_set_value(obj, 0);
    heap = gw_window_get_user(win);
    if (heap != 0) {
        gw_heap_free(heap);
        gw_window_set_user(win, 0);
    }
}

/* 200 x 100, totals mode, hidden scroll bar, mode button. */
/* 0x0018eac0 */
GwWin *gw_heap_window_create(int x, int y)
{
    GwWin *win;
    float cfg[5];
    unsigned st[5];
    int sz[2];
    unsigned n;

    gw_get_cell_size(&sz[0], &sz[1]);
    win = gw_window_open("Gindows Heap Infomation", 0x1005f, x, y, 200, 100, 0);
    gw_window_set_handlers(win, gw_heap_window_update, gw_heap_window_draw, gw_heap_window_close);
    gw_window_set_user(win, gw_heap_calloc(1, 12));
    gw_heap_get_stats(st);
    n = (st[3] + st[4]) * sz[1];
    cfg[0] = 0;
    cfg[1] = 0;
    cfg[2] = (float)n;
    cfg[3] = (float)sz[1];
    cfg[4] = (float)sz[1] * 5.0f;
    gw_window_set_vscrollbar(win, gw_scrollbar_create(1, cfg, 0, 0, 0, 0, win));
    gw_scrollbar_set_enabled(gw_window_get_vscrollbar(win), 0);
    gw_icon_button_create(gw_heap_window_mode_button, 11, 1.0f, 2, 2, 0, 0, win);
    return win;
}

/* 0x0018ec40 */
void *gw_dc_malloc(unsigned size, char *file, int line)
{
    void *ptr = gw_heap_alloc(size);
    GwMem *blk;

    if (ptr == 0)
        return 0;
    blk = (GwMem *)ptr - 1;
    blk->file = file;
    blk->line = line;
    return ptr;
}

/* 0x0018ec88 */
void *gw_dc_calloc(int n, int size, char *file, int line)
{
    void *ptr = gw_heap_calloc(n, size);
    GwMem *blk;

    if (ptr == 0)
        return 0;
    blk = (GwMem *)ptr - 1;
    blk->file = file;
    blk->line = line;
    return ptr;
}

/* A null pointer is reported with file and line. */
/* 0x0018ecd0 */
void gw_dc_free(void *ptr, char *file, int line)
{
    GwMem *blk = (GwMem *)ptr - 1;

    if (ptr == 0) {
        gw_console_printf("WARNING: dcFree get null pointer in %s(%d)\n", file, line);
        return;
    }
    if (blk->file != 0)
        blk->file = 0;
    gw_heap_free(ptr);
}

/*
 * Used, free, largest free, used count, free count; 1. A corrupt header sets all to -1, returns 0.
 */
/* 0x0018ecf8 */
static int gw_heap_get_stats(unsigned *stats)
{
    GwMem *blk;

    memset(stats, 0, 20);
    blk = gw_heap_list.next;
    while (blk != &gw_heap_list) {
        if (blk->magic == 0xa5a5) {
            if (*(unsigned *)blk == 0x1a5a5) {
                stats[3] = stats[3] + 1;
                stats[0] = stats[0] + blk->size;
            } else {
                stats[4] = stats[4] + 1;
                stats[1] = stats[1] + blk->size;
                if (stats[2] < blk->size)
                    stats[2] = blk->size;
            }
        } else {
            stats[0] = -1;
            stats[1] = -1;
            stats[2] = -1;
            stats[3] = -1;
            stats[4] = -1;
            return 0;
        }
        blk = blk->next;
    }
    return 1;
}

/* List head plus one free block. */
/* 0x0018ede8 */
void gw_heap_init(GwMem *base, int size)
{
    gw_heap_cur = (void *)base;
    gw_heap_base = (void *)base;
    gw_heap_size = size;
    gw_heap_list.magic = 0xa5a5;
    gw_heap_list.used = 1;
    gw_heap_list.size = 32;
    gw_heap_list.next = gw_heap_list.prev = base;
    gw_heap_list.line = 0;
    base->used = 0;
    base->size = size;
    gw_heap_list.file = 0;
    base->next = base->prev = &gw_heap_list;
    base->file = 0;
    base->line = 0;
    base->magic = 0xa5a5;
}

/*
 * size + 32-byte header rounded to 32; first fit from the rover; splits when over 32 bytes remain.
 */
/* 0x0018ee48 */
static void *gw_heap_alloc(unsigned size)
{
    GwMem *blk;
    GwMem *start;
    GwMem *n;
    unsigned rest;

    size = (size + 63) & 0xffffffe0;
    blk = gw_heap_cur;
    start = blk;
    do {
        if (*(unsigned *)blk == 0xa5a5 && blk->size >= size) {
            rest = blk->size - size;
            if (rest > 32) {
                n = (GwMem *)((char *)blk + size);
                n->size = rest;
                blk->next->prev = n;
                n->magic = 0xa5a5;
                n->used = 0;
                n->next = blk->next;
                blk->next = n;
                n->prev = blk;
                blk->size = size;
                blk->used = 1;
            } else
                blk->used = 1;
            blk->file = 0;
            blk->line = 0xffffffff;
            start = blk->next;
            gw_heap_cur = start;
            return blk + 1;
        }
        blk = blk->next;
    } while (blk != start);
    return 0;
}

/* 0x0018eef8 */
static void *gw_heap_calloc(int n, int size)
{
    int total = n * size;
    void *ptr = gw_heap_alloc(total);

    if (ptr != 0)
        memset(ptr, 0, total);
    return ptr;
}

/* Merges free neighbours, keeps the rover valid. Ignores memory without a valid header. */
/* 0x0018ef48 */
static void gw_heap_free(void *ptr)
{
    GwMem *blk;
    unsigned size;

    if (ptr == 0)
        return;
    if (*(unsigned short *)((char *)ptr - 32) != 0xa5a5)
        return;
    blk = (GwMem *)((char *)ptr - 32);
    if (blk->used == 0)
        return;
    if (blk->prev->used == 0) {
        size = blk->size;
        blk->next->prev = blk->prev;
        blk->prev->next = blk->next;
        blk->prev->size += size;
        if (gw_heap_cur == blk)
            gw_heap_cur = (void *)blk->prev;
        blk->used = 0;
        *(unsigned short *)((char *)ptr - 32) = 0;
        blk = blk->prev;
    }
    if (blk->next->used == 0) {
        blk->next->next->prev = blk;
        blk->size += blk->next->size;
        if (gw_heap_cur == blk->next)
            gw_heap_cur = blk;
        blk->next = blk->next->next;
    }
    blk->used = 0;
}
