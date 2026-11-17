/*
 * Area 1 light trails along the path model's node chains: one a second, starting ahead of the
 * player, drawn as additive bands; from map scene 6 the second pair of chains.
 */

#include "stg01.h"
#include "mathdef.h"
#include "anim.h"
#include "camera.h"
#include "fog.h"
#include "libc.h"
#include "math_util.h"
#include "node_draw_state.h"
#include "player.h"
#include "task.h"
#include "ul_gx_prim.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "line.h"
#include "stage_base.h"

/* 28 bytes */
struct TrailPt {
    Vec3 pos;
    float r, g, b;              /* 0x0c */
    float dist;                 /* 0x18: along the trail */
};

struct Trail {
    int npts;
    float len;                  /* 0x04 */
    TrailPt *pts;               /* 0x08 */
};

/* 244 bytes: the drawn part, growing behind its head */
struct LineWork {
    char pad_00[8];
    Trail *trail;               /* 0x08 */
    Vec3 off;                   /* 0x0c: the player's scroll since the start */
    float dist[50];             /* 0x18: ring of head distances */
    int head;                   /* 0xe0 */
    int tail;                   /* 0xe4 */
    int count;                  /* 0xe8 */
    TrailPt *buf;               /* 0xec */
    int nbuf;                   /* 0xf0 */
};

/* 68 bytes: four trails, one re-posed and followed at a time */
struct LineMngWork {
    char *models;
    char pad_04[4];
    Trail tr[4];                /* 0x08 */
    int cur;                    /* 0x38 */
    int timer;                  /* 0x3c */
    float t;                    /* 0x40: the motion's frame */
};

/* offsets into the path models; 0 for none */
typedef struct TrailSrc {
    int model;
    int motion;
} TrailSrc;

#define TR_VTX(v, x, y, q, c) \
    do { \
        float *v_ = (float *)(v); \
        *v_++ = (x); \
        *v_++ = (y); \
        *v_ = (q); \
        *(unsigned int *)(v_ + 1) = (c); \
    } while (0)

extern TrailSrc line_sources[4];
extern struct TaskDesc line_mng_desc;
extern struct TaskDesc line_desc;

/* 0x00af8020 */
void line_draw_band(float *p0, float *p1, float alpha0, float r0, float g0, float b0, float alpha1, float r1, float g1, float b1)
{
    TrailVtx v[4];
    TrailVtx *vp = v;
    float frac, x0, y0, iz0, x1, y1, dx, dy, iz1, z0, z1;
    unsigned int col0, col1;

    frac = yuki_clip_line(p0, p1);
    if (yuki_is_offscreen(p1))
        return;
    if (!(fabsf(frac - 1.0f) < FLT_EPSILON)) {
        alpha1 = alpha0 + (alpha1 - alpha0) * frac;
        r1 = r0 + (r1 - r0) * frac;
        g1 = g0 + (g1 - g0) * frac;
        b1 = b0 + (b1 - b0) * frac;
    }
    frac = yuki_clip_line(p1, p0);
    if (!(fabsf(frac - 1.0f) < FLT_EPSILON)) {
        alpha0 = alpha1 + (alpha0 - alpha1) * frac;
        r0 = r1 + (r0 - r1) * frac;
        g0 = g1 + (g0 - g1) * frac;
        b0 = b1 + (b0 - b1) * frac;
    }
    z0 = p0[2];
    do { x0 = p0[0]; y0 = p0[1]; } while (0);
    do { x1 = p1[0]; } while (0);
    z1 = p1[2];
    do { y1 = p1[1]; } while (0);
    iz0 = 1.0f / z0;
    dx = y0 - y1;
    dy = x1 - x0;
    iz1 = 1.0f / z1;
    if (sqrtf(dx * dx + dy * dy) <= FLT_EPSILON)
        frac = 4194304.0f;
    else
        frac = 1.0f / sqrtf(dx * dx + dy * dy) * 0.5f;
    dx *= frac;
    dy *= frac;
    col0 = ((int)(alpha0 * 255.0f) + 1) >> 1 << 24 | ((int)(b0 * 255.0f) & 0xff) << 16 | ((int)(g0 * 255.0f) & 0xff) << 8 | ((int)(r0 * 255.0f) & 0xff);
    col1 = ((int)(alpha1 * 255.0f) + 1) >> 1 << 24 | ((int)(b1 * 255.0f) & 0xff) << 16 | ((int)(g1 * 255.0f) & 0xff) << 8 | ((int)(r1 * 255.0f) & 0xff);
    TR_VTX(vp++, x0 + dx, y0 + dy, iz0, col0);
    TR_VTX(vp++, x1 + dx, y1 + dy, iz1, col1);
    TR_VTX(vp++, x0 - dx, y0 - dy, iz0, col0);
    TR_VTX(vp, x1 - dx, y1 - dy, iz1, col1);
    ulGxBeginStrip(1);
    ulGxAddStrip(v, 4);
    ulGxEndStrip();
}

/* 0x00af8438 */
void line_draw_segment(float alpha0, float alpha1, TrailPt *pa, TrailPt *pb)
{
    Vec3 s, t;
    float zn, frac;
    float sx, sy, sz, tx, ty, tz;
    float r0, g0, b0, r1, g1, b1;

    ulMatrixApplyPoint((float *)ulMatrixCur, &pa->pos.x, &s.x);
    ulMatrixApplyPoint((float *)ulMatrixCur, &pb->pos.x, &t.x);
    do {
        Vec3 *v_ = &s;

        sx = v_->x;
        sy = v_->y;
        sz = v_->z;
    } while (0);
    do {
        Vec3 *v_ = &t;

        tx = v_->x;
        ty = v_->y;
        tz = v_->z;
    } while (0);
    zn = -cam_near;
    if (zn < sz && zn < tz)
        return;
    r0 = pa->r;
    g0 = pa->g;
    b0 = pa->b;
    r1 = pb->r;
    g1 = pb->g;
    b1 = pb->b;
    if (zn < sz) {
        frac = (zn - sz) / (tz - sz);
        sx += (tx - sx) * frac;
        sy += (ty - sy) * frac;
        alpha0 += (alpha1 - alpha0) * frac;
        r0 += (r1 - r0) * frac;
        g0 += (g1 - g0) * frac;
        b0 += (b1 - b0) * frac;
        sz = zn;
    } else if (zn < tz) {
        frac = (zn - tz) / (sz - tz);
        tx += (sx - tx) * frac;
        ty += (sy - ty) * frac;
        alpha1 += (alpha0 - alpha1) * frac;
        r1 += (r0 - r1) * frac;
        g1 += (g0 - g1) * frac;
        b1 += (b0 - b1) * frac;
        tz = zn;
    }
    project_xyz(sx, sy, sz, &s.x);
    project_xyz(tx, ty, tz, &t.x);
    line_draw_band(&s.x, &t.x, alpha0, r0, g0, b0, alpha1, r1, g1, b1);
}

/* 0x00af8678 */
void line_draw_points(float alpha, TrailPt *p, int n)
{
    int i;
    float alpha1 = 0.0f;
    float rcp = 1.0f / p[n - 1].dist;
    float alpha0;

    for (i = 0; i < n - 1; i++, p++) {
        alpha0 = alpha1;
        alpha1 = alpha * p[1].dist * rcp;
        line_draw_segment(alpha0, alpha1, p, p + 1);
    }
}

/* 0x00af8720 */
int line_extract(float from, float to, Trail *trl, TrailPt *out)
{
    int n = 0;
    float len = trl->len;
    TrailPt *p = trl->pts + 1;
    float *dp;

    from = MIN(MAX_GE(from, 0.0f), len);
    to = MIN(MAX_GE(to, from), trl->len);
    while (p->dist < from)
        p++;
    {
        float frac = (from - (p - 1)->dist) / (p->dist - (p - 1)->dist);
        float rem = 1.0f - frac;

        dp = &out->pos.x;
        out++;
        *dp++ = (p - 1)->pos.x * rem + p->pos.x * frac;
        *dp++ = (p - 1)->pos.y * rem + p->pos.y * frac;
        *dp++ = (p - 1)->pos.z * rem + p->pos.z * frac;
        *dp++ = (p - 1)->r * rem + p->r * frac;
        *dp++ = (p - 1)->g * rem + p->g * frac;
        *dp++ = (p - 1)->b * rem + p->b * frac;
        *dp = 0.0f;
    }
    while (p->dist < to) {
        n++;
        *out = *p++;
        out->dist -= from;
        out++;
    }

    {
        float frac = (to - (p - 1)->dist) / (p->dist - (p - 1)->dist);
        float rem = 1.0f - frac;

        out->pos.x = (p - 1)->pos.x * rem + p->pos.x * frac;
        out->pos.y = (p - 1)->pos.y * rem + p->pos.y * frac;
        out->pos.z = (p - 1)->pos.z * rem + p->pos.z * frac;
        out->r = (p - 1)->r * rem + p->r * frac;
        out->g = (p - 1)->g * rem + p->g * frac;
        out->b = (p - 1)->b * rem + p->b * frac;
        out->dist = to - from;
    }
    return n + 2;
}

/* 0x00af8a00 */
void line_init(LineWork *w, Trail *trl)
{
    float scl = camera_get_far();
    TrailPt *p = trl->pts + trl->npts - 1;
    Vec3 plp;
    float lim;
    int i;

    w->tail = 0;
    w->head = 0;
    w->count = 1;
    player_get_pos(&plp);
    lim = plp.z - scl;
    for (i = 0; i < trl->npts; i++, p--) {
        if (p->pos.z < lim) {
            w->dist[w->head] = p->dist;
            break;
        }
    }
    w->buf = yuki_alloc((trl->npts + 2) * sizeof(TrailPt));
    w->trail = trl;
    v3_set(&w->off, 0.0f, 0.0f, 0.0f);
    task_set_dlist(w, 4);
}

/* 0x00af8b20 */
void line_release(LineWork *w)
{
    yuki_free(w->buf);
    w->buf = 0;
}

/* 0x00af8b50 */
void line_exec(LineWork *w)
{
    Vec3 shift;
    float *dist = w->dist;
    float hd;

    if (dist[w->tail] >= w->trail->len) {
        FINISH_IF(w);
    } else {
        player_get_accel(&shift);
        v3_sub(&w->off, &w->off, &shift);
        w->count++;
        hd = dist[w->head] + 8.333334f;
        if (w->count < 51) {
            w->head = (w->head + 1) % 50;
        } else {
            w->count = 50;
            w->head = (w->head + 1) % 50;
            w->tail = (w->tail + 1) % 50;
        }
        dist[w->head] = hd;
        w->nbuf = line_extract(dist[w->tail], hd, w->trail, w->buf);
    }
}

/* 0x00af8cc0 */
void line_disp(LineWork *w)
{
    TrailPt *last = &w->buf[w->nbuf - 1];
    Vec3 cpos;
    Vec3 ep;
    float fnear, ffar, alp;

    ulMatrixPush();
    player_get_pos(&cpos);
    v3_add(&cpos, &w->off);
    do {
        Vec3 *p_ = &cpos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    draw_set_blend(4, 5);
    ulMatrixApplyPoint((float *)ulMatrixCur, &last->pos.x, &ep.x);
    fnear = fog_get_near();
    ffar = fog_get_far();
    alp = 1.0f - (-ep.z - fnear) / (ffar - fnear);
    alp = MIN(MAX_GE(alp, 0.0f), 1.0f);
    project_point(&ep, &ep);
    line_draw_points(alp, w->buf, w->nbuf);
    ulMatrixPop();
}

/* 0x00af8e20 */
void *line_create(void *parent, void *trl)
{
    return task_create_inherit_arg(parent, &line_desc, 244, trl);
}

/* 0x00af8e48 */
int line_count_nodes(unsigned int *nd, int cnt)
{
    do {
        cnt++;
        if (nd[11])
            cnt = line_count_nodes((unsigned int *)nd[11], cnt);
        nd = (unsigned int *)nd[12];
    } while (nd);
    return cnt;
}

/* 0x00af8e90 */
TrailPt *line_walk_nodes(unsigned int *nd, TrailPt *out, float *acc)
{
    do {
        float *rgb;

        ulMatrixPush();
        rgb = (float *)(nd + 8);
        anim_apply_node(nd);
        ulMatrixGetTranslation((float *)ulMatrixCur, &out->pos.x);
        *acc += ulVec3Distance(&out->pos, &out[-1].pos);
        out->dist = *acc;
        out->r = rgb[0];
        out->g = rgb[1];
        out->b = rgb[2];
        out++;
        if (nd[11])
            out = line_walk_nodes((unsigned int *)nd[11], out, acc);
        ulMatrixPop();
        nd = (unsigned int *)nd[12];
    } while (nd);
    return out;
}

/* 0x00af8f60 */
float line_walk_root(unsigned int *nd, TrailPt *out)
{
    float acc;

    acc = 0;
    do {
        float *rgb;

        ulMatrixPush();
        rgb = (float *)(nd + 8);
        anim_apply_node(nd);
        ulMatrixGetTranslation((float *)ulMatrixCur, &out->pos.x);
        out->dist = 0;
        out->r = rgb[0];
        out->g = rgb[1];
        out->b = rgb[2];
        out++;
        if (nd[11])
            out = line_walk_nodes((unsigned int *)nd[11], out, &acc);
        ulMatrixPop();
        nd = (unsigned int *)nd[12];
    } while (nd);
    return acc;
}

/* 0x00af9010 */
void line_trail_build(unsigned int *root, Trail *trl)
{
    TrailPt *p;
    float base;
    int i, n;

    trl->npts = n = line_count_nodes(root, 0);
    trl->pts = yuki_alloc(n * sizeof(TrailPt));
    trl->len = line_walk_root(root, trl->pts);
    p = trl->pts + 1;
    base = p->dist;
    for (i = 1; i < trl->npts; i++) {
        p->dist -= base;
        p++;
    }
    trl->pts++;
    trl->npts--;
    trl->len -= base;
}

/* 0x00af90d0 */
void line_trail_rebuild(unsigned int *root, Trail *trl)
{
    TrailPt *p;
    float base;
    int i;

    trl->npts++;
    trl->len = line_walk_root(root, trl->pts - 1);
    p = trl->pts;
    base = p->dist;
    for (i = 1; i < trl->npts; i++) {
        p->dist -= base;
        p++;
    }
    trl->npts--;
    trl->len -= base;
}

/* 0x00af9160 */
void line_trail_free(Trail *trl)
{
    do { --trl->pts; } while (0);
    yuki_free(trl->pts);
    trl->pts = 0;
}

/* 0x00af9198 */
void line_mng_init(LineMngWork *w)
{
    TrailSrc *ts = line_sources;
    Trail *trl = w->tr;

    do {
        ulMatrixPush();
        matrix_unit_set(0);
        if (ts->motion)
            anim_start(w->models + ts->motion, 0);
        else
            anim_start(0, 0);
        line_trail_build((unsigned int *)(w->models + ts->model), trl);
        ts++;
        ulMatrixPop();
        trl++;
    } while (ts < line_sources + 4);
    w->t = 0;
    w->cur = 0;
    w->timer = 0;
}

/* 0x00af9260 */
void line_mng_release(LineMngWork *w)
{
    unsigned int i;
    Trail *trl = w->tr;

    for (i = 0; i < 4; i++)
        line_trail_free(trl++);
}

/* 0x00af92a8 */
void line_mng_exec(LineMngWork *w)
{
    int i;

    if (map_get_current() >= 6) {
        w->t = 0;
        i = w->cur + 2;
    } else {
        w->t = 0;
        i = w->cur;
    }
    ulMatrixPush();
    matrix_unit_set(0);
    if (line_sources[i].motion)
        anim_start(w->models + line_sources[i].motion, w->t);
    else
        anim_start(0, 0);
    line_trail_rebuild((unsigned int *)(w->models + line_sources[i].model), &w->tr[i]);
    ulMatrixPop();
    if (!w->timer) {
        line_create(w, &w->tr[i]);
        w->cur = (w->cur + 1) % 2;
    }
    w->timer = (w->timer + 1) % 60;
}

/* 0x00af93e0 */
void line_mng_disp(void)
{
}

/* 0x00af93e8 */
void *line_mng_create(int *w)
{
    return task_create_inherit(w, &line_mng_desc, 68);
}
