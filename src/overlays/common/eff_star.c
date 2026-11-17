/*
 * Star dust: drifting stars around a point ahead of the camera, drawn as streaks along its
 * movement and wrapped back into the field. Number, colour and range can change;
 * eff_star_col_create fades the colour.
 */

#include "types_common.h"

#include "types_overlays.h"

#include "debug.h"
#include "camera.h"
#include "color.h"
#include "heap.h"
#include "libc.h"
#include "player.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "line_batch.h"
#include "math_util.h"
#include "task.h"
#include "eff_star.h"
#include "system.h"
#include "mathdef.h"

static inline void v3_copy(Vec3 *dst, Vec3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

static inline void v3_set(Vec3 *v, float x, float y, float z)
{
    v->x = x;
    v->y = y;
    v->z = z;
}

static inline void v3_add(Vec3 *dst, Vec3 *src)
{
    dst->x += src->x;
    dst->y += src->y;
    dst->z += src->z;
}

static inline void v3_sub(Vec3 *dst, Vec3 *lhs, Vec3 *rhs)
{
    dst->x = lhs->x - rhs->x;
    dst->y = lhs->y - rhs->y;
    dst->z = lhs->z - rhs->z;
}

static inline void v3_mul(Vec3 *dst, Vec3 *src, float scale)
{
    dst->x = src->x * scale;
    dst->y = src->y * scale;
    dst->z = src->z * scale;
}

extern int ulMatrixPush(void);

extern char str_calc_center[];
extern char str_task_dist[];
extern char str_task_center[];
extern char str_vec_cam[];
extern char str_native_vec_cam[];
extern char str_unit_vec_cam[];
extern char str_unit2_vec_cam[];
extern char str_eff_star_c[];
extern struct TaskDesc eff_star_class;
extern struct TaskDesc eff_star_col_class;

/* 0x00b12c00 stg01, 0x00b1de90 stg02, 0x00b16fe8 stg03, 0x00b1d2a8 stg04, 0x00b26570 stg22, 0x00b02848 stg23, 0x00cc77c8 b_earth, 0x00cd9d08 b_eden, 0x00cc5db8 b_mars, 0x00ccaaa0 b_uranus, 0x00ccd220 b_venus */
void calc_center(EffStarWork *w)
{
    Vec3 eye, dir, unit;

    v3_copy(&w->eye_prev, &w->eye);
    camera_get_eye(&eye);
    camera_get_view_vec(&dir);
    vec3_normalize(&dir, &dir);
    v3_mul(&w->center, &dir, w->dist);
    if (w->center.x >= 1e7f || w->center.y <= -1e7f) {
        ERROR_PRINTF(str_calc_center);
        ERROR_PRINTF(str_task_dist, w->dist);
        ERROR_PRINTF(str_task_center, w->center.x, w->center.y, w->center.z);
        ERROR_PRINTF(str_vec_cam, dir.x, dir.y, dir.z);
        camera_get_view_vec(&dir);
        ERROR_PRINTF(str_native_vec_cam, dir.x, dir.y, dir.z);
        vec3_normalize(&dir, &dir);
        ERROR_PRINTF(str_unit_vec_cam, dir.x, dir.y, dir.z);
        camera_get_view_vec(&dir);
        ERROR_PRINTF(str_native_vec_cam, dir.x, dir.y, dir.z);
        vec3_normalize(&unit, &dir);
        ERROR_PRINTF(str_unit2_vec_cam, unit.x, unit.y, unit.z);
    }
    v3_copy(&w->eye, &eye);
    v3_sub(&w->delta, &w->eye, &w->eye_prev);
    v3_sub(&w->delta, &w->delta, &w->acc);
    v3_set(&w->acc, 0, 0, 0);
}

/* 0x00b12f40 stg01, 0x00b1e1d0 stg02, 0x00b17328 stg03, 0x00b1d5e8 stg04, 0x00b268b0 stg22, 0x00b02b88 stg23, 0x00cc7b08 b_earth, 0x00cda048 b_eden, 0x00cc60f8 b_mars, 0x00ccade0 b_uranus, 0x00ccd560 b_venus */
void eff_star_place(float spread, EffStarWork *w, Star *star)
{
    float cx, cy, cz, r, span, vspan;

    do {
        Vec3 *ctr = &w->center;
        cx = ctr->x;
        cy = ctr->y;
        cz = ctr->z;
    } while (0);
    r = w->range;
    span = r + r;
    vspan = spread + spread;
    star->pos.x = cx + (RAND_FLOAT_B() * span - r);
    star->pos.y = cy + (RAND_FLOAT_B() * span - r);
    star->pos.z = cz + (RAND_FLOAT_B() * span - r);
    v3_copy(&star->prev, &star->pos);
    star->vel.x = RAND_FLOAT_B() * vspan - spread;
    star->vel.z = RAND_FLOAT_B() * vspan - spread;
    star->vel.y = RAND_FLOAT_B() * vspan - spread;
}

/* 0x00b130d0 stg01, 0x00b1e360 stg02, 0x00b174b8 stg03, 0x00b1d778 stg04, 0x00b26a40 stg22, 0x00b02d18 stg23, 0x00cc7c98 b_earth, 0x00cda1d8 b_eden, 0x00cc6288 b_mars, 0x00ccaf70 b_uranus, 0x00ccd6f0 b_venus */
void eff_star_move(Star *star, Vec3 *delta)
{
    v3_sub(&star->pos, &star->pos, delta);
    v3_add(&star->pos, &star->vel);
}

/* 0x00b13130 stg01, 0x00b1e3c0 stg02, 0x00b17518 stg03, 0x00b1d7d8 stg04, 0x00b26aa0 stg22, 0x00b02d78 stg23, 0x00cc7cf8 b_earth, 0x00cda238 b_eden, 0x00cc62e8 b_mars, 0x00ccafd0 b_uranus, 0x00ccd750 b_venus */
void *eff_star_add_streak(void *pkt, Star *star)
{
    pkt = line_add_v(pkt, &star->pos, &star->prev);
    v3_copy(&star->prev, &star->pos);
    return pkt;
}

/* 0x00b13180 stg01, 0x00b1e410 stg02, 0x00b17568 stg03, 0x00b1d828 stg04, 0x00b26af0 stg22, 0x00b02dc8 stg23, 0x00cc7d48 b_earth, 0x00cda288 b_eden, 0x00cc6338 b_mars, 0x00ccb020 b_uranus, 0x00ccd7a0 b_venus */
void eff_star_wrap(float cx, float cy, float cz, float r, Star *star)
{
    float span = r + r;
    float ox = star->pos.x, oy = star->pos.y, oz = star->pos.z;
    float crd;

    do { } while (0);
    {
        float hi = cx + r;
        crd = ox;
        while (hi < crd)
            crd -= span;
    }
    while (crd < cx - r)
        crd += span;
    star->pos.x = crd;
    crd = oy;
    while (cy + r < crd)
        crd -= span;
    while (crd < cy - r)
        crd += span;
    star->pos.y = crd;
    crd = oz;
    while (cz + r < crd)
        crd -= span;
    while (crd < cz - r)
        crd += span;
    star->pos.z = crd;
    if (ox != star->pos.x || oy != star->pos.y || oz != star->pos.z)
        v3_copy(&star->prev, &star->pos);
}

/* 0x00b13320 stg01, 0x00b1e5b0 stg02, 0x00b17708 stg03, 0x00b1d9c8 stg04, 0x00b26c90 stg22, 0x00b02f68 stg23, 0x00cc7ee8 b_earth, 0x00cda428 b_eden, 0x00cc64d8 b_mars, 0x00ccb1c0 b_uranus, 0x00ccd940 b_venus */
void eff_star_place_all(EffStarWork *w, FogArg *cfg)
{
    int n = w->max;
    Star *star;

    if (n > 0) {
        star = w->buf;
        do {
            eff_star_place(cfg->k, w, star);
            star++;
        } while (--n);
    }
}

/* 0x00b13388 stg01, 0x00b1e618 stg02, 0x00b17770 stg03, 0x00b1da30 stg04, 0x00b26cf8 stg22, 0x00b02fd0 stg23, 0x00cc7f50 b_earth, 0x00cda490 b_eden, 0x00cc6540 b_mars, 0x00ccb228 b_uranus, 0x00ccd9a8 b_venus */
void eff_star_move_all(EffStarWork *w)
{
    int n = w->max;
    Star *star;

    if (n > 0) {
        star = w->buf;
        do {
            eff_star_move(star, &w->delta);
            n--;
            star++;
        } while (n);
    }
}

/* 0x00b133e0 stg01, 0x00b1e670 stg02, 0x00b177c8 stg03, 0x00b1da88 stg04, 0x00b26d50 stg22, 0x00b03028 stg23, 0x00cc7fa8 b_earth, 0x00cda4e8 b_eden, 0x00cc6598 b_mars, 0x00ccb280 b_uranus, 0x00ccda00 b_venus */
void eff_star_draw_all(EffStarWork *w)
{
    float cx, cy, cz, r;
    int n;
    Star *star;
    int vis;
    void *pkt;

    do {
        Vec3 *ctr = &w->center;
        cx = ctr->x;
        cy = ctr->y;
        cz = ctr->z;
    } while (0);
    r = w->range;
    n = w->max;
    star = w->buf;
    if (n > 0) {
        do {
            eff_star_wrap(cx, cy, cz, r, star);
            star++;
        } while (--n);
    }
    vis = 1;
    if (!(fabsf(w->limit) < FLT_EPSILON) && w->limit < vec3_length_squared(&w->delta))
        vis = 0;
    n = w->n;
    if (vis) {
        if (n > 0) {
            star = w->buf;
            pkt = line_set_color(w->color);
            do {
                pkt = eff_star_add_streak(pkt, star);
                n--;
                star++;
            } while (n);
            line_flush(pkt);
        }
    } else if (n > 0) {
        star = w->buf;
        do {
            v3_copy(&star->prev, &star->pos);
            n--;
            star++;
        } while (n);
    }
}

/* 0x00b13558 stg01, 0x00b1e7e8 stg02, 0x00b17940 stg03, 0x00b1dc00 stg04, 0x00b26ec8 stg22, 0x00b031a0 stg23, 0x00cc8120 b_earth, 0x00cda660 b_eden, 0x00cc6710 b_mars, 0x00ccb3f8 b_uranus, 0x00ccdb78 b_venus */
void eff_star_init(EffStarWork *w, FogArg *cfg)
{
    w->n = cfg->count;
    w->dist = cfg->range;
    w->max = cfg->count;
    w->range = cfg->range;
    w->color = cfg->color;
    if (w->max < 1) {
        do { FINISH(w); } while (0);
        return;
    }
    w->buf = heap_block_tag(game_malloc(w->max * 36), str_eff_star_c, 403);
    calc_center(w);
    ulMatrixPush();
    do {
        Vec3 *p_ = &w->eye;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    eff_star_place_all(w, cfg);
    ulMatrixPop();
    w->limit = 0;
}

/* 0x00b13630 stg01, 0x00b1e8c0 stg02, 0x00b17a18 stg03, 0x00b1dcd8 stg04, 0x00b26fa0 stg22, 0x00b03278 stg23, 0x00cc81f8 b_earth, 0x00cda738 b_eden, 0x00cc67e8 b_mars, 0x00ccb4d0 b_uranus, 0x00ccdc50 b_venus */
void eff_star_release(EffStarWork *w)
{
    heap_free(w->buf, 0);
}

/* 0x00b13640 stg01, 0x00b1e8d0 stg02, 0x00b17a28 stg03, 0x00b1dce8 stg04, 0x00b26fb0 stg22, 0x00b03288 stg23, 0x00cc8208 b_earth, 0x00cda748 b_eden, 0x00cc67f8 b_mars, 0x00ccb4e0 b_uranus, 0x00ccdc60 b_venus */
void eff_star_exec(EffStarWork *w)
{
    Vec3 ofs;

    player_get_vec_old(&ofs);
    v3_add(&w->acc, &ofs);
    ulMatrixPush();
    do {
        Vec3 *p_ = &w->eye;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    eff_star_move_all(w);
    ulMatrixPop();
}

/* 0x00b136d0 stg01, 0x00b1e960 stg02, 0x00b17ab8 stg03, 0x00b1dd78 stg04, 0x00b27040 stg22, 0x00b03318 stg23, 0x00cc8298 b_earth, 0x00cda7d8 b_eden, 0x00cc6888 b_mars, 0x00ccb570 b_uranus, 0x00ccdcf0 b_venus */
void eff_star_disp(EffStarWork *w)
{
    calc_center(w);
    ulMatrixPush();
    do {
        Vec3 *p_ = &w->eye;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    eff_star_draw_all(w);
    ulMatrixPop();
}

/* 0x00b13720 stg01, 0x00b1e9b0 stg02, 0x00b17b08 stg03, 0x00b1ddc8 stg04, 0x00b27090 stg22, 0x00b03368 stg23, 0x00cc82e8 b_earth, 0x00cda828 b_eden, 0x00cc68d8 b_mars, 0x00ccb5c0 b_uranus, 0x00ccdd40 b_venus */
void *eff_star_create(void *parent, FogArg *cfg)
{
    return task_create_arg(parent, &eff_star_class, 88, cfg);
}

/* 0x00b13748 stg01, 0x00b1e9d8 stg02, 0x00b17b30 stg03, 0x00b1ddf0 stg04, 0x00b270b8 stg22, 0x00b03390 stg23, 0x00cc8310 b_earth, 0x00cda850 b_eden, 0x00cc6900 b_mars, 0x00ccb5e8 b_uranus, 0x00ccdd68 b_venus */
void eff_star_set_color(EffStarWork *w, unsigned int color)
{
    if (w && !(TASK_OF(w)->flags & TASK_F_END))
        w->color = color;
}

/* 0x00b13768 stg01, 0x00b1e9f8 stg02, 0x00b17b50 stg03, 0x00b1de10 stg04, 0x00b270d8 stg22, 0x00b033b0 stg23, 0x00cc8330 b_earth, 0x00cda870 b_eden, 0x00cc6920 b_mars, 0x00ccb608 b_uranus, 0x00ccdd88 b_venus */
unsigned int eff_star_get_color(EffStarWork *w)
{
    if (!w || (TASK_OF(w)->flags & TASK_F_END))
        return 0;
    return w->color;
}

/* 0x00b13790 stg01, 0x00b1ea20 stg02, 0x00b17b78 stg03, 0x00b1de38 stg04, 0x00b27100 stg22, 0x00b033d8 stg23, 0x00cc8358 b_earth, 0x00cda898 b_eden, 0x00cc6948 b_mars, 0x00ccb630 b_uranus, 0x00ccddb0 b_venus */
void eff_star_set_count(EffStarWork *w, int n)
{
    if (w && !(TASK_OF(w)->flags & TASK_F_END))
        w->n = n <= w->max ? n : w->max;
}

/* 0x00b137c0 stg01, 0x00b1ea50 stg02, 0x00b17ba8 stg03, 0x00b1de68 stg04, 0x00b27130 stg22, 0x00b03408 stg23, 0x00cc8388 b_earth, 0x00cda8c8 b_eden, 0x00cc6978 b_mars, 0x00ccb660 b_uranus, 0x00ccdde0 b_venus */
int eff_star_get_count(EffStarWork *w)
{
    if (!w || (TASK_OF(w)->flags & TASK_F_END))
        return 0;
    return w->n;
}

/* 0x00b137e8 stg01, 0x00b1ea78 stg02, 0x00b17bd0 stg03, 0x00b1de90 stg04, 0x00b27158 stg22, 0x00b03430 stg23, 0x00cc83b0 b_earth, 0x00cda8f0 b_eden, 0x00cc69a0 b_mars, 0x00ccb688 b_uranus, 0x00ccde08 b_venus */
int eff_star_get_max(EffStarWork *w)
{
    if (!w || (TASK_OF(w)->flags & TASK_F_END))
        return 0;
    return w->max;
}

/* 0x00b13810 stg01, 0x00b1eaa0 stg02, 0x00b17bf8 stg03, 0x00b1deb8 stg04, 0x00b27180 stg22, 0x00b03458 stg23, 0x00cc83d8 b_earth, 0x00cda918 b_eden, 0x00cc69c8 b_mars, 0x00ccb6b0 b_uranus, 0x00ccde30 b_venus */
void eff_star_set_limit(float dis, EffStarWork *w)
{
    if (w && !(TASK_OF(w)->flags & TASK_F_END))
        w->limit = dis * dis;
}

/* 0x00b13838 stg01, 0x00b1eac8 stg02, 0x00b17c20 stg03, 0x00b1dee0 stg04, 0x00b271a8 stg22, 0x00b03480 stg23, 0x00cc8400 b_earth, 0x00cda940 b_eden, 0x00cc69f0 b_mars, 0x00ccb6d8 b_uranus, 0x00ccde58 b_venus */
void eff_star_col_init(EffStarColWork *work, EffStarWork *star, unsigned int to, int frames)
{
    int a_from, a_to;

    work->star = star;
    work->from = eff_star_get_color(star);
    work->to = to;
    work->frames = frames;
    work->count = 0;
    a_from = color_get_a(work->from);
    a_to = color_get_a(work->to);
    if (!a_from && !a_to)
        eff_star_set_count(work->star, 0);
    else
        eff_star_set_count(work->star, eff_star_get_max(work->star));
    if (work->frames > 0) {
        eff_star_set_color(work->star, work->from);
        return;
    }
    eff_star_set_color(work->star, work->to);
    FINISH(work);
}

/* 0x00b13918 stg01, 0x00b1eba8 stg02, 0x00b17d00 stg03, 0x00b1dfc0 stg04, 0x00b27288 stg22, 0x00b03560 stg23, 0x00cc84e0 b_earth, 0x00cdaa20 b_eden, 0x00cc6ad0 b_mars, 0x00ccb7b8 b_uranus, 0x00ccdf38 b_venus */
void eff_star_col_release(EffStarColWork *w)
{
    if (!color_get_a(w->to))
        eff_star_set_count(w->star, 0);
}

/* 0x00b13958 stg01, 0x00b1ebe8 stg02, 0x00b17d40 stg03, 0x00b1e000 stg04, 0x00b272c8 stg22, 0x00b035a0 stg23, 0x00cc8520 b_earth, 0x00cdaa60 b_eden, 0x00cc6b10 b_mars, 0x00ccb7f8 b_uranus, 0x00ccdf78 b_venus */
void eff_star_col_exec(EffStarColWork *w)
{
    if (++w->count >= w->frames) {
        FINISH_IF(w);
        w->count = w->frames;
    }
    eff_star_set_color(w->star, color_blend(w->from, w->to, (float)w->count / w->frames));
}

/* 0x00b139d8 stg01, 0x00b1ec68 stg02, 0x00b17dc0 stg03, 0x00b1e080 stg04, 0x00b27348 stg22, 0x00b03620 stg23, 0x00cc85a0 b_earth, 0x00cdaae0 b_eden, 0x00cc6b90 b_mars, 0x00ccb878 b_uranus, 0x00ccdff8 b_venus */
int eff_star_col_create(void *star, unsigned int color, int frames)
{
    EffStarColWork *work = task_create(star, &eff_star_col_class, 20);

    if (work)
        eff_star_col_init(work, star, color, frames);
    return (int)work;
}
