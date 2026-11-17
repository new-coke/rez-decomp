/*
 * Brahma's rail, Area 2: up to 200 points along the player's recorded path, ahead or behind, bent
 * by a shape table; drawn as a wide line, removed when its Brahma leaves.
 */

#include "stg02.h"
#include "mathdef.h"
#include "gw_varedit.h"
#include "lens.h"
#include "libc.h"
#include "math_util.h"
#include "player.h"
#include "task.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_brahma_rail.h"
#include "en_brahma_rail_manager.h"
#include "../common/enemy_vars.h"

extern ScreenCfg screen_cfg;
extern ParamRec en_brahma_rail_params;
extern VarDef en_brahma_rail_vardefs[7];
extern void (*en_brahma_rail_disp_table[])(int *w);
extern void (*en_brahma_rail_states[])(int *w);
extern struct TaskDesc en_brahma_rail_desc;

typedef struct EnBrahmaRailWork {
    int phase;
    int next;                   /* 0x04 */
    unsigned int step;          /* 0x08 */
    int next_step;              /* 0x0c */
    Vec3 pts[201];              /* 0x10: a ring of rail points */
    int ang[200][3];            /* 0x97c */
    Vec3 origin;                /* 0x12dc */
    int rotz;                   /* 0x12e8 */
    int kind;                   /* 0x12ec: the row of en_brahma_rail_shape_ofs / _rot */
    int head;                   /* 0x12f0: the first point */
    int cur;                    /* 0x12f4: the last point written */
    int lead;                   /* 0x12f8: path frames ahead of the player (< 0: behind) */
    int max;                    /* 0x12fc: the highest point index written */
    float t;                    /* 0x1300: shape phase, 0..100 */
    float speed;                /* 0x1304 */
    char pad_1308[4];
    int toggle;                 /* 0x130c */
    int flags;                  /* 0x1310: 1 = leaving */
} EnBrahmaRailWork;

extern int en_brahma_rail_shape_rot[][100];
extern Vec3 en_brahma_rail_shape_ofs[][100];

/* en_brahma_rail_params: name, label, values */
typedef struct EnBrahmaRailRec {
    char *name;
    char *label;                /* 0x04 */
    int use_custom;                   /* 0x08: draws with the colours and width below */
    int col[3];                 /* 0x0c */
    float uv[2];                /* 0x18 */
    int clip;                   /* 0x20: draws only the visible span */
} EnBrahmaRailRec;

#define EN_BRAHMA_RAIL_REC ((EnBrahmaRailRec *)&en_brahma_rail_params)
extern LineStyle en_brahma_rail_style;
extern LineStyle en_brahma_rail_custom_style;

/* 0x00afdb88 */
void en_brahma_rail_vars_register(void)
{
    VarDef def;
    VarDef *vd;
    int *fld;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&def, 0, sizeof def);
    def.name = en_brahma_rail_params.name;
    def.fmt = en_brahma_rail_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&def);
    vd = en_brahma_rail_vardefs;
    fld = en_brahma_rail_params.v;
    for (i = 0; i < 7; i++) {
        vd->ptr = fld;
        fld++;
        vd++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_brahma_rail_vardefs);
}

/* 0x00afdc38 */
void en_brahma_rail_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00afdc40 */
void en_brahma_rail_add_point(EnBrahmaRailWork *w)
{
    Vec3 ofs;
    Vec3 pos;
    int ang[3];
    Vec3 pt;
    int *rot_tbl;
    Vec3 *ofs_tbl;
    float t, frac;
    int k, next, rot;
    Vec3 *org;

    ofs_tbl = en_brahma_rail_shape_ofs[w->kind];
    rot_tbl = en_brahma_rail_shape_rot[w->kind];
    if (++w->cur == 200)
        w->cur = 0;
    w->max = MAX(w->cur, w->max);
    if (w->lead >= 0)
        player_pass_eval_a(&pos, ang, w->lead * 2);
    else
        en_brahma_rail_manager_get_past(&pos, ang, -w->lead * 2);
    w->t += 2.0f;
    if (100.0f <= w->t)
        w->t -= 100.0f;
    t = w->t;
    k = t;
    frac = t - k;
    next = (k + 1) % 100;
    v3_sub(&ofs, &ofs_tbl[next], &ofs_tbl[k]);
    ofs.x *= frac;
    ofs.y *= frac;
    ofs.z *= frac;
    v3_add(&ofs, &ofs_tbl[k]);
    rot = rot_tbl[k] + (rot_tbl[next] - rot_tbl[k]) * frac;
    rot = (rot & 0x8000) ? rot | 0xffff0000 : rot & 0xffff;
    ulMatrixPush();
    matrix_unit_set(0);
    org = &w->origin;
    do {
        Vec3 *p_ = org;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    vu0_matrix_rotate_z(ulMatrixCur, w->rotz);
    ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&ofs, (float *)&pt);
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_brahma_rail_rotate_yxz(ang);
    ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&pt, (float *)&w->pts[w->cur]);
    ulMatrixPop();
    i3_set(w->ang[w->cur], 0, 0, rot + ang[2]);
}

/* 0x00afdf20 */
void en_brahma_rail_state_lay(EnBrahmaRailWork *w)
{
    Vec3 shift;
    Vec3 *p;
    int i, n, still;

    switch (w->step) {
    case 0:
        *(int *)&w->t = 0;
        w->head = -1;
        w->cur = -1;
        w->lead = -80;
        w->step++;
        w->next_step++;
    case 1:
        player_get_frame_vel_old(&shift);
        still = fabsf(shift.x) < 0.01f && fabsf(shift.y) < 0.01f && fabsf(shift.z) < 0.01f;
        if (!still) {
            n = w->max;
            for (i = 0, p = w->pts; i <= n; i++, p++) {
                p->x += shift.x;
                p->y += shift.y;
                p->z += shift.z;
            }
        }
        if (w->flags & 1) {
            if ((w->toggle = (w->toggle + 1) % 2) == 0) {
                w->head = (w->head + 1) % 200;
                if (--w->lead < -79)
                    FINISH_IF(w);
            }
        } else {
            if ((w->toggle = (w->toggle + 1) % 2) == 0) {
                w->head = (w->head + 1) % 200;
                en_brahma_rail_add_point(w);
                if (w->lead < 120) {
                    w->lead++;
                    en_brahma_rail_add_point(w);
                }
            }
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00afe150 */
void en_brahma_rail_exec(int *w)
{
    EnBrahmaRailWork *rail = (EnBrahmaRailWork *)w;

    do {
        rail->phase = rail->next;
        rail->step = rail->next_step;
    } while (0);
    if (en_brahma_rail_states[rail->phase])
        en_brahma_rail_states[rail->phase](w);
}

/* 0x00afe1a0 */
int en_brahma_rail_test_point(void *wp, int i)
{
    EnBrahmaRailWork *w = wp;
    Vec3 spt;
    int vis;
    float x, y, z;
    int idx = i + 1;

    ulMatrixApplyPoint((float *)ulMatrixCur, &w->pts[(w->cur + idx) % 199].x, &spt.x);
    project_point(&spt, &spt);
    do { x = spt.x; } while (0);
    do { y = spt.y; } while (0);
    z = spt.z;
    vis = 0;
    if (0.0f < x && 0.0f < y) {
        float *cfg = (float *)&screen_cfg;

        if (x < cfg[10] && y < cfg[11] && 0.0f < z)
            vis = 1;
    }
    return vis;
}

/* 0x00afe278 */
int en_brahma_rail_find_visible(void *w, int lo, int hi, int *out)
{
    int mid;

    if (hi - lo < 5)
        return 0;
    mid = (lo + hi) / 2;
    if (en_brahma_rail_test_point(w, mid)) {
        *out = mid;
        return 1;
    }
    if (en_brahma_rail_find_visible(w, lo, mid, out))
        return 1;
    return en_brahma_rail_find_visible(w, mid, hi, out);
}

/* 0x00afe328 */
int en_brahma_rail_find_edge(char *w, int dir, int lo, int hi)
{
    int mid;

loop:
    mid = (lo + hi) / 2;
    if (mid == lo)
        goto done;
    if (en_brahma_rail_test_point(w, mid)) {
        if (dir)
            lo = mid;
        else
            hi = mid;
    } else {
        if (dir)
            hi = mid;
        else
            lo = mid;
    }
    goto loop;
done:
    {
        EnBrahmaRailWork *rail = (EnBrahmaRailWork *)w;
        int next = mid + 1;

        return (rail->cur + next) % rail->max;
    }
}

/* 0x00afe3e0 */
int en_brahma_rail_calc_span(EnBrahmaRailWork *w, int *lo, int *hi)
{
    int max = w->max;
    int cur = w->cur;
    int first = 0;
    int mid;

    if (cur != max)
        first = cur + 1;
    if (en_brahma_rail_test_point(w, 0)) {
        *lo = first;
        *hi = en_brahma_rail_find_edge((char *)w, 1, 0, max);
        return 1;
    }
    if (en_brahma_rail_test_point(w, max)) {
        *lo = en_brahma_rail_find_edge((char *)w, 0, 0, max);
        *hi = cur;
        return 1;
    }
    if (!en_brahma_rail_find_visible(w, 0, max, &mid))
        return 0;
    *lo = en_brahma_rail_find_edge((char *)w, 0, 0, mid);
    *hi = en_brahma_rail_find_edge((char *)w, 1, mid, max);
    return 1;
}

/* 0x00afe4f0 */
void en_brahma_rail_disp_ahead(EnBrahmaRailWork *w)
{
    Vec3 buf[60];
    int cur, n, k, from, to, wrap;

    v3_copy(&w->pts[200], w->pts);
    cur = w->cur;
    n = w->lead - 60;
    if (cur >= n) {
        if (EN_BRAHMA_RAIL_REC->clip && w->kind == 0) {
            int lo, hi;

            if (en_brahma_rail_calc_span(w, &lo, &hi)) {
                if (lo < hi) {
                    if (cur < lo || hi <= cur - n) {
                        draw_polyline_wide_nocap(&en_brahma_rail_style, &w->pts[lo], hi - lo);
                    } else {
                        if (lo < cur - n)
                            draw_polyline_wide_nocap(&en_brahma_rail_style, &w->pts[lo], cur - n - lo);
                        if (cur + 1 < hi)
                            draw_polyline_wide_nocap(&en_brahma_rail_style, &w->pts[cur + 1], hi - cur - 1);
                    }
                } else {
                    if (hi <= cur) {
                        draw_polyline_wide_nocap(&en_brahma_rail_style, w->pts, hi < cur - n ? hi : cur - n);
                    } else {
                        draw_polyline_wide_nocap(&en_brahma_rail_style, w->pts, cur - n);
                        draw_polyline_wide_nocap(&en_brahma_rail_style, &w->pts[cur + 1], hi - cur - 1);
                    }
                    if (cur < lo) {
                        draw_polyline_wide_nocap(&en_brahma_rail_style, &w->pts[lo], 200 - lo);
                    } else {
                        if (lo < cur - n)
                            draw_polyline_wide_nocap(&en_brahma_rail_style, &w->pts[lo], cur - n - lo);
                        draw_polyline_wide_nocap(&en_brahma_rail_style, &w->pts[cur + 1], 199 - cur);
                    }
                }
            }
        } else {
            draw_polyline_wide_nocap(&en_brahma_rail_style, w->pts, cur - n);
            draw_polyline_wide_nocap(&en_brahma_rail_style, &w->pts[cur + 1], 199 - cur);
        }
        {
            Vec3 *dst = buf, *p;

            p = &w->pts[cur - n];
            if ((cur - n) & 1) {
                v3_copy(dst++, p++);
                n--;
            }
            for (k = n / 2; k >= 0; k--) {
                v3_copy(dst, p);
                dst++;
                p += 2;
            }
            if (n & 1)
                v3_copy(dst++, &w->pts[cur]);
            draw_polyline_wide_nocap(&en_brahma_rail_style, buf, dst - buf - 1);
        }
    } else {
        if (EN_BRAHMA_RAIL_REC->clip && w->kind == 0) {
            int lo, hi;

            if (en_brahma_rail_calc_span(w, &lo, &hi)) {
                if (lo < hi) {
                    from = lo < cur + 1 ? cur + 1 : lo;
                    to = cur + 199;
                    to -= n;
                    if (hi < to)
                        to = hi;
                    draw_polyline_wide_nocap(&en_brahma_rail_style, &w->pts[from], to - from);
                } else {
                    if (cur + 1 < hi)
                        draw_polyline_wide_nocap(&en_brahma_rail_style, &w->pts[cur + 1], hi - cur - 1);
                    {
                        int end = cur + 200;

                        if (lo < end - n)
                            draw_polyline_wide_nocap(&en_brahma_rail_style, &w->pts[lo], end - n - lo);
                    }
                }
            }
        } else {
            draw_polyline_wide_nocap(&en_brahma_rail_style, &w->pts[cur + 1], 199 - n);
        }
        {
            Vec3 *dst = buf, *p;

            p = w->pts;
            for (k = cur / 2; k >= 0; k--) {
                v3_set(dst, p->x, p->y, p->z);
                dst++;
                p += 2;
            }
            if (cur & 1)
                v3_copy(dst++, &w->pts[cur]);
            draw_polyline_wide_nocap(&en_brahma_rail_style, buf, dst - buf - 1);
        }
        {
            Vec3 *dst = buf, *p;

            wrap = cur + 200;
            p = &w->pts[wrap - n];
            if ((wrap - n) & 1) {
                v3_copy(dst++, p++);
                n--;
            }
            for (k = (n - cur) / 2; k >= 0; k--) {
                v3_copy(dst, p);
                dst++;
                p += 2;
            }
            if ((n - cur) & 1)
                v3_copy(dst++, &w->pts[200]);
            draw_polyline_wide_nocap(&en_brahma_rail_style, buf, dst - buf - 1);
        }
    }
}

/* 0x00afead8 */
void en_brahma_rail_disp_line(EnBrahmaRailWork *w)
{
    EnBrahmaRailRec *rec = EN_BRAHMA_RAIL_REC;
    LineStyle *style = &en_brahma_rail_style;
    int lo, hi;
    int cur;

    if (rec->use_custom) {
        en_brahma_rail_custom_style.col[0] = rec->col[0];
        en_brahma_rail_custom_style.col[1] = rec->col[1];
        en_brahma_rail_custom_style.col[2] = rec->col[2];
        en_brahma_rail_custom_style.width = rec->uv[0];
        en_brahma_rail_custom_style.rate_in = rec->uv[1];
        en_brahma_rail_custom_style.unk_0c = 0;
        en_brahma_rail_custom_style.unk_18[0] = 0;
        en_brahma_rail_custom_style.unk_18[1] = 0;
    } else if (w->lead < 61) {
        if (rec->clip && w->kind == 0) {
            if (!en_brahma_rail_calc_span(w, &lo, &hi))
                return;
            cur = w->cur;
            if (lo < hi) {
                if (cur < lo || hi <= cur) {
                    draw_polyline_wide_nocap(style, &w->pts[lo], hi - lo);
                } else {
                    draw_polyline_wide_nocap(style, &w->pts[lo], cur - lo);
                    draw_polyline_wide_nocap(style, &w->pts[cur + 1], hi - cur - 1);
                }
            } else {
                v3_copy(&w->pts[200], &w->pts[0]);
                if (hi <= cur) {
                    draw_polyline_wide_nocap(style, w->pts, hi);
                } else {
                    draw_polyline_wide_nocap(style, w->pts, cur);
                    draw_polyline_wide_nocap(style, &w->pts[cur + 1], hi - cur - 1);
                }
                if (cur < lo) {
                    draw_polyline_wide_nocap(style, &w->pts[lo], 200 - lo);
                } else {
                    draw_polyline_wide_nocap(style, &w->pts[lo], cur - lo);
                    draw_polyline_wide_nocap(style, &w->pts[cur + 1], 199 - cur);
                }
            }
        } else {
            draw_polyline_wide_nocap(style, w->pts, w->cur);
            if (w->max == 199) {
                v3_copy(&w->pts[200], &w->pts[0]);
                draw_polyline_wide_nocap(style, &w->pts[w->cur + 1], w->max - w->cur);
            }
        }
    } else {
        en_brahma_rail_disp_ahead(w);
    }
}

/* 0x00afed58 */
void en_brahma_rail_disp(int *w)
{
    void (*fn)(int *w) = en_brahma_rail_disp_table[*w];

    if (fn)
        fn(w);
}

/* 0x00afed98 */
void en_brahma_rail_init(char *w, float x, float y, int rotz, int kind)
{
    EnBrahmaRailWork *rail = (EnBrahmaRailWork *)w;
    Vec3 pvel;
    int n;

    { int kpos = MAX_GE(kind, 0); n = kpos < 4 ? kpos : 3; }
    rail->origin.x = x;
    rail->origin.y = y;
    rail->kind = n;
    rail->rotz = rotz;
    player_get_vec(&pvel);
    {
        float len = vec3_length(&pvel);

        rail->speed = 1.0f / (len + len);
    }
    task_set_dlist(w, 6);
    rail->next_step = 0;
    rail->next = 0;
}

/* 0x00afee20 */
void en_brahma_rail_release(void)
{
}

/* 0x00afee28 */
void *en_brahma_rail_create(void *parent, int rotz, int kind, float x, float y)
{
    char *w = task_create(parent, &en_brahma_rail_desc, 4884);

    en_brahma_rail_init(w, x, y, rotz, kind);
    return w;
}

/* 0x00afeea8 */
void en_brahma_rail_leave(int *w)
{
    ((EnBrahmaRailWork *)w)->flags |= 1;
}

/* 0x00afeeb8 */
int en_brahma_rail_sample(void *wp, float t, Vec3 *pos, int *ang)
{
    EnBrahmaRailWork *w = wp;
    Vec3 dir;
    Vec3 seg;
    Vec3 off;
    float fpos, frac, end, roll;
    int k, cur, nxt, past;

    fpos = (float)w->head + t * w->speed + 80.0f + w->toggle * 0.5f;
    k = fpos;
    frac = fpos - (float)k;
    cur = k % 200;
    nxt = (cur + 1) % 200;
    past = 0;
    if (w->head < w->cur)
        end = w->cur;
    else
        end = w->cur + 200;
    if (end <= fpos)
        past = 1;
    if (past) {
        i3_copy(ang, w->ang[w->cur]);
        v3_copy(pos, &w->pts[w->cur]);
        dir.x = 0.0f;
        dir.y = 10.0f;
        do { dir.z = 0.0f; ulMatrixPush(); } while (0);
        matrix_unit_set(0);
        vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
        do { vu0_matrix_rotate_z(ulMatrixCur, ang[2]); } while (0);
        ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&dir, (float *)pos);
        ulMatrixPop();
        return (w->flags & 3) != 1;
    }
    if (fabsf(frac) < FLT_EPSILON) {
        v3_copy(pos, &w->pts[cur]);
        v3_sub(&dir, &w->pts[nxt], &w->pts[cur]);
        do {
            ang[1] = (int)RAD_TO_ANG(atan2f(-dir.x, -dir.z));
            ang[0] = (int)RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
        } while (0);
        ang[2] = -w->ang[cur][2];
    } else {
        v3_sub(&seg, &w->pts[nxt], &w->pts[cur]);
        seg.x *= frac;
        seg.y *= frac;
        seg.z *= frac;
        v3_sum(pos, &w->pts[cur], &seg);
        do {
            ang[1] = (int)RAD_TO_ANG(atan2f(-seg.x, -seg.z));
            ang[0] = (int)RAD_TO_ANG(atan2f(seg.y, sqrtf(seg.x * seg.x + seg.z * seg.z)));
        } while (0);
        roll = w->ang[cur][2] + (w->ang[nxt][2] - w->ang[cur][2]) * frac;
        ang[2] = -roll;
        if (ang[2] & 0x8000)
            ang[2] |= 0xffff0000;
        else
            ang[2] &= 0xffff;
    }
    v3_set(&off, 0.0f, 10.0f, 0.0f);
    ulMatrixPush();
    matrix_unit_set(0);
    do { vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z); } while (0);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
    ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&off, (float *)pos);
    ulMatrixPop();
    return 2;
}

/* 0x00aff3b0 */
void en_brahma_rail_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
