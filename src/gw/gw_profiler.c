/*
 * Debug GUI profiler: groups (VBLANK, MAPLE, RENDER, DMA) of coloured time bars per sampling
 * period; 'Profiler' window with scale and period buttons.
 */

#include "types_gw.h"
#include "gw_profiler.h"
#include "libc.h"
#include "gw_button.h"
#include "gw_console.h"
#include "gw_desktop.h"
#include "gw_draw.h"
#include "gw_heap.h"
#include "gw_menu.h"
#include "gw_mouse.h"
#include "gw_system.h"
#include "gw_window.h"
#include "nj_draw.h"
#include "nj_system.h"
#include "nj_view.h"
#include "color.h"

static void gw_profiler_window_detach(void);
static GwProf *gw_profiler_create(void);
static GwWin *gw_profiler_window_open(int x, int y, int width, int height);
static GwProfGroup *gw_profiler_add_group(GwProf *prof, char *name);
static void gw_profiler_remove_group(GwProf *prof, char *name);
static GwProfGroup *gw_profiler_find_group(GwProf *prof, char *name);
static void gw_profiler_mark_ex(GwProf *prof, GwProfGroup *grp, unsigned int color);
static GwProfEntry *gw_profiler_begin_ex(GwProf *prof, GwProfGroup *grp, int color);
static GwProfEntry *gw_profiler_end_last_ex(GwProf *prof, GwProfGroup *grp);
static GwProfEntry *gw_profiler_entry_alloc_ex(GwProf *prof, GwProfGroup *grp);
static void gw_profiler_restart_ex(GwProf *prof, GwProfGroup *grp, GwProfEntry *entry, int color);
static void gw_profiler_end_ex(GwProf *prof, GwProfGroup *grp, GwProfEntry *entry);
static GwWin *gw_profiler_window_create(GwProf *prof, int x, int y, int width, int height);
static void gw_profiler_apply_marks(void);

extern char gw_profiler_menu_defs[];
extern GwItem *gw_profiler_menu;
extern int gw_profiler_flag;
extern int fptoui(float f);

/* 0x0019bd00 */
static void gw_profiler_int_test(int val)
{
    gw_console_printf("int_test %04x\n", val);
}

/* Desktop menu entry plus the main profiler with VBLANK, MAPLE, RENDER, DMA. */
/* 0x0019bd10 */
static void gw_profiler_install(void)
{
    GwItem *menu = gw_desktop_get_menu();

    gw_profiler_menu = gw_menu_build(0, gw_profiler_menu_defs);
    gw_menu_append(menu, gw_profiler_menu);
    gw_profiler_main = gw_profiler_create();
    gw_prof_vblank = gw_profiler_group_create("VBLANK");
    gw_prof_maple = gw_profiler_group_create("MAPLE");
    gw_prof_render = gw_profiler_group_create("RENDER");
    gw_prof_dma = gw_profiler_group_create("DMA");
    gw_profiler_flag = 0;
}

/* 0x0019bda8 */
static void gw_profiler_uninstall(void)
{
    gw_profiler_window_detach();
}

/* Window at (32, 32), 320 wide, a row per group; closes it if open. */
/* 0x0019bdb0 */
static void gw_profiler_toggle(void)
{
    int cell[2];
    int n = 0;
    GwProfGroup *grp;

    for (grp = gw_profiler_main->groups; grp; grp = grp->next)
        n++;
    gw_get_cell_size(&cell[0], &cell[1]);
    if (gw_profiler_main->win == 0)
        gw_profiler_window_open(32, 32, 320, cell[1] + 8 + cell[1] * n);
    else
        gw_window_close(gw_profiler_main->win);
}

/* 0x0019be50 */
static void gw_profiler_window_attach(void)
{
}

/* 0x0019be58 */
static void gw_profiler_window_detach(void)
{
}

/* Samples every 12 frames, bar scale 5. */
/* 0x0019be60 */
static GwProf *gw_profiler_create(void)
{
    GwProf *prof;

    prof = gw_dc_calloc(1, 44, "gw_profiler.c", 316);
    prof->scale = 5.0f;
    prof->period = 12;
    prof->cnt = 1;
    return prof;
}

/* Closes its window, removes every group. */
/* 0x0019beb0 */
static void gw_profiler_destroy(GwProf *prof)
{
    if (prof) {
        if (prof->win)
            gw_window_close(prof->win);
        while (prof->groups)
            gw_profiler_remove_group(prof, prof->groups->name);
        gw_dc_free(prof, "gw_profiler.c", 349);
    }
}

/* Every prof->period frames the collected bars become the shown page and collection restarts. */
/* 0x0019bf40 */
static void gw_profiler_frame(GwProf *prof)
{
    GwProfGroup *grp;

    if (prof) {
        if (--prof->cnt <= 0) {
            prof->cnt = prof->period;
            prof->page ^= 1;
            prof->t0 = nj_timer_get_count();
            for (grp = prof->groups; grp; grp = grp->next) {
                grp->prev_count = grp->count - 1;
                grp->count = 0;
                grp->cur = 0;
            }
        }
    }
}

/* Main profiler. */
/* 0x0019bfb8 */
GwProfGroup *gw_profiler_group_create(char *name)
{
    return gw_profiler_add_group(gw_profiler_main, name);
}

/* Main profiler. */
/* 0x0019bfe0 */
static void gw_profiler_group_remove(char *name)
{
    gw_profiler_remove_group(gw_profiler_main, name);
}

/* Main profiler. */
/* 0x0019bff0 */
static GwProfGroup *gw_profiler_group_find(char *name)
{
    return gw_profiler_find_group(gw_profiler_main, name);
}

/* Zero-length bar. */
/* 0x0019c018 */
static void gw_profiler_mark(GwProfGroup *grp, int color)
{
    gw_profiler_mark_ex(gw_profiler_main, grp, color);
}

/* 0x0019c030 */
GwProfEntry *gw_profiler_begin(GwProfGroup *grp, int color)
{
    return gw_profiler_begin_ex(gw_profiler_main, grp, color);
}

/* 0x0019c058 */
static GwProfEntry *gw_profiler_end_last(GwProfGroup *grp)
{
    return gw_profiler_end_last_ex(gw_profiler_main, grp);
}

/* For the page being filled. */
/* 0x0019c080 */
static GwProfEntry *gw_profiler_entry_alloc(GwProfGroup *grp)
{
    return gw_profiler_entry_alloc_ex(gw_profiler_main, grp);
}

/* New colour, at the current counter. */
/* 0x0019c0a8 */
static void gw_profiler_restart(GwProfGroup *grp, GwProfEntry *entry, int color)
{
    gw_profiler_restart_ex(gw_profiler_main, grp, entry, color);
}

/* At the current counter. */
/* 0x0019c0c8 */
void gw_profiler_end(GwProfGroup *grp, GwProfEntry *entry)
{
    gw_profiler_end_ex(gw_profiler_main, grp, entry);
}

/* Global profiler. */
/* 0x0019c0e0 */
static GwWin *gw_profiler_window_open(int x, int y, int width, int height)
{
    return gw_profiler_window_create(gw_profiler_main, x, y, width, height);
}

/* At the end of the group list. */
/* 0x0019c118 */
static GwProfGroup *gw_profiler_add_group(GwProf *prof, char *name)
{
    GwProfGroup **pp;
    GwProfGroup *grp;
    GwProfGroup *tail;

    if (prof == 0)
        return 0;
    pp = &prof->groups;
    if (*pp) {
        do {
            tail = *pp;
            pp = &tail->next;
        } while (tail->next);
    }
    grp = gw_dc_calloc(1, 1044, "gw_profiler.c", 556);
    grp->name = name;
    *pp = grp;
    return grp;
}

/* 0x0019c1a8 */
static void gw_profiler_remove_group(GwProf *prof, char *name)
{
    GwProfGroup **pp;

    if (prof && prof->groups) {
        pp = &prof->groups;
        do {
            if (strcmp((*pp)->name, name) == 0) {
                GwProfGroup *grp = *pp;

                *pp = grp->next;
                gw_dc_free(grp, "gw_profiler.c", 590);
                return;
            }
            pp = &(*pp)->next;
        } while (*pp);
    }
}

/* 0x0019c238 */
static GwProfGroup *gw_profiler_find_group(GwProf *prof, char *name)
{
    GwProfGroup *grp;

    if (prof == 0)
        return 0;
    grp = prof->groups;
    if (grp) {
        do {
            if (strcmp(grp->name, name) == 0)
                return grp;
            grp = grp->next;
        } while (grp);
    }
    return 0;
}

/* Zero-length; time in 100 microsecond units. */
/* steer(cast) */
/* 0x0019c2a0 */
static void gw_profiler_mark_ex(GwProf *prof, GwProfGroup *grp, unsigned int color)
{
    GwProfEntry *entry;
    float usec;

    if (prof) {
        if (grp) {
            entry = gw_profiler_entry_alloc_ex(prof, grp);
            if (entry) {
                grp->cur = grp->count - 1;
                usec = (float)((unsigned (*)(unsigned))nj_timer_to_usec)(nj_timer_diff(prof->t0, nj_timer_get_count()));
                entry->color = color;
                entry->end = entry->start = (unsigned short)(usec * 0.01f);
            }
        }
    }
}

/* 0 when full. */
/* steer(cast) */
/* 0x0019c370 */
static GwProfEntry *gw_profiler_begin_ex(GwProf *prof, GwProfGroup *grp, int color)
{
    GwProfEntry *entry;
    float usec;

    if (prof == 0 || grp == 0)
        return 0;
    entry = gw_profiler_entry_alloc_ex(prof, grp);
    if (entry == 0)
        return 0;
    grp->cur = grp->count - 1;
    usec = (float)((unsigned (*)(unsigned))nj_timer_to_usec)(nj_timer_diff(prof->t0, nj_timer_get_count()));
    entry->color = color;
    entry->start = (unsigned short)(usec * 0.01f);
    return entry;
}

/* steer(cast) */
/* 0x0019c440 */
static GwProfEntry *gw_profiler_end_last_ex(GwProf *prof, GwProfGroup *grp)
{
    GwProfEntry *entry;
    float usec;

    if (prof == 0 || grp == 0)
        return 0;
    entry = &grp->ent[prof->page][grp->cur];
    usec = (float)((unsigned (*)(unsigned))nj_timer_to_usec)(nj_timer_diff(prof->t0, nj_timer_get_count()));
    entry->end = (unsigned short)(usec * 0.01f);
    return entry;
}

/* 64 per period; 0 when full. */
/* 0x0019c4f0 */
static GwProfEntry *gw_profiler_entry_alloc_ex(GwProf *prof, GwProfGroup *grp)
{
    if (prof == 0 || grp == 0)
        return 0;
    if (grp->count >= 64)
        return 0;
    return &grp->ent[prof->page][grp->count++];
}

/* New colour, at the current time. */
/* steer(cast) */
/* 0x0019c540 */
static void gw_profiler_restart_ex(GwProf *prof, GwProfGroup *grp, GwProfEntry *entry, int color)
{
    float usec;

    if (prof) {
        if (entry) {
            usec = (float)((unsigned (*)(unsigned))nj_timer_to_usec)(nj_timer_diff(prof->t0, nj_timer_get_count()));
            entry->color = color;
            entry->start = (unsigned short)(usec * 0.01f);
        }
    }
}

/* steer(cast) */
/* 0x0019c5e8 */
static void gw_profiler_end_ex(GwProf *prof, GwProfGroup *grp, GwProfEntry *entry)
{
    float usec;

    if (prof) {
        if (entry) {
            usec = (float)((unsigned (*)(unsigned))nj_timer_to_usec)(nj_timer_diff(prof->t0, nj_timer_get_count()));
            entry->end = (unsigned short)(usec * 0.01f);
        }
    }
}

/* One step per press. */
/* 0x0019c680 */
static void gw_profiler_scale_up_button(GwWin *win, int ev)
{
    if (ev == 0) {
        int pressed = gw_mouse_get_press();
        GwProf *prof = gw_window_get_user(win);

        if (pressed & 1)
            prof->scale += 1.0f;
    }
}

/* One step per press, down to 1. */
/* 0x0019c6e0 */
static void gw_profiler_scale_down_button(GwWin *win, int ev)
{
    if (ev == 0) {
        int pressed = gw_mouse_get_press();
        GwProf *prof = gw_window_get_user(win);

        if (pressed & 1) {
            prof->scale -= 1.0f;
            prof->scale = (prof->scale >= 1.0f) ? prof->scale : 1.0f;
        }
    }
}

/* One frame per press. */
/* 0x0019c758 */
static void gw_profiler_period_up_button(GwWin *win, int ev)
{
    if (ev == 0) {
        int pressed = gw_mouse_get_press();
        GwProf *prof = gw_window_get_user(win);

        if (pressed & 1)
            prof->period += 1;
    }
}

/* One frame per press, down to 1. */
/* 0x0019c7b0 */
static void gw_profiler_period_down_button(GwWin *win, int ev)
{
    if (ev == 0) {
        int pressed = gw_mouse_get_press();
        GwProf *prof = gw_window_get_user(win);

        if (pressed & 1) {
            if (prof->period >= 2)
                prof->period -= 1;
        }
    }
}

/* Does nothing in effect. */
/* 0x0019c818 */
static void gw_profiler_window_update(GwWin *win)
{
    if (gw_window_has_active(win))
        gw_window_get_user(win);
}

/*
 * Clipped to the row; overlapping bars one pixel lower, in front; non-empty bars get a darker end.
 */
/* 0x0019c850 */
static void gw_profiler_bar_draw(GwWin *win, GwProf *prof, GwProfGroup *grp, int x, int y, int bw, int bh, float zoom)
{
    GwVtx vtx[4];
    GwProfEntry *entry;
    int i;
    unsigned col;
    float z, fx, ytop, z_inset, fy, prev, fw, x1, x0;

    fx = (float)x;
    fy = (float)y;
    fw = (float)bw;
    z_inset = ((float (*)(int, float))gw_float_bits_add)(1, zoom); /* steer(cast) */
    prev = 0.0f;
    entry = grp->ent[prof->page];
    i = grp->prev_count;
    vtx[1].y = vtx[3].y = fy + (float)bh;
    while (i-- >= 0) {
        x0 = (float)entry->start * (prof->scale * 0.1f);
        x1 = (float)entry->end * (prof->scale * 0.1f);
        col = entry->color;
        vtx[0].col = vtx[1].col = col;
        if (fw <= x0)
            break;
        if (fw <= x1)
            x1 = fw;
        if (x1 <= x0) {
            vtx[2].col = col;
            x1 = x0 + 1.0f;
        } else {
            col &= 0x707070;
            vtx[2].col = col;
        }
        vtx[3].col = col;
        if (prev <= x0) {
            z = zoom;
            prev = x1;
            ytop = fy;
        } else {
            z = z_inset;
            ytop = fy + 1.0f;
        }
        x0 += fx;
        x1 += fx;
        vtx[0].y = vtx[2].y = ytop;
        vtx[0].z = vtx[1].z = vtx[2].z = vtx[3].z = z;
        vtx[0].x = vtx[1].x = x0;
        vtx[2].x = vtx[3].x = x1;
        nj_screen_strip_draw(vtx, 4, 0);
        entry++;
    }
}

/* Period and scale buttons with 'int' and 'width' values, then a labelled row per group. */
/* 0x0019ca78 */
static void gw_profiler_window_draw(GwWin *win)
{
    GwProf *prof;
    GwProfGroup *grp;
    int coords[6];
    int xa, x, y, barw, xoff;
    float zoom;

    prof = gw_window_get_user(win);
    gw_get_cell_size(&coords[0], &coords[1]);
    gw_window_get_abs_pos(win, &coords[2], &coords[3]);
    gw_window_set_pos(prof->btn[3], 2, 2);
    gw_window_get_size(prof->btn[3], &coords[4], &coords[5]);
    xa = coords[4];
    gw_window_set_pos(prof->btn[2], xa + 6, 2);
    {
        int tx = xa + 10;

        x = tx + coords[4];
    }
    gw_win_printf(win, x, 2, "int %2d", prof->period);
    x += coords[0] * 7;
    gw_window_set_pos(prof->btn[1], x, 2);
    {
        int tx = x + 4;

        x = tx + coords[4];
    }
    gw_window_set_pos(prof->btn[0], x, 2);
    {
        int tx = x + 4;

        gw_win_printf(win, tx + coords[4], 2, "width %2d", (int)prof->scale);
    }
    y = coords[5] + 6;
    gw_window_get_size(win, &coords[4], &coords[5]);
    grp = prof->groups;
    zoom = gw_float_bits_add(gw_window_get_scale(win), 1);
    xoff = coords[0] * 8 + 1;
    barw = coords[4] - coords[0] * 8 - 2;
    gw_profiler_apply_marks();
    while (grp) {
        gw_win_text(win, 1, y, grp->name);
        gw_profiler_bar_draw(win, prof, grp, coords[2] + xoff, coords[3] + y, barw, coords[1], zoom);
        y += coords[1];
        if (y >= coords[5])
            break;
        if (y + coords[1] >= coords[5])
            coords[1] = coords[5] - y;
        grp = grp->next;
    }
}

/* Main profiler: unticks the menu entry. */
/* 0x0019cc88 */
static void gw_profiler_window_destroyed(GwWin *win)
{
    GwProf *prof = gw_window_get_user(win);

    prof->win = 0;
    if (prof == gw_profiler_main) {
        gw_profiler_window_detach();
        gw_item_set_value(gw_profiler_menu, 0);
    }
}

/* Unless it has one; scale and period buttons. */
/* 0x0019ccd0 */
static GwWin *gw_profiler_window_create(GwProf *prof, int x, int y, int width, int height)
{
    GwWin *win;

    if (prof->win == 0) {
        win = gw_window_open("Profiler", 0x1005f, x, y, width, height, 0);
        prof->win = win;
        if (win == 0)
            return 0;
        gw_window_set_handlers(win, gw_profiler_window_update, gw_profiler_window_draw, gw_profiler_window_destroyed);
        prof->btn[0] = gw_icon_button_create(gw_profiler_scale_up_button, 19, 1.0f, 0, 0, 0, 0, win);
        prof->btn[1] = gw_icon_button_create(gw_profiler_scale_down_button, 18, 1.0f, 0, 0, 0, 0, win);
        prof->btn[2] = gw_icon_button_create(gw_profiler_period_up_button, 19, 1.0f, 0, 0, 0, 0, win);
        prof->btn[3] = gw_icon_button_create(gw_profiler_period_down_button, 18, 1.0f, 0, 0, 0, 0, win);
        gw_window_set_user(win, prof);
        gw_window_set_user(prof->btn[0], prof);
        gw_window_set_user(prof->btn[1], prof);
        gw_window_set_user(prof->btn[2], prof);
        gw_window_set_user(prof->btn[3], prof);
        if (prof == gw_profiler_main) {
            gw_profiler_window_attach();
            gw_item_set_value(gw_profiler_menu, 1);
        }
    }
    return prof->win;
}

/* Ticks the main profiler, marks VBLANK, starts a MAPLE bar. */
/* 0x0019ce88 */
static void gw_profiler_frame_start(void)
{
    gw_profiler_frame(gw_profiler_main);
    gw_profiler_mark_ex(gw_profiler_main, gw_prof_vblank, 0xffff0000);
    gw_profiler_begin_ex(gw_profiler_main, gw_prof_maple, 0xff00ff00);
}

/* 0x0019cec8 */
static void gw_profiler_mark_vblank(void)
{
    gw_profiler_mark_ex(gw_profiler_main, gw_prof_vblank, 0xff00ff00);
}

/*
 * Each pending RENDER mark starts its render time (nj_profile_get_timing) before its end, not
 * after.
 */
/* steer(cast) */
/* 0x0019cee0 */
static void gw_profiler_apply_marks(void)
{
    GwProfEntry *entry;
    int n, i;
    int *tick;
    unsigned ticks;

    if (gw_profiler_mark_cnt > 0) {
        ((void (*)(void *))nj_profile_get_timing)(gw_profiler_timing);
        n = gw_profiler_mark_cnt;
        tick = &gw_profiler_timing[1];
        do {
            i = n - 1;
            do { } while (0);
            ticks = *tick;
            n = i;
            gw_profiler_mark_cnt = i;
            entry = gw_profiler_marks[i];
            entry->start = entry->end - fptoui((float)ticks * 0.01f);
            if (!(entry->end > entry->start))
                entry->start = entry->end;
            tick++;
        } while (i > 0);
    }
}

/* Zero-length RENDER bar, queued (up to 8) until its render time is known. */
/* steer(cast) */
/* 0x0019cfd0 */
static void gw_profiler_push_mark(void)
{
    GwProf *prof = gw_profiler_main;
    GwProfEntry *entry;
    int now, n;

    now = (int)((float)((unsigned (*)(unsigned))nj_timer_to_usec)(nj_timer_diff(prof->t0, nj_timer_get_count())) * 0.01f);
    entry = gw_profiler_entry_alloc_ex(gw_profiler_main, gw_prof_render);
    if (entry) {
        entry->end = entry->start = now;
        entry->color = ARGB(0xff, 0xff, 0, 0xff);
        n = gw_profiler_mark_cnt;
        if (n < 8) {
            gw_profiler_marks[n] = entry;
            gw_profiler_mark_cnt = n + 1;
        }
    }
}

/* 0x0019d098 */
static void gw_profiler_mark_dma(void)
{
    gw_profiler_mark_ex(gw_profiler_main, gw_prof_dma, 0xffffff00);
}

/* 0x0019d0b0 */
static void gw_profiler_mark_dma_alt(void)
{
    gw_profiler_mark_ex(gw_profiler_main, gw_prof_dma, 0xff00e0e0);
}

/* 0x0019d0c8 */
static void gw_profiler_end_maple(void)
{
    gw_profiler_end_last_ex(gw_profiler_main, gw_prof_maple);
}
