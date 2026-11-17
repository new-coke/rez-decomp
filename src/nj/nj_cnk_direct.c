/*
 * Chunk-model renderers: pre-built VU1 packets, direct strips (positions copied into VU1
 * packets), environment-mapped GS path, wireframe line strips; screen scale; texture by number.
 */

#include "types_nj.h"
#include "ninja.h"
#include "nj_cnk_direct.h"
#include "libc.h"
#include "nj_draw.h"
#include "ps_displist.h"
#include "ps_gs.h"
#include "ps_pkt.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_texture.h"

static void nj_cnk_direct_strips(NjSysView *sys, short *p, int *vtx, int stride, int step);
static void nj_cnk_gs_uv_strips_env(NjSysView *sys, short *p);
static void nj_cnk_gs_strips_env(NjSysView *sys, short *p);
static void nj_cnk_wire_uv_strips(NjSysView *sys, short *p);
static void nj_cnk_wire_strips(NjSysView *sys, short *p);

extern float nj_screen_scale_x;
extern float nj_screen_scale_y;
extern int nj_vu0_direct_points_map_clip(NjSysView *sys, int *src, char *dst, int n, int mode);
extern void nj_object_walk(NjObject *obj, int (*draw)(void *model));
extern int abs(int x);
extern void nj_vu0_env_load_consts(NjSysView *sys);
extern NjPkt *nj_vu0_env_emit_uv_strip(NjSysView *sys, NjCnkIdx *p, int n, char *vbuf, PsGifTag *dst);
extern NjPkt *nj_vu0_env_emit_strip(NjSysView *sys, short *p, int n, char *vbuf, PsGifTag *dst);
extern void nj_vu0_wire_skin_first(NjSysView *sys, int *src, char *dst, int n);
extern void nj_vu0_wire_skin_mid(NjSysView *sys, int *src, char *dst, int n);
extern int nj_vu0_wire_skin_last_clip(NjSysView *sys, int *src, char *dst, int n);
extern int nj_cnk_wire_clip_count;
extern int nj_vu0_wire_points_clip(NjSysView *sys, int *src, char *dst, int n, int mode);
extern void nj_vu0_wire_load_consts(NjSysView *sys);
extern int nj_vu0_wire_emit_uv_strip_clip(NjSysView *sys, NjCnkIdx *p, int n, char *vbuf, NjQword *dst);
extern int nj_vu0_wire_emit_strip_clip(NjSysView *sys, short *p, int n, char *vbuf, NjQword *dst);
extern NjPkt *nj_vu0_wire_emit_uv_strip(NjSysView *sys, NjCnkIdx *p, int n, char *vbuf, PsGifTag *dst);
extern NjPkt *nj_vu0_wire_emit_strip(NjSysView *sys, short *p, int n, char *vbuf, PsGifTag *dst);

/* Relative to a 640 x 240 field, for the VU1 renderers; also resets the two constants beside it. */
/* 0x001a6208 */
void nj_set_screen_scale(float x, float y)
{
    NjSysView *sys = NJ_SYS_VIEW;

    x = x / 640.0f;
    y = y / 240.0f;
    sys->unk_520 = -1728.0f;
    sys->unk_524 = -1928.0f;
    nj_screen_scale_x = x;
    sys->scale_x = x;
    nj_screen_scale_y = y;
    sys->scale_y = y;
}

/* Entry idx of the current texture list, filter setting 1. */
/* 0x001a6268 */
void njSetTextureNum(int idx)
{
    ulGxSelectTexture(ulGxGetTextureList(), idx);
    ulGxSetTexFilter(1);
}

/* Global texture number texno, filter setting 1. */
/* 0x001a62a0 */
static void njSetTextureNumG(int texno)
{
    ulGxSetTexture(ulTextureFindUsedById(texno));
    ulGxSetTexFilter(1);
}

/* Compiled list (nj_compile.c): per strip, changed GS settings and parameters, VIF data by ref. */
/* 0x001a62c8 */
static void nj_cnk_packet_plist(short *p)
{
    NjSysView *sys = NJ_SYS_VIEW;
    void *dlist, *pkt;
    int head, type, flags, size;

    dlist = psGsGetDisplayList();
    head = *p++;
    while (head != 255) {
        type = head & 0xff;
        flags = head & 0xff00;
        if (type < 8) {
        } else if (type < 16) {
            if (type == 8)
                p++;
        } else if (type < 64) {
            int len = *p++;
            ulGxDlReadAttr(sys, type, p);
            ulGxDlSetAlphaMode(sys, flags);
            p += len;
        } else {
            size = *p++;
            if (type < 67) {
                if (type >= 64) {
                    pkt = psGsOpenRefPkt();
                    ulGxSetRenderFlags(sys, flags);
                    if (psDisplayListIsDirty(dlist)) {
                        psVifPktOpenGifPkt(pkt);
                        psDisplayListFlushGsRegs(dlist, pkt);
                        psVifPktCloseGifPkt(pkt);
                    }
                    sys->strip_prim = sys->prim | 0x14;
                    psVifPktAddWord(pkt, 0x6c010040);
                    psVifPktAddWords(pkt, &sys->vu1_param, 4);
                    psVifPktAddWord(pkt, 0x1500000e);
                    psGsCloseRefPkt();
                    psDisplayListAddRefPkt(dlist, p + 5 + p[4], p[3]);
                }
            }
            p += size;
        }
        head = *p++;
    }
}

/*
 * Skips the model if a corner of its bounding-sphere cube is outside the clip volume (no clipping
 * here). Sphere centre depth (distance, inverse, inverse 12.20) goes to VU1 with the screen scale.
 */
/* 0x001a6470 */
int nj_cnk_packet_draw_culled(NjCnkModel *model)
{
    NjSysView *sys = NJ_SYS_VIEW;
    NjVec3 center;
    void *pkt;
    float inv;

    ulGxSetMatrix(0, ulMatrixCur);
    if (nj_cube_clipped(&model->center, model->r))
        return 0;
    ulMatrixApplyPoint((char *)sys + 0xc0, &model->center, &center);
    if (model->plist) {
        psGsOpenRefPkt();
        sys->depth = -center.z;
        inv = -1.0f / center.z;
        sys->inv_depth = inv;
        sys->inv_depth_fx = inv * 1048576.0f;
        pkt = psDisplayListGetVifPkt(psGsGetDisplayList());
        psVifPktAddWord(pkt, 0x01000101);
        psVifPktAddWord(pkt, 0x6c020051);
        psVifPktAddWords(pkt, &sys->scale_x, 8);
        psGsCloseRefPkt();
        nj_cnk_packet_plist(model->plist);
    }
    return 0;
}

/* Without the clip test. */
/* 0x001a6578 */
int nj_cnk_packet_draw(NjCnkModel *model)
{
    NjSysView *sys = NJ_SYS_VIEW;
    NjVec3 center;
    void *pkt;
    float inv;

    ulGxSetMatrix(0, ulMatrixCur);
    ulMatrixApplyPoint((char *)sys + 0xc0, &model->center, &center);
    if (model->plist) {
        psGsOpenRefPkt();
        sys->depth = -center.z;
        inv = -1.0f / center.z;
        sys->inv_depth = inv;
        sys->inv_depth_fx = inv * 1048576.0f;
        pkt = psDisplayListGetVifPkt(psGsGetDisplayList());
        psVifPktAddWord(pkt, 0x01000101);
        psVifPktAddWord(pkt, 0x6c020051);
        psVifPktAddWords(pkt, &sys->scale_x, 8);
        psGsCloseRefPkt();
        nj_cnk_packet_plist(model->plist);
    }
    return 0;
}

/* From nj_cnk_direct_vtx; texture coordinates of textured strips are skipped. */
/* 0x001a6668 */
static void nj_cnk_direct_plist(short *p)
{
    NjSysView *sys = NJ_SYS_VIEW;
    void *dlist, *pkt;
    int head, type, flags, size;

    head = *p++;
    while (head != 255) {
        type = head & 0xff;
        flags = head & 0xff00;
        if (type < 8) {
        } else if (type < 16) {
            if (type == 8)
                p++;
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
                if (psDisplayListIsDirty(dlist)) {
                    psVifPktOpenGifPkt(pkt);
                    psDisplayListFlushGsRegs(dlist, pkt);
                    psVifPktCloseGifPkt(pkt);
                }
                nj_cnk_direct_strips(sys, p, nj_cnk_direct_vtx, nj_cnk_direct_stride, 2);
                break;
            case 65:
            case 66:
                dlist = psGsGetDisplayList();
                pkt = psDisplayListGetVifPkt(dlist);
                ulGxSetRenderFlags(sys, flags);
                if (psDisplayListIsDirty(dlist)) {
                    psVifPktOpenGifPkt(pkt);
                    psDisplayListFlushGsRegs(dlist, pkt);
                    psVifPktCloseGifPkt(pkt);
                }
                nj_cnk_direct_strips(sys, p, nj_cnk_direct_vtx, nj_cnk_direct_stride, 6);
                break;
            }
            p += size;
        }
        head = *p++;
    }
}

/* nj_cnk_direct_vtx/_stride: 12, 16, 24, 28 bytes for types 34, 35, 41, 42 (else none); counts. */
/* 0x001a6840 */
static int nj_cnk_direct_vlist_noclip(int *p)
{
    int head, type, size, n, stride;
    int end;
    int *vtx;

    head = *p;
    end = 255;
    while (head != end) {
        end = 255;
        {
            type = head & 0xff;
            size = (unsigned int)head >> 16;
            if (head & 0x4000) {
                printf("Vlist for Compact Shape\n");
                p += size + 1;
            } else {
                n = ((unsigned short *)p)[3];
                switch (type) {
                case 34:
                    vtx = p + 2;
                    stride = 12;
                    break;
                case 42:
                    vtx = p + 2;
                    stride = 28;
                    break;
                case 41:
                    vtx = p + 2;
                    stride = 24;
                    break;
                case 35:
                    vtx = p + 2;
                    stride = 16;
                    break;
                default:
                    goto def;
                }
                nj_cnk_direct_vtx = vtx;
                nj_cnk_direct_stride = stride;
                goto next;
            def:
                nj_cnk_direct_vtx = 0;
                nj_cnk_direct_stride = 0;
            next:
                NJ_SYS_VIEW->nvtx += n;
                p += size + 1;
            }
        }
        head = *p;
        end = 255;
    }
    return 0;
}

/* No clipping; sphere centre depth to VU1 as nj_cnk_packet_draw_culled. Returns 0. */
/* 0x001a6978 */
int nj_cnk_direct_draw_noclip(NjCnkModel *model)
{
    NjSysView *sys = NJ_SYS_VIEW;
    NjVec3 center;
    void *pkt;
    float inv;

    ulGxSetMatrix(0, ulMatrixCur);
    ulMatrixApplyPoint((char *)sys + 0xc0, &model->center, &center);
    if (model->vlist)
        nj_cnk_direct_vlist_noclip(model->vlist);
    if (nj_cnk_direct_vtx == 0)
        return 0;
    if (model->plist) {
        psGsOpenRefPkt();
        sys->depth = -center.z;
        inv = -1.0f / center.z;
        sys->inv_depth = inv;
        sys->inv_depth_fx = inv * 1048576.0f;
        pkt = psDisplayListGetVifPkt(psGsGetDisplayList());
        psVifPktAddWord(pkt, 0x01000101);
        psVifPktAddWord(pkt, 0x6c020051);
        psVifPktAddWords(pkt, &sys->scale_x, 8);
        nj_cnk_direct_plist(model->plist);
        psGsCloseRefPkt();
    }
    return 0;
}

/* Env-mapped nj_cnk_gs_plist_noclip; texture chunks skipped; scale only to cached lists. */
/* 0x001a6a88 */
static void nj_cnk_gs_plist_env(short *p, float scale)
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
                nj_cnk_gs_plist_env(sys->cache[flags >> 8], scale);
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
                p++;
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
                nj_cnk_gs_strips_env(sys, p);
                psVifPktCloseGifPkt(pkt);
                break;
            case 65:
                dlist = psGsGetDisplayList();
                pkt = psDisplayListGetVifPkt(dlist);
                sys->uv_scale = 1.0f / 256.0f;
                ulGxSetRenderFlags(sys, flags);
                psVifPktOpenGifPkt(pkt);
                psDisplayListFlushGsRegs(dlist, pkt);
                nj_cnk_gs_uv_strips_env(sys, p);
                psVifPktCloseGifPkt(pkt);
                break;
            case 66:
                dlist = psGsGetDisplayList();
                pkt = psDisplayListGetVifPkt(dlist);
                sys->uv_scale = 1.0f / 1024.0f;
                ulGxSetRenderFlags(sys, flags);
                psVifPktOpenGifPkt(pkt);
                psDisplayListFlushGsRegs(dlist, pkt);
                nj_cnk_gs_uv_strips_env(sys, p);
                psVifPktCloseGifPkt(pkt);
                break;
            }
            p += size;
        }
        head = *p++;
    }
}

/* Plus a vertex clip test; returns the last result; positive (off screen) forgets the data. */
/* 0x001a6d28 */
static int nj_cnk_direct_vlist_clip(int *p)
{
    NjSysView *sys = NJ_SYS_VIEW;
    int head, type, size, n, ret;
    int *vtx;
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
                vtx = p + 2;
                ret = nj_vu0_direct_points_map_clip(sys, vtx, dst, n, 3);
                nj_cnk_direct_vtx = vtx;
                nj_cnk_direct_stride = 12;
                break;
            case 41:
                vtx = p + 2;
                ret = nj_vu0_direct_points_map_clip(sys, vtx, dst, n, 6);
                nj_cnk_direct_vtx = vtx;
                nj_cnk_direct_stride = 24;
                break;
            case 35:
                vtx = p + 2;
                ret = nj_vu0_direct_points_map_clip(sys, vtx, dst, n, 4);
                nj_cnk_direct_vtx = vtx;
                nj_cnk_direct_stride = 16;
                break;
            case 42:
                vtx = p + 2;
                ret = nj_vu0_direct_points_map_clip(sys, vtx, dst, n, 7);
                nj_cnk_direct_vtx = vtx;
                nj_cnk_direct_stride = 28;
                break;
            default:
                ret = n;
                nj_cnk_direct_vtx = 0;
                nj_cnk_direct_stride = 0;
                break;
            }
            NJ_SYS_VIEW->nvtx += n;
            p += size + 1;
        }
        head = *p;
    }
    if (ret > 0) {
        nj_cnk_direct_vtx = 0;
        nj_cnk_direct_stride = 0;
    }
    return ret;
}

/* Nothing if the sphere is not fully in front or any vertex fails the clip test (no clipping). */
/* 0x001a6ee8 */
int nj_cnk_direct_draw_clip(NjCnkModel *model)
{
    NjSysView *sys = NJ_SYS_VIEW;
    NjVec3 center;
    void *pkt;
    float inv;

    ulGxSetMatrix(0, ulMatrixCur);
    ulMatrixApplyPoint((char *)sys + 0xc0, &model->center, &center);
    if (center.z + model->r > -1.0f)
        return 0;
    if (model->vlist) {
        if (nj_cnk_direct_vlist_clip(model->vlist) > 0)
            return 0;
    }
    if (nj_cnk_direct_vtx == 0)
        return 0;
    if (model->plist) {
        psGsOpenRefPkt();
        sys->depth = -center.z;
        inv = -1.0f / center.z;
        sys->inv_depth = inv;
        sys->inv_depth_fx = inv * 1048576.0f;
        pkt = psDisplayListGetVifPkt(psGsGetDisplayList());
        psVifPktAddWord(pkt, 0x01000101);
        psVifPktAddWord(pkt, 0x6c020051);
        psVifPktAddWords(pkt, &sys->scale_x, 8);
        nj_cnk_direct_plist(model->plist);
        psGsCloseRefPkt();
    }
    return 0;
}

/* Clip test unless the no-clip draw flag is set. */
/* 0x001a7028 */
static int nj_cnk_direct_draw(NjCnkModel *model)
{
    if (ulGxGetDrawFlags() & 1)
        return nj_cnk_direct_draw_noclip(model);
    return nj_cnk_direct_draw_clip(model);
}

/* 0x001a7070 */
static void nj_object_draw_direct(NjObject *obj)
{
    nj_object_walk(obj, nj_cnk_direct_draw);
}

/* VU1 block for n vertices, write position not advanced: optional header, positions; words. */
/* 0x001a7080 */
static int nj_vu1_xyz_block(PsVifPkt *pkt, int n, int **tag, int **xyz)
{
    int *p;
    int k;

    p = (int *)pkt->cur;
    k = 0;
    if (tag) {
        *p++ = 0x6c018000;
        *tag = p;
        p += 4;
        k = 5;
    }
    if (xyz) {
        *p++ = (n << 16) | 0x68008004;
        *xyz = p;
        {
            int nwords = k + 1;
            k = nwords + n * 3;
        }
        p += n * 3;
    }
    *p = 0x17000000;
    return k + 1;
}

/* Positions from vtx (stride per vertex, step per entry); over 50 skipped; unused is unused. */
/* 0x001a70f8 */
static void nj_cnk_direct_strips(NjSysView *sys, short *p, int *vtx, int stride, int step)
{
    PsVifPkt *pkt;
    int *blk, *dst, *src, *end;
    int unused[4];
    int n, i, j, len, words;

    n = (*p++ << 2) >> 2 & 0x3fff;
    pkt = psGsGetRefPkt();
    sys->strip_prim = sys->prim | 0x14;
    psVifPktAddWord(pkt, 0x01000101);
    psVifPktAddWord(pkt, 0x6c010040);
    psVifPktAddWords(pkt, &sys->vu1_param, 4);
    psVifPktAddWord(pkt, 0x1500000e);
    psVifPktAddWord(pkt, 0x01000104);
    for (i = 0; i < n; i++) {
        len = *p++;
        len = abs(len);
        if (len > 50) {
            p += len;
            continue;
        }
        blk = (int *)pkt->cur;
        blk[0] = 0x6c018002;
        blk[5] = (len << 16) | 0x68008004;
        dst = blk + 6;
        end = dst + len * 3;
        *end = 0x17000000;
        blk[2] = len;
        words = len * 3 + 7;
        for (j = 0; j < len; j++) {
            src = (int *)((char *)vtx + *p * stride);
            p = (short *)((char *)p + step);
            *dst++ = src[0];
            *dst++ = src[1];
            *dst++ = src[2];
        }
        pkt->cur = (int)(blk + words);
        pkt->nwords += words;
        NJ_SYS_VIEW->ntri += len - 2;
    }
    psVifPktAddWord(pkt, 0x01000101);
    psVifPktAddWord(pkt, 0x11000000);
}

/* Environment-mapped nj_cnk_gs_uv_strips_noclip. */
/* 0x001a7308 */
static void nj_cnk_gs_uv_strips_env(NjSysView *sys, short *p)
{
    PsGifTag tag;
    PsGifTag *gif;
    PsVifPkt *pkt;
    int n, len, i;

    tag = *(PsGifTag *)sys->tex_tag;
    tag.prim = (sys->prim & ~7) | 0x14;
    n = (*p++ << 2) >> 2 & 0x3fff;
    nj_vu0_env_load_consts(sys);
    for (i = 0; i < n; i++) {
        len = *p++;
        pkt = psGsGetRefPkt();
        len = abs(len);
        gif = (PsGifTag *)psVifPktAddGifTag(pkt, (struct PsGifTag *)&tag);
        gif->nloop = len;
        psVifPktAdvanceGif(pkt, nj_vu0_env_emit_uv_strip(sys, (NjCnkIdx *)p, len, sys->vbuf, gif + 1), len * 3);
        p += len * 3;
        sys->ntri += len - 2;
    }
}

/* No UV chunks: the environment map supplies them, so drawn textured too. */
/* 0x001a7458 */
static void nj_cnk_gs_strips_env(NjSysView *sys, short *p)
{
    PsGifTag tag;
    PsGifTag *gif;
    PsVifPkt *pkt;
    NjPkt *end;
    int n, len, i;

    tag = *(PsGifTag *)sys->tex_tag;
    tag.prim = (sys->prim & ~7) | 0x14;
    n = (*p++ << 2) >> 2 & 0x3fff;
    nj_vu0_env_load_consts(sys);
    for (i = 0; i < n; i++) {
        len = *p++;
        pkt = psGsGetRefPkt();
        len = abs(len);
        gif = (PsGifTag *)psVifPktAddGifTag(pkt, (struct PsGifTag *)&tag);
        gif->nloop = len;
        end = nj_vu0_env_emit_strip(sys, p, len, sys->vbuf, gif + 1);
        p += len;
        psVifPktAdvanceGif(pkt, end, len * 3);
        sys->ntri += len - 2;
    }
}

/* Texture/material chunks skipped; strips as line strips (chunk types: nj_cnk_gs_plist_clip). */
/* 0x001a7598 */
static void nj_cnk_wire_plist(short *p)
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
                nj_cnk_wire_plist(sys->cache[flags >> 8]);
                sys->cache[flags >> 8] = 0;
                sys->ncache--;
                break;
            case 3:
                break;
            }
        } else if (type < 16) {
            if (type == 8)
                p++;
        } else if (type < 64) {
            int len = *p++;
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
                nj_cnk_wire_strips(sys, p);
                psVifPktCloseGifPkt(pkt);
                break;
            case 65:
            case 66:
                dlist = psGsGetDisplayList();
                pkt = psDisplayListGetVifPkt(dlist);
                sys->uv_scale = 1.0f / 256.0f;
                ulGxSetRenderFlags(sys, flags);
                psVifPktOpenGifPkt(pkt);
                psDisplayListFlushGsRegs(dlist, pkt);
                nj_cnk_wire_uv_strips(sys, p);
                psVifPktCloseGifPkt(pkt);
                break;
            }
            p += size;
        }
        head = *p++;
    }
}

/* Wireframe nj_cnk_gs_vblock_clip into vbuf; the last pass counts vertices. */
/* 0x001a7798 */
static int nj_cnk_wire_vblock(NjSysView *sys, int *chunk, char *vbuf)
{
    int n, ret;
    char *dst;
    int *src;

    n = (unsigned int)chunk[1] >> 16;
    dst = vbuf + (chunk[1] & 0xffff) * 64;
    src = chunk + 2;
    switch (chunk[0] & 0x7f00) {
    case 0:
        nj_vu0_wire_skin_first(sys, src, dst, n);
        return n;
    case 0x100:
        nj_vu0_wire_skin_mid(sys, src, dst, n);
        return n;
    case 0x200:
        ret = nj_vu0_wire_skin_last_clip(sys, src, dst, n);
        sys->nvtx += n;
        return ret;
    }
    return n;
}

/* Clip codes into vbuf; counts vertices and nj_cnk_wire_clip_count; returns the last result. */
/* 0x001a7860 */
int nj_cnk_wire_vlist(int *p, char *vbuf)
{
    NjSysView *sys = NJ_SYS_VIEW;
    int head, type, size, n, ret;
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
            dst = vbuf + (p[1] & 0xffff) * 64;
            switch (type) {
            case 34:
                ret = nj_vu0_wire_points_clip(sys, p + 2, dst, n, 3);
                break;
            case 35:
                ret = nj_vu0_wire_points_clip(sys, p + 2, dst, n, 4);
                break;
            case 42:
                ret = nj_vu0_wire_points_clip(sys, p + 2, dst, n, 7);
                break;
            case 44:
                ret = nj_cnk_wire_vblock(sys, p, vbuf);
                break;
            case 41:
                ret = nj_vu0_wire_points_clip(sys, p + 2, dst, n, 6);
                break;
            default:
                ret = n;
                break;
            }
            sys->nvtx += n;
            nj_cnk_wire_clip_count += ret;
            p += size + 1;
        }
        head = *p;
    }
    return ret;
}

/* Returns 0. */
/* 0x001a79d8 */
static int nj_cnk_wire_draw(NjCnkModel *model)
{
    ulGxSetMatrix(0, ulMatrixCur);
    if (model->vlist)
        nj_cnk_wire_vlist(model->vlist, NJ_SYS_VIEW->vbuf);
    if (model->plist) {
        psGsOpenRefPkt();
        nj_cnk_wire_plist(model->plist);
        psGsCloseRefPkt();
    }
    return 0;
}

/* Same as nj_cnk_wire_draw. */
/* 0x001a7a40 */
static int nj_cnk_wire_draw2(NjCnkModel *model)
{
    ulGxSetMatrix(0, ulMatrixCur);
    if (model->vlist)
        nj_cnk_wire_vlist(model->vlist, NJ_SYS_VIEW->vbuf);
    if (model->plist) {
        psGsOpenRefPkt();
        nj_cnk_wire_plist(model->plist);
        psGsCloseRefPkt();
    }
    return 0;
}

/* 0x001a7aa8 */
static void nj_object_draw_wire(NjObject *obj)
{
    nj_object_walk(obj, nj_cnk_wire_draw);
}

/* As a line strip: on-screen runs whole; a vertex needing clipping is skipped. */
/* 0x001a7ab8 */
static void nj_cnk_wire_uv_strip(NjSysView *sys, NjCnkIdx *idx, int n, char *vbuf)
{
    PsVifPkt *pkt;
    NjCnkIdx *p;
    PsGifTag *tag;
    NjQword *d;
    int k, j;

    pkt = psGsGetRefPkt();
    sys->batch_tag[0] = NJ_GIFTAG(0, 0, 1, (sys->prim & ~7) | 2, 0, 2);
    sys->batch_tag[1] = 0x41;
    p = idx;
    while (n >= 3) {
        nj_vu0_wire_load_consts(sys);
        tag = (PsGifTag *)pkt->cur;
        d = (NjQword *)(tag + 1);
        k = nj_vu0_wire_emit_uv_strip_clip(sys, p, n, vbuf, d);
        if (k >= 3) {
            *(NjQword *)tag = *(NjQword *)sys->batch_tag;
            tag->nloop = k;
            pkt->gtag = (int)tag;
            j = k * 2;
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
            p++;
            n--;
        }
    }
}

/* 0x001a7c20 */
static void nj_cnk_wire_uv_strips(NjSysView *sys, short *p)
{
    int n, len, i;

    n = *p++;
    n &= 0x3fff;
    for (i = 0; i < n; i++) {
        len = abs(*p++);
        nj_cnk_wire_uv_strip(sys, (NjCnkIdx *)p, len, sys->vbuf);
        p += len * 3;
    }
}

/* As nj_cnk_wire_uv_strip. */
/* 0x001a7cb8 */
static void nj_cnk_wire_strip(NjSysView *sys, short *idx, int n, char *vbuf)
{
    PsVifPkt *pkt;
    short *p;
    PsGifTag *tag;
    NjQword *d;
    int k, j;

    pkt = psGsGetRefPkt();
    sys->batch_tag[0] = NJ_GIFTAG(0, 0, 1, (sys->prim & ~7) | 2, 0, 2);
    sys->batch_tag[1] = 0x41;
    p = idx;
    n = abs(n);
    while (n >= 3) {
        nj_vu0_wire_load_consts(sys);
        tag = (PsGifTag *)pkt->cur;
        d = (NjQword *)(tag + 1);
        k = nj_vu0_wire_emit_strip_clip(sys, p, n, vbuf, d);
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
            p++;
            n--;
        }
    }
}

/* 0x001a7e18 */
static void nj_cnk_wire_strips(NjSysView *sys, short *p)
{
    int n, len, i;

    n = *p++;
    n &= 0x3fff;
    for (i = 0; i < n; i++) {
        len = abs(*p++);
        nj_cnk_wire_strip(sys, p, len, sys->vbuf);
        p += len;
    }
}

/* Line-strip nj_cnk_gs_uv_strips_noclip. */
/* 0x001a7ea0 */
static void nj_cnk_wire_uv_strips_noclip(NjSysView *sys, short *p)
{
    PsGifTag tag;
    PsGifTag *gif;
    PsVifPkt *pkt;
    int n, len, i;

    tag = *(PsGifTag *)sys->tex_tag;
    tag.prim = (sys->prim & ~7) | 0x12;
    n = (*p++ << 2) >> 2 & 0x3fff;
    nj_vu0_wire_load_consts(sys);
    for (i = 0; i < n; i++) {
        len = *p++;
        pkt = psGsGetRefPkt();
        len = abs(len);
        gif = (PsGifTag *)psVifPktAddGifTag(pkt, (struct PsGifTag *)&tag);
        gif->nloop = len;
        psVifPktAdvanceGif(pkt, nj_vu0_wire_emit_uv_strip(sys, (NjCnkIdx *)p, len, sys->vbuf, gif + 1), len * 3);
        p += len * 3;
        sys->ntri += len - 2;
    }
}

/* Line-strip nj_cnk_gs_strips_noclip. */
/* 0x001a7ff0 */
static void nj_cnk_wire_strips_noclip(NjSysView *sys, short *p)
{
    PsGifTag tag;
    PsGifTag *gif;
    PsVifPkt *pkt;
    NjPkt *end;
    int n, len, i;

    tag = *(PsGifTag *)sys->gouraud_tag;
    tag.prim = (sys->prim & ~7) | 2;
    n = (*p++ << 2) >> 2 & 0x3fff;
    nj_vu0_wire_load_consts(sys);
    for (i = 0; i < n; i++) {
        len = *p++;
        pkt = psGsGetRefPkt();
        len = abs(len);
        gif = (PsGifTag *)psVifPktAddGifTag(pkt, (struct PsGifTag *)&tag);
        gif->nloop = len;
        end = nj_vu0_wire_emit_strip(sys, p, len, sys->vbuf, gif + 1);
        p += len;
        psVifPktAdvanceGif(pkt, end, len * 2);
        sys->ntri += len - 2;
    }
}
