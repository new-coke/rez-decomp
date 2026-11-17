/*
 * Screen-space sprites: depth-sized billboards at projected world points, textured and filled
 * screen rectangles. Points far off screen are skipped.
 */

#include "types_game.h"
#include "libc.h"
#include "nj_strip.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "billboard.h"
#include "line_batch.h"
#include "system.h"

typedef struct RectVtx {
    float x;
    float y;
    float z;
    int col;
} RectVtx;

extern float screen_cfg[];

static float screen_project(float *in, float *out);

/* out = {|x|, |y|} * screen scales * -1/z, out[2] = -z. Returns -1/z. */
/* 0x0017dd30 */
static float screen_project_size(float x, float y, float z, float *out)
{
    float scl_x = screen_cfg[0];
    float scl_y = screen_cfg[1];
    float inv_z = -1.0f / z;

    out[2] = -z;
    out[0] = fabsf(x * inv_z * scl_x);
    out[1] = fabsf(y * inv_z * scl_y);
    return inv_z;
}

/* Skipped outside depth (0, 1) or more than 1500 units off screen centre. */
/* 0x0017dd88 */
void billboard_draw(int tex_idx, int tex, int pt, float width, float height)
{
    BillboardVtx vtx[8];
    float pos[3];
    float sz[3];
    float depth;
    float x;
    float y;

#ifndef VERSION_US
    ulGxSelectTexture(ulGxGetTextureList(), tex_idx);
#endif
    ulMatrixApplyPoint(ulMatrixCur, pt, pos);
    screen_project_size(width, height, pos[2], sz);
    depth = screen_project(pos, pos);
    if (depth <= 0.0f || depth >= 1.0f)
        return;
    x = pos[0];
    y = pos[1];
    if (x <= -1500.0f || x >= 1500.0f || y <= -1500.0f || y >= 1500.0f)
        return;
    width = sz[0];
    height = sz[1];
    x -= width * 0.5f;
    y -= height * 0.5f;
    vtx[0].x = x;
    vtx[0].y = y;
    vtx[0].z = depth;
    vtx[0].u = 0.0f;
    vtx[0].v = 0.0f;
    vtx[0].tex = tex;
    vtx[1].x = x + width;
    vtx[1].y = y + height;
    vtx[1].z = depth;
    vtx[1].u = 1.0f;
    vtx[1].v = 1.0f;
    vtx[1].tex = tex;
#ifdef VERSION_US
    ulGxSelectTexture(ulGxGetTextureList(), tex_idx);
#endif
    ulGxBeginSprite(1);
    ulGxAddSprites(vtx, 2);
    ulGxEndSprite();
}

/* uv = {u0, v0, u1, v1}. */
/* 0x0017df20 */
void billboard_draw_uv(int tex_idx, int tex, int pt, float *uv, float width, float height)
{
    BillboardVtx vtx[8];
    float pos[3];
    float sz[3];
    float depth;
    float x;
    float y;

#ifndef VERSION_US
    ulGxSelectTexture(ulGxGetTextureList(), tex_idx);
#endif
    ulMatrixApplyPoint(ulMatrixCur, pt, pos);
    screen_project_size(width, height, pos[2], sz);
    depth = screen_project(pos, pos);
    if (depth <= 0.0f || depth >= 1.0f)
        return;
    x = pos[0];
    y = pos[1];
    if (x <= -1500.0f || x >= 1500.0f || y <= -1500.0f || y >= 1500.0f)
        return;
    width = sz[0];
    height = sz[1];
    x -= width * 0.5f;
    y -= height * 0.5f;
    vtx[0].x = x;
    vtx[0].y = y;
    vtx[0].z = depth;
    vtx[0].u = uv[0];
    vtx[0].v = uv[1];
    vtx[0].tex = tex;
    vtx[1].x = x + width;
    vtx[1].y = y + height;
    vtx[1].z = depth;
    vtx[1].u = uv[2];
    vtx[1].v = uv[3];
    vtx[1].tex = tex;
#ifdef VERSION_US
    ulGxSelectTexture(ulGxGetTextureList(), tex_idx);
#endif
    ulGxBeginSprite(1);
    ulGxAddSprites(vtx, 2);
    ulGxEndSprite();
}

/* (x, y) top-left. Skipped 1500 or more units off screen centre. */
/* 0x0017e0d0 */
void tex_rect_draw(int tex_idx, int tex, float x, float y, float z, float width, float height)
{
    BillboardVtx vtx[8];

#ifdef VERSION_US
    if (x <= -1500.0f || x >= 1500.0f || y <= -1500.0f || y >= 1500.0f)
        return;
#else
    ulGxSelectTexture(ulGxGetTextureList(), tex_idx);
#endif
    vtx[0].x = x;
    vtx[0].y = y;
    vtx[0].z = z;
    vtx[0].u = 0.0f;
    vtx[0].v = 0.0f;
    vtx[0].tex = tex;
    vtx[1].x = x + width;
    vtx[1].y = y + height;
    vtx[1].z = z;
    vtx[1].u = 1.0f;
    vtx[1].v = 1.0f;
    vtx[1].tex = tex;
#ifdef VERSION_US
    ulGxSelectTexture(ulGxGetTextureList(), tex_idx);
#endif
    ulGxBeginSprite(1);
    ulGxAddSprites(vtx, 2);
    ulGxEndSprite();
}

/* uv = {u0, v0, u1, v1}. */
/* 0x0017e1b0 */
void tex_rect_draw_uv(int tex_idx, int tex, float *uv, float x, float y, float z, float width, float height)
{
    BillboardVtx vtx[8];

#ifdef VERSION_US
    if (x <= -1500.0f || x >= 1500.0f || y <= -1500.0f || y >= 1500.0f)
        return;
#else
    ulGxSelectTexture(ulGxGetTextureList(), tex_idx);
#endif
    vtx[0].x = x;
    vtx[0].y = y;
    vtx[0].z = z;
    vtx[0].u = uv[0];
    vtx[0].v = uv[1];
    vtx[0].tex = tex;
    vtx[1].x = x + width;
    vtx[1].y = y + height;
    vtx[1].z = z;
    vtx[1].u = uv[2];
    vtx[1].v = uv[3];
    vtx[1].tex = tex;
#ifdef VERSION_US
    ulGxSelectTexture(ulGxGetTextureList(), tex_idx);
#endif
    ulGxBeginSprite(1);
    ulGxAddSprites(vtx, 2);
    ulGxEndSprite();
}

/* 0x0017e298 */
void billboard_draw_rot(int tex_idx, int tex, int ang, int pt, float width, float height)
{
    float pos[3];
    float sz[3];
    BillboardVtx vtx[4];
    float s;
    float c;
    float depth;
    float zero;
    float one;
    float dx;
    float dy;
    float px;
    float py;

    zero = 0.0f;
#ifndef VERSION_US
    ulGxSelectTexture(ulGxGetTextureList(), tex_idx);
#endif
    ulMatrixApplyPoint(ulMatrixCur, pt, pos);
    screen_project_size(width, height, pos[2], sz);
    depth = screen_project(pos, pos);
    if (depth <= zero)
        return;
    one = 1.0f;
    if (depth >= one)
        return;
    px = pos[0];
    py = pos[1];
    if (px <= -1500.0f || px >= 1500.0f || py <= -1500.0f || py >= 1500.0f)
        return;
    width = sz[0];
    height = sz[1];
    width *= 0.5f;
    height *= 0.5f;
    ulSinCos(ang, &s, &c);
    dy = width * c - height * s;
    dx = width * s + height * c;
    vtx[0].x = px - dx;
    vtx[0].y = py - dy;
    vtx[0].z = depth;
    vtx[0].u = zero;
    vtx[0].v = zero;
    vtx[0].tex = tex;
    vtx[1].x = px + dy;
    vtx[1].y = py - dx;
    vtx[1].z = depth;
    vtx[1].u = one;
    vtx[1].v = zero;
    vtx[1].tex = tex;
    vtx[2].x = px - dy;
    vtx[2].y = py + dx;
    vtx[2].z = depth;
    vtx[2].u = zero;
    vtx[2].v = one;
    vtx[2].tex = tex;
    vtx[3].x = px + dx;
    vtx[3].y = py + dy;
    vtx[3].z = depth;
    vtx[3].u = one;
    vtx[3].v = one;
    vtx[3].tex = tex;
#ifdef VERSION_US
    ulGxSelectTexture(ulGxGetTextureList(), tex_idx);
#endif
    ulGxBeginTexStrip(1);
    ulGxAddTexStrip(vtx, 4);
    ulGxEndTexStrip();
}

/* (cx, cy) centre. Skipped 1500 or more units off screen centre. */
/* 0x0017e4c8 */
void tex_rect_draw_rot(int tex_idx, int tex, int ang, float cx, float cy, float z, float width, float height)
{
    BillboardVtx vtx[4];
    float s;
    float c;
    float dx;
    float dy;

#ifdef VERSION_US
    if (cx <= -1500.0f || cx >= 1500.0f || cy <= -1500.0f || cy >= 1500.0f)
        return;
#else
    ulGxSelectTexture(ulGxGetTextureList(), tex_idx);
#endif
    width *= 0.5f;
    height *= 0.5f;
    ulSinCos(ang, &s, &c);
    dy = width * c - height * s;
    dx = width * s + height * c;
    vtx[0].x = cx - dx;
    vtx[0].y = cy - dy;
    vtx[0].z = z;
    vtx[0].u = 0.0f;
    vtx[0].v = 0.0f;
    vtx[0].tex = tex;
    vtx[1].x = cx + dy;
    vtx[1].y = cy - dx;
    vtx[1].z = z;
    vtx[1].u = 1.0f;
    vtx[1].v = 0.0f;
    vtx[1].tex = tex;
    vtx[2].x = cx - dy;
    vtx[2].y = cy + dx;
    vtx[2].z = z;
    vtx[2].u = 0.0f;
    vtx[2].v = 1.0f;
    vtx[2].tex = tex;
    vtx[3].x = cx + dx;
    vtx[3].y = cy + dy;
    vtx[3].z = z;
    vtx[3].u = 1.0f;
    vtx[3].v = 1.0f;
    vtx[3].tex = tex;
#ifdef VERSION_US
    ulGxSelectTexture(ulGxGetTextureList(), tex_idx);
#endif
    ulGxBeginTexStrip(1);
    ulGxAddTexStrip(vtx, 4);
    ulGxEndTexStrip();
}

/* Pivot (px, py) in rectangle space lands on (x, y). Skipped 1500+ units off screen centre. */
/* 0x0017e678 */
#ifdef VERSION_US
/* 0x0017e678 */
void tex_rect_draw_rot_pivot(int tex_idx, int tex, float *uv, int ang, float x, float y, float z, float width,
                             float height, float px, float py)
#else
void tex_rect_draw_rot_pivot(float x, float y, float z, float width, float height, float px, float py, int tex_idx, int tex,
                   float *uv, int ang)
#endif
{
    BillboardVtx vtx[4];
    float s;
    float c;

#ifdef VERSION_US
    if (x <= -1500.0f || 1500.0f <= x || y <= -1500.0f || 1500.0f <= y) {
        return;
    }
#else
    ulGxSelectTexture(ulGxGetTextureList(), tex_idx);
#endif
    vtx[0].tex = vtx[1].tex = vtx[2].tex = vtx[3].tex = tex;
    vtx[0].z = vtx[1].z = vtx[2].z = vtx[3].z = z;
    vtx[0].u = uv[0];
    vtx[0].v = uv[1];
    vtx[1].v = uv[3];
    vtx[2].u = uv[2];
    vtx[1].u = uv[0];
    vtx[2].v = uv[1];
    vtx[3].u = uv[2];
    vtx[3].v = uv[3];
    ulSinCos(ang, &s, &c);
    {
        float dx, dy, rx, ry;

        dx = 0.0f - px;
        dy = height - py;
        rx = dx * c - dy * s;
        ry = dx * s + dy * c;
        vtx[0].x = x + rx;
        vtx[0].y = y - ry;
        dy = 0.0f - py;
        rx = dx * c - dy * s;
        ry = dx * s + dy * c;
        vtx[1].x = x + rx;
        vtx[1].y = y - ry;
        dx = width - px;
        dy = height - py;
        rx = dx * c - dy * s;
        ry = dx * s + dy * c;
        vtx[2].x = x + rx;
        vtx[2].y = y - ry;
        dy = 0.0f - py;
        rx = dx * c - dy * s;
        ry = dx * s + dy * c;
        vtx[3].x = x + rx;
        vtx[3].y = y - ry;
    }
#ifdef VERSION_US
    ulGxSelectTexture(ulGxGetTextureList(), tex_idx);
#endif
    ulGxBeginTexStrip(1);
    ulGxAddTexStrip(vtx, 4);
    ulGxEndTexStrip();
}

/* Adds n << 16 to the bit pattern; lifts a depth slightly. */
/* 0x0017e878 */
static float rect_float_add_bits16(float x, int n)
{
    *(int *)&x = *(int *)&x + (n << 16);
    return x;
}

/* Outline at a nudged depth to stay on top; top edge starts one unit left to close the corner. */
/* 0x0017e898 */
void rect_draw_outline(float x, float y, float z, float width, float height, int line_col, int col)
{
    RectVtx vtx[4];
    float right = x + width;
    float bottom = y + height;
    void *batch;

    vtx[0].z = vtx[1].z = vtx[2].z = vtx[3].z = z;
    vtx[0].x = x;
    vtx[0].y = y;
    vtx[1].x = x;
    vtx[1].y = bottom;
    vtx[2].x = right;
    vtx[2].y = y;
    vtx[3].x = right;
    vtx[3].y = bottom;
    vtx[0].col = vtx[1].col = vtx[2].col = vtx[3].col = col;
    nj_color_strip_begin(1);
    nj_color_strip_vertices(vtx, 4);
    nj_color_strip_end();
    z = rect_float_add_bits16(z, 1);
    batch = (void *)line_set_color(line_col);
    batch = line_add_seg(batch, x - 1.0f, y, z, right, y, z);
    batch = line_add_seg(batch, x, y, z, x, bottom, z);
    batch = line_add_seg(batch, right, bottom, z, right, y, z);
    batch = line_add_seg(batch, right, bottom, z, x, bottom, z);
    line_flush(batch);
}

/* out[2] = -z. Returns -1/z. */
/* 0x0017e9f8 */
static float screen_project(float *in, float *out)
{
    float inv_z = -1.0f / in[2];
    float sx = in[0] * inv_z * screen_cfg[0] + screen_cfg[2];
    float sy = in[1] * inv_z * screen_cfg[1] + screen_cfg[3];

    out[0] = sx;
    out[1] = sy;
    out[2] = -in[2];
    return inv_z;
}
