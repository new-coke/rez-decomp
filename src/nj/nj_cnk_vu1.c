/*
 * VU1 chunk-model renderer, the game's main one. Vertices transformed/skinned/lit on VU0 or copied
 * from the model; each strip packed into a VU1 block with its GIF tag. Edge-crossing strips:
 * unclipped runs to VU1, crossing triangles clipped on the EE and sent as fans.
 */

#include "types_nj.h"
#include "ninja.h"
#include "nj_cnk_vu1.h"
#include "ul_gx.h"
#include "types_main.h"
#include "ps_displist.h"
#include "ps_gs.h"
#include "ps_pkt.h"
#include "ps_timer.h"
#include "ul_vu0.h"

static void nj_cnk_vu1_uv_strips_raw(NjSysView *sys, short *p, NjVtxSrc *vtxsrc);
static void nj_cnk_vu1_strips_raw(NjSysView *sys, short *p, NjVtxSrc *vtxsrc);
static void nj_cnk_vu1_strips_unlit_env(NjSysView *sys, short *p);
static void nj_cnk_vu1_uv_strips_unlit(NjSysView *sys, short *p);
static void nj_cnk_vu1_strips_unlit(NjSysView *sys, short *p);
static void nj_cnk_vu1_strips_env(NjCnkCtx *ctx, short *p);
static void nj_cnk_vu1_uv_strips_lit(NjCnkCtx *ctx, short *p);
static void nj_cnk_vu1_strips_lit(NjCnkCtx *ctx, short *p);
static void nj_cnk_vu1_strips_env_noclip(NjCnkCtx *ctx, short *list);
static void nj_cnk_vu1_uv_strips_noclip(NjCnkCtx *ctx, short *list);
static void nj_cnk_vu1_strips_noclip(NjCnkCtx *ctx, short *list);

typedef int qword __attribute__((mode(TI)));
typedef void (*NjStripFn)(NjSysView *sys, short *idx, int n, int *vtx, int *xyz, int *nrm, int *col, int *st);

#define NJ_SYS_COUNT ((NjSysCount *)&ulGxState)

/* Display-list VIF writer (psGsGetRefPkt returns the open one). */
typedef struct NjCnkVu1NjVifPkt {
    int start;
    char *cur;                  /* 0x04: write pointer */
    int nwords;                 /* 0x08: words written */
    char pad_0c[4];
    unsigned short *vif;        /* 0x10 */
    int nq;                     /* 0x14: quadwords in the open GIF packet */
    NjGifTagBits *gif;          /* 0x18: last GIF tag */
    int nreg;                   /* 0x1c */
} NjCnkVu1NjVifPkt;

typedef void (*NjCnkConv)(NjCnkCtx *ctx, short *q, int n, char *vbuf, char *o1, char *o2, char *o3);
typedef void (*NjCnkVu1NjCnkConv)(NjCnkCtx *ctx, short *q, int n, char *vbuf, char *o1, char *o2);

extern float nj_light_intensity[4];
extern float nj_ambient_intensity[4];
extern float nj_cnk_override_rgba[];
extern void nj_vu0_unpack_color(NjSysView *sys, unsigned char *p, float *out, float *add);
extern NjQword nj_cnk_white;
extern NjVtxSrc nj_cnk_vu1_vtxsrc;
extern void nj_vu0_vu1_skin_lit_first(NjSysView *sys, int *src, char *dst, int n);
extern void nj_vu0_vu1_skin_lit_mid(NjSysView *sys, int *src, char *dst, int n);
extern int nj_vu0_vu1_skin_lit_env_last_clip(NjSysView *sys, int *src, char *dst, int n);
extern int nj_vu0_vu1_pos_clip(NjSysView *sys, int *src, char *dst, int n);
extern int nj_vu0_vu1_lit_env_clip(NjSysView *sys, int *src, char *dst, int n);
extern int nj_vu0_vu1_col_clip(NjSysView *sys, int *src, char *dst, int n);
extern int nj_vu0_vu1_env_clip(NjSysView *sys, int *src, char *dst, int n);
extern int nj_vu0_vu1_skin_lit_env_last(NjSysView *sys, int *src, char *dst, int n);
extern int nj_vu0_vu1_pos(NjSysView *sys, int *src, char *dst, int n);
extern int nj_vu0_vu1_lit_env(NjSysView *sys, int *src, char *dst, int n);
extern int nj_vu0_vu1_col(NjSysView *sys, int *src, char *dst, int n);
extern int nj_vu0_vu1_env(NjSysView *sys, int *src, char *dst, int n);
extern void nj_object_walk(NjObject *obj, int (*draw)(void *model));
extern float nj_cnk_col_scale;
extern float nj_cnk_col_scale2;
extern void nj_vu0_copy_nrm_col(NjSysView *sys, short *idx, int n, int *vtx, int *xyz, int *nrm, int *col, int *st);
extern int nj_vu0_gather_white_st_clip(NjSysView *sys, short *p, int n, int *vtx, int *xyz, int *col, int *st);
extern void nj_vu0_copy_white(NjSysView *sys, short *p, int n, int *vtx, NjQword *dst);
extern int nj_vu0_gather_white_uv_clip(NjSysView *sys, NjCnkIdx *p, int n, int *vtx, int *xyz, int *col, int *st);
extern void nj_vu0_copy_white_uv(NjSysView *sys, NjCnkIdx *p, int n, int *vtx, NjQword *dst);
extern int nj_vu0_gather_white_clip(NjSysView *sys, short *p, int n, int *vtx, int *xyz, int *col);
extern void nj_vu0_copy_white_b(NjSysView *sys, short *p, int n, int *vtx, NjQword *dst);
extern int nj_vu0_gather_st_clip(NjCnkCtx *ctx, short *q, int n, char *vbuf, char *o1, char *o2, char *o3);
extern int nj_vu0_gather_uv_clip(NjCnkCtx *ctx, short *q, int n, char *vbuf, char *o1, char *o2, char *o3);
extern void nj_vu0_copy_uv(NjCnkCtx *ctx, short *q, int n, char *vbuf, char *out);
extern int nj_vu0_gather_clip(NjCnkCtx *ctx, short *q, int n, char *vbuf, char *o1, char *o2);
extern void nj_vu0_copy(NjCnkCtx *ctx, short *q, int n, char *vbuf, char *out);
extern int nj_vu0_emit_fog_clip(NjCnkCtx *ctx, short *idx, int n, char *vbuf, NjQword *out);
extern char *vu0_clip_emit_tex_fog(NjCnkCtx *ctx, int a, int n, char *out);
extern int abs(int);
extern int nj_vu0_emit_fog_uv_clip(NjCnkCtx *ctx, short *idx, int n, char *vbuf, NjQword *out);
extern int nj_vu0_emit_fog_col_clip(NjCnkCtx *ctx, short *idx, int n, char *vbuf, NjQword *out);
extern char *vu0_clip_emit_fog(NjCnkCtx *ctx, int a, int n, char *out);
extern void nj_vu0_gather_st(NjCnkCtx *ctx, short *q, int n, char *vbuf, char *o1, char *o2, char *o3);
extern void nj_vu0_gather_uv(NjCnkCtx *ctx, short *q, int n, char *vbuf, char *o1, char *o2, char *o3);
extern void nj_vu0_gather(NjCnkCtx *ctx, short *q, int n, char *vbuf, char *o1, char *o2);
extern void nj_vu0_gather_white_st(NjCnkCtx *ctx, short *q, int n, char *vbuf, char *o1, char *o2, char *o3);
extern void nj_vu0_gather_white_uv(NjCnkCtx *ctx, short *q, int n, char *vbuf, char *o1, char *o2, char *o3);
extern void nj_vu0_gather_white(NjCnkCtx *ctx, short *q, int n, char *vbuf, char *o1, char *o2);

#ifdef VERSION_US

#endif /* VERSION_US */

extern int vu0_clip_gather();
extern int vu0_clip_gather_st();

/* As colour vectors; ambient is nj_cnk_strip_state's second VU1 colour. */
/* 0x001a8130 */
void nj_set_light_intensity(float light, float ambient)
{
    float *light_vec = nj_light_intensity;
    float *ambient_vec = nj_ambient_intensity;

    light_vec[3] = 1.0f;
    light_vec[0] = light;
    ambient_vec[0] = ambient;
    light_vec[1] = light;
    light_vec[2] = light;
    *(int *)&ambient_vec[3] = 0;
    ambient_vec[1] = ambient;
    ambient_vec[2] = ambient;
}

#ifdef VERSION_US

/*
 * Material chunk 16-23: type bits 0-2 pick diffuse, ambient, specular (b, g, r, a bytes); override
 * flags replace or add diffuse. Bug: src only advances past a replaced diffuse (same 4 bytes).
 */
/* 0x001a8170 */
void nj_cnk_vu1_set_material(NjSysView *sys, int flags, int type, unsigned char *src)
{
    float *col;
    int bits;

    bits = (type & 0xff) - 16;
    col = sys->mat_col;
    if (bits & 1) {
        if (nj_cnk_override_flags & 0x10) {
            src += 4;
            *(qword *)sys->mat_col = *(qword *)nj_cnk_override_rgba;
        } else if (nj_cnk_override_flags & 0x20) {
            nj_vu0_unpack_color(sys, src, col, nj_cnk_override_rgba);
        } else {
            nj_vu0_unpack_color(sys, src, col, 0);
        }
    }
    if (bits & 2) {
        nj_vu0_unpack_color(sys, src, col + 4, 0);
    }
    if (bits & 4) {
        nj_vu0_unpack_color(sys, src, col + 8, 0);
    }
    sys->mat_flags = flags;
    sys->alpha = ulGxAlphaTable[(flags & 0x3f00) >> 8];
}

#else

/* As above, converted in C, each colour from its own four bytes; an added override is clamped. */
/* 0x001a8170 */
void nj_cnk_vu1_set_material(NjSysView *sys, int flags, int type, unsigned char *src)
{
    float *col;
    float k;
    int bits;

    bits = (type & 0xff) - 16;
    col = sys->mat_col;
    k = 1.0f / 255.0f;
    if (bits & 1) {
        if (nj_cnk_override_flags & 0x10) {
            vu0_qword_copy(col, nj_cnk_override_rgba);
            src += 4;
        } else {
            col[2] = (float)*src++ * k;
            col[1] = (float)*src++ * k;
            col[0] = (float)*src++ * k;
            col[3] = (float)*src++ * k;
            if (nj_cnk_override_flags & 0x20) {
                vu0_vec_add(col, col, nj_cnk_override_rgba);
                vu0_vec_clamp(col, col, 0.0f, 1.0f);
            }
        }
    }
    if (bits & 2) {
        col[6] = (float)*src++ * k;
        col[5] = (float)*src++ * k;
        col[4] = (float)*src++ * k;
        col[7] = (float)*src++ * k;
    }
    if (bits & 4) {
        col[10] = (float)*src++ * k;
        col[9] = (float)*src++ * k;
        col[8] = (float)*src++ * k;
        col[11] = (float)*src++ * k;
    }
    sys->mat_flags = flags;
    sys->alpha = ulGxAlphaTable[(flags & 0x3f00) >> 8];
}

#endif /* !VERSION_US */

/* VU1 GIF tag templates: one GS setting; textured vertex (UV, colour, position). */
/* 0x001a8288 */
static void nj_vu1_upload_giftags(void)
{
    PsVifPkt *pkt;
    unsigned long tex_tag[2];
    unsigned long single_tag[2];

    pkt = psGsGetRefPkt();
    single_tag[0] = 0x1000000000000001UL;
    tex_tag[0] = 0x3000000000008000UL;
    tex_tag[1] = 0x412;
    single_tag[1] = 0;
    psVifPktAddWord(pkt, 0x01000101);
    psVifPktAddUnpack(pkt, 0x6c01004b, single_tag, 1);
    psVifPktAddUnpack(pkt, 0x6c01004c, tex_tag, 1);
}

/*
 * From strip flags (after override): blend, fog, material/ambient (white/none for coloured
 * vertices), light mode (unlit if ignore-light or no normals; env), double-sided setting.
 */
/* 0x001a8320 */
void nj_cnk_strip_state(NjSysView *sys, int type, int flags, int vtype)
{
    void *dlist;
    PsVifPkt *pkt;
    int mode, abe, fog;

    mode = 0;
    dlist = psGsGetDisplayList();
    sys->strip_flags = flags;
    if (nj_cnk_override_flags & 0x800) {
        int and_mask = nj_cnk_strip_and_mask & 0xff00;
        int or_bits = nj_cnk_strip_or_bits & 0xff00;
        flags &= and_mask;
        flags |= or_bits;
    }
    if (flags & 0x800) {
        abe = 64;
        psDisplayListUpdateGsReg(dlist, 66, sys->alpha);
    } else {
        abe = 0;
    }
    fog = (flags & 0x2000) ? 0 : 8;
    sys->prim = (sys->prim & ~0x48) | (abe | fog);
    if (vtype == 35 || vtype == 42) {
        mode = 4;
        sys->mat_col_q = nj_cnk_white;
        sys->col2_q = REZ_QWORD_ZERO(NjQword);
    } else if ((flags & 0x4000) || type == 65) {
        sys->mat_col_q = *(NjQword *)sys->mat_col;
        sys->col2_q = (*(NjQword *)&nj_ambient_intensity); /* steer(cast) */
    } else {
        sys->mat_col_q = *(NjQword *)sys->mat_col;
        if (flags & 0x400)
            sys->col2_q = REZ_QWORD_ZERO(NjQword);
        else
            sys->col2_q = (*(NjQword *)&nj_ambient_intensity); /* steer(cast) */
    }
    if (flags & 0x4000)
        mode |= 2;
    if ((flags & 0x100) || vtype == 34)
        mode |= 1;
    sys->vu1_param = (sys->vu1_param & 0xfffffff8) | mode;
    sys->unk_40c = (flags & 0x1000) ? 0 : 0x8000;
    pkt = psGsGetRefPkt();
    psVifPktAddWord(pkt, 0x01000101);
    psVifPktAddUnpack(pkt, 0x6c02003d, &sys->mat_col_q, 2);
    psVifPktAddUnpack(pkt, 0x6c010040, &sys->vu1_param, 1);
}

/* 1 when ignore-light and the vertices have no colours (types other than 35, 42): drawn white. */
/* 0x001a8510 */
static int nj_cnk_vu1_is_lit(NjSysView *sys, int flags)
{
    int ret = 0;
    int vtype;

    if (flags & 0x100) {
        vtype = sys->vtx_type;
        if (vtype == 35)
            return ret;
        ret = vtype != 42;
    }
    return ret;
}

/*
 * Compiled list (nj_compile.c) on VU1 with clipping: per strip chunk, state, GIF tag (textured
 * for textured/env strips), parameters and two matrices to VU1, clip program, data by reference.
 */
/* 0x001a8540 */
static void nj_cnk_vu1pkt_plist_clip(short *p)
{
    NjSysView *sys = NJ_SYS_VIEW;
    void *dlist, *pkt;
    int head, type, flags, size;
    int *tag;
    int prim;

    dlist = psGsGetDisplayList();
    head = *p++;
    while (head != 255) {
        type = head & 0xff;
        flags = head & 0xff00;
        if (type < 8) {
        } else if (type < 16) {
            if (type == 8)
                ulGxDlSetTexture(sys, flags, *p++);
        } else if (type < 64) {
            int len = *p++;
            nj_cnk_vu1_set_material(sys, flags, type, p);
            p += len;
        } else {
            size = (unsigned short)*p++;
            if (type < 67) {
                if (type >= 64) {
                    pkt = psGsOpenRefPkt();
                    nj_cnk_strip_state(sys, type, flags, p[2]);
                    if (psDisplayListIsDirty(dlist)) {
                        psVifPktOpenGifPkt(pkt);
                        psDisplayListFlushGsRegs(dlist, pkt);
                        psVifPktCloseGifPkt(pkt);
                    }
                    if ((head & 0x4000) || type == 65 || type == 66) {
                        prim = sys->prim | 0x14;
                        tag = sys->vu1_tex_tag;
                    } else {
                        prim = sys->prim | 4;
                        tag = sys->vu1_tag;
                    }
                    sys->strip_prim = prim;
                    psVifPktAddWord(pkt, 0x01000101);
                    psVifPktAddWord(pkt, 0x6c010055);
                    psVifPktAddWords(pkt, tag, 4);
                    psVifPktAddWord(pkt, 0x6c010040);
                    psVifPktAddWords(pkt, &sys->vu1_param, 4);
                    psVifPktAddWord(pkt, 0x6c040059);
                    psVifPktAddWords(pkt, (char *)sys + 0x140, 16);
                    psVifPktAddWord(pkt, 0x6c040030);
                    psVifPktAddWords(pkt, (char *)sys + 0x300, 16);
                    psVifPktAddWord(pkt, 0x15000018);
                    psGsCloseRefPkt();
                    psDisplayListAddRefPkt(dlist, p + 5 + p[4], p[3]);
                }
            }
            p += size;
        }
        head = *p++;
    }
}

/* Adds the time to the VU1 timing. Returns 0. */
/* 0x001a87b8 */
int nj_cnk_vu1pkt_draw_clip(NjCnkModel *model)
{
    NjSysView *sys;
    int ticks;

    ulGxSetMatrix(0, ulMatrixCur);
    if (model->plist) {
        ticks = psTimerGetCount();
        psGsOpenRefPkt();
        nj_vu1_upload_giftags();
        psGsCloseRefPkt();
        nj_cnk_vu1pkt_plist_clip(model->plist);
        ticks = psTimerDiff(psTimerGetCount(), ticks);
        sys = NJ_SYS_VIEW;
        sys->vu1_ticks += ticks;
    }
    return 0;
}

/* No clipping: one matrix, non-clipping program. */
/* 0x001a8850 */
static void nj_cnk_vu1pkt_plist_noclip(short *p)
{
    NjSysView *sys = NJ_SYS_VIEW;
    void *dlist, *pkt;
    int head, type, flags, size;
    int *tag;
    int prim;

    dlist = psGsGetDisplayList();
    head = *p++;
    while (head != 255) {
        type = head & 0xff;
        flags = head & 0xff00;
        if (type < 8) {
        } else if (type < 16) {
            if (type == 8)
                ulGxDlSetTexture(sys, flags, *p++);
        } else if (type < 64) {
            int len = *p++;
            nj_cnk_vu1_set_material(sys, flags, type, p);
            p += len;
        } else {
            size = (unsigned short)*p++;
            if (type < 67) {
                if (type >= 64) {
                    pkt = psGsOpenRefPkt();
                    nj_cnk_strip_state(sys, type, flags, p[2]);
                    if (psDisplayListIsDirty(dlist)) {
                        psVifPktOpenGifPkt(pkt);
                        psDisplayListFlushGsRegs(dlist, pkt);
                        psVifPktCloseGifPkt(pkt);
                    }
                    if ((head & 0x4000) || type == 65 || type == 66) {
                        prim = sys->prim | 0x14;
                        tag = sys->vu1_tex_tag;
                    } else {
                        prim = sys->prim | 4;
                        tag = sys->vu1_tag;
                    }
                    sys->strip_prim = prim;
                    psVifPktAddWord(pkt, 0x01000101);
                    psVifPktAddWord(pkt, 0x6c010055);
                    psVifPktAddWords(pkt, tag, 4);
                    psVifPktAddWord(pkt, 0x6c010040);
                    psVifPktAddWords(pkt, &sys->vu1_param, 4);
                    psVifPktAddWord(pkt, 0x6c040059);
                    psVifPktAddWords(pkt, (char *)sys + 0x140, 16);
                    psVifPktAddWord(pkt, 0x1500001a);
                    psGsCloseRefPkt();
                    psDisplayListAddRefPkt(dlist, p + 5 + p[4], p[3]);
                }
            }
            p += size;
        }
        head = *p++;
    }
}

/* No clipping. */
/* 0x001a8aa8 */
int nj_cnk_vu1pkt_draw_noclip(NjCnkModel *model)
{
    NjSysView *sys;
    int ticks;

    ulGxSetMatrix(0, ulMatrixCur);
    if (model->plist) {
        ticks = psTimerGetCount();
        psGsOpenRefPkt();
        nj_vu1_upload_giftags();
        psGsCloseRefPkt();
        nj_cnk_vu1pkt_plist_noclip(model->plist);
        ticks = psTimerDiff(psTimerGetCount(), ticks);
        sys = NJ_SYS_VIEW;
        sys->vu1_ticks += ticks;
    }
    return 0;
}

/* Clipping unless the no-clip draw flag is set. */
/* 0x001a8b40 */
static int nj_cnk_vu1pkt_draw(NjCnkModel *model)
{
    if (ulGxGetDrawFlags() & 1)
        return nj_cnk_vu1pkt_draw_noclip(model);
    return nj_cnk_vu1pkt_draw_clip(model);
}

/* From nj_cnk_vu1_vtxsrc, no VU0 transform; strip chunks 64, 65 copied into VU1 blocks. */
/* 0x001a8b88 */
static void nj_cnk_vu1_plist_raw(short *p)
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
                ulGxDlSetTexture(sys, flags, *p++);
        } else if (type < 64) {
            int len = *p++;
            nj_cnk_vu1_set_material(sys, flags, type, p);
            p += len;
        } else {
            size = *p++;
            switch (type) {
            case 64:
                dlist = psGsGetDisplayList();
                pkt = psDisplayListGetVifPkt(dlist);
                nj_cnk_strip_state(sys, 64, flags, sys->vtx_type);
                if (psDisplayListIsDirty(dlist)) {
                    psVifPktOpenGifPkt(pkt);
                    psDisplayListFlushGsRegs(dlist, pkt);
                    psVifPktCloseGifPkt(pkt);
                }
                nj_cnk_vu1_strips_raw(sys, p, &nj_cnk_vu1_vtxsrc);
                break;
            case 65:
                dlist = psGsGetDisplayList();
                pkt = psDisplayListGetVifPkt(dlist);
                nj_cnk_strip_state(sys, 65, flags, sys->vtx_type);
                if (psDisplayListIsDirty(dlist)) {
                    psVifPktOpenGifPkt(pkt);
                    psDisplayListFlushGsRegs(dlist, pkt);
                    psVifPktCloseGifPkt(pkt);
                }
                nj_cnk_vu1_uv_strips_raw(sys, p, &nj_cnk_vu1_vtxsrc);
                break;
            }
            p += size;
        }
        head = *p++;
    }
}

/* For nj_cnk_vu1_plist_raw: type, stride, position/normal/colour arrays (34, 41, 35, 42). */
/* 0x001a8d80 */
static int nj_cnk_vu1_vlist_raw(int *p)
{
    NjSysView *sys = NJ_SYS_VIEW;
    NjVtxSrc *vtxsrc = &nj_cnk_vu1_vtxsrc;
    int head, type, size, n;

    sys->vtx_attr = 0;
    head = *p;
    while (head != 255) {
        type = head & 0xff;
        n = (unsigned int)p[1] >> 16;
        size = (unsigned int)head >> 16;
        vtxsrc->type = type;
        sys->vtx_type = type;
        switch (type) {
        case 34:
            vtxsrc->stride = 12;
            vtxsrc->pos = p + 2;
            vtxsrc->vtx = p + 2;
            vtxsrc->nrm = 0;
            vtxsrc->col = 0;
            break;
        case 41:
            vtxsrc->nrm = p + 5;
            sys->vtx_attr |= 1;
            vtxsrc->pos = p + 2;
            vtxsrc->vtx = p + 2;
            vtxsrc->stride = 24;
            vtxsrc->col = 0;
            break;
        case 35:
            vtxsrc->col = p + 5;
            sys->vtx_attr |= 2;
            vtxsrc->pos = p + 2;
            vtxsrc->vtx = p + 2;
            vtxsrc->stride = 16;
            vtxsrc->nrm = 0;
            break;
        case 42:
            vtxsrc->pos = p + 2;
            vtxsrc->vtx = p + 2;
            sys->vtx_attr |= 3;
            vtxsrc->nrm = p + 5;
            vtxsrc->col = p + 8;
            vtxsrc->stride = 28;
            break;
        default:
            vtxsrc->pos = 0;
            vtxsrc->stride = 0;
            vtxsrc->vtx = 0;
            vtxsrc->nrm = 0;
            vtxsrc->col = 0;
            break;
        }
        NJ_SYS_VIEW->nvtx += n;
        p += size + 1;
        head = *p;
    }
    return 0;
}

/* No clipping; adds to the vertex and VU1 timings. Returns 0. */
/* 0x001a8f00 */
int nj_cnk_vu1_draw_raw(NjCnkModel *model)
{
    NjSysView *sys = NJ_SYS_VIEW;
    PsVifPkt *pkt;
    int ticks;

    ulGxSetMatrix(0, ulMatrixCur);
    if (model->vlist) {
        int start = psTimerGetCount();
        nj_cnk_vu1_vlist_raw(model->vlist);
        sys->vlist_ticks += psTimerDiff(psTimerGetCount(), start);
    }
    if (nj_cnk_vu1_vtxsrc.pos == 0)
        return 0;
    if (model->plist == 0)
        return 0;
    ticks = psTimerGetCount();
    psGsOpenRefPkt();
    pkt = psGsGetRefPkt();
    psVifPktAddWord(pkt, 0x01000101);
    psVifPktAddWord(pkt, 0x6c040059);
    psVifPktAddWords(pkt, (char *)sys + 0x140, 16);
    nj_cnk_vu1_plist_raw(model->plist);
    psGsCloseRefPkt();
    sys->vu1_ticks += psTimerDiff(psTimerGetCount(), ticks);
    return 0;
}

/* From the vertex buffer, clipped (cache chunks as nj_cnk_gs_plist_clip); white or shaded. */
/* 0x001a9028 */
static void nj_cnk_vu1_plist_clip(short *p)
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
                nj_cnk_vu1_plist_clip(sys->cache[flags >> 8]);
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
            nj_cnk_vu1_set_material(sys, flags, type, p);
            p += len;
        } else {
            size = *p++;
            switch (type) {
            case 64:
                dlist = psGsGetDisplayList();
                pkt = psDisplayListGetVifPkt(dlist);
                nj_cnk_strip_state(sys, 64, flags, sys->vtx_type);
                if (psDisplayListIsDirty(dlist)) {
                    psVifPktOpenGifPkt(pkt);
                    psDisplayListFlushGsRegs(dlist, pkt);
                    psVifPktCloseGifPkt(pkt);
                }
                if (nj_cnk_vu1_is_lit(sys, flags)) {
                    if (head & 0x4000)
                        nj_cnk_vu1_strips_unlit_env(sys, p);
                    else
                        nj_cnk_vu1_strips_unlit(sys, p);
                } else {
                    if (head & 0x4000)
                        nj_cnk_vu1_strips_env(sys, p);
                    else
                        nj_cnk_vu1_strips_lit(sys, p);
                }
                break;
            case 65:
                dlist = psGsGetDisplayList();
                pkt = psDisplayListGetVifPkt(dlist);
                nj_cnk_strip_state(sys, 65, flags, sys->vtx_type);
                if (psDisplayListIsDirty(dlist)) {
                    psVifPktOpenGifPkt(pkt);
                    psDisplayListFlushGsRegs(dlist, pkt);
                    psVifPktCloseGifPkt(pkt);
                }
                if (nj_cnk_vu1_is_lit(sys, flags))
                    nj_cnk_vu1_uv_strips_unlit(sys, p);
                else
                    nj_cnk_vu1_uv_strips_lit(sys, p);
                break;
            }
            p += size;
        }
        head = *p++;
    }
}

/* VU1-path nj_cnk_gs_vblock_clip. */
/* 0x001a92f0 */
static int nj_cnk_vu1_vblock_clip(NjSysView *sys, int *chunk)
{
    int n;
    char *dst;
    int *src;

    src = chunk + 2;
    n = (unsigned int)chunk[1] >> 16;
    dst = sys->vbuf + (chunk[1] & 0xffff) * 64;
    switch (chunk[0] & 0x7f00) {
    case 0:
        nj_vu0_vu1_skin_lit_first(sys, src, dst, n);
        break;
    case 0x100:
        nj_vu0_vu1_skin_lit_mid(sys, src, dst, n);
        break;
    case 0x200:
        return nj_vu0_vu1_skin_lit_env_last_clip(sys, src, dst, n);
    }
    return n;
}

/*
 * Returns list clip codes: above bit 8, planes some vertex is outside; low six bits, planes all
 * are outside (cullable). Skinned/unknown chunks cross every plane, never culled.
 */
/* 0x001a93b0 */
int nj_cnk_vu1_vlist_clip(int *p)
{
    NjSysView *sys = NJ_SYS_VIEW;
    int head, type, size, n, codes, or_codes, and_codes;
    char *dst;

    or_codes = 0;
    and_codes = 63;
    head = *p;
    while (head != 255) {
        type = head & 0xff;
        n = (unsigned int)p[1] >> 16;
        dst = sys->vbuf + (p[1] & 0xffff) * 64;
        size = (unsigned int)head >> 16;
        sys->vtx_type = type;
        switch (type) {
        case 34:
            codes = nj_vu0_vu1_pos_clip(sys, p + 2, dst, n);
            break;
        case 41:
            codes = nj_vu0_vu1_lit_env_clip(sys, p + 2, dst, n);
            break;
        case 35:
            codes = nj_vu0_vu1_col_clip(sys, p + 2, dst, n);
            break;
        case 42:
            codes = nj_vu0_vu1_env_clip(sys, p + 2, dst, n);
            break;
        case 44:
            nj_cnk_vu1_vblock_clip(sys, p);
            /* fall through */
        default:
            codes = 0x3f00;
            break;
        }
        NJ_SYS_VIEW->nvtx += n;
        p += size + 1;
        or_codes |= (codes >> 8) & 0xff;
        and_codes &= codes;
        head = *p;
    }
    return (or_codes << 8) | and_codes;
}

/* No clipping; own material reader; strip writers pick white or shaded per strip. */
/* 0x001a9520 */
static void nj_cnk_vu1_plist_noclip(short *p)
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
                nj_cnk_vu1_plist_noclip(sys->cache[flags >> 8]);
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
                nj_cnk_strip_state(sys, 64, flags, sys->vtx_type);
                if (psDisplayListIsDirty(dlist)) {
                    psVifPktOpenGifPkt(pkt);
                    psDisplayListFlushGsRegs(dlist, pkt);
                    psVifPktCloseGifPkt(pkt);
                }
                if (head & 0x4000)
                    nj_cnk_vu1_strips_env_noclip(sys, p);
                else
                    nj_cnk_vu1_strips_noclip(sys, p);
                break;
            case 65:
                dlist = psGsGetDisplayList();
                pkt = psDisplayListGetVifPkt(dlist);
                nj_cnk_strip_state(sys, 65, flags, sys->vtx_type);
                if (psDisplayListIsDirty(dlist)) {
                    psVifPktOpenGifPkt(pkt);
                    psDisplayListFlushGsRegs(dlist, pkt);
                    psVifPktCloseGifPkt(pkt);
                }
                nj_cnk_vu1_uv_strips_noclip(sys, p);
                break;
            }
            p += size;
        }
        head = *p++;
    }
}

/* No clip codes. */
/* 0x001a9798 */
static int nj_cnk_vu1_vblock_noclip(NjSysView *sys, int *chunk)
{
    int n;
    char *dst;
    int *src;

    src = chunk + 2;
    n = (unsigned int)chunk[1] >> 16;
    dst = sys->vbuf + (chunk[1] & 0xffff) * 64;
    switch (chunk[0] & 0x7f00) {
    case 0:
        nj_vu0_vu1_skin_lit_first(sys, src, dst, n);
        break;
    case 0x100:
        nj_vu0_vu1_skin_lit_mid(sys, src, dst, n);
        break;
    case 0x200:
        return nj_vu0_vu1_skin_lit_env_last(sys, src, dst, n);
    }
    return n;
}

/* No clip codes; records normals/colours present. Returns 0. */
/* 0x001a9858 */
int nj_cnk_vu1_vlist_noclip(int *p)
{
    NjSysView *sys = NJ_SYS_VIEW;
    int head, type, size, n;
    char *dst;

    sys->vtx_attr = 0;
    head = *p;
    while (head != 255) {
        type = head & 0xff;
        n = (unsigned int)p[1] >> 16;
        dst = sys->vbuf + (p[1] & 0xffff) * 64;
        size = (unsigned int)head >> 16;
        sys->vtx_type = type;
        switch (type) {
        case 34:
            nj_vu0_vu1_pos(sys, p + 2, dst, n);
            break;
        case 41:
            nj_vu0_vu1_lit_env(sys, p + 2, dst, n);
            sys->vtx_attr |= 1;
            break;
        case 35:
            nj_vu0_vu1_col(sys, p + 2, dst, n);
            sys->vtx_attr |= 2;
            break;
        case 42:
            nj_vu0_vu1_env(sys, p + 2, dst, n);
            sys->vtx_attr |= 3;
            break;
        case 44:
            nj_cnk_vu1_vblock_noclip(sys, p);
            sys->vtx_attr |= 1;
            break;
        }
        NJ_SYS_VIEW->nvtx += n;
        p += size + 1;
        head = *p;
    }
    return 0;
}

/* Vertex then polygon list, timed; the clip codes are not used to cull. Returns 0. */
/* 0x001a99b0 */
int nj_cnk_vu1_draw_clip(NjCnkModel *model)
{
    NjSysView *sys;
    int ticks;

    ulGxSetMatrix(0, ulMatrixCur);
    if (model->vlist) {
        int start = psTimerGetCount();
        nj_cnk_vu1_vlist_clip(model->vlist);
        start = psTimerDiff(psTimerGetCount(), start);
        sys = NJ_SYS_VIEW;
        sys->vlist_ticks += start;
    }
    if (model->plist) {
        ticks = psTimerGetCount();
        psGsOpenRefPkt();
        nj_vu1_upload_giftags();
        nj_cnk_vu1_plist_clip(model->plist);
        psGsCloseRefPkt();
        ticks = psTimerDiff(psTimerGetCount(), ticks);
        sys = NJ_SYS_VIEW;
        sys->vu1_ticks += ticks;
    }
    return 0;
}

/* No clipping. */
/* 0x001a9a90 */
int nj_cnk_vu1_draw_noclip(NjCnkModel *model)
{
    NjSysView *sys;
    int ticks;

    ulGxSetMatrix(0, ulMatrixCur);
    if (model->vlist) {
        int start = psTimerGetCount();
        nj_cnk_vu1_vlist_noclip(model->vlist);
        start = psTimerDiff(psTimerGetCount(), start);
        sys = NJ_SYS_VIEW;
        sys->vlist_ticks += start;
    }
    if (model->plist) {
        ticks = psTimerGetCount();
        psGsOpenRefPkt();
        nj_vu1_upload_giftags();
        nj_cnk_vu1_plist_noclip(model->plist);
        psGsCloseRefPkt();
        ticks = psTimerDiff(psTimerGetCount(), ticks);
        sys = NJ_SYS_VIEW;
        sys->vu1_ticks += ticks;
    }
    return 0;
}

/* Clipping unless the no-clip flag; then raw vertices if the direct flag too. */
/* 0x001a9b70 */
int nj_cnk_vu1_draw(NjCnkModel *model)
{
    if (ulGxGetDrawFlags() & 1) {
        if (ulGxGetDrawFlags() & 2)
            return nj_cnk_vu1_draw_raw(model);
        return nj_cnk_vu1_draw_noclip(model);
    }
    return nj_cnk_vu1_draw_clip(model);
}

/* 0x001a9be0 */
void nj_object_draw_vu1(NjObject *obj)
{
    nj_object_walk(obj, nj_cnk_vu1_draw);
}

/*
 * Fuller nj_vu1_xyz_block: optional GIF tag, header, position, normal, colour, UV arrays (each
 * start returned), then program continue. Returns words used.
 */
/* 0x001a9bf0 */
static int nj_vu1_vertex_block(PsVifPkt *pkt, int n, int **gif_tag, int **hdr, int **xyz, int **nrm, int **col, int **st)
{
    int *p;
    int k;

    p = (int *)pkt->cur;
    k = 0;
    if (gif_tag) {
        *p++ = 0x6c018000;
        *gif_tag = p;
        p += 4;
        k = 5;
    }
    if (hdr) {
        *p++ = 0x6c018002;
        *hdr = p;
        p += 4;
        k += 5;
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
    if (nrm) {
        *p++ = (n << 16) | 0x68008005;
        *nrm = p;
        {
            int nwords = k + 1;
            k = nwords + n * 3;
        }
        p += n * 3;
    }
    if (col) {
        *p++ = (n << 16) | 0x6c008007;
        *col = p;
        p += n * 4;
        {
            int nwords = k + 1;
            k = nwords + n * 4;
        }
    }
    if (st) {
        *p++ = (n << 16) | 0x64008006;
        *st = p;
        p += n * 2;
        {
            int nwords = k + 1;
            k = nwords + n * 2;
        }
    }
    *p = 0x17000000;
    return k + 1;
}

/* Entries (vertex index, u, v): type 34 positions, UV in 1/256. */
/* 0x001a9d30 */
static void nj_cnk_vu1_copy_uv(NjSysView *sys, short *idx, int n, int *vtx, int *xyz, int *nrm, int *col, float *st)
{
    int *src;
    int i;
    float k;

    k = 1.0f / 256.0f;
    for (i = 0; i < n; i++) {
        src = vtx + idx[0] * 3;
        xyz[0] = src[0];
        xyz[1] = src[1];
        xyz[2] = src[2];
        xyz += 3;
        st[0] = (float)idx[1] * k;
        st[1] = (float)idx[2] * k;
        idx += 3;
        st += 2;
    }
}

/* Type 41. */
/* 0x001a9db0 */
static void nj_cnk_vu1_copy_uv_nrm(NjSysView *sys, short *idx, int n, int *vtx, int *xyz, int *nrm, int *col, float *st)
{
    int *src;
    int i;
    float k;

    k = 1.0f / 256.0f;
    for (i = 0; i < n; i++) {
        src = vtx + idx[0] * 6;
        xyz[0] = src[0];
        xyz[1] = src[1];
        xyz[2] = src[2];
        xyz += 3;
        nrm[0] = src[3];
        nrm[1] = src[4];
        nrm[2] = src[5];
        nrm += 3;
        st[0] = (float)idx[1] * k;
        st[1] = (float)idx[2] * k;
        idx += 3;
        st += 2;
    }
}

/* Type 35; colours scaled by nj_cnk_col_scale. */
/* 0x001a9e50 */
static void nj_cnk_vu1_copy_uv_col(NjSysView *sys, short *idx, int n, int *vtx, int *xyz, int *nrm, float *col, float *st)
{
    int *src;
    short *entry;
    int i, rgba;
    float k, scale;

    scale = nj_cnk_col_scale;
    do { } while (0);
    k = 1.0f / 256.0f;
    entry = idx;
    for (i = 0; i < n; i++) {
        src = vtx + entry[0] * 4;
        xyz[0] = src[0];
        xyz[1] = src[1];
        xyz[2] = src[2];
        xyz += 3;
        rgba = src[3];
        col[0] = (float)(rgba & 0xff) * scale;
        col[1] = (float)((rgba >> 8) & 0xff) * scale;
        col[2] = (float)((rgba >> 16) & 0xff) * scale;
        col[3] = (float)(int)((unsigned int)rgba >> 24) * scale;
        col += 4;
        st[0] = (float)entry[1] * k;
        st[1] = (float)entry[2] * k;
        entry += 3;
        st += 2;
    }
}

/* Type 42; colours to 0..1. */
/* 0x001a9f30 */
static void nj_cnk_vu1_copy_uv_nrm_col(NjSysView *sys, short *idx, int n, int *vtx, int *xyz, int *nrm, float *col, float *st)
{
    int *src;
    int i, rgba;
    float k, scale;

    scale = 1.0f / 255.0f;
    k = 1.0f / 256.0f;
    for (i = 0; i < n; i++) {
        src = vtx + idx[0] * 7;
        xyz[0] = src[0];
        xyz[1] = src[1];
        xyz[2] = src[2];
        xyz += 3;
        nrm[0] = src[3];
        nrm[1] = src[4];
        nrm[2] = src[5];
        nrm += 3;
        rgba = src[6];
        col[0] = (float)(rgba & 0xff) * scale;
        col[1] = (float)((rgba >> 8) & 0xff) * scale;
        col[2] = (float)((rgba >> 16) & 0xff) * scale;
        col[3] = (float)(int)((unsigned int)rgba >> 24) * scale;
        col += 4;
        st[0] = (float)idx[1] * k;
        st[1] = (float)idx[2] * k;
        idx += 3;
        st += 2;
    }
}

/* From model vertices: copier by type, GIF tag, params; strips up to 50 (longer skipped). */
/* 0x001aa038 */
static void nj_cnk_vu1_uv_strips_raw(NjSysView *sys, short *p, NjVtxSrc *vtxsrc)
{
    float tag[4];
    int pad[4];
    int *nrm, *col;
    int *gif_tag;
    int *hdr;
    int *xyz, *st;
    int **nrmp, **colp;
    NjStripFn fn;
    PsVifPkt *pkt;
    int i, n, len, k;
    int *gt;

    n = (*p++ << 2) >> 2 & 0x3fff;
    switch (vtxsrc->type) {
    case 34:
        colp = 0;
        fn = (NjStripFn)nj_cnk_vu1_copy_uv;
        nrmp = 0;
        break;
    case 41:
        nrmp = &nrm;
        fn = (NjStripFn)nj_cnk_vu1_copy_uv_nrm;
        colp = 0;
        break;
    case 35:
        nrmp = 0;
        fn = (NjStripFn)nj_cnk_vu1_copy_uv_col;
        colp = &col;
        break;
    case 42:
        nrmp = &nrm;
        fn = (NjStripFn)nj_cnk_vu1_copy_uv_nrm_col;
        colp = &col;
        break;
    default:
        colp = 0;
        nrmp = 0;
        break;
    }
    sys->strip_prim = sys->prim | 0x14;
    gt = sys->vu1_tex_tag;
    pkt = psGsGetRefPkt();
    psVifPktAddWord(pkt, 0x01000101);
    psVifPktAddWord(pkt, 0x6c010055);
    psVifPktAddWords(pkt, gt, 4);
    psVifPktAddWord(pkt, 0x6c010040);
    psVifPktAddWords(pkt, &sys->vu1_param, 4);
    psVifPktAddWord(pkt, 0x1500001a);
    psVifPktAddWord(pkt, 0x01000104);
    tag[0] = -1.0f;
    for (i = 0; i < n; i++) {
        len = *p++;
        if (len > 0) {
            tag[2] = 1.0f;
        } else {
            tag[2] = -1.0f;
            len = -len;
        }
        if (len > 50) {
            p += len * 3;
            continue;
        }
        k = nj_vu1_vertex_block(pkt, len, &gif_tag, &hdr, &xyz, nrmp, colp, &st);
        hdr[0] = *(int *)&tag[0];
        hdr[1] = len;
        hdr[2] = *(int *)&tag[2];
        fn(sys, p, len, vtxsrc->pos, xyz, nrm, col, st);
        p += len * 3;
        pkt->nwords += k;
        *(char **)&pkt->cur += k * 4;
        NJ_SYS_VIEW->ntri += len - 2;
    }
    psVifPktAddWord(pkt, 0x01000101);
    psVifPktAddWord(pkt, 0x10000000);
}

/* Type 34 positions. */
/* 0x001aa318 */
static void nj_cnk_vu1_copy(NjSysView *sys, short *idx, int n, int *vtx, int *xyz, int *nrm, int *col, float *st)
{
    int *src;
    int i;

    for (i = 0; i < n; i++) {
        src = vtx + idx[0] * 3;
        *xyz++ = src[0];
        *xyz++ = src[1];
        *xyz++ = src[2];
        idx++;
    }
}

/* Type 41. */
/* 0x001aa370 */
static void nj_cnk_vu1_copy_nrm(NjSysView *sys, short *idx, int n, int *vtx, int *xyz, int *nrm, int *col, float *st)
{
    int *src;
    int i;

    for (i = 0; i < n; i++) {
        src = vtx + idx[0] * 6;
        xyz[0] = src[0];
        xyz[1] = src[1];
        xyz[2] = src[2];
        xyz += 3;
        nrm[0] = src[3];
        nrm[1] = src[4];
        nrm[2] = src[5];
        nrm += 3;
        idx++;
    }
}

/* Type 35, scaled by nj_cnk_col_scale2. */
/* 0x001aa3d8 */
static void nj_cnk_vu1_copy_col(NjSysView *sys, short *idx, int n, int *vtx, int *xyz, int *nrm, float *col, float *st)
{
    int *src;
    short *entry;
    int i, rgba;
    float scale;

    scale = nj_cnk_col_scale2;
    do { } while (0);
    entry = idx;
    for (i = 0; i < n; i++) {
        src = vtx + entry[0] * 4;
        xyz[0] = src[0];
        xyz[1] = src[1];
        xyz[2] = src[2];
        xyz += 3;
        rgba = src[3];
        col[0] = (float)(rgba & 0xff) * scale;
        col[1] = (float)((rgba >> 8) & 0xff) * scale;
        col[2] = (float)((rgba >> 16) & 0xff) * scale;
        col[3] = (float)(int)((unsigned int)rgba >> 24) * scale;
        col += 4;
        entry++;
    }
}

/* Without UV; env-mapped strips still use the textured GIF tag. */
/* 0x001aa488 */
static void nj_cnk_vu1_strips_raw(NjSysView *sys, short *p, NjVtxSrc *vtxsrc)
{
    float tag[4];
    int pad[4];
    int *nrm, *col;
    int *gif_tag;
    int *hdr;
    int *xyz;
    int **nrmp, **colp;
    NjStripFn fn;
    PsVifPkt *pkt;
    int i, n, len, k;
    float pad_float;
    int *gt;
    int prim;

    n = (*p++ << 2) >> 2 & 0x3fff;
    switch (vtxsrc->type) {
    case 34:
        colp = 0;
        fn = (NjStripFn)nj_cnk_vu1_copy;
        nrmp = 0;
        break;
    case 41:
        nrmp = &nrm;
        fn = (NjStripFn)nj_cnk_vu1_copy_nrm;
        colp = 0;
        break;
    case 35:
        nrmp = 0;
        fn = (NjStripFn)nj_cnk_vu1_copy_col;
        colp = &col;
        break;
    case 42:
        nrmp = &nrm;
        fn = (NjStripFn)nj_vu0_copy_nrm_col;
        colp = &col;
        break;
    default:
        colp = 0;
        nrmp = 0;
        break;
    }
    if (sys->strip_flags & 0x4000) {
        prim = sys->prim | 0x14;
        gt = sys->vu1_tex_tag;
    } else {
        prim = sys->prim | 4;
        gt = sys->vu1_tag;
    }
    sys->strip_prim = prim;
    pkt = psGsGetRefPkt();
    psVifPktAddWord(pkt, 0x6c010055);
    psVifPktAddWords(pkt, gt, 4);
    psVifPktAddWord(pkt, 0x6c010040);
    psVifPktAddWords(pkt, &sys->vu1_param, 4);
    psVifPktAddWord(pkt, 0x1500001a);
    psVifPktAddWord(pkt, 0x01000104);
    tag[0] = -1.0f;
    for (i = 0; i < n; i++) {
        len = *p++;
        if (len > 0) {
            tag[2] = 1.0f;
        } else {
            tag[2] = -1.0f;
            len = -len;
        }
        if (len > 50) {
            p += len;
            continue;
        }
        k = nj_vu1_vertex_block(pkt, len, &gif_tag, &hdr, &xyz, nrmp, colp, 0);
        hdr[0] = *(int *)&tag[0];
        hdr[1] = len;
        hdr[2] = *(int *)&tag[2];
        fn(sys, p, len, vtxsrc->pos, xyz, nrm, col, 0);
        pkt->nwords += k;
        *(char **)&pkt->cur += k * 4;
        p += len;
        NJ_SYS_VIEW->ntri += len - 2;
    }
    psVifPktAddWord(pkt, 0x01000101);
    psVifPktAddWord(pkt, 0x10000000);
}

/* As nj_vu1_vertex_block, quadword-aligned (headers own quadword, 4-word positions). */
/* 0x001aa768 */
static int nj_vu1_strip_block_b(PsVifPkt *pkt, int n, int **gif_tag, int **hdr, int **xyz, int **col, int **st)
{
    int *p;
    int k, i, pad;

    p = (int *)pkt->cur;
    k = 0;
    pad = 3 - (pkt->nwords & 3);
    for (i = 0; i < pad; i++) {
        *p++ = 0;
        k++;
    }
    *p++ = 0x6c018000;
    *gif_tag = p;
    p += 4;
    p[3] = 0x6c018002;
    p[2] = 0;
    p[1] = 0;
    p[0] = 0;
    *hdr = p + 4;
    p += 8;
    p[3] = (n << 16) | 0x6c008004;
    p[2] = 0;
    p[1] = 0;
    p[0] = 0;
    *xyz = p + 4;
    p += 4 + n * 4;
    {
        int nwords = k + 17;
        k = nwords + n * 4;
    }
    if (col) {
        *col = p + 4;
        p[3] = (n << 16) | 0x6c008007;
        {
            int nwords = k + 4;
            k = nwords + n * 4;
        }
        p[2] = 0;
        p[1] = 0;
        p[0] = 0;
        p += 4 + n * 4;
    }
    if (st) {
        *st = p + 4;
        p[3] = (n << 16) | 0x64008006;
        p[2] = 0;
        p[1] = 0;
        p[0] = 0;
        p += 4 + n * 2;
        {
            int nwords = k + 4;
            k = nwords + n * 2;
        }
    }
    *p = 0x17000000;
    return k + 1;
}

/* As nj_vu1_vertex_block: header and positions always, colours and UV optional. */
/* 0x001aa8c0 */
static int nj_vu1_strip_block(PsVifPkt *pkt, int n, int **gif_tag, int **hdr, int **xyz, int **col, int **st)
{
    int *p;
    int k;

    p = (int *)pkt->cur;
    *p++ = 0x6c018002;
    *hdr = p;
    p += 4;
    *p++ = (n << 16) | 0x68008004;
    *xyz = p;
    k = n * 3 + 6;
    p += n * 3;
    if (col) {
        *p++ = (n << 16) | 0x6c008007;
        *col = p;
        p += n * 4;
        {
            int nwords = k + 1;
            k = nwords + n * 4;
        }
    }
    if (st) {
        *p++ = (n << 16) | 0x64008006;
        *st = p;
        p += n * 2;
        {
            int nwords = k + 1;
            k = nwords + n * 2;
        }
    }
    *p = 0x17000000;
    return k + 1;
}

/*
 * Env-mapped white strip with clipping: unclipped run to VU1 (header: mode, length, dir,
 * primitive); next triangle clipped on the EE as a fan; advance one, flip winding. Counts clips.
 */
/* 0x001aa980 */
static void nj_cnk_vu1_strip_unlit_env(NjSysView *sys, short *p, int n, int *vtx, float dir)
{
    int pad_top[4];
    NjClipOut out;
    NjQword in[4];
    NjQword a[4];
    NjQword b[4];
    int pad_mid[4];
    int *gif_tag;
    int *hdr;
    int *xyz, *col, *st;
    PsVifPkt *pkt;
    int strip_prim, fan_prim;
    int k, run, nclip;

    pkt = psGsGetRefPkt();
    strip_prim = sys->prim | 0x14;
    fan_prim = sys->prim | 0x15;
    while (n >= 3) {
        k = nj_vu1_strip_block(pkt, n, &gif_tag, &hdr, &xyz, &col, &st);
        run = nj_vu0_gather_white_st_clip(sys, p, n, vtx, xyz, col, st);
        if (run >= 3) {
            hdr[1] = run | 0x8000;
            hdr[3] = strip_prim;
            *(float *)&hdr[0] = -1.0f;
            *(char **)&pkt->cur += k * 4;
            pkt->nwords += k;
            *(float *)&hdr[2] = dir;
            p += run - 2;
            n += 2 - run;
            if (run & 1)
                dir = -dir;
        }
        if (n < 3)
            break;
        nj_vu0_copy_white(sys, p, 3, vtx, in);
        nclip = ulGxClipTriangle(&out, in, a, b);
        if (nclip > 0) {
            k = nj_vu1_strip_block(pkt, nclip, &gif_tag, &hdr, &xyz, &col, &st);
            *(float *)&hdr[0] = 1.0f;
            hdr[1] = nclip | 0x8000;
            *(float *)&hdr[2] = dir;
            hdr[3] = fan_prim;
            vu0_clip_gather_st(sys, out.out, nclip, xyz, col, st);
            pkt->nwords += k;
            *(char **)&pkt->cur += k * 4;
            NJ_SYS_VIEW->nstrip += 1;
        }
        n--;
        p++;
        dir = -dir;
    }
}

/* Textured GIF tag, program start, strips up to 50 (negative length = reversed); counts. */
/* 0x001aabf0 */
static void nj_cnk_vu1_strips_unlit_env(NjSysView *sys, short *p)
{
    PsVifPkt *pkt;
    int n, i, len;
    float dir;
    unsigned long tag;

    sys->batch_tag[1] = 0x412;
    tag = NJ_GIFTAG(0, 0, 1, (sys->prim & ~7) | 0x14, 0, 3);
    tag |= 0x8000;
    sys->batch_tag[0] = tag;
    n = (*p++ << 2) >> 2 & 0x3fff;
    pkt = psGsGetRefPkt();
    psVifPktAddUnpack(pkt, 0x6c010055, sys->vu1_tex_tag, 1);
    psVifPktAddWord(pkt, 0x15000008);
    psVifPktAddWord(pkt, 0x01000104);
    for (i = 0; i < n; i++) {
        len = *p++;
        dir = 1.0f;
        if (len <= 0) {
            dir = -1.0f;
            len = -len;
        }
        if (len <= 50)
            nj_cnk_vu1_strip_unlit_env(sys, p, len, sys->vbuf, dir);
        p += len;
        NJ_SYS_VIEW->ntri += len - 2;
    }
    psVifPktAddWord(pkt, 0x01000101);
    psVifPktAddWord(pkt, 0x10000000);
}

/* Textured strip (vertex index, u, v). */
/* 0x001aad58 */
static void nj_cnk_vu1_uv_strip_unlit(NjSysView *sys, NjCnkIdx *p, int n, int *vtx, float dir)
{
    int pad_top[4];
    NjClipOut out;
    NjQword in[4];
    NjQword a[4];
    NjQword b[4];
    int pad_mid[4];
    int *gif_tag;
    int *hdr;
    int *xyz, *col, *st;
    PsVifPkt *pkt;
    int strip_prim, fan_prim;
    int k, run, nclip, skip;

    pkt = psGsGetRefPkt();
    strip_prim = sys->prim | 0x14;
    fan_prim = sys->prim | 0x15;
    while (n >= 3) {
        k = nj_vu1_strip_block(pkt, n, &gif_tag, &hdr, &xyz, &col, &st);
        run = nj_vu0_gather_white_uv_clip(sys, p, n, vtx, xyz, col, st);
        if (run >= 3) {
            hdr[1] = run | 0x8000;
            hdr[3] = strip_prim;
            *(float *)&hdr[0] = -1.0f;
            *(char **)&pkt->cur += k * 4;
            pkt->nwords += k;
            *(float *)&hdr[2] = dir;
            skip = run - 2;
            p += skip;
            n += 2 - run;
            if (skip & 1)
                dir = -dir;
        }
        if (n < 3)
            break;
        nj_vu0_copy_white_uv(sys, p, 3, vtx, in);
        nclip = ulGxClipTriangle(&out, in, a, b);
        if (nclip > 0) {
            k = nj_vu1_strip_block(pkt, nclip, &gif_tag, &hdr, &xyz, &col, &st);
            *(float *)&hdr[0] = 1.0f;
            hdr[1] = nclip | 0x8000;
            *(float *)&hdr[2] = dir;
            hdr[3] = fan_prim;
            vu0_clip_gather_st(sys, out.out, nclip, xyz, col, st);
            pkt->nwords += k;
            *(char **)&pkt->cur += k * 4;
            NJ_SYS_VIEW->nstrip += 1;
        }
        n--;
        p++;
        dir = -dir;
    }
}

/* Textured strips via nj_cnk_vu1_uv_strip_unlit. */
/* 0x001aafd0 */
static void nj_cnk_vu1_uv_strips_unlit(NjSysView *sys, short *p)
{
    PsVifPkt *pkt;
    int n, i, len;
    float dir;

    n = (*p++ << 2) >> 2 & 0x3fff;
    pkt = psGsGetRefPkt();
    psVifPktAddUnpack(pkt, 0x6c010055, sys->vu1_tex_tag, 1);
    psVifPktAddWord(pkt, 0x15000008);
    psVifPktAddWord(pkt, 0x01000104);
    for (i = 0; i < n; i++) {
        len = *p++;
        dir = 1.0f;
        if (len <= 0) {
            dir = -1.0f;
            len = -len;
        }
        if (len <= 50)
            nj_cnk_vu1_uv_strip_unlit(sys, (NjCnkIdx *)p, len, sys->vbuf, dir);
        p += len * 3;
        NJ_SYS_VIEW->ntri += len - 2;
    }
    psVifPktAddWord(pkt, 0x01000101);
    psVifPktAddWord(pkt, 0x10000000);
}

/* Untextured strip. */
/* 0x001ab100 */
static void nj_cnk_vu1_strip_unlit(NjSysView *sys, short *p, int n, int *vtx, float dir)
{
    int pad_top[4];
    NjClipOut out;
    NjQword in[4];
    NjQword a[4];
    NjQword b[4];
    int pad_mid[4];
    int *gif_tag;
    int *hdr;
    int *xyz, *col;
    PsVifPkt *pkt;
    int strip_prim, fan_prim;
    int k, run, nclip;

    pkt = psGsGetRefPkt();
    strip_prim = sys->prim | 4;
    fan_prim = sys->prim | 5;
    while (n >= 3) {
        k = nj_vu1_strip_block(pkt, n, &gif_tag, &hdr, &xyz, &col, 0);
        run = nj_vu0_gather_white_clip(sys, p, n, vtx, xyz, col);
        if (run >= 3) {
            hdr[1] = run | 0x8000;
            hdr[3] = strip_prim;
            *(float *)&hdr[0] = -1.0f;
            *(char **)&pkt->cur += k * 4;
            pkt->nwords += k;
            *(float *)&hdr[2] = dir;
            p += run - 2;
            n += 2 - run;
            if (run & 1)
                dir = -dir;
        }
        if (n < 3)
            break;
        nj_vu0_copy_white_b(sys, p, 3, vtx, in);
        nclip = ulGxClipTriangle(&out, in, a, b);
        if (nclip > 0) {
            k = nj_vu1_strip_block(pkt, nclip, &gif_tag, &hdr, &xyz, &col, 0);
            *(float *)&hdr[0] = 1.0f;
            hdr[1] = nclip | 0x8000;
            *(float *)&hdr[2] = dir;
            hdr[3] = fan_prim;
            vu0_clip_gather(sys, out.out, nclip, xyz, col);
            pkt->nwords += k;
            *(char **)&pkt->cur += k * 4;
            NJ_SYS_VIEW->nstrip += 1;
        }
        n--;
        p++;
        dir = -dir;
    }
}

/* Untextured strips via nj_cnk_vu1_strip_unlit. */
/* 0x001ab360 */
static void nj_cnk_vu1_strips_unlit(NjSysView *sys, short *p)
{
    PsVifPkt *pkt;
    int n, i, len;
    float dir;

    n = (*p++ << 2) >> 2 & 0x3fff;
    pkt = psGsGetRefPkt();
    psVifPktAddUnpack(pkt, 0x6c010055, sys->vu1_tag, 1);
    psVifPktAddWord(pkt, 0x15000008);
    psVifPktAddWord(pkt, 0x01000104);
    for (i = 0; i < n; i++) {
        len = *p++;
        dir = 1.0f;
        if (len <= 0) {
            dir = -1.0f;
            len = -len;
        }
        if (len <= 50)
            nj_cnk_vu1_strip_unlit(sys, p, len, sys->vbuf, dir);
        p += len;
        NJ_SYS_VIEW->ntri += len - 2;
    }
    psVifPktAddWord(pkt, 0x01000101);
    psVifPktAddWord(pkt, 0x10000000);
}

/* Env-mapped shaded strips with clipping; nj_cnk_vu1_strip_env inlined per strip (up to 50). */
/* 0x001ab490 */
static void nj_cnk_vu1_strips_env(NjCnkCtx *ctx, short *p)
{
    NjCnkVu1NjVifPkt *dl;
    int i, n, cnt, hd;
    float dir;
    unsigned long tag;

    ctx->tag[1] = 0x412;
    tag = (((ctx->prim & ~7L) | 0x14) << 47) | 0xc001L << 46;
    tag |= 0x8000;
    ctx->tag[0] = tag;
    hd = *p++;
    cnt = hd & 0x3fff;
    dl = psGsGetRefPkt();
    psVifPktAddUnpack(dl, 0x6c010055, &ctx->vu1_tex_tag, 1);
    psVifPktAddWord(dl, 0x15000008);
    psVifPktAddWord(dl, 0x01000104);
    for (i = 0; i < cnt;) {
        n = *p++;
        if (n > 0) {
            dir = 1.0f;
        } else {
            n = -n;
            dir = -1.0f;
        }
        if (n < 51) {
            NjCnkVu1NjVifPkt *pk;
            void *x;
            int run, nclip, words, strip_prim, fan_prim, k;
            int gif_tag;
            NjVuHdr *hdr;
            char *xyz, *col, *st;
            int unused2[4];
            NjClipBuf cb;
            char tmp[3][64];
            int unused[4];
            short *cur;
            char *vbuf;
            float sg;

            k = n;
            cur = p;
            sg = dir;
            vbuf = ctx->vbuf;
            pk = psGsGetRefPkt();
            strip_prim = ctx->prim | 20;
            fan_prim = ctx->prim | 21;
            while (k >= 3) {
                words = nj_vu1_strip_block(pk, k, &gif_tag, &hdr, &xyz, &col, &st);
                run = nj_vu0_gather_st_clip(ctx, cur, k, vbuf, xyz, col, st);
                if (run >= 3) {
                    hdr->nloop = run | 0x8000;
                    hdr->prim = strip_prim;
                    hdr->mode = -1.0f;
                    pk->cur += words * 4;
                    pk->nwords += words;
                    hdr->sign = sg;
                    cur += run - 2;
                    k += 2 - run;
                    if (run & 1)
                        sg = -sg;
                }
                if (k < 3)
                    break;
                nclip = ulGxClipTriangle(&cb, vbuf + cur[0] * 64, vbuf + cur[1] * 64, vbuf + cur[2] * 64);
                if (nclip > 0) {
                    words = nj_vu1_strip_block(pk, nclip, &gif_tag, &hdr, &xyz, &col, &st);
                    hdr->mode = 1.0f;
                    hdr->nloop = nclip | 0x8000;
                    hdr->sign = sg;
                    hdr->prim = fan_prim;
                    x = *(void **)&cb.out;
                    vu0_clip_gather_st(ctx, x, nclip, xyz, col, st);
                    pk->nwords += words;
                    pk->cur += words * 4;
                    NJ_SYS_COUNT->nstrip++;
                }
                k--;
                cur++;
                sg = -sg;
            }
        }
        i++;
        p += n;
        NJ_SYS_COUNT->ntri += n - 2;
    }
    psVifPktAddWord(dl, 0x01000101);
    psVifPktAddWord(dl, 0x10000000);
}

/* Shaded textured strip: colours from the vertex buffer instead of white. */
/* 0x001ab860 */
static void nj_cnk_vu1_uv_strip_lit(NjCnkCtx *ctx, short *idx, int n, char *vbuf, float dir)
{
    NjCnkVu1NjVifPkt *pk;
    int run, nclip, words, strip_prim, fan_prim, x;
    int gif_tag;
    NjVuHdr *hdr;
    char *xyz, *col, *st;
    NjClipBuf cb;
    char tmp[3][64];
    int unused[4];

    pk = psGsGetRefPkt();
    strip_prim = ctx->prim | 20;
    fan_prim = ctx->prim | 21;
    while (n >= 3) {
        words = nj_vu1_strip_block(pk, n, &gif_tag, &hdr, &xyz, &col, &st);
        run = nj_vu0_gather_uv_clip(ctx, idx, n, vbuf, xyz, col, st);
        if (run >= 3) {
            hdr->nloop = run | 0x8000;
            hdr->prim = strip_prim;
            hdr->mode = -1.0f;
            pk->cur += words * 4;
            pk->nwords += words;
            hdr->sign = dir;
            idx += (run - 2) * 3;
            n += 2 - run;
            if ((run - 2) & 1)
                dir = -dir;
        }
        if (n < 3)
            break;
        nj_vu0_copy_uv(ctx, idx, 3, vbuf, tmp[0]);
        nclip = ulGxClipTriangle(&cb, tmp[0], tmp[1], tmp[2]);
        if (nclip > 0) {
            words = nj_vu1_strip_block(pk, nclip, &gif_tag, &hdr, &xyz, &col, &st);
            x = cb.out;
            hdr->mode = 1.0f;
            hdr->nloop = nclip | 0x8000;
            hdr->sign = dir;
            hdr->prim = fan_prim;
            vu0_clip_gather_st(ctx, x, nclip, xyz, col, st);
            pk->nwords += words;
            pk->cur += words * 4;
            NJ_SYS_COUNT->nstrip++;
        }
        n--;
        idx += 3;
        dir = -dir;
    }
}

/* Via nj_cnk_vu1_uv_strip_lit. */
/* 0x001abad8 */
static void nj_cnk_vu1_uv_strips_lit(NjCnkCtx *ctx, short *p)
{
    NjCnkVu1NjVifPkt *pk;
    int i, n, cnt, head;
    float dir;

    head = *p++;
    cnt = head & 0x3fff;
    pk = psGsGetRefPkt();
    psVifPktAddUnpack(pk, 0x6c010055, &ctx->vu1_tex_tag, 1);
    psVifPktAddWord(pk, 0x15000008);
    psVifPktAddWord(pk, 0x01000104);
    for (i = 0; i < cnt;) {
        n = *p++;
        if (n > 0) {
            dir = 1.0f;
        } else {
            dir = -1.0f;
            n = -n;
        }
        if (n < 51)
            nj_cnk_vu1_uv_strip_lit(ctx, p, n, ctx->vbuf, dir);
        i++;
        p += n * 3;
        NJ_SYS_COUNT->ntri += n - 2;
    }
    psVifPktAddWord(pk, 0x01000101);
    psVifPktAddWord(pk, 0x10000000);
}

/* Shaded untextured strip. */
/* 0x001abc08 */
static void nj_cnk_vu1_strip_lit(NjCnkCtx *ctx, short *idx, int n, char *vbuf, float dir)
{
    NjCnkVu1NjVifPkt *pk;
    int run, nclip, words, strip_prim, fan_prim, x;
    int gif_tag;
    NjVuHdr *hdr;
    char *xyz, *col;
    NjClipBuf cb;
    char tmp[3][64];
    int unused[4];

    pk = psGsGetRefPkt();
    strip_prim = ctx->prim | 4;
    fan_prim = ctx->prim | 5;
    while (n >= 3) {
        words = nj_vu1_strip_block(pk, n, &gif_tag, &hdr, &xyz, &col, 0);
        run = nj_vu0_gather_clip(ctx, idx, n, vbuf, xyz, col);
        if (run >= 3) {
            hdr->nloop = run | 0x8000;
            hdr->prim = strip_prim;
            hdr->mode = -1.0f;
            pk->cur += words * 4;
            pk->nwords += words;
            hdr->sign = dir;
            idx += run - 2;
            n += 2 - run;
            if (run & 1)
                dir = -dir;
        }
        if (n < 3)
            break;
        nj_vu0_copy(ctx, idx, 3, vbuf, tmp[0]);
        nclip = ulGxClipTriangle(&cb, tmp[0], tmp[1], tmp[2]);
        if (nclip > 0) {
            words = nj_vu1_strip_block(pk, nclip, &gif_tag, &hdr, &xyz, &col, 0);
            x = cb.out;
            hdr->mode = 1.0f;
            hdr->nloop = nclip | 0x8000;
            hdr->sign = dir;
            hdr->prim = fan_prim;
            vu0_clip_gather(ctx, x, nclip, xyz, col);
            pk->nwords += words;
            pk->cur += words * 4;
            NJ_SYS_COUNT->nstrip++;
        }
        n--;
        idx++;
        dir = -dir;
    }
}

/* Via nj_cnk_vu1_strip_lit. */
/* 0x001abe68 */
static void nj_cnk_vu1_strips_lit(NjCnkCtx *ctx, short *p)
{
    NjCnkVu1NjVifPkt *pk;
    int i, n, cnt, head;
    float dir;

    head = *p++;
    cnt = head & 0x3fff;
    pk = psGsGetRefPkt();
    psVifPktAddUnpack(pk, 0x6c010055, &ctx->vu1_tag, 1);
    psVifPktAddWord(pk, 0x15000008);
    psVifPktAddWord(pk, 0x01000104);
    for (i = 0; i < cnt;) {
        n = *p++;
        if (n > 0) {
            dir = 1.0f;
        } else {
            dir = -1.0f;
            n = -n;
        }
        if (n < 51)
            nj_cnk_vu1_strip_lit(ctx, p, n, ctx->vbuf, dir);
        i++;
        NJ_SYS_COUNT->ntri += n - 2;
        p += n;
    }
    psVifPktAddWord(pk, 0x01000101);
    psVifPktAddWord(pk, 0x10000000);
}

/* Env-mapped fogged strip straight to the GS: unclipped runs as GIF strips, crossings as fans. */
/* 0x001abf98 */
static void nj_cnk_ctx_strip_tex(NjCnkCtx *ctx, short *idx, int n, char *vbuf)
{
    NjCnkVu1NjVifPkt *pk;
    NjGifTagBits *tag;
    NjQword *v;
    short *cur;
    int run, k, nclip;
    NjClipBuf cb;
    char tmp[3][64];            /* unused here; the sibling strip writers convert into it */

    pk = psGsGetRefPkt();
    n = abs(n);
    cur = idx;
    while (n >= 3) {
        tag = (NjGifTagBits *)pk->cur;
        v = (NjQword *)(tag + 1);
        run = nj_vu0_emit_fog_clip(ctx, cur, n, vbuf, v);
        if (run >= 3) {
            *(NjQword *)tag = *(NjQword *)ctx->tag;
            tag->nloop = run;
            k = run * 3;
            pk->gif = tag;
            pk->cur = (char *)(v + k);
            pk->nq += 1 + k;
            n += 2 - run;
            cur += run - 2;
        }
        if (n < 3)
            break;
        nclip = ulGxClipTriangle(&cb, vbuf + cur[0] * 64, vbuf + cur[1] * 64, vbuf + cur[2] * 64);
        if (nclip > 0) {
            tag = psVifPktAddGifTag(pk, ctx->tag);
            tag->nloop = nclip;
            tag->prim = (tag->prim & 0x7f8) | 5;
            psVifPktAdvanceGif(pk, vu0_clip_emit_tex_fog(ctx, cb.out, nclip, (char *)(tag + 1)), nclip * 3);
        }
        n--;
        cur++;
    }
}

/* Textured Gouraud strip GIF tag first; counts triangles. */
/* 0x001ac188 */
static void nj_cnk_ctx_strips_tex(NjCnkCtx *ctx, short *p)
{
    int i, n, cnt, head, k;

    ctx->tag[1] = 0x412;
    ctx->tag[0] = (((ctx->prim & ~7L) | 0x14) << 47) | 0xc001L << 46;
    head = *p++;
    cnt = head & 0x3fff;
    for (i = 0; i < cnt;) {
        n = *p++;
        nj_cnk_ctx_strip_tex(ctx, p, n, ctx->vbuf);
        i++;
        k = abs(n);
        p += k;
        NJ_SYS_COUNT->ntri += k - 2;
    }
}

/* Textured strip (vertex index, u, v). */
/* 0x001ac268 */
static void nj_cnk_ctx_uv_strip(NjCnkCtx *ctx, short *idx, int n, char *vbuf)
{
    NjCnkVu1NjVifPkt *pk;
    NjGifTagBits *tag;
    NjQword *v;
    short *cur;
    int run, k, nclip;
    NjClipBuf cb;
    char tmp[3][64];

    pk = psGsGetRefPkt();
    n = abs(n);
    cur = idx;
    while (n >= 3) {
        tag = (NjGifTagBits *)pk->cur;
        v = (NjQword *)(tag + 1);
        run = nj_vu0_emit_fog_uv_clip(ctx, cur, n, vbuf, v);
        if (run >= 3) {
            *(NjQword *)tag = *(NjQword *)ctx->tag;
            tag->nloop = run;
            k = run * 3;
            pk->gif = tag;
            pk->cur = (char *)(v + k);
            pk->nq += 1 + k;
            n += 2 - run;
            cur += (run - 2) * 3;
        }
        if (n < 3)
            break;
        nj_vu0_copy_uv(ctx, cur, 3, vbuf, tmp[0]);
        nclip = ulGxClipTriangle(&cb, tmp[0], tmp[1], tmp[2]);
        if (nclip > 0) {
            tag = psVifPktAddGifTag(pk, ctx->tag);
            tag->nloop = nclip;
            tag->prim = (tag->prim & 0x7f8) | 5;
            psVifPktAdvanceGif(pk, vu0_clip_emit_tex_fog(ctx, cb.out, nclip, (char *)(tag + 1)), nclip * 3);
        }
        n--;
        cur += 3;
    }
}

/* Via nj_cnk_ctx_uv_strip. */
/* 0x001ac468 */
static void nj_cnk_ctx_uv_strips(NjCnkCtx *ctx, short *p)
{
    int i, n, cnt, head, k;

    ctx->tag[1] = 0x412;
    ctx->tag[0] = (((ctx->prim & ~7L) | 0x14) << 47) | 0xc001L << 46;
    head = *p++;
    cnt = head & 0x3fff;
    for (i = 0; i < cnt;) {
        n = *p++;
        nj_cnk_ctx_uv_strip(ctx, p, n, ctx->vbuf);
        i++;
        k = abs(n);
        p += k * 3;
        NJ_SYS_COUNT->ntri += k - 2;
    }
}

/* Untextured strip with vertex colours. */
/* 0x001ac550 */
static void nj_cnk_ctx_strip(NjCnkCtx *ctx, short *idx, int n, char *vbuf)
{
    NjCnkVu1NjVifPkt *pk;
    NjGifTagBits *tag;
    NjQword *v;
    short *cur;
    int run, k, nclip;
    NjClipBuf cb;
    char tmp[3][64];

    pk = psGsGetRefPkt();
    n = abs(n);
    cur = idx;
    while (n >= 3) {
        tag = (NjGifTagBits *)pk->cur;
        v = (NjQword *)(tag + 1);
        run = nj_vu0_emit_fog_col_clip(ctx, cur, n, vbuf, v);
        if (run >= 3) {
            *(NjQword *)tag = *(NjQword *)ctx->tag;
            tag->nloop = run;
            k = run * 3;
            pk->gif = tag;
            pk->cur = (char *)(v + k);
            pk->nq += 1 + k;
            n += 2 - run;
            cur += run - 2;
        }
        if (n < 3)
            break;
        nj_vu0_copy(ctx, cur, 3, vbuf, tmp[0]);
        nclip = ulGxClipTriangle(&cb, tmp[0], tmp[1], tmp[2]);
        if (nclip > 0) {
            tag = psVifPktAddGifTag(pk, ctx->tag);
            tag->nloop = nclip;
            tag->prim = (tag->prim & 0x7f8) | 5;
            psVifPktAdvanceGif(pk, vu0_clip_emit_fog(ctx, cb.out, nclip, (char *)(tag + 1)), nclip * 3);
        }
        n--;
        cur++;
    }
}

/* Via nj_cnk_ctx_strip. */
/* 0x001ac748 */
static void nj_cnk_ctx_strips(NjCnkCtx *ctx, short *p)
{
    int i, n, cnt, head, k;

    ctx->tag[1] = 0x412;
    ctx->tag[0] = (((ctx->prim & ~7L) | 4) << 47) | 0xc001L << 46;
    head = *p++;
    cnt = head & 0x3fff;
    for (i = 0; i < cnt;) {
        n = *p++;
        nj_cnk_ctx_strip(ctx, p, n, ctx->vbuf);
        i++;
        k = abs(n);
        p += k;
        NJ_SYS_COUNT->ntri += k - 2;
    }
}

/*
 * Env-mapped strips on VU1, no clipping: per strip (up to 50) a block with its GIF tag and a
 * winding header from the vertex buffer. Header words 2 and 4 are never set. Counts triangles.
 */
/* 0x001ac828 */
static void nj_cnk_ctx_vu1_strips_tex(NjCnkCtx *ctx, short *p)
{
    NjCnkVu1NjVifPkt *pk;
    unsigned int *tw;
    int i, n, cnt, hd, words;
    unsigned long tag;
    union {
        float f[4];
        int i[4];
    } hdr_val;
    unsigned int *gif_tag;
    int *hdr;
    char *xyz, *col, *st;

    tw = (unsigned int *)ctx->tag;
    tag = (((ctx->prim & ~7L) | 0x14) << 47) | 0xc001L << 46;
    hd = *p++;
    tag |= 0x8000;
    cnt = hd & 0x3fff;
    ctx->tag[1] = 0x412;
    ctx->tag[0] = tag;
    pk = psGsGetRefPkt();
    psVifPktAddWord(pk, 0x6c010055);
    psVifPktAddWords(pk, &ctx->vu1_tex_tag, 4);
    psVifPktAddWord(pk, 0x01000104);
    psVifPktAddWord(pk, 0x1500000a);
    hdr_val.f[0] = -1.0f;
    for (i = 0; i < cnt; i++) {
        n = *p++;
        if (n > 0) {
            hdr_val.f[2] = 1.0f;
        } else {
            hdr_val.f[2] = -1.0f;
            n = -n;
        }
        if (n >= 51) {
            p += n;
        } else {
            words = nj_vu1_strip_block(pk, n, &gif_tag, &hdr, &xyz, &col, &st);
            *gif_tag++ = tw[0] | n;
            *gif_tag++ = tw[1];
            *gif_tag++ = tw[2];
            *gif_tag++ = tw[3];
            *hdr++ = hdr_val.i[0];
            *hdr++ = hdr_val.i[1];
            *hdr++ = hdr_val.i[2];
            *hdr++ = hdr_val.i[3];
            nj_vu0_gather_st(ctx, p, n, ctx->vbuf, xyz, col, st);
            pk->nwords += words;
            pk->cur += words * 4;
            p += n;
            NJ_SYS_COUNT->ntri += n - 2;
        }
    }
    psVifPktAddWord(pk, 0x10000000);
    psVifPktAddWord(pk, 0x01000101);
}

/* Textured strips (vertex index, u, v). */
/* 0x001acaa0 */
static void nj_cnk_ctx_vu1_uv_strips(NjCnkCtx *ctx, short *p)
{
    NjCnkVu1NjVifPkt *pk;
    unsigned int *tw;
    int i, n, cnt, hd, words;
    unsigned long tag;
    union {
        float f[4];
        int i[4];
    } hdr_val;
    unsigned int *gif_tag;
    int *hdr;
    char *xyz, *col, *st;

    tw = (unsigned int *)ctx->tag;
    tag = (((ctx->prim & ~7L) | 0x14) << 47) | 0xc001L << 46;
    hd = *p++;
    tag |= 0x8000;
    cnt = hd & 0x3fff;
    ctx->tag[1] = 0x412;
    ctx->tag[0] = tag;
    pk = psGsGetRefPkt();
    psVifPktAddWord(pk, 0x6c010055);
    psVifPktAddWords(pk, &ctx->vu1_tex_tag, 4);
    psVifPktAddWord(pk, 0x1500000a);
    psVifPktAddWord(pk, 0x01000104);
    hdr_val.f[0] = -1.0f;
    for (i = 0; i < cnt; i++) {
        n = *p++;
        if (n > 0) {
            hdr_val.f[2] = 1.0f;
        } else {
            hdr_val.f[2] = -1.0f;
            n = -n;
        }
        if (n >= 51) {
            p += n * 3;
        } else {
            words = nj_vu1_strip_block(pk, n, &gif_tag, &hdr, &xyz, &col, &st);
            *gif_tag++ = tw[0] | n;
            *gif_tag++ = tw[1];
            *gif_tag++ = tw[2];
            *gif_tag++ = tw[3];
            *hdr++ = hdr_val.i[0];
            *hdr++ = hdr_val.i[1];
            *hdr++ = hdr_val.i[2];
            *hdr++ = hdr_val.i[3];
            nj_vu0_gather_uv(ctx, p, n, ctx->vbuf, xyz, col, st);
            pk->nwords += words;
            pk->cur += words * 4;
            p += n * 3;
            NJ_SYS_COUNT->ntri += n - 2;
        }
    }
    psVifPktAddWord(pk, 0x01000101);
    psVifPktAddWord(pk, 0x10000000);
}

/* Untextured, another VU1 program. Bug: the strip header is copied from a never-set local. */
/* 0x001acd20 */
static void nj_cnk_ctx_vu1_strips(NjCnkCtx *ctx, short *p)
{
    NjCnkVu1NjVifPkt *pk;
    unsigned int *tw;
    int i, n, cnt, hd, words;
    int hdr_val[4];
    unsigned int *gif_tag;
    int *hdr;
    char *xyz, *col;

    tw = (unsigned int *)ctx->tag;
    hd = *p++;
    cnt = hd & 0x3fff;
    ctx->tag[1] = 0x412;
    ctx->tag[0] = (long)(int)(ctx->prim | 4) << 47 | 0x3000400000008000L;
    pk = psGsGetRefPkt();
    psVifPktAddWord(pk, 0x6c010055);
    psVifPktAddWords(pk, &ctx->vu1_tag, 4);
    psVifPktAddWord(pk, 0x15000010);
    psVifPktAddWord(pk, 0x01000104);
    for (i = 0; i < cnt; i++) {
        n = *p++;
        n = abs(n);
        if (n >= 51) {
            p += n;
        } else {
            words = nj_vu1_strip_block(pk, n, &gif_tag, &hdr, &xyz, &col, 0);
            gif_tag[0] = tw[0] | n;
            gif_tag[1] = tw[1];
            gif_tag[2] = tw[2];
            gif_tag[3] = tw[3];
            hdr[0] = hdr_val[0];
            hdr[1] = hdr_val[1];
            hdr[2] = hdr_val[2];
            hdr[3] = hdr_val[3];
            nj_vu0_gather(ctx, p, n, ctx->vbuf, xyz, col);
            pk->nwords += words;
            pk->cur += words * 4;
            p += n;
            NJ_SYS_COUNT->ntri += n - 2;
        }
    }
    psVifPktAddWord(pk, 0x01000101);
    psVifPktAddWord(pk, 0x10000000);
}

/* Env-mapped strips on VU1, no clipping, white or shaded; header: length, winding, primitive. */
/* 0x001acf30 */
static void nj_cnk_vu1_strips_env_noclip(NjCnkCtx *ctx, short *list)
{
    NjCnkConv conv;
    NjCnkVu1NjVifPkt *pk;
    short *cur;
    int i, n, cnt, hd, words, strip_prim;
    float dir;
    int gif_tag;
    NjVuHdr *hdr;
    char *xyz, *col, *st;

    if (nj_cnk_vu1_is_lit(ctx, ctx->strip_flags))
        conv = nj_vu0_gather_white_st;
    else
        conv = nj_vu0_gather_st;
    cur = list;
    strip_prim = ctx->prim | 20;
    hd = *cur++;
    cnt = hd & 0x3fff;
    pk = psGsGetRefPkt();
    psVifPktAddUnpack(pk, 0x6c010055, &ctx->vu1_tex_tag, 1);
    psVifPktAddWord(pk, 0x15000008);
    psVifPktAddWord(pk, 0x01000104);
    for (i = 0; i < cnt; i++) {
        n = *cur++;
        if (n > 0) {
            dir = 1.0f;
        } else {
            dir = -1.0f;
            n = -n;
        }
        if (n >= 51) {
            cur += n;
        } else {
            words = nj_vu1_strip_block(pk, n, &gif_tag, &hdr, &xyz, &col, &st);
            hdr->nloop = n | 0x8000;
            hdr->sign = dir;
            hdr->mode = -1.0f;
            hdr->prim = strip_prim;
            conv(ctx, cur, n, ctx->vbuf, xyz, col, st);
            pk->nwords += words;
            pk->cur += words * 4;
            cur += n;
            NJ_SYS_COUNT->ntri += n - 2;
        }
    }
    psVifPktAddWord(pk, 0x10000000);
    psVifPktAddWord(pk, 0x01000101);
}

/* Textured strips (vertex index, u, v). */
/* 0x001ad130 */
static void nj_cnk_vu1_uv_strips_noclip(NjCnkCtx *ctx, short *list)
{
    NjCnkConv conv;
    NjCnkVu1NjVifPkt *pk;
    short *cur;
    int i, n, cnt, hd, words, strip_prim;
    float dir;
    int gif_tag;
    NjVuHdr *hdr;
    char *xyz, *col, *st;

    if (nj_cnk_vu1_is_lit(ctx, ctx->strip_flags))
        conv = nj_vu0_gather_white_uv;
    else
        conv = nj_vu0_gather_uv;
    cur = list;
    strip_prim = ctx->prim | 20;
    hd = *cur++;
    cnt = hd & 0x3fff;
    pk = psGsGetRefPkt();
    psVifPktAddUnpack(pk, 0x6c010055, &ctx->vu1_tex_tag, 1);
    psVifPktAddWord(pk, 0x15000008);
    psVifPktAddWord(pk, 0x01000104);
    for (i = 0; i < cnt; i++) {
        n = *cur++;
        if (n > 0) {
            dir = 1.0f;
        } else {
            dir = -1.0f;
            n = -n;
        }
        if (n >= 51) {
            cur += n * 3;
        } else {
            words = nj_vu1_strip_block(pk, n, &gif_tag, &hdr, &xyz, &col, &st);
            hdr->nloop = n | 0x8000;
            hdr->sign = dir;
            hdr->mode = -1.0f;
            hdr->prim = strip_prim;
            conv(ctx, cur, n, ctx->vbuf, xyz, col, st);
            pk->nwords += words;
            pk->cur += words * 4;
            cur += n * 3;
            NJ_SYS_COUNT->ntri += n - 2;
        }
    }
    psVifPktAddWord(pk, 0x01000101);
    psVifPktAddWord(pk, 0x10000000);
}

/* Untextured strips. */
/* 0x001ad340 */
static void nj_cnk_vu1_strips_noclip(NjCnkCtx *ctx, short *list)
{
    NjCnkVu1NjCnkConv conv;
    NjCnkVu1NjVifPkt *pk;
    short *cur;
    int i, n, cnt, hd, words, strip_prim;
    union {
        float f[4];
        int i[4];
    } hdr_val;
    int gif_tag;
    int *hdr;
    char *xyz, *col;

    if (nj_cnk_vu1_is_lit(ctx, ctx->strip_flags))
        conv = nj_vu0_gather_white;
    else
        conv = nj_vu0_gather;
    cur = list;
    strip_prim = ctx->prim | 4;
    hd = *cur++;
    cnt = hd & 0x3fff;
    pk = psGsGetRefPkt();
    psVifPktAddUnpack(pk, 0x6c010055, &ctx->vu1_tag, 1);
    psVifPktAddWord(pk, 0x15000008);
    psVifPktAddWord(pk, 0x01000104);
    hdr_val.f[0] = -1.0f;
    for (i = 0; i < cnt; i++) {
        n = *cur++;
        if (n > 0) {
            hdr_val.f[2] = 1.0f;
        } else {
            hdr_val.f[2] = -1.0f;
            n = -n;
        }
        if (n >= 51) {
            cur += n;
        } else {
            words = nj_vu1_strip_block(pk, n, &gif_tag, &hdr, &xyz, &col, 0);
            hdr[1] = n | 0x8000;
            *(float *)hdr = -1.0f;
            hdr[2] = hdr_val.i[2];
            hdr[3] = strip_prim;
            conv(ctx, cur, n, ctx->vbuf, xyz, col);
            pk->nwords += words;
            pk->cur += words * 4;
            cur += n;
            NJ_SYS_COUNT->ntri += n - 2;
        }
    }
    psVifPktAddWord(pk, 0x01000101);
    psVifPktAddWord(pk, 0x10000000);
}

/* Shaded env-mapped strip; crossing triangles clipped straight from the vertex buffer, no copy. */
/* 0x001ad550 */
static void nj_cnk_vu1_strip_env(NjCnkCtx *ctx, short *idx, int n, char *vbuf, float dir)
{
    NjCnkVu1NjVifPkt *pk;
    int run, nclip, words, strip_prim, fan_prim;
    int gif_tag;
    NjVuHdr *hdr;
    char *xyz, *col, *st;
    int unused2[4];
    NjClipBuf cb;
    char tmp[3][64];
    int unused[4];

    pk = psGsGetRefPkt();
    strip_prim = ctx->prim | 20;
    fan_prim = ctx->prim | 21;
    while (n >= 3) {
        words = nj_vu1_strip_block(pk, n, &gif_tag, &hdr, &xyz, &col, &st);
        run = nj_vu0_gather_st_clip(ctx, idx, n, vbuf, xyz, col, st);
        if (run >= 3) {
            hdr->nloop = run | 0x8000;
            hdr->prim = strip_prim;
            hdr->mode = -1.0f;
            pk->cur += words * 4;
            pk->nwords += words;
            hdr->sign = dir;
            idx += run - 2;
            n += 2 - run;
            if (run & 1)
                dir = -dir;
        }
        if (n < 3)
            break;
        nclip = ulGxClipTriangle(&cb, vbuf + idx[0] * 64, vbuf + idx[1] * 64, vbuf + idx[2] * 64);
        if (nclip > 0) {
            words = nj_vu1_strip_block(pk, nclip, &gif_tag, &hdr, &xyz, &col, &st);
            hdr->mode = 1.0f;
            hdr->nloop = nclip | 0x8000;
            hdr->sign = dir;
            hdr->prim = fan_prim;
            vu0_clip_gather_st(ctx, *(void **)&cb.out, nclip, xyz, col, st);
            pk->nwords += words;
            pk->cur += words * 4;
            NJ_SYS_COUNT->nstrip++;
        }
        n--;
        idx++;
        dir = -dir;
    }
}

/* 0x001ad7c0 */
static void nj_cnk_vu1_nop(void)
{
}
