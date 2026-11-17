/* Debug GUI 'Texture Viewer' (pad version): shoulder buttons step textures; zoom, name, formats. */

#include "types_gw.h"
#include "libc.h"
#include "gwu_texview.h"
#include "gw_draw.h"
#include "gw_heap.h"
#include "gw_menu.h"
#include "gw_system.h"
#include "gw_window.h"
#include "ps_pad.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"
#include "ul_texture.h"
#include "color.h"

extern float gw_texview_zoom_scales[];

/* "UNKNOWN" if not a known GS format. */
/* 0x00198050 */
static char *gw_texview_psm_name(unsigned psm)
{
    switch (psm) {
    case 0:
        return "PSMCT32";
    case 1:
        return "PSMCT24";
    case 2:
        return "PSMCT16";
    case 19:
        return "PSMT8";
    case 20:
        return "PSMT4";
    default:
        return "UNKNOWN";
    }
}

/*
 * Number and count, VRAM used/free, name (12 chars), id, size, formats, top mip, VRAM address/size.
 */
/* 0x001980c0 */
static void gw_texview_draw_info(void *win, GwTexView *tv, int *tex, int x, int y, int cw, int dy)
{
    int info[8];
    char name[16];
    unsigned psm;
    unsigned cpsm;
    char *pn;

    ulTextureGetInfo(info);
    gw_win_printf(win, x, y, "Texture: %4d/%4d", tv->index, info[3]);
    y += dy;
    gw_win_printf(win, 0, y, "Use Mem  : %06X Byte", info[1]);
    y += dy;
    gw_win_printf(win, 0, y, "Free Addr: %06X", info[6]);
    y += dy;
    strncpy(name, (char *)tex + 128, 12);
    y += dy;
    name[12] = 0;
    gw_win_printf(win, 0, y, "NAME:    %s", name);
    y += dy;
    gw_win_printf(win, 0, y, "ID:      %d", tex[42]);
    y += dy;
    gw_win_printf(win, 0, y, "Size:    %d x %d", ulTextureGetWidth(tex), ulTextureGetHeight(tex));
    y += dy;
    psm = tex[14];
    cpsm = tex[15];
    pn = gw_texview_psm_name(psm);
    gw_win_printf(win, 0, y, "PSM:     %s", pn);
    if (psm - 19 < 2)
        pn = gw_texview_psm_name(cpsm);
    else
        pn = "--------";
    y += dy;
    gw_win_printf(win, 0, y, "CPSM:    %s", pn);
    y += dy;
    gw_win_printf(win, 0, y, "MXL:     %1d", tex[13]);
    y += dy;
    gw_win_printf(win, 0, y, "TexMem:  %05X(%05X)", ulTextureGetAddr(tex), ulTextureGetSize(tex));
}

/* While browsing: the texture at zoom scale 24 cells in, info beside it. */
/* 0x001982f0 */
static void gw_texview_draw(void *win)
{
    GwTexView *tv;
    int *tex;
    GwTexVtx vtx[2];
    int px;
    int py;
    int cw;
    int ch;
    float z;
    float left;
    float top;
    float fw;
    float fh;

    tv = (GwTexView *)gw_window_get_user(win);
    gw_window_get_abs_pos(win, &px, &py);
    gw_get_cell_size(&cw, &ch);
    {
        int ix = px + cw * 24;
        int iy = py + ch;
        tv->y = (float)iy;
        tv->x = (float)ix;
    }
    z = gw_float_bits_add(gw_window_get_scale(win), 1);
    if (tv->mode == 1) {
        tex = ulTextureGetUsed(tv->index);
        if (tex) {
            ulGxSetTexture(tex);
            left = tv->x;
            fw = (float)tex[11];
            fh = (float)tex[12];
            vtx[0].x = left;
            vtx[0].y = top = tv->y;
            vtx[0].z = z;
            vtx[0].u = 0.0f;
            vtx[0].v = 0.0f;
            vtx[0].col = ARGB(0x80, 0x80, 0x80, 0x80);
            vtx[1].x = left + fw * tv->scale;
            vtx[1].y = top + fh * tv->scale;
            vtx[1].z = z;
            vtx[1].u = 1.0f;
            vtx[1].v = 1.0f;
            vtx[1].col = ARGB(0x80, 0x80, 0x80, 0x80);
            ulGxBeginSprite(1);
            ulGxAddSprites(vtx, 2);
            ulGxEndSprite();
        }
        gw_texview_draw_info(win, tv, tex, 0, 0, cw, ch);
    }
}

/* R2/L2 step zoom 0..4. */
/* 0x00198470 */
static void gw_texview_update_zoom(void *win, GwTexView *tv)
{
    int *pad;

    pad = psPadGet(tv->port);
    if (pad[14] & 2) {
        tv->level++;
        if (tv->level >= 5)
            tv->level = 4;
    } else if (pad[14] & 1) {
        tv->level--;
        if (tv->level < 0)
            tv->level = 0;
    }
    tv->scale = gw_texview_zoom_scales[tv->level];
}

/*
 * Start begins browsing; then L1/R1 step (wrap), Start stops and prints the texture list, R2/L2
 * zoom.
 */
/* steer(cast) */
/* 0x00198500 */
static void gw_texview_update(void *win)
{
    GwTexView *tv;
    int *pad;
    int n;
    void *dump_arg;

    tv = (GwTexView *)gw_window_get_user(win);
    pad = psPadGet(tv->port);
    n = ulTextureGetCount();
    switch (tv->mode) {
    case 0:
        if (pad[12] & 0x800)
            tv->mode = 1;
        break;
    case 1:
        if (pad[14] & 4)
            tv->index--;
        else if (pad[14] & 8)
            tv->index++;
        if (tv->index < 0)
            tv->index = n - 1;
        if (!(tv->index < n))
            tv->index = 0;
        if (pad[12] & 0x800) {
            dump_arg = (char *)win + (((int)tv & 1) >> 1); /* steer(zero) */
            if (!(((int)win & 1) >> 1))
                goto release;
            tv->level = 0;
        release:
            do { } while (0); /* steer(barrier) */
            tv->mode = 0;
            ((void (*)(void *))ulTextureDump)(dump_arg);
            do { } while (0); /* steer(barrier) */
        }
        gw_texview_update_zoom(win, tv);
        break;
    }
}

/* Unticks the menu entry, frees the state. */
/* 0x00198608 */
static void gw_texview_destroy(void *win)
{
    int item;
    int user;

    item = gw_window_get_slot(win, 0);
    if (item)
        gw_item_set_value(item, 0);
    user = gw_window_get_user(win);
    if (user) {
        gw_dc_free((void *)user, "gwu_texview.c", 330);
        gw_window_set_user(win, 0);
    }
}

/* 288 high, wide enough for info and a 288-pixel image; pad port 1, browsing, zoom 1. */
/* 0x00198678 */
void *gw_texview_open(int x, int y, void *parent)
{
    int cw;
    int ch;
    void *win;
    GwTexView *tv;

    gw_get_cell_size(&cw, &ch);
    win = gw_window_open("Texture Viewer", 0x1005f, x, y, cw * 24 + 288, 288, parent);
    if (win == 0)
        return 0;
    gw_window_set_handlers(win, gw_texview_update, gw_texview_draw, gw_texview_destroy);
    tv = gw_dc_calloc(1, 48, "gwu_texview.c", 358);
    gw_window_set_user(win, tv);
    tv->port = 1;
    tv->tex_info[0] = ulTextureGetRecords();
    tv->tex_info[1] = ulTextureGetMax();
    tv->mode = 1;
    tv->level = 2;
    tv->scale = 1.0f;
    return win;
}
