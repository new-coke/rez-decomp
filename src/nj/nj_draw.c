/* Ninja drawing: tree walk, shapes, points, screen and textured strips as GIF, cube clip test. */

#include "ninja.h"
#include "types_nj.h"
#include "libc.h"
#include "nj_draw.h"
#include "nj_cnk_gs.h"
#include "nj_matrix.h"
#include "nj_strip.h"
#include "ps_displist.h"
#include "ps_gs.h"
#include "ps_pkt.h"
#include "ul_gx.h"
#include "ul_texture.h"
#include "rez_wire_compile.h"

extern int vu0_points_any_clipped(void *m, NjVec4 *p, int n);
extern char nj_object_saved_matrix[];
extern const unsigned long nj_points_giftag_tmpl[2];
extern unsigned long nj_points_giftag[2];
extern void vu0_matrix_apply(void *m, NjVec4 *in, NjVec4 *out);
extern void nj_object_walk(NjObject *obj, int (*draw)(void *model));

/* Any corner of the cube (half-size radius) outside nj_clip_matrix's volume. */
/* 0x001a3910 */
int nj_cube_clipped(NjVec3 *center, float radius)
{
    NjVec4 corners[8];

    corners[0].x = corners[1].x = corners[4].x = corners[5].x = center->x - radius;
    corners[2].x = corners[3].x = corners[6].x = corners[7].x = center->x + radius;
    corners[0].y = corners[2].y = corners[4].y = corners[6].y = center->y + radius;
    corners[1].y = corners[3].y = corners[5].y = corners[7].y = center->y - radius;
    corners[0].z = corners[1].z = corners[2].z = corners[3].z = center->z - radius;
    corners[4].z = corners[5].z = corners[6].z = corners[7].z = center->z + radius;
    return vu0_points_any_clipped(nj_clip_matrix, corners, 8);
}

/* Half-pixel shifted to the pixel centre; uploaded to VU1. */
/* 0x001a39c0 */
void nj_set_screen_offset(float x, float y)
{
    NjSys *sys = &ulGxState;
    void *dlist, *pkt;

    sys->screen_ofs.x = x + 0.5f;
    sys->screen_ofs.y = y + 0.5f;
    dlist = psGsGetDisplayList();
    pkt = psDisplayListOpenRefPkt(dlist);
    psVifPktAddWord(pkt, 0x01000101);
    psVifPktAddWord(pkt, 0x6c010050);
    psVifPktAddWords(pkt, &sys->screen_ofs, 4);
    psDisplayListCloseRefPkt(dlist);
}

/* 0x001a3a58 */
static int nj_shape_draw_thunk(int shape)
{
    return nj_shape_draw(shape);
}

/* GS chunk-model renderer. */
/* 0x001a3a78 */
static void nj_object_draw_gs(int obj)
{
    nj_object_walk(obj, nj_cnk_gs_draw);
}

/* Current matrix as the model matrix; returns 0. */
/* 0x001a3a88 */
int nj_shape_draw(int shape)
{
    ulGxSetMatrix(0, ulMatrixCur);
    ulGxDrawShape(shape, 0);
    return 0;
}

/* 0x001a3ac8 */
void nj_object_draw_shapes(int obj)
{
    nj_object_walk(obj, nj_shape_draw);
}

/*
 * Per node: translate/rotate/scale on a pushed matrix (eval flags skip), draw, children. Nodes may
 * save to nj_object_saved_matrix, hide the model or cut children (always or on draw != 0).
 */
/* steer(cast) */
/* 0x001a3ad8 */
void nj_object_walk(NjObject *obj, int (*draw)(void *model))
{
    unsigned int fl;
    int ret;

    do {
        fl = obj->evalflags;
        ret = 0;
        if (fl & 0x800)
            njGetMatrix(nj_object_saved_matrix);
        if ((fl & 7) == 7) {
            if (!(fl & 8) && obj->model)
                ret = draw(obj->model);
            if (!(fl & 0x10)) {
                if (!(fl & 0x100) || !ret)
                    nj_object_walk(obj->child, draw);
            }
        } else {
            njPushMatrixEx();
            if (!(fl & 1))
                njTranslateEx(obj->pos);
            if (!(fl & 2)) {
                if (!(fl & 0x1000)) {
                    if (!(fl & 0x400))
                        ((void (*)(int *, int))nj_matrix_rotate_cur_ang3)(obj->ang, fl & 0x20);
                }
            }
            if (!(fl & 4))
                njScaleEx(obj->scl);
            if (!(fl & 8) && obj->model)
                ret = draw(obj->model);
            if (!(fl & 0x10)) {
                if (!(fl & 0x100) || !ret) {
                    if (obj->child)
                        nj_object_walk(obj->child, draw);
                }
            }
            njPopMatrixEx();
        }
        obj = obj->sibling;
    } while (obj);
}

/* 0x001a3c68 */
static void nj_draw_nop(void)
{
}

/* 0x001a3c70 */
static void nj_draw_nop_2(void)
{
}

/* Projected on the EE (screen matrix); transparency bit blends; alpha halved (128 opaque). */
/* 0x001a3c78 */
static void nj_draw_points(NjPointList *pl, int n, unsigned int flags)
{
    NjSys *sys = &ulGxState;
    void *dlist, *pkt;
    NjPkt *p;
    int i;
    int fl;
    unsigned int col;
    NjVec3 *vec;
    NjVec4 pos;
    float inv_w;

    fl = 0; if (flags & 0x40) fl = 1;
    ulGxSetMatrix(0, ulMatrixCur);
    nj_points_giftag[0] = nj_points_giftag_tmpl[0] | ((unsigned long)n | ((unsigned long)fl << 53));
    nj_points_giftag[1] = nj_points_giftag_tmpl[1];
    dlist = psGsGetDisplayList();
    pkt = psDisplayListOpenRefPkt(dlist);
    psVifPktOpenGifPkt(pkt);
    p = psVifPktAddGifTag(pkt, nj_points_giftag) + 1;
    for (i = 0; i < n; i++) {
        col = pl->col[i];
        p->i[3] = ((col >> 24) + 1) >> 1;
        p->i[0] = (col >> 16) & 0xff;
        p->i[1] = (col >> 8) & 0xff;
        p->i[2] = col & 0xff;
        p++;
        inv_w = 1.0f;
        pos.x = pl->pts[i].x;
        pos.y = pl->pts[i].y;
        pos.z = pl->pts[i].z;
        pos.w = inv_w;
        vu0_matrix_apply((char *)sys + 0x140, &pos, &pos);
        inv_w = inv_w / pos.w;
        p->i[0] = (int)(pos.x * inv_w * 16.0f);
        p->i[1] = (int)(pos.y * inv_w * 16.0f);
        p->i[2] = (int)(pos.z * inv_w * 16.0f);
        p->i[3] = 0;
        p++;
    }
    psVifPktAdvanceGif(pkt, p, n * 2);
    psVifPktCloseGifPkt(pkt);
    psDisplayListCloseRefPkt(dlist);
}

/* 0x001a3e80 */
void nj_tex_strip_draw(void *verts, int n, void *trans)
{
    nj_tex_strip_begin(trans);
    nj_tex_strip_vertices(verts, n);
    nj_tex_strip_end();
}

/* Screen-space, Gouraud-shaded. */
/* 0x001a3ec0 */
void nj_screen_strip_draw(NjColVtx *vtx, int n, int trans)
{
    NjSysView *sys = NJ_SYS_VIEW;
    void *dlist, *pkt;
    unsigned long tag[2];
    int *p, *pos;
    int i, abe;

    dlist = psGsGetDisplayList();
    pkt = psDisplayListOpenRefPkt(dlist);
    psVifPktOpenGifPkt(pkt);
    psDisplayListFlushGsRegs(dlist, pkt);
    abe = 0;
    if (trans)
        abe = 1;
    tag[0] = NJ_GIFTAG(n, 1, 1, NJ_PRIM(4, 1, 0, 0, abe, 0, 0, 0, 0), 0, 2);
    tag[1] = 0x41;
    p = (int *)(psVifPktAddGifTag(pkt, tag) + 1);
    for (i = 0; i < n; i++) {
        p[3] = (unsigned int)(vtx->a + 1) >> 1;
        p[0] = vtx->r;
        p[1] = vtx->g;
        p[2] = vtx->b;
        pos = p + 4;
        pos[0] = (int)((vtx->x * sys->scr_scale_x + sys->scr_org_x) * 16.0f);
        pos[1] = (int)((vtx->y * sys->scr_scale_y + sys->scr_org_y) * 16.0f);
        pos[2] = (int)(vtx->z * sys->depth_scale * 16.0f);
        pos[3] = 0;
        p += 8;
        vtx++;
    }
    psVifPktAdvanceGif(pkt, p, n * 2);
    psVifPktCloseGifPkt(pkt);
    psDisplayListCloseRefPkt(dlist);
}

/* Screen-space, texture texno (complains if not loaded); perspective UV, colours halved. */
/* 0x001a4070 */
static void njDrawTexture(NjTexVtx *verts, int n, int texno, int trans)
{
    NjSysView *sys = NJ_SYS_VIEW;
    void *tex, *dlist, *pkt;
    NjPkt *p;
    NjTexVtx *vtx;
    int i, abe;

    tex = ulTextureFindUsedById(texno);
    if (tex)
        ulGxSetTexture(tex);
    else
        printf("njDrawTexture: not found texno! [%d]\n", texno);
    abe = trans != 0;
    nj_points_giftag[0] = NJ_GIFTAG(n, 0, 1, NJ_PRIM(4, 1, 1, 0, abe, 0, 0, 0, 0), 0, 3);
    nj_points_giftag[1] = 0x412;
    dlist = psGsGetDisplayList();
    pkt = psDisplayListOpenRefPkt(dlist);
    psVifPktOpenGifPkt(pkt);
    psDisplayListFlushGsRegs(dlist, pkt);
    p = psVifPktAddGifTag(pkt, nj_points_giftag) + 1;
    vtx = verts;
    for (i = 0; i < n; i++) {
        p->f[0] = vtx->u * vtx->z;
        p->f[1] = vtx->v * vtx->z;
        p->f[2] = vtx->z;
        p++;
        p->u[3] = (unsigned int)(vtx->a + 1) >> 1;
        p->u[0] = (unsigned int)(vtx->r + 1) >> 1;
        p->u[1] = (unsigned int)(vtx->g + 1) >> 1;
        p->u[2] = (unsigned int)(vtx->b + 1) >> 1;
        p++;
        p->i[0] = (int)((vtx->x * sys->scr_scale_x + sys->scr_org_x) * 16.0f);
        p->i[1] = (int)((vtx->y * sys->scr_scale_y + sys->scr_org_y) * 16.0f);
        p->i[2] = (int)(vtx->z * sys->depth_scale * 16.0f);
        p->i[3] = 0;
        p++;
        vtx++;
    }
    psVifPktAdvanceGif(pkt, p, n * 3);
    psVifPktCloseGifPkt(pkt);
    psDisplayListCloseRefPkt(dlist);
}
