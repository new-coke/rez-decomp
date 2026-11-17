/*
 * Graphics core: render state (ulGxState), model vertex and display-list interpreters via VU0 into
 * GIF packets, triangle and line clippers, lock sections, display start-up per video mode.
 */

#include "types_ul.h"
#include "qword.h"
#include "libc.h"
#include "libc_internal.h"
#include "ul_gx.h"
#include "libgraph.h"
#include "ps_displist.h"
#include "ps_gs.h"
#include "ps_gxpkt.h"
#include "ps_pkt.h"
#include "ps_timer.h"
#include "ul_gx_vertex.h"
#include "ul_matrix.h"
#include "ul_texture.h"
#include "ul_vu0.h"
#include "nj_cnk_vu1.h"
#include "ps_surf.h"

static void ulGxExecDlistClip(UlGxBig *gx, short *list);
static void ulGxExecDlistNoClip(UlGxBig *gx, short *list);
static void ulGxMakeFrustumMatrix(float *mtx, float dist, float znear, float zfar, float ext_x, float ext_y);

typedef struct ClipCtx {
    char pad_00[1168];
    float scale;            /* 1168 */
    char pad_494[12];
    float bias;             /* 1184 */
} ClipCtx;

/* Always zero (the never-taken tests below). */
#define AZ(p) ((((int)(p)) & 1) >> 1)

typedef int v128 REZ_QWORD;

typedef union GifWord {
    unsigned long v;
} GifWord;

typedef struct GxDev {
    char pad_00[4];
    void *cur;               /* 4 */
    char pad_08[20];
    int nreg;                /* 28 */
} GxDev;

typedef struct V4 {
    float x, y, z, w;
} V4;

typedef struct UlGxProj {
    float dist;               /* 0 */
    float xscale;               /* 4 */
    float yscale;               /* 8 */
    float znear;              /* 12 */
    float zfar;              /* 16 */
    float cx;              /* 20 */
    float cy;              /* 24 */
    char pad_1c[8];
    float ext_x;              /* 36 */
    float ext_y;              /* 40 */
} UlGxProj;

typedef struct GxInitDesc {
    int mode;               /* 0 */
    int tv_mode;                  /* 4 */
    int field_mode;                  /* 8 */
    int ntex;                  /* 12 */
    unsigned int size1;     /* 16 */
    unsigned int size2;     /* 20 */
    unsigned int base;      /* 24 */
} GxInitDesc;

#ifdef VERSION_US

/* Line reported by the display-mode assertion in ulGxInit. */
#define UL_GX_ASSERT_LINE 1613

#else

#define UL_GX_ASSERT_LINE 1618

#endif /* !VERSION_US */

extern UlGxBig ulGxState;
extern char ulGxLockBlock0[];
extern int ulGxClipVertexCount;
extern int ulGxClipPoolIndex;
extern UlClipVtx *ulGxClipListA[16];
extern UlClipVtx *ulGxClipListB[16];
extern UlClipVtx ulGxClipPool[];
extern int vu0_clip_split_tex_z(UlGxBig *, UlClipVtx *, UlClipVtx *, UlClipVtx *, float *);
extern int ul_tri_backfacing(UlClipVtx *, UlClipVtx *, UlClipVtx *, int);
extern void *vu0_gx_emit_clipped_tex(UlGxBig *, UlClipVtx **, int, void *);
extern int vu0_clip_split_z(UlGxBig *, UlClipVtx *, UlClipVtx *, UlClipVtx *, float *);
extern void *vu0_gx_emit_clipped(UlGxBig *, UlClipVtx **, int, void *);
extern void vu0_clip_split_y(UlGxBig *, UlClipVtx *, UlClipVtx *, UlClipVtx *, float);
extern void vu0_clip_split_x(UlGxBig *, UlClipVtx *, UlClipVtx *, UlClipVtx *, float);
extern int ul_line_clip_codes(UlClipVtx *, UlClipVtx *);
extern void ul_clip_split_z(UlGxClipBuf *, UlClipVtx *, UlClipVtx *, float);
extern int ul_clip_split_axis(UlClipVtx *, UlClipVtx *, UlClipVtx *, float, int);
extern char nj_cnk_override_rgba[];
extern unsigned long ulGxTexFuncTable[];
extern unsigned long ulGxTexFilterTable[];
extern void *vu0_gx_emit_uv_strip_cull(UlGxBig *, short *, int, int, unsigned long *);
extern void *vu0_gx_emit_tex_strip_cull(UlGxBig *, short *, int, int, unsigned long *);
extern void *vu0_gx_emit_strip_cull(UlGxBig *, short *, int, int, unsigned long *);
extern void vu0_gx_project_pos_clip(UlGxBig *, unsigned int *, int, int);
extern void vu0_gx_project_col_clip(UlGxBig *, unsigned int *, int, int);
extern void vu0_gx_project_lit_env_clip(UlGxBig *, unsigned int *, int, int);
extern void vu0_gx_project_col_env_clip(UlGxBig *, unsigned int *, int, int);
extern void vu0_gx_project_pos(UlGxBig *, unsigned int *, int, int);
extern void vu0_gx_project_col(UlGxBig *, unsigned int *, int, int);
extern void vu0_gx_project_lit_env(UlGxBig *, unsigned int *, int, int);
extern void vu0_gx_project_col_env(UlGxBig *, unsigned int *, int, int);
extern void vu0_vec_min(V4 *dst, V4 *a, V4 *b);
extern void vu0_vec_max(V4 *dst, V4 *a, V4 *b);
extern int ulGxDrawFlagWord;
extern UlGxCam ulGxProj;
extern char ulGxIdentityMatrix[];
extern void psVifPktAddUnpackMatrix(void *, unsigned int, void *);
extern UlGxProj ulGxFrustumState;
extern void vu0_matrix_identity(float *);
extern void vu0_matrix_zero(float *);
extern int ulGxCurTexList;
extern void *ulGxCurTexture;
extern void psAssertFail(const char *, int, char *);
extern int ulGxLockCount0;
extern int ulGxLockCount1;
extern int ulGxLockedPkt;
extern int psGxPktCur;
extern char ulGxVu1Program[];
extern void *ulGxInitTable[];

/* Bug: values land crosswise (x + width in ymin, y in xmax). Nothing calls it. */
/* 0x001b9618 */
static void ulGxSetClipWindow(float x, float y, float width, float height)
{
    ulGxState.xmin = x;
    ulGxState.ymin = x + width;
    ulGxState.xmax = y;
    ulGxState.ymax = y + height;
}

/* Strip, triangle, vertex counts and list timings. */
/* 0x001b9640 */
static void ulGxClearCounters(void)
{
    ulGxState.unk_91c = 0;
    ulGxState.unk_918 = 0;
    ulGxState.nstrip = 0;
    ulGxState.unk_92c = 0;
    ulGxState.ntri = 0;
    ulGxState.nvtx = 0;
    ulGxState.unk_938 = 0;
    ulGxState.dlist_ticks = 0;
    ulGxState.vlist_ticks = 0;
}

/* 640x224 centred at 2048, 2048, projection distance 500; clip and screen rectangles. */
/* 0x001b9670 */
static void ulGxInitProjection(UlGxBig *gx)
{
    UlGxCam *cam = &gx->cam;
    float half_width;

    cam->width = 640.0f;
    cam->height = 224.0f;
    cam->cy = cam->cx = 2048.0f;
    cam->unk_28 = 2.0f;
    cam->znear = -1.0f;
    cam->zfar = -10000.0f;
    cam->xscale = 1.0f;
    cam->yscale = 0.466666675f;
    cam->dist = 500.0f;
    gx->xmin = 16.0f;
    gx->ymin = 16.0f;
    gx->xmax = 4080.0f;
    gx->ymax = 4080.0f;
    half_width = cam->width * 0.5f;
    gx->scr_x0 = cam->cx - half_width;
    gx->scr_y0 = cam->cy - cam->height * 0.5f;
    gx->scr_x1 = cam->cx + half_width;
    gx->scr_y1 = cam->cy + cam->height * 0.5f;
}

/* 0x001b9750 */
static void ulGxStub(void)
{
}

/*
 * Colour and UV scale constants, GIF tag templates, default primitive mode, first lock block,
 * camera.
 */
/* 0x001b9758 */
static void ulGxResetGlobals(void)
{
    UlGxBig *gx = &ulGxState;

    memset(gx, 0, 2368);
    gx->unk_5f4 = 1.52590219e-05f;
    gx->sub = (UlGxSub *)ulGxLockBlock0;
    gx->col_max_r = 255.0f;
    gx->unk_5f0 = 64;
    gx->map_scale_z = 0.0f;
    gx->k_255 = gx->col_max_b = gx->col_max_g = 255.0f;
    gx->k_128 = gx->col_one_a = gx->col_one_b = gx->col_one_g = gx->col_one_r = gx->col_max_a = 128.0f;
    gx->k_inv256 = 0.00390625f;
    gx->k_inv65536 = 1.52590219e-05f;
    gx->col_scale_b = gx->col_scale_g = gx->col_scale = 0.00392156886f;
    gx->alpha_scale = 0.0078125f;
    gx->map_scale_y = -0.5f;
    gx->map_ofs_y = gx->map_ofs_x = gx->map_scale_x = 0.5f;
    gx->tag_tri = 0x8001UL << 46;
    gx->tag_tex = 0xc001UL << 46;
    gx->tag_tri_regs = 65;
    gx->tag2 = 0x8009UL << 46;
    gx->map_scale_w = 0.0f;
    gx->tag_tex_regs = 1042;
    gx->tag2_regs = 65;
    gx->map_ofs_z = 0.0f;
    gx->map_ofs_w = 0.0f;
    gx->tag3 = 0xc009UL << 46;
    gx->tag3_regs = 1042;
    gx->tex1 = 0xfc00UL << 28;
    gx->prim2d = gx->prim = 8;
    ulGxInitProjection(gx);
}

/* Edge from..to at x = bound, all attributes interpolated. Prints "???" when out is an input. */
/* 0x001b98a8 */
static void ulGxClipLerpX(void *gx, UlClipVtx *from, UlClipVtx *to, UlClipVtx *out, float bound)
{
    float t;
    float one_minus_t;

    if (from == out || to == out)
        printf("???\n");
    t = (bound - from->f[4]) / (to->f[4] - from->f[4]);
    out->f[4] = bound;
    one_minus_t = 1.0f - t;
    out->f[5] = from->f[5] * one_minus_t + to->f[5] * t;
    out->f[6] = from->f[6] * one_minus_t + to->f[6] * t;
    out->f[7] = from->f[7] * one_minus_t + to->f[7] * t;
    out->f[11] = from->f[11] * one_minus_t + to->f[11] * t;
    out->f[8] = from->f[8] * one_minus_t + to->f[8] * t;
    out->f[9] = from->f[9] * one_minus_t + to->f[9] * t;
    out->f[10] = from->f[10] * one_minus_t + to->f[10] * t;
    out->f[12] = from->f[12] * one_minus_t + to->f[12] * t;
    out->f[13] = from->f[13] * one_minus_t + to->f[13] * t;
    out->f[14] = from->f[14] * one_minus_t + to->f[14] * t;
    out->flags = 0;
}

/*
 * Edge from..to at y = bound, all attributes interpolated, left/right outcodes redone. "???" if out
 * is an input.
 */
/* 0x001b9a28 */
static void ulGxClipLerpY(ClipCtx *gx, UlClipVtx *from, UlClipVtx *to, UlClipVtx *out, float bound)
{
    float t;
    float one_minus_t;
    float y;

    if (from == out || to == out)
        printf("???\n");
    t = (bound - from->f[5]) / (to->f[5] - from->f[5]);
    one_minus_t = 1.0f - t;
    out->f[4] = from->f[4] * one_minus_t + to->f[4] * t;
    out->f[5] = bound;
    out->f[6] = from->f[6] * one_minus_t + to->f[6] * t;
    out->f[7] = from->f[7] * one_minus_t + to->f[7] * t;
    out->f[11] = from->f[11] * one_minus_t + to->f[11] * t;
    out->f[8] = from->f[8] * one_minus_t + to->f[8] * t;
    out->f[9] = from->f[9] * one_minus_t + to->f[9] * t;
    out->f[10] = from->f[10] * one_minus_t + to->f[10] * t;
    out->f[12] = from->f[12] * one_minus_t + to->f[12] * t;
    out->f[13] = from->f[13] * one_minus_t + to->f[13] * t;
    out->f[14] = from->f[14] * one_minus_t + to->f[14] * t;
    out->flags = 0;
    y = out->f[4] * gx->scale + gx->bias;
    if (y < -1.0f)
        out->flags = 2;
    if (y > 1.0f)
        out->flags |= 1;
}

/*
 * Near plane, then (any vertex outside) ymin/ymax and xmin/xmax; drawn as a fan. cull drops
 * back faces. New vertices from ulGxClipPool.
 */
/* 0x001b9bf8 */
void ulGxClipDrawTexTriangle(UlGxBig *gx, UlClipVtx *vtx0, UlClipVtx *vtx1, UlClipVtx *vtx2, int cull)
{
    UlGxGifTag tag;
    void *pkt;
    float *near = &gx->scr_x0;
    UlClipVtx **in;
    UlClipVtx **out;
    UlClipVtx *cur;
    UlClipVtx *next;
    UlClipVtx *nv;
    UlGxGifTag *gt;
    void *pkt_end;
    unsigned int flags;
    int i;
    int cnt;
    int code;
    float lo;
    float hi;

    i = 0;
    pkt = psGsGetRefPkt();
    out = ulGxClipListB;
    ulGxClipPoolIndex = 0;
    ulGxClipVertexCount = 3;
    cnt = 0;
    ulGxClipListA[1] = vtx1;
    ulGxClipListA[2] = vtx2;
    flags = 0;
    ulGxClipListA[0] = vtx0;
    ulGxClipListA[3] = vtx0;
    in = ulGxClipListA;
    do {
        cur = in[i];
        next = in[i + 1];
        code = 0;
        if (cur->flags & 0x20)
            code = 1;
        if (next->flags & 0x20)
            code |= 2;
        switch (code) {
        case 0:
            *out++ = cur;
            flags |= cur->flags;
            cnt++;
            break;
        case 1:
            nv = &ulGxClipPool[ulGxClipPoolIndex];
            vu0_clip_split_tex_z(gx, cur, next, nv, near);
            cnt++;
            *out = nv;
            flags |= nv->flags;
            ulGxClipPoolIndex++;
            out++;
            break;
        case 2:
            *out++ = cur;
            flags |= cur->flags;
            nv = &ulGxClipPool[ulGxClipPoolIndex];
            vu0_clip_split_tex_z(gx, next, cur, nv, near);
            cnt += 2;
            *out = nv;
            ulGxClipPoolIndex++;
            flags |= nv->flags;
            out++;
            break;
        }
        i++;
    } while (i < ulGxClipVertexCount);
    if (cnt == 0)
        return;
    if (cull != 0 && ul_tri_backfacing(ulGxClipListB[0], ulGxClipListB[1], ulGxClipListB[2], cull) != 0)
        return;
    if ((flags & 0xf) == 0) {
        *(unsigned long *)&tag = gx->tag_tex;
        tag.PRIM = (gx->prim & ~7UL) | 0x15;
        tag.NLOOP = cnt;
        ((unsigned long *)&tag)[1] = gx->tag_tex_regs;
        gt = psVifPktAddGifTag(pkt, &tag);
        pkt_end = vu0_gx_emit_clipped_tex(gx, ulGxClipListB, cnt, gt + 1);
        psVifPktAdvanceGif(pkt, pkt_end, cnt * 3);
        return;
    }

    in = ulGxClipListB;
    ulGxClipVertexCount = cnt;
    in[cnt] = in[0];
    cnt = 0;
    lo = gx->ymin;
    hi = gx->ymax;
    out = ulGxClipListA;
    for (i = 0; i < ulGxClipVertexCount; i++) {
        cur = in[i];
        next = in[i + 1];
        code = (cur->flags & 8) ? 1 : ((cur->flags & 4) ? 2 : 0);
        if (next->flags & 8)
            code |= 4;
        else if (next->flags & 4)
            code |= 8;
        switch (code) {
        case 0:
            *out++ = cur;
            cnt++;
            break;
        case 1:
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            ulGxClipLerpY(gx, cur, next, nv, lo);
            cnt++;
            *out++ = nv;
            break;
        case 2:
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            ulGxClipLerpY(gx, next, cur, nv, hi);
            cnt++;
            *out++ = nv;
            break;
        case 4:
            *out++ = cur;
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            ulGxClipLerpY(gx, next, cur, nv, lo);
            cnt += 2;
            *out++ = nv;
            break;
        case 6:
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            ulGxClipLerpY(gx, next, cur, nv, hi);
            cnt += 2;
            *out++ = nv;
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            ulGxClipLerpY(gx, next, cur, nv, lo);
            *out++ = nv;
            break;
        case 8:
            *out++ = cur;
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            ulGxClipLerpY(gx, cur, next, nv, hi);
            cnt += 2;
            *out++ = nv;
            break;
        case 9:
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            ulGxClipLerpY(gx, cur, next, nv, lo);
            cnt += 2;
            *out++ = nv;
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            ulGxClipLerpY(gx, cur, next, nv, hi);
            *out++ = nv;
            break;
        case 5:
        case 10:
            break;
        }
    }

    in = ulGxClipListA;
    ulGxClipVertexCount = cnt;
    in[cnt] = in[0];
    cnt = 0;
    lo = gx->xmin;
    hi = gx->xmax;
    out = ulGxClipListB;
    for (i = 0; i < ulGxClipVertexCount; i++) {
        cur = in[i];
        next = in[i + 1];
        code = (cur->flags & 2) ? 1 : ((cur->flags & 1) ? 2 : 0);
        if (next->flags & 2)
            code |= 4;
        else if (next->flags & 1)
            code |= 8;
        switch (code) {
        case 0:
            *out++ = cur;
            cnt++;
            break;
        case 1:
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            ulGxClipLerpX(gx, cur, next, nv, lo);
            cnt++;
            *out++ = nv;
            break;
        case 2:
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            ulGxClipLerpX(gx, next, cur, nv, hi);
            cnt++;
            *out++ = nv;
            break;
        case 4:
            *out++ = cur;
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            ulGxClipLerpX(gx, next, cur, nv, lo);
            cnt += 2;
            *out++ = nv;
            break;
        case 6:
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            ulGxClipLerpX(gx, cur, next, nv, hi);
            cnt += 2;
            *out++ = nv;
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            ulGxClipLerpX(gx, cur, next, nv, lo);
            *out++ = nv;
            break;
        case 8:
            *out++ = cur;
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            ulGxClipLerpX(gx, cur, next, nv, hi);
            cnt += 2;
            *out++ = nv;
            break;
        case 9:
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            ulGxClipLerpX(gx, next, cur, nv, lo);
            cnt += 2;
            *out++ = nv;
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            ulGxClipLerpX(gx, next, cur, nv, hi);
            *out++ = nv;
            break;
        case 5:
        case 10:
            break;
        }
    }
    if (cnt < 3)
        return;
    *(unsigned long *)&tag = gx->tag_tex;
    tag.PRIM = (gx->prim & ~7UL) | 0x15;
    tag.NLOOP = cnt;
    ((unsigned long *)&tag)[1] = gx->tag_tex_regs;
    gt = psVifPktAddGifTag(pkt, &tag);
    pkt_end = vu0_gx_emit_clipped_tex(gx, ulGxClipListB, cnt, gt + 1);
    psVifPktAdvanceGif(pkt, pkt_end, cnt * 3);
}

/* Untextured ulGxClipDrawTexTriangle. */
/* 0x001ba488 */
void ulGxClipDrawTriangle(UlGxBig *gx, UlClipVtx *vtx0, UlClipVtx *vtx1, UlClipVtx *vtx2, int cull)
{
    UlGxGifTag tag;
    void *pkt;
    float *near = &gx->scr_x0;
    UlClipVtx **in;
    UlClipVtx **out;
    UlClipVtx *cur;
    UlClipVtx *next;
    UlClipVtx *nv;
    UlGxGifTag *gt;
    void *pkt_end;
    unsigned int flags;
    int i;
    int cnt;
    int code;
    float lo;
    float hi;

    i = 0;
    pkt = psGsGetRefPkt();
    out = ulGxClipListB;
    ulGxClipPoolIndex = 0;
    ulGxClipVertexCount = 3;
    cnt = 0;
    ulGxClipListA[1] = vtx1;
    ulGxClipListA[2] = vtx2;
    flags = 0;
    ulGxClipListA[0] = vtx0;
    ulGxClipListA[3] = vtx0;
    in = ulGxClipListA;
    do {
        cur = in[i];
        next = in[i + 1];
        code = 0;
        if (cur->flags & 0x20)
            code = 1;
        if (next->flags & 0x20)
            code |= 2;
        switch (code) {
        case 0:
            *out++ = cur;
            flags |= cur->flags;
            cnt++;
            break;
        case 1:
            nv = &ulGxClipPool[ulGxClipPoolIndex];
            vu0_clip_split_z(gx, cur, next, nv, near);
            cnt++;
            *out = nv;
            flags |= nv->flags;
            ulGxClipPoolIndex++;
            out++;
            break;
        case 2:
            *out++ = cur;
            flags |= cur->flags;
            nv = &ulGxClipPool[ulGxClipPoolIndex];
            vu0_clip_split_z(gx, next, cur, nv, near);
            cnt += 2;
            *out = nv;
            ulGxClipPoolIndex++;
            flags |= nv->flags;
            out++;
            break;
        }
        i++;
    } while (i < ulGxClipVertexCount);
    if (cnt == 0)
        return;
    if (cull != 0 && ul_tri_backfacing(ulGxClipListB[0], ulGxClipListB[1], ulGxClipListB[2], cull) != 0)
        return;
    if ((flags & 0xf) == 0) {
        *(unsigned long *)&tag = gx->tag_tri;
        tag.PRIM = (gx->prim & ~7UL) | 5;
        tag.NLOOP = cnt;
        ((unsigned long *)&tag)[1] = gx->tag_tri_regs;
        gt = psVifPktAddGifTag(pkt, &tag);
        pkt_end = vu0_gx_emit_clipped(gx, ulGxClipListB, cnt, gt + 1);
        psVifPktAdvanceGif(pkt, pkt_end, cnt * 2);
        return;
    }

    in = ulGxClipListB;
    ulGxClipVertexCount = cnt;
    in[cnt] = in[0];
    cnt = 0;
    lo = gx->ymin;
    hi = gx->ymax;
    out = ulGxClipListA;
    for (i = 0; i < ulGxClipVertexCount; i++) {
        cur = in[i];
        next = in[i + 1];
        code = (cur->flags & 8) ? 1 : ((cur->flags & 4) ? 2 : 0);
        if (next->flags & 8)
            code |= 4;
        else if (next->flags & 4)
            code |= 8;
        switch (code) {
        case 0:
            *out++ = cur;
            cnt++;
            break;
        case 1:
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            vu0_clip_split_y(gx, cur, next, nv, lo);
            cnt++;
            *out++ = nv;
            break;
        case 2:
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            vu0_clip_split_y(gx, next, cur, nv, hi);
            cnt++;
            *out++ = nv;
            break;
        case 4:
            *out++ = cur;
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            vu0_clip_split_y(gx, next, cur, nv, lo);
            cnt += 2;
            *out++ = nv;
            break;
        case 6:
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            vu0_clip_split_y(gx, next, cur, nv, hi);
            cnt += 2;
            *out++ = nv;
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            vu0_clip_split_y(gx, next, cur, nv, lo);
            *out++ = nv;
            break;
        case 8:
            *out++ = cur;
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            vu0_clip_split_y(gx, cur, next, nv, hi);
            cnt += 2;
            *out++ = nv;
            break;
        case 9:
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            vu0_clip_split_y(gx, cur, next, nv, lo);
            cnt += 2;
            *out++ = nv;
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            vu0_clip_split_y(gx, cur, next, nv, hi);
            *out++ = nv;
            break;
        case 5:
        case 10:
            break;
        }
    }

    in = ulGxClipListA;
    ulGxClipVertexCount = cnt;
    in[cnt] = in[0];
    cnt = 0;
    lo = gx->xmin;
    hi = gx->xmax;
    out = ulGxClipListB;
    for (i = 0; i < ulGxClipVertexCount; i++) {
        cur = in[i];
        next = in[i + 1];
        code = (cur->flags & 2) ? 1 : ((cur->flags & 1) ? 2 : 0);
        if (next->flags & 2)
            code |= 4;
        else if (next->flags & 1)
            code |= 8;
        switch (code) {
        case 0:
            *out++ = cur;
            cnt++;
            break;
        case 1:
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            vu0_clip_split_x(gx, cur, next, nv, lo);
            cnt++;
            *out++ = nv;
            break;
        case 2:
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            vu0_clip_split_x(gx, next, cur, nv, hi);
            cnt++;
            *out++ = nv;
            break;
        case 4:
            *out++ = cur;
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            vu0_clip_split_x(gx, next, cur, nv, lo);
            cnt += 2;
            *out++ = nv;
            break;
        case 6:
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            vu0_clip_split_x(gx, cur, next, nv, hi);
            cnt += 2;
            *out++ = nv;
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            vu0_clip_split_x(gx, cur, next, nv, lo);
            *out++ = nv;
            break;
        case 8:
            *out++ = cur;
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            vu0_clip_split_x(gx, cur, next, nv, hi);
            cnt += 2;
            *out++ = nv;
            break;
        case 9:
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            vu0_clip_split_x(gx, next, cur, nv, lo);
            cnt += 2;
            *out++ = nv;
            nv = &ulGxClipPool[ulGxClipPoolIndex++];
            vu0_clip_split_x(gx, next, cur, nv, hi);
            *out++ = nv;
            break;
        case 5:
        case 10:
            break;
        }
    }
    if (cnt < 3)
        return;
    *(unsigned long *)&tag = gx->tag_tri;
    tag.PRIM = (gx->prim & ~7UL) | 5;
    tag.NLOOP = cnt;
    ((unsigned long *)&tag)[1] = gx->tag_tri_regs;
    gt = psVifPktAddGifTag(pkt, &tag);
    pkt_end = vu0_gx_emit_clipped(gx, ulGxClipListB, cnt, gt + 1);
    psVifPktAdvanceGif(pkt, pkt_end, cnt * 2);
}

/* Clip space: near plane and -1..1 x/y. 0 if gone, else 2 with the ends in cb->out. */
/* 0x001bad10 */
static int ulGxClipLine(UlGxClipBuf *cb, UlClipVtx *p0, UlClipVtx *p1)
{
    UlClipVtx **in;
    UlClipVtx **out;
    UlClipVtx **swap;
    UlClipVtx *from;
    UlClipVtx *nv;
    int used;
    int codes;
    UlClipVtx *to;

    in = cb->in_list;
    out = cb->out_list;
    used = 0;
    cb->in_list[0] = p0;
    in[1] = p1;
    to = p1;
    cb->nout = 2;
    from = cb->in_list[0];
    codes = ul_line_clip_codes(from, to);
    codes = ((codes >> 10) & 2) | ((codes & 0x20) >> 5);
    switch (codes) {
    case 0:
        cb->out_list[0] = from;
        out[1] = to;
        break;
    case 1:
        cb->out_list[0] = from;
        ul_clip_split_z(cb, from, to, -1.0f);
        used = 1;
        out[1] = &cb->pool[0];
        break;
    case 2:
        ul_clip_split_z(cb, to, from, -1.0f);
        used = 1;
        cb->out_list[0] = &cb->pool[0];
        out[1] = to;
        break;
    default:
        return 0;
    }

    swap = in;
    in = out;
    out = swap;
    from = in[0];
    to = in[1];
    codes = ul_line_clip_codes(from, to);
    codes = ((codes >> 4) & 0xc) | (codes & 3);
    switch (codes) {
    case 0:
        out[0] = from;
        out[1] = to;
        break;
    case 1:
        out[0] = from;
        nv = &cb->pool[used];
        ul_clip_split_axis(nv, from, to, 1.0f, 0);
        used++;
        out[1] = nv;
        break;
    case 2:
        out[0] = from;
        nv = &cb->pool[used];
        ul_clip_split_axis(nv, to, from, -1.0f, 0);
        used++;
        out[1] = nv;
        break;
    case 4:
        nv = &cb->pool[used];
        ul_clip_split_axis(nv, to, from, 1.0f, 0);
        used++;
        out[0] = nv;
        out[1] = to;
        break;
    case 6:
        nv = &cb->pool[used];
        ul_clip_split_axis(nv, to, from, 1.0f, 0);
        used++;
        out[0] = nv;
        nv = &cb->pool[used];
        ul_clip_split_axis(nv, to, from, -1.0f, 0);
        used++;
        out[1] = nv;
        break;
    case 8:
        nv = &cb->pool[used];
        ul_clip_split_axis(nv, from, to, -1.0f, 0);
        used++;
        out[0] = nv;
        out[1] = to;
        break;
    case 9:
        nv = &cb->pool[used];
        ul_clip_split_axis(nv, from, to, -1.0f, 0);
        used++;
        out[0] = nv;
        nv = &cb->pool[used];
        ul_clip_split_axis(nv, from, to, 1.0f, 0);
        used++;
        out[1] = nv;
        break;
    default:
        return 0;
    }

    swap = in;
    in = out;
    out = swap;
    from = in[0];
    to = in[1];
    codes = ul_line_clip_codes(from, to);
    codes = ((codes >> 6) & 0xc) | ((codes & 0xc) >> 2);
    switch (codes) {
    case 0:
        out[0] = from;
        out[1] = to;
        break;
    case 1:
        out[0] = from;
        nv = &cb->pool[used];
        ul_clip_split_axis(nv, from, to, 1.0f, 1);
        used++;
        out[1] = nv;
        break;
    case 2:
        out[0] = from;
        nv = &cb->pool[used];
        ul_clip_split_axis(nv, to, from, -1.0f, 1);
        used++;
        out[1] = nv;
        break;
    case 4:
        nv = &cb->pool[used];
        ul_clip_split_axis(nv, to, from, 1.0f, 1);
        used++;
        out[0] = nv;
        out[1] = to;
        break;
    case 6:
        nv = &cb->pool[used];
        ul_clip_split_axis(nv, to, from, 1.0f, 1);
        used++;
        out[0] = nv;
        nv = &cb->pool[used];
        ul_clip_split_axis(nv, to, from, -1.0f, 1);
        used++;
        out[1] = nv;
        break;
    case 8:
        nv = &cb->pool[used];
        ul_clip_split_axis(nv, from, to, -1.0f, 1);
        used++;
        out[0] = nv;
        out[1] = to;
        break;
    case 9:
        nv = &cb->pool[used];
        ul_clip_split_axis(nv, from, to, -1.0f, 1);
        used++;
        out[0] = nv;
        nv = &cb->pool[used];
        ul_clip_split_axis(nv, from, to, 1.0f, 1);
        used++;
        out[1] = nv;
        break;
    default:
        return 0;
    }
    cb->used = used;
    cb->out = out;
    return 2;
}

/* Clip space: near plane and -1..1 x/y. Returns the vertex count (0 = gone); polygon in cb->out. */
/* 0x001bb158 */
int ulGxClipTriangle(UlGxClipBuf *cb, UlClipVtx *vtx0, UlClipVtx *vtx1, UlClipVtx *vtx2)
{
    int cnt;
    int i;
    UlClipVtx **out;
    UlClipVtx **in;
    UlClipVtx *nv;
    int used;
    int n;
    UlClipVtx **swap;
    int near01;
    int near2;
    int near_code;

    in = cb->in_list;
    out = cb->out_list;
    used = 0;
    cb->nout = 3;
    near01 = ((vtx0->flags & 0x20) >> 3) | ((vtx1->flags & 0x20) >> 4);
    near2 = (vtx2->flags & 0x20) >> 5;
    in[3] = vtx0;
    cb->in_list[0] = vtx0;
    near_code = near01 | near2;
    in[1] = vtx1;
    in[2] = vtx2;
    switch (near_code) {
    case 0:
        out[0] = vtx0;
        out[1] = vtx1;
        out[2] = vtx2;
        cnt = 3;
        break;
    case 1:
        nv = &cb->pool[used];
        out[0] = vtx0;
        out[1] = vtx1;
        ul_clip_split_axis(nv, vtx1, vtx2, -1.0f, 2);
        used = 2;
        out[2] = nv;
        nv = &cb->pool[1];
        ul_clip_split_axis(nv, vtx0, vtx2, -1.0f, 2);
        cnt = 4;
        out[3] = nv;
        break;
    case 2:
        nv = &cb->pool[used];
        out[0] = vtx0;
        ul_clip_split_axis(nv, vtx0, vtx1, -1.0f, 2);
        used = 2;
        cnt = 4;
        out[1] = nv;
        nv = &cb->pool[1];
        ul_clip_split_axis(nv, vtx2, vtx1, -1.0f, 2);
        out[2] = nv;
        out[3] = vtx2;
        break;
    case 3:
        nv = &cb->pool[used];
        out[0] = vtx0;
        ul_clip_split_axis(nv, vtx0, vtx1, -1.0f, 2);
        used = 2;
        cnt = 3;
        out[1] = nv;
        nv = &cb->pool[1];
        ul_clip_split_axis(nv, vtx0, vtx2, -1.0f, 2);
        out[2] = nv;
        break;
    case 4:
        nv = &cb->pool[used];
        ul_clip_split_axis(nv, vtx1, vtx0, -1.0f, 2);
        used = 2;
        out[0] = nv;
        out[1] = vtx1;
        out[2] = vtx2;
        nv = &cb->pool[1];
        ul_clip_split_axis(nv, vtx2, vtx0, -1.0f, 2);
        cnt = 4;
        out[3] = nv;
        break;
    case 5:
        nv = &cb->pool[used];
        ul_clip_split_axis(nv, vtx1, vtx0, -1.0f, 2);
        used = 2;
        out[0] = nv;
        out[1] = vtx1;
        nv = &cb->pool[1];
        ul_clip_split_axis(nv, vtx1, vtx2, -1.0f, 2);
        cnt = 3;
        out[2] = nv;
        break;
    case 6:
        nv = &cb->pool[used];
        ul_clip_split_axis(nv, vtx2, vtx1, -1.0f, 2);
        used = 2;
        out[0] = nv;
        out[1] = vtx2;
        nv = &cb->pool[1];
        ul_clip_split_axis(nv, vtx2, vtx0, -1.0f, 2);
        cnt = 3;
        out[2] = nv;
        break;
    default:
        if (AZ(vtx0)) {   /* never taken */
            near_code = (int)out + AZ(out);
            out[0] = (UlClipVtx *)near_code;
            out[1] = (UlClipVtx *)near_code;
        }
        return 0;
    }

    /* x: outcode bits 0 (+) and 1 (-) */
    out[cnt] = out[0];
    n = cnt;
    swap = in;
    in = out;
    out = swap;
    cnt = 0;
    for (i = 0; i < n; i++) {
        vtx0 = in[i];
        vtx1 = in[i + 1];
        switch (((vtx0->flags & 3) << 2) | (vtx1->flags & 3)) {
        case 0:
            out[cnt++] = vtx0;
            break;
        case 1:
            out[cnt++] = vtx0;
            nv = &cb->pool[used];
            ul_clip_split_axis(nv, vtx0, vtx1, 1.0f, 0);
            used++;
            out[cnt++] = nv;
            break;
        case 2:
            out[cnt++] = vtx0;
            nv = &cb->pool[used];
            ul_clip_split_axis(nv, vtx1, vtx0, -1.0f, 0);
            used++;
            out[cnt++] = nv;
            break;
        case 4:
            nv = &cb->pool[used];
            ul_clip_split_axis(nv, vtx1, vtx0, 1.0f, 0);
            out[cnt++] = nv;
            used++;
            break;
        case 6:
            nv = &cb->pool[used];
            ul_clip_split_axis(nv, vtx1, vtx0, 1.0f, 0);
            used++;
            out[cnt++] = nv;
            nv = &cb->pool[used];
            ul_clip_split_axis(nv, vtx1, vtx0, -1.0f, 0);
            used++;
            out[cnt++] = nv;
            break;
        case 8:
            nv = &cb->pool[used];
            ul_clip_split_axis(nv, vtx0, vtx1, -1.0f, 0);
            out[cnt++] = nv;
            used++;
            break;
        case 9:
            nv = &cb->pool[used];
            ul_clip_split_axis(nv, vtx0, vtx1, -1.0f, 0);
            used++;
            out[cnt++] = nv;
            nv = &cb->pool[used];
            ul_clip_split_axis(nv, vtx0, vtx1, 1.0f, 0);
            used++;
            out[cnt++] = nv;
            break;
        case 5:
        case 10:
            break;
        }
    }

    /* y: outcode bits 2 (+) and 3 (-) */
    out[cnt] = out[0];
    n = cnt;
    swap = in;
    in = out;
    out = swap;
    cnt = 0;
    for (i = 0; i < n; i++) {
        vtx1 = in[i + 1];
        vtx0 = in[i];
        switch ((vtx0->flags & 0xc) | ((vtx1->flags & 0xc) >> 2)) {
        case 0:
            out[cnt++] = vtx0;
            break;
        case 1:
            out[cnt++] = vtx0;
            nv = &cb->pool[used];
            ul_clip_split_axis(nv, vtx0, vtx1, 1.0f, 1);
            used++;
            out[cnt++] = nv;
            break;
        case 2:
            out[cnt++] = vtx0;
            nv = &cb->pool[used];
            ul_clip_split_axis(nv, vtx1, vtx0, -1.0f, 1);
            used++;
            out[cnt++] = nv;
            break;
        case 4:
            nv = &cb->pool[used];
            ul_clip_split_axis(nv, vtx1, vtx0, 1.0f, 1);
            out[cnt++] = nv;
            used++;
            break;
        case 6:
            nv = &cb->pool[used];
            ul_clip_split_axis(nv, vtx1, vtx0, 1.0f, 1);
            used++;
            out[cnt++] = nv;
            nv = &cb->pool[used];
            ul_clip_split_axis(nv, vtx1, vtx0, -1.0f, 1);
            used++;
            out[cnt++] = nv;
            break;
        case 8:
            nv = &cb->pool[used];
            ul_clip_split_axis(nv, vtx0, vtx1, -1.0f, 1);
            out[cnt++] = nv;
            used++;
            break;
        case 9:
            nv = &cb->pool[used];
            ul_clip_split_axis(nv, vtx0, vtx1, -1.0f, 1);
            used++;
            out[cnt++] = nv;
            nv = &cb->pool[used];
            ul_clip_split_axis(nv, vtx0, vtx1, 1.0f, 1);
            used++;
            out[cnt++] = nv;
            break;
        case 5:
        case 10:
            break;
        }
    }
    out[cnt] = out[0];
    n = cnt;
    cb->out = out;
    cb->used = used;
    cb->nout = n;
    return n;
}

/*
 * Up to three RGBA byte colours per fmt, scaled to 0..1. nj_cnk_override_flags may replace or add
 * nj_cnk_override_rgba to the first.
 */
/* 0x001bb988 */
void ulGxDlReadAttr(UlGxBig *gx, unsigned char fmt, unsigned char *p)
{
    float *col = &gx->col0;
    int mask = fmt - 16;
    float k = 0.00392156886f;

    if (mask & 1) {
        if (nj_cnk_override_flags & 0x10) {
            vu0_qword_copy(col, nj_cnk_override_rgba);
            p += 4;
        } else {
            col[2] = *p++ * k;
            col[1] = *p++ * k;
            gx->col0 = *p++ * k;
            col[3] = *p++ * k;
            if (nj_cnk_override_flags & 0x20) {
                vu0_vec_add(col, col, nj_cnk_override_rgba);
                vu0_vec_clamp(col, col, 0.0f, 1.0f);
            }
        }
    }
    if (mask & 2) {
        col[6] = *p++ * k;
        col[5] = *p++ * k;
        col[4] = *p++ * k;
        col[7] = *p++ * k;
    }
    if (mask & 4) {
        col[10] = *p++ * k;
        col[9] = *p++ * k;
        col[8] = *p++ * k;
        col[11] = *p++ * k;
    }
}

/* From ulGxAlphaTable. */
/* 0x001bbb60 */
void ulGxDlSetAlphaMode(UlGxBig *gx, unsigned int mode)
{
    gx->alpha_mode = mode;
    gx->alpha = ulGxAlphaTable[(mode & 0x3f00) >> 8];
}

/*
 * 0x800 alpha blend, 0x2000 flat; 0x400/0x1000 change VU1 material constants. With override 0x800,
 * nj_cnk_strip_and_mask and nj_cnk_strip_or_bits override.
 */
/* 0x001bbb88 */
void ulGxSetRenderFlags(UlGxBig *gx, unsigned int flags)
{
    char *base;
    char *col_ofs;
    void *dev;
    unsigned long prim;
    unsigned int blend;
    unsigned int and_mask;
    unsigned int or_bits;
    unsigned long abe_bit;
    int iip_bit;

    base = (char *)gx;
    gx->render_flags = flags;
    if (nj_cnk_override_flags & 0x800) {
        and_mask = nj_cnk_strip_and_mask & 0xff00;
        or_bits = nj_cnk_strip_or_bits & 0xff00;
        flags &= and_mask;
        flags |= or_bits;
    }
    blend = flags & 0x800;
    abe_bit = blend ? 64 : 0;
    iip_bit = (flags & 0x2000) ? 0 : 8;
    prim = gx->prim & ~0x48UL;
    prim |= iip_bit | abe_bit;
    gx->prim = prim;
    if (blend) {
        dev = psGsGetDisplayList();
        psDisplayListUpdateGsReg(dev, 66, gx->alpha);
    }
    *(v128 *)(base + 976) = *(v128 *)(base + 2208);
    if (flags & 0x400) {
        col_ofs = base + 992;
        *(v128 *)col_ofs = REZ_QWORD_ZERO(v128);
    } else {
        col_ofs = base + 992;
        vu0_vec_mul(col_ofs, base + 2224, base + 960);
    }
    if (flags & 0x1000)
        *(int *)(base + 1036) = 0;
    else
        *(int *)(base + 1036) = 0x8000;
    dev = psGsGetRefPkt();
    psVifPktAddWord(dev, 0x1000101);
    psVifPktAddUnpack(dev, 0x6c01003d, base + 976, 1);
    psVifPktAddUnpack(dev, 0x6c01003e, col_ofs, 1);
    psVifPktAddUnpack(dev, 0x6c010040, base + 1024, 1);
}

/* Filter and clamp encoded in sel and mode. Warns for an index outside the list. */
/* 0x001bbce0 */
void ulGxDlSetTexture(UlGxBig *gx, short mode, short sel)
{
    UlGxTexList *tl;
    UlGxTexRec *rec;
    unsigned long hi;
    unsigned long lo;
    unsigned long tex1;
    unsigned long tex0;
    unsigned long tex1_reg;
    unsigned long gx_tex0;
    unsigned long gx_tex1;
    void *dev;
    int tex_sel;
    int idx;

    gx->tex_mode = mode;
    tl = gx->texlist;
    if (tl == 0)
        return;
    tex_sel = sel;
    hi = ulGxTexFilterTable[(tex_sel & 0xc000) >> 14];
    lo = ulGxTexFuncTable[(mode & 0x3000) >> 12];
    idx = tex_sel & 0xffff1fff;
    if (idx >= 0 && idx < tl->count) {
        gx->texidx = idx;
        rec = tl->ent[idx].rec;
        dev = psGsGetDisplayList();
        tex1 = rec->tex1;
        tex0 = rec->tex0;
        tex1 = (tex1 & ~0x1e0UL) | hi;
        gx_tex0 = gx->tex0;
        gx_tex1 = gx->tex1;
        tex0 = (tex0 & ~0x1800000000UL) | (gx_tex0 & 0x1800000000UL);
        tex1_reg = (tex1 & ~0x0FFF00180001UL) | (gx_tex1 & 0x0FFF00180001UL);
        psDisplayListSetGsReg(dev, 20, tex1_reg);
        psDisplayListSetGsReg(dev, 6, tex0);
        psDisplayListSetGsReg(dev, 8, lo);
        if (rec->nextra > 0) {
            unsigned long miptbp2 = rec->miptbp2;

            psDisplayListSetGsReg(dev, 52, rec->miptbp1);
            psDisplayListSetGsReg(dev, 54, miptbp2);
        }
        rec = 0;
        return;
    }
    printf("WARING: Texture index is illegal\n");
}

/* Each run starts with its signed length. */
/* steer(cast) */
/* 0x001bbeb8 */
static void ulGxDrawTexStripsClip(UlGxBig *gx, short *runs)
{
    short header = *runs;
    int nruns = header & 0x3fff;
    int n;
    int len;

    runs++;
    if (nruns) {
        n = nruns;
        do {
            len = *runs++;
            n--;
            ((void (*)(UlGxBig *, short *, int, int, int))ulGxEmitTexStrip)(gx, runs, len, gx->vbuf, gx->sub->unk_14);
            runs += len < 0 ? -len : len;
        } while (n);
    }
}

/* Separately indexed UVs scaled by uvscale. */
/* 0x001bbf48 */
static void ulGxDrawTexStripsUVClip(UlGxBig *gx, short *runs, float uvscale)
{
    short header;
    int nruns;
    int n;
    int len;

    gx->uvscale = uvscale;
    header = *runs;
    nruns = header & 0x3fff;
    runs++;
    if (nruns) {
        n = nruns;
        do {
            len = *runs++;
            ulGxEmitTexStripUV(gx, runs, len, gx->vbuf);
            n--;
            runs = (short *)((char *)runs + (len < 0 ? -len : len) * 6);
        } while (n);
    }
}

/* Untextured. */
/* steer(cast) */
/* 0x001bbfe0 */
static void ulGxDrawStripsClip(UlGxBig *gx, short *runs)
{
    short header = *runs;
    int nruns = header & 0x3fff;
    int n;
    int len;

    runs++;
    if (nruns) {
        n = nruns;
        do {
            len = *runs++;
            n--;
            ((void (*)(UlGxBig *, short *, int, int, int))ulGxEmitStrip)(gx, runs, len, gx->vbuf, gx->sub->unk_14);
            runs += len < 0 ? -len : len;
        } while (n);
    }
}

/* Each strip straight into the packet. */
/* 0x001bc070 */
static void ulGxDrawTexStripsUVNoClip(UlGxBig *gx, short *runs)
{
    unsigned long tag[2];
    unsigned long *pkt;
    unsigned long prim_bits;
    GxDev *dev;
    void *pkt_end;
    short header;
    int nruns;
    int n;
    int len;

    prim_bits = (((gx->prim & ~7UL) | 20) & 0x7ff) << 47;
    tag[1] = gx->tag_tex_regs;
    tag[0] = (gx->tag_tex & 0xfc007fffffffffffUL) | prim_bits;
    header = *runs;
    nruns = header & 0x3fff;
    runs++;
    if (nruns) {
        n = nruns;
        do {
            len = *runs;
            dev = psGsGetRefPkt();
            n--;
            runs++;
            pkt = psVifPktAddGifTag(dev, tag);
            pkt_end = vu0_gx_emit_uv_strip_cull(gx, runs, len, gx->vbuf, pkt + 2);
            if (len < 0)
                len = -len;
            ((GifWord *)pkt)->v = (pkt[0] & ~0x7fffUL) | (len & 0x7fff);
            runs = (short *)((char *)runs + len * 6);
            dev->cur = pkt_end;
            dev->nreg += len * 3;
            psVifPktCountGifQwords(dev);
            gx->ntri += len - 2;
        } while (n);
    }
}

/* One GIF packet per strip; counts triangles. */
/* 0x001bc1d8 */
static void ulGxDrawTexStripsNoClip(UlGxBig *gx, short *runs)
{
    unsigned long tag[2];
    unsigned long *pkt;
    void *dev;
    void *pkt_end;
    short *p;
    short header;
    int nruns;
    int n;
    int len;

    dev = psGsGetRefPkt();
    p = runs;
    tag[0] = (gx->tag_tex & 0xfc007fffffffffffUL) | ((((gx->prim & ~7UL) | 20) & 0x7ff) << 47);
    tag[1] = gx->tag_tex_regs;
    header = *p;
    nruns = header & 0x3fff;
    p++;
    if (nruns) {
        n = nruns;
        do {
            len = *p++;
            pkt = psVifPktAddGifTag(dev, tag);
            n--;
            pkt_end = vu0_gx_emit_tex_strip_cull(gx, p, len, gx->vbuf, pkt + 2);
            if (len < 0)
                len = -len;
            pkt[0] = (pkt[0] & ~0x7fffUL) | (len & 0x7fff);
            psVifPktAdvanceGif(dev, pkt_end, len * 3);
            p += len;
            gx->ntri += len - 2;
        } while (n);
    }
}

/* Untextured. */
/* 0x001bc330 */
static void ulGxDrawStripsNoClip(UlGxBig *gx, short *runs)
{
    unsigned long tag[2];
    unsigned long *pkt;
    void *dev;
    void *pkt_end;
    short header;
    int nruns;
    int n;
    int len;
    unsigned long prim_bits;

    prim_bits = (((gx->prim & ~7UL) | 4) & 0x7ff) << 47;
    tag[1] = gx->tag_tri_regs;
    tag[0] = (gx->tag_tri & 0xfc007fffffffffffUL) | prim_bits;
    header = *runs;
    nruns = header & 0x3fff;
    runs++;
    if (nruns) {
        n = nruns;
        do {
            len = *runs;
            dev = psGsGetRefPkt();
            n--;
            runs++;
            pkt = psVifPktAddGifTag(dev, tag);
            pkt_end = vu0_gx_emit_strip_cull(gx, runs, len, gx->vbuf, pkt + 2);
            if (len < 0)
                len = -len;
            pkt[0] = (pkt[0] & ~0x7fffUL) | (len & 0x7fff);
            runs += len;
            psVifPktAdvanceGif(dev, pkt_end, len * 2);
            gx->ntri += len - 2;
        } while (n);
    }
}

/* Runs and frees slot k. */
/* 0x001bc480 */
static void ulGxReleaseSlotClip(UlGxBig *gx, int k)
{
    ulGxExecDlistClip(gx, gx->slot[k]);
    gx->slot[k] = 0;
    gx->nslot--;
}

/* 0x001bc4d0 */
static void ulGxReleaseSlotNoClip(UlGxBig *gx, int k)
{
    ulGxExecDlistNoClip(gx, gx->slot[k]);
    gx->slot[k] = 0;
    gx->nslot--;
}

/* 32-entry slot table. */
/* 0x001bc520 */
static void ulGxSetSlot(UlGxBig *gx, void *obj, int k)
{
    int n = gx->nslot + 1;

    gx->slot[k] = obj;
    gx->nslot = n;
}

/*
 * Opcode in the low byte: 4 parks the rest in a slot and ends, 5 runs and frees a slot, 8 texture,
 * 16..63 colours/blend/flags, 64..66 strips (tex, untex, UV in 1/256 or 1/1024). 0xff ends.
 */
/* steer(cast) */
/* 0x001bc540 */
static void ulGxExecDlistClip(UlGxBig *gx, short *list)
{
    short cmd;
    int op;
    int arg;
    int len;
    void *dev;
    void *buf;

    cmd = *list++;
    while (cmd != 255) {
        op = cmd & 0xff;
        arg = cmd & 0xff00;
        if (op < 8) {
            switch (op) {
            case 3:
                break;
            case 4:
                ulGxSetSlot(gx, list, arg >> 8);
                return;
            case 5:
                ulGxReleaseSlotClip(gx, arg >> 8);
                break;
            }
        } else if (op < 16) {
            if (op == 8)
                ulGxDlSetTexture(gx, (short)arg, *list++);
        } else if (op < 64) {
            int n;
            n = *list++;
            ulGxDlReadAttr(gx, op, list);
            ulGxDlSetAlphaMode(gx, arg);
            list += n;
        } else {
            len = *list++;
            switch (op) {
            case 64:
                dev = psGsGetDisplayList();
                buf = psDisplayListGetVifPkt(dev);
                ulGxSetRenderFlags(gx, arg);
                psVifPktOpenGifPkt(buf);
                psDisplayListFlushGsRegs(dev, buf);
                if (cmd & 0x4000)
                    ((void (*)(UlGxBig *, short *, int))ulGxDrawTexStripsClip)(gx, list, arg);
                else
                    ((void (*)(UlGxBig *, short *, int))ulGxDrawStripsClip)(gx, list, arg);
                psVifPktCloseGifPkt(buf);
                break;
            case 65:
                dev = psGsGetDisplayList();
                buf = psDisplayListGetVifPkt(dev);
                ulGxSetRenderFlags(gx, arg);
                psVifPktOpenGifPkt(buf);
                psDisplayListFlushGsRegs(dev, buf);
                ((void (*)(UlGxBig *, short *, int, float))ulGxDrawTexStripsUVClip)(gx, list, arg, 0.00390625f);
                psVifPktCloseGifPkt(buf);
                break;
            case 66:
                dev = psGsGetDisplayList();
                buf = psDisplayListGetVifPkt(dev);
                ulGxSetRenderFlags(gx, arg);
                psVifPktOpenGifPkt(buf);
                psDisplayListFlushGsRegs(dev, buf);
                ((void (*)(UlGxBig *, short *, int, float))ulGxDrawTexStripsUVClip)(gx, list, arg, 0.0009765625f);
                psVifPktCloseGifPkt(buf);
                break;
            }
            list += len;
        }
        cmd = *list++;
    }
}

/*
 * With outcodes: positions, coloured, lit+envmap, coloured+envmap, or skinned runs.
 * Compact-shape lists are skipped with a message.
 */
/* 0x001bc7f0 */
static void ulGxExecVlistClip(UlGxBig *gx, unsigned int *list)
{
    unsigned int cmd;
    unsigned int hdr;
    int n;
    int cnt;
    int vtx;
    int op;

    while (*list != 255) {
        cmd = *list;
        op = cmd & 0xff;
        n = cmd >> 16;
        if (cmd & 0x4000) {
            printf("Vlist for Compact Shape\n");
            list += n + 1;
        } else {
            hdr = list[1];
            cnt = hdr >> 16;
            vtx = gx->vbuf + ((hdr & 0xffff) << 6);
            switch (op - 34) {
            case 0:
                vu0_gx_project_pos_clip(gx, list + 2, vtx, cnt);
                break;
            case 7:
                vu0_gx_project_lit_env_clip(gx, list + 2, vtx, cnt);
                break;
            case 10:
                ulGxVlistPacketClip(gx, list);
                break;
            case 1:
                vu0_gx_project_col_clip(gx, list + 2, vtx, cnt);
                break;
            case 8:
                vu0_gx_project_col_env_clip(gx, list + 2, vtx, cnt);
                break;
            }
            ulGxState.nvtx += cnt;
            list += n + 1;
        }
    }
}

/* 0x001bc948 */
static void ulGxExecDlistNoClip(UlGxBig *gx, short *list)
{
    short cmd;
    int op;
    int arg;
    int len;
    void *dev;
    void *buf;

    cmd = *list++;
    while (cmd != 255) {
        op = cmd & 0xff;
        arg = cmd & 0xff00;
        if (op < 8) {
            switch (op) {
            case 3:
                break;
            case 4:
                ulGxSetSlot(gx, list, arg >> 8);
                return;
            case 5:
                ulGxReleaseSlotNoClip(gx, arg >> 8);
                break;
            }
        } else if (op < 16) {
            if (op == 8)
                ulGxDlSetTexture(gx, (short)arg, *list++);
        } else if (op < 64) {
            int n;
            n = *list++;
            ulGxDlReadAttr(gx, op, list);
            ulGxDlSetAlphaMode(gx, arg);
            list += n;
        } else {
            len = *list++;
            switch (op) {
            case 64:
                dev = psGsGetDisplayList();
                buf = psDisplayListGetVifPkt(dev);
                ulGxSetRenderFlags(gx, arg);
                psVifPktOpenGifPkt(buf);
                psDisplayListFlushGsRegs(dev, buf);
                if (cmd & 0x4000)
                    ulGxDrawTexStripsNoClip(gx, list);
                else
                    ulGxDrawStripsNoClip(gx, list);
                psVifPktCloseGifPkt(buf);
                break;
            case 65:
                dev = psGsGetDisplayList();
                buf = psDisplayListGetVifPkt(dev);
                gx->uvscale = 0.00390625f;
                ulGxSetRenderFlags(gx, arg);
                psVifPktOpenGifPkt(buf);
                psDisplayListFlushGsRegs(dev, buf);
                ulGxDrawTexStripsUVNoClip(gx, list);
                psVifPktCloseGifPkt(buf);
                break;
            case 66:
                dev = psGsGetDisplayList();
                buf = psDisplayListGetVifPkt(dev);
                gx->uvscale = 0.0009765625f;
                ulGxSetRenderFlags(gx, arg);
                psVifPktOpenGifPkt(buf);
                psDisplayListFlushGsRegs(dev, buf);
                ulGxDrawTexStripsUVNoClip(gx, list);
                psVifPktCloseGifPkt(buf);
                break;
            }
            list += len;
        }
        cmd = *list++;
    }
}

/* Without outcodes. */
/* 0x001bcbf0 */
static void ulGxExecVlistNoClip(UlGxBig *gx, unsigned int *list)
{
    unsigned int cmd;
    unsigned int hdr;
    int n;
    int cnt;
    int vtx;
    int op;

    while (*list != 255) {
        cmd = *list;
        op = cmd & 0xff;
        n = cmd >> 16;
        if (cmd & 0x4000) {
            printf("Vlist for Compact Shape\n");
            list += n + 1;
        } else {
            hdr = list[1];
            cnt = hdr >> 16;
            vtx = gx->vbuf + ((hdr & 0xffff) << 6);
            switch (op - 34) {
            case 0:
                vu0_gx_project_pos(gx, list + 2, vtx, cnt);
                break;
            case 7:
                vu0_gx_project_lit_env(gx, list + 2, vtx, cnt);
                break;
            case 10:
                ulGxVlistPacketNoClip(gx, list);
                break;
            case 1:
                vu0_gx_project_col(gx, list + 2, vtx, cnt);
                break;
            case 8:
                vu0_gx_project_col_env(gx, list + 2, vtx, cnt);
                break;
            }
            ulGxState.nvtx += cnt;
            list += n + 1;
        }
    }
}

/* Bounding cube: -1 off screen or outside depth, 3 crossing the clip rectangle, 0 inside. */
/* 0x001bcd48 */
static int ulGxCullBox(UlGxBig *gx, void *box)
{
    V4 corners[8];
    V4 proj[8];
    V4 mn;
    V4 mx;
    V4 *corner;
    V4 *dst;
    int i;
    int bits;
    V4 *pn;
    V4 *px;

    ulGxMakeBoxCorners(box, corners);
    corner = corners;
    dst = proj;
    for (i = 0; i < 8; i++) {
        ulGxTransformDivide(corner, dst);
        dst++;
        corner++;
    }
    if (AZ(i))      /* never taken */
        return 0;
    mn.x = mn.y = mn.z = mn.w = 10000000.0f;
    mx.x = mx.y = mx.z = mx.w = -10000000.0f;
    pn = &mn;
    px = &mx;
    corner = proj;
    for (i = 0; i < 8; i++) {
        if (gx->znear < corner->w) {
            vu0_vec_min(pn, pn, corner);
            vu0_vec_max(px, px, corner);
        }
        corner++;
    }
    if (mx.w < gx->znear)
        return -1;
    if (mn.w > gx->zfar)
        return -1;
    if (mx.x < gx->scr_x0)
        return -1;
    if (mn.x > gx->scr_x1)
        return -1;
    if (mx.y < gx->scr_y0)
        return -1;
    if (mn.y > gx->scr_y1)
        return -1;
    bits = 0;
    corner = proj;
    for (i = 0; i < 8; i++) {
        bits |= ulGxBoxOutcode(corner, &gx->xmin, &gx->xmax);
        corner++;
    }
    return bits > 0 ? 3 : 0;
}

/* Vertex list, then display list; times go to the frame statistics. */
/* 0x001bcf00 */
static void ulGxDrawShapeClip(UlGxDrawable *shape)
{
    UlGxBig *gx = &ulGxState;
    int start;

    if (shape->vlist) {
        start = psTimerGetCount();
        ulGxExecVlistClip(gx, shape->vlist);
        gx->vlist_ticks += psTimerDiff(psTimerGetCount(), start);
    }
    if (shape->dlist) {
        start = psTimerGetCount();
        psGsOpenRefPkt();
        ulGxExecDlistClip(gx, shape->dlist);
        psGsCloseRefPkt();
        gx->dlist_ticks += psTimerDiff(psTimerGetCount(), start);
    }
}

/* 0x001bcfc8 */
static void ulGxDrawShapeNoClip(UlGxDrawable *shape)
{
    UlGxBig *gx = &ulGxState;
    int start;

    if (shape->vlist) {
        start = psTimerGetCount();
        ulGxExecVlistNoClip(gx, shape->vlist);
        gx->vlist_ticks += psTimerDiff(psTimerGetCount(), start);
    }
    if (shape->dlist) {
        start = psTimerGetCount();
        psGsOpenRefPkt();
        ulGxExecDlistNoClip(gx, shape->dlist);
        psGsCloseRefPkt();
        gx->dlist_ticks += psTimerDiff(psTimerGetCount(), start);
    }
}

/*
 * No clipping with ulGxState.drawflags bit 0. Without flags 0x2000 tests the cube, result unused.
 */
/* 0x001bd090 */
void ulGxDrawShape(UlGxDrawable *shape, int flags)
{
    UlGxBig *gx = &ulGxState;

    if (!(flags & 0x2000) && shape->radius > 0.0f)
        ulGxCullBox(gx, shape);
    switch ((gx->drawflags & 1) ^ 1) {
    case 0:
        ulGxDrawShapeNoClip(shape);
        break;
    case -1:
        break;
    case 1:
    default:
        ulGxDrawShapeClip(shape);
        break;
    }
}

/* Linear ramp z0..z1 -> fog0..fog1; uploads to VU1. */
/* 0x001bd140 */
void ulGxSetFogRange(float z0, float z1, float fog0, float fog1)
{
    UlGxBig *gx = &ulGxState;
    void *dev;
    void *buf;

    fog0 *= 255.0f;
    fog1 *= 255.0f;
    gx->fog_max = 255.0f;
    gx->fog_scale = (fog0 - fog1) / (z1 - z0);
    gx->fog_offset2 = gx->fog_offset = (z1 * fog1 - z0 * fog0) / (z1 - z0);
    dev = psGsGetDisplayList();
    buf = psDisplayListOpenRefPkt(dev);
    psVifPktAddWord(buf, 0x1000101);
    psVifPktAddWord(buf, 0x6c010056);
    psVifPktAddWords(buf, &gx->fog_scale, 4);
    psDisplayListCloseRefPkt(dev);
}

/* 0x001bd208 */
void ulGxEnableFog(int on)
{
    UlGxBig *gx = &ulGxState;

    if (on) {
        gx->prim |= 0x20;
        gx->prim3d |= 0x20;
        gx->prim2d |= 0x20;
    } else {
        gx->prim &= ~0x20;
        gx->prim3d &= ~0x20;
        gx->prim2d &= ~0x20;
    }
}

/* 0..1. */
/* 0x001bd270 */
void ulGxSetFogColor(float r, float g, float b)
{
    unsigned long col;

    col = (unsigned long)((int)(r * 255.0f) & 0xff);
    col |= (unsigned long)((int)(g * 255.0f) & 0xff) << 8;
    col |= (unsigned long)((int)(b * 255.0f) & 0xff) << 16;
    psGsWriteReg(61, col);
}

/* Fixed screen offset instead of the display origin (modes 4..6); uploads to VU1. */
/* 0x001bd2c0 */
static void ulGxInitScreenScale(float scale_x, float scale_y)
{
    UlGxBig *gx = &ulGxState;
    void *dev;
    void *buf;
    float oy;
    float k;

    gx->scale_w = 0.0f;
    oy = 2048.0f - scale_y * 480.0f * 0.5f;
    k = 1728.0f;
    gx->offset_w = gx->scale_w;
    gx->scale_x = scale_x;
    gx->scale_y = scale_y;
    gx->scale_z = 1048576.0f;
    gx->offset_x = k;
    gx->offset_y = oy;
    gx->offset_z = gx->scale_w;
    dev = psGsGetDisplayList();
    buf = psDisplayListOpenRefPkt(dev);
    psVifPktAddWord(buf, 0x6c010047);
    psVifPktAddWords(buf, &gx->scale_x, 4);
    psVifPktAddWord(buf, 0x6c010048);
    psVifPktAddWords(buf, &gx->offset_x, 4);
    psDisplayListCloseRefPkt(dev);
}

/* With the display origin; uploads to VU1. */
/* 0x001bd3a8 */
static void ulGxSetScreenScale(float x, float y)
{
    UlGxBig *gx = &ulGxState;
    float origin[2];
    void *dev;
    void *buf;

    gx->cam.scale_x = x;
    gx->scale_x = x;
    gx->cam.scale_y = y;
    gx->scale_y = y;
    gx->scale_z = 1048576.0f;
    gx->scale_w = 0.0f;
    psGsGetScreenOrigin(&origin[0], &origin[1]);
    gx->offset_x = origin[0];
    gx->offset_y = origin[1];
    gx->offset_z = gx->offset_w = 0.0f;
    dev = psGsGetDisplayList();
    buf = psDisplayListOpenRefPkt(dev);
    psVifPktAddWord(buf, 0x6c010047);
    psVifPktAddWords(buf, &gx->scale_x, 4);
    psVifPktAddWord(buf, 0x6c010048);
    psVifPktAddWords(buf, &gx->offset_x, 4);
    psDisplayListCloseRefPkt(dev);
}

/* 0x001bd480 */
int ulGxGetDrawFlags(void)
{
    return ulGxDrawFlagWord;
}

/* Bit 0: no clipping. Returns the previous flags. */
/* 0x001bd490 */
static int ulGxSetDrawFlags(int flags)
{
    int old = ulGxState.drawflags;

    ulGxState.drawflags = flags;
    return old;
}

/* To VU1; a null pointer leaves that matrix. */
/* 0x001bd4a8 */
static void ulGxUploadLightMatrices(void *dir_mtx, void *col_mtx)
{
    void *dev;
    void *buf;

    dev = psGsGetDisplayList();
    buf = psDisplayListOpenRefPkt(dev);
    psVifPktAddWord(buf, 0x1000101);
    if (dir_mtx) {
        psVifPktAddWord(buf, 0x6c040018);
        psVifPktAddWords(buf, dir_mtx, 16);
    }
    if (col_mtx) {
        psVifPktAddWord(buf, 0x6c04001c);
        psVifPktAddWords(buf, col_mtx, 16);
    }
    psDisplayListCloseRefPkt(dev);
}

/* 0x001bd558 */
static void ulGxSetScreenSize(float width, float height)
{
    ulGxProj.width = width;
    ulGxProj.height = height;
}

/* Relative to the middle of the drawing area. */
/* 0x001bd570 */
static void ulGxSetScreenCenter(float x, float y)
{
    ulGxProj.cx = x + 2048.0f;
    ulGxProj.cy = y + 2048.0f;
}

/* sel 0 world, 1 view, 2 projection, 3 frustum; nothing for a null mtx. */
/* steer(cast) */
/* 0x001bd598 */
static int ulGxLoadMatrix(int sel, void *mtx)
{
    UlGxBig *gx = &ulGxState;

    if (mtx) {
        switch (sel) {
        case 0:
            return ((int (*)(void *, void *))vu0_matrix_copy)(mtx, gx);
        case 1:
            return ((int (*)(void *, void *))vu0_matrix_copy)(mtx, (char *)gx + 64);
        case 2:
            return ((int (*)(void *, void *))vu0_matrix_copy)(mtx, (char *)gx + 128);
        case 3:
            return ((int (*)(void *, void *))vu0_matrix_copy)(mtx, (char *)gx + 704);
        default:
            __assert("ul_gx.c", 348, "0");
        }
    }
}

/* sel as ulGxLoadMatrix, null = identity; rebuilds dependent matrices and uploads them to VU1. */
/* 0x001bd658 */
void ulGxSetMatrix(int sel, void *mtx)
{
    UlGxBig *gx = &ulGxState;
    char *base = (char *)gx;
    float rot[16];
    void *dev;
    void *buf;

    if (mtx == 0)
        mtx = ulGxIdentityMatrix;
    switch (sel) {
    case 0:
        {
            char *modelview;
            char *mvp;
            char *light;
            char *clip;

            vu0_matrix_copy(base, mtx);
            modelview = base + 192;
            vu0_matrix_mul(modelview, base + 64, mtx);
            mvp = base + 320;
            vu0_matrix_mul(mvp, base + 128, modelview);
            light = base + 384;
            clip = base + 768;
            ulMatrixCopyRotation(rot, mtx);
            vu0_matrix_normalize3(rot, rot);
            vu0_matrix_mul(light, base + 1760, rot);
            vu0_matrix_mul(clip, base + 704, modelview);
            dev = psGsGetDisplayList();
            buf = psDisplayListOpenRefPkt(dev);
            psVifPktAddWord(buf, 0x1000101);
            psVifPktAddUnpackMatrix(buf, 0x6c04000c, modelview);
            psVifPktAddUnpackMatrix(buf, 0x6c040014, mvp);
            psVifPktAddUnpackMatrix(buf, 0x6c040018, light);
            psVifPktAddUnpackMatrix(buf, 0x6c040030, clip);
            psDisplayListCloseRefPkt(dev);
        }
        break;
    case 1:
        {
            char *view;
            char *view_proj;

            view = base + 64;
            vu0_matrix_copy(view, mtx);
            view_proj = base + 256;
            vu0_matrix_mul(view_proj, base + 128, mtx);
            dev = psGsGetDisplayList();
            buf = psDisplayListOpenRefPkt(dev);
            psVifPktAddWord(buf, 0x1000101);
            psVifPktAddWord(buf, 0x6c040004);
            psVifPktAddWords(buf, view, 16);
            psVifPktAddWord(buf, 0x6c040010);
            psVifPktAddWords(buf, view_proj, 16);
            psDisplayListCloseRefPkt(dev);
        }
        break;
    case 2:
        {
            char *proj;

            proj = base + 128;
            vu0_matrix_copy(proj, mtx);
            dev = psGsGetDisplayList();
            buf = psDisplayListOpenRefPkt(dev);
            psVifPktAddWord(buf, 0x1000101);
            psVifPktAddWord(buf, 0x6c040008);
            psVifPktAddWords(buf, proj, 16);
            psDisplayListCloseRefPkt(dev);
        }
        break;
    case 3:
        {
            char *frustum;

            frustum = base + 704;
            vu0_matrix_copy(frustum, mtx);
            dev = psGsGetDisplayList();
            buf = psDisplayListOpenRefPkt(dev);
            psVifPktAddWord(buf, 0x1000101);
            psVifPktAddWord(buf, 0x6c04002c);
            psVifPktAddWords(buf, frustum, 16);
            psDisplayListCloseRefPkt(dev);
        }
        break;
    }
}

/* Maps lo..hi to -1..1, four components. */
/* 0x001bd938 */
static void ulGxCalcScaleOffset(float *lo, float *hi, float *scale, float *offset)
{
    int i;

    for (i = 0; i < 4; i++) {
        scale[i] = 2.0f / (hi[i] - lo[i]);
        offset[i] = -(lo[i] + hi[i]) / (hi[i] - lo[i]);
    }
}

/* Through the non-null pointers. */
/* 0x001bd9a8 */
static void ulGxGetDepthRange(float *znear, float *zfar)
{
    UlGxBig *gx = &ulGxState;

    if (znear)
        *znear = gx->cam.znear;
    if (zfar)
        *zfar = gx->cam.zfar;
}

/*
 * Stored negative (camera looks down -z); derives clipper and VU1 depth constants, rebuilds
 * frustum.
 */
/* 0x001bd9d8 */
void ulGxSetDepthRange(float znear, float zfar)
{
    UlGxBig *gx = &ulGxState;
    float anear;
    float afar;
    float rnear;
    float rfar;
    void *dev;
    void *buf;

    if (znear > 0.0f)
        znear = -znear;
    if (zfar > 0.0f)
        zfar = -zfar;
    anear = fabsf(znear);
    afar = fabsf(zfar);
    rnear = 1.0f / anear;
    rfar = 1.0f / afar;
    gx->cam.zfar = zfar;
    gx->znear = anear;
    gx->cam.rfar = -1.0f / zfar;
    gx->zfar = afar;
    gx->cam.rnear = -1.0f / znear;
    gx->cam.znear = znear;
    gx->dnear = rnear * 1048576.0f;
    gx->dfar = rfar * 1048576.0f;
    ulGxCalcScaleOffset(&gx->xmin, &gx->xmax, &gx->nscale_x, &gx->noffset_x);
    gx->neg_znear = -anear;
    gx->inv_znear = rnear;
    dev = psGsGetDisplayList();
    buf = psDisplayListOpenRefPkt(dev);
    psVifPktAddWord(buf, 0x1000101);
    psVifPktAddWord(buf, 0x6c010038);
    psVifPktAddWords(buf, &gx->xmin, 4);
    psVifPktAddWord(buf, 0x6c010039);
    psVifPktAddWords(buf, &gx->xmax, 4);
    psVifPktAddWord(buf, 0x6c010049);
    psVifPktAddWords(buf, &gx->nscale_x, 4);
    psVifPktAddWord(buf, 0x6c01004a);
    psVifPktAddWords(buf, &gx->noffset_x, 4);
    psVifPktAddWord(buf, 0x6c01003a);
    psVifPktAddWords(buf, &gx->scr_x0, 4);
    psDisplayListCloseRefPkt(dev);
    gx->frustum.znear = anear;
    gx->frustum.zfar = afar;
    ulGxSetFrustum(&gx->frustum);
}

/* Through the non-null pointers. */
/* 0x001bdbe8 */
void ulGxGetPixelScale(float *sx, float *sy)
{
    UlGxCam *cam = &ulGxProj;

    if (sx)
        *sx = cam->pix_sx;
    if (sy)
        *sy = cam->pix_sy;
}

/* 0x001bdc18 */
static void ulGxSetPixelScale(float sx, float sy)
{
    ulGxProj.pix_sx = sx;
    ulGxProj.pix_sy = sy;
}

/* Multiplied by the pixel aspect; rebuilds the frustum. */
/* 0x001bdc30 */
void ulGxSetAspect(float x, float y)
{
    UlGxCam *cam = &ulGxProj;
    Frustum *frustum = (Frustum *)((char *)cam + 712);

    x *= cam->pix_sx;
    y *= cam->pix_sy;
    frustum->yscale = y;
    cam->xscale = x;
    frustum->xscale = x;
    cam->yscale = y;
    ulGxSetFrustum(frustum);
}

/* Rebuilds frustum and projection; returns the previous distance. */
/* 0x001bdc68 */
static float ulGxSetProjDistance(float dist)
{
    UlGxProj *frustum = &ulGxFrustumState;
    float proj_mtx[16];
    float frustum_mtx[16];
    float old = frustum->dist;

    frustum->dist = dist;
    ulGxMakeFrustumMatrix(frustum_mtx, dist, frustum->znear, frustum->zfar, frustum->ext_x, frustum->ext_y);
    ulGxSetMatrix(3, frustum_mtx);
    ulGxMakeProjMatrix(proj_mtx, frustum->dist, frustum->xscale, frustum->yscale, frustum->cx, frustum->cy);
    ulGxSetMatrix(2, proj_mtx);
    return old;
}

/* Distance at which extent spans fov (radians). */
/* 0x001bdd08 */
static float ulGxFovToDistance(float fov, float extent)
{
    return extent * 0.5f / tanf(fov * 0.5f);
}

/* Radians; rebuilds the frustum. */
/* 0x001bdd50 */
void ulGxSetHFov(float fov)
{
    UlGxCam *cam = &ulGxProj;
    Frustum *frustum = (Frustum *)((char *)cam + 712);
    float dist;

    dist = cam->width * 0.5f / tanf(fov * 0.5f);
    frustum->dist = dist;
    cam->dist = dist;
    ulGxSetFrustum(frustum);
}

/* Radians; rebuilds the projection matrix. */
/* 0x001bddc0 */
static void ulGxSetFov(float fov)
{
    UlGxCam *cam = &ulGxProj;
    float proj_mtx[16];

    cam->fov = fov;
    cam->dist = cam->height * 0.5f / tanf(fov * 0.5f);
    ulGxMakeProjMatrix(proj_mtx, cam->dist, cam->xscale, cam->yscale, cam->cx, cam->cy);
    ulGxSetMatrix(2, proj_mtx);
}

/* Perspective with the screen mapping folded in: scale sx, sy, centre tx, ty, depth znear..zfar. */
/* 0x001bde48 */
static void ulGxMakeScreenMatrix(float *mtx, float dist, float sx, float sy, float tx, float ty, float znear, float zfar,
                   float depth0, float depth1)
{
    float viewport_mtx[16];
    float depth_scale;
    float depth_ofs;

    depth_ofs = (-depth0 * znear + depth0 * zfar) / (-znear + zfar);
    depth_scale = (zfar * znear * (-depth0 + depth1)) / (-znear + zfar);
    vu0_matrix_identity(mtx);
    do { } while (0);
    mtx[0] = dist;
    do {
        do {
            mtx[5] = dist;
            mtx[10] = 0;
            mtx[15] = 0;
        } while (0);
        mtx[14] = 1.0f;
    } while (0);
    mtx[11] = -1.0f;
    do {
        vu0_matrix_identity(viewport_mtx);
        do {
            viewport_mtx[0] = sx;
        } while (0);
        viewport_mtx[5] = -sy;
    } while (0);
    do { } while (0);
    viewport_mtx[10] = depth_scale;
    viewport_mtx[12] = tx;
    do {
        do {
            viewport_mtx[13] = ty;
            viewport_mtx[14] = depth_ofs;
        } while (0);
    } while (0);
    vu0_matrix_mul(mtx, viewport_mtx, mtx);
}

/* View -> screen; depth 2^20 / z. Records the parameters in ulGxProj. */
/* 0x001bdf58 */
void ulGxMakeProjMatrix(float *mtx, float dist, float xscale, float yscale, float cx, float cy)
{
    UlGxCam *cam = &ulGxProj;
    float k;

    cam->dist = dist;
    cam->xscale = xscale;
    cam->yscale = yscale;
    k = 1048576.0f;
    cam->cx = cx;
    cam->cy = cy;
    vu0_matrix_zero(mtx);
    do {
        mtx[0] = xscale * dist;
    } while (0);
    do {
        mtx[5] = -yscale * dist;
    } while (0);
    do {
        mtx[8] = -cx;
    } while (0);
    do {
        mtx[9] = -cy;
    } while (0);
    mtx[14] = k;
    do {
        mtx[11] = -1.0f;
    } while (0);
}

/* For the clipper. */
/* 0x001be018 */
static void ulGxMakeFrustumMatrix(float *mtx, float dist, float znear, float zfar, float ext_x, float ext_y)
{
    float x = znear * ext_y / dist;
    float y = znear * ext_x / dist;

    vu0_matrix_zero(mtx);
    do {
        mtx[0] = (znear + znear) / (y + y);
    } while (0);
    do {
        mtx[5] = -(znear + znear) / (x + x);
    } while (0);
    do {
        mtx[10] = -((zfar + znear) / (zfar - znear));
    } while (0);
    do {
        mtx[14] = zfar * -2.0f * znear / (zfar - znear);
    } while (0);
    do {
        mtx[11] = -1.0f;
    } while (0);
}

/* 0x001be100 */
static void ulGxMakeProjMatrices(float *screen_mtx, float *frustum_mtx, float *viewport_mtx, float dist, float xscale, float yscale, float cx,
                   float cy, float depth0, float depth1, float znear, float zfar, float ext_x, float ext_y)
{
    float x = znear * ext_x / dist;
    float y = znear * ext_y / dist;

    ulGxMakeScreenMatrix(screen_mtx, dist, xscale, yscale, cx, cy, znear, zfar, depth0, depth1);
    vu0_matrix_zero(frustum_mtx);
    do {
        frustum_mtx[0] = (znear + znear) / (x + x);
    } while (0);
    do {
        frustum_mtx[5] = -(znear + znear) / (y + y);
    } while (0);
    do {
        frustum_mtx[10] = -((zfar + znear) / (zfar - znear));
    } while (0);
    do {
        frustum_mtx[14] = zfar * -2.0f * znear / (zfar - znear);
    } while (0);
    do {
        frustum_mtx[11] = -1.0f;
    } while (0);
    vu0_matrix_zero(viewport_mtx);
    do {
        viewport_mtx[0] = dist * xscale * x / znear;
    } while (0);
    do {
        viewport_mtx[5] = dist * yscale * y / znear;
    } while (0);
    do {
        viewport_mtx[10] = (-depth1 + depth0) * 0.5f;
    } while (0);
    do {
        viewport_mtx[14] = (depth1 + depth0) * 0.5f;
    } while (0);
    do {
        viewport_mtx[12] = cx;
    } while (0);
    do {
        viewport_mtx[13] = cy;
    } while (0);
    do {
        viewport_mtx[15] = 1.0f;
    } while (0);
}

/* 0x001be2d8 */
void ulGxGetFrustum(UlGxFrustum *dst)
{
    *dst = ulGxState.frustum;
}

/* 0x001be340 */
void ulGxSetFrustum(UlGxFrustum *fr)
{
    float proj_mtx[16];
    float frustum_mtx[16];

    ulGxState.frustum = *fr;
    ulGxMakeFrustumMatrix(frustum_mtx, fr->dist, fr->znear, fr->zfar, fr->ext_x, fr->ext_y);
    ulGxSetMatrix(3, frustum_mtx);
    ulGxMakeProjMatrix(proj_mtx, fr->dist, fr->xscale, fr->yscale, fr->cx, fr->cy);
    ulGxSetMatrix(2, proj_mtx);
}

/* 0x00BBGGRR. */
/* 0x001be420 */
void ulGxSetClearColor(unsigned int col)
{
    psGsSetClearColor(col & 0xff, (col >> 8) & 0xff, (col >> 16) & 0xff);
}

/* Modes 0..7 each. */
/* 0x001be438 */
void ulGxSetAlphaBlend(int src_mode, int dst_mode)
{
    psGsWriteReg(66, ulGxAlphaTable[((src_mode & 7) << 3) | (dst_mode & 7)]);
}

/* -log2(x) in 1/16 steps. Nothing calls it. */
/* 0x001be468 */
static void ulGxSetTextureLod(float x)
{
    UlGxBig *gx = &ulGxState;
    int lod;

    lod = (int)(-(logf(x) / 0.693147182f) * 16.0f) & 0xfff;
    gx->tex1 = (gx->tex1 & 0xfffff000ffe7ffffUL) | ((unsigned long)lod << 32);
    psGsSetTexLod(0, 0, lod);
}

/* 0 nearest, 1 bilinear, 2 trilinear. */
/* 0x001be4f0 */
void ulGxSetTexFilter(int mode)
{
    switch (mode) {
    case 0:
        psGsSetTexFilter(0, 0);
        break;
    case 1:
        psGsSetTexFilter(1, 1);
        break;
    case 2:
        psGsSetTexFilter(1, 5);
        break;
    }
}

/* 0x001be540 */
void ulGxSetTexFunc(int tfx)
{
    psGsSetTexFunc(tfx, 1);
}

/* 0x001be548 */
void ulGxSetTexClamp(int wms, int wmt)
{
    psGsSetClamp(wms, wmt, 0, 0, 0, 0);
}

/* Only if idx is in range. */
/* 0x001be560 */
void ulGxSelectTexture(UlGxTexList *list, int idx)
{
    if (idx < list->count)
        ulGxSetTexture(list->ent[idx].rec);
}

/* 0x001be590 */
int ulGxGetTextureList(void)
{
    return ulGxCurTexList;
}

/* Returns the previous one. */
/* 0x001be5a0 */
UlGxTexList *ulGxSetTextureList(UlGxTexList *list)
{
    UlGxTexList *old = ulGxState.texlist;

    ulGxState.texlist = list;
    ulGxState.texidx = -1;
    return old;
}

/* Asserts on a null surface. */
/* 0x001be5c0 */
void ulGxSetTexture(void *tex)
{
    if (tex) {
        ulGxCurTexture = tex;
        psSurfBind(tex);
        return;
    }
    printf("ulGxSetTexture: texture surface is NULL!\n");
    psAssertFail("ul_gx.c", 1237, "e");
}

/* Constant part of ulGxState to VU1 memory. */
/* steer(cast) */
/* 0x001be610 */
static void ulGxUploadGlobals(void)
{
    void *dev;
    void *buf;

    dev = psGsGetDisplayList();
    buf = psDisplayListOpenRefPkt(dev);
    psVifPktAddWord(buf, 0x1000101);
    psVifPktAddWord(buf, 0x6c5f0000);
    psVifPktAddWords(buf, ((int *)&ulGxState), 380);
    psDisplayListCloseRefPkt(dev);
}

/*
 * Kind 0 or 1: following GS packets go to the kind's lock block. Nests; outermost starts the block.
 */
/* 0x001be680 */
void ulGxLock(int kind)
{
    switch (kind) {
    case 0:
        if (ulGxLockCount0 == 0) {
            ulGxLockedPkt = psGxPktCur;
            psGxPktBegin(ulGxLockBlock0);
        }
        ulGxLockCount0++;
        break;
    case 1:
        if (ulGxLockCount1 == 0) {
            ulGxLockedPkt = psGxPktCur;
            psGxPktBegin(ulGxLockBlock1);
        }
        ulGxLockCount1++;
        break;
    }
}

/* Outermost sends the block and restores the previous packet. */
/* 0x001be708 */
void ulGxUnlock(int kind)
{
    switch (kind) {
    case 0:
        if (--ulGxLockCount0 == 0) {
            psGxPktSend(ulGxLockBlock0);
            psGxPktCur = ulGxLockedPkt;
        }
        break;
    case 1:
        if (--ulGxLockCount1 == 0) {
            psGxPktSend(ulGxLockBlock1);
            psGxPktCur = ulGxLockedPkt;
        }
        break;
    }
}

/* ulGxLock(0) without switching the current packet. */
/* 0x001be778 */
static void ulGxLockDefault(void)
{
    if (ulGxLockCount0 == 0)
        psGxPktBegin(ulGxLockBlock0);
    ulGxLockCount0++;
}

/* ulGxUnlock(0) without restoring the current packet. */
/* 0x001be7b0 */
static void ulGxUnlockDefault(void)
{
    if (--ulGxLockCount0 == 0)
        psGxPktSend(ulGxLockBlock0);
}

/* steer(cast) */
/* 0x001be7d8 */
static int ulGxWaitVSync(void)
{
    return ((int (*)(void))psGsVSyncField)();
}

/* 0x001be7f8 */
void ulGxResetCounters(void)
{
    ulGxClearCounters();
}

/* 0x001be800 */
void ulGxSyncPath(void)
{
    psGsSyncPath();
}

/* Warns unless 64-byte aligned (the test only checks 32). */
/* 0x001be808 */
static void ulGxSetVertexBuffer(void *buf, int n)
{
    UlGxBig *gx = &ulGxState;

    if ((unsigned int)buf & 0x1f)
        printf("ulGxSetVertexBuffer: buffer pointer is not 64 byte alignment!\n");
    gx->vbuf = (int)buf;
    gx->vbuf_size = n;
}

/* 0x001be860 */
void ulGxSetVertexBuffer_thunk(void * buf, int size)
{
    ulGxSetVertexBuffer(buf, size);
}

/*
 * Two packet areas, GS work area, texture table (record + free-list pointer each), each
 * 256-aligned.
 */
/* 0x001be868 */
unsigned int ulGxGetWorkSize(GxDesc *desc)
{
    unsigned int size;

    size = (desc->size1 + 255) & 0xffffff00;
    size += (desc->size2 + 255) & 0xffffff00;
    size += ((desc->ntex << 5) + 319) & 0xffffff00;
    size += (desc->ntex * 180 + 255) & 0xffffff00;
    return size;
}

/* 0x001be8b8 */
static void ulGxResetTextures(void)
{
    ulTextureClearTable();
    psGsStub();
}

/* First set-up packet: VU1 microprogram, constant ulGxState, VU1 GIF tag templates. */
/* steer(cast) */
/* 0x001be8d8 */
static void ulGxInitHardware(void)
{
    unsigned long tex_tag[2];
    unsigned long prim_tag[2];
    void *dev;
    void *buf;

    dev = psGsGetMainDisplayList();
    psDisplayListOpen(dev);
    psGsSetDisplayList(dev);
    ((void (*)(void *, int))psGsAddCallTag)(ulGxVu1Program, 0);
    buf = psDisplayListOpenRefPkt(dev);
    psVifPktAddWord(buf, 0x1000101);
    psVifPktAddWord(buf, 0x6c600000);
    psVifPktAddWords(buf, ((char *)&ulGxState), 384);
    {
        unsigned long prim_tag_word = 0x1000000000000001UL;
        unsigned long tex_tag_word = 0x3000000000008000UL;
        prim_tag[0] = prim_tag_word;
        tex_tag[0] = tex_tag_word;
        prim_tag[1] = 0;
        tex_tag[1] = 1042;
    }
    psVifPktAddUnpack(buf, 0x6c01004b, prim_tag, 1);
    psVifPktAddUnpack(buf, 0x6c01004c, tex_tag, 1);
    psDisplayListCloseRefPkt(dev);
    psDisplayListClose(dev);
    psDisplayListFlush(dev, 1);
    sceGsSyncPath(0, 0);
}

/* Projection distance 500, centre 2048, 2048. */
/* 0x001be9e8 */
static void ulGxInitFrustum(UlGxFrustum *fr)
{
    fr->dist = 500.0f;
    fr->znear = fr->xscale = 1.0f;
    fr->zfar = 10000.0f;
    fr->cx = fr->cy = 2048.0f;
    fr->unk_1c = 0.0f;
    fr->depth_max = 1048576.0f;
    fr->ext_y = fr->ext_x = 2040.0f;
}

/*
 * Display mode 0..6 (NTSC/PAL, full/half height) sets frame size and interlace; asserts otherwise.
 * Carves packets and texture table from desc->base, starts the GS, sets camera, depth, fov, fog.
 */
/* steer(cast) */
/* 0x001bea40 */
void ulGxInit(GxInitDesc *desc)
{
    UlGxBig *gx = &ulGxState;
    UlGxFrustum *fr = &gx->frustum;
    int cfg[7];
    unsigned int tex_tbl;
    unsigned int buf_b;
    unsigned int gs_work;
    void *dev;
    void **dp = ulGxInitTable;

    cfg[0] = 1;
    cfg[1] = desc->tv_mode;
    cfg[2] = desc->field_mode;
    switch (desc->mode) {
    case 0:
        cfg[3] = 640;
        cfg[4] = 224;
        cfg[2] = 1;
        break;
    case 1:
        cfg[3] = 640;
        cfg[4] = 448;
        cfg[2] = 0;
        break;
    case 2:
        cfg[3] = 640;
        cfg[4] = 240;
        cfg[2] = 1;
        break;
    case 3:
        cfg[3] = 320;
        cfg[4] = 240;
        cfg[2] = 1;
        break;
    case 4:
        cfg[3] = 640;
        cfg[4] = 224;
        cfg[2] = 1;
        break;
    case 5:
    case 6:
        cfg[3] = 640;
        cfg[4] = 256;
        cfg[2] = 1;
        break;
    default:
        printf("ulGxInit: Illegal display mode! [%d]\n", desc->mode);
        __assert("ul_gx.c", UL_GX_ASSERT_LINE, "0");
    }
    buf_b = desc->base + ((desc->size1 + 255) & 0xffffff00);
    gs_work = buf_b + ((desc->size2 + 255) & 0xffffff00);
    tex_tbl = gs_work + (((desc->ntex << 5) + 319) & 0xffffff00);
    cfg[5] = gs_work;
    cfg[6] = (desc->ntex << 5) + 64;
    *dp = psGsInit(cfg);
    ulTextureInitTable((void *)tex_tbl, desc->ntex);
    ((void (*)(int))ulGxResetGlobals)(0);
    gx->noffset_z = 0.0f;
    gx->nscale_x = 0.00048828125f;
    gx->nscale_y = 0.00048828125f;
    gx->nscale_z = 0.0f;
    gx->noffset_x = -2048.0f;
    gx->noffset_y = -2048.0f;
    ulGxInitFrustum(fr);
    if (desc->size2)
        psGxPktInitChannel(dp + 22, buf_b, desc->size2, 2);
    dev = psGsGetMainDisplayList();
    psDisplayListOpen(dev);
    psGsSetDisplayList(dev);
    switch (desc->mode) {
    case 0:
        ulGxSetScreenSize(640.0f, 448.0f);
        ulGxSetPixelScale(1.0f, 0.5f);
        ulGxSetScreenScale(1.0f, 0.5f);
        fr->xscale = 1.0f;
        fr->yscale = 0.5f;
        break;
    case 1:
        ulGxSetScreenSize(640.0f, 448.0f);
        ulGxSetPixelScale(1.0f, 1.0f);
        ulGxSetScreenScale(1.0f, 1.0f);
        fr->xscale = 1.0f;
        fr->yscale = 1.0f;
        break;
    case 2:
        ulGxSetScreenSize(640.0f, 480.0f);
        ulGxSetPixelScale(1.0f, 0.5f);
        ulGxSetScreenScale(1.0f, 0.5f);
        fr->xscale = 1.0f;
        fr->yscale = 0.5f;
        break;
    case 3:
        ulGxSetScreenSize(320.0f, 240.0f);
        ulGxSetPixelScale(1.0f, 1.0f);
        ulGxSetScreenScale(0.5f, 0.5f);
        fr->xscale = 1.0f;
        fr->yscale = 1.0f;
        break;
    case 4:
        ulGxSetScreenSize(640.0f, 448.0f);
        ulGxSetPixelScale(1.0f, 0.5f);
        ulGxInitScreenScale(1.0f, 0.5f);
        fr->xscale = 1.0f;
        fr->yscale = 0.5f;
        break;
    case 5:
        ulGxSetScreenSize(640.0f, 512.0f);
        ulGxSetPixelScale(1.0f, 0.5f);
        ulGxInitScreenScale(1.0f, 0.5f);
        fr->xscale = 1.0f;
        fr->yscale = 0.5f;
        break;
    case 6:
        ulGxSetScreenSize(640.0f, 512.0f);
        ulGxSetPixelScale(1.0f, 0.533333361f);
        ulGxInitScreenScale(1.0f, 0.533333361f);
        fr->xscale = 1.0f;
        fr->yscale = 0.533333361f;
        break;
    }
    ulGxSetAspect(1.0f, 1.0f);
    ulGxSetScreenCenter(0.0f, 0.0f);
    ulGxSetDepthRange(1.0f, 1000.0f);
    ulGxSetFov(1.04719758f);
    ulGxSetFogColor(1.0f, 1.0f, 1.0f);
    ulGxSetFogRange(100.0f, 10000.0f, 0.0f, 255.0f);
    ulGxSetFrustum(fr);
    psDisplayListClose(dev);
    psDisplayListFlush(dev, 1);
    sceGsSyncPath(0, 0);
    ulGxInitHardware();
}
