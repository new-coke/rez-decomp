/*
 * Immediate 2D/3D primitive batches for effects and overlays: sprites and strips, as VU1 vertex
 * packets or EE-built GIF packets. Each kind has a Begin/Add/End triple.
 */

#include "types_ul.h"
#include "ul_gx_prim.h"
#include "ps_displist.h"
#include "ps_gs.h"
#include "ps_pkt.h"

extern UlGxBig ulGxState;
extern unsigned int ulGxBatchGifTag[];
extern int ulGxSpriteCount;
extern void *vu0_gx_sprite_vertices(UlGxBig *, void *, int, void *);

/*
 * Unpacks for GIF tag, positions, colours and (with st) UVs, then the microprogram resume. Area
 * starts via tag/xyz/col/st; returns the length in quadwords.
 */
/* 0x001beeb8 */
int ulGxBeginVertexPkt(VifBuf *buf, int n, unsigned int **tag, unsigned int **xyz, unsigned int **col,
                  unsigned int **st)
{
    unsigned int num_bits = n << 16;
    unsigned int *p = buf->buf;
    int len;

    *p++ = 0x6c018000;
    *tag = p;
    p += 4;
    *p = num_bits | 0x68008004;
    p++;
    *xyz = p;
    p = (unsigned int *)((char *)p + n * 12);
    *p = num_bits | 0x6e008007;
    p++;
    *col = p;
    {
        int sub_len = n * 3 + 7;
        p += n;
        len = sub_len + n;
    }
    if (st) {
        int sub_len;
        *p = num_bits | 0x64008006;
        p++;
        *st = p;
        p += n * 2;
        sub_len = len + 1;
        len = sub_len + n * 2;
    }
    *p = 0x17000000;
    return len + 1;
}

/* VU1 sprite program; flag bit 0 = alpha blend. Sends pending GS state, starts the microprogram. */
/* 0x001bef68 */
void ulGxBeginSprite(int flag)
{
    UlGxBig *gx = &ulGxState;
    void *dev;
    void *buf;
    int type_bits;
    int tex_bits;

    gx->prim_regs = 1042;
    tex_bits = gx->prim2d | 0x10;
    type_bits = ((flag & 1) << 6) | 6;
    gx->prim_tag = ((unsigned long)(tex_bits | type_bits) << 47) | 0x3000400000008000UL;
    dev = psGsGetDisplayList();
    buf = psDisplayListOpenRefPkt(dev);
    if (psDisplayListIsDirty(dev)) {
        psVifPktOpenGifPkt(buf);
        psDisplayListFlushGsRegs(dev, buf);
        psVifPktCloseGifPkt(buf);
    }
    psVifPktAddWord(buf, 0x15000004);
    psVifPktAddWord(buf, 0x1000104);
}

/* Six words per vertex: position (3), UV (2), colour (1). */
/* 0x001bf030 */
void ulGxAddSprites(unsigned int *verts, int n)
{
    GxWord *area[4];
    GxPkt *buf;
    int len;
    int i;
    unsigned int *tag;

    buf = psDisplayListGetVifPkt(psGsGetDisplayList());
    len = ulGxBeginVertexPkt(buf, n, &area[0], &area[1], &area[2], &area[3]);
    for (i = 0; i < n; i++) {
        (area[1]++)->u = verts[0];
        (area[1]++)->u = verts[1];
        (area[1]++)->u = verts[2];
        (area[3]++)->u = verts[3];
        (area[3]++)->u = verts[4];
        (area[2]++)->u = verts[5];
        verts += 6;
    }
    tag = ulGxBatchGifTag;
    (area[0]++)->u = *tag++ | n;
    (area[0]++)->u = *tag++;
    (area[0]++)->u = *tag++;
    (area[0]++)->u = *tag;
    buf->cur += len * 4;
    buf->cnt += len;
}

/* Waits for the microprogram. */
/* 0x001bf1a0 */
void ulGxEndSprite(void)
{
    void *dev;
    void *buf;

    dev = psGsGetDisplayList();
    buf = psDisplayListGetVifPkt(dev);
    psVifPktAddWord(buf, 0x11000000);
    psVifPktAddWord(buf, 0x1000101);
    psDisplayListCloseRefPkt(dev);
}

/* Built as one GIF packet; flag bit 0 = alpha blend. ulGxEndSpriteGif fills in the count. */
/* 0x001bf200 */
void ulGxBeginSpriteGif(int flag)
{
    UlGxBig *gx = &ulGxState;
    void *dev;
    void *buf;

    gx->prim_tag = ((((unsigned long)(flag & 1) << 6) | 22) << 47) | (0xc001UL << 46);
    gx->prim_regs = 1042;
    dev = psGsGetDisplayList();
    buf = psDisplayListOpenRefPkt(dev);
    psVifPktOpenGifPkt(buf);
    if (psDisplayListIsDirty(dev))
        psDisplayListFlushGsRegs(dev, buf);
    psVifPktAddGifTag(buf, &gx->prim_tag);
    ulGxSpriteCount = 0;
}

/* Converted by vu0_gx_sprite_vertices. */
/* 0x001bf2a8 */
void ulGxAddSpritesGif(void *list, int n)
{
    UlGxBig *gx = &ulGxState;
    UlGxBuf *buf;
    void *pkt_end;

    buf = psDisplayListGetVifPkt(psGsGetDisplayList());
    pkt_end = vu0_gx_sprite_vertices(gx, list, n, buf->cur);
    buf->cur = pkt_end;
    buf->nqw += n * 3;
    ulGxSpriteCount += n;
}

/* Stores the sprite count in the GIF tag. */
/* 0x001bf338 */
void ulGxEndSpriteGif(void)
{
    void *dev;
    UlGxBuf *buf;

    dev = psGsGetDisplayList();
    buf = psDisplayListGetVifPkt(dev);
    *buf->gtag = (*buf->gtag & ~0x7fffUL) | (ulGxSpriteCount & 0x7fff);
    psVifPktCountGifQwords(buf);
    psVifPktCloseGifPkt(buf);
    psDisplayListCloseRefPkt(dev);
}

/* Untextured, VU1 strip program; as ulGxBeginSprite. */
/* 0x001bf3a8 */
void ulGxBeginStrip(int flag)
{
    UlGxBig *gx = &ulGxState;
    void *dev;
    void *buf;
    int type_bits;

    gx->prim_regs = 1042;
    type_bits = ((flag & 1) << 6) | 4;
    gx->prim_tag = ((unsigned long)(gx->prim2d | type_bits) << 47) | 0x3000400000008000UL;
    dev = psGsGetDisplayList();
    buf = psDisplayListOpenRefPkt(dev);
    if (psDisplayListIsDirty(dev)) {
        psVifPktOpenGifPkt(buf);
        psDisplayListFlushGsRegs(dev, buf);
        psVifPktCloseGifPkt(buf);
    }
    psVifPktAddWord(buf, 0x1500000c);
    psVifPktAddWord(buf, 0x1000104);
}

/* Four words per vertex: position (3), colour (1). */
/* 0x001bf468 */
void ulGxAddStrip(unsigned int *verts, int n)
{
    GxWord *area[3];
    GxPkt *buf;
    int len;
    int i;
    unsigned int *tag;

    buf = psDisplayListGetVifPkt(psGsGetDisplayList());
    len = ulGxBeginVertexPkt(buf, n, &area[0], &area[1], &area[2], 0);
    for (i = 0; i < n; i++) {
        (area[1]++)->u = verts[0];
        (area[1]++)->u = verts[1];
        (area[1]++)->u = verts[2];
        (area[2]++)->u = verts[3];
        verts += 4;
    }
    tag = ulGxBatchGifTag;
    (area[0]++)->u = *tag++ | n;
    (area[0]++)->u = *tag++;
    (area[0]++)->u = *tag++;
    (area[0]++)->u = *tag;
    buf->cur += len * 4;
    buf->cnt += len;
}

/* Waits for the microprogram. */
/* 0x001bf5b8 */
void ulGxEndStrip(void)
{
    void *dev;
    void *buf;

    dev = psGsGetDisplayList();
    buf = psDisplayListGetVifPkt(dev);
    psVifPktAddWord(buf, 0x10000000);
    psVifPktAddWord(buf, 0x1000101);
    psDisplayListCloseRefPkt(dev);
}

/* flag bit 0 = alpha blend; sends pending GS state. */
/* 0x001bf618 */
static void ulGxBeginGouraudGif(int flag)
{
    UlGxBig *gx = &ulGxState;
    void *dev;
    void *buf;

    gx->prim_tag = ((((unsigned long)(flag & 1) << 6) | 12) << 47) | (0xc001UL << 46);
    gx->prim_regs = 1042;
    dev = psGsGetDisplayList();
    buf = psDisplayListOpenRefPkt(dev);
    psVifPktOpenGifPkt(buf);
    if (psDisplayListIsDirty(dev))
        psDisplayListFlushGsRegs(dev, buf);
}

/*
 * One strip; 16-byte vertices: x, y, z floats, RGBA in the last 4 bytes; mapped with ulGxState
 * scale/offset.
 */
/* 0x001bf6b0 */
static void ulGxAddGouraudGif(char *verts, int n)
{
    UlGxBig *gx = &ulGxState;
    UlGxBuf *buf;
    unsigned long tag[2];
    unsigned long *pkt;
    GxW *out;
    int i;

    buf = psDisplayListGetVifPkt(psGsGetDisplayList());
    tag[1] = gx->prim_regs;
    tag[0] = n | gx->prim_tag;
    pkt = psVifPktAddGifTag(buf, tag);
    out = (GxW *)(pkt + 2);
    for (i = 0; i < n; i++) {
        out[2].f = ((float *)verts)[2];
        out[3].i = (unsigned char)verts[15];
        out[2].i = (unsigned char)verts[14];
        out[1].i = (unsigned char)verts[13];
        out[0].i = (unsigned char)verts[12];
        out += 4;
        out[0].i = (int)((((float *)verts)[0] * gx->scale_x + gx->offset_x) * 16.0f);
        out[1].i = (int)((((float *)verts)[1] * gx->scale_y + gx->offset_y) * 16.0f);
        out[2].i = (int)(((float *)verts)[2] * gx->scale_z * 16.0f);
        out[3].i = 0;
        verts += 16;
        out += 4;
    }
    buf->cur = (int *)out;
    buf->nqw += n * 3;
    psVifPktCountGifQwords(buf);
}

/* 0x001bf800 */
static void ulGxEndGouraudGif(void)
{
    void *dev;

    dev = psGsGetDisplayList();
    psVifPktCloseGifPkt(psDisplayListGetVifPkt(dev));
    psDisplayListCloseRefPkt(dev);
}

/* Textured, VU1 strip program; as ulGxBeginSprite. */
/* 0x001bf840 */
void ulGxBeginTexStrip(int flag)
{
    UlGxBig *gx = &ulGxState;
    void *dev;
    void *buf;
    int type_bits;
    int tex_bits;

    gx->prim_regs = 1042;
    tex_bits = gx->prim2d | 0x10;
    type_bits = ((flag & 1) << 6) | 4;
    gx->prim_tag = ((unsigned long)(tex_bits | type_bits) << 47) | 0x3000400000008000UL;
    dev = psGsGetDisplayList();
    buf = psDisplayListOpenRefPkt(dev);
    if (psDisplayListIsDirty(dev)) {
        psVifPktOpenGifPkt(buf);
        psDisplayListFlushGsRegs(dev, buf);
        psVifPktCloseGifPkt(buf);
    }
    psVifPktAddWord(buf, 0x1500000c);
    psVifPktAddWord(buf, 0x1000104);
}

/* Vertices as in ulGxAddSprites. */
/* 0x001bf908 */
void ulGxAddTexStrip(unsigned int *verts, int n)
{
    GxWord *area[4];
    GxPkt *buf;
    int len;
    int i;
    unsigned int *tag;

    buf = psDisplayListGetVifPkt(psGsGetDisplayList());
    len = ulGxBeginVertexPkt(buf, n, &area[0], &area[1], &area[2], &area[3]);
    for (i = 0; i < n; i++) {
        (area[1]++)->u = verts[0];
        (area[1]++)->u = verts[1];
        (area[1]++)->u = verts[2];
        (area[3]++)->u = verts[3];
        (area[3]++)->u = verts[4];
        (area[2]++)->u = verts[5];
        verts += 6;
    }
    tag = ulGxBatchGifTag;
    (area[0]++)->u = *tag++ | n;
    (area[0]++)->u = *tag++;
    (area[0]++)->u = *tag++;
    (area[0]++)->u = *tag;
    buf->cur += len * 4;
    buf->cnt += len;
}

/* 0x001bfa78 */
void ulGxEndTexStrip(void)
{
    void *dev;
    void *buf;

    dev = psGsGetDisplayList();
    buf = psDisplayListGetVifPkt(dev);
    psVifPktAddWord(buf, 0x1000101);
    psVifPktAddWord(buf, 0x10000000);
    psDisplayListCloseRefPkt(dev);
}

/* As ulGxBeginGouraudGif. */
/* 0x001bfad8 */
void ulGxBeginTexGouraudGif(int flag)
{
    UlGxBig *gx = &ulGxState;
    void *dev;
    void *buf;

    gx->prim_tag = ((((unsigned long)(flag & 1) << 6) | 28) << 47) | (0xc001UL << 46);
    gx->prim_regs = 1042;
    dev = psGsGetDisplayList();
    buf = psDisplayListOpenRefPkt(dev);
    psVifPktOpenGifPkt(buf);
    if (psDisplayListIsDirty(dev))
        psDisplayListFlushGsRegs(dev, buf);
}

/*
 * One strip; 24-byte vertices x, y, z, s, t, RGBA. s, t scaled by z (z is q, perspective-correct).
 */
/* 0x001bfb70 */
void ulGxAddTexGouraudGif(char *verts, int n)
{
    UlGxBig *gx = &ulGxState;
    UlGxBuf *buf;
    unsigned long tag[2];
    unsigned long *pkt;
    GxW *out;
    int i;

    tag[0] = n | gx->prim_tag;
    tag[1] = gx->prim_regs;
    buf = psDisplayListGetVifPkt(psGsGetDisplayList());
    pkt = psVifPktAddGifTag(buf, tag);
    out = (GxW *)(pkt + 2);
    for (i = 0; i < n; i++) {
        out[0].f = ((float *)verts)[3] * ((float *)verts)[2];
        out[1].f = ((float *)verts)[4] * ((float *)verts)[2];
        out[2].f = ((float *)verts)[2];
        out += 4;
        out[3].i = (unsigned char)verts[23];
        out[2].i = (unsigned char)verts[22];
        out[1].i = (unsigned char)verts[21];
        out[0].i = (unsigned char)verts[20];
        out += 4;
        out[0].i = (int)((((float *)verts)[0] * gx->scale_x + gx->offset_x) * 16.0f);
        out[1].i = (int)((((float *)verts)[1] * gx->scale_y + gx->offset_y) * 16.0f);
        out[2].i = (int)(((float *)verts)[2] * gx->scale_z * 16.0f);
        out[3].i = 0;
        verts += 24;
        out += 4;
    }
    buf->cur = (int *)out;
    buf->nqw += n * 3;
    psVifPktCountGifQwords(buf);
}

/* 0x001bfce8 */
void ulGxEndTexGouraudGif(void)
{
    void *dev;

    dev = psGsGetDisplayList();
    psVifPktCloseGifPkt(psDisplayListGetVifPkt(dev));
    psDisplayListCloseRefPkt(dev);
}
