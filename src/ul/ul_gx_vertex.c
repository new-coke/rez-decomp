/*
 * Model vertex stage: VU0 transform, lighting and outcodes, vertex-list packet dispatch, indexed
 * strips as GIF packets; triangles crossing the screen edge go to the clipper.
 */

#include "types_ul.h"
#include "qword.h"
#include "sdk_compat.h"
#include "ul_gx_vertex.h"
#include "ps_gs.h"
#include "ul_gx.h"
#include "ul_vu0.h"

static int ulGxVertexOutcode(UlGxClip *gx, UlGxVtx *vtx);

typedef struct UlGxV {
    char pad_00[1612];
    char *vtx;              /* 1612 */
} UlGxV;

extern void vu0_matrix_apply(void *, void *, void *);
extern UlCam ulGxProj;
extern float ulGxViewMatrix[12];
extern char ulGxState[];
extern int vu0_gx_skin_lit_first(UlGxCtx *, int *, char *, int);
extern int vu0_gx_skin_lit_mid(UlGxCtx *, int *, char *, int);
extern int vu0_gx_skin_lit_env_last_clip(UlGxCtx *, int *, char *, int);
extern int vu0_gx_skin_lit_env_last(UlGxCtx *, int *, char *, int);
extern int vu0_gx_project_lit_env_clip(UlGxCtx *, int *, char *, int);
extern int vu0_gx_project_lit_env(UlGxCtx *, int *, char *, int);
extern void vu0_gx_load_strip_consts(UlGxStrip *);
extern int vu0_gx_emit_strip_cull_clip(UlGxStrip *, short *, int, int, void *);
extern void vu0_clip_load_consts(UlGxStrip *);
extern void vu0_gx_clip_copy(UlGxStrip *, short *, int, int, void *);
extern int vu0_gx_emit_strip_clip(UlGxStrip *, short *, int, int, void *);
extern int vu0_gx_emit_uv_strip_cull_clip(UlGxStrip *, short *, int, int, void *);
extern void vu0_gx_clip_copy_uv(UlGxStrip *, short *, int, int, void *);
extern int vu0_gx_emit_uv_strip_clip(UlGxStrip *, short *, int, int, void *);
extern int vu0_gx_emit_tex_strip_cull_clip(UlGxStrip *, short *, int, int, void *);
extern void vu0_gx_clip_copy_b(UlGxStrip *, short *, int, int, void *);
extern int vu0_gx_emit_tex_strip_clip(UlGxStrip *, short *, int, int, void *);

/* Cube of half-size box[5] around box[2], box[3]. Bug: z reuses the x centre. */
/* 0x001c2400 */
void ulGxMakeBoxCorners(float *box, float *out)
{
    float half;
    float x, y;
    float xh;
    float yl;
    float xl;
    float yh;

    x = box[2];
    y = box[3];
    half = box[5];
    yh = y + half;
    yl = y - half;
    xl = x - half;
    xh = x + half;

    out[0] = xl; out[1] = yh; out[2] = xh; out[3] = 1.0f;
    out[4] = xl; out[5] = yl; out[6] = xh; out[7] = 1.0f;
    out[8] = xh; out[9] = yh; out[10] = xh; out[11] = 1.0f;
    out[12] = xh; out[13] = yl; out[14] = xh; out[15] = 1.0f;
    out[16] = xl; out[17] = yh; out[18] = xl; out[19] = 1.0f;
    out[20] = xl; out[21] = yl; out[22] = xl; out[23] = 1.0f;
    out[24] = xh; out[25] = yh; out[26] = xl; out[27] = 1.0f;
    out[28] = xh; out[29] = yl; out[30] = xl; out[31] = 1.0f;
}

/* Bits 0/1 x above/below, 2/3 y, 4/5 w. */
/* 0x001c24a8 */
int ulGxBoxOutcode(float *pt, float *lo, float *hi)
{
    int code = 0;

    if (pt[0] > hi[0])
        code = 1;
    if (pt[0] < lo[0])
        code |= 2;
    if (pt[1] > hi[1])
        code |= 4;
    if (pt[1] < lo[1])
        code |= 8;
    if (pt[3] > hi[3])
        code |= 16;
    if (pt[3] < lo[3])
        code |= 32;
    return code;
}

/* Light matrix, clamp to 0..1, same matrix again for the colour. */
/* 0x001c2538 */
static void ulGxClampLightColor(char *gx, void *nrm, void *out)
{
    float lit[4];

    vu0_matrix_apply(gx + 448, nrm, lit);
    vu0_vec_clamp(lit, lit, 0.0f, 1.0f);
    vu0_matrix_apply(gx + 448, lit, out);
}

/* Model-view into view, projection, divide into out. Returns 1 / w. */
/* 0x001c2598 */
static float ulGxProjectVertex(char *gx, void *pos, void *view, void *out)
{
    float clip[4];
    float iw;
    char *mtx = gx + 128;

    vu0_matrix_apply(gx + 192, pos, view);
    vu0_matrix_apply(mtx, view, clip);
    iw = 1.0f / clip[3];
    vu0_vec_scale(out, clip, iw);
    return iw;
}

/*
 * n vertices (position + normal, six floats) into gx->vtx. Returns the outcode OR (0 = no
 * clipping). out unused.
 */
/* 0x001c2620 */
static int ulGxTransformVertices(UlGxV *gx, int flags, float *src, int n, char *out)
{
    float pos[4];
    float nrm[4];
    char *vtx;
    int i;
    int code = 0;
    int si = (int)src;
    int ni = (int)(src + 3);

    pos[3] = 1.0f;
    nrm[3] = 0;
    vtx = gx->vtx;
    for (i = 0; i < n; i++) {
        pos[0] = ((float *)si)[0];
        pos[1] = ((float *)si)[1];
        pos[2] = ((float *)si)[2];
        si += 24;
        *(float *)(vtx + 56) = ulGxProjectVertex(gx, pos, vtx, vtx + 16);
        nrm[0] = ((float *)ni)[0];
        nrm[1] = ((float *)ni)[1];
        nrm[2] = ((float *)ni)[2];
        ni += 24;
        ulGxClampLightColor(gx, nrm, vtx + 32);
        code |= ulGxVertexOutcode(gx, vtx);
        vtx += 64;
    }
    return code;
}

/* Only with both low bits of flags. idx is ignored downstream: vertices land at gx->vtx[0]. */
/* 0x001c2730 */
static int ulGxTransformVerticesAt(UlGxCtx *gx, int flags, float *src, int n, int idx)
{
    char *out = gx->vtx + idx * 64;
    int code = 0;

    if ((flags & 3) == 3)
        code = ulGxTransformVertices(gx, flags, src, n, out);
    return code;
}

/* With ulGxProj; camera looks down -z, screen y down. out[2] = 2^20 / -z. Returns 1 / -z. */
/* 0x001c2770 */
static float ulGxProjectPoint(float *pt, float *out)
{
    float iz = -1.0f / pt[2];
    float scale = ulGxProj.dist * iz;

    out[0] = pt[0] * scale * ulGxProj.xscale + ulGxProj.cx;
    out[1] = -pt[1] * scale * ulGxProj.yscale + ulGxProj.cy;
    out[2] = iz * 1048576.0f;
    return iz;
}

/* ulGxViewMatrix (3x4), then ulGxProjectPoint. */
/* 0x001c27f0 */
static float ulGxTransformProjectPoint(float *pt, float *out)
{
    float view[3];
    float *mtx = ulGxViewMatrix;

    view[0] = mtx[0] * pt[0] + mtx[1] * pt[1] + mtx[2] * pt[2] + mtx[3];
    view[1] = mtx[4] * pt[0] + mtx[5] * pt[1] + mtx[6] * pt[2] + mtx[7];
    view[2] = mtx[8] * pt[0] + mtx[9] * pt[1] + mtx[10] * pt[2] + mtx[11];
    return ulGxProjectPoint(view, out);
}

/* ulGxState MVP, x/y/z divided by w. Returns 1 / w. */
/* 0x001c28a8 */
float ulGxTransformDivide(void *in, float *out)
{
    float iw;
    char *gx = ulGxState;

    vu0_matrix_apply(gx + 320, in, out);
    iw = 1.0f / out[3];
    out[0] = out[0] * iw;
    out[1] = out[1] * iw;
    out[2] = out[2] * iw;
    return iw;
}

/* 0x001c2920 */
static void ulGxTransformModelView(void *in, void *out)
{
    char *gx = ulGxState;

    vu0_matrix_apply(gx + 192, in, out);
}

/* Into vtx->code: 1/2 x above/below, 4/8 y, 32/16 depth above/below. */
/* 0x001c2940 */
static int ulGxVertexOutcode(UlGxClip *gx, UlGxVtx *vtx)
{
    int code = 0;
    float xlo;
    float ylo;
    float yhi;
    float xhi;

    ylo = gx->ylo;
    yhi = gx->yhi;
    xlo = gx->xlo;
    xhi = gx->xhi;

    if (gx->zhi < vtx->f[2])
        code = 32;
    else if (vtx->f[2] < gx->zlo)
        code = 16;
    if (xhi < vtx->f[4])
        code |= 1;
    else if (vtx->f[4] < xlo)
        code |= 2;
    if (yhi < vtx->f[5])
        code |= 4;
    else if (vtx->f[5] < ylo)
        code |= 8;
    vtx->code = code;
    return code;
}

/*
 * Header: first vertex, count, pass (first, middle, last; last also envmaps and adds to
 * gx->nvtx). Returns the pass's result, 0 for an unknown pass.
 */
/* 0x001c29d8 */
int ulGxVlistPacketClip(UlGxCtx *gx, int *pkt)
{
    int *body = pkt + 2;
    int cnt = (unsigned int)pkt[1] >> 16;
    char *vtx = gx->vtx + (pkt[1] & 0xffff) * 64;
    int ret;

    switch (pkt[0] & 0x7f00) {
    case 0:
        return vu0_gx_skin_lit_first(gx, body, vtx, cnt);
    case 0x100:
        return vu0_gx_skin_lit_mid(gx, body, vtx, cnt);
    case 0x200:
        ret = vu0_gx_skin_lit_env_last_clip(gx, body, vtx, cnt);
        gx->nvtx += cnt;
        return ret;
    }
    return 0;
}

/* For a model fully on screen (no outcodes). */
/* 0x001c2aa8 */
void ulGxVlistPacketNoClip(UlGxCtx *gx, int *pkt)
{
    int *body = pkt + 2;
    int cnt = (unsigned int)pkt[1] >> 16;
    char *vtx = gx->vtx + (pkt[1] & 0xffff) * 64;

    switch (pkt[0] & 0x7f00) {
    case 0:
        vu0_gx_skin_lit_first(gx, body, vtx, cnt);
        break;
    case 0x100:
        vu0_gx_skin_lit_mid(gx, body, vtx, cnt);
        break;
    case 0x200:
        vu0_gx_skin_lit_env_last(gx, body, vtx, cnt);
        gx->nvtx += cnt;
        break;
    }
}

/* Unskinned, with outcodes; does not add to gx->nvtx. */
/* 0x001c2b80 */
static int ulGxVlistEnvClip(UlGxCtx *gx, int *pkt)
{
    int hdr = pkt[1];

    return vu0_gx_project_lit_env_clip(gx, pkt + 2, gx->vtx + (hdr & 0xffff) * 64, (unsigned int)hdr >> 16);
}

/* Unskinned, no outcodes; adds to gx->nvtx. */
/* 0x001c2bb8 */
static void ulGxVlistEnv(UlGxCtx *gx, int *pkt)
{
    int cnt = (unsigned int)pkt[1] >> 16;

    vu0_gx_project_lit_env(gx, pkt + 2, gx->vtx + (pkt[1] & 0xffff) * 64, cnt);
    gx->nvtx += cnt;
}

/*
 * |n| vertices, back faces culled (n < 0 flips the winding). On-screen runs go out as GIF packets;
 * a triangle needing clipping goes through ulGxClipDrawTriangle.
 */
/* 0x001c2c10 */
void ulGxEmitStrip(UlGxStrip *gx, short *idx, int n, int vbuf)
{
    UlPkt *pk = psGsGetRefPkt();
    float vtx0[16], vtx1[16], vtx2[16];
    PsGifTag *cur;
    PsGifTag *data;
    short *p;
    int sign;
    int emitted;
    int total;

    gx->tag.regs = 65;
    *(unsigned long *)&gx->tag = (((gx->prim & ~7) | 4) << 47) | ((unsigned long)0x8001 << 46);
    if (n > 0) {
        sign = 1;
    } else {
        n = -n;
        sign = -1;
    }
    p = idx;
    while (n > 2) {
        vu0_gx_load_strip_consts(gx);
        cur = pk->cur;
        data = cur + 1;
        emitted = vu0_gx_emit_strip_cull_clip(gx, p, sign * n, vbuf, data);
        if (emitted > 2) {
            if (emitted & 1) sign = -sign;
            *(u_long128 *)cur = *(u_long128 *)&gx->tag;
            cur->nloop = emitted;
            pk->gtag = cur;
            pk->nqw += 1 + emitted * 2;
            p += emitted - 2;
            total = n + 2;
            n = total - emitted;
            pk->cur = data + emitted * 2;
        }
        if (emitted < n) {
            n--;
            vu0_clip_load_consts(gx);
            vu0_gx_clip_copy(gx, p, 3, vbuf, vtx0);
            p++;
            ulGxClipDrawTriangle(gx, vtx0, vtx1, vtx2, sign);
            sign = -sign;
        }
    }
}

/* 0x001c2dc8 */
static void ulGxEmitStripNoCull(UlGxStrip *gx, short *idx, int n, int vbuf)
{
    UlPkt *pk = psGsGetRefPkt();
    float vtx0[16], vtx1[16], vtx2[16];
    PsGifTag *cur;
    PsGifTag *data;
    short *p = idx;
    int done = 0;
    int lim = n - 2;
    int emitted;
    int total;
    int k;

    gx->tag.regs = 65;
    *(unsigned long *)&gx->tag = (((gx->prim & ~7) | 4) << 47) | ((unsigned long)0x8001 << 46);
    vu0_gx_load_strip_consts(gx);
    do {
        cur = pk->cur;
        data = cur + 1;
        emitted = vu0_gx_emit_strip_clip(gx, p, n, vbuf, data);
        if (emitted > 2) {
            k = done - 2;
            *(u_long128 *)cur = *(u_long128 *)&gx->tag;
            cur->nloop = emitted;
            pk->nqw += 1 + emitted * 2;
            pk->gtag = cur;
            pk->cur = data + emitted * 2;
            done = k + emitted;
            p += emitted - 2;
            total = n + 2;
            n = total - emitted;
        }
        if (done < lim) {
            done++;
            vu0_clip_load_consts(gx);
            vu0_gx_clip_copy(gx, p++, 3, vbuf, vtx0);
            ulGxClipDrawTriangle(gx, vtx0, vtx1, vtx2, 0);
            n--;
        }
    } while (done < lim);
}

/*
 * Separately indexed UVs: three indices per entry. Clipped triangles via ulGxClipDrawTexTriangle.
 */
/* 0x001c2f68 */
void ulGxEmitTexStripUV(UlGxStrip *gx, short *idx, int n, int vbuf)
{
    UlPkt *pk = psGsGetRefPkt();
    float vtx0[16], vtx1[16], vtx2[16];
    PsGifTag *cur;
    PsGifTag *data;
    short *p;
    int sign;
    int emitted;
    int total;
    int qwords;
    int k;

    gx->tag.regs = 1042;
    *(unsigned long *)&gx->tag = (((gx->prim & ~7) | 20) << 47) | ((unsigned long)0xc001 << 46);
    if (n > 0) {
        sign = 1;
    } else {
        n = -n;
        sign = -1;
    }
    p = idx;
top:
    if (n > 2) {
        do {
            vu0_gx_load_strip_consts(gx);
            cur = pk->cur;
            data = cur + 1;
            emitted = vu0_gx_emit_uv_strip_cull_clip(gx, p, sign * n, vbuf, data);
            if (emitted > 2) {
                qwords = emitted * 3;
                *(u_long128 *)cur = *(u_long128 *)&gx->tag;
                k = emitted - 2;
                p += k * 3;
                total = n + 2;
                if (k & 1)
                    sign = -sign;
                cur->nloop = emitted;
                n = total - emitted;
                pk->gtag = cur;
                pk->cur = data + qwords;
                pk->nqw += 1 + qwords;
            }
            if (emitted >= n)
                goto top;
            n--;
            vu0_clip_load_consts(gx);
            vu0_gx_clip_copy_uv(gx, p, 3, vbuf, vtx0);
            p += 3;
            ulGxClipDrawTexTriangle(gx, vtx0, vtx1, vtx2, sign);
            sign = -sign;
        } while (0);
        goto top;
    }
}

/* 0x001c3120 */
static void ulGxEmitTexStripUVNoCull(UlGxStrip *gx, short *idx, int n, int vbuf)
{
    UlPkt *pk = psGsGetRefPkt();
    PsGifTag *cur;
    PsGifTag *data;
    int done = 0;
    float vtx0[16], vtx1[16], vtx2[16];
    short *p = idx;
    int emitted;
    int lim = n - 2;
    int total;
    int qwords;
    int k;

    *(unsigned long *)&gx->tag = (((gx->prim & ~7) | 20) << 47) | ((unsigned long)0xc001 << 46);
    gx->tag.regs = 1042;
    do {
        vu0_gx_load_strip_consts(gx);
        cur = pk->cur;
        data = cur + 1;
        emitted = vu0_gx_emit_uv_strip_clip(gx, p, n, vbuf, data);
        if (emitted > 2) {
            qwords = emitted * 3;
            *(u_long128 *)cur = *(u_long128 *)&gx->tag;
            k = done - 2;
            total = n + 2;
            n = total - emitted;
            done = k + emitted;
            cur->nloop = emitted;
            p += (emitted - 2) * 3;
            pk->gtag = cur;
            pk->cur = data + qwords;
            pk->nqw += 1 + qwords;
        }
        if (done < lim) {
            done++;
            vu0_gx_load_strip_consts(gx);
            n--;
            vu0_clip_load_consts(gx);
            vu0_gx_clip_copy_uv(gx, p, 3, vbuf, vtx0);
            ulGxClipDrawTexTriangle(gx, vtx0, vtx1, vtx2, 0);
            p += 3;
        }
    } while (done < lim);
}

/* One index per vertex (UV from the vertex). Clipped triangles via ulGxClipDrawTexTriangle. */
/* 0x001c32d0 */
void ulGxEmitTexStrip(UlGxStrip *gx, short *idx, int n, int vbuf)
{
    UlPkt *pk = psGsGetRefPkt();
    float vtx0[16], vtx1[16], vtx2[16];
    PsGifTag *cur;
    PsGifTag *data;
    short *p;
    int sign;
    int emitted;
    int total;
    int qwords;

    gx->tag.regs = 1042;
    *(unsigned long *)&gx->tag = (((gx->prim & ~7) | 20) << 47) | ((unsigned long)0xc001 << 46);
    if (n > 0) {
        sign = 1;
    } else {
        n = -n;
        sign = -1;
    }
    p = idx;
top:
    if (n > 2) {
        do {
            vu0_gx_load_strip_consts(gx);
            cur = pk->cur;
            data = cur + 1;
            emitted = vu0_gx_emit_tex_strip_cull_clip(gx, p, sign * n, vbuf, data);
            if (emitted > 2) {
                qwords = emitted * 3;
                if (emitted & 1) sign = -sign;
                *(u_long128 *)cur = *(u_long128 *)&gx->tag;
                cur->nloop = emitted;
                pk->gtag = cur;
                pk->nqw += 1 + qwords;
                p += emitted - 2;
                total = n + 2;
                n = total - emitted;
                pk->cur = data + qwords;
            }
            if (emitted < n) {
                n--;
                vu0_clip_load_consts(gx);
                vu0_gx_clip_copy_b(gx, p, 3, vbuf, vtx0);
                p++;
                ulGxClipDrawTexTriangle(gx, vtx0, vtx1, vtx2, sign);
                sign = -sign;
            }
        } while (0);
        goto top;
    }
}

/*
 * Tag PRIM 20, NREG 3, regs 0x412 (ST, RGBAQ, XYZF2). vu0_gx_emit_tex_strip_clip writes what fits
 * after a GIF tag copy (NLOOP patched); the rest are clipped one at a time.
 */
/* 0x001c3480 */
static void ulGxEmitTexStripNoCull(UlGxStrip *gx, short *idx, int n, int vbuf)
{
    UlPkt *pk = psGsGetRefPkt();
    PsGifTag *cur;
    PsGifTag *data;
    int done = 0;
    float vtx0[16], vtx1[16], vtx2[16];
    short *p = idx;
    int emitted;
    int lim = n - 2;
    int total;
    int qwords;
    int k;

    gx->tag.regs = 1042;
    *(unsigned long *)&gx->tag = (((gx->prim & ~7) | 20) << 47) | ((unsigned long)0xc001 << 46);
    do {
        vu0_gx_load_strip_consts(gx);
        cur = pk->cur;
        data = cur + 1;
        emitted = vu0_gx_emit_tex_strip_clip(gx, p, n, vbuf, data);
        if (emitted > 2) {
            qwords = emitted * 3;
            *(u_long128 *)cur = *(u_long128 *)&gx->tag;
            k = done - 2;
            done = k + emitted;
            p += emitted - 2;
            cur->nloop = emitted;
            total = n + 2;
            pk->gtag = cur;
            n = total - emitted;
            pk->cur = data + qwords;
            pk->nqw += 1 + qwords;
        }
        if (done < lim) {
            n--;
            vu0_clip_load_consts(gx);
            vu0_gx_clip_copy_b(gx, p++, 3, vbuf, vtx0);
            ulGxClipDrawTexTriangle(gx, vtx0, vtx1, vtx2, 0);
            done++;
        }
    } while (done < lim);
}
