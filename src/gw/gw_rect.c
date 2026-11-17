/* Debug GUI shapes in window coordinates: bevelled lines, outlines, solid and four-colour fills. */

#include "types_gw.h"
#include "gw_rect.h"
#include "gw_draw.h"
#include "gw_window.h"
#include "ul_gx_prim.h"

/* Two pixels: col over darkened col for mode 1, else the reverse. */
/* 0x00195058 */
static void gw_draw_bevel_hline(GwWin *win, int mode, int x0, int y, int x1, unsigned col, int layer)
{
    int ax;
    int ay;
    float left;
    float top;
    float right;
    float z;
    unsigned first_col;
    unsigned second_col;
    GwVtx vtx[4];

    gw_window_get_abs_pos(win, &ax, &ay);
    left = (float)(ax + x0);
    top = (float)(ay + y);
    right = (float)(ax + x1);
    z = gw_float_bits_add(win->scale, layer);
    vtx[3].z = z;
    vtx[2].z = z;
    vtx[1].z = z;
    vtx[0].z = z;
    if (mode == 1) {
        first_col = col;
        second_col = gw_color_scale(first_col, 0.2f);
    } else {
        first_col = gw_color_scale(col, 0.2f);
        second_col = col;
    }
    ulGxBeginStrip(1);
    vtx[1].y = top + 1.0f;
    vtx[2].x = right + 1.0f;
    vtx[0].x = left;
    vtx[0].y = top;
    vtx[3].y = top + 1.0f;
    vtx[2].y = top;
    vtx[1].x = left;
    vtx[3].col = first_col;
    vtx[2].col = first_col;
    vtx[1].col = first_col;
    vtx[0].col = first_col;
    vtx[3].x = right + 1.0f;
    ulGxAddStrip(vtx, 4);
    vtx[1].y = top + 2.0f;
    vtx[0].y = top + 1.0f;
    vtx[3].col = second_col;
    vtx[3].y = top + 2.0f;
    vtx[2].col = second_col;
    vtx[1].col = second_col;
    vtx[2].y = top + 1.0f;
    vtx[0].col = second_col;
    ulGxAddStrip(vtx, 4);
    ulGxEndStrip();
}

/* Two pixels: col left of darkened col for mode 1, else the reverse. */
/* 0x00195200 */
void gw_draw_bevel_vline(GwWin *win, int mode, int x, int y0, int y1, unsigned col, int layer)

{
    unsigned second_col;
    int ax;
    GwVtx vtx[4];
    float top;
    float left;
    int ay;
    float bottom;
    unsigned first_col;
    float z;
    gw_window_get_abs_pos(win, &ax, &ay);
    top = (float)(ay + y0);
    left = (float)(ax + x);
    bottom = (float)(ay + y1);
    z = gw_float_bits_add(win->scale, layer);
    vtx[3].z = z;
    vtx[2].z = z;
    vtx[1].z = z;
    vtx[0].z = z;
    if (mode == 1) {
        first_col = col;
        second_col = gw_color_scale(first_col, 0.2f);
    } else {
        first_col = gw_color_scale(col, 0.2f);
        second_col = col;
    }
    ulGxBeginStrip(1);
    vtx[0].x = left;
    vtx[0].y = top;
    vtx[1].y = bottom + 1.0f;
    vtx[2].y = top;
    vtx[1].x = left;
    vtx[2].x = left + 1.0f;
    vtx[3].x = left + 1.0f;
    vtx[3].col = first_col;
    vtx[3].y = bottom + 1.0f;
    vtx[2].col = first_col;
    vtx[1].col = first_col;
    vtx[0].col = first_col;
    ulGxAddStrip(vtx, 4);
    vtx[2].x = left + 2.0f;
    vtx[0].x = left + 1.0f;
    vtx[3].x = left + 2.0f;
    vtx[3].col = second_col;
    vtx[1].x = left + 1.0f;
    vtx[2].col = second_col;
    vtx[1].col = second_col;
    vtx[0].col = second_col;
    ulGxAddStrip(vtx, 4);
    ulGxEndStrip();
}

/* One pixel. */
/* 0x001953a8 */
static void gw_draw_rect_outline(GwWin *win, int x0, int y0, int x1, int y1, unsigned col, int layer)
{
    int ax;
    int ay;
    float left;
    float top;
    float right;
    float bottom;
    float z;
    GwVtx vtx[4];

    gw_window_get_abs_pos(win, &ax, &ay);
    left = (float)(ax + x0);
    right = (float)(ax + x1);
    top = (float)(ay + y0);
    bottom = (float)(ay + y1);
    z = gw_float_bits_add(win->scale, layer);
    vtx[3].z = z;
    vtx[2].z = z;
    vtx[1].z = z;
    vtx[3].col = col;
    vtx[2].col = col;
    vtx[1].col = col;
    vtx[0].col = col;
    vtx[0].z = z;
    ulGxBeginStrip(1);
    vtx[0].x = left;
    vtx[0].y = top;
    vtx[1].x = left;
    vtx[1].y = bottom;
    vtx[2].x = left + 1.0f;
    vtx[2].y = top;
    vtx[3].x = left + 1.0f;
    vtx[3].y = bottom;
    ulGxAddStrip(vtx, 4);
    vtx[0].x = left;
    vtx[0].y = bottom;
    vtx[1].y = bottom + 1.0f;
    vtx[2].x = right;
    vtx[3].x = right;
    vtx[1].x = left;
    vtx[2].y = bottom;
    vtx[3].y = bottom + 1.0f;
    ulGxAddStrip(vtx, 4);
    vtx[0].y = top + 1.0f;
    vtx[1].x = right;
    vtx[1].y = bottom + 1.0f;
    vtx[2].x = right + 1.0f;
    vtx[3].y = bottom + 1.0f;
    vtx[0].x = right;
    vtx[2].y = top + 1.0f;
    vtx[3].x = right + 1.0f;
    ulGxAddStrip(vtx, 4);
    vtx[0].x = left + 1.0f;
    vtx[1].x = left + 1.0f;
    vtx[0].y = top;
    vtx[1].y = top + 1.0f;
    vtx[2].x = right + 1.0f;
    vtx[2].y = top;
    vtx[3].x = right + 1.0f;
    vtx[3].y = top + 1.0f;
    ulGxAddStrip(vtx, 4);
    ulGxEndStrip();
}

/* Corners included; layer steps above the window. */
/* 0x00195598 */
void gw_draw_rect_fill(GwWin *win, int x0, int y0, int x1, int y1, unsigned col, float layer)
{
    int ax;
    int ay;
    int layer_idx;
    float left;
    float top;
    float right;
    float bottom;
    float z;
    GwVtx vtx[4];

    gw_window_get_abs_pos(win, &ax, &ay);
    layer_idx = (int)layer;
    left = (float)(ax + x0);
    top = (float)(ay + y0);
    right = (float)(ax + x1 + 1);
    bottom = (float)(ay + y1 + 1);
    z = gw_float_bits_add(win->scale, layer_idx);
    vtx[0].col = vtx[1].col = vtx[2].col = vtx[3].col = col;
    vtx[1].x = vtx[0].x = left;
    vtx[2].y = vtx[0].y = top;
    vtx[1].y = bottom;
    vtx[2].x = right;
    vtx[3].x = right;
    vtx[3].y = bottom;
    vtx[0].z = vtx[1].z = vtx[2].z = vtx[3].z = z;
    ulGxBeginStrip(1);
    ulGxAddStrip(vtx, 4);
    ulGxEndStrip();
}

/* One colour per corner from col[4]. */
/* 0x001956d0 */
void gw_draw_rect_fill_gradient(GwWin *win, int x0, int y0, int x1, int y1, unsigned *col, float layer)
{
    int ax;
    int ay;
    int layer_idx;
    float left;
    float top;
    float right;
    float bottom;
    float z;
    GwVtx vtx[4];

    gw_window_get_abs_pos(win, &ax, &ay);
    layer_idx = (int)layer;
    left = (float)(ax + x0);
    top = (float)(ay + y0);
    right = (float)(ax + x1 + 1);
    bottom = (float)(ay + y1 + 1);
    z = gw_float_bits_add(win->scale, layer_idx);
    vtx[1].x = vtx[0].x = left;
    vtx[2].y = vtx[0].y = top;
    vtx[1].y = bottom;
    vtx[2].x = right;
    vtx[0].z = vtx[1].z = vtx[2].z = vtx[3].z = z;
    vtx[0].col = col[0];
    vtx[3].x = right;
    vtx[1].col = col[1];
    vtx[2].col = col[2];
    vtx[3].col = col[3];
    vtx[3].y = bottom;
    ulGxBeginStrip(1);
    ulGxAddStrip(vtx, 4);
    ulGxEndStrip();
}
