/* Image and texture upload: GS host-to-local transfers into a DMA packet or display list. */

#include "types_nj.h"
#include "ninja.h"
#include "ps_image.h"
#include "ps_displist.h"
#include "ps_gs.h"
#include "ps_gxpkt.h"
#include "ps_pkt.h"

extern NjGxPkt *psGxPktCur;

/* BITBLTBUF, TRXPOS, TRXREG, TRXDIR and an image GIF tag, then a ref to size bytes at src. */
/* steer(cast) */
/* 0x001b2c30 */
static void psImageDlAddRegs(unsigned long bitbltbuf, unsigned long trxpos, unsigned long trxreg, void *src, unsigned int size)
{
    void *dl, *pkt;
    unsigned short *code;
    unsigned long tag[2], img[2];

    dl = psGsGetDisplayList();
    size >>= 4;
    pkt = psDisplayListOpenRefPkt(dl);
    code = psVifPktOpenGifPkt(pkt);
    tag[0] = 0x1000000000000004UL;
    tag[1] = 14;
    psVifPktAddGifTag(pkt, tag);
    psVifPktAddRegAD(pkt, 80, bitbltbuf);
    psVifPktAddRegAD(pkt, 81, trxpos);
    psVifPktAddRegAD(pkt, 82, trxreg);
    psVifPktAddRegAD(pkt, 83, 0);
    psVifPktCountGifQwords(pkt);
    img[0] = (unsigned long)size | 2L << 58;
    img[1] = 0;
    psVifPktAdvanceGif(pkt, ((unsigned long *(*)(void *, unsigned long *))psVifPktAddGifTag)(pkt, img) + 2, 0);
    psVifPktCloseGifPkt(pkt);
    *code += size;
    psDisplayListCloseRefPkt(dl);
    psDisplayListAddRefPkt(dl, src, size);
}

/* BITBLTBUF, TRXPOS, TRXREG, TRXDIR between two TEXFLUSHes, one A+D packet. */
/* 0x001b2d90 */
void psImageDlAddTransferRegs(unsigned long bitbltbuf, unsigned long trxpos, unsigned long trxreg, unsigned long trxdir)
{
    void *dl, *pkt;
    unsigned long tag[2];

    dl = psGsGetDisplayList();
    pkt = psDisplayListOpenRefPkt(dl);
    psVifPktOpenGifPkt(pkt);
    tag[0] = 0x1000000000000006UL;
    tag[1] = 14;
    psVifPktAddGifTag(pkt, tag);
    psVifPktAddRegAD(pkt, 63, 0);
    psVifPktAddRegAD(pkt, 80, bitbltbuf);
    psVifPktAddRegAD(pkt, 81, trxpos);
    psVifPktAddRegAD(pkt, 82, trxreg);
    psVifPktAddRegAD(pkt, 83, trxdir);
    psVifPktAddRegAD(pkt, 63, 0);
    psVifPktCountGifQwords(pkt);
    psVifPktCloseGifPkt(pkt);
    psDisplayListCloseRefPkt(dl);
}

/* 0x001b2ea0 */
static void psImageNop(void)
{
}

/* 0x001b2ea8 */
static void psImageNop2(void)
{
}

/* DMA cnt tag with transfer registers and image GIF tag, then a ref tag for size words at src. */
/* 0x001b2eb0 */
static void psImagePktAddImage(NjGxPkt *pkt, int width, int height, int psm, int dbp, int size, int src)
{
    NjGxTag img, cnt, ref;
    int bw, qwc;
    unsigned long bitbltbuf, trxreg;

    bw = width / 64 ? width / 64 : 1;
    bitbltbuf = (unsigned long)(dbp / 64) << 32 | (unsigned long)bw << 48 | (unsigned long)psm << 56;
    qwc = size / 4;
    trxreg = width | (unsigned long)height << 32;
    cnt.lo = 0x10000006;
    img.lo = (qwc | 0x8000) | 2L << 58;
    ref.lo = (qwc | 0x30000000) | (unsigned long)src << 32;
    psGxPktOpenTag(pkt, &cnt);
    psGxPktAddRegADGifTag(pkt, 4);
    psGxPktAddRegAD(pkt, 80, bitbltbuf);
    psGxPktAddRegAD(pkt, 81, 0);
    psGxPktAddRegAD(pkt, 82, trxreg);
    psGxPktAddRegAD(pkt, 83, 0);
    psGxPktAddGifTag(pkt, &img);
    psGxPktOpenTag(pkt, &ref);
}

/* Bytes: 256 entries for psm 19 (8 bit), 16 for 20 (4 bit); 32 or 16 bits for cpsm 0 or 2. */
/* 0x001b2fd8 */
static int psImageClutSize(int psm, int cpsm)
{
    int n, bits;
    int unused[4];              /* never used; sets the stack frame size */

    switch (psm) {
    case 19:
        n = 256;
        break;
    case 20:
        n = 16;
        break;
    default:
        n = 0;
        break;
    }
    switch (cpsm) {
    case 2:
        bits = 16;
        break;
    case 0:
        bits = 32;
        break;
    default:
        bits = 0;
        break;
    }
    return (unsigned int)(n * bits) >> 3;
}

/* Into psGxPktCur: every mip level, then the clut for formats 19/20; consecutive from src. */
/* 0x001b3030 */
void psImagePktAddTexture(NjGsTex *tex, int src)
{
    NjGxPkt *pkt;
    int i, width, height, psm;

    pkt = psGxPktCur;
    psm = tex->psm;
    width = tex->w;
    height = tex->h;
    for (i = 0; i <= tex->nmip; i++) {
        psImagePktAddImage(pkt, width, height, psm, tex->dbp[i], tex->size[i], src);
        width >>= 1;
        height >>= 1;
        src += tex->size[i] * 4;
    }
    switch (psm) {
    case 19:
        psImagePktAddImage(pkt, 16, 16, tex->cpsm, tex->cbp, psImageClutSize(19, tex->cpsm) / 4, src);
        break;
    case 20:
        psImagePktAddImage(pkt, 8, 2, tex->cpsm, tex->cbp, psImageClutSize(20, tex->cpsm) / 4, src);
        break;
    }
}

/* Display-list psImagePktAddImage; gx_pkt is unused. */
/* steer(cast) */
/* 0x001b31f8 */
void psImageDlAddImage(NjGxPkt *gx_pkt, int width, int height, int psm, int dbp, int size, void *src)
{
    void *dl, *pkt;
    unsigned short *code;
    int bw, qwc;
    unsigned long tag[2], img[2], bitbltbuf, trxreg;

    qwc = size / 4;
    img[0] = qwc | 2L << 58;
    img[1] = 0;
    dl = psGsGetDisplayList();
    pkt = psDisplayListOpenRefPkt(dl);
    code = psVifPktOpenGifPkt(pkt);
    bw = width / 64 ? width / 64 : 1;
    bitbltbuf = (unsigned long)(dbp / 64) << 32 | (unsigned long)bw << 48 | (unsigned long)psm << 56;
    trxreg = width | (unsigned long)height << 32;
    tag[0] = 0x1000000000000004UL;
    tag[1] = 14;
    psVifPktAddGifTag(pkt, tag);
    psVifPktAddRegAD(pkt, 80, bitbltbuf);
    psVifPktAddRegAD(pkt, 81, 0);
    psVifPktAddRegAD(pkt, 82, trxreg);
    psVifPktAddRegAD(pkt, 83, 0);
    psVifPktCountGifQwords(pkt);
    psVifPktAdvanceGif(pkt, ((unsigned long *(*)(void *, unsigned long *))psVifPktAddGifTag)(pkt, img) + 2, 0);
    psVifPktCloseGifPkt(pkt);
    *code += qwc;
    psDisplayListCloseRefPkt(dl);
    psDisplayListAddRefPkt(dl, src, qwc);
}

/* Display-list psImagePktAddTexture. */
/* 0x001b33b0 */
void psImageDlAddTexture(NjGsTex *tex, int src)
{
    NjGxPkt *pkt;
    int i, width, height, psm;

    pkt = psGxPktCur;
    psm = tex->psm;
    width = tex->w;
    height = tex->h;
    for (i = 0; i <= tex->nmip; i++) {
        psImageDlAddImage(pkt, width, height, psm, tex->dbp[i], tex->size[i], src);
        width >>= 1;
        height >>= 1;
        src += tex->size[i] * 4;
    }
    switch (psm) {
    case 19:
        psImageDlAddImage(pkt, 16, 16, tex->cpsm, tex->cbp, psImageClutSize(19, tex->cpsm) / 4, src);
        break;
    case 20:
        psImageDlAddImage(pkt, 8, 2, tex->cpsm, tex->cbp, psImageClutSize(20, tex->cpsm) / 4, src);
        break;
    }
}
