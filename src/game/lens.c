/*
 * Screen-space lines and glows: clipped poly-line ribbons with round caps, the textured C-Lens
 * strip, lens-flare ghosts from a light toward the screen centre, camera-facing beam glows.
 */

#include "types_game.h"
#include "libc.h"
#include "gw_varedit.h"
#include "nj_draw.h"
#include "nj_strip.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_texture.h"
#include "lens.h"
#include "camera.h"
#include "color.h"
#include "debug.h"
#include "math_util.h"
#include "node_draw_state.h"
#include "task.h"
#include "types_common.h"
#include "types_gw.h"
#include "mathdef.h"
#include "system.h"

#define WIDTH_MIN(v, lo) do { float t_ = (v); if (t_ < (lo)) t_ = (lo); (v) = t_; } while (0)

#define WIDTH_MAX(v, hi) do { float t_ = (v); if ((hi) < t_) t_ = (hi); (v) = t_; } while (0)

typedef struct { int col_center; int col_inner; int col_outer; char pad_0c[4]; float width; float sc; } PolylineWideStyle;
typedef struct PolylineStyle { int col_center; int col_edge; float width; } PolylineStyle;

typedef struct LensPoint {
    float v[3];
} LensPoint;

typedef struct LensFlareGhost {
    unsigned int col;
    int cnt;
    int tex;
    float pos;
    float size;
    float fade_size;
    float fade_alpha;
} LensFlareGhost;

extern float polyline_width_min;
extern void project_xyz_abs(float, float, float, float *);
extern Vtx c_lens_cap_quad[4];
extern Vtx c_lens_strip_quad[4];
extern int c_lens_n;
extern char lens_flare_ghosts[];
extern int c_lens_num;
extern int c_lens_tex;
extern float c_lens_offset;
extern float c_lens_add_r;
extern float c_lens_add_rr;
extern float c_lens_add_rc;
extern Vtx c_lens_quad[4];
extern int lens_flare_vars_group;
extern char gw_varedit_root[];
extern GwVarDef lens_flare_vardef[];
extern GwVarDef lens_flare_vardefs[];
extern struct TaskDesc c_lens_desc;
extern float c_lens_shine_r[2];
extern float screen_cfg[];
extern float screen_layout[];
extern int lens_flare_tex;

extern void draw_polyline(PolylineStyle *st, Vec3 *pts, int n, unsigned int cap);
extern void draw_polyline_wide(PolylineWideStyle *st, Vec3 *pts, int n, unsigned int cap);

/* On-screen half-width. Returns the old value. */
/* 0x00124d50 */
float polyline_set_width_min(float v)
{
    float old = polyline_width_min;

    polyline_width_min = v;
    return old;
}

/* 0x00124d60 */
float polyline_get_width_min(void)
{
    return polyline_width_min;
}

/* Onto z = -cam_near along the segment. */
/* 0x00124d70 */
void clip_to_znear(float *pt, float *other)
{
    float z = -cam_near;
    float t = (z - pt[2]) / (other[2] - pt[2]);

    pt[2] = z;
    pt[0] = pt[0] + (other[0] - pt[0]) * t;
    pt[1] = pt[1] + (other[1] - pt[1]) * t;
}

/* Near plane, project, screen clip to +-1000, perspective-correct depth. -1 gone, 1 near-cut, 0. */
/* 0x00124dd0 */
int clip_line_2d(float *va, float *vb)
{
    float z = -cam_near;
    int r = 0;
    float ia;
    float ib;
    float t;
    float ax;
    float ay;
    float az;
    float bx;
    float by;
    float bz;

    if (z < va[2] && z < vb[2])
        return -1;
    if (z < va[2]) {
        clip_to_znear(va, vb);
        r = 1;
    } else if (z < vb[2]) {
        clip_to_znear(vb, va);
        r = 1;
    }
    project_point(va, va);
    project_point(vb, vb);
    ax = va[0];
    ay = va[1];
    az = va[2];
    bx = vb[0];
    by = vb[1];
    bz = vb[2];
    if (ax <= -1000.0f) {
        if (bx <= -1000.0f)
            return -1;
        ia = 1.0f / az;
        ib = 1.0f / bz;
        t = (-1000.0f - ax) / (bx - ax);
        az = 1.0f / (ia + t * (ib - ia));
        ay = ay + t * (by - ay);
        ax = -1000.0f;
    } else if (bx <= -1000.0f) {
        ib = 1.0f / bz;
        ia = 1.0f / az;
        t = (-1000.0f - bx) / (ax - bx);
        by = by + t * (ay - by);
        bz = 1.0f / (ib + t * (ia - ib));
        bx = -1000.0f;
    }
    if (1000.0f <= ax) {
        if (1000.0f <= bx)
            return -1;
        ia = 1.0f / az;
        ib = 1.0f / bz;
        t = (1000.0f - ax) / (bx - ax);
        az = 1.0f / (ia + t * (ib - ia));
        ay = ay + t * (by - ay);
        ax = 1000.0f;
    } else if (1000.0f <= bx) {
        ib = 1.0f / bz;
        ia = 1.0f / az;
        t = (1000.0f - bx) / (ax - bx);
        by = by + t * (ay - by);
        bz = 1.0f / (ib + t * (ia - ib));
        bx = 1000.0f;
    }
    if (ay <= -1000.0f) {
        if (by <= -1000.0f)
            return -1;
        ia = 1.0f / az;
        ib = 1.0f / bz;
        t = (-1000.0f - ay) / (by - ay);
        az = 1.0f / (ia + t * (ib - ia));
        ax = ax + t * (bx - ax);
        ay = -1000.0f;
    } else if (by <= -1000.0f) {
        ib = 1.0f / bz;
        ia = 1.0f / az;
        t = (-1000.0f - by) / (ay - by);
        bx = bx + t * (ax - bx);
        bz = 1.0f / (ib + t * (ia - ib));
        by = -1000.0f;
    }
    if (1000.0f <= ay) {
        if (1000.0f <= by)
            return -1;
        ia = 1.0f / az;
        ib = 1.0f / bz;
        t = (1000.0f - ay) / (by - ay);
        az = 1.0f / (ia + t * (ib - ia));
        ax = ax + t * (bx - ax);
        ay = 1000.0f;
    } else if (1000.0f <= by) {
        ib = 1.0f / bz;
        ia = 1.0f / az;
        t = (1000.0f - by) / (ay - by);
        bx = bx + t * (ax - bx);
        bz = 1.0f / (ib + t * (ia - ib));
        by = 1000.0f;
    }
    va[0] = ax;
    va[1] = ay;
    va[2] = az;
    vb[0] = bx;
    vb[1] = by;
    vb[2] = bz;
    return r;
}

/* 0x00125270 */
int transform_and_clip_line(float *va, float *vb)
{
    ulMatrixApplyPoint(ulMatrixCur, va, va);
    ulMatrixApplyPoint(ulMatrixCur, vb, vb);
    return clip_line_2d(va, vb);
}

/* Half ring outer -> inner colour, half disc inner -> centre. Skipped well off screen. */
/* 0x001252d8 */
void draw_line_marker(int center_col, int inner_col, int outer_col, float x, float y, float z, float ofs_x, float ofs_y, float inner_scale)
{
    LineVtx vtx[26];
    float s, c;
    LineVtx *p;
    float r1k, r2k;
    int i;
    int ang, next_ang;
    float *ps;
    int ofs;

    if (x <= -704.0f)
        return;
    if (1344.0f <= x)
        return;
    if (y <= -784.0f)
        return;
    if (1264.0f <= y)
        return;
    r1k = ofs_x * inner_scale;
    r2k = ofs_y * inner_scale;
    for (i = 14, p = vtx; i != 0; i--, p++)
        p->z = z;
    p = vtx;
    ang = 0;
    for (i = 7; i != 0; i--) {
        ulSinCos(ang, &s, &c);
        ang += 0x1555;
        p->x = x + ofs_x * c - ofs_y * s;
        p->y = y - ofs_x * s - ofs_y * c;
        p->col = outer_col;
        p++;
        p->x = x + r1k * c - r2k * s;
        p->y = y - r1k * s - r2k * c;
        p->col = inner_col;
        p++;
    }
    ps = &s;
    nj_color_strip_vertices(vtx, 14);
    do { /* steer(barrier) */
        ofs = ((int)ps & 1) >> 1;
    } while (0);
    ps = (float *)((char *)ps + ofs);
    ulSinCos(0, &s, &c);
    ang = 0;
    i = 2;
    next_ang = 0x1555;
    do {
        p = vtx;
        p->col = inner_col;
        p->x = x + r1k * c - r2k * s;
        p->y = y - r1k * s - r2k * c;
        p++;
        ulSinCos(next_ang, ps, &c);
        next_ang += 0x3fff;
        p->col = inner_col;
        p->x = x + r1k * c - r2k * s;
        p->y = y - r1k * s - r2k * c;
        p++;
        p->col = center_col;
        p->x = x;
        p->y = y;
        p++;
        ulSinCos(ang + 0x2aaa, ps, &c);
        ang += 0x3fff;
        p->col = inner_col;
        p->x = x + r1k * c - r2k * s;
        p->y = y - r1k * s - r2k * c;
        p++;
        ulSinCos(ang, ps, &c);
        p->col = inner_col;
        p->x = x + r1k * c - r2k * s;
        p->y = y - r1k * s - r2k * c;
        nj_color_strip_vertices(vtx, 5);
    } while (--i != 0);
}

/* Half disc edge -> centre colour. */
/* 0x00125638 */
void draw_star(int center_col, int edge_col, float x, float y, float z, float ofs_x, float ofs_y)
{
    LineVtx vtx[5];
    float sn;
    float cs;
    LineVtx *p;
    int n;
    int next_ang, ang;

    p = vtx;
    for (n = 5; n != 0; n--, p++)
        p->z = z;
    ulSinCos(0, &sn, &cs);
    ang = 0;
    n = 2;
    next_ang = 0x1555;
    for (; n != 0; n--) {
        p = &vtx[0];
        p->col = edge_col;
        p->x = x + ofs_x * cs - ofs_y * sn;
        p->y = y - ofs_x * sn - ofs_y * cs;
        p = &vtx[1];
        ulSinCos(next_ang, &sn, &cs);
        next_ang += 0x3fff;
        p->col = edge_col;
        p->x = x + ofs_x * cs - ofs_y * sn;
        p->y = y - ofs_x * sn - ofs_y * cs;
        p = &vtx[2];
        p->x = x;
        p->col = center_col;
        p->y = y;
        p = &vtx[3];
        ulSinCos(ang + 0x2aaa, &sn, &cs);
        ang += 0x3fff;
        p->col = edge_col;
        p->x = x + ofs_x * cs - ofs_y * sn;
        p->y = y - ofs_x * sn - ofs_y * cs;
        ulSinCos(ang, &sn, &cs);
        p = &vtx[4];
        p->col = edge_col;
        p->x = x + ofs_x * cs - ofs_y * sn;
        p->y = y - ofs_x * sn - ofs_y * cs;
        nj_color_strip_vertices(vtx, 5);
    }
}

/* Five bands: outer, inner (st->sc of the width), centre. Half-width polyline_width_min..300. */
/* 0x00125880 */
void draw_polyline_wide(PolylineWideStyle *st, Vec3 *pts, int n, unsigned int cap)
{
    LineVtx vtx[10];
    Vec3 start;
    Vec3 end;
    float dir[4];
    float t[4];
    unsigned int on;
    float lx, ly, lz, lw, lh;
    float za, ex, ey, wd, sc;
    float iw, ih;

    if (n <= 0)
        return;
    draw_set_blend(4, 1);
    nj_color_strip_begin(1);
    on = 0;
    ly = lz = lx = lh = lw = 0.0f;
    vtx[0].col = st->col_outer;
    vtx[1].col = st->col_outer;
    vtx[2].col = st->col_inner;
    vtx[3].col = st->col_inner;
    vtx[4].col = st->col_center;
    vtx[5].col = st->col_center;
    vtx[6].col = st->col_inner;
    vtx[7].col = st->col_inner;
    vtx[8].col = st->col_outer;
    vtx[9].col = st->col_outer;
    do {
        start = *pts++;
        end = *pts;
        if (transform_and_clip_line(&start, &end) >= 0) {
            za = 1.0f / start.z;
            lz = 1.0f / end.z;
            dir[0] = end.x - start.x;
            dir[2] = end.y - start.y;
            vtx[0].z = vtx[2].z = vtx[4].z = vtx[6].z = vtx[8].z = za;
            vtx[1].z = vtx[3].z = vtx[5].z = vtx[7].z = vtx[9].z = lz;
            vec2_normalize_xz(dir, dir);
            project_xyz_abs(1.0f, 1.0f, start.z, t);
            wd = t[0] * st->width;
            WIDTH_MIN(wd, polyline_width_min);
            WIDTH_MAX(wd, 300.0f);
            ex = dir[2] * wd;
            ey = dir[0] * wd;
            project_xyz_abs(1.0f, 1.0f, end.z, t);
            wd = t[0] * st->width;
            WIDTH_MIN(wd, polyline_width_min);
            WIDTH_MAX(wd, 300.0f);
            lw = dir[2] * wd;
            lh = dir[0] * wd;
            if (cap != 0)
                draw_line_marker(st->col_center, st->col_inner, st->col_outer, start.x, start.y, za, ex, ey, st->sc);
            lx = end.x;
            ly = end.y;
            sc = st->sc;
            vtx[0].x = start.x + ex;
            vtx[0].y = start.y - ey;
            vtx[1].x = lx + lw;
            vtx[1].y = ly - lh;
            vtx[4].x = start.x;
            vtx[4].y = start.y;
            vtx[5].x = lx;
            vtx[5].y = ly;
            vtx[8].x = start.x - ex;
            vtx[8].y = start.y + ey;
            vtx[9].x = lx - lw;
            vtx[9].y = ly + lh;
            ex *= sc;
            ey *= sc;
            vtx[2].x = start.x + ex;
            vtx[2].y = start.y - ey;
            iw = lw * sc;
            ih = lh * sc;
            vtx[3].x = lx + iw;
            vtx[3].y = ly - ih;
            vtx[6].x = start.x - ex;
            vtx[6].y = start.y + ey;
            vtx[7].x = lx - iw;
            vtx[7].y = ly + ih;
            nj_color_strip_vertices(vtx, 10);
            on = 1;
            break;
        }
    } while (--n > 0);
    while (--n > 0) {
        vtx[0] = vtx[1];
        vtx[2] = vtx[3];
        vtx[4] = vtx[5];
        vtx[6] = vtx[7];
        vtx[8] = vtx[9];
        start = *pts++;
        end = *pts;
        if (transform_and_clip_line(&start, &end) < 0) {
            if (on & cap) {
                draw_line_marker(st->col_center, st->col_inner, st->col_outer, vtx[4].x, vtx[4].y, lz, -lw, -lh, st->sc);
                on = 0;
            }
        } else {
            float delta[4];
            float unit[4];
            float proj_b[4];

            lz = 1.0f / end.z;
            delta[0] = end.x - start.x;
            delta[2] = end.y - start.y;
            vtx[1].z = vtx[3].z = vtx[5].z = vtx[7].z = vtx[9].z = lz;
            vec2_normalize_xz(delta, unit);
            project_xyz_abs(1.0f, 1.0f, end.z, proj_b);
            wd = proj_b[0] * st->width;
            WIDTH_MIN(wd, polyline_width_min);
            WIDTH_MAX(wd, 300.0f);
            lw = unit[2] * wd;
            lh = unit[0] * wd;
            lx = end.x;
            ly = end.y;
            vtx[1].x = lx + lw;
            vtx[1].y = ly - lh;
            iw = lw * st->sc;
            ih = lh * st->sc;
            vtx[3].x = lx + iw;
            vtx[3].y = ly - ih;
            vtx[5].x = lx;
            vtx[5].y = ly;
            vtx[7].x = lx - iw;
            vtx[7].y = ly + ih;
            vtx[9].x = lx - lw;
            vtx[9].y = ly + lh;
            nj_color_strip_vertices(vtx, 10);
            on = 1;
        }
    }
    if (on & cap)
        draw_line_marker(st->col_center, st->col_inner, st->col_outer, lx, ly, lz, -lw, -lh, st->sc);
    nj_color_strip_end();
}

/* pts[0..n]; edge to centre colour; perspective width >= polyline_width_min. cap: draw_star. */
/* 0x00125ee0 */
void draw_polyline(PolylineStyle *st, Vec3 *pts, int n, unsigned int cap)
{
    LineVtx vtx[6];
    Vec3 start;
    Vec3 end;
    float dir[4];
    float t[4];
    unsigned int on;
    float lx, ly, lz, lw, lh;
    float za, ex, ey, wd;

    if (n <= 0)
        return;
    draw_set_blend(4, 1);
    nj_color_strip_begin(1);
    on = 0;
    ly = lz = lx = lh = lw = 0.0f;
    vtx[0].col = st->col_edge;
    vtx[1].col = st->col_edge;
    vtx[2].col = st->col_center;
    vtx[3].col = st->col_center;
    vtx[4].col = st->col_edge;
    vtx[5].col = st->col_edge;
    do {
        start = *pts++;
        end = *pts;
        if (transform_and_clip_line(&start, &end) >= 0) {
            za = 1.0f / start.z;
            lz = 1.0f / end.z;
            dir[0] = end.x - start.x;
            dir[2] = end.y - start.y;
            vtx[0].z = vtx[2].z = vtx[4].z = za;
            vtx[1].z = vtx[3].z = vtx[5].z = lz;
            vec2_normalize_xz(dir, dir);
            project_xyz_abs(1.0f, 1.0f, start.z, t);
            wd = t[0] * st->width;
            do { float t_ = wd; if (t_ < polyline_width_min) t_ = polyline_width_min; wd = t_; } while (0);
            ex = dir[2] * wd;
            ey = dir[0] * wd;
            project_xyz_abs(1.0f, 1.0f, end.z, t);
            wd = t[0] * st->width;
            do { float t_ = wd; if (t_ < polyline_width_min) t_ = polyline_width_min; wd = t_; } while (0);
            lw = dir[2] * wd;
            lh = dir[0] * wd;
            if (cap != 0)
                draw_star(st->col_center, st->col_edge, start.x, start.y, za, ex, ey);
            lx = end.x;
            ly = end.y;
            vtx[0].x = start.x + ex;
            vtx[0].y = start.y - ey;
            vtx[1].x = lx + lw;
            vtx[1].y = ly - lh;
            vtx[2].x = start.x;
            vtx[2].y = start.y;
            vtx[3].x = lx;
            vtx[3].y = ly;
            vtx[4].x = start.x - ex;
            vtx[4].y = start.y + ey;
            vtx[5].x = lx - lw;
            vtx[5].y = ly + lh;
            nj_color_strip_vertices(vtx, 6);
            on = 1;
            break;
        }
    } while (--n > 0);
    while (--n > 0) {
        vtx[0] = vtx[1];
        vtx[2] = vtx[3];
        vtx[4] = vtx[5];
        start = *pts++;
        end = *pts;
        if (transform_and_clip_line(&start, &end) < 0) {
            if (on & cap) {
                draw_star(st->col_center, st->col_edge, vtx[2].x, vtx[2].y, lz, -lw, -lh);
                on = 0;
            }
        } else {
            float delta[4];
            float unit[4];
            float proj_b[4];

            lz = 1.0f / end.z;
            delta[0] = end.x - start.x;
            delta[2] = end.y - start.y;
            vtx[1].z = vtx[3].z = vtx[5].z = lz;
            vec2_normalize_xz(delta, unit);
            project_xyz_abs(1.0f, 1.0f, end.z, proj_b);
            wd = proj_b[0] * st->width;
            do { float t_ = wd; if (t_ < polyline_width_min) t_ = polyline_width_min; wd = t_; } while (0);
            lw = unit[2] * wd;
            lh = unit[0] * wd;
            lx = end.x;
            ly = end.y;
            vtx[1].x = lx + lw;
            vtx[1].y = ly - lh;
            vtx[3].x = lx;
            vtx[3].y = ly;
            vtx[5].x = lx - lw;
            vtx[5].y = ly + lh;
            nj_color_strip_vertices(vtx, 6);
            on = 1;
        }
    }
    if (on & cap)
        draw_star(st->col_center, st->col_edge, lx, ly, lz, -lw, -lh);
    nj_color_strip_end();
}

/* 0x001263d8 */
void draw_polyline_wide_capped(int st, Vec3 *pts, int n)
{
    draw_polyline_wide(st, pts, n, 1);
}

/* 0x001263e0 */
void draw_polyline_wide_nocap(int st, Vec3 *pts, int n)
{
    draw_polyline_wide(st, pts, n, 0);
}

/* 0x001263e8 */
void draw_polyline_capped(int st, int pts, int n)
{
    draw_polyline(st, pts, n, 1);
}

/* 0x001263f0 */
void draw_polyline_nocap(int st, int pts, int n)
{
    draw_polyline(st, pts, n, 0);
}

/* Quad at (x, y) spanned by the half-width (dx, dy), reaching past the end. */
/* 0x001263f8 */
void c_lens_draw_cap(int col, float x, float y, float z, float dx, float dy)
{
    Vtx *p;
    int i;

    p = c_lens_cap_quad;
    for (i = 4; i != 0; i--) {
        p->col = col;
        p->z = z;
        p++;
    }
    p = c_lens_cap_quad;
    p[0].x = x - dy + dx;
    p[0].y = y - dx - dy;
    p[1].x = x + dx;
    p[1].y = y - dy;
    p[2].x = x - dy - dx;
    p[2].y = y - dx + dy;
    p[3].x = x - dx;
    p[3].y = y + dy;
    nj_tex_strip_draw(p, 4, 1);
}

/*
 * Colour from tbl->col_center, minus tbl->col_edge per segment; once alpha would go negative the
 * segment is transparent and the strip stops one later. Caps use tex_id + 1.
 */
/* 0x00126488 */
void c_lens_draw_strip(int tex_id, PolylineStyle *tbl, LensPoint *pts, int n)
{
    LensPoint start;
    LensPoint end;
    float dir[4];
    float w[4];
    Vtx *vtx = c_lens_strip_quad;
    int fade, dfade;
    unsigned int col, start_col;
    unsigned int on;
    float inv_start, inv_end, wd, ax, ay, bx, by, end_x, end_y;

    if (n <= 0)
        return;
    draw_set_blend(4, 1);
    end_y = end_x = inv_end = bx = by = 0.0f;
    on = 0;
    fade = color_get_a(tbl->col_center);
    dfade = color_get_a(tbl->col_edge);
    col = tbl->col_center;
    do {
        start = *pts++;
        end = *pts;
        if (transform_and_clip_line(&start, &end) >= 0) {
            start_col = col;
            inv_start = 1.0f / start.v[2];
            inv_end = 1.0f / end.v[2];
            fade -= dfade;
            col -= tbl->col_edge;
            if (fade < 0)
                col &= 0xffffff;
            dir[0] = end.v[0] - start.v[0];
            dir[2] = end.v[1] - start.v[1];
            vtx[0].col = start_col;
            vtx[2].col = start_col;
            vtx[1].col = col;
            vtx[3].col = col;
            vtx[0].z = vtx[2].z = inv_start;
            vtx[1].z = vtx[3].z = inv_end;
            vec2_normalize_xz(dir, dir);
            project_xyz_abs(1.0f, 1.0f, start.v[2], w);
            wd = w[0] * tbl->width;
            do { float t_ = wd; if (t_ < polyline_width_min) t_ = polyline_width_min; wd = t_; } while (0);
            ax = dir[2] * wd;
            ay = dir[0] * wd;
            project_xyz_abs(1.0f, 1.0f, end.v[2], w);
            wd = w[0] * tbl->width;
            do { float t_ = wd; if (t_ < polyline_width_min) t_ = polyline_width_min; wd = t_; } while (0);
            bx = dir[2] * wd;
            by = dir[0] * wd;
            on = 1;
            ulGxSetTexture(ulTextureFindById(tex_id + 1));
            c_lens_draw_cap(start_col, start.v[0], start.v[1], inv_start, ax, ay);
            end_x = end.v[0];
            end_y = end.v[1];
            vtx[0].x = start.v[0] + ax;
            vtx[0].y = start.v[1] - ay;
            vtx[1].x = end_x + bx;
            vtx[1].y = end_y - by;
            vtx[2].x = start.v[0] - ax;
            vtx[2].y = start.v[1] + ay;
            vtx[3].x = end_x - bx;
            vtx[3].y = end_y + by;
            ulGxSetTexture(ulTextureFindById(tex_id));
            nj_tex_strip_draw(vtx, 4, 1);
            break;
        }
    } while (--n > 0);
    while (--n > 0) {
        float delta[4];
        float unit[4];
        float proj_b[4];

        if (fade < 0)
            return;
        fade -= dfade;
        col -= tbl->col_edge;
        if (fade < 0)
            col &= 0xffffff;
        vtx[0] = vtx[1];
        vtx[2] = vtx[3];
        vtx[1].col = col;
        vtx[3].col = col;
        start = *pts++;
        end = *pts;
        if (transform_and_clip_line(&start, &end) < 0)
            break;
        inv_end = 1.0f / end.v[2];
        delta[0] = end.v[0] - start.v[0];
        delta[2] = end.v[1] - start.v[1];
        vtx[1].z = vtx[3].z = inv_end;
        vec2_normalize_xz(delta, unit);
        project_xyz_abs(1.0f, 1.0f, end.v[2], proj_b);
        wd = proj_b[0] * tbl->width;
        do { float t_ = wd; if (t_ < polyline_width_min) t_ = polyline_width_min; wd = t_; } while (0);
        bx = unit[2] * wd;
        by = unit[0] * wd;
        on = 1;
        end_x = end.v[0];
        end_y = end.v[1];
        vtx[1].x = end_x + bx;
        vtx[1].y = end_y - by;
        vtx[3].x = end_x - bx;
        vtx[3].y = end_y + by;
        nj_tex_strip_draw(vtx, 4, 1);
    }
    if (on) {
        ulGxSetTexture(ulTextureFindById(tex_id + 1));
        c_lens_draw_cap(col, end_x, end_y, inv_end, -bx, -by);
    }
}

/* Ghost c_lens_n -> c_lens_num .. c_lens_add_rc. */
/* 0x001269c0 */
void c_lens_params_load(void)
{
    char *ent = lens_flare_ghosts + c_lens_n * 28;

    c_lens_num = ((int *)(lens_flare_ghosts + c_lens_n * 28))[1];
    c_lens_tex = ((int *)(lens_flare_ghosts + c_lens_n * 28))[2];
    c_lens_offset = ((float *)(lens_flare_ghosts + c_lens_n * 28))[3];
    c_lens_add_r = ((float *)(lens_flare_ghosts + c_lens_n * 28))[4];
    c_lens_add_rr = ((float *)(lens_flare_ghosts + c_lens_n * 28))[5];
    c_lens_add_rc = ((float *)(lens_flare_ghosts + c_lens_n * 28))[6];
}

/* c_lens_num .. c_lens_add_rc -> ghost c_lens_n. */
/* 0x00126a40 */
void c_lens_params_store(void)
{
    char *ent = lens_flare_ghosts + c_lens_n * 28;

    ((int *)(lens_flare_ghosts + c_lens_n * 28))[1] = c_lens_num;
    ((int *)(lens_flare_ghosts + c_lens_n * 28))[2] = c_lens_tex;
    ((float *)(lens_flare_ghosts + c_lens_n * 28))[3] = c_lens_offset;
    ((float *)(lens_flare_ghosts + c_lens_n * 28))[4] = c_lens_add_r;
    ((float *)(lens_flare_ghosts + c_lens_n * 28))[5] = c_lens_add_rr;
    ((float *)(lens_flare_ghosts + c_lens_n * 28))[6] = c_lens_add_rc;
}

/* c_lens_quad: full screen, white, depth 0.95, texture repeated twice. */
/* 0x00126ab8 */
void c_lens_init(int *w)
{
    Vtx *vtx = c_lens_quad;
    int group;
    float zf = 0.95f;
    float zero = 0.0f;
    float two = 2.0f;

    vtx[3].z = zf;
    w[2] = 0;
    w[3] = 0;
    vtx[2].z = zf;
    vtx[1].z = zf;
    vtx[3].col = RGBA(0xff, 0xff, 0xff, 0xff);
    vtx[2].col = RGBA(0xff, 0xff, 0xff, 0xff);
    vtx[1].col = RGBA(0xff, 0xff, 0xff, 0xff);
    vtx[1].u = zero;
    vtx[0].u = zero;
    vtx[3].u = two;
    vtx[2].u = two;
    vtx[0].z = zf;
    vtx[0].col = RGBA(0xff, 0xff, 0xff, 0xff);
    vtx[2].v = zero;
    vtx[0].v = zero;
    vtx[3].v = two;
    vtx[1].v = two;
    c_lens_params_load();
    group = gw_varedit_add(gw_varedit_root, 0, lens_flare_vardef);
    lens_flare_vars_group = group;
    gw_varedit_add_table(gw_varedit_root, group, lens_flare_vardefs);
}

/* Started after the first frame. */
/* 0x00126b80 */
void c_lens_exec(int *obj)
{
    if (obj[2] == 0)
        obj[2] = 1;
}

/* 0x00126b98 */
void c_lens_release(void)
{
    gw_varedit_remove(gw_varedit_root, 0, lens_flare_vardef);
}

/* 0x00126bb0 */
void *c_lens_create(void *parent)
{
    return task_create_inherit(parent, &c_lens_desc, 16);
}

/*
 * sel & 1 picks one of two four-ghost tables. With cam, fades as the light turns away, gone facing
 * away. Ghost sprites step from the light to the screen centre, fading near the screen edge.
 */
/* 0x00126bd8 */
void lens_flare_draw(float *pos, float *cam, float radius, float alpha, unsigned int sel)
{
    float p[4];
    float axis[4];
    float delta[4];
    Vtx vtx[6];
    LensFlareGhost *ghost;
    unsigned int col;
    float t;
    float fc, ratio, scale, al;
    float x, y, dx, dy, size, sz, fade;
    int group, i;

    ghost = (*(LensFlareGhost (*)[2][4])&lens_flare_ghosts)[sel & 1]; /* steer(cast) */
    if (view_sphere_visible_local(pos, radius) == 0)
        return;
    col = ghost->col;
    fc = (float)color_get_a(col);
    if (cam != 0) {
        camera_get_view_vec(axis);
        ulMatrixApplyVector(ulMatrixCur, cam, delta);
        sz = ulVec3Dot(axis, delta);
        if (0.0f <= sz)
            return;
        fc = fc * -sz;
    }
    project_point(pos, p);
    delta[0] = p[0] - screen_cfg[2];
    delta[1] = p[1] - screen_cfg[3];
    ratio = (delta[0] * delta[0] + delta[1] * delta[1]) / (screen_layout[2] * screen_layout[3]);
    if (fabsf(delta[0]) < FLT_EPSILON && fabsf(delta[1]) < FLT_EPSILON)
        delta[0] = 1.0f;
    scale = radius / sqrtf(delta[0] * delta[0] + delta[1] * delta[1]);
    delta[2] = p[2];
    delta[0] = delta[0] * scale;
    delta[1] = delta[1] * scale;
    project_point_abs(delta, axis);
    if (delta[0] < 0.0f)
        axis[0] = -axis[0];
    if (delta[1] < 0.0f)
        axis[1] = -axis[1];
    scale = radius * c_lens_shine_r[0] / sqrtf((axis[0] + axis[1]) * (axis[0] + axis[1]) + (axis[1] - axis[0]) * (axis[1] - axis[0]));
    axis[0] = axis[0] * scale;
    axis[1] = axis[1] * scale;
    draw_set_override_flags(0);
    draw_set_blend(4, 1);
    ulGxSetTexFilter(1);
    ulGxSetTexClamp(1, 1);
    vtx[0].z = vtx[1].z = vtx[2].z = vtx[3].z = vtx[4].z = vtx[5].z = 1.0526316f;
    vtx[0].u = 0.0f;
    vtx[0].v = 0.0f;
    vtx[1].u = 0.0f;
    vtx[1].v = 1.0f;
    vtx[2].u = 1.0f;
    vtx[2].v = 0.0f;
    vtx[3].u = 1.0f;
    vtx[3].v = 1.0f;
    vtx[4].u = 0.0f;
    vtx[4].v = 0.0f;
    vtx[5].u = 0.0f;
    vtx[5].v = 1.0f;
    for (group = 0; group < 4; group++, ghost++) {
        size = ghost->size * (1.0f - ghost->fade_size * ratio);
        dx = (screen_cfg[2] - p[0]) * ghost->pos;
        dy = (screen_cfg[3] - p[1]) * ghost->pos;
        sz = size;
        x = p[0] + dx;
        y = p[1] + dy;
        if (size <= 0.0f)
            continue;
        fade = 1.0f - ghost->fade_alpha * ratio;
        if (fade <= 0.0f)
            continue;
        al = fc * c_lens_shine_r[1] * fade * alpha;
        t = (al < 255.0f) ? al : 255.0f;
        col = (col & 0xffffff) | ((((int)t + 1) >> 1) << 24);
        ulGxSetTextureList(lens_flare_tex);
        for (i = 0; i < ghost->cnt; i++) {
            ulGxSelectTexture(ulGxGetTextureList(), ghost->tex);
            ulGxBeginTexStrip(1);
            vtx[0].x = x - (axis[0] + axis[1]) * sz;
            vtx[0].y = y + (axis[0] - axis[1]) * sz;
            vtx[1].x = x - axis[0] * sz;
            vtx[1].y = y - axis[1] * sz;
            vtx[2].x = x - axis[1] * sz;
            vtx[2].y = y + axis[0] * sz;
            vtx[3].x = x;
            vtx[3].y = y;
            vtx[0].col = vtx[1].col = vtx[2].col = vtx[3].col = vtx[4].col = vtx[5].col = col;
            vtx[4].x = x + (axis[0] - axis[1]) * sz;
            vtx[4].y = y + (axis[0] + axis[1]) * sz;
            vtx[5].x = x + axis[0] * sz;
            vtx[5].y = y + axis[1] * sz;
            ulGxAddTexStrip(vtx, 6);
            vtx[0].x = x - (axis[0] - axis[1]) * sz;
            vtx[0].y = y - (axis[0] + axis[1]) * sz;
            vtx[2].x = x + axis[1] * sz;
            vtx[2].y = y - axis[0] * sz;
            vtx[4].x = x + (axis[0] + axis[1]) * sz;
            vtx[4].y = y - (axis[0] - axis[1]) * sz;
            sz += size;
            x += dx;
            y += dy;
            ulGxAddTexStrip(vtx, 6);
            ulGxEndTexStrip();
        }
    }
    ulGxSetTexClamp(0, 0);
}

/* At the current origin; dir_in optional, turned by the matrix. */
/* 0x001271b8 */
void lens_flare_draw_mtx(int dir_in, int sel, float size, float alpha)
{
    float origin[4];
    float dir[4];

    ulMatrixGetTranslation(ulMatrixCur, origin);
    if (dir_in) {
        ulMatrixApplyVector(ulMatrixCur, dir_in, dir);
        lens_flare_draw(origin, dir, size, alpha, sel);
    } else {
        lens_flare_draw(origin, 0, size, alpha, sel);
    }
}

/* dir_in optional. */
/* 0x00127268 */
void lens_flare_draw_point(float size, float alpha, int point, int dir_in, int sel)
{
    float pos[4];
    float dir[4];

    ulMatrixApplyPoint(ulMatrixCur, point, pos);
    if (dir_in) {
        ulMatrixApplyVector(ulMatrixCur, dir_in, dir);
        lens_flare_draw(pos, dir, size, alpha, sel);
    } else {
        lens_flare_draw(pos, 0, size, alpha, sel);
    }
}

/* Oriented from screen centre, half_w before pos. dir: fades by dir z, skipped facing away. */
/* 0x00127320 */
void light_beam_draw(unsigned int col, float *pos, float *dir, float half_w)
{
    float axis[4];
    float delta[4];
    float p[4];
    Vtx vtx[6];
    float scale;
    float z;

    if (view_sphere_visible_local(pos, half_w) == 0)
        return;
    if (dir != 0) {
        if (dir[2] < 0.0f)
            return;
        col = (col & 0xffffff) | ((int)((float)color_get_a(col) * dir[2]) + 1 >> 1 << 24);
    }
    project_point(pos, p);
    delta[0] = p[0] - screen_cfg[2];
    delta[1] = p[1] - screen_cfg[3];
    if (fabsf(delta[0]) < FLT_EPSILON && fabsf(delta[1]) < FLT_EPSILON)
        delta[0] = 1.0f;
    scale = half_w / sqrtf(delta[0] * delta[0] + delta[1] * delta[1]);
    delta[2] = p[2];
    delta[0] *= scale;
    delta[1] *= scale;
    project_point_abs(delta, axis);
    if (delta[0] < 0.0f)
        axis[0] = -axis[0];
    if (delta[1] < 0.0f)
        axis[1] = -axis[1];
    ulGxSetTexFilter(1);
    ulGxSetTexClamp(1, 1);
    z = 1.0f / (p[2] - half_w);
    vtx[0].z = vtx[1].z = vtx[2].z = vtx[3].z = vtx[4].z = vtx[5].z = z;
    vtx[0].col = vtx[1].col = vtx[2].col = vtx[3].col = vtx[4].col = vtx[5].col = col;
    vtx[0].u = 0.0f;
    vtx[0].v = 0.0f;
    vtx[1].u = 0.0f;
    vtx[1].v = 1.0f;
    vtx[2].u = 1.0f;
    vtx[2].v = 0.0f;
    vtx[3].u = 1.0f;
    vtx[3].v = 1.0f;
    vtx[4].u = 0.0f;
    vtx[4].v = 0.0f;
    vtx[5].u = 0.0f;
    vtx[5].v = 1.0f;
    ulGxBeginTexStrip(1);
    vtx[3].x = p[0];
    vtx[3].y = p[1];
    vtx[1].x = p[0] - axis[0];
    vtx[4].y = p[1] + axis[0] + axis[1];
    vtx[0].y = p[1] + axis[0] - axis[1];
    vtx[1].y = p[1] - axis[1];
    vtx[2].x = p[0] - axis[1];
    vtx[2].y = p[1] + axis[0];
    vtx[5].y = p[1] + axis[1];
    vtx[4].x = p[0] + axis[0] - axis[1];
    vtx[0].x = p[0] - axis[0] - axis[1];
    vtx[5].x = p[0] + axis[0];
    ulGxAddTexStrip(vtx, 6);
    vtx[0].x = p[0] - axis[0] + axis[1];
    vtx[0].y = p[1] - axis[0] - axis[1];
    vtx[2].y = p[1] - axis[0];
    vtx[2].x = p[0] + axis[1];
    vtx[4].y = p[1] - axis[0] + axis[1];
    vtx[4].x = p[0] + axis[0] + axis[1];
    ulGxAddTexStrip(vtx, 6);
    ulGxEndTexStrip();
    ulGxSetTexClamp(0, 0);
}

/* At the current origin; dir_in optional, turned by the matrix. */
/* 0x00127668 */
void light_beam_draw_mtx(int col, int dir_in, float half_w)
{
    float origin[4];
    float dir[4];

    ulMatrixGetTranslation(ulMatrixCur, origin);
    if (dir_in) {
        ulMatrixApplyVector(ulMatrixCur, dir_in, dir);
        light_beam_draw(col, origin, dir, half_w);
    } else {
        light_beam_draw(col, origin, 0, half_w);
    }
}

/* 0x00127700 */
void light_beam_draw_point(int col, int point, int dir_in, float half_w)
{
    float pos[4];
    float dir[4];

    ulMatrixApplyPoint(ulMatrixCur, point, pos);
    if (dir_in) {
        ulMatrixApplyVector(ulMatrixCur, dir_in, dir);
        light_beam_draw(col, pos, dir, half_w);
    } else {
        light_beam_draw(col, pos, 0, half_w);
    }
}
