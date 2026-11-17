/* Debug GUI 'Peripheral Info' window: a pad port's controller state; toggles pressure mode. */

#include "types_gw.h"
#include "color.h"
#include "gwu_periinfo.h"
#include "gw_button.h"
#include "gw_draw.h"
#include "gw_heap.h"
#include "gw_menu.h"
#include "gw_system.h"
#include "libpad.h"

extern GwWin *gw_window_get_parent(GwWin *w);
extern GwPeri *gw_window_get_user(GwWin *w);
extern char *psPadGet(int id);
extern int psPadGetFlags(char *h);
extern int gw_window_get_slot(GwWin *w, int i);
extern int psPadGetType(GwPadInfo *h);
extern void gw_window_get_pos(void *w, int *x, int *y);
extern void gw_window_get_size(void *w, int *width, int *height);
extern void gw_window_set_user(GwWin *w, void *user);
extern GwWin *gw_window_open(char *title, int flags, int x, int y, int w, int h, GwWin *parent);
extern void gw_window_set_handlers(GwWin *w, GwFn update, GwFn draw, GwFn close);
extern void gw_window_set_slot(void *w, int i, int v);
extern int psPadGetState(unsigned char *pad);

/* Enabled only for a controller with pressure buttons; else pressure mode is noted off. */
/* 0x001a0090 */
static void gw_periinfo_vib_button_update(GwBtn *btn)
{
    GwPeri *peri = gw_window_get_user(gw_window_get_parent((GwWin *)btn));

    if (psPadGetFlags(psPadGet(gw_periinfo_ports[peri->port])) & 1) {
        gw_button_set_enabled(btn, 1);
        return;
    }
    gw_button_set_enabled(btn, 0);
    peri->vib = 0;
}

/* On release toggles pressure mode (stable pad with pressure buttons only). */
/* 0x001a0120 */
static void gw_periinfo_vib_button(GwWin *win, int ev)
{
    GwPeri *peri = gw_window_get_user(gw_window_get_parent(win));
    int id;
    char *pad;

    if (ev == 1) {
        id = gw_periinfo_ports[peri->port];
        pad = psPadGet(id);
        if (pad[38] & 1) {
            if (psPadGetState(pad) == 6) {
                switch (peri->vib) {
                case 0:
                    scePadEnterPressMode(id, 0);
                    peri->vib = ev;
                    break;
                case 1:
                    scePadExitPressMode(id, 0);
                    peri->vib = 0;
                    break;
                }
            }
        }
    }
}

/* '<' and '>': on release show the other pad port. */
/* 0x001a01f0 */
static void gw_periinfo_port_button(GwWin *win, int ev)
{
    GwPeri *peri = gw_window_get_user(gw_window_get_parent(win));

    if (ev == 1) {
        switch (gw_window_get_slot(win, 0)) {
        case 0:
            peri->port = peri->port - 1;
            if (peri->port < 0)
                peri->port = 1;
            break;
        case 1:
            peri->port = peri->port + 1;
            if (!(peri->port < 2))
                peri->port = 0;
            break;
        }
    }
}

/* 0x001a0290 */
static char *gw_pad_state_name(int state)
{
    switch (state) {
    case 0:
        return "scePadStateDiscon";
    case 1:
        return "scePadStateFindPad";
    case 2:
        return "scePadStateFindCTP1";
    case 5:
        return "scePadStateExecCmd";
    case 6:
        return "scePadStateStable";
    case 7:
        return "scePadStateError";
    case 99:
        return "scePadStateClosed";
    }
    return "????????????????";
}

/*
 * Port, pressure mode, id, modes, name, state, buttons, sticks, pressures (grey unless on),
 * actuators.
 */
/* 0x001a0348 */
static void gw_periinfo_draw(GwWin *win)
{
    int cw, ch, px, py, sw, sh;
    GwPeri *peri;
    GwPadInfo *pad;
    GwActInfo *act;
    int y, i, id, k;
    unsigned old;

    gw_get_cell_size(&cw, &ch);
    peri = gw_window_get_user(win);
    pad = psPadGet(gw_periinfo_ports[peri->port]);
    act = pad->act;
    peri->state = psPadGetState(pad);
    peri->id = psPadGetType(pad);
    gw_window_get_pos(peri->next_btn, &px, &py);
    gw_window_get_size(peri->next_btn, &sw, &sh);
    gw_win_printf(win, px + sw + cw, py, "%s", gw_periinfo_port_names[peri->port]);
    gw_window_get_pos(peri->vib_btn, &px, &py);
    gw_window_get_size(peri->vib_btn, &sw, &sh);
    if (peri->vib == 1)
        gw_win_printf(win, px + sw + cw, py, "ON");
    else
        gw_win_printf(win, px + sw + cw, py, "OFF");
    id = peri->id;
    k = 0;
    if (!(7 < (unsigned)id))
        k = id;
    y = py + sh + 2;
    gw_win_printf(win, 0, y, "ID    :%02X", id & 0xff);
    y += ch;
    gw_win_printf(win, 0, y, "(FLAG:%02X ID:%02X IDOFS:%d NBID:%d)", pad->flags, act->info[0], act->info[1], act->info[2]);
    y += ch;
    gw_win_printf(win, 0, y, "ID TBL:[%02X %02X %02X %02X]", act->info[3], act->info[4], act->info[5], act->info[6]);
    y += ch;
    gw_win_printf(win, 0, y, "NAME  :%s", gw_periinfo_type_names[k]);
    y += ch;
    gw_win_printf(win, 0, y, "STATUS:%s(%d)", gw_pad_state_name(peri->state), peri->state);
    y += ch;
    gw_win_printf(win, 0, y, "ON    :%08X", pad->on);
    y += ch;
    gw_win_printf(win, 0, y, "PRESS :%08X", pad->press);
    y += ch;
    gw_win_printf(win, 0, y, "REPEAT:%08X", pad->repeat);
    y += ch;
    gw_win_printf(win, 0, y, "(LX,LY):(%02X,%02X) (RX,RY):(%02X,%02X)", pad->stick[2], pad->stick[3], pad->stick[0], pad->stick[1]);
    if ((pad->flags & 1) && peri->vib == 1)
        old = gw_window_get_draw_color(win);
    else
        old = gw_window_set_draw_color(win, ARGB(0x80, 0x40, 0x40, 0x40));
    y += ch;
    y += ch;
    gw_win_printf(win, 0, y, "L1:%02X L2:%02X     R1:%02X R2:%02X", pad->pressure[8], pad->pressure[10], pad->pressure[9], pad->pressure[11]);
    y += ch;
    gw_win_printf(win, 0, y, "     %02X              %02X", pad->pressure[2], pad->pressure[4]);
    y += ch;
    gw_win_printf(win, 0, y, "   %02X  %02X          %02X  %02X", pad->pressure[1], pad->pressure[0], pad->pressure[7], pad->pressure[5]);
    y += ch;
    gw_win_printf(win, 0, y, "     %02X              %02X", pad->pressure[3], pad->pressure[6]);
    gw_window_set_draw_color(win, old);
    if (pad->flags & 2) {
        if (act->nact > 0) {
            y += ch;
            y += ch;
            gw_win_printf(win, 0, y, "Vibration : %d", act->nact);
            for (i = 0; i < act->nact; i++) {
                y += ch;
                gw_win_printf(win, 0, y, "FUNC:%d SUB:%d SIZE:%d CURR:%d mA", act->act[i].func, act->act[i].sub,
                             act->act[i].size, act->act[i].curr * 10);
            }
        }
    }
}

/* Looks up the pad record, nothing else. */
/* 0x001a0850 */
static void gw_periinfo_window_update(GwWin *win)
{
    psPadGet(gw_window_get_user(win)->port);
}

/* Unticks the menu entry, frees the state. */
/* 0x001a0870 */
static void gw_periinfo_window_close(GwWin *win)
{
    GwItem *it = gw_window_get_slot(win, 0);
    GwPeri *peri;

    if (it)
        gw_item_set_value(it, 0);
    peri = gw_window_get_user(win);
    if (peri) {
        gw_dc_free(peri, "gwu_periinfo.c", 351);
        gw_window_set_user(win, 0);
    }
}

/* 32 x 16 cells, port 0, '<', '>' and 'PRS' buttons. 0 on failure. */
/* 0x001a08e0 */
GwWin *gw_periinfo_window_create(int x, int y, GwWin *parent)
{
    int coords[4];
    GwWin *win;
    GwPeri *peri;
    int bw;

    gw_get_cell_size(&coords[0], &coords[1]);
    win = gw_window_open("Peripheral Info", 0x1005f, x, y, coords[0] << 5, coords[1] << 4, parent);
    if (win == 0)
        return 0;
    gw_window_set_handlers(win, gw_periinfo_window_update, gw_periinfo_draw, gw_periinfo_window_close);
    peri = gw_dc_calloc(1, 28, "gwu_periinfo.c", 378);
    if (peri) {
        gw_window_set_user(win, peri);
        peri->port = 0;
    }
    peri->prev_btn = gw_push_button_create(gw_periinfo_port_button, "<", 0, 0, -1, -1, win);
    gw_window_set_slot(peri->prev_btn, 0, 0);
    gw_window_get_size(peri->prev_btn, &coords[2], &coords[3]);
    bw = coords[2];
    peri->next_btn = gw_push_button_create(gw_periinfo_port_button, ">", bw, 0, -1, -1, win);
    gw_window_set_slot(peri->next_btn, 0, 1);
    peri->vib_btn = gw_push_button_create(gw_periinfo_vib_button, "PRS", coords[0] * 10, 0, -1, -1, win);
    gw_window_set_handlers(peri->vib_btn, gw_periinfo_vib_button_update, 0, 0);
    return win;
}
