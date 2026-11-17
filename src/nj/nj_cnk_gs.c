/*
 * Chunk-model renderer on EE/VU0: vertex list transformed and lit into the vertex buffer, strips
 * sent as GIF packets. Clipping variant: unclipped runs whole, edge-crossing triangles one by one.
 */

#include "types_nj.h"
#include "ninja.h"
#include "libc.h"
#include "nj_cnk_gs.h"
#include "nj_draw.h"
#include "ps_displist.h"
#include "ps_gs.h"
#include "ps_pkt.h"
#include "ul_gx.h"

static void nj_cnk_gs_uv_strips_clip(NjSysView *sys, short *p, float scale);
static void nj_cnk_gs_strips_clip(NjSysView *sys, short *p);
static void nj_cnk_gs_uv_strips_noclip(NjSysView *sys, short *p);
static void nj_cnk_gs_strips_noclip(NjSysView *sys, short *p);

extern void nj_vu0_gs_skin_lit_first(NjSysView *sys, int *src, char *dst, int n);
extern void nj_vu0_gs_skin_lit_mid(NjSysView *sys, int *src, char *dst, int n);
extern int nj_vu0_gs_skin_lit_last_clip(NjSysView *sys, int *src, char *dst, int n);
extern void nj_vu0_gs_pos_clip(NjSysView *sys, int *src, char *dst, int n);
extern void nj_vu0_gs_lit_clip(NjSysView *sys, int *src, char *dst, int n);
extern void nj_vu0_gs_col_clip(NjSysView *sys, int *src, char *dst, int n);
extern void nj_vu0_gs_env_clip(NjSysView *sys, int *src, char *dst, int n);
extern void nj_vu0_gs_skin_lit_last(NjSysView *sys, int *src, char *dst, int n);
extern void nj_vu0_gs_pos(NjSysView *sys, int *src, char *dst, int n);
extern void nj_vu0_gs_lit(NjSysView *sys, int *src, char *dst, int n);
extern void nj_vu0_gs_col(NjSysView *sys, int *src, char *dst, int n);
extern void nj_vu0_gs_nrm_col(NjSysView *sys, int *src, char *dst, int n);
extern void nj_vu0_gs_load_consts(NjSysView *sys);
extern int vu0_gx_emit_uv_strip_clip(NjSysView *sys, NjCnkIdx *p, int n, char *vbuf, NjQword *dst);
extern void vu0_clip_load_consts(NjSysView *sys);
extern void vu0_gx_clip_copy_uv(NjSysView *sys, NjCnkIdx *p, int n, char *vbuf, char *dst);
extern int abs(int x);
extern int nj_vu0_gs_emit_strip_clip(NjSysView *sys, short *p, int n, char *vbuf, NjQword *dst);
extern void vu0_gx_clip_copy(NjSysView *sys, short *p, int n, char *vbuf, char *dst);
extern NjPkt *vu0_gx_emit_uv_strip(NjSysView *sys, NjCnkIdx *p, int n, char *vbuf, PsGifTag *dst);
extern NjPkt *nj_vu0_gs_emit_strip(NjSysView *sys, short *p, int n, char *vbuf, PsGifTag *dst);

/*
 * Chunks: 4 caches the rest, 5 draws a cached list, 8 texture, 16-63 material, 64-66 strips (UV
 * in 1/256 or 1/1024). Bug: 66 passes 1/256, overwriting the 1/1024 it set.
 */
/* 0x001a5078 */
static void nj_cnk_gs_plist_clip(short *p)
{
    NjSysView *sys = NJ_SYS_VIEW;
    void *dlist, *pkt;
    int head, type, flags, size;

    head = *p++;
    while (head != 255) {
        type = head & 0xff;
        flags = head & 0xff00;
        if (type < 8) {
            switch (type) {
            case 4:
                sys->cache[flags >> 8] = p;
                sys->ncache++;
                return;
            case 5:
                nj_cnk_gs_plist_clip(sys->cache[flags >> 8]);
                sys->cache[flags >> 8] = 0;
                sys->ncache--;
                break;
            case 3:
                break;
            default:
                break;
            }
        } else if (type < 16) {
            if (type == 8)
                ulGxDlSetTexture(sys, flags, *p++);
        } else if (type < 64) {
            int len = *p++;
            ulGxDlReadAttr(sys, type, p);
            ulGxDlSetAlphaMode(sys, flags);
            p += len;
        } else {
            size = *p++;
            switch (type) {
            case 64:
                dlist = psGsGetDisplayList();
                pkt = psDisplayListGetVifPkt(dlist);
                ulGxSetRenderFlags(sys, flags);
                psVifPktOpenGifPkt(pkt);
                psDisplayListFlushGsRegs(dlist, pkt);
                nj_cnk_gs_strips_clip(sys, p);
                psVifPktCloseGifPkt(pkt);
                break;
            case 65:
                dlist = psGsGetDisplayList();
                pkt = psDisplayListGetVifPkt(dlist);
                sys->uv_scale = 1.0f / 256.0f;
                ulGxSetRenderFlags(sys, flags);
                psVifPktOpenGifPkt(pkt);
                psDisplayListFlushGsRegs(dlist, pkt);
                nj_cnk_gs_uv_strips_clip(sys, p, 1.0f / 256.0f);
                psVifPktCloseGifPkt(pkt);
                break;
            case 66:
                dlist = psGsGetDisplayList();
                pkt = psDisplayListGetVifPkt(dlist);
                sys->uv_scale = 1.0f / 1024.0f;
                ulGxSetRenderFlags(sys, flags);
                psVifPktOpenGifPkt(pkt);
                psDisplayListFlushGsRegs(dlist, pkt);
                nj_cnk_gs_uv_strips_clip(sys, p, 1.0f / 256.0f);
                psVifPktCloseGifPkt(pkt);
                break;
            }
            p += size;
        }
        head = *p++;
    }
}

/* Skinned chunk at its index and count; flags: first/middle/last pass; last makes clip codes. */
/* 0x001a5330 */
static int nj_cnk_gs_vblock_clip(NjSysView *sys, int *chunk)
{
    int n;
    char *dst;
    int *src;

    src = chunk + 2;
    n = (unsigned int)chunk[1] >> 16;
    dst = sys->vbuf + (chunk[1] & 0xffff) * 64;
    switch (chunk[0] & 0x7f00) {
    case 0:
        nj_vu0_gs_skin_lit_first(sys, src, dst, n);
        break;
    case 0x100:
        nj_vu0_gs_skin_lit_mid(sys, src, dst, n);
        break;
    case 0x200:
        return nj_vu0_gs_skin_lit_last_clip(sys, src, dst, n);
    }
    return n;
}

/*
 * With clip codes: 34 positions, 35 +colour, 41 +normal, 42 +normal+colour, 44 skinned. Compact
 * shapes are skipped with a message. Counts vertices. Returns 0.
 */
/* 0x001a53f0 */
static int nj_cnk_gs_vlist_clip(int *p)
{
    NjSysView *sys = NJ_SYS_VIEW;
    int head, type, size, n;
    char *dst;

    for (; *p != 255;) {
        head = *p;
        type = head & 0xff;
        size = (unsigned int)head >> 16;
        if (head & 0x4000) {
            printf("Vlist for Compact Shape\n");
            p += size + 1;
        } else {
            n = (unsigned int)p[1] >> 16;
            dst = sys->vbuf + (p[1] & 0xffff) * 64;
            switch (type) {
            case 34:
                nj_vu0_gs_pos_clip(sys, p + 2, dst, n);
                break;
            case 41:
                nj_vu0_gs_lit_clip(sys, p + 2, dst, n);
                break;
            case 44:
                nj_cnk_gs_vblock_clip(sys, p);
                break;
            case 35:
                nj_vu0_gs_col_clip(sys, p + 2, dst, n);
                break;
            case 42:
                nj_vu0_gs_env_clip(sys, p + 2, dst, n);
                break;
            }
            sys->nvtx += n;
            p += size + 1;
        }
    }
    return 0;
}

/* As nj_cnk_gs_plist_clip, no clipping. */
/* 0x001a5548 */
static void nj_cnk_gs_plist_noclip(short *p)
{
    NjSysView *sys = NJ_SYS_VIEW;
    void *dlist, *pkt;
    int head, type, flags, size;

    head = *p++;
    while (head != 255) {
        type = head & 0xff;
        flags = head & 0xff00;
        if (type < 8) {
            switch (type) {
            case 4:
                sys->cache[flags >> 8] = p;
                sys->ncache++;
                return;
            case 5:
                nj_cnk_gs_plist_noclip(sys->cache[flags >> 8]);
                sys->cache[flags >> 8] = 0;
                sys->ncache--;
                break;
            case 3:
                break;
            default:
                break;
            }
        } else if (type < 16) {
            if (type == 8)
                ulGxDlSetTexture(sys, flags, *p++);
        } else if (type < 64) {
            int len = *p++;
            ulGxDlReadAttr(sys, type, p);
            ulGxDlSetAlphaMode(sys, flags);
            p += len;
        } else {
            size = *p++;
            switch (type) {
            case 64:
                dlist = psGsGetDisplayList();
                pkt = psDisplayListGetVifPkt(dlist);
                ulGxSetRenderFlags(sys, flags);
                psVifPktOpenGifPkt(pkt);
                psDisplayListFlushGsRegs(dlist, pkt);
                nj_cnk_gs_strips_noclip(sys, p);
                psVifPktCloseGifPkt(pkt);
                break;
            case 65:
                dlist = psGsGetDisplayList();
                pkt = psDisplayListGetVifPkt(dlist);
                sys->uv_scale = 1.0f / 256.0f;
                ulGxSetRenderFlags(sys, flags);
                psVifPktOpenGifPkt(pkt);
                psDisplayListFlushGsRegs(dlist, pkt);
                nj_cnk_gs_uv_strips_noclip(sys, p);
                psVifPktCloseGifPkt(pkt);
                break;
            case 66:
                dlist = psGsGetDisplayList();
                pkt = psDisplayListGetVifPkt(dlist);
                sys->uv_scale = 1.0f / 1024.0f;
                ulGxSetRenderFlags(sys, flags);
                psVifPktOpenGifPkt(pkt);
                psDisplayListFlushGsRegs(dlist, pkt);
                nj_cnk_gs_uv_strips_noclip(sys, p);
                psVifPktCloseGifPkt(pkt);
                break;
            }
            p += size;
        }
        head = *p++;
    }
}

/* No clip codes; returns 0. */
/* 0x001a57f0 */
static int nj_cnk_gs_vblock_noclip(NjSysView *sys, int *chunk)
{
    int n;
    char *dst;
    int *src;

    src = chunk + 2;
    n = (unsigned int)chunk[1] >> 16;
    dst = sys->vbuf + (chunk[1] & 0xffff) * 64;
    switch (chunk[0] & 0x7f00) {
    case 0:
        nj_vu0_gs_skin_lit_first(sys, src, dst, n);
        break;
    case 0x100:
        nj_vu0_gs_skin_lit_mid(sys, src, dst, n);
        break;
    case 0x200:
        nj_vu0_gs_skin_lit_last(sys, src, dst, n);
        break;
    }
    return 0;
}

/* No clip codes. */
/* 0x001a5898 */
static int nj_cnk_gs_vlist_noclip(int *p)
{
    NjSysView *sys = NJ_SYS_VIEW;
    int head, type, size, n;
    char *dst;

    head = *p;
    while (head != 255) {
        type = head & 0xff;
        size = (unsigned int)head >> 16;
        if (head & 0x4000) {
            printf("Vlist for Compact Shape\n");
            p += size + 1;
        } else {
            n = (unsigned int)p[1] >> 16;
            dst = sys->vbuf + (p[1] & 0xffff) * 64;
            switch (type) {
            case 34:
                nj_vu0_gs_pos(sys, p + 2, dst, n);
                break;
            case 41:
                nj_vu0_gs_lit(sys, p + 2, dst, n);
                break;
            case 44:
                nj_cnk_gs_vblock_noclip(sys, p);
                break;
            case 35:
                nj_vu0_gs_col(sys, p + 2, dst, n);
                break;
            case 42:
                nj_vu0_gs_nrm_col(sys, p + 2, dst, n);
                break;
            }
            sys->nvtx += n;
            p += size + 1;
        }
        head = *p;
    }
    return 0;
}

/* Returns 0. */
/* 0x001a59f0 */
static int nj_cnk_gs_draw_noclip(NjCnkModel *model)
{
    ulGxSetMatrix(0, ulMatrixCur);
    if (model->vlist)
        nj_cnk_gs_vlist_noclip(model->vlist);
    if (model->plist) {
        psGsOpenRefPkt();
        nj_cnk_gs_plist_noclip(model->plist);
        psGsCloseRefPkt();
    }
    return 0;
}

/* Returns 0. */
/* 0x001a5a58 */
static int nj_cnk_gs_draw_clip(NjCnkModel *model)
{
    ulGxSetMatrix(0, ulMatrixCur);
    if (model->vlist)
        nj_cnk_gs_vlist_clip(model->vlist);
    if (model->plist) {
        psGsOpenRefPkt();
        nj_cnk_gs_plist_clip(model->plist);
        psGsCloseRefPkt();
    }
    return 0;
}

/* Clipping unless the no-clip draw flag is set. */
/* 0x001a5ac0 */
int nj_cnk_gs_draw(NjCnkModel *model)
{
    if (ulGxGetDrawFlags() & 1)
        return nj_cnk_gs_draw_noclip(model);
    return nj_cnk_gs_draw_clip(model);
}

/* 0x001a5b08 */
static int nj_shape_draw_thunk2(int shape)
{
    return nj_shape_draw(shape);
}

/* Unclipped run as one strip, next triangle clipped alone; advance one until < 3 left. */
/* 0x001a5b28 */
static void nj_cnk_gs_uv_strip_clip(NjSysView *sys, NjCnkIdx *idx, int n, char *vbuf)
{
    PsVifPkt *pkt;
    NjCnkIdx *p;
    char tri[3][64];
    PsGifTag *tag;
    NjQword *d;
    int k, j;

    pkt = psGsGetRefPkt();
    sys->batch_tag[0] = NJ_GIFTAG(0, 0, 1, (sys->prim & ~7) | 0x14, 0, 3);
    sys->batch_tag[1] = 0x412;
    p = idx;
    while (n >= 3) {
        nj_vu0_gs_load_consts(sys);
        tag = (PsGifTag *)pkt->cur;
        d = (NjQword *)(tag + 1);
        k = vu0_gx_emit_uv_strip_clip(sys, p, n, vbuf, d);
        if (k >= 3) {
            *(NjQword *)tag = *(NjQword *)sys->batch_tag;
            tag->nloop = k;
            pkt->gtag = (int)tag;
            j = k * 3;
            pkt->cur = (int)(d + j);
            pkt->nqw += 1 + j;
            {
                int skip = k - 2;
                p += skip;
            }
            {
                int left = n + 2;
                n = left - k;
            }
        }
        if (k < n) {
            vu0_clip_load_consts(sys);
            n--;
            vu0_gx_clip_copy_uv(sys, p, 3, vbuf, tri[0]);
            p++;
            ulGxClipDrawTexTriangle(sys, tri[0], tri[1], tri[2], 0);
        }
    }
}

/* UV scaled by scale. */
/* 0x001a5cc8 */
static void nj_cnk_gs_uv_strips_clip(NjSysView *sys, short *p, float scale)
{
    int n, len, i;

    sys->uv_scale = scale;
    n = *p++;
    n &= 0x3fff;
    for (i = 0; i < n; i++) {
        len = abs(*p++);
        nj_cnk_gs_uv_strip_clip(sys, (NjCnkIdx *)p, len, sys->vbuf);
        p += len * 3;
    }
}

/* As nj_cnk_gs_uv_strip_clip. */
/* 0x001a5d60 */
static void nj_cnk_gs_strip_clip(NjSysView *sys, short *idx, int n, char *vbuf)
{
    PsVifPkt *pkt;
    short *p;
    char tri[3][64];
    PsGifTag *tag;
    NjQword *d;
    int k, j;

    pkt = psGsGetRefPkt();
    sys->batch_tag[0] = NJ_GIFTAG(0, 0, 1, (sys->prim & ~7) | 4, 0, 2);
    sys->batch_tag[1] = 0x41;
    p = idx;
    n = abs(n);
    while (n >= 3) {
        nj_vu0_gs_load_consts(sys);
        tag = (PsGifTag *)pkt->cur;
        d = (NjQword *)(tag + 1);
        k = nj_vu0_gs_emit_strip_clip(sys, p, n, vbuf, d);
        if (k >= 3) {
            *(NjQword *)tag = *(NjQword *)sys->batch_tag;
            tag->nloop = k;
            pkt->gtag = (int)tag;
            j = k * 2;
            pkt->cur = (int)(d + j);
            pkt->nqw += 1 + j;
            p += k - 2;
            {
                int left = n + 2;
                n = left - k;
            }
        }
        if (k < n) {
            vu0_clip_load_consts(sys);
            n--;
            vu0_gx_clip_copy(sys, p, 3, vbuf, tri[0]);
            p++;
            ulGxClipDrawTriangle(sys, tri[0], tri[1], tri[2], 0);
        }
    }
}

/* 0x001a5ef0 */
static void nj_cnk_gs_strips_clip(NjSysView *sys, short *p)
{
    int n, len, i;

    n = *p++;
    n &= 0x3fff;
    for (i = 0; i < n; i++) {
        len = abs(*p++);
        nj_cnk_gs_strip_clip(sys, p, len, sys->vbuf);
        p += len;
    }
}

/* One GIF strip each, material primitive settings; counts triangles. */
/* 0x001a5f78 */
static void nj_cnk_gs_uv_strips_noclip(NjSysView *sys, short *p)
{
    PsGifTag tag;
    PsGifTag *gif;
    PsVifPkt *pkt;
    int n, len, i;

    tag = *(PsGifTag *)sys->tex_tag;
    tag.prim = (sys->prim & ~7) | 0x14;
    n = (*p++ << 2) >> 2 & 0x3fff;
    nj_vu0_gs_load_consts(sys);
    for (i = 0; i < n; i++) {
        len = *p++;
        pkt = psGsGetRefPkt();
        len = abs(len);
        gif = (PsGifTag *)psVifPktAddGifTag(pkt, (struct PsGifTag *)&tag);
        gif->nloop = len;
        psVifPktAdvanceGif(pkt, vu0_gx_emit_uv_strip(sys, (NjCnkIdx *)p, len, sys->vbuf, gif + 1), len * 3);
        p += len * 3;
        sys->ntri += len - 2;
    }
}

/* As nj_cnk_gs_uv_strips_noclip. */
/* 0x001a60c8 */
static void nj_cnk_gs_strips_noclip(NjSysView *sys, short *p)
{
    PsGifTag tag;
    PsGifTag *gif;
    PsVifPkt *pkt;
    NjPkt *end;
    int n, len, i;

    tag = *(PsGifTag *)sys->gouraud_tag;
    tag.prim = (sys->prim & ~7) | 4;
    n = (*p++ << 2) >> 2 & 0x3fff;
    nj_vu0_gs_load_consts(sys);
    for (i = 0; i < n; i++) {
        len = *p++;
        pkt = psGsGetRefPkt();
        len = abs(len);
        gif = (PsGifTag *)psVifPktAddGifTag(pkt, (struct PsGifTag *)&tag);
        gif->nloop = len;
        end = nj_vu0_gs_emit_strip(sys, p, len, sys->vbuf, gif + 1);
        p += len;
        psVifPktAdvanceGif(pkt, end, len * 2);
        sys->ntri += len - 2;
    }
}
