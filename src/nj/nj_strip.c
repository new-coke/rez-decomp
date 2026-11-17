/*
 * Ninja strips in begin/vertices/end batches: textured, coloured and flat via VU1 (50 vertices per
 * call); clipped variants transform on VU0, send unclipped runs to the GS, clip crossings singly.
 */

#include "types_nj.h"
#include "ninja.h"
#include "nj_strip.h"
#include "ps_displist.h"
#include "ps_gs.h"
#include "ps_pkt.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"

typedef union NjQwordU {
    unsigned long ul[2];
    unsigned int ui[4];
} NjQwordU;

extern int ulGxPrimBits[];
extern unsigned int nj_flat_strip_color;
extern NjQwordU nj_strip_giftag;
extern int nj_vu0_emit_tex_vertices(NjSysView *sys, NjTexVtx *v, int n, NjQword *dst);
extern void nj_vu0_clip_tex_vertices(NjSysView *sys, NjTexVtx *v, int n, NjQword *dst);
extern NjPkt *vu0_clip_emit_tex_fog(NjSysView *sys, void *src, int n, NjQword *dst);
extern int nj_vu0_emit_color_vertices(NjSysView *sys, NjColVtx *v, int n, NjQword *dst);
extern void nj_vu0_clip_color_vertices(NjSysView *sys, NjColVtx *v, int n, NjQword *dst);
extern NjPkt *vu0_clip_emit_fog(NjSysView *sys, void *src, int n, NjQword *dst);

/* trans: alpha blend. GIF tag, display-list packet, changed GS settings, VU1 program start. */
/* 0x001a4290 */
void nj_tex_strip_begin(int trans)
{
    void *dlist, *pkt;
    int abe, prim;

    abe = 0;
    if (trans)
        abe = 0x40;
    abe |= 4;
    prim = ulGxPrimBits[0] | 0x10;
    nj_strip_giftag.ul[0] = NJ_GIFTAG(0, 1, 1, prim | abe, 0, 3);
    nj_strip_giftag.ul[1] = 0x412;
    dlist = psGsGetDisplayList();
    pkt = psDisplayListOpenRefPkt(dlist);
    if (psDisplayListIsDirty(dlist)) {
        psVifPktOpenGifPkt(pkt);
        psDisplayListFlushGsRegs(dlist, pkt);
        psVifPktCloseGifPkt(pkt);
    }
    psVifPktAddWord(pkt, 0x1500000c);
    psVifPktAddWord(pkt, 0x01000104);
}

/* Over 50 vertices are dropped. */
/* 0x001a4358 */
void nj_tex_strip_vertices(NjVtxUV *verts, int n)
{
    NjVifWr *pkt;
    NjVtxUV *vtx;
    unsigned int *tp, *xp, *cp, *up;
    int k, i;

    if (n <= 50) {
        pkt = psDisplayListGetVifPkt(psGsGetDisplayList());
        k = ulGxBeginVertexPkt(pkt, n, &tp, &xp, &cp, &up);
        vtx = verts;
        for (i = 0; i < n; i++) {
            *xp++ = *(unsigned int *)&vtx->x;
            *xp++ = *(unsigned int *)&vtx->y;
            *xp++ = *(unsigned int *)&vtx->z;
            *cp++ = vtx->col;
            *up++ = *(unsigned int *)&vtx->u;
            *up++ = *(unsigned int *)&vtx->v;
            vtx++;
        }
        *tp++ = nj_strip_giftag.ui[0] | n;
        *tp++ = nj_strip_giftag.ui[1];
        *tp++ = nj_strip_giftag.ui[2];
        *tp++ = nj_strip_giftag.ui[3];
        pkt->cur += k * 4;
        pkt->nwords += k;
    }
}

/* Lets VU1 finish, closes the packet. */
/* 0x001a44d0 */
void nj_tex_strip_end(void)
{
    void *dlist, *pkt;

    dlist = psGsGetDisplayList();
    pkt = psDisplayListGetVifPkt(dlist);
    psVifPktAddWord(pkt, 0x10000000);
    psVifPktAddWord(pkt, 0x01000101);
    psDisplayListCloseRefPkt(dlist);
}

/* Gouraud, untextured; as nj_tex_strip_begin. */
/* 0x001a4530 */
void nj_color_strip_begin(int trans)
{
    void *dlist, *pkt;
    int abe;

    abe = 0;
    if (trans)
        abe = 0x40;
    abe |= 4;
    nj_strip_giftag.ul[0] = NJ_GIFTAG(0, 1, 1, ulGxPrimBits[0] | abe, 0, 3);
    nj_strip_giftag.ul[1] = 0x412;
    dlist = psGsGetDisplayList();
    pkt = psDisplayListOpenRefPkt(dlist);
    if (psDisplayListIsDirty(dlist)) {
        psVifPktOpenGifPkt(pkt);
        psDisplayListFlushGsRegs(dlist, pkt);
        psVifPktCloseGifPkt(pkt);
    }
    psVifPktAddWord(pkt, 0x1500000c);
    psVifPktAddWord(pkt, 0x01000104);
}

/* At most 50. */
/* 0x001a45f8 */
void nj_color_strip_vertices(NjColVtx *verts, int n)
{
    NjVifWr *pkt;
    NjColVtx *vtx;
    unsigned int *tp, *xp, *cp;
    int k, i;

    if (n <= 50) {
        pkt = psDisplayListGetVifPkt(psGsGetDisplayList());
        k = ulGxBeginVertexPkt(pkt, n, &tp, &xp, &cp, 0);
        vtx = verts;
        for (i = 0; i < n; i++) {
            *xp++ = *(unsigned int *)&vtx->x;
            *xp++ = *(unsigned int *)&vtx->y;
            *xp++ = *(unsigned int *)&vtx->z;
            *cp++ = *(unsigned int *)&vtx->b;
            vtx++;
        }
        *tp++ = nj_strip_giftag.ui[0] | n;
        *tp++ = nj_strip_giftag.ui[1];
        *tp++ = nj_strip_giftag.ui[2];
        *tp++ = nj_strip_giftag.ui[3];
        pkt->cur += k * 4;
        pkt->nwords += k;
    }
}

/* 0x001a4750 */
void nj_color_strip_end(void)
{
    void *dlist, *pkt;

    dlist = psGsGetDisplayList();
    pkt = psDisplayListGetVifPkt(dlist);
    psVifPktAddWord(pkt, 0x10000000);
    psVifPktAddWord(pkt, 0x01000101);
    psDisplayListCloseRefPkt(dlist);
}

/* Single colour col; as nj_tex_strip_begin. */
/* 0x001a47b0 */
void nj_flat_strip_begin(unsigned int col, int trans)
{
    void *dlist, *pkt;
    int abe;

    abe = 0;
    if (trans)
        abe = 0x40;
    abe |= 1;
    nj_strip_giftag.ul[0] = NJ_GIFTAG(0, 1, 1, ulGxPrimBits[0] | abe, 0, 3);
    nj_strip_giftag.ul[1] = 0x412;
    nj_flat_strip_color = col;
    dlist = psGsGetDisplayList();
    pkt = psDisplayListOpenRefPkt(dlist);
    if (psDisplayListIsDirty(dlist)) {
        psVifPktOpenGifPkt(pkt);
        psDisplayListFlushGsRegs(dlist, pkt);
        psVifPktCloseGifPkt(pkt);
    }
    psVifPktAddWord(pkt, 0x1500000c);
    psVifPktAddWord(pkt, 0x01000104);
}

/* 2D, at most 50, all at depth z. */
/* 0x001a4878 */
void nj_flat_strip_vertices(NjVec2 *verts, int n, float z)
{
    NjVifWr *pkt;
    NjVec2 *vtx;
    unsigned int *tp, *xp, *cp;
    unsigned int col;
    int k, i;

    col = nj_flat_strip_color;
    if (n <= 50) {
        pkt = psDisplayListGetVifPkt(psGsGetDisplayList());
        k = ulGxBeginVertexPkt(pkt, n, &tp, &xp, &cp, 0);
        vtx = verts;
        for (i = 0; i < n; i++) {
            *xp++ = *(unsigned int *)&vtx->x;
            *xp++ = *(unsigned int *)&vtx->y;
            *xp++ = *(unsigned int *)&z;
            *cp++ = col;
            vtx++;
        }
        *tp++ = nj_strip_giftag.ui[0] | n;
        *tp++ = nj_strip_giftag.ui[1];
        *tp++ = nj_strip_giftag.ui[2];
        *tp++ = nj_strip_giftag.ui[3];
        pkt->cur += k * 4;
        pkt->nwords += k;
    }
}

/* 0x001a49e0 */
void nj_flat_strip_end(void)
{
    void *dlist, *pkt;

    dlist = psGsGetDisplayList();
    pkt = psDisplayListGetVifPkt(dlist);
    psVifPktAddWord(pkt, 0x10000000);
    psVifPktAddWord(pkt, 0x01000101);
    psDisplayListCloseRefPkt(dlist);
}

/* Current matrix; GIF tag, GIF packet, changed GS settings. */
/* 0x001a4a40 */
void nj_clip_tex_strip_begin(int trans)
{
    NjSysView *sys = NJ_SYS_VIEW;
    void *dlist, *pkt;
    int abe, prim;

    ulGxSetMatrix(0, ulMatrixCur);
    abe = 0;
    if (trans)
        abe = 0x40;
    abe |= 4;
    prim = sys->prim_bits | 0x10;
    sys->batch_tag[0] = NJ_GIFTAG(0, 0, 1, prim | abe, 0, 3);
    sys->batch_tag[1] = 0x412;
    dlist = psGsGetDisplayList();
    pkt = psDisplayListOpenRefPkt(dlist);
    psVifPktOpenGifPkt(pkt);
    if (psDisplayListIsDirty(dlist))
        psDisplayListFlushGsRegs(dlist, pkt);
}

/* Unclipped run as one strip, next triangle clipped as a fan; advance one; until < 3 left. */
/* 0x001a4af0 */
void nj_clip_tex_strip_vertices(NjTexVtx *vtx, int n)
{
    NjSysView *sys = NJ_SYS_VIEW;
    PsVifPkt *pkt;
    NjClipOut out;
    NjQword in[4];
    NjQword a[4];
    NjQword b[4];
    PsGifTag *tag;
    NjQword *d;
    int k, nclip, j;

    pkt = psDisplayListGetVifPkt(psGsGetDisplayList());
    while (n >= 3) {
        tag = (PsGifTag *)pkt->cur;
        d = (NjQword *)(tag + 1);
        k = nj_vu0_emit_tex_vertices(sys, vtx, n, d);
        if (k >= 3) {
            *(NjQword *)tag = *(NjQword *)sys->batch_tag;
            tag->nloop = k;
            pkt->gtag = (int)tag;
            j = k * 3;
            pkt->cur = (int)(d + j);
            pkt->nqw += 1 + j;
            vtx += k - 2;
            {
                int left = n + 2;
                n = left - k;
            }
        }
        if (n < 3)
            break;
        nj_vu0_clip_tex_vertices(sys, vtx, 3, in);
        nclip = ulGxClipTriangle(&out, in, a, b);
        if (nclip > 0) {
            tag = (PsGifTag *)psVifPktAddGifTag(pkt, sys->batch_tag);
            tag->nloop = nclip;
            tag->prim = (tag->prim & ~7) | 5;
            psVifPktAdvanceGif(pkt, vu0_clip_emit_tex_fog(sys, out.out, nclip, (NjQword *)(tag + 1)), nclip * 3);
        }
        n--;
        vtx++;
    }
}

/* 0x001a4ce0 */
void nj_clip_tex_strip_end(void)
{
    void *dlist;

    dlist = psGsGetDisplayList();
    psVifPktCloseGifPkt(psDisplayListGetVifPkt(dlist));
    psDisplayListCloseRefPkt(dlist);
}

/* 0x001a4d20 */
void nj_clip_tex_strip_draw(NjTexVtx *verts, int n, int trans)
{
    nj_clip_tex_strip_begin(trans);
    nj_clip_tex_strip_vertices(verts, n);
    nj_clip_tex_strip_end();
}

/* As nj_clip_tex_strip_begin. */
/* 0x001a4d60 */
void nj_clip_color_strip_begin(int trans)
{
    NjSysView *sys = NJ_SYS_VIEW;
    void *dlist, *pkt;
    int abe;

    ulGxSetMatrix(0, ulMatrixCur);
    abe = 0;
    if (trans)
        abe = 0x40;
    abe |= 4;
    sys->batch_tag[0] = NJ_GIFTAG(0, 0, 1, sys->prim_bits | abe, 0, 3);
    sys->batch_tag[1] = 0x412;
    dlist = psGsGetDisplayList();
    pkt = psDisplayListOpenRefPkt(dlist);
    psVifPktOpenGifPkt(pkt);
    if (psDisplayListIsDirty(dlist))
        psDisplayListFlushGsRegs(dlist, pkt);
}

/* As nj_clip_tex_strip_vertices. */
/* 0x001a4e10 */
void nj_clip_color_strip_vertices(NjColVtx *vtx, int n)
{
    NjSysView *sys = NJ_SYS_VIEW;
    PsVifPkt *pkt;
    NjClipOut out;
    NjQword in[4];
    NjQword a[4];
    NjQword b[4];
    PsGifTag *tag;
    NjQword *d;
    int k, nclip, j;

    pkt = psDisplayListGetVifPkt(psGsGetDisplayList());
    while (n >= 3) {
        tag = (PsGifTag *)pkt->cur;
        d = (NjQword *)(tag + 1);
        k = nj_vu0_emit_color_vertices(sys, vtx, n, d);
        if (k >= 3) {
            *(NjQword *)tag = *(NjQword *)sys->batch_tag;
            tag->nloop = k;
            pkt->gtag = (int)tag;
            j = k * 3;
            pkt->cur = (int)(d + j);
            pkt->nqw += 1 + j;
            vtx += k - 2;
            {
                int left = n + 2;
                n = left - k;
            }
        }
        if (n < 3)
            break;
        nj_vu0_clip_color_vertices(sys, vtx, 3, in);
        nclip = ulGxClipTriangle(&out, in, a, b);
        if (nclip > 0) {
            tag = (PsGifTag *)psVifPktAddGifTag(pkt, sys->batch_tag);
            tag->nloop = nclip;
            tag->prim = (tag->prim & ~7) | 5;
            psVifPktAdvanceGif(pkt, vu0_clip_emit_fog(sys, out.out, nclip, (NjQword *)(tag + 1)), nclip * 3);
        }
        n--;
        vtx++;
    }
}

/* 0x001a4ff8 */
void nj_clip_color_strip_end(void)
{
    void *dlist;

    dlist = psGsGetDisplayList();
    psVifPktCloseGifPkt(psDisplayListGetVifPkt(dlist));
    psDisplayListCloseRefPkt(dlist);
}

/* 0x001a5038 */
static void nj_clip_color_strip_draw(NjColVtx *verts, int n, int trans)
{
    nj_clip_color_strip_begin(trans);
    nj_clip_color_strip_vertices(verts, n);
    nj_clip_color_strip_end();
}
