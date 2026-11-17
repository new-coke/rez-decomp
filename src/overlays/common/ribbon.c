/*
 * Ribbon trails: a ring buffer of recent positions (a point or a posed model's marked nodes, each
 * with a width), aged a frame at a time, drawn as screen-space textured strips fading to the tail.
 */

#include "types_common.h"

#include "types_overlays.h"

#include "debug.h"
#include "anim.h"
#include "libc.h"
#include "math_util.h"
#include "render.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "ribbon.h"
#include "mathdef.h"

static inline void v3_copy(Vec3 *dst, Vec3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

extern int ulMatrixPush(void);
extern void *ulMatrixCur;

#define MAX_GE(a, b) ((a) >= (b) ? (a) : (b))

extern void project_xyz(float x, float y, float z, float *out);

extern NjClip cam_far;

extern Vec3 ribbon_width_axis;
extern UvVtx ribbon_vertices[];

/* 0x00b13a48 stg01, 0x00b1ecd8 stg02, 0x00b17e30 stg03, 0x00b1e0f0 stg04, 0x00b298a0 stg05, 0x00b273b8 stg22 */
void ribbon_init(Ribbon *rib, void *buf, int frames, int seg, int step)
{
    int n;

    rib->head = -1;
    rib->n = 0;
    frames -= frames % (step + 1);
    rib->buf = buf;
    rib->step = step;
    rib->seg = seg;
    rib->len = 0;
    n = frames > 1 ? frames : 2;
    rib->count = n < 201 ? n : 200;
}

/* 0x00b13aa0 stg01, 0x00b1ed30 stg02, 0x00b17e88 stg03, 0x00b1e148 stg04, 0x00b298f8 stg05, 0x00b27410 stg22 */
RibPt *ribbon_collect_nodes(RibPt *p, unsigned int *node, unsigned int mask, int count)
{
    Vec3 v;

    do {
        ulMatrixPush();
        anim_apply_node(node);
        if (node[0] & mask) {
            v = ribbon_width_axis;
            ulMatrixGetTranslation((float *)ulMatrixCur, &p->pos.x);
            ulMatrixApplyVector((float *)ulMatrixCur, &v.x, &v.x);
            p->wid = vec3_length(&v);
            p += count;
        }
        if (node[11])
            p = ribbon_collect_nodes(p, (unsigned int *)node[11], mask, count);
        ulMatrixPop();
        node = (unsigned int *)node[12];
    } while (node);
    return p;
}

/* 0x00b13ba8 stg01, 0x00b1ee38 stg02, 0x00b17f90 stg03, 0x00b1e250 stg04, 0x00b29a00 stg05, 0x00b27518 stg22 */
void ribbon_age(Ribbon *rib)
{
    int n = rib->n;

    if (n > 1) {
        rib->len -= rib->buf[(rib->head - n + 2 + rib->count) % rib->count].seg;
        rib->n = n - 1;
        return;
    }
    rib->n = 0;
    rib->head = -1;
    rib->len = 0;
}

/* 0x00b13c20 stg01, 0x00b1eeb0 stg02, 0x00b18008 stg03, 0x00b1e2c8 stg04, 0x00b29a78 stg05, 0x00b27590 stg22 */
void ribbon_add_model(Ribbon *rib, Vec3 *pos, int *ang, int mask, int root, int clip, float time)
{
    int count = rib->count;
    int n = rib->n;
    int n_nxt = n + 1;
    int head = rib->head;
    int seg = rib->seg;
    RibPt *buf = rib->buf;
    int nhead, num, grp, lo, j;
    RibPt *pt;

    if (count < n_nxt)
        rib->len -= buf[(head - n + 2 + count) % count].seg;
    nhead = (head + 1) % count;
    num = n_nxt < count ? n_nxt : count;
    rib->n = num;
    rib->head = nhead;
    pt = &buf[nhead];
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    ribbon_rotate_yxz(ang);
    seg--;
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    anim_start((void *)clip, time);
    ribbon_collect_nodes(pt, (unsigned int *)root, mask, count);
    ulMatrixPop();
    grp = rib->step + 1;
    if (num < grp)
        grp = num;
    for (; seg != -1; seg--) {
        pt->acc = 0;
        if (num == 1) {
            pt->seg = 0;
            rib->len = 0;
        } else {
            lo = nhead % grp ? nhead - nhead % grp : nhead - grp;
            pt->seg = ulVec3Distance(&pt->pos, &buf[head].pos);
            rib->len += pt->seg;
            for (j = nhead; lo < j; j--)
                pt->acc += buf[(j + count) % count].seg;
        }
        pt += count;
        buf += count;
    }
}

/* 0x00b13ea0 stg01, 0x00b1f130 stg02, 0x00b18288 stg03, 0x00b1e548 stg04, 0x00b29cf8 stg05, 0x00b27810 stg22 */
void ribbon_add_points(Ribbon *rib, Vec3 *pts, int *ang, float wid)
{
    int n = rib->n;
    int count = rib->count;
    int head = rib->head;
    int seg = rib->seg;
    RibPt *buf = rib->buf;
    int n_nxt = n + 1;
    int num, nhead, i, k, grp, lo, j;
    RibPt *p;

    if (count < n_nxt)
        rib->len -= buf[(head - n + 2 + count) % count].seg;
    nhead = (head + 1) % count;
    num = n_nxt < count ? n_nxt : count;
    rib->n = num;
    rib->head = nhead;
    p = &buf[nhead];
    for (i = 0; i < seg; i++) {
        v3_copy(&p->pos, pts++);
        p->wid = wid;
        p += count;
    }
    grp = rib->step + 1;
    if (num < grp)
        grp = num;
    p = &buf[nhead];
    for (seg--; seg != -1; seg--) {
        p->acc = 0;
        if (num == 1) {
            p->seg = 0;
            rib->len = 0;
        } else {
            lo = nhead % grp ? nhead - nhead % grp : nhead - grp;
            p->seg = ulVec3Distance(&p->pos, &buf[head].pos);
            rib->len += p->seg;
            for (j = nhead; lo < j; j--)
                p->acc += buf[(j + count) % count].seg;
        }
        p += count;
        buf += count;
    }
}

/*
 * Edge vertices of p into ribbon_vertices[i]: p offset either way by (dx, dy) * wid (at least 1),
 * clamped to +-1000, texture coordinate ts. Returns i + 2.
 */
/* 0x00b14100 stg01, 0x00b1f390 stg02, 0x00b184e8 stg03, 0x00b1e7a8 stg04, 0x00b29f58 stg05, 0x00b27a70 stg22 */
int ribbon_edge_vertices(int i, int last, float *p, float dx, float dy, float alpha, float ts, float wid)
{
    UvVtx *v = &ribbon_vertices[i];
    float x, z, y;
    float iz, ex, ey, wmin;
    unsigned int col;

    do { x = p[0]; z = p[2]; y = p[1]; } while (0);
    iz = 1.0f / z;
    col = ((int)alpha + 1) >> 1 << 24 | 0x808080;
    wmin = 1.0f;
    wid = MAX_GE(wid, wmin);
    dx *= wid;
    dy *= wid;
    ex = MIN(MAX_GE(x + dx, -1000.0f), 1000.0f);
    ey = MIN(MAX_GE(y + dy, -1000.0f), 1000.0f);
    do {
        v[0].x = ex;
        v[0].y = ey;
        v[0].q = iz;
        v[0].t = 0;
        v[0].s = ts;
        v[0].c = col;
    } while (0);
    v++;
    ex = MIN(MAX_GE(x - dx, -1000.0f), 1000.0f);
    ey = MIN(MAX_GE(y - dy, -1000.0f), 1000.0f);
    do {
        v->x = ex;
        v->y = ey;
        v->q = iz;
        v->t = 1.0f;
        v->s = ts;
        v->c = col;
    } while (0);
    return i + 2;
}

/* 0x00b142d0 stg01, 0x00b1f560 stg02, 0x00b186b8 stg03, 0x00b1e978 stg04, 0x00b2a128 stg05, 0x00b27c40 stg22 */
void ribbon_draw_segment(RibPt *row, int n, int head, int count, int step, int *cur, int *nxt, Vec3 *prev,
                                 Vec3 *v, float *dis_a, float *dis_b, float alpha, float wid, float inv)
{
    Vec3 sa, sb, org_a, org_b;
    float prj_a[4], prj_b[4];
    float zn, k, len_sq, dx, dy, frac, al_b, al_a, ts_b, ts_a, sz_b, sz_a;
    float px, py, pz, qx, qy, qz;
    int vis = 1, cont, lo, nv;
    float *tip;
    RibPt *p;

    ts_a = *dis_a * inv;
    ts_b = *dis_b * inv;
    al_a = (1.0f - ts_a) * alpha;
    al_b = (1.0f - ts_b) * alpha;
    do {
        px = prev->x;
        py = prev->y;
        pz = prev->z;
    } while (0);
    do {
        qx = v->x;
        qy = v->y;
        qz = v->z;
    } while (0);
    zn = -cam_far.near;
    if (zn < pz) {
        frac = (zn - pz) / (qz - pz);
        pz = zn;
        px += (qx - px) * frac;
        py += (qy - py) * frac;
        al_a += (al_b - al_a) * frac;
        ts_a += (ts_b - ts_a) * frac;
    } else if (zn < qz) {
        vis = 0;
        frac = (zn - qz) / (pz - qz);
        qz = zn;
        qx += (px - qx) * frac;
        qy += (py - qy) * frac;
        al_b += (al_a - al_b) * frac;
        ts_b += (ts_a - ts_b) * frac;
    }
    project_xyz(px, py, pz, &sa.x);
    project_xyz(qx, qy, qz, &sb.x);
    v3_copy(&org_a, &sa);
    v3_copy(&org_b, &sb);
    if (yuki_is_offscreen(&sb.x))
        vis = 0;
    frac = yuki_clip_line(&sa.x, &sb.x);
    if (!(fabsf(frac - 1.0f) < FLT_EPSILON)) {
        al_b = al_a + (al_b - al_a) * frac;
        ts_b = ts_a + (ts_b - ts_a) * frac;
    }
    frac = yuki_clip_line(&sb.x, &sa.x);
    if (!(fabsf(frac - 1.0f) < FLT_EPSILON)) {
        al_a = al_b + (al_a - al_b) * frac;
        ts_b = ts_a + (ts_b - ts_a) * frac;
    }
    prj_a[2] = sa.z;
    prj_a[0] = 1.0f;
    prj_a[1] = 0;
    project_point_abs(prj_a, prj_a);
    sz_a = prj_a[0];
    prj_a[0] = 1.0f;
    prj_a[2] = sb.z;
    prj_a[1] = 0;
    project_point_abs(prj_a, prj_a);
    sz_b = prj_a[0];
    dx = sa.y - sb.y;
    dy = sb.x - sa.x;
    frac = dx * dx + dy * dy;
    frac = sqrtf(frac) >= FLT_EPSILON ? 1.0f / sqrtf(frac) * wid : wid * 8388608.0f;
    dx *= frac;
    dy *= frac;
    p = &row[(*cur + count) % count];
    nv = ribbon_edge_vertices(0, 0, &sa.x, dx, dy, al_a, ts_a, p->wid * sz_a);
    p = &row[(*nxt + count) % count];
    if (!vis) {
        nv = ribbon_edge_vertices(nv, 1, &sb.x, dx, dy, al_b, ts_b, p->wid * sz_b);
        ulGxAddTexStrip(ribbon_vertices, nv);
        return;
    }
    nv = ribbon_edge_vertices(nv, 0, &sb.x, dx, dy, al_b, ts_b, p->wid * sz_b);
    tip = &sb.x;
    lo = head - n + 1;
    for (;;) {
        cont = 1;
        *cur = *nxt;
        *nxt -= step;
        if (*nxt < lo) {
            *nxt = lo;
            cont = 0;
        }
        ts_a = ts_b;
        al_a = al_b;
        *dis_a = *dis_b;
        *dis_b += p->acc;
        p = &row[(*nxt + count) % count];
        v3_copy(prev, v);
        ulMatrixApplyPoint((float *)ulMatrixCur, &p->pos.x, &v->x);
        px = qx;
        py = qy;
        pz = qz;
        ts_b = *dis_b * inv;
        al_b = (1.0f - ts_b) * alpha;
        do {
            qx = v->x;
            qy = v->y;
            qz = v->z;
        } while (0);
        zn = -cam_far.near;
        if (zn < qz) {
            cont = 0;
            frac = (zn - qz) / (pz - qz);
            qz = zn;
            qx += (px - qx) * frac;
            qy += (py - qy) * frac;
            al_b += (al_a - al_b) * frac;
            ts_b += (ts_a - ts_b) * frac;
        }
        v3_copy(&sa, &sb);
        project_xyz(qx, qy, qz, &sb.x);
        v3_copy(&org_a, &sa);
        v3_copy(&org_b, &sb);
        if (yuki_is_offscreen(&sb.x))
            cont = 0;
        frac = yuki_clip_line(&sa.x, &sb.x);
        if (!(fabsf(frac - 1.0f) < FLT_EPSILON)) {
            al_b = al_a + (al_b - al_a) * frac;
            ts_b = ts_a + (ts_b - ts_a) * frac;
        }
        prj_b[0] = 1.0f;
        prj_b[1] = 0;
        prj_b[2] = qz;
        project_point_abs(prj_b, prj_b);
        sz_b = prj_b[0];
        dx = sa.y - sb.y;
        dy = sb.x - sa.x;
        frac = dx * dx + dy * dy;
        len_sq = MAX_GE(frac, FLT_EPSILON);
        frac = 1.0f / sqrtf(len_sq) * wid;
        dx *= frac;
        dy *= frac;
        if (cont) {
            nv = ribbon_edge_vertices(nv, 0, tip, dx, dy, al_b, ts_b, p->wid * sz_b);
            continue;
        }
        nv = ribbon_edge_vertices(nv, 1, tip, dx, dy, al_b, ts_b, p->wid * sz_b);
        ulGxAddTexStrip(ribbon_vertices, nv);
        return;
    }
}

/* 0x00b14af8 stg01, 0x00b1fd88 stg02, 0x00b18ee0 stg03, 0x00b1f1a0 stg04, 0x00b2a950 stg05, 0x00b28468 stg22 */
void ribbon_draw_row(RibPt *row, int n, int head, int count, int step, float alpha, float wid, float inv)
{
    Vec3 prev, v, scr;
    int cur, nxt;
    float dis_a, dis_b;
    int vis_a, vis_b;
    RibPt *p = &row[head];

    dis_a = 0;
    ulMatrixApplyPoint((float *)ulMatrixCur, &p->pos.x, &v.x);
    project_point(&v, &scr);
    nxt = head % step ? head - head % step : head - step;
    dis_b = p->acc;
    vis_b = 0;
    cur = head;
    if (v.z < -cam_far.near)
        vis_b = !yuki_is_offscreen(&scr.x);
    while (head - n + 1 < nxt) {
        v3_copy(&prev, &v);
        vis_a = vis_b;
        p = &row[(nxt + count) % count];
        ulMatrixApplyPoint((float *)ulMatrixCur, &p->pos.x, &v.x);
        project_point(&v, &scr);
        vis_b = 0;
        if (v.z < -cam_far.near)
            vis_b = !yuki_is_offscreen(&scr.x);
        if (vis_a || vis_b) {
            ribbon_draw_segment(row, n, head, count, step, &cur, &nxt, &prev, &v, &dis_a, &dis_b, alpha, wid, inv);
            vis_b = 0;
        }
        dis_a = dis_b;
        cur = nxt;
        nxt -= step;
        dis_b += p->acc;
    }
}

/* 0x00b14d60 stg01, 0x00b1fff0 stg02, 0x00b19148 stg03, 0x00b1f408 stg04, 0x00b2abb8 stg05, 0x00b286d0 stg22 */
void ribbon_draw(Ribbon *rib, int tex, float fade, float width)
{
    int n = rib->n;
    int seg = rib->seg;
    RibPt *row = rib->buf;
    int dl;

    if (n >= 2) {
        dl = RenderSetCurrentDisplayList(6);
        seg--;
        ulGxSelectTexture(ulGxGetTextureList(), tex);
        fade *= 255.0f;
        ulGxBeginTexStrip(1);
        for (; seg != -1; seg--) {
            ribbon_draw_row(row, n, rib->head, rib->count, rib->step + 1, fade, width, 1.0f / rib->len);
            row += rib->count;
        }
        ulGxEndTexStrip();
        RenderSetCurrentDisplayList(dl);
    }
}

/* 0x00b14ec0 stg01, 0x00b20150 stg02, 0x00b192a8 stg03, 0x00b1f568 stg04, 0x00b2ad18 stg05, 0x00b28830 stg22 */
void ribbon_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
