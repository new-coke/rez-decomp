/*
 * Debug GUI 'Texture Viewer': steps through loaded textures, scrollable zoomable picture with id,
 * size, formats, VRAM; clamp, flip, blend, filter and background buttons.
 */

#include "types_gw.h"
#include "gwu_texture.h"
#include "libc.h"
#include "gw_button.h"
#include "gw_coloredit_win.h"
#include "gw_desktop.h"
#include "gw_draw.h"
#include "gw_menu.h"
#include "gw_mouse.h"
#include "gw_system.h"
#include "gw_window.h"
#include "nj_state.h"
#include "ps_pad.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"
#include "ul_texture.h"
#include "color.h"

#define CTL(p, off) ((p)->ctl[((off) - 20) / 4])
#define TV gw_texview_state

extern char gw_texview_menu_defs[];
extern GwItem *gw_texview_menu;
extern int gw_config_pad_port;
extern unsigned char gw_keyboard_state[];
extern int gw_texview_frame[];

/* 0x0019e9a8 */
void gw_texview_install(void)
{
    GwItem *menu = gw_desktop_get_menu();

    gw_texview_menu = gw_menu_build(0, gw_texview_menu_defs);
    gw_menu_append(menu, gw_texview_menu);
}

/* Bottom left, 21 cells + 256 wide, at least 256 high; zoom entry 6, clamp on, grey background. */
/* 0x0019e9f0 */
static void gw_texview_init_state(GwTexWin *tv)
{
    int rect[4];
    int height;
    int cell[2];
    int width;
    int x;
    int y;

    gw_get_desktop_rect(rect);
    gw_get_cell_size(&cell[0], &cell[1]);
    width = 21 * cell[0] + 256;
    tv->scx = 0;
    tv->scy = 0;
    height = 256;
    if (height - 1 < (cell[1] << 4))
        height = cell[1] << 4;
    x = rect[0] + 4;
    y = rect[3] - height - 4;
    tv->zoom = gw_texview_zoom_table[6];
    tv->w = width;
    tv->x = x;
    tv->y = y;
    tv->back = 0xff808080;
    tv->count = 10;
    tv->flip_u = 0;
    tv->clamp = 1;
    tv->zi = 6;
    tv->h = height;
    tv->flip_v = 0;
    tv->flip = 0;
    tv->tr = 0;
    tv->ss = 0;
    tv->bi = 0;
    tv->index = 0;
}

/* Stops at the last, or wraps to the first. */
/* 0x0019eac8 */
static void gw_texview_next(GwTexWin *tv, int wrap)
{
    int i = tv->index + 1;

    tv->index = i;
    if (i < tv->count)
        return;
    if (wrap)
        tv->index = 0;
    else
        tv->index = tv->count - 1;
}

/* Stops at the first, or wraps to the last. */
/* 0x0019eb00 */
static void gw_texview_prev(GwTexWin *tv, int wrap)
{
    int i = tv->index - 1;

    tv->index = i;
    if (i >= 0)
        return;
    if (wrap)
        tv->index = tv->count - 1;
    else
        tv->index = 0;
}

/* Zoom table entry zi; the scroll position scales with it. */
/* 0x0019eb30 */
static void gw_texview_apply_zoom(GwTexWin *tv)
{
    float old_zoom, new_zoom;
    float *tab = gw_texview_zoom_table;

    old_zoom = tv->zoom;

    tv->scx = tv->scx / old_zoom;
    tv->scy = tv->scy / old_zoom;
    new_zoom = tab[tv->zi];
    tv->zoom = new_zoom;
    tv->scx = tv->scx * new_zoom;
    tv->scy = tv->scy * new_zoom;
    tv->scx = floorf(tv->scx);
    tv->scy = floorf(tv->scy);
}

/* Left/Right step textures (a fresh press wraps), Up/Down zoom (0..17), L1/R1 step and wrap. */
/* 0x0019ebc0 */
static void gw_texview_handle_input(GwWin *win, GwTexWin *tv)
{
    int *pad = psPadGet(gw_config_pad_port);
    int n = ulTextureGetCount();
    unsigned char *keys;
    int buttons, zoom_idx;

    if (tv->index < 0)
        tv->index = 0;
    if (!(tv->index < n))
        tv->index = n - 1;
    keys = gw_keyboard_state;
    if (keys[14] == 79)
        gw_texview_next(tv, keys[13] == 79);
    if (keys[14] == 80)
        gw_texview_prev(tv, keys[13] == 80);
    if (keys[14] == 82)
        tv->zi = tv->zi - 1;
    if (keys[14] == 81)
        tv->zi = tv->zi + 1;
    zoom_idx = tv->zi;
    zoom_idx = (-1 < zoom_idx) ? zoom_idx : 0;
    if (!((unsigned)zoom_idx < 18))
        zoom_idx = 17;
    tv->zi = zoom_idx;
    gw_texview_apply_zoom(tv);
    buttons = pad[14];
    if (buttons & 4)
        gw_texview_prev(tv, 1);
    else if (buttons & 8)
        gw_texview_next(tv, 1);
}

/* Picture right, button rows left, texture number and zoom print positions. */
/* 0x0019ed30 */
static void gw_texview_layout(GwWin *win, GwTexWin *tv)
{
    int coords[7];
    int bw, bh;

    gw_window_get_pos(win, &coords[0], &coords[1]);
    gw_window_get_size(win, &coords[2], &coords[3]);
    gw_get_cell_size(&coords[4], &coords[5]);
    bw = coords[2] - coords[4] * 21 - 2;
    bh = coords[3] - 4;
    tv->x = coords[0];
    tv->y = coords[1];
    tv->w = coords[2];
    tv->h = coords[3];
    gw_window_set_pos(CTL(tv, 20), coords[4] * 21, 2);
    gw_window_set_size(CTL(tv, 20), bw, bh);
    coords[0] = 2;
    gw_window_get_size(CTL(tv, 24), &coords[2], &coords[6]);
    coords[1] = coords[3] - coords[6] - 4;
    gw_window_set_pos(CTL(tv, 24), coords[0], coords[1]);
    coords[0] = 2;
    coords[1] = 2;
    gw_window_set_pos(CTL(tv, 44), 2, 2);
    gw_window_get_size(CTL(tv, 44), &coords[2], &coords[3]);
    coords[0] += 4; coords[0] += coords[2];
    gw_window_set_pos(CTL(tv, 52), coords[0], coords[1]);
    gw_window_get_size(CTL(tv, 52), &coords[2], &coords[3]);
    coords[0] += 4; coords[0] += coords[2];
    gw_window_set_pos(CTL(tv, 48), coords[0], coords[1]);
    gw_window_get_size(CTL(tv, 48), &coords[2], &coords[3]);
    coords[0] = 2;
    coords[1] += 4; coords[1] += coords[3];
    gw_window_set_pos(CTL(tv, 56), 2, coords[1]);
    gw_window_get_size(CTL(tv, 56), &coords[2], &coords[3]);
    coords[0] += 4; coords[0] += coords[2];
    gw_window_set_pos(CTL(tv, 60), coords[0], coords[1]);
    gw_window_get_size(CTL(tv, 60), &coords[2], &coords[3]);
    coords[0] += 4; coords[0] += coords[2];
    gw_window_set_pos(CTL(tv, 64), coords[0], coords[1]);
    gw_window_get_size(CTL(tv, 64), &coords[2], &coords[3]);
    coords[0] += 4; coords[0] += coords[2];
    gw_window_set_pos(CTL(tv, 68), coords[0], coords[1]);
    coords[0] = 2;
    coords[1] += 4; coords[1] += coords[3];
    gw_window_set_pos(CTL(tv, 32), 2, coords[1]);
    gw_window_get_size(CTL(tv, 32), &coords[2], &coords[3]);
    coords[0] += 4; coords[0] += coords[2];
    gw_window_set_pos(CTL(tv, 28), coords[0], coords[1]);
    gw_window_get_size(CTL(tv, 28), &coords[2], &coords[3]);
    {
        int tx = coords[0] + 4;

        tv->lay[2] = tx + coords[2];
    }
    tv->lay[3] = coords[1];
    coords[0] = 2;
    {
        int top = coords[1] + 4;

        coords[1] = top + coords[3];
    }
    gw_window_set_pos(CTL(tv, 40), 2, coords[1]);
    gw_window_get_size(CTL(tv, 40), &coords[2], &coords[3]);
    coords[0] += 4; coords[0] += coords[2];
    gw_window_set_pos(CTL(tv, 36), coords[0], coords[1]);
    gw_window_get_size(CTL(tv, 36), &coords[2], &coords[3]);
    tv->lay[1] = coords[1];
    coords[0] += 4; coords[0] += coords[2];
    coords[1] += 4; coords[1] += coords[3];
    tv->lay[0] = coords[0];
    tv->lay[4] = coords[1];
}

/* Scroll position and scaled size used this frame. */
/* 0x0019f0b8 */
static void gw_texview_save_scroll(int win, GwTexWin *tv)
{
    float scx = tv->scx;
    float scy = tv->scy;
    float sc2x = tv->sc2x;
    float sc2y = tv->sc2y;

    tv->draw_scx = scx;
    tv->draw_scy = scy;
    tv->draw_sc2x = sc2x;
    tv->draw_sc2y = sc2y;
}

/* 0x0019f0e0 */
static void gw_texview_update_count(GwTexWin *tv)
{
    tv->count = ulTextureGetCount();
}

/* While active. */
/* 0x0019f108 */
static void gw_texview_window_update(GwWin *win)
{
    if (gw_window_has_active(win)) {
        gw_texview_handle_input(win, &gw_texview_state);
        gw_texview_layout(win, &gw_texview_state);
    }
}

/* Unticks the menu entry. */
/* 0x0019f168 */
static void gw_texview_window_close(void)
{
    gw_texview_frame[0] = 0;
    gw_item_set_value(gw_texview_menu, 0);
}

/* "UNKNOWN" if not a known GS format. */
/* 0x0019f180 */
static char *gw_psm_name(unsigned psm)
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
    }
    return "UNKNOWN";
}

/* Number, zoom, id, size, formats, top mip, VRAM address/size, used/free; also the zoomed size. */
/* 0x0019f1f0 */
static void gw_texview_window_draw(GwWin *win)
{
    GwTexWin *tv = &gw_texview_state;
    unsigned old;
    int info[8];
    int cell[2];
    GwTex *tex;
    int x, y;
    int psm, cpsm;
    char *name;

    old = gw_window_set_draw_color(win, gw_get_default_color());
    gw_get_cell_size(&cell[0], &cell[1]);
    tex = ulTextureGetUsed(tv->index);
    if (tex) {
        gw_win_printf(win, tv->lay[2], tv->lay[3], "%4d/%4d", tv->index, tv->count - 1);
        x = tv->lay[0];
        y = tv->lay[1];
        gw_win_printf(win, x, y, "%4.2f", tv->zoom);
        y = tv->lay[4];
        tv->id = tex->id;
        tv->sc2x = (float)ulTextureGetWidth(tex) * tv->zoom;
        tv->sc2y = (float)ulTextureGetHeight(tex) * tv->zoom;
        gw_win_printf(win, 1, y, "Id:      %08x", tex->id);
        y += cell[1];
        gw_win_printf(win, 1, y, "Size:    %dx%d", ulTextureGetWidth(tex), ulTextureGetHeight(tex));
        psm = tex->psm;
        cpsm = tex->cpsm;
        name = gw_psm_name(psm);
        y += cell[1];
        gw_win_printf(win, 0, y, "PSM:     %s", name);
        name = (unsigned)(psm - 19) < 2 ? gw_psm_name(cpsm) : "--------";
        y += cell[1];
        gw_win_printf(win, 0, y, "CPSM:    %s", name);
        y += cell[1];
        gw_win_printf(win, 0, y, "MXL:     %1d", tex->mxl);
        y += cell[1];
        gw_win_printf(win, 0, y, "TexMem:  %05X(%05X)", ulTextureGetAddr(tex), ulTextureGetSize(tex));
        y += cell[1];
        ulTextureGetInfo(info);
        y += cell[1];
        gw_win_printf(win, 0, y, "Use Mem  :%05X Byte", info[1]);
        gw_win_printf(win, 0, y + cell[1], "Free Addr:%05X", info[6]);
        gw_window_set_draw_color(win, old);
    }
}

/* Left drag scrolls. */
/* 0x0019f4a8 */
static void gw_texview_picture_update(GwWin *win)
{
    GwTexWin *tv;
    int coords[4];

    if (gw_window_is_active(win)) {
        tv = &gw_texview_state;
        if (gw_mouse_get_on() & 1) {
            gw_mouse_get_prev_pos(&coords[0], &coords[1]);
            gw_mouse_get_pos(&coords[2], &coords[3]);
            coords[2] = coords[2] - coords[0];
            coords[3] = coords[3] - coords[1];
            tv->scx = tv->scx - (float)coords[2];
            tv->scy = tv->scy - (float)coords[3];
        }
    }
}

/* With flip, blend, filter settings; clamp stops the picture at the texture edges. */
/* steer(cast) */
/* 0x0019f540 */
static void gw_texview_picture_draw(GwWin *win)
{
    GwTexWin *tv = &gw_texview_state;
    GwSpritePt pt[2];
    int coords[4];
    float v0;
    float u1;
    float u0;
    float y0;
    float z;
    float x0;
    float fh;
    float y1;
    float fw;
    float v1;
    float x1;
    int ss_old, filter_old;
    GwTex *tex;

    gw_texview_update_count(tv);
    gw_texview_save_scroll(win, tv);
    gw_window_get_abs_pos(win, &coords[0], &coords[1]);
    gw_window_get_size(win, &coords[2], &coords[3]);
    z = gw_float_bits_add(gw_window_get_scale(win), 1);
    fw = (float)coords[2];
    fh = (float)coords[3];
    x1 = (float)(coords[0] + coords[2]);
    u0 = tv->draw_scx / tv->draw_sc2x;
    v0 = tv->draw_scy / tv->draw_sc2y;
    y1 = (float)(coords[1] + coords[3]);
    x0 = (float)coords[0];
    y0 = (float)coords[1];
    u1 = (tv->draw_scx + fw) / tv->draw_sc2x;
    v1 = (tv->draw_scy + fh) / tv->draw_sc2y;
    if (tv->clamp) {
        if (u0 < 0.0f) {
            u0 = 0.0f;
            x0 -= tv->scx;
        }
        if (v0 < 0.0f) {
            v0 = 0.0f;
            y0 -= tv->scy;
        }
        if (1.0f <= u1) {
            u1 = 1.0f;
            x1 -= (tv->scx - tv->sc2x) + fw;
        }
        if (1.0f <= v1) {
            v1 = 1.0f;
            y1 -= (tv->scy - tv->sc2y) + fh;
        }
    }
    pt[0].x = floorf(x0);
    pt[1].x = floorf(x1);
    pt[0].y = floorf(y0);
    pt[1].y = floorf(y1);
    pt[0].u = u0;
    pt[1].u = u1;
    pt[0].v = v0;
    pt[1].v = v1;
    if (u1 < u0 || v1 < v0)
        return;
    pt[1].z = z;
    pt[1].col = ARGB(0x80, 0x80, 0x80, 0x80);
    pt[0].z = z;
    pt[0].col = ARGB(0x80, 0x80, 0x80, 0x80);
    ss_old = gw_change_ss_mode(tv->ss);
    ((void (*)(unsigned))nj_texture_flip_mode)(tv->flip);
    njTextureClampMode(0);
    filter_old = gw_change_tex_filter(tv->bi != 0);
    tex = ulTextureGetUsed(tv->index);
    if (tex) {
        ulGxSetTexture(tex);
        ulGxBeginSprite(tv->tr != 0);
        ulGxAddSprites(pt, 2);
        ulGxEndSprite();
        gw_change_tex_filter(filter_old);
        gw_change_ss_mode(ss_old);
    }
}

/* Background colour: alpha halved, R/B swapped to GS order. */
/* 0x0019f808 */
static void gw_texview_back_color_changed(GwWin *win)
{
    GwTexWin *tv = &gw_texview_state;
    unsigned argb = gw_coloredit_get_color(win);
    unsigned abgr;

    abgr = ((((((argb >> 24) + 1) >> 1) << 24) | (argb & 0xff00)) | (((argb >> 16) & 0xff) | ((argb & 0xff) << 16)));
    tv->back = abgr;
    gw_window_set_color(tv->ctl[0], abgr);
}

/* 'BackColor': colour editor for the background. */
/* 0x0019f870 */
static void gw_texview_back_color_button(GwWin *win, int ev)
{
    GwTexWin *tv = &gw_texview_state;
    GwWin *edit;

    if (ev == 0) {
        gw_mouse_get_press();
        if (gw_mouse_get_repeat() & 1) {
            edit = gw_coloredit_window_create(0, tv->back, 64, 64, 0, 0, 0);
            if (edit)
                gw_coloredit_set_change_callback(edit, gw_texview_back_color_changed);
        }
    }
}

/* Per left repeat; the first press may wrap to the first texture. */
/* 0x0019f8f0 */
static void gw_texview_next_button(GwWin *win, int ev)
{
    GwTexWin *tv = &gw_texview_state;
    int press;

    if (ev == 0) {
        press = gw_mouse_get_press();
        if (gw_mouse_get_repeat() & 1)
            gw_texview_next(tv, press & 1);
    }
}

/* Per left repeat; the first press may wrap to the last texture. */
/* 0x0019f958 */
static void gw_texview_prev_button(GwWin *win, int ev)
{
    GwTexWin *tv = &gw_texview_state;
    int press;

    if (ev == 0) {
        press = gw_mouse_get_press();
        if (gw_mouse_get_repeat() & 1)
            gw_texview_prev(tv, press & 1);
    }
}

/* Per left repeat, up to entry 17. */
/* 0x0019f9c0 */
static void gw_texview_zoom_in_button(GwWin *win, int ev)
{
    GwTexWin *tv = &gw_texview_state;
    int zoom_idx;

    if (ev == 0) {
        if (gw_mouse_get_repeat() & 1) {
            zoom_idx = tv->zi + 1;
            zoom_idx = (-1 < zoom_idx) ? zoom_idx : 0;
            if (!((unsigned)zoom_idx < 18))
                zoom_idx = 17;
            tv->zi = zoom_idx;
            gw_texview_apply_zoom(tv);
        }
    }
}

/* Per left repeat, down to entry 0. */
/* 0x0019fa30 */
static void gw_texview_zoom_out_button(GwWin *win, int ev)
{
    GwTexWin *tv = &gw_texview_state;
    int zoom_idx;

    if (ev == 0) {
        if (gw_mouse_get_repeat() & 1) {
            zoom_idx = tv->zi - 1;
            zoom_idx = (-1 < zoom_idx) ? zoom_idx : 0;
            if (!((unsigned)zoom_idx < 18))
                zoom_idx = 17;
            tv->zi = zoom_idx;
            gw_texview_apply_zoom(tv);
        }
    }
}

/* 'Clamp': picture stays inside the texture edges. */
/* 0x0019faa0 */
static void gw_texview_clamp_button(GwBtn *btn)
{
    GwTexWin *tv = &gw_texview_state;

    tv->clamp = gw_toggle_button_get(btn);
}

/* From the FlipU and FlipV toggles. */
/* 0x0019fad0 */
static void gw_texview_update_flip(GwTexWin *tv)
{
    if (tv->flip_u) {
        if (tv->flip_v)
            tv->flip = 0x60000;
        else
            tv->flip = 0x40000;
    } else {
        if (tv->flip_v)
            tv->flip = 0x20000;
        else
            tv->flip = 0;
    }
}

/* 'FlipU': horizontal mirror. */
/* 0x0019fb10 */
static void gw_texview_flip_u_button(GwBtn *btn)
{
    GwTexWin *tv = &gw_texview_state;

    tv->flip_u = gw_toggle_button_get(btn);
    gw_texview_update_flip(tv);
}

/* 'FlipV': vertical mirror. */
/* 0x0019fb40 */
static void gw_texview_flip_v_button(GwBtn *btn)
{
    GwTexWin *tv = &gw_texview_state;

    tv->flip_v = gw_toggle_button_get(btn);
    gw_texview_update_flip(tv);
}

/* 'TR': alpha blending. */
/* 0x0019fb70 */
static void gw_texview_tr_button(GwBtn *btn)
{
    GwTexWin *tv = &gw_texview_state;

    tv->tr = gw_toggle_button_get(btn);
}

/* 'SS': the GUI's ss mode while drawing. */
/* 0x0019fba0 */
static void gw_texview_ss_button(GwBtn *btn)
{
    GwTexWin *tv = &gw_texview_state;

    tv->ss = gw_toggle_button_get(btn);
}

/* 'BI': bilinear filtering. */
/* 0x0019fbd0 */
static void gw_texview_bi_button(GwBtn *btn)
{
    GwTexWin *tv = &gw_texview_state;

    tv->bi = gw_toggle_button_get(btn);
}

/* 'INIT': zoom entry 6, top-left corner. */
/* 0x0019fc00 */
static void gw_texview_init_button(GwWin *win, int ev)
{
    GwTexWin *tv = &gw_texview_state;
    int press;

    if (ev == 0) {
        press = gw_mouse_get_press();
        gw_mouse_get_repeat();
        if (press & 1) {
            tv->zi = 6;
            tv->scx = 0;
            tv->scy = 0;
            gw_texview_apply_zoom(tv);
        }
    }
}

/* Desktop menu entry: closes the viewer, or opens it with picture, arrow buttons and toggles. */
/* 0x0019fc70 */
static void gw_texview_toggle(void)
{
    int cell[2];

    if (TV.frame) {
        gw_window_close(TV.frame);
        return;
    }
    gw_get_cell_size(&cell[0], &cell[1]);
    if (TV.w == 0)
        gw_texview_init_state(&TV);
    TV.frame = gw_window_open("Texture Viewer", 0x1005f, TV.x, TV.y, TV.w, TV.h, 0);
    gw_window_set_handlers(TV.frame, gw_texview_window_update, gw_texview_window_draw, gw_texview_window_close);
    TV.ctl[0] = gw_window_open("Texture", 0x40128, 0, 0, 0, 0, TV.frame);
    gw_window_set_handlers(TV.ctl[0], gw_texview_picture_update, gw_texview_picture_draw, 0);
    gw_window_set_color(TV.ctl[0], TV.back);
    TV.ctl[2] = gw_icon_button_create(gw_texview_next_button, 19, 1.0f, 0, 0, 12, 12, TV.frame);
    TV.ctl[3] = gw_icon_button_create(gw_texview_prev_button, 18, 1.0f, 0, 0, 12, 12, TV.frame);
    TV.ctl[4] = gw_icon_button_create(gw_texview_zoom_in_button, 19, 1.0f, 0, 0, 12, 12, TV.frame);
    TV.ctl[5] = gw_icon_button_create(gw_texview_zoom_out_button, 18, 1.0f, 0, 0, 12, 12, TV.frame);
    TV.ctl[1] = gw_push_button_create(gw_texview_back_color_button, "BackColor", 0, 0, cell[0] * 10, cell[1] + 1, TV.frame);
    TV.ctl[6] = gw_toggle_button_create(gw_texview_clamp_button, "Clamp", 0, 0, cell[0] * 6, cell[1] + 1, TV.frame);
    gw_toggle_button_set(TV.ctl[6], TV.clamp);
    TV.ctl[8] = gw_toggle_button_create(gw_texview_flip_u_button, "FlipU", 0, 0, cell[0] * 6, cell[1] + 1, TV.frame);
    gw_toggle_button_set(TV.ctl[8], TV.flip_u);
    TV.ctl[7] = gw_toggle_button_create(gw_texview_flip_v_button, "FlipV", 0, 0, cell[0] * 6, cell[1] + 1, TV.frame);
    gw_toggle_button_set(TV.ctl[7], TV.flip_v);
    TV.ctl[9] = gw_toggle_button_create(gw_texview_tr_button, "TR", 0, 0, cell[0] * 3, cell[1] + 1, TV.frame);
    gw_toggle_button_set(TV.ctl[9], TV.tr);
    TV.ctl[10] = gw_toggle_button_create(gw_texview_ss_button, "SS", 0, 0, cell[0] * 3, cell[1] + 1, TV.frame);
    gw_toggle_button_set(TV.ctl[10], TV.ss);
    TV.ctl[11] = gw_toggle_button_create(gw_texview_bi_button, "BI", 0, 0, cell[0] * 3, cell[1] + 1, TV.frame);
    gw_toggle_button_set(TV.ctl[11], TV.bi);
    TV.ctl[12] = gw_push_button_create(gw_texview_init_button, "INIT", 0, 0, cell[0] * 5, cell[1] + 1, TV.frame);
    gw_item_set_value(gw_texview_menu, 1);
}

/* 0x001a0050 */
static void gw_texview_close(void)
{
    GwTexWin *tv = &gw_texview_state;

    if (tv->frame) {
        gw_window_close(tv->frame);
        tv->frame = 0;
    }
}
