/*
 * Vishnu's strings, Area 2: 16-point ropes from Vishnu to each hand, drawn as lines; shrink once
 * the hand is gone, fall away without Vishnu.
 */

#include "stg02.h"
#include "gw_varedit.h"
#include "lens.h"
#include "libc.h"
#include "math_util.h"
#include "player.h"
#include "task.h"
#include "en_vishnu_string.h"
#include "en_vishnu.h"
#include "en_vishnu_hand.h"
#include "../common/enemy_vars.h"

extern float en_vishnu_string_line_len;
extern ParamRec en_vishnu_string_params;
extern VarDef en_vishnu_string_vardefs[9];
extern float en_vishnu_string_rate_rebound;
extern struct TaskDesc en_vishnu_string_desc;

/* copied from en_vishnu_string_params when enabled */
typedef struct EnVishnuStringStyle {
    int pcol_center;
    int pcol_out1;                      /* 0x04 */
    float r_all;                    /* 0x08 */
} EnVishnuStringStyle;

extern EnVishnuStringStyle en_vishnu_string_custom_style;
extern char en_vishnu_string_style[];

/* 16 points from the Vishnu node to the hand */
typedef struct EnVishnuStringWork {
    void *owner;                /* en_vishnu object */
    char *hand;                 /* 0x04: the hand work (en_vishnu_hand) */
    int idx;                    /* 0x08: the hand's slot on the owner (en_vishnu_get_hand_pos) */
    float len;                  /* 0x0c: shrinks after the hand is gone */
    Vec3 vel;                   /* 0x10 */
    Vec3 pts[16];               /* 0x1c */
    float t;                    /* 0xdc: frames falling without an owner */
} EnVishnuStringWork;

#define V3_GET(v, vx, vy, vz) \
    do { \
        vx = (v)->x; \
        vy = (v)->y; \
        vz = (v)->z; \
    } while (0)

/* 0x00b113c0 */
void en_vishnu_string_vars_register(void)
{
    VarDef hdr;
    VarDef *def;
    int *val;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&hdr, 0, sizeof hdr);
    hdr.name = en_vishnu_string_params.name;
    hdr.fmt = en_vishnu_string_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&hdr);
    def = en_vishnu_string_vardefs;
    val = en_vishnu_string_params.v;
    for (i = 0; i < 9; i++) {
        def->ptr = val;
        val++;
        def++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_vishnu_string_vardefs);
}

/* 0x00b11470 */
void en_vishnu_string_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b11478 */
void en_vishnu_string_init_points(void *work)
{
    EnVishnuStringWork *w = work;
    Vec3 *first = &w->pts[0];
    Vec3 *last = &w->pts[15];
    Vec3 step;
    Vec3 *p, *nxt;
    int i;

    en_vishnu_get_hand_pos(w->owner, w->idx, first);
    en_vishnu_hand_get_string_pos(w->hand, last);
    step.x = (last->x - first->x) * 0.06666667f;
    step.y = (last->y - first->y) * 0.06666667f;
    step.z = (last->z - first->z) * 0.06666667f;
    for (i = 14, p = first, nxt = &w->pts[1]; i != 0; i--, p++, nxt++) {
        nxt->x = p->x + step.x;
        nxt->y = p->y + step.y;
        nxt->z = p->z + step.z;
    }
}

static inline float string_param(ParamRec *r, int i)
{
    return *(float *)&r->v[i];
}

/* 0x00b11568 */
void en_vishnu_string_move(EnVishnuStringWork *w)
{
    Vec3 prev[16];
    Vec3 shift;
    Vec3 ofs;
    Vec3 *p, *nxt, *pa;
    float *src = &w->pts[0].x;
    float *dst = &prev[1].x;
    Vec3 *shift_p = &shift;
    float seg, lim, rate, dsq, x, y, z, sx, sy, sz;
    int i;

    player_get_frame_vel(&shift);
    v3_add(&shift, &w->vel);
    V3_GET(shift_p, x, y, z);
    if (!w->owner) {
        float grav = string_param(&en_vishnu_string_params, 4);

        w->t += 1.0f;
        y += grav * 0.00027777778f * (0.5f - w->t);
    }
    sx = *src++;
    sy = *src++;
    sz = *src++;
    *--dst = sz;
    *--dst = sy;
    *--dst = sx;
    dst += 6;
    for (i = 14; i != 0; i--) {
        sx = *src++;
        sy = *src++;
        sz = *src++;
        sx += x;
        sy += y;
        sz += z;
        *--src = sz;
        *--src = sy;
        *--src = sx;
        src += 3;
        *--dst = sz;
        *--dst = sy;
        *--dst = sx;
        dst += 6;
    }
    sx = *src++;
    sy = *src++;
    sz = *src++;
    if (!w->owner || !w->hand) {
        sx += x;
        sz += z;
        sy += y;
        *--src = sz;
        *--src = sy;
        *--src = sx;
    }
    *--dst = sz;
    *--dst = sy;
    *--dst = sx;
    seg = w->len;
    lim = seg * 4.0f * seg;
    rate = string_param(&en_vishnu_string_params, 1);
    p = &w->pts[1];
    pa = prev;
    for (i = 14; i != 0; p++, pa++, i--) {
        nxt = pa + 2;
        dsq = vec3_distance_squared(pa, nxt);
        if (lim < dsq) {
            v3_sub(&ofs, nxt, pa);
            v3_scale(&ofs, seg / sqrtf(dsq));
            v3_sum(p, pa, &ofs);
            v3_sum(p + 1, p, &ofs);
        } else {
            float mx, my, mz, nx, ny, nz, px, py, pz;

            mx = pa->x;
            my = pa->y;
            mz = pa->z;
            V3_GET(nxt, nx, ny, nz);
            mx += nx;
            my += ny;
            mz += nz;
            mx *= 0.5f;
            my *= 0.5f;
            mz *= 0.5f;
            V3_GET(p, px, py, pz);
            mx -= px;
            my -= py;
            mz -= pz;
            mx *= rate;
            my *= rate;
            mz *= rate;
            px += mx;
            py += my;
            pz += mz;
            p->x = px;
            p->y = py;
            p->z = pz;
        }
    }
}

/* 0x00b118f8 */
void en_vishnu_string_reverse(void *w)
{
    Vec3 rev[16];
    Vec3 *vp;
    float *src;
    float *dst, *pts;
    float x, y, z;
    int i;

    pts = &((EnVishnuStringWork *)w)->pts[1].x;
    src = &((EnVishnuStringWork *)w)->pts[0].x;
    vp = &rev[16];
    dst = (float *)&rev[16];
    for (i = 16; i; i--) {
        x = src[0];
        y = src[1];
        z = src[2];
        vp--;
        *--dst = x;
        *--dst = y;
        *--dst = z;
        src += 3;
    }
    src = (float *)vp;
    vp = (Vec3 *)pts;
    src++;
    dst = (float *)vp;
    for (i = 16; i; i--) {
        x = src[-1];
        y = src[0];
        z = src[1];
        *--dst = x;
        *--dst = y;
        *--dst = z;
        dst += 6;
        src += 3;
    }
}

/* 0x00b11988 */
void en_vishnu_string_exec_normal(void *work)
{
    EnVishnuStringWork *w = work;
    Vec3 hpos;

    if (w->owner) {
        if (!(TASK_OF(w->owner)->flags & TASK_F_END))
            en_vishnu_get_hand_pos(w->owner, w->idx, &w->pts[0]);
        else
            en_vishnu_string_detach((int *)w);
    }
    if (w->hand && !(TASK_OF(w->hand)->flags & TASK_F_END)) {
        en_vishnu_hand_get_string_pos(w->hand, &hpos);
        if (w->owner)
            v3_copy(&w->pts[15], &hpos);
        else
            v3_copy(&w->pts[0], &hpos);
    } else {
        if (w->owner) {
            w->len *= *(float *)&en_vishnu_string_params.v[2];
            w->hand = 0;
        }
        if (!w->owner || w->len < 0.1f) {
            do {
                TASK_OF(w)->disp = 0;
                do { FINISH(w); } while (0);
            } while (0);
            return;
        }
    }
    v3_scale(&w->vel, 0.97f);
    en_vishnu_string_move(w);
    if (w->hand) {
        if (w->owner)
            v3_copy(&w->pts[15], &hpos);
        else
            v3_copy(&w->pts[0], &hpos);
    }
}

/* 0x00b11b08 */
void en_vishnu_string_exec(void *w)
{
    en_vishnu_string_init_points(w);
    TASK_OF(w)->exec = en_vishnu_string_exec_normal;
}

static inline void tri_set(EnVishnuStringStyle *style, int pcol_center, int pcol_out1, float r_all)
{
    style->pcol_center = pcol_center;
    style->pcol_out1 = pcol_out1;
    style->r_all = r_all;
}

/* 0x00b11b38 */
void en_vishnu_string_disp(char *w)
{
    void *src;
    float swap;

    src = en_vishnu_string_style;
    if (en_vishnu_string_params.v[5]) {
        tri_set(&en_vishnu_string_custom_style, en_vishnu_string_params.v[6], en_vishnu_string_params.v[7], *(float *)&en_vishnu_string_params.v[8]);
        src = &en_vishnu_string_custom_style;
    }
    swap = polyline_set_width_min(1.0f);
    draw_polyline_nocap(src, w + 28, 15);
    polyline_set_width_min(swap);
}

/* 0x00b11bd0 */
void en_vishnu_string_init(EnVishnuStringView *w, int owner, int hand, int idx, Vec3 *pos)
{
    w->owner = owner;
    w->hand = hand;
    w->idx = idx;
    v3_copy(&w->vel, pos);
    w->len = ((struct { float f; } *)&en_vishnu_string_line_len)->f;
    task_set_dlist(w, 6);
}

/* 0x00b11c10 */
void en_vishnu_string_release(void)
{
}

/* 0x00b11c18 */
EnVishnuStringView *en_vishnu_string_create(void *parent, int hand, int idx, Vec3 *pos)
{
    EnVishnuStringView *w = task_create_sibling(parent, &en_vishnu_string_desc, 224);

    en_vishnu_string_init(w, (int)parent, hand, idx, pos);
    return w;
}

/* 0x00b11c98 */
void en_vishnu_string_detach(int *w)
{
    en_vishnu_string_reverse(w);
    *w = 0;
}

/* 0x00b11cc0 */
float en_vishnu_string_max_len(void)
{
    return en_vishnu_string_line_len * 15.0f;
}

/* 0x00b11cd8 */
float en_vishnu_string_get_rebound(void)
{
    return en_vishnu_string_rate_rebound;
}
