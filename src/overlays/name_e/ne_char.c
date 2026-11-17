/*
 * Name entry screen: an entered letter, built from spinning tiles that settle into shape; a
 * dismissed letter shrinks as its tiles fly apart.
 */

#include "name_e.h"
#include "mathdef.h"
#include "approach.h"
#include "bs_glyph.h"
#include "color.h"
#include "heap.h"
#include "math_util.h"
#include "nj_strip.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "debug.h"
#include "ne_char.h"

extern struct TaskDesc ne_char_desc;
extern Vec3 ne_char_quad[4];
extern Vec3 ne_char_normal;
extern void ulMatrixPush(void);

/* 0x00af6c40 */
void ne_char_init(NeChar *w, NeCharArg *prm)
{
    Vec3 *p;
    int (*ap)[3];
    int n;
    float sc[2];
    float x, y, z;
    int rot;

    w->pos.x = prm->pos.x;
    w->life = prm->life;
    w->pos.y = prm->pos.y;
    w->color = prm->color;
    w->pos.z = prm->pos.z;
    w->scale = prm->scale;
    glyph_lookup(prm->ch, (void **)&w->verts, &w->count);
    if (w->count <= 0) {
        do { FINISH(w); } while (0);
        return;
    }
    w->pieces = heap_block_tag(game_malloc(w->count * 12), "ne_char.c", 87);
    w->angs = heap_block_tag(game_malloc(w->count * 12), "ne_char.c", 88);
    p = w->pieces;
    n = w->count;
    ap = w->angs;
    do {
        rot = (int)DEG_TO_ANG(RAND_FLOAT_B() * 360.0f);
        x = 0.0f;
        y = 0.0f;
        z = 0.0f;
        x = 5.0f;
        do {
            float old_x = x, old_y = y;
            ulSinCos(rot, &sc[0], &sc[1]);
            x = old_x * sc[1] - old_y * sc[0];
            y = old_x * sc[0] + old_y * sc[1];
        } while (0);
        p->x = x;
        p->y = y;
        p->z = z;
        p++;
        (*ap)[0] = (int)DEG_TO_ANG(RAND_FLOAT_B() * 720.0f);
        (*ap)[1] = (int)DEG_TO_ANG(RAND_FLOAT_B() * 720.0f);
        (*ap)[2] = (int)DEG_TO_ANG(RAND_FLOAT_B() * 720.0f);
        ap++;
    } while (--n);
}

/* 0x00af6e98 */
void ne_char_release(NeChar *w)
{
    heap_free(w->pieces, 0);
    heap_free(w->angs, 0);
}

/* 0x00af6ed0 */
void ne_char_exec_dismiss(NeChar *w)
{
    int zero_ang[3];
    int n;
    Vec3 *p;
    Vec3 zero_pos;
    float rate;
    int (*ap)[3];

    w->scale *= 0.95f;
    if (w->scale <= 1.0f) {
        w->scale = 0;
        FINISH_IF(w);
    }
    n = w->count;
    if (w->life > 0)
        rate = 1.0f / w->life;
    else
        rate = 1.0f;
    if (n > 0) {
        zero_ang[0] = 0;
        zero_ang[1] = 0;
        zero_ang[2] = 0;
        *(int *)&zero_pos.x = 0;
        *(int *)&zero_pos.y = 0;
        *(int *)&zero_pos.z = 0;
        p = w->pieces;
        ap = w->angs;
        do {
            vec3_approach(&zero_pos, p, rate);
            p++;
            ang3_approach(zero_ang, *ap, rate);
            ap++;
        } while (--n);
    }
}

/* 0x00af6fe8 */
void ne_char_exec(NeChar *w)
{
    int zero_ang[3];
    Vec3 *vtx, *p;
    int (*ap)[3];
    int n;
    float rate;

    if (w->life > 0)
        rate = 1.0f / w->life;
    else
        rate = 1.0f;
    n = w->count;
    if (n > 0) {
        zero_ang[0] = 0;
        zero_ang[1] = 0;
        zero_ang[2] = 0;
        vtx = w->verts;
        p = w->pieces;
        ap = w->angs;
        do {
            vec3_approach(vtx, p, 0.15f);
            vtx = (Vec3 *)((char *)vtx + 24);
            p++;
            ang3_approach(zero_ang, *ap, rate);
            ap++;
        } while (--n);
    }
    if (w->life > 0)
        w->life--;
}

/* 0x00af70e0 */
void ne_char_disp(NeChar *w)
{
    TrailVtx vtx[4], *vp;
    Vec3 scr, nrm, *scr_p;
    Vec3 *p, *corner;
    int (*ap)[3];
    int n, k, vis;
    unsigned int col;
    float x, y, z, scl, dmax, dmin, depth, sx, sy;

    n = w->count;
    if (n <= 0)
        return;
    x = w->pos.x;
    y = w->pos.y;
    z = w->pos.z;
    scl = w->scale;
    p = w->pieces;
    ap = w->angs;
    ulMatrixPush();
    dmax = 1.0f;
    dmin = 0.0f;
    ne_char_rotate_yxz(w->ang);
    do {
        ulMatrixPush();
        vis = 1;
        vu0_matrix_translate(ulMatrixCur, x + p->x * scl, y + p->y * scl, z);
        ne_char_rotate_yxz(*ap);
        ulMatrixApplyVector(ulMatrixCur, &ne_char_normal, &nrm);
        col = color_blend((color_get_a(w->color) + 1) >> 1 << 24 | 0x404040, w->color, (nrm.z + dmax) * 0.5f);
        vu0_matrix_scale(ulMatrixCur, w->scale, w->scale, w->scale);
        corner = ne_char_quad;
        vp = vtx;
        k = 4;
        scr_p = &scr;
        do {
            ulMatrixApplyPoint(ulMatrixCur, corner, scr_p);
            depth = project_point(scr_p, scr_p);
            do { sx = scr.x; sy = scr_p->y; } while (0);
            vp->x = sx;
            vp->y = sy;
            vp->q = depth;
            if (depth <= dmin || dmax <= depth) {
                vis = 0;
                break;
            }
            corner++;
            vp++;
        } while (--k);
        ulMatrixPop();
        nj_color_strip_begin(0);
        if (vis) {
            vtx[3].c = col;
            vtx[2].c = col;
            vtx[1].c = col;
            vtx[0].c = col;
            nj_color_strip_vertices(vtx, 4);
        }
        nj_color_strip_end();
        p++;
        ap++;
    } while (--n);
    ulMatrixPop();
}

/* 0x00af7338 */
void *ne_char_create(void *parent, NeCharArg *prm)
{
    return task_create_arg(parent, &ne_char_desc, 52, prm);
}

/* 0x00af7360 */
void ne_char_dismiss(NeChar *w)
{
    w->life = 30;
    TASK_OF(w)->exec = (void (*)(void *))ne_char_exec_dismiss;
}

/* 0x00af7378 */
void ne_char_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
