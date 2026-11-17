/*
 * Wire renderer: 3D lines (VU0 or VU1 transformed), draw colour and width, box culling, model
 * draw paths (direct, skinned, precompiled), and the wire compiler turning wire meshes into VU1
 * packets.
 */

#include "types_game.h"
#include "libc.h"
#include "libc_internal.h"
#include "rez_wire_compile.h"

typedef struct DrawCtx {
    char pad_00[320];
    char blk320[720];
    char blk1040[352];
    char blk1392[8];            /* 1392 */
    int word1400;               /* 1400 */
    char pad_57c[900];
    int base_flags;             /* 2304 */
} DrawCtx;

typedef struct WireVtx {
    int pos[3];                 /* 0 */
    char pad_0c[20];
    int col[4];                 /* 32 */
    char pad_30[12];
    int hidden;                 /* 60 */
} WireVtx;

typedef struct WireSeg {
    WireVtx v[2];
} WireSeg;

/* 128 bytes */
typedef struct WireCtx {
    char pad_00[1612];
    WireSeg *seg;               /* 1612 */
} WireCtx;

typedef union GifTag {
    unsigned long d[2];
    unsigned int w[4];
} GifTag;

typedef struct WireLineVtx {
    int pos[3];                 /* 0 */
    int col;                    /* 12 */
} WireLineVtx;

typedef struct DrawCtxView {
    char pad_00[1392];
    int regs;                   /* 1392 */
    char pad_574[12];
    int r;                      /* 1408 */
    int g;                      /* 1412 */
    int b;                      /* 1416 */
    unsigned int a;             /* 1420 */
    char pad_590[880];
    int base_flags;             /* 2304 */
} DrawCtxView;

typedef struct ScanRec {
    int head;
    unsigned short lo;          /* 4 */
    unsigned short hi;          /* 6 */
} ScanRec;

typedef struct DNode DNode;

struct DNode {
    unsigned int flags;         /* 0 */
    void *d4;                   /* 4 */
    char pos[12];               /* 8 */
    char rot[12];               /* 20 */
    char scl[12];               /* 32 */
    DNode *child;                 /* 44 */
    DNode *next;                 /* 48 */
};

typedef struct DCtx DCtx;

struct DCtx {
    DCtx *next;                 /* 0 */
};

typedef struct RezWireCompileDNode RezWireCompileDNode;

struct RezWireCompileDNode {
    unsigned int flags;         /* 0 */
    int d4;                     /* 4 */
    char pos[12];               /* 8 */
    char rot[12];               /* 20 */
    char scl[12];               /* 32 */
    RezWireCompileDNode *child;                 /* 44 */
    RezWireCompileDNode *next;                 /* 48 */
};

typedef struct RezWireCompileDCtx RezWireCompileDCtx;

struct RezWireCompileDCtx {
    char pad_00[4];
    int d4;                     /* 4 */
    char pad_08[4];
    RezWireCompileDCtx *next;                  /* 12 */
    RezWireCompileDCtx *child;                  /* 16 */
};

typedef struct DumpNode DumpNode;

struct DumpNode {
    int lines;                  /* 0 */
    char pad_04[4];
    int size;                   /* 8 */
    DumpNode *next;             /* 12 */
    DumpNode *child;            /* 16 */
};

typedef struct TNode TNode;

struct TNode {
    char pad_00[4];
    int data;                   /* 4 */
    char pad_08[4];
    TNode *next;                /* 12 */
    TNode *child;               /* 16 */
};

typedef struct TCtx TCtx;

struct TCtx {
    char pad_00[4];
    int dst;                    /* 4 */
    char pad_08[36];
    TCtx *child;                /* 44 */
    TCtx *next;                 /* 48 */
};

typedef struct SNode SNode;

struct SNode {
    SNode *next;                /* 0 */
    int count;                  /* 4 */
};

enum WireKind { WIRE_KIND_0 };

typedef struct WireSrc {
    int *data;
    enum WireKind kind;         /* stride in ints */
} WireSrc;

typedef struct WireMesh {
    int count;                  /* 0 */
    unsigned short idx[1];      /* 4: index pairs */
} WireMesh;

typedef struct WNode WNode;

struct WNode {
    char pad_00[4];
    void *data;                 /* 4 */
    char pad_08[36];
    WNode *child;                 /* 44 */
    WNode *next;                 /* 48 */
};

typedef struct WEnt {
    struct WEnt *next;          /* 0 */
    int lines;                     /* 4 */
} WEnt;

typedef struct WHdr {
    int lines;
    char *blk;                  /* 4 */
    int size;                   /* 8 */
    char *next;                  /* 12 */
    char *child;                  /* 16 */
} WHdr;

extern int line_draw_flags;
extern void *psGsGetDisplayList(void);
extern void *psDisplayListOpenRefPkt(void *);
extern int psDisplayListIsDirty(void *);
extern void psVifPktOpenGifPkt(void *);
extern void psDisplayListFlushGsRegs(void *, void *);
extern void psVifPktCloseGifPkt(void *);
extern void psVifPktAddWord(void *, int);
extern void psVifPktAddWords(void *, void *, int);
extern int draw_poly_count;
extern DlBuf *psDisplayListGetVifPkt(void *);
extern void vu0_wire_vertex_convert(WireCtx *, void *, WireSeg *, int);
extern void psDisplayListCloseRefPkt(void *);
extern void *ulMatrixCur;
extern void ulGxSetMatrix(int, int);
extern unsigned int draw_color;
extern int draw_flags;
extern float wire_line_width;
extern char ulGxState[];
extern void vu0_matrix_mul(void *, void *, int);
extern int vu0_box_clip_codes(void *, float (*)[4], int, int);
extern void vu0_points_any_clipped(void *, float (*)[4], int);
extern char *nj_cnk_direct_vtx;
extern int nj_cnk_direct_stride;
extern void psGsOpenRefPkt(void);
extern void psGsCloseRefPkt(void);
extern void vu0_weight_blend_first(CmdCtx *, int *, char *, unsigned int);
extern void vu0_weight_blend_add_count(CmdCtx *, int *, char *, unsigned int);
extern unsigned int vu0_weight_blend_last(CmdCtx *, int *, char *, unsigned int);
extern unsigned int vu0_points_transform_clip(CmdCtx *, int *, char *, unsigned int, int);
extern void vu0_weight_blend_add(CmdCtx *, int *, char *, unsigned int);
extern int vu0_points_transform(CmdCtx *, int *, char *, unsigned int, int);
extern char model_list_work[];
extern int njGetMatrix(char *);
extern int njPushMatrixEx(void);
extern int njTranslateEx(char *);
extern int nj_matrix_rotate_cur_ang3(char *, unsigned int);
extern int njScaleEx(char *);
extern int njPopMatrixEx(void);
extern void psDisplayListAddRefPkt(void *, void *, unsigned int);
extern char model_tree_work[];
extern void (*wire_free_func)(int);
extern char str_wire_assert_zero[];
extern char str_wire_object[];
extern void *(*wire_alloc_func)(int);
extern int ulGxGetDrawFlags();

static int draw_vif_packet_build(VifBuf *dl, int n, unsigned int **hdr_out, unsigned int **pos_out, unsigned int **col_out);
static void wire_cmd_find_vtx(int *rec, int **out_ptr, int *out_kind);

/*
 * EE (VU0) transform. on sets line flag 0x40; opens a reference packet uploading the line state
 * and starting VU1's line program.
 */
/* 0x00179530 */
void line_begin_vu0(int on)
{
    void *pkt;
    void *dl;
    DrawCtx *gx = &(*(DrawCtx *)&ulGxState); /* steer(cast) */
    int flags;

    if (on)
        line_draw_flags |= 0x40;
    else
        line_draw_flags &= ~0x40;
    flags = line_draw_flags | 1;
    flags |= (*(DrawCtx *)&ulGxState).base_flags; /* steer(cast) */
    line_draw_flags = flags;
    gx->word1400 = flags;
    pkt = psGsGetDisplayList();
    dl = psDisplayListOpenRefPkt(pkt);
    if (psDisplayListIsDirty(pkt)) {
        psVifPktOpenGifPkt(dl);
        psDisplayListFlushGsRegs(pkt, dl);
        psVifPktCloseGifPkt(dl);
    }
    psVifPktAddWord(dl, 0x1000101);
    psVifPktAddWord(dl, 0x6c010057);
    psVifPktAddWords(dl, gx->blk1392, 4);
    psVifPktAddWord(dl, 0x6c010055);
    psVifPktAddWords(dl, gx->blk1040, 4);
    psVifPktAddWord(dl, 0x15000014);
    psVifPktAddWord(dl, 0x1000104);
}

/*
 * n/2 lines, clipped on VU0; segments with neither end clipped away go as GIF lines, 25 per chunk.
 * Counts in draw_poly_count.
 */
/* 0x00179648 */
void line_draw_vu0(void *src, int n)
{
    GifTag tag;
    unsigned int *hdr;
    unsigned int *pos;
    unsigned int *col;
    WireCtx *ctx = &ulGxState;
    void *pkt;
    DlBuf *dl;
    WireSeg *seg;
    int left;
    int chunk;
    int size;
    int k;
    int skip;
    int cnt;

    pkt = psGsGetDisplayList();
    dl = psDisplayListGetVifPkt(pkt);
    vu0_wire_vertex_convert(ctx, src, ctx->seg, n);
    if (psDisplayListIsDirty(pkt)) {
        psVifPktOpenGifPkt(dl);
        psDisplayListFlushGsRegs(pkt, dl);
        psVifPktCloseGifPkt(dl);
    }
    tag.d[0] = ((unsigned long)line_draw_flags << 47) | 0x3000400000008000UL;
    tag.d[1] = 0x412;
    left = n / 2;
    seg = ctx->seg;
    while (left > 0) {
        chunk = left < 26 ? left : 25;
        size = draw_vif_packet_build(dl, chunk * 2, &hdr, &pos, &col);
        skip = 0;
        for (k = 0; k < chunk; k++, seg++) {
            if (seg->v[0].hidden != 0 || seg->v[1].hidden != 0) {
                skip++;
                continue;
            }
            *pos++ = seg->v[0].pos[0];
            *pos++ = seg->v[0].pos[1];
            *pos++ = seg->v[0].pos[2];
            *col++ = seg->v[0].col[0];
            *col++ = seg->v[0].col[1];
            *col++ = seg->v[0].col[2];
            *col++ = seg->v[0].col[3];
            *pos++ = seg->v[1].pos[0];
            *pos++ = seg->v[1].pos[1];
            *pos++ = seg->v[1].pos[2];
            *col++ = seg->v[1].col[0];
            *col++ = seg->v[1].col[1];
            *col++ = seg->v[1].col[2];
            *col++ = seg->v[1].col[3];
        }
        cnt = chunk - skip;
        if (cnt > 0) {
            hdr[0] = tag.w[0] | (cnt * 2);
            hdr[1] = tag.w[1];
            hdr[2] = tag.w[2];
            hdr[3] = tag.w[3];
            dl->buf += size;
            dl->words += size;
        }
        left -= chunk;
    }
    draw_poly_count += n / 2;
}

/* 0x00179928 */
void line_end_vu0(void)
{
    void *pkt = psGsGetDisplayList();
    void *dl = psDisplayListGetVifPkt(pkt);

    psVifPktAddWord(dl, 0x10000000);
    psVifPktAddWord(dl, 0x1000101);
    psDisplayListCloseRefPkt(pkt);
}

/* VU1 transform: also uploads the current matrix; starts the transforming line program. */
/* 0x00179988 */
void line_begin_vu1(int on)
{
    void *pkt;
    void *dl;
    DrawCtx *gx = &(*(DrawCtx *)&ulGxState); /* steer(cast) */
    int flags;

    if (on)
        line_draw_flags |= 0x40;
    else
        line_draw_flags &= ~0x40;
    flags = line_draw_flags | 1;
    flags |= (*(DrawCtx *)&ulGxState).base_flags; /* steer(cast) */
    line_draw_flags = flags;
    gx->word1400 = flags;
    ulGxSetMatrix(0, ulMatrixCur);
    pkt = psGsGetDisplayList();
    dl = psDisplayListOpenRefPkt(pkt);
    if (psDisplayListIsDirty(pkt)) {
        psVifPktOpenGifPkt(dl);
        psDisplayListFlushGsRegs(pkt, dl);
        psVifPktCloseGifPkt(dl);
    }
    psVifPktAddWord(dl, 0x1000101);
    psVifPktAddWord(dl, 0x6c010057);
    psVifPktAddWords(dl, gx->blk1392, 4);
    psVifPktAddWord(dl, 0x6c040059);
    psVifPktAddWords(dl, gx->blk320, 16);
    psVifPktAddWord(dl, 0x6c010055);
    psVifPktAddWords(dl, gx->blk1040, 4);
    psVifPktAddWord(dl, 0x15000020);
    psVifPktAddWord(dl, 0x1000104);
}

/*
 * n/2 lines of {position, packed colour}; chunks of 25 (count, positions, colours) to VU1. Counts
 * in draw_poly_count.
 */
/* 0x00179ad0 */
void line_draw_vu1(WireLineVtx *src, int n)
{
    void *pkt;
    DlBuf *dl;
    unsigned int *dst;
    unsigned int *hdr;
    unsigned int *pos;
    unsigned int *col;
    int left;
    int chunk;
    int size;
    int k;
    WireLineVtx *vtx;

    pkt = psGsGetDisplayList();
    dl = psDisplayListGetVifPkt(pkt);
    if (psDisplayListIsDirty(pkt)) {
        psVifPktOpenGifPkt(dl);
        psDisplayListFlushGsRegs(pkt, dl);
        psVifPktCloseGifPkt(dl);
    }
    left = n / 2;
    vtx = src;
    while (left > 0) {
        chunk = left < 26 ? left : 25;
        dst = dl->buf;
        *dst++ = 0x6c018000;
        hdr = dst;
        dst += 4;
        *dst++ = ((chunk * 2) << 16) | 0x68008004;
        pos = dst;
        dst += chunk * 6;
        size = chunk * 6 + 6;
        size += 1 + chunk * 2;
        size += 1;
        *dst++ = ((chunk * 2) << 16) | 0x6e00c007;
        col = dst;
        dst += chunk * 2;
        *dst = 0x17000000;
        for (k = 0; k < chunk; k++) {
            *pos++ = vtx[0].pos[0];
            *pos++ = vtx[0].pos[1];
            *pos++ = vtx[0].pos[2];
            *col++ = vtx[0].col;
            *pos++ = vtx[1].pos[0];
            *pos++ = vtx[1].pos[1];
            *pos++ = vtx[1].pos[2];
            *col++ = vtx[1].col;
            vtx += 2;
        }
        *hdr = chunk;
        dl->buf += size;
        dl->words += size;
        left -= chunk;
    }
    draw_poly_count += n / 2;
}

/* 0x00179cb8 */
void line_end_vu1(void)
{
    void *pkt = psGsGetDisplayList();

    psVifPktAddWord(psDisplayListGetVifPkt(pkt), 0x1000101);
    psDisplayListCloseRefPkt(pkt);
}

/* Alpha below 128 enables blending; uploaded to VU1. Returns the previous colour. */
/* 0x00179d00 */
unsigned int draw_set_color(unsigned int color)
{
    unsigned int old = draw_color;
    void *pkt;
    void *dl;
    DrawCtxView *gx = &(*(DrawCtxView *)&ulGxState); /* steer(cast) */
    int flags;

    gx->r = color & 0xff;
    gx->g = (color >> 8) & 0xff;
    gx->b = (color >> 16) & 0xff;
    gx->a = color >> 24;
    draw_color = color;
    if (gx->a < 128)
        draw_flags |= 0x40;
    else
        draw_flags &= ~0x40;
    flags = draw_flags | 1;
    flags |= (*(DrawCtxView *)&ulGxState).base_flags; /* steer(cast) */
    draw_flags = flags;
    gx->regs = flags;
    pkt = psGsGetDisplayList();
    dl = psDisplayListOpenRefPkt(pkt);
    psVifPktAddWord(dl, 0x1000101);
    psVifPktAddWord(dl, 0x6c020057);
    psVifPktAddWords(dl, &gx->regs, 8);
    psDisplayListCloseRefPkt(pkt);
    return old;
}

/* 0x00179df8 */
static int draw_get_color(void)
{
    return draw_color;
}

/* Returns the previous one. */
/* 0x00179e00 */
float line_width_set(float v)
{
    float old = wire_line_width;

    wire_line_width = v;
    return old;
}

/* Since the count was cleared. */
/* 0x00179e10 */
static int draw_get_poly_count(void)
{
    return draw_poly_count;
}

/* 0x00179e18 */
void draw_clear_poly_count(void)
{
    draw_poly_count = 0;
}

/* View clip matrix times the current matrix. */
/* 0x00179e20 */
static void draw_matrix_update(void)
{
    char *gx = ulGxState;

    vu0_matrix_mul(gx + 768, gx + 704, ulMatrixCur);
}

/* Cube of half-size half about center, current matrix; 0 when fully inside. */
/* 0x00179e38 */
unsigned int draw_cull_box(float half, float *center)
{
    float mtx[16];
    float corners[8][4];
    float xm = center[0] - half;
    float xp = center[0] + half;
    float yp = center[1] + half;
    float ym = center[1] - half;
    float zm = center[2] - half;
    float zp = center[2] + half;
    char *base = ulGxState;

    corners[0][0] = corners[1][0] = corners[4][0] = corners[5][0] = xm;
    corners[2][0] = corners[3][0] = corners[6][0] = corners[7][0] = xp;
    corners[0][1] = corners[2][1] = corners[4][1] = corners[6][1] = yp;
    corners[1][1] = corners[3][1] = corners[5][1] = corners[7][1] = ym;
    corners[0][2] = corners[1][2] = corners[2][2] = corners[3][2] = zm;
    corners[4][2] = corners[5][2] = corners[6][2] = corners[7][2] = zp;
    return vu0_box_clip_codes(base + 704, corners, 8, ulMatrixCur);
}

/* Unused. Whether any cube corner is clipped, current matrix. */
/* 0x00179ef0 */
static void draw_clip_box_local(float *center, float half)
{
    float mtx[16];
    float corners[8][4];
    float xm = center[0] - half;
    float xp = center[0] + half;
    float yp = center[1] + half;
    float ym = center[1] - half;
    float zm = center[2] - half;
    float zp = center[2] + half;
    char *base = ulGxState;

    corners[0][0] = corners[1][0] = corners[4][0] = corners[5][0] = xm;
    corners[2][0] = corners[3][0] = corners[6][0] = corners[7][0] = xp;
    corners[0][1] = corners[2][1] = corners[4][1] = corners[6][1] = yp;
    corners[1][1] = corners[3][1] = corners[5][1] = corners[7][1] = ym;
    corners[0][2] = corners[1][2] = corners[2][2] = corners[3][2] = zm;
    corners[4][2] = corners[5][2] = corners[6][2] = corners[7][2] = zp;
    vu0_matrix_mul(mtx, base + 704, ulMatrixCur);
    vu0_points_any_clipped(mtx, corners, 8);
}

/* Unused. With the Ninja clip matrix. */
/* 0x00179fb0 */
static void draw_clip_box(float *center, float half)
{
    float corners[8][4];
    float xm = center[0] - half;
    float xp = center[0] + half;
    float yp = center[1] + half;
    float ym = center[1] - half;
    float zm = center[2] - half;
    float zp = center[2] + half;

    corners[0][0] = corners[1][0] = corners[4][0] = corners[5][0] = xm;
    corners[2][0] = corners[3][0] = corners[6][0] = corners[7][0] = xp;
    corners[0][1] = corners[2][1] = corners[4][1] = corners[6][1] = yp;
    corners[1][1] = corners[3][1] = corners[5][1] = corners[7][1] = ym;
    corners[0][2] = corners[1][2] = corners[2][2] = corners[3][2] = zm;
    corners[4][2] = corners[5][2] = corners[6][2] = corners[7][2] = zp;
    vu0_points_any_clipped(nj_clip_matrix, corners, 8);
}

/* Flags 0x08 and 0x80 from bits; returns previous flags without bits 0..2. */
/* 0x0017a060 */
static int draw_flags_set(int bits)
{
    int old = draw_flags;

    draw_flags = old | (bits & 0x88);
    return old & ~7;
}

/*
 * Header, n positions, n colours with col_out, then a VU1 program start. Parts via the out
 * pointers; returns the size in words.
 */
/* 0x0017a080 */
static int draw_vif_packet_build(VifBuf *dl, int n, unsigned int **hdr_out, unsigned int **pos_out, unsigned int **col_out)
{
    unsigned int *dst = dl->buf;
    int words;

    *dst++ = 0x6c018000;
    *hdr_out = dst;
    dst += 4;
    *dst++ = (n << 16) | 0x68008004;
    *pos_out = dst;
    dst += n * 3;
    words = n * 3 + 6;
    if (col_out != 0) {
        *dst++ = (n << 16) | 0x6c008007;
        *col_out = dst;
        dst += n * 4;
        words += n * 4 + 1;
    }
    *dst = 0x17000000;
    return words + 1;
}

/*
 * Index-pair edges from the vertex data model_cmd_scan_direct found; 25 lines per chunk. Counts
 * in draw_poly_count.
 */
/* 0x0017a118 */
static void model_edges_draw_direct(MeshObj *mesh)
{
    unsigned long tag[2];
    unsigned int *hdr;
    unsigned int *pos;
    int *pv[2];
    void *pkt;
    DlBuf *dl;
    unsigned short *ip;
    char *ctx = ulGxState;
    int *cnt = &mesh->count;
    int n;
    int chunk;
    int size;
    int k;

    if (mesh->valid == 0)
        return;
    pkt = psGsGetDisplayList();
    dl = psDisplayListGetVifPkt(pkt);
    if (psDisplayListIsDirty(pkt)) {
        psVifPktOpenGifPkt(dl);
        psDisplayListFlushGsRegs(pkt, dl);
        psVifPktCloseGifPkt(dl);
    }
    tag[0] = ((unsigned long)draw_flags << 47) | 0x3000400000008000UL;
    tag[1] = 0x412;
    psVifPktAddWord(dl, 0x1000101);
    ip = mesh->idx;
    psVifPktAddWord(dl, 0x6c040059);
    psVifPktAddWords(dl, ctx + 320, 16);
    psVifPktAddWord(dl, 0x1500001c);
    psVifPktAddWord(dl, 0x1000104);
    n = mesh->count;
    while (n > 0) {
        chunk = n < 26 ? n : 25;
        size = draw_vif_packet_build(dl, chunk * 2, &hdr, &pos, 0);
        *hdr = chunk;
        for (k = 0; k < chunk; k++) {
            pv[0] = (int *)(nj_cnk_direct_vtx + ip[0] * nj_cnk_direct_stride);
            pv[1] = (int *)(nj_cnk_direct_vtx + ip[1] * nj_cnk_direct_stride);
            ip += 2;
            *pos++ = pv[0][0];
            *pos++ = pv[0][1];
            *pos++ = pv[0][2];
            *pos++ = pv[1][0];
            *pos++ = pv[1][1];
            *pos++ = pv[1][2];
        }
        dl->buf += size;
        dl->words += size;
        n -= chunk;
    }
    psVifPktAddWord(dl, 0x1000101);
    psVifPktAddWord(dl, 0x10000000);
    draw_poly_count += *cnt;
}

/*
 * Last vertex chunk and stride (types 34, 35, 41, 42: 12, 16, 24, 28 bytes); sums vertex counts
 * into the state total. Returns 0.
 */
/* 0x0017a390 */
static int model_cmd_scan_direct(ScanRec *rec)
{
    unsigned int n;
    unsigned int hi;
    int type;
    int head;

    while ((head = rec->head) != 255) {
        type = head & 0xff;
        n = (unsigned int)head >> 16;
        hi = rec->hi;
        switch (type) {
        case 34:
            nj_cnk_direct_vtx = (char *)(rec + 1);
            nj_cnk_direct_stride = 12;
            break;
        case 41:
            nj_cnk_direct_vtx = (char *)(rec + 1);
            nj_cnk_direct_stride = 24;
            break;
        case 35:
            nj_cnk_direct_vtx = (char *)(rec + 1);
            nj_cnk_direct_stride = 16;
            break;
        case 42:
            nj_cnk_direct_vtx = (char *)(rec + 1);
            nj_cnk_direct_stride = 28;
            break;
        default:
            nj_cnk_direct_vtx = 0;
            nj_cnk_direct_stride = 0;
            break;
        }
        (*(CmdCtx *)&ulGxState).total += hi; /* steer(cast) */
        {
            unsigned int adv = n * 4 + 4;
            rec = (ScanRec *)((char *)rec + adv);
        }
    }
    return 0;
}

/* Untransformed vertices. */
/* 0x0017a458 */
int model_obj_draw_direct(int *obj, int rec)
{
    ulGxSetMatrix(0, ulMatrixCur);
    if (*obj != 0)
        model_cmd_scan_direct(*obj);
    if (!nj_cnk_direct_vtx)
        return 0;
    if (rec != 0) {
        psGsOpenRefPkt();
        model_edges_draw_direct(rec);
        psGsCloseRefPkt();
    }
    return 0;
}

/*
 * Vertices from model_cmd_transform; two matrices; 25 lines per chunk; nothing for an invalid or
 * empty mesh. Counts in draw_poly_count.
 */
/* 0x0017a4d8 */
static void model_edges_draw_normal(MeshObj *mesh)
{
    MeshVtx *pv[2];
    unsigned int *hdr;
    unsigned int *pos;
    void *pkt;
    DlBuf *dl;
    unsigned short *ip;
    MeshCtx *ctx = &ulGxState;
    int n;
    int chunk;
    int size;
    int k;
    int *pc;

    pc = &mesh->count;
    if (mesh->valid == 0)
        return;
    if (*pc == 0)
        return;
    pkt = psGsGetDisplayList();
    dl = psDisplayListGetVifPkt(pkt);
    if (psDisplayListIsDirty(pkt)) {
        psVifPktOpenGifPkt(dl);
        psDisplayListFlushGsRegs(pkt, dl);
        psVifPktCloseGifPkt(dl);
    }
    psVifPktAddWord(dl, 0x1000101);
    ip = mesh->idx;
    psVifPktAddWord(dl, 0x6c040059);
    psVifPktAddWords(dl, ctx->mtx128, 16);
    psVifPktAddWord(dl, 0x6c040030);
    psVifPktAddWords(dl, ctx->mtx704, 16);
    psVifPktAddWord(dl, 0x1500001e);
    psVifPktAddWord(dl, 0x1000104);
    n = mesh->count;
    while (n > 0) {
        chunk = n < 26 ? n : 25;
        size = draw_vif_packet_build(dl, chunk * 2, &hdr, &pos, 0);
        for (k = 0; k < chunk; k++) {
            pv[0] = &ctx->vtx[ip[0]];
            pv[1] = &ctx->vtx[ip[1]];
            ip += 2;
            *pos++ = pv[0]->pos[0];
            *pos++ = pv[0]->pos[1];
            *pos++ = pv[0]->pos[2];
            *pos++ = pv[1]->pos[0];
            *pos++ = pv[1]->pos[1];
            *pos++ = pv[1]->pos[2];
        }
        *hdr = chunk;
        dl->buf += size;
        dl->words += size;
        n -= chunk;
    }
    psVifPktAddWord(dl, 0x1000101);
    psVifPktAddWord(dl, 0x10000000);
    draw_poly_count += *pc;
}

/* First matrix only, simpler VU1 program. Builds a GIF tag that is never used. */
/* 0x0017a750 */
static void model_edges_draw_simple(MeshObj *mesh)
{
    unsigned long tag[2];
    MeshVtx *pv[2];
    unsigned int *hdr;
    unsigned int *pos;
    void *pkt;
    DlBuf *dl;
    unsigned short *ip;
    MeshCtx *ctx = &ulGxState;
    int n;
    int chunk;
    int size;
    int k;
    int *pc;

    pc = &mesh->count;
    if (mesh->valid == 0)
        return;
    if (*pc == 0)
        return;
    pkt = psGsGetDisplayList();
    dl = psDisplayListGetVifPkt(pkt);
    if (psDisplayListIsDirty(pkt)) {
        psVifPktOpenGifPkt(dl);
        psDisplayListFlushGsRegs(pkt, dl);
        psVifPktCloseGifPkt(dl);
    }
    tag[0] = ((unsigned long)draw_flags << 47) | 0x3000400000008000UL;
    tag[1] = 0x412;
    psVifPktAddWord(dl, 0x1000101);
    ip = mesh->idx;
    psVifPktAddWord(dl, 0x6c040059);
    psVifPktAddWords(dl, ctx->mtx128, 16);
    psVifPktAddWord(dl, 0x1500001c);
    psVifPktAddWord(dl, 0x1000104);
    n = mesh->count;
    while (n > 0) {
        chunk = n < 26 ? n : 25;
        size = draw_vif_packet_build(dl, chunk * 2, &hdr, &pos, 0);
        *hdr = chunk;
        for (k = 0; k < chunk; k++) {
            pv[0] = &ctx->vtx[ip[0]];
            pv[1] = &ctx->vtx[ip[1]];
            ip += 2;
            *pos++ = pv[0]->pos[0];
            *pos++ = pv[0]->pos[1];
            *pos++ = pv[0]->pos[2];
            *pos++ = pv[1]->pos[0];
            *pos++ = pv[1]->pos[1];
            *pos++ = pv[1]->pos[2];
        }
        dl->buf += size;
        dl->words += size;
        n -= chunk;
    }
    psVifPktAddWord(dl, 0x1000101);
    psVifPktAddWord(dl, 0x10000000);
    draw_poly_count += *pc;
}

/* Last weight chunk (0x200) ends the blend with clip codes and returns them; else vertex count. */
/* 0x0017a9d8 */
static unsigned int model_cmd_weight_clip(CmdCtx *gx, CmdRec *rec)
{
    unsigned int ent_word = rec->ent;
    int type = rec->head & 0x7f00;
    unsigned int hi = ent_word >> 16;
    char *ent = gx->base + ((ent_word & 0xffff) << 6);

    switch (type) {
    case 0:
        vu0_weight_blend_first(gx, rec->payload, ent, hi);
        break;
    case 0x100:
        vu0_weight_blend_add_count(gx, rec->payload, ent, hi);
        break;
    case 0x200:
        return vu0_weight_blend_last(gx, rec->payload, ent, hi);
    }
    return hi;
}

/*
 * Vertex chunks (34, 35, 41, 42) and weight chunks (44) into the vertex table; returns the last
 * chunk's clip result.
 */
/* 0x0017aa98 */
static unsigned int model_cmd_transform_clip(CmdRec *rec)
{
    CmdCtx *gx = &(*(CmdCtx *)&ulGxState); /* steer(cast) */
    unsigned int ret = 0;
    unsigned int n;
    unsigned int hi;
    char *ent;
    int type;
    int head;

    while ((head = rec->head) != 255) {
        type = head & 0xff;
        n = (unsigned int)head >> 16;
        hi = rec->ent >> 16;
        ent = gx->base + ((rec->ent & 0xffff) << 6);
        switch (type) {
        case 34:
            ret = vu0_points_transform_clip(gx, rec->payload, ent, hi, 3);
            break;
        case 41:
            ret = vu0_points_transform_clip(gx, rec->payload, ent, hi, 6);
            break;
        case 35:
            ret = vu0_points_transform_clip(gx, rec->payload, ent, hi, 4);
            break;
        case 42:
            ret = vu0_points_transform_clip(gx, rec->payload, ent, hi, 7);
            break;
        case 44:
            ret = model_cmd_weight_clip(gx, rec);
            break;
        default:
            ret = hi;
            break;
        }
        (*(CmdCtx *)&ulGxState).total += hi; /* steer(cast) */
        {
            unsigned int adv = n * 4 + 4;
            rec = (CmdRec *)((char *)rec + adv);
        }
    }
    return ret;
}

/* Chunk kind: first of a vertex set (0), following (0x100), last (0x200). Returns 0. */
/* 0x0017abe0 */
static int model_cmd_weight(CmdCtx *gx, CmdRec *rec)
{
    unsigned int ent_word = rec->ent;
    int type = rec->head & 0x7f00;
    unsigned int hi = ent_word >> 16;
    char *ent = gx->base + ((ent_word & 0xffff) << 6);

    switch (type) {
    case 0:
        vu0_weight_blend_first(gx, rec->payload, ent, hi);
        break;
    case 0x100:
        vu0_weight_blend_add_count(gx, rec->payload, ent, hi);
        break;
    case 0x200:
        vu0_weight_blend_add(gx, rec->payload, ent, hi);
        break;
    }
    return 0;
}

/*
 * Vertex chunks (34, 35, 41, 42, each its own format) and weight chunks (44); counts vertices.
 * Returns 0.
 */
/* 0x0017ac88 */
int model_cmd_transform(CmdRec *rec)
{
    CmdCtx *gx = (CmdCtx *)ulGxState; /* steer(cast): ulGx state as CmdCtx */
    unsigned int n;
    unsigned int hi;
    char *ent;
    int type;
    int head;

    while ((head = rec->head) != 255) {
        type = head & 0xff;
        n = (unsigned int)head >> 16;
        hi = rec->ent >> 16;
        ent = gx->base + ((rec->ent & 0xffff) << 6);
        switch (type) {
        case 34:
            vu0_points_transform(gx, rec->payload, ent, hi, 3);
            break;
        case 41:
            vu0_points_transform(gx, rec->payload, ent, hi, 6);
            break;
        case 35:
            vu0_points_transform(gx, rec->payload, ent, hi, 4);
            break;
        case 42:
            vu0_points_transform(gx, rec->payload, ent, hi, 7);
            break;
        case 44:
            model_cmd_weight(gx, rec);
            break;
        }
        (*(CmdCtx *)&ulGxState).total += hi; /* steer(cast): the context's total as a CmdCtx field */
        {
            unsigned int adv = n * 4 + 4;
            rec = (CmdRec *)((char *)rec + adv);
        }
    }
    return 0;
}

/* Transforms the chunk data first. Returns 0. */
/* 0x0017add0 */
int model_obj_draw_normal(int *obj, int rec)
{
    ulGxSetMatrix(0, ulMatrixCur);
    if (*obj != 0)
        model_cmd_transform((CmdRec *)*obj);
#ifdef VERSION_US
    if (rec != 0 && ((int *)rec)[1] > 0) {  /* meshes without edges are skipped */
#else
    if (rec != 0) {
#endif
        psGsOpenRefPkt();
        model_edges_draw_normal(rec);
        psGsCloseRefPkt();
    }
    return 0;
}

/* Simple edge drawing. */
/* 0x0017ae50 */
static int model_obj_draw_simple(int *obj, int rec)
{
    ulGxSetMatrix(0, ulMatrixCur);
    if (*obj != 0)
        model_cmd_transform((CmdRec *)*obj);
#ifdef VERSION_US
    if (rec != 0 && ((int *)rec)[1] > 0) {  /* meshes without edges are skipped */
#else
    if (rec != 0) {
#endif
        psGsOpenRefPkt();
        model_edges_draw_simple(rec);
        psGsCloseRefPkt();
    }
    return 0;
}

/* Flag 1 clear: normal; flags 1 and 2: direct (untransformed); flag 1 only: simple. */
/* 0x0017aed0 */
void model_obj_draw(void *obj, void *rec)
{
    if (rec != 0) {
        if (ulGxGetDrawFlags() & 1) {
            if (ulGxGetDrawFlags() & 2)
                model_obj_draw_direct(obj, rec);
            else
                model_obj_draw_simple(obj, rec);
        } else {
            model_obj_draw_normal(obj, rec);
        }
    }
}

/* rec walks the mesh list alongside the nodes; node flags skip translate/rotate/scale. */
/* 0x0017af70 */
static void model_list_draw(DNode *node, DCtx *rec)
{
    unsigned int fl;

    do {
        fl = node->flags;
        if (fl & 0x800)
            njGetMatrix(model_list_work);
        if ((fl & 7) == 7) {
            if (!(fl & 8) && node->d4)
                model_obj_draw(node->d4, rec);
            if (!(fl & 0x10))
                model_list_draw(node->child, rec->next);
        } else {
            njPushMatrixEx();
            if (!(fl & 1))
                njTranslateEx(node->pos);
            if (!(fl & 2)) {
                if (!(fl & 0x1000)) {
                    if (!(fl & 0x400))
                        nj_matrix_rotate_cur_ang3(node->rot, fl & 0x20);
                }
            }
            if (!(fl & 4))
                njScaleEx(node->scl);
            if (!(fl & 8) && node->d4)
                model_obj_draw(node->d4, rec);
            if (!(fl & 0x10) && node->child)
                model_list_draw(node->child, rec->next);
            njPopMatrixEx();
        }
        node = node->next;
        rec = rec->next;
    } while (node);
}

/* Matrix upload, VU1 line program start, packet data linked in. Counts in draw_poly_count. */
/* 0x0017b0d0 */
void model_packet_draw_normal(PktObj *obj)
{
    void *pkt;
    void *dl;
    char *gx = ulGxState;

    if (obj->data != 0) {
        ulGxSetMatrix(0, ulMatrixCur);
        pkt = psGsGetDisplayList();
        dl = psDisplayListOpenRefPkt(pkt);
        psVifPktAddWord(dl, 0x1000101);
        psVifPktAddWord(dl, 0x6c040059);
        psVifPktAddWords(dl, gx + 320, 16);
        psVifPktAddWord(dl, 0x1500001e);
        psDisplayListCloseRefPkt(pkt);
        psDisplayListAddRefPkt(pkt, obj->data, obj->size >> 4);
        draw_poly_count += obj->words;
    }
}

/* VU1's simpler line program. */
/* 0x0017b1a8 */
void model_packet_draw_simple(PktObj *obj)
{
    void *pkt;
    void *dl;
    char *gx = ulGxState;

    if (obj->data != 0) {
        ulGxSetMatrix(0, ulMatrixCur);
        pkt = psGsGetDisplayList();
        dl = psDisplayListOpenRefPkt(pkt);
        psVifPktAddWord(dl, 0x1000101);
        psVifPktAddWord(dl, 0x6c040059);
        psVifPktAddWords(dl, gx + 320, 16);
        psVifPktAddWord(dl, 0x1500001c);
        psDisplayListCloseRefPkt(pkt);
        psDisplayListAddRefPkt(pkt, obj->data, obj->size >> 4);
        draw_poly_count += obj->words;
    }
}

/* Simple with draw flag 1. */
/* 0x0017b280 */
static void model_packet_draw(void *obj)
{
    if (ulGxGetDrawFlags(obj) & 1)
        model_packet_draw_simple(obj);
    else
        model_packet_draw_normal(obj);
}

/* pkt walks the packet tree alongside; flags skip transforms; flag 0x2000 ends the sibling list. */
/* 0x0017b2c0 */
static void model_tree_draw(RezWireCompileDNode *node, RezWireCompileDCtx *pkt)
{
    unsigned int fl;

    do {
        fl = node->flags;
        if (fl & 0x2000)
            return;
        if (fl & 0x800)
            njGetMatrix(model_tree_work);
        if ((fl & 7) == 7) {
            if (!(fl & 8) && node->d4 && pkt->d4)
                model_packet_draw(pkt);
            if (!(fl & 0x10))
                model_tree_draw(node->child, pkt->child);
        } else {
            njPushMatrixEx();
            if (!(fl & 1))
                njTranslateEx(node->pos);
            if (!(fl & 2)) {
                if (!(fl & 0x1000)) {
                    if (!(fl & 0x400))
                        nj_matrix_rotate_cur_ang3(node->rot, fl & 0x20);
                }
            }
            if (!(fl & 4))
                njScaleEx(node->scl);
            if (!(fl & 8) && node->d4 && pkt->d4)
                model_packet_draw(pkt);
            if (!(fl & 0x10) && node->child)
                model_tree_draw(node->child, pkt->child);
            njPopMatrixEx();
        }
        node = node->next;
        pkt = pkt->next;
    } while (node);
}

/* Debug: line count and packet size per node. */
/* 0x0017b440 */
static void wire_tree_dump(DumpNode *node)
{
    for (; node != 0; node = node->next) {
        printf("nbLines:%4d, pktSize:%04x\n", node->lines, node->size);
        if (node->child != 0)
            wire_tree_dump(node->child);
    }
}

/* In place, walking the mesh list (*cur) alongside. */
/* 0x0017b4a0 */
static void wire_tree_write(TNode *hdr, TCtx *node, int **cur)
{
    TNode *next;

    do {
        if (hdr->data != 0)
            wire_packet_write(node->dst, (char *)*cur + 4, hdr->data);
        if (hdr->child != 0) {
            *cur = (int *)**cur;
            wire_tree_write(hdr->child, node->child, cur);
        }
        next = hdr->next;
        if (next == 0)
            break;
        *cur = (int *)**cur;
        node = node->next;
        hdr = next;
    } while (1);
}

/* In place, for model tree root and mesh list list. */
/* 0x0017b538 */
void wire_write(int wire, int root, int list)
{
    wire_tree_write(wire, root, &list);
}

/* 6 words per line, 7 per chunk of up to 25, plus 3, rounded up to quadwords; 0 for no lines. */
/* 0x0017b558 */
static int wire_packet_size(int *pkt)
{
    int n = *pkt;
    int words;
    int chunk;

    if (n == 0)
        return 0;
    words = 1;
    while (n > 0) {
        chunk = n < 26 ? n : 25;
        words += 6;
        words += chunk * 6;
        n -= chunk;
        words += 1;
    }
    words += 2;
    while ((words & 3) != 0)
        words++;
    return words * 4;
}

/* A 32-byte header per entry plus every entry's packet but the last. */
/* 0x0017b5e0 */
int wire_tree_packet_size(SNode *ent)
{
    int total = 0;

    for (; ent != 0; ent = ent->next) {
        total += 32;
        if (ent->next != 0)
            total += wire_packet_size(&ent->count);
    }
    return total;
}

/* Via the free hook; nothing for 0. */
/* 0x0017b640 */
void wire_free(int block)
{
    if (block != 0)
        wire_free_func(block);
}

/*
 * Per chunk of up to 25 lines: count, both ends (vertex list via the mesh's index pairs), program
 * start. Returns words, rounded to quadwords (as wire_packet_size).
 */
/* 0x0017b668 */
int wire_packet_write(int *obj, WireMesh *mesh, unsigned int *out)
{
    WireSrc src;
    unsigned short *ip;
    int n;
    int words;
    int chunk;
    int k;
    int *va;
    int *vb;
    unsigned int *dst;
    unsigned int *hdr;
    int i;

    if (*obj != 0)
        wire_cmd_find_vtx(*obj, (int * *)&src, (int *)&src.kind);
    ip = mesh->idx;
    dst = out;
    *dst = 0x1000104;
    dst++;
    words = 1;
    n = mesh->count;
    if (n > 0) {
        hdr = dst;
        i = 1;
        do {
            chunk = n < 26 ? n : 25;
            dst[0] = 0x6c018000;
            hdr[i] = chunk;
            dst[5] = (chunk << 17) | 0x68008004;
            dst += 6;
            i += 6;
            for (k = 0; k < chunk; k++) {
                va = src.data + ip[0] * src.kind;
                vb = src.data + ip[1] * src.kind;
                hdr[i - 1] = va[0];
                i++;
                hdr[i - 1] = va[1];
                i++;
                hdr[i - 1] = va[2];
                i++;
                hdr[i - 1] = vb[0];
                i++;
                hdr[i - 1] = vb[1];
                i++;
                hdr[i - 1] = vb[2];
                i++;
                ip += 2;
                dst += 6;
            }
            hdr[i - 1] = 0x17000000;
            i++;
            dst++;
            words += 7;
            n -= chunk;
            words += chunk * 6;
        } while (n > 0);
    }
    dst[0] = 0x1000101;
    dst[1] = 0x10000000;
    words += 2;
    while ((words & 3) != 0)
        words++;
    return words;
}

/* Last chunk's vertices and stride in words: types 34, 35, 41, 42 -> 3, 4, 6, 7; else none. */
/* 0x0017b830 */
static void wire_cmd_find_vtx(int *rec, int **out_ptr, int *out_kind)
{
    int head;
    unsigned int n;

    while ((head = *rec) != 255) {
        n = (unsigned int)head >> 16;
        head &= 0xff;
        switch (head) {
        case 34:
            *out_kind = 3;
            *out_ptr = rec + 2;
            break;
        case 41:
            *out_kind = 6;
            *out_ptr = rec + 2;
            break;
        case 35:
            *out_kind = 4;
            *out_ptr = rec + 2;
            break;
        case 42:
            *out_kind = 7;
            *out_ptr = rec + 2;
            break;
        default:
            *out_ptr = 0;
            *out_kind = 0;
            break;
        }
        {
            int sz = n * 4 + 4;
            rec = (int *)((char *)rec + sz);
        }
    }
}

/* base + *off must be 16-byte aligned; *off moves past it; size in words -> *count. */
/* 0x0017b8f8 */
static char *wire_block_pack(void *obj, void *mesh, char *base, int *off, int *count)
{
    char *dst;
    int n;

    *count = 0;
    dst = base + *off;
    if ((unsigned int)dst & 0xf) {
        printf("packet pointer is not 16 byte align! [%08x(%08x)]\n", dst, *off);
        __assert("rez_wire_compile.c", 561, str_wire_assert_zero);
    }
    n = wire_packet_write(obj, mesh, dst);
    *count = n;
    *off += n * 4;
    return dst;
}

/* Header (line count, packet, size), then children and siblings. 0 when the mesh list ran out. */
/* 0x0017b9b0 */
static WHdr *wire_tree_pack(WNode *node, WEnt **cursor, char *base, int *off)
{
    WEnt *ent = *cursor;
    WHdr *hdr;
    int cnt;

    if (ent == 0)
        return 0;
    if (ent->next == 0)
        return 0;
    hdr = (WHdr *)(base + *off);
    *off += 32;
    hdr->lines = ent->lines;
    if (node->data != 0) {
        hdr->blk = wire_block_pack(node->data, &ent->lines, base, off, &cnt);
        hdr->size = (cnt * 4 + 15) & 0xfffffff0u;
    } else {
        hdr->blk = 0;
        hdr->size = 0;
    }
    if (node->child != 0) {
        *cursor = (*cursor)->next;
        hdr->child = (char *)wire_tree_pack(node->child, cursor, base, off);
    }
    if (node->next != 0) {
        *cursor = (*cursor)->next;
        hdr->next = (char *)wire_tree_pack(node->next, cursor, base, off);
    }
    return hdr;
}

/* One buffer via the alloc hook; 0 for model flag 0x2000 or out of memory. */
/* 0x0017bad0 */
void *wire_compile(WireObj *obj, void *list)
{
    void *cursor;
    int off;
    int size;
    void *buf;

    if (obj == 0)
        __assert("rez_wire_compile.c", 629, str_wire_object);
    if (obj->flags & 0x2000)
        return 0;
    cursor = list;
    size = wire_tree_packet_size(list);
    buf = wire_alloc_func(size);
    if (buf != 0) {
        memset(buf, 0, size);
        off = 0;
        wire_tree_pack(obj, &cursor, buf, &off);
    }
    return buf;
}

/* 0x0017bb80 */
void wire_alloc_set(int alloc_fn, int free_fn)
{
    wire_alloc_func = alloc_fn;
    wire_free_func = free_fn;
}
