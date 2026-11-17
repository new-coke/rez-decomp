/* Debug drawing: axis markers, screen crosses, HUD bar gauges. */

#include "types_game.h"
#include "libc.h"
#include "nj_strip.h"
#include "debugdraw.h"
#include "float_bits.h"
#include "lockon.h"
#include "node_draw_state.h"

extern float debug_draw_box_verts[];

/* Three lines of length 2 * half along X, Y, Z through the current origin. */
/* 0x00111168 */
void debug_draw_box(int n, float half)
{
    float *p = debug_draw_box_verts;

    p[0] = half;
    p[3] = -half;
    p[7] = half;
    p[14] = half;
    p[10] = -half;
    p[17] = -half;
    draw_set_blend(4, 5);
    lockon_draw_line_3d(p, p + 3, n, n);
    lockon_draw_line_3d(p + 6, p + 9, n, n);
    lockon_draw_line_3d(p + 12, p + 15, n, n);
}

/* 0x001111f8 */
void debug_draw_cross(int dev, float x, float y, float z, float half)
{
    lockon_draw_line_2d(dev, dev, 0.2f, x, y - half, x, y + half);
    lockon_draw_line_2d(dev, dev, 0.2f, x - half, y, x + half, y);
}

/* Only prints the function name. */
/* 0x00111288 */
static void DebugDrawArrow(void)
{
    printf("debugdraw.c : DebugDrawArrow()\n");
}

/* 0x00111298 */
static void debug_draw_arrow_to(float *from, float *to, int color)
{
    float dir[3];

    dir[0] = to[0] - from[0];
    dir[1] = to[1] - from[1];
    dir[2] = to[2] - from[2];
    ((void (*)(float *, float *, int))DebugDrawArrow)(from, dir, color); /* steer(cast) */
}

/* Bar inset one pixel, filled to rate (0..1), one depth step in front. */
/* 0x001112f0 */
void debug_draw_gauge(float z, float x, float y, float width, float height, float rate, int bar_col, int bg_col)
{
    DVtx vtx[4];
    float inner_z;
    float left;
    float top;
    float bottom;
    float right;

    nj_color_strip_begin(0);
    vtx[0].z = vtx[1].z = vtx[2].z = vtx[3].z = z;
    vtx[3].color = bg_col;
    vtx[2].color = bg_col;
    vtx[0].color = vtx[1].color = bg_col;
    vtx[1].x = x;
    vtx[0].x = x;
    vtx[2].y = y;
    vtx[3].x = x + width;
    vtx[0].y = y;
    vtx[3].y = y + height;
    vtx[1].y = y + height;
    vtx[2].x = x + width;
    nj_color_strip_vertices(vtx, 4);
    inner_z = float_add_bits16(1, z);
    vtx[0].z = vtx[1].z = vtx[2].z = vtx[3].z = inner_z;
    vtx[1].color = vtx[2].color = vtx[3].color = bar_col;
    top = vtx[2].y + 1.0f;
    vtx[0].y = vtx[2].y = top;
    bottom = vtx[3].y - 1.0f;
    width -= 2.0f;
    vtx[0].color = bar_col;
    vtx[1].y = vtx[3].y = bottom;
    left = vtx[1].x + 1.0f;
    vtx[0].x = vtx[1].x = left;
    right = left + width * rate;
    vtx[3].x = right;
    vtx[2].x = right;
    nj_color_strip_vertices(vtx, 4);
    nj_color_strip_end();
}

/* Second bar in add_col from the end of the first, add_rate of the width. */
/* 0x00111458 */
void debug_draw_gauge_dual(float z, float x, float y, float width, float height, float rate, float add_rate, int bar_col, int add_col,
                 int bg_col)
{
    DVtx vtx[4];
    float inner_z;
    float left;
    float top;
    float bottom;
    float right;

    nj_color_strip_begin(0);
    vtx[0].z = vtx[1].z = vtx[2].z = vtx[3].z = z;
    vtx[3].color = bg_col;
    vtx[2].color = bg_col;
    vtx[0].color = vtx[1].color = bg_col;
    vtx[1].x = x;
    vtx[0].x = x;
    vtx[2].y = y;
    vtx[3].x = x + width;
    vtx[0].y = y;
    vtx[3].y = y + height;
    vtx[1].y = y + height;
    vtx[2].x = x + width;
    nj_color_strip_vertices(vtx, 4);
    inner_z = float_add_bits16(1, z);
    vtx[0].z = vtx[1].z = vtx[2].z = vtx[3].z = inner_z;
    vtx[1].color = vtx[2].color = vtx[3].color = bar_col;
    top = vtx[2].y + 1.0f;
    vtx[0].y = vtx[2].y = top;
    bottom = vtx[3].y - 1.0f;
    width -= 2.0f;
    vtx[0].color = bar_col;
    vtx[1].y = vtx[3].y = bottom;
    left = vtx[1].x + 1.0f;
    vtx[0].x = vtx[1].x = left;
    right = left + width * rate;
    vtx[3].x = right;
    vtx[2].x = right;
    nj_color_strip_vertices(vtx, 4);
    vtx[0].color = vtx[1].color = vtx[2].color = vtx[3].color = add_col;
    vtx[0].x = vtx[1].x = vtx[2].x + width * add_rate;
    nj_color_strip_vertices(vtx, 4);
    nj_color_strip_end();
}
