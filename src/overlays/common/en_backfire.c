/*
 * Backfire: an enemy's exhaust glow, a flickering, spinning additive sprite at a point or at each
 * marked node; the beam version adds a textured flame streak scaled by the owner.
 */

#include "types_common.h"
#include "color.h"

#include "types_overlays.h"

#include "debug.h"
#include "anim.h"
#include "fog.h"
#include "libc.h"
#include "math_util.h"
#include "node_draw_state.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "en_backfire.h"
#include "mathdef.h"

static inline void v3_add(Vec3 *dst, Vec3 *src)
{
    dst->x += src->x;
    dst->y += src->y;
    dst->z += src->z;
}

/* sc receives the sine and cosine */
#define ROT_Z(ang, sc, x, y) \
    do { \
        float rot_u = x, rot_v = y; \
        ulSinCos(ang, &sc[0], &sc[1]); \
        x = rot_u * sc[1] - rot_v * sc[0]; \
        y = rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

#define ROT_X(ang, sc, y, z) \
    do { \
        float rot_u = y, rot_v = z; \
        ulSinCos(ang, &sc[0], &sc[1]); \
        y = rot_u * sc[1] - rot_v * sc[0]; \
        z = rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

#define ROT_Y(ang, sc, x, z) \
    do { \
        float rot_u = x, rot_v = z; \
        ulSinCos(ang, &sc[0], &sc[1]); \
        x = rot_u * sc[1] + rot_v * sc[0]; \
        z = -rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

static inline void v3_mul(Vec3 *dst, Vec3 *src, float scale)
{
    dst->x = src->x * scale;
    dst->y = src->y * scale;
    dst->z = src->z * scale;
}

extern int ulMatrixPush(void);
extern void *ulMatrixCur;

#define MAX_GE(a, b) ((a) >= (b) ? (a) : (b))

extern void project_xyz(float x, float y, float z, float *out);
extern float cam_near;

extern NjClip cam_far;

#define UV_VTX(v, x, y, q, t, s, c) \
    do { \
        float *v_ = (v); \
        *v_++ = (x); \
        *v_++ = (y); \
        *v_++ = (q); \
        *v_++ = (t); \
        *v_ = (s); \
        *(unsigned int *)(v_ + 1) = (c); \
    } while (0)

extern Vec3 en_backfire_forward;
extern struct TaskDesc en_backfire_class;
extern struct TaskDesc en_backfire_beam_class;
extern int en_backfire_texture;
extern int en_backfire_beam_texture;

/* 0x00b0cde8 stg01, 0x00b18078 stg02, 0x00b111d0 stg03, 0x00b17a48 stg04, 0x00b2b060 stg05, 0x00b20758 stg22 */
int en_backfire_collect_nodes(unsigned int *node, unsigned int mask, NodeRec **out, int num)
{
    do {
        if (num <= 0)
            return 0;
        ulMatrixPush();
        anim_apply_node(node);
        if (node[0] & mask) {
            num--;
            ulMatrixGetTranslation((float *)ulMatrixCur, &(*out)->pos.x);
            yuki_matrix_get_scale(&(*out)->scale);
            ulMatrixApplyVector((float *)ulMatrixCur, &en_backfire_forward.x, &(*out)->fwd.x);
            (*out)++;
        }
        if (!(node[0] & 0x10))
            num = en_backfire_collect_nodes((unsigned int *)node[11], mask, out, num);
        ulMatrixPop();
        node = (unsigned int *)node[12];
    } while (node);
    return num;
}

/* 0x00b0cee0 stg01, 0x00b18170 stg02, 0x00b112c8 stg03, 0x00b17b40 stg04, 0x00b2b158 stg05, 0x00b20850 stg22 */
void en_backfire_init(EnBackFireWork *w, BackFireLink *lnk)
{
    ModelPart *gfx = lnk->gfx;
    NodeRec *p;

    if (!gfx) {
        w->n = 1;
        w->buf = yuki_alloc(36);
    } else {
        w->n = yuki_node_count((struct YNode *)gfx->root, lnk->flags, 0);
        w->buf = yuki_alloc(w->n * 36);
        if (lnk->use_pose) {
            p = w->buf;
            anim_start((void *)gfx->clip, gfx->frame);
            ulMatrixPush();
            matrix_unit_set(0);
            vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
            en_backfire_collect_nodes((unsigned int *)gfx->root, lnk->flags, &p, w->n);
            ulMatrixPop();
        }
    }
    w->link = *lnk;
    w->angle = 0;
    w->counter = 0;
    w->tex = en_backfire_texture;
    w->beam_tex = en_backfire_beam_texture;
    task_set_dlist(w, 6);
}

/* 0x00b0d048 stg01, 0x00b182d8 stg02, 0x00b11430 stg03, 0x00b17ca8 stg04, 0x00b2b2c0 stg05, 0x00b209b8 stg22 */
void en_backfire_release(EnBackFireWork *w)
{
    yuki_free(w->buf);
    w->buf = 0;
}

/* 0x00b0d078 stg01, 0x00b18308 stg02, 0x00b11460 stg03, 0x00b17cd8 stg04, 0x00b2b2f0 stg05, 0x00b209e8 stg22 */
void en_backfire_exec(EnBackFireWork *w)
{
    w->counter = (w->counter + 1) % 6;
    w->angle = w->angle + 1638.4f;
}

/* 0x00b0d0c0 stg01, 0x00b18350 stg02, 0x00b114a8 stg03, 0x00b17d20 stg04, 0x00b2b338 stg05, 0x00b20a30 stg22 */
void en_backfire_disp(EnBackFireWork *w)
{
    float v[4], sz[4], npos[4];
    float sc[4][2];
    NodeRec *rec;
    int old_tst, old_zt, old_fc;
    void *fog;
    BackFireLink *lnk = &w->link;
    ModelPart *gfx = w->link.gfx;
    Vec3 *scl = lnk->obj_scale;
    unsigned int col;
    float smin, size, hw, hh, x, y, z, pw, ph;
    int *ang;
    NodeRec *cur;
    int n;
    Task *ptask;
    NjClip *clip;

    if (lnk->alpha)
        col = ((int)(*lnk->alpha * 255.0f) + 1) >> 1 << 24 | 0x808080;
    else
        col = COL_WHITE;
    ulGxSetTextureList((void *)w->tex_list);
    ulSinCos((int)(w->counter / 6.0f * ANG_TURN), &sc[0][0], &sc[0][1]);
    smin = MIN(scl->z, MIN(scl->x, scl->y));
    if (lnk->scale)
        smin *= *lnk->scale;
    size = smin * 10.0f;
    hw = (sc[0][0] + 9.0f) * smin;
    hw *= 0.5f;
    hh = (sc[0][1] + 9.0f) * smin;
    hh *= 0.5f;
    fog = fog_set_enable(0);
    old_tst = draw_set_test_mode(0);
    old_zt = draw_set_ztest_mode(0x60000);
    old_fc = draw_set_fc_mode(0x2000);
    draw_set_blend(4, 1);
    if (!gfx) {
        if (view_sphere_visible(lnk->pos, size)) {
            ulMatrixApplyPoint((float *)ulMatrixCur, &lnk->pos->x, v);
            if (v[2] < -cam_near) {
                float *tp = sz;
                sz[2] = v[2];
                sz[0] = 1.0f;
                sz[1] = 0;
                project_point((struct Vec3 *)v, (struct Vec3 *)v);
                project_point_abs(tp, tp);
                hw *= sz[0];
                hh *= sz[0];
                yuki_sprite2_draw(w->angle, col, w->tex, v[0], v[1], 1.0f / v[2], hw, hh);
                yuki_sprite2_draw(w->angle, col, w->tex, v[0], v[1], 1.0f / v[2], hh, hw);
            }
        }
    } else if (lnk->use_pose) {
        int n;

        n = w->n;
        cur = w->buf;
        ang = lnk->ang;
        while (n-- > 0) {
            do {
                x = cur->pos.x;
                y = cur->pos.y;
                z = cur->pos.z;
            } while (0);
            ROT_Z(ang[2], sc[1], x, y);
            ROT_X(ang[0], sc[2], y, z);
            ROT_Y(ang[1], sc[3], x, z);
            npos[0] = x;
            npos[1] = y;
            npos[2] = z;
            v3_add((Vec3 *)npos, lnk->pos);
            if (view_sphere_visible((Vec3 *)npos, size)) {
                ulMatrixApplyPoint((float *)ulMatrixCur, npos, npos);
                if (!(-cam_near < npos[2])) {
                    float pw, ph;
                    v[0] = cur->scale.x;
                    v[2] = npos[2];
                    v[1] = 0;
                    project_point((struct Vec3 *)npos, (struct Vec3 *)npos);
                    project_point_abs(v, v);
                    pw = hw * v[0];
                    ph = hh * v[0];
                    yuki_sprite2_draw(w->angle, col, w->tex, npos[0], npos[1], 1.0f / npos[2], pw, ph);
                    yuki_sprite2_draw(w->angle, col, w->tex, npos[0], npos[1], 1.0f / npos[2], ph, pw);
                }
            }
        }
    } else {
        rec = w->buf;
        n = w->n;
        anim_start((void *)gfx->clip, gfx->frame);
        ulMatrixPush();
        matrix_unit_set(0);
        do {
            Vec3 *p_ = lnk->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        en_backfire_rotate_yxz(lnk->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_scale(ulMatrixCur, lnk->obj_scale->x, lnk->obj_scale->y, lnk->obj_scale->z);
        en_backfire_collect_nodes((unsigned int *)gfx->root, lnk->flags, &rec, n);
        ulMatrixPop();
        rec = w->buf;
        if (n-- > 0) {
            clip = &cam_far;
            do {
                if (view_sphere_visible(&rec->pos, size)) {
                    ulMatrixApplyPoint((float *)ulMatrixCur, &rec->pos.x, v);
                    if (!(-clip->near < v[2])) {
                        sz[0] = rec->scale.x;
                        sz[2] = v[2];
                        sz[1] = 0;
                        project_point((struct Vec3 *)v, (struct Vec3 *)v);
                        project_point_abs(sz, sz);
                        pw = hw * sz[0];
                        ph = hh * sz[0];
                        yuki_sprite2_draw(w->angle, col, w->tex, v[0], v[1], 1.0f / v[2], pw, ph);
                        yuki_sprite2_draw(w->angle, col, w->tex, v[0], v[1], 1.0f / v[2], ph, pw);
                    }
                }
            } while (n-- > 0);
        }
    }
    draw_set_fc_mode(old_fc);
    draw_set_ztest_mode(old_zt);
    draw_set_test_mode(old_tst);
    fog_set_enable(fog);
    if (lnk->cb) {
        ptask = TASK_OF(w)->parent;
        ((void (*)(void *))lnk->cb)(ptask ? ptask + 1 : 0);
    }
}

/* 0x00b0d798 stg01, 0x00b18a28 stg02, 0x00b11b80 stg03, 0x00b183f8 stg04, 0x00b2ba10 stg05, 0x00b21108 stg22 */
void *en_backfire_create_with_cb(void *parent, BackFireLink *lnk, int clear)
{
    if (clear) {
        lnk->use_pose = 0;
        lnk->scale = 0;
        lnk->alpha = 0;
    }
    return task_create_inherit_arg(parent, &en_backfire_class, 84, lnk);
}

/* 0x00b0d7d0 stg01, 0x00b18a60 stg02, 0x00b11bb8 stg03, 0x00b18430 stg04, 0x00b2ba48 stg05, 0x00b21140 stg22 */
void *en_backfire_create(void *parent, BackFireLink *lnk, int clear)
{
    lnk->cb = 0;
    return en_backfire_create_with_cb(parent, lnk, clear);
}

/* 0x00b0d7f0 stg01, 0x00b18a80 stg02, 0x00b11bd8 stg03, 0x00b18450 stg04, 0x00b2ba68 stg05, 0x00b21160 stg22 */
int en_backfire_set_texture(int tex)
{
    int prev = en_backfire_texture;

    en_backfire_texture = tex;
    return prev;
}

/* 0x00b0d800 stg01, 0x00b18a90 stg02, 0x00b11be8 stg03, 0x00b18460 stg04, 0x00b2ba78 stg05, 0x00b21170 stg22 */
int en_backfire_set_cb(EnBackFireWork *w, int cb)
{
    int prev = w->link.cb;

    w->link.cb = cb;
    return prev;
}

/* 0x00b0d810 stg01, 0x00b18aa0 stg02, 0x00b11bf8 stg03, 0x00b18470 stg04, 0x00b2ba88 stg05, 0x00b21180 stg22 */
void en_backfire_beam_init(EnBackFireWork *w, BackFireLink *lnk)
{
    ModelPart *gfx = lnk->gfx;
    NodeRec *p;

    if (!gfx) {
        w->n = 1;
        w->buf = yuki_alloc(36);
    } else {
        w->n = yuki_node_count((struct YNode *)gfx->root, lnk->flags, 0);
        w->buf = yuki_alloc(w->n * 36);
        if (lnk->use_pose) {
            p = w->buf;
            anim_start((void *)gfx->clip, gfx->frame);
            ulMatrixPush();
            matrix_unit_set(0);
            vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
            en_backfire_collect_nodes((unsigned int *)gfx->root, lnk->flags, &p, w->n);
            ulMatrixPop();
        }
    }
    w->link = *lnk;
    w->angle = 0;
    w->counter = 0;
    w->tex = en_backfire_texture;
    w->beam_tex = en_backfire_beam_texture;
    task_set_dlist(w, 6);
}

/* 0x00b0d978 stg01, 0x00b18c08 stg02, 0x00b11d60 stg03, 0x00b185d8 stg04, 0x00b2bbf0 stg05, 0x00b212e8 stg22 */
void en_backfire_beam_release(EnBackFireWork *w)
{
    yuki_free(w->buf);
    w->buf = 0;
}

/* 0x00b0d9a8 stg01, 0x00b18c38 stg02, 0x00b11d90 stg03, 0x00b18608 stg04, 0x00b2bc20 stg05, 0x00b21318 stg22 */
void en_backfire_beam_exec(EnBackFireWork *w)
{
    w->counter = (w->counter + 1) % 6;
    w->angle = w->angle + 1638.4f;
}

/* 0x00b0d9f0 stg01, 0x00b18c80 stg02, 0x00b11dd8 stg03, 0x00b18650 stg04, 0x00b2bc68 stg05, 0x00b21360 stg22 */
void en_backfire_beam_draw_band(float *p0, float *p1, unsigned int col, float u0, float u1, float width0, float width1)
{
    UvVtx v[4];
    UvVtx *vp = v;
    float frac, x0, y0, z0, x1, y1, z1, dx, dy, iz0, iz1, ax, ay, bx, by;

    frac = yuki_clip_line(p0, p1);
    if (yuki_is_offscreen(p1))
        return;
    if (!(fabsf(frac - 1.0f) < FLT_EPSILON)) u1 = u0 + (u1 - u0) * frac;
    frac = yuki_clip_line(p1, p0);
    if (!(fabsf(frac - 1.0f) < FLT_EPSILON)) u0 = u1 + (u0 - u1) * frac;
    do {
        x0 = p0[0];
        y0 = p0[1];
        z0 = p0[2];
    } while (0);
    do {
        x1 = p1[0];
        y1 = p1[1];
        z1 = p1[2];
    } while (0);
    iz0 = 1.0f / z0;
    iz1 = 1.0f / z1;
    dx = y0 - y1;
    dy = x1 - x0;
    if (sqrtf(dx * dx + dy * dy) <= FLT_EPSILON)
        frac = 8388608.0f;
    else
        frac = 1.0f / sqrtf(dx * dx + dy * dy);
    dx *= frac;
    dy *= frac;
    ax = dx * width0;
    ay = dy * width0;
    bx = dx * width1;
    by = dy * width1;
    UV_VTX((float *)vp++, x0 + ax, y0 + ay, iz0, 0, u0, col);
    UV_VTX((float *)vp++, x1 + bx, y1 + by, iz1, 0, u1, col);
    UV_VTX((float *)vp++, x0 - ax, y0 - ay, iz0, 1.0f, u0, col);
    UV_VTX((float *)vp++, x1 - bx, y1 - by, iz1, 1.0f, u1, col);
    ulGxBeginTexStrip(1);
    ulGxAddTexStrip(v, 4);
    ulGxEndTexStrip();
}

/* 0x00b0dd10 stg01, 0x00b18fa0 stg02, 0x00b120f8 stg03, 0x00b18970 stg04, 0x00b2bf88 stg05, 0x00b21680 stg22 */
void en_backfire_beam_draw(EnBackFireWork *w, Vec3 *p0, Vec3 *p1, unsigned int col, float width)
{
    Vec3 va, vb;
    float sz[3];
    float zn, frac, u0, u1, width0, width1, ax, ay, az, bx, by, bz;
    void *fog;

    fog = fog_set_enable(0);
    u0 = 0;
    u1 = 1.0f;
    draw_set_blend(4, 1);
    ulGxSelectTexture(ulGxGetTextureList(), w->beam_tex);
    ulMatrixApplyPoint((float *)ulMatrixCur, &p0->x, &va.x);
    ulMatrixApplyPoint((float *)ulMatrixCur, &p1->x, &vb.x);
    sz[0] = width;
    sz[2] = va.z;
    sz[1] = u0;
    project_point_abs(sz, sz);
    width0 = sz[0];
    sz[0] = width;
    sz[2] = vb.z;
    sz[1] = u0;
    project_point_abs(sz, sz);
    width1 = sz[0];
    do {
        ax = va.x;
        ay = va.y;
        az = va.z;
    } while (0);
    do {
        Vec3 *v_ = &vb;
        bx = v_->x;
        by = v_->y;
        bz = v_->z;
    } while (0);
    zn = -cam_near;
    if (az < zn && bz < zn) {
        if (zn < az) {
            frac = (zn - az) / (bz - az);
            az = zn;
            ax += (bx - ax) * frac;
            ay += (by - ay) * frac;
            u0 = frac + u0;
        } else if (zn < bz) {
            frac = (zn - bz) / (az - bz);
            bz = zn;
            bx += (ax - bx) * frac;
            by += (ay - by) * frac;
            u1 = -frac + u1;
        }
        project_xyz(ax, ay, az, &va.x);
        project_xyz(bx, by, bz, &vb.x);
        en_backfire_beam_draw_band(&va.x, &vb.x, col, u0, u1, width0, width1);
    }
    fog_set_enable(fog);
}

/* 0x00b0df50 stg01, 0x00b191e0 stg02, 0x00b12338 stg03, 0x00b18bb0 stg04, 0x00b2c1c8 stg05, 0x00b218c0 stg22 */
void en_backfire_beam_disp(EnBackFireWork *w)
{
    float v[4], sz[4], npos[4], tip[4], vp[4];
    float sc[6][2];
    NodeRec *rec;
    int old_tst, old_zt, old_fc;
    void *fog;
    BackFireLink *lnk = &w->link;
    ModelPart *gfx = w->link.gfx;
    Vec3 *scl = lnk->obj_scale;
    Vec3 *pos = 0;
    unsigned int col;
    float smin, size, hw, hh;
    int *ang = 0;
    NodeRec *cur = 0;
    Task *ptask = 0;

    if (lnk->alpha)
        col = ((int)(*lnk->alpha * 255.0f) + 1) >> 1 << 24 | 0x808080;
    else
        col = COL_WHITE;
    ulGxSetTextureList((void *)w->tex_list);
    ulSinCos((int)(w->counter / 6.0f * ANG_TURN), &sc[0][0], &sc[0][1]);
    smin = MIN(scl->z, MIN(scl->x, scl->y));
    if (lnk->scale)
        smin *= *lnk->scale;
    size = smin * 10.0f;
    hw = (sc[0][0] + 9.0f) * smin;
    hw *= 0.5f;
    hh = (sc[0][1] + 9.0f) * smin;
    hh *= 0.5f;
    fog = fog_set_enable(0);
    old_tst = draw_set_test_mode(0);
    old_zt = draw_set_ztest_mode(0x60000);
    old_fc = draw_set_fc_mode(0x2000);
    draw_set_blend(4, 1);
    if (!gfx) {
        float x, y, len, width;

        pos = lnk->pos;
        x = y = 0;
        {
            float raw = lnk->len;

            len = lnk->len_scale ? *lnk->len_scale * raw : raw;
        }
        {
            float raw = lnk->width;

            width = lnk->width_scale ? *lnk->width_scale * raw : raw;
        }
        if (view_sphere_visible(pos, size)) {
            ulMatrixApplyPoint((float *)ulMatrixCur, &pos->x, v);
            if (v[2] < -cam_near) {
                project_point((struct Vec3 *)v, (struct Vec3 *)v);
                project_point_abs(sz, sz);
                hw *= sz[0];
                hh *= sz[0];
                yuki_sprite2_draw(w->angle, col, w->tex, v[0], v[1], 1.0f / v[2], hw, hh);
                yuki_sprite2_draw(w->angle, col, w->tex, v[0], v[1], 1.0f / v[2], hh, hw);
            }
        }
        if (view_sphere_visible(pos, MAX_GE(len, width))) {
            ROT_X(lnk->ang[0], sc[1], y, len);
            ROT_Y(lnk->ang[1], sc[2], x, len);
            sz[0] = x;
            sz[1] = y;
            sz[2] = len;
            v[0] = pos->x + sz[0];
            v[1] = pos->y + sz[1];
            v[2] = pos->z + sz[2];
            en_backfire_beam_draw(w, pos, (Vec3 *)v, col, width);
        }
    } else if (lnk->use_pose) {
        float x = 0, y = 0, z = 0, len, width, pw, ph;
        int n;

        n = w->n;
        cur = w->buf;
        ang = lnk->ang;
        while (n-- > 0) {
            {
                float raw = lnk->len;

                len = (lnk->len_scale ? *lnk->len_scale * raw : raw) * cur->scale.z;
            }
            {
                float raw = lnk->width;

                width = (lnk->width_scale ? *lnk->width_scale * raw : raw) * cur->scale.y;
            }
            x = cur->pos.x;
            y = cur->pos.y;
            z = cur->pos.z;
            ROT_Z(ang[2], sc[3], x, y);
            ROT_X(ang[0], sc[4], y, z);
            ROT_Y(ang[1], sc[5], x, z);
            npos[0] = x;
            npos[1] = y;
            npos[2] = z;
            v3_add((Vec3 *)npos, lnk->pos);
            if (view_sphere_visible((Vec3 *)npos, size)) {
                ulMatrixApplyPoint((float *)ulMatrixCur, npos, vp);
                if (vp[2] < -cam_near) {
                    v[0] = cur->scale.x;
                    v[2] = vp[2];
                    v[1] = 0;
                    project_point((struct Vec3 *)vp, (struct Vec3 *)vp);
                    project_point_abs(v, v);
                    pw = hw * v[0];
                    ph = hh * v[0];
                    yuki_sprite2_draw(w->angle, col, w->tex, vp[0], vp[1], 1.0f / vp[2], pw, ph);
                    yuki_sprite2_draw(w->angle, col, w->tex, vp[0], vp[1], 1.0f / vp[2], ph, pw);
                }
            }
            if (view_sphere_visible((Vec3 *)npos, MAX_GE(width, len * vec3_length(&cur->fwd)))) {
                v3_mul((Vec3 *)tip, &cur->fwd, len);
                v3_add((Vec3 *)tip, (Vec3 *)npos);
                en_backfire_beam_draw(w, (Vec3 *)npos, (Vec3 *)tip, col, width);
            }
        }
    } else {
        float len = 0, width = 0, pw, ph;
        int n = 0;

        rec = w->buf;
        n = w->n;
        anim_start((void *)gfx->clip, gfx->frame);
        ulMatrixPush();
        matrix_unit_set(0);
        do {
            Vec3 *p_ = lnk->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        en_backfire_rotate_yxz(lnk->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_scale(ulMatrixCur, lnk->obj_scale->x, lnk->obj_scale->y, lnk->obj_scale->z);
        en_backfire_collect_nodes((unsigned int *)gfx->root, lnk->flags, &rec, n);
        ulMatrixPop();
        rec = w->buf;
        while (n-- > 0) {
            {
                float raw = lnk->len;

                len = (lnk->len_scale ? *lnk->len_scale * raw : raw) * rec->scale.z;
            }
            {
                float raw = lnk->width;

                width = (lnk->width_scale ? *lnk->width_scale * raw : raw) * rec->scale.y;
            }
            if (view_sphere_visible(&rec->pos, size)) {
                ulMatrixApplyPoint((float *)ulMatrixCur, &rec->pos.x, v);
                if (v[2] < -cam_far.near) {
                    sz[0] = rec->scale.x;
                    sz[2] = v[2];
                    sz[1] = 0;
                    project_point((struct Vec3 *)v, (struct Vec3 *)v);
                    project_point_abs(sz, sz);
                    pw = hw * sz[0];
                    ph = hh * sz[0];
                    yuki_sprite2_draw(w->angle, col, w->tex, v[0], v[1], 1.0f / v[2], pw, ph);
                    yuki_sprite2_draw(w->angle, col, w->tex, v[0], v[1], 1.0f / v[2], ph, pw);
                }
            }
            if (view_sphere_visible(&rec->pos, MAX_GE(width, len * vec3_length(&rec->fwd)))) {
                v3_mul((Vec3 *)v, &rec->fwd, len);
                v3_add((Vec3 *)v, &rec->pos);
                en_backfire_beam_draw(w, &rec->pos, (Vec3 *)v, col, width);
            }
        }
    }
    draw_set_fc_mode(old_fc);
    draw_set_ztest_mode(old_zt);
    draw_set_test_mode(old_tst);
    fog_set_enable(fog);
    if (lnk->cb) {
        ptask = TASK_OF(w)->parent;
        ((void (*)(void *))lnk->cb)(ptask ? ptask + 1 : 0);
    }
}

/* 0x00b0e920 stg01, 0x00b19bb0 stg02, 0x00b12d08 stg03, 0x00b19580 stg04, 0x00b2cb98 stg05, 0x00b22290 stg22 */
void *en_backfire_beam_create_with_cb(void *parent, BackFireLink *lnk)
{
    return task_create_inherit_arg(parent, &en_backfire_beam_class, 84, lnk);
}

/* 0x00b0e948 stg01, 0x00b19bd8 stg02, 0x00b12d30 stg03, 0x00b195a8 stg04, 0x00b2cbc0 stg05, 0x00b222b8 stg22 */
void *en_backfire_beam_create(void *parent, BackFireLink *lnk)
{
    lnk->cb = 0;
    return en_backfire_beam_create_with_cb(parent, lnk);
}

/* 0x00b0e968 stg01, 0x00b19bf8 stg02, 0x00b12d50 stg03, 0x00b195c8 stg04, 0x00b2cbe0 stg05, 0x00b222d8 stg22 */
int en_backfire_beam_set_texture(int tex)
{
    int prev = en_backfire_beam_texture;

    en_backfire_beam_texture = tex;
    return prev;
}

/* 0x00b0e978 stg01, 0x00b19c08 stg02, 0x00b12d60 stg03, 0x00b195d8 stg04, 0x00b2cbf0 stg05, 0x00b222e8 stg22 */
int en_backfire_beam_set_cb(EnBackFireWork *w, int cb)
{
    int prev = w->link.cb;

    w->link.cb = cb;
    return prev;
}

/* 0x00b0e988 stg01, 0x00b19c18 stg02, 0x00b12d70 stg03, 0x00b195e8 stg04, 0x00b2cc00 stg05, 0x00b222f8 stg22 */
void en_backfire_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
