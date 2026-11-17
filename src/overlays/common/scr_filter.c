/*
 * Screen filter: a soft, slowly turning disc of light where a point ahead of the player's view
 * projects. Distance, size, centre and rim alpha and colour ease to scr_filter_fade's targets.
 */

#include "types_common.h"

#include "types_overlays.h"

#include "debug.h"
#include "color.h"
#include "float_bits.h"
#include "nj_strip.h"
#include "node_draw_state.h"
#include "player.h"
#include "task.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "scr_filter.h"
#include "system.h"

extern int ulMatrixPush(void);

extern void *scr_root;
extern struct TaskDesc scr_filter_class;
extern struct TaskDesc scr_filter_mng_class;

/* Four 5-vertex strips of three 30-degree segments; alpha inner at centre, outer at rim. */
/* 0x00b124f8 stg01, 0x00b1d788 stg02, 0x00b168e0 stg03, 0x00b1cba0 stg04, 0x00b29198 stg05, 0x00b25e68 stg22, 0x00b02140 stg23 */
void scr_filter_draw_disc(float x, float y, float z, float r, int rot, unsigned int color, int inner,
                          int outer)
{
    TrailVtx v[8];
    float s, c;
    unsigned int col_in, col_out;
    int i;
    TrailVtx *p;

    draw_set_blend(4, 1);
    p = v;
    for (i = 8; i != 0; i--) {
        p->q = z;
        p++;
    }
    col_in = color & 0xffffff | (inner + 1) >> 1 << 24;
    col_out = color & 0xffffff | (outer + 1) >> 1 << 24;
    p = v;
    p->c = col_out;
    p++;
    p->c = col_out;
    p++;
    p->c = col_in;
    p++;
    p->c = col_out;
    p++;
    p->c = col_out;
    ulSinCos(rot, &s, &c);
    nj_color_strip_begin(1);
    for (i = 4; i != 0; i--) {
        p = v;
        p->x = x + r * c;
        p->y = y - r * s;
        p++;
        ulSinCos(rot += 5461, &s, &c);
        p->x = x + r * c;
        p->y = y - r * s;
        p++;
        p->x = x;
        p->y = y;
        p++;
        ulSinCos(rot += 5461, &s, &c);
        p->x = x + r * c;
        p->y = y - r * s;
        p++;
        ulSinCos(rot += 5461, &s, &c);
        p->x = x + r * c;
        p->y = y - r * s;
        nj_color_strip_vertices(v, 5);
    }
    nj_color_strip_end();
}

/* 0x00b12758 stg01, 0x00b1d9e8 stg02, 0x00b16b40 stg03, 0x00b1ce00 stg04, 0x00b293f8 stg05, 0x00b260c8 stg22, 0x00b023a0 stg23 */
void scr_filter_init(ScrFilterWork *w)
{
    w->dist = 400.0f;
    w->inner = 40;
    w->size = 420.0f;
    w->color = COL_WHITE;
    w->outer = 0;
    w->frames = 0;
    task_set_dlist(w, 6);
}

/* 0x00b12798 stg01, 0x00b1da28 stg02, 0x00b16b80 stg03, 0x00b1ce40 stg04, 0x00b29438 stg05, 0x00b26108 stg22, 0x00b023e0 stg23 */
void scr_filter_release(void)
{
}

/* 0x00b127a0 stg01, 0x00b1da30 stg02, 0x00b16b88 stg03, 0x00b1ce48 stg04, 0x00b29440 stg05, 0x00b26110 stg22, 0x00b023e8 stg23 */
void scr_filter_exec(ScrFilterWork *w)
{
    int n = w->frames;
    float rate;

    w->rot += 1092;
    if (n > 0) {
        rate = 1.0f / n;
        w->frames = n - 1;
        w->dist += (w->tdist - w->dist) * rate;
        w->size += (w->tsize - w->size) * rate;
        w->inner = w->inner + (w->tinner - w->inner) * rate;
        w->outer = w->outer + (w->touter - w->outer) * rate;
        w->color = color_blend(w->color, w->tcolor, rate);
    }
}

/* 0x00b12898 stg01, 0x00b1db28 stg02, 0x00b16c80 stg03, 0x00b1cf40 stg04, 0x00b29538 stg05, 0x00b26208 stg22, 0x00b024e0 stg23 */
void scr_filter_disp(ScrFilterWork *w)
{
    Vec3 v;
    float x, y, r;

    if (!w->inner && !w->outer)
        return;
    ulMatrixPush();
    scr_filter_rotate_yxz(w->ang);
    v.x = 0;
    v.y = 0;
    v.z = -w->dist;
    ulMatrixApplyVector((float *)ulMatrixCur, &v.x, &v.x);
    w->depth = project_point(&v, &v);
    ulMatrixPop();
    w->sx = v.x;
    w->sy = v.y;
    player_get_frame_ang(w->ang);
    if (0.0f < w->depth && w->depth <= 1.0f) {
        x = w->sx;
        y = w->sy;
        r = w->size;
        if (x < -680.0f || 1320.0f < x || y < -760.0f || 1240.0f < y)
            return;
        if (x - r <= -680.0f)
            r = x - -680.0f;
        if (1320.0f <= x + r)
            r = 1320.0f - x;
        if (y - r <= -760.0f)
            r = y - -760.0f;
        if (1240.0f <= y + r)
            r = 1240.0f - y;
        scr_filter_draw_disc(x, y, float_add_bits16(1, 1.0526316f), r, w->rot, w->color, w->inner, w->outer);
    }
}

/* 0x00b12a70 stg01, 0x00b1dd00 stg02, 0x00b16e58 stg03, 0x00b1d118 stg04, 0x00b29710 stg05, 0x00b263e0 stg22, 0x00b026b8 stg23 */
void *scr_filter_create(void *parent)
{
    return task_create(parent, &scr_filter_class, 84);
}

/* 0x00b12a98 stg01, 0x00b1dd28 stg02, 0x00b16e80 stg03, 0x00b1d140 stg04, 0x00b29738 stg05, 0x00b26408 stg22, 0x00b026e0 stg23 */
void scr_filter_mng_init(ScrFilterWork **mng)
{
    *mng = scr_filter_create(scr_root);
}

/* 0x00b12ac8 stg01, 0x00b1dd58 stg02, 0x00b16eb0 stg03, 0x00b1d170 stg04, 0x00b29768 stg05, 0x00b26438 stg22, 0x00b02710 stg23 */
void scr_filter_mng_release(ScrFilterWork **mng)
{
    ScrFilterWork *flt = *mng;

    if (flt)
        TASK_OF(flt)->flags |= TASK_F_END;
}

/* 0x00b12ae8 stg01, 0x00b1dd78 stg02, 0x00b16ed0 stg03, 0x00b1d190 stg04, 0x00b29788 stg05, 0x00b26458 stg22, 0x00b02730 stg23 */
void scr_filter_mng_exec(ScrFilterWork **mng)
{
    if (*mng == 0 || (TASK_OF(*mng)->flags & TASK_F_END))
        TASK_OF(mng)->flags |= TASK_F_END;
}

/* 0x00b12b18 stg01, 0x00b1dda8 stg02, 0x00b16f00 stg03, 0x00b1d1c0 stg04, 0x00b297b8 stg05, 0x00b26488 stg22, 0x00b02760 stg23 */
void *scr_filter_mng_create(void *parent)
{
    return task_create(parent, &scr_filter_mng_class, 4);
}

/* 0x00b12b40 stg01, 0x00b1ddd0 stg02, 0x00b16f28 stg03, 0x00b1d1e8 stg04, 0x00b297e0 stg05, 0x00b264b0 stg22, 0x00b02788 stg23 */
void scr_filter_fade(void *fader, void *to, int frames)
{
    ScrFilterWork *flt;
    ScrFilterTo *tgt = to;

    if (fader && !(TASK_OF(fader)->flags & TASK_F_END) && (flt = *(ScrFilterWork **)fader)
        && !(TASK_OF(flt)->flags & TASK_F_END)) {
        flt->tdist = tgt->dist;
        flt->tsize = tgt->size;
        flt->tinner = tgt->inner;
        flt->touter = tgt->outer;
        flt->tcolor = tgt->color;
        if (frames < 1)
            frames = 1;
        flt->frames = frames;
    }
}

/* 0x00b12bb0 stg01, 0x00b1de40 stg02, 0x00b16f98 stg03, 0x00b1d258 stg04, 0x00b29850 stg05, 0x00b26520 stg22, 0x00b027f8 stg23 */
void scr_filter_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
