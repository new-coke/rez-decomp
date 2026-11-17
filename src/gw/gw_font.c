/*
 * Debug GUI font: 'GINDOWS FONT' texture, three sizes (12 x 16, 6 x 11, 5 x 8) and icons, as
 * sprites.
 */

#include "types_gw.h"
#include <stdarg.h>
#include "libc.h"
#include "gw_font.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"
#include "ul_texture.h"
#include "color.h"

typedef struct GwPt {
    float x;
    float y;
} GwPt;

typedef struct GwIconUv {
    unsigned uv0;       /* 0 top-left u (high half), v (low half) */
    unsigned pad;       /* 4 */
    unsigned uv1;       /* 8 bottom-right */
} GwIconUv;

extern int gw_font_image[];
extern int gw_font_tex;
extern int gw_font_cell_w;
extern float gw_font_scale;
extern float gw_font_depth;
extern unsigned gw_font_color[1];
extern unsigned gw_font_cur;
extern int gw_font_large_utab[];
extern int gw_font_large_vtab[];
extern int gw_font_middle_utab[];
extern int gw_font_middle_vtab[];
extern int gw_font_small_utab[];
extern int gw_font_small_vtab[];
extern GwPt gw_font_icon_offsets[];
extern const GwIconUv gw_font_icon_uvs[];

/* From the embedded image; scale reset, neutral grey. */
/* 0x00195818 */
void gw_font_init(int font)
{
    ulGxLock(1);
    gw_font_tex = ulTextureLoadBlockDirect(gw_font_image);
    ulGxUnlock(1);
    ulTextureSetName(gw_font_tex, "GINDOWS FONT");
    gw_font_set(font);
    gw_font_set_scale(1.0f);
    gw_font_set_color(ARGB(0x80, 0x80, 0x80, 0x80));
}

/* Nearest float with the low 12 mantissa bits clear. */
/* 0x00195888 */
static float gw_float_round_bits(float x)
{
    *(int *)&x = (*(int *)&x + 0x7ff) & 0xfffff000;
    return x;
}

/* 0: 12 x 16, 1: 6 x 11, 2: 5 x 8; other fonts leave the outputs untouched. */
/* 0x001958b0 */
void gw_font_get_cell_size(unsigned font, int *cw, int *ch)
{
    switch (font) {
    case 0:
        *cw = 12;
        *ch = 16;
        break;
    case 1:
        *cw = 6;
        *ch = 11;
        break;
    case 2:
        *cw = 5;
        *ch = 8;
        break;
    }
}

/* Returns the previous font. */
/* 0x001958f8 */
int gw_font_set(unsigned font)
{
    int old;

    old = gw_font_cur;
    gw_font_cur = font;
    switch (font) {
    case 0:
        gw_font_cell_w = 12;
        gw_font_cell_h = 16;
        break;
    case 1:
        gw_font_cell_w = 6;
        gw_font_cell_h = 11;
        break;
    case 2:
        gw_font_cell_w = 5;
        gw_font_cell_h = 8;
        break;
    }
    return old;
}

/* Depth becomes its reciprocal; returns the previous scale. */
/* 0x00195958 */
float gw_font_set_scale(float scale)
{
    float old;

    old = gw_font_scale;
    gw_font_scale = scale;
    gw_font_depth = gw_float_round_bits(1.0f / scale);
    return old;
}

/* Scale becomes its reciprocal; returns the previous depth. */
/* 0x001959a0 */
float gw_font_set_depth(float z)
{
    float old;

    old = gw_font_depth;
    gw_font_depth = z;
    gw_font_scale = 1.0f / z;
    return old;
}

/* Returns the previous one. */
/* 0x001959c8 */
unsigned gw_font_set_color(unsigned col)
{
    unsigned old;

    old = (*(unsigned *)&gw_font_color); /* steer(cast) */
    (*(unsigned *)&gw_font_color) = col; /* steer(cast) */
    return old;
}

/*
 * Top-left (x, y), bottom-right dx, dy on; UVs from the column/row tables (16 per row from space).
 */
/* 0x001959d8 */
static GwGlyphVtx *gw_font_glyph_vertices(GwGlyphVtx *out, int code, int *utab, int *vtab, float x, float y, float dx, float dy)
{
    unsigned short us;

    us = code - 32;
    out->col = gw_font_color[0];
    out->x = x;
    out->y = y;
    out->z = gw_font_depth;
    out->u = utab[(us & 15) * 2] + 0x8000;
    out->v = (vtab[us >> 4] << 16) + 0x8000;
    out++;
    out->x = x + dx;
    out->y = y + dy;
    out->col = gw_font_color[0];
    out->z = gw_font_depth;
    out->u = utab[(us & 15) * 2 + 1] + 0x8000;
    out->v = (vtab[(us >> 4) + 1] << 16) + 0x8000;
    return out + 1;
}

/* 0x00195a78 */
void gw_font_putc(float x, float y, unsigned short code)
{
    GwGlyphVtx vtx[2];
    int *utab;
    int *vtab;
    float dx;
    float dy;

    switch (gw_font_cur) {
    case 0:
        utab = gw_font_large_utab;
        vtab = gw_font_large_vtab;
        dx = 12.0f;
        dy = 16.0f;
        break;
    case 1:
        utab = gw_font_middle_utab;
        vtab = gw_font_middle_vtab;
        dx = 6.0f;
        dy = 11.0f;
        break;
    case 2:
        utab = gw_font_small_utab;
        vtab = gw_font_small_vtab;
        dx = 5.0f;
        dy = 8.0f;
        break;
    }
    gw_font_glyph_vertices(vtx, code, utab, vtab, x, y, dx, dy);
    ulGxSetTexture(gw_font_tex);
    ulGxBeginSprite(1);
    ulGxAddSprites(vtx, 2);
    ulGxEndSprite();
}

/* Unclipped, one batch; newline returns to x. Batch holds 64 characters, unchecked. */
/* 0x00195b48 */
static void gw_font_text(char *s, float x, float y)
{
    GwGlyphVtx buf[128];
    GwGlyphVtx *out;
    int *utab;
    int *vtab;
    float dx;
    float dy;
    float start_x;
    int n;
    int c;

    switch (gw_font_cur) {
    case 0:
        utab = gw_font_large_utab;
        vtab = gw_font_large_vtab;
        dx = 12.0f;
        dy = 16.0f;
        break;
    case 1:
        utab = gw_font_middle_utab;
        vtab = gw_font_middle_vtab;
        dx = 6.0f;
        dy = 11.0f;
        break;
    case 2:
        utab = gw_font_small_utab;
        vtab = gw_font_small_vtab;
        dx = 5.0f;
        dy = 8.0f;
        break;
    }
    n = 0;
    start_x = x;
    out = buf;
    while ((c = *s++) != 0) {
        if (c == 10) {
            x = start_x;
            y += dy;
        } else {
            out = gw_font_glyph_vertices(out, c & 0xffff, utab, vtab, x, y, dx, dy);
            x += dx;
            n++;
        }
    }
    if (n > 0) {
        ulGxBeginSpriteGif(1);
        ulGxAddSpritesGif(buf, n * 2);
        ulGxEndSpriteGif();
    }
}

/* Stops at the first character crossing clip_x and row crossing clip_y. 64 characters at most. */
/* 0x00195cc8 */
void gw_font_text_clip(char *s, float x, float y, float clip_x, float clip_y)
{
    int *vtab;
    float bottom;
    float dx;
    int n;
    int c;
    GwGlyphVtx *out;
    GwGlyphVtx buf[128];
    float start_x;
    float bottom_copy;
    float nx;
    float dy;
    int *utab;

    switch (gw_font_cur) {
    case 0:
        utab = gw_font_large_utab;
        vtab = gw_font_large_vtab;
        dx = 12.0f;
        dy = 16.0f;
        break;
    case 1:
        utab = gw_font_middle_utab;
        vtab = gw_font_middle_vtab;
        dx = 6.0f;
        dy = 11.0f;
        break;
    case 2:
        utab = gw_font_small_utab;
        vtab = gw_font_small_vtab;
        dx = 5.0f;
        dy = 8.0f;
        break;
    }
    bottom = y + dy;
    out = buf;
    n = 0;
    bottom_copy = bottom;
    start_x = x;
    if (bottom_copy <= clip_y && (c = *s++) != 0) {
        do {
            if (c == 10) {
                x = start_x;
                bottom = bottom_copy;
                y = bottom;
                bottom = y + dy;
            } else {
                nx = x + dx;
                if (clip_x < nx)
                    break;
                out = gw_font_glyph_vertices(out, c & 0xffff, utab, vtab, x, y, dx, dy);
                n++;
                x = nx;
            }
            bottom_copy = bottom;
            if (!(bottom_copy <= clip_y))
                break;
        } while ((c = *s++) != 0);
    }
    if (n > 0) {
        ulGxSetTexture(gw_font_tex);
        ulGxBeginSpriteGif(1);
        ulGxAddSpritesGif(buf, n * 2);
        ulGxEndSpriteGif();
    }
}

/* Up to 256 bytes. */
/* 0x00195ea8 */
static void gw_font_printf(char *fmt, float x, float y, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, y);
    vsprintf(buf, fmt, ap);
    gw_font_text(buf, x, y);
}

/* Up to 256 bytes. */
/* 0x00195f10 */
static void gw_font_printf_clip(char *fmt, float x, float y, float clip_x, float clip_y, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, clip_y);
    vsprintf(buf, fmt, ap);
    gw_font_text_clip(buf, x, y, clip_x, clip_y);
}

/* Normal size, origin from gw_font_icon_offsets. */
/* 0x00195f98 */
void gw_font_icon(int idx, float x, float y)
{
    x -= gw_font_icon_offsets[idx].x;
    y -= gw_font_icon_offsets[idx].y;
    gw_font_icon_scaled(x, y, 1.0f, idx);
}

/* Top-left at (x, y): 16 pixels (icons 0..7) or 8, times scale. */
/* 0x00195fc8 */
void gw_font_icon_scaled(float x, float y, float scale, int idx)
{
    GwGlyphVtx vert[2];
    GwGlyphVtx *vtx;
    float z;
    float size;
    unsigned col;

    z = gw_font_depth;
    size = 16.0f;
    if (idx >= 8)
        size = 8.0f;
    size = size * scale;
    col = (*(unsigned *)&gw_font_color); /* steer(cast) */
    vtx = vert;
    vtx->x = x;
    vtx->y = y;
    vtx->z = z;
    vtx->col = col;
    vtx->u = (gw_font_icon_uvs[idx].uv0 & 0xffff0000) + 0x8000;
    vtx->v = (gw_font_icon_uvs[idx].uv0 << 16) + 0x8000;
    vtx++;
    vtx->x = x + size;
    vtx->y = y + size;
    vtx->z = z;
    vtx->col = col;
    vtx->u = (gw_font_icon_uvs[idx].uv1 & 0xffff0000) + 0x8000;
    vtx->v = (gw_font_icon_uvs[idx].uv1 << 16) + 0x8000;
    ulGxSetTexture(gw_font_tex);
    ulGxBeginSprite(1);
    ulGxAddSprites(vert, 2);
    ulGxEndSprite();
}
