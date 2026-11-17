/* VIF packet writer: VIF streams with GIF packets and UNPACK blocks, and the DMA packet buffers. */

#include "types_ps.h"
#include "libc.h"
#include "ps_pkt.h"

typedef union PsAdQword {
    unsigned long ul[2];        /* 0 data, 8 register address */
    int i[4];
} PsAdQword;

/* After n quadwords written up to cur: counts them since the open tag, 4 words each. */
/* 0x001b3c40 */
static void psVifPktAdvance(PsVifWr *pkt, int *cur, int n)
{
    pkt->cur = cur;
    pkt->nreg += n;
    pkt->nwords += pkt->nreg * 4;
}

/* 4 words per quadword since the open tag. */
/* 0x001b3c68 */
static void psVifPktCountQwords(PsVifWr *pkt)
{
    pkt->nwords += pkt->nreg * 4;
}

/* Unlike psVifPktAddGifTag, does not count the tag quadword in the GIF packet. */
/* 0x001b3c80 */
static PsGifTag *psVifPktPutGifTag(PsVifWr *pkt, PsGifTag *tag)
{
    *(PsGifTag *)pkt->cur = *tag;
    pkt->nreg = 0;
    pkt->gtag = (PsGifTag *)pkt->cur;
    pkt->cur += 4;
    return pkt->gtag;
}

/* Code in the first word of a quadword (other three untouched); returns the code word. */
/* 0x001b3cb0 */
int *psVifPktAddVifCode(PsVifWr *pkt, long code)
{
    int *p;

    p = pkt->cur;
    *p = code;
    pkt->cur = p + 4;
    pkt->nwords += 4;
    return p;
}

/* Data, then GS register address. */
/* 0x001b3cd8 */
void psVifPktAddRegAD(PsVifWr *pkt, unsigned long addr, unsigned long data)
{
    PsAdQword *qw = (PsAdQword *)pkt->cur;

    qw->ul[1] = addr;
    qw->ul[0] = data;
    pkt->cur += 4;
    pkt->nreg++;
}

/* 0x001b3d00 */
void psVifPktAddGsReg(PsVifWr *pkt, PsGsReg *reg)
{
    *(PsGsReg *)pkt->cur = *reg;
    pkt->cur += 4;
    pkt->nreg++;
}

/* Ends a block of n quadwords written up to end. */
/* 0x001b3d30 */
void psVifPktAdvanceGif(PsVifWr *pkt, int *end, int n)
{
    pkt->cur = end;
    pkt->nreg += n;
    psVifPktCountGifQwords(pkt);
}

/* 0x001b3d48 */
void psVifPktCountGifQwords(PsVifWr *pkt)
{
    pkt->nqw += pkt->nreg;
}

/* 0x001b3d60 */
static void psVifPktStub(void)
{
}

/* Becomes the open GIF tag. */
/* 0x001b3d68 */
PsGifTag *psVifPktAddGifTag(PsVifWr *pkt, PsGifTag *tag)
{
    *(PsGifTag *)pkt->cur = *tag;
    pkt->nreg = 0;
    pkt->gtag = (PsGifTag *)pkt->cur;
    pkt->cur += 4;
    pkt->nqw++;
    return pkt->gtag;
}

/* Sets EOP on the open tag and the size in the DIRECTHL; without a tag the DIRECTHL is removed. */
/* 0x001b3da8 */
void psVifPktCloseGifPkt(PsVifWr *pkt)
{
    if (pkt->gtag) {
        pkt->gtag->eop = 1;
        ((PsVifCode *)pkt->last)->imm = pkt->nqw;
        pkt->nwords += pkt->nqw * 4;
    } else {
        pkt->cur = pkt->last;
        pkt->nwords--;
    }
}

/*
 * Pads with zeros and FLUSHE so DIRECTHL ends a quadword and GIF data is aligned; with two
 * words already in the quadword, writes DIRECTHL only.
 */
/* 0x001b3e00 */
int *psVifPktOpenGifPkt(PsVifWr *pkt)
{
    switch (pkt->nwords & 3) {
    case 0:
        *pkt->cur++ = 0;
        *pkt->cur++ = 0;
        *pkt->cur++ = 0x10000000;
        pkt->last = pkt->cur;
        *pkt->cur++ = 0x51000000;
        pkt->nwords += 4;
        break;
    case 1:
        *pkt->cur++ = 0;
        *pkt->cur++ = 0x10000000;
        pkt->last = pkt->cur;
        *pkt->cur++ = 0x51000000;
        pkt->nwords += 3;
        break;
    case 2:
        pkt->last = pkt->cur;
        *pkt->cur++ = 0x51000000;
        pkt->nwords += 2;
        break;
    case 3:
        *pkt->cur++ = 0;
        *pkt->cur++ = 0;
        *pkt->cur++ = 0;
        *pkt->cur++ = 0x10000000;
        pkt->last = pkt->cur;
        *pkt->cur++ = 0x51000000;
        pkt->nwords += 5;
        break;
    }
    pkt->nqw = 0;
    pkt->gtag = 0;
    return pkt->last;
}

/* Zero pads so UNPACK ends a quadword, then copies four quadwords; returns the copied data. */
/* 0x001b3f58 */
int *psVifPktAddUnpackMatrix(PsVifWr *pkt, int code, PsQword *src)
{
    int *p;
    int pad;
    unsigned int n;

    p = pkt->cur;
    n = pkt->nwords & 3;
    switch (n) {
    case 0:
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = code;
        pad = 3;
        break;
    case 1:
        *p++ = 0;
        *p++ = 0;
        *p++ = code;
        pad = 2;
        break;
    case 2:
        *p++ = 0;
        *p++ = code;
        pad = 1;
        break;
    case 3:
        *p++ = code;
        pad = 0;
        break;
    }
    ((PsQword *)p)[0] = src[0];
    ((PsQword *)p)[1] = src[1];
    ((PsQword *)p)[2] = src[2];
    ((PsQword *)p)[3] = src[3];
    pkt->cur = p + 16;
    pkt->nwords += pad + 17;
    return p;
}

/* Zero pads so UNPACK ends a quadword, then copies nqw quadwords; returns the copied data. */
/* 0x001b4030 */
int *psVifPktAddUnpack(PsVifWr *pkt, int code, PsQword *src, int nqw)
{
    int *p;
    int *dst;
    int pad;
    int i;
    int len;
    unsigned int n;

    p = pkt->cur;
    n = pkt->nwords & 3;
    switch (n) {
    case 0:
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = code;
        pad = 3;
        break;
    case 1:
        *p++ = 0;
        *p++ = 0;
        *p++ = code;
        pad = 2;
        break;
    case 2:
        *p++ = 0;
        *p++ = code;
        pad = 1;
        break;
    case 3:
        *p++ = code;
        pad = 0;
        break;
    }
    dst = p;
    for (i = 0; i < nqw; i++) {
        *(PsQword *)dst = *src;
        src++;
        dst += 4;
    }
    pkt->cur = dst;
    len = nqw * 4 + 1;
    pkt->nwords += pad + len;
    return p;
}

/* 0x001b4120 */
int *psVifPktAddWords(PsVifWr *pkt, int *src, int n)
{
    int i;
    int *p;

    p = pkt->cur;
    for (i = 0; i < n; i++)
        *p++ = *src++;
    pkt->cur = p;
    pkt->nwords += n;
    return p;
}

/* Also remembered as the last VIF code. */
/* 0x001b4168 */
int *psVifPktAddWord(PsVifWr *pkt, int word)
{
    int *p;

    p = pkt->cur;
    *p = word;
    pkt->cur = p + 1;
    pkt->nwords++;
    pkt->last = p;
    return p;
}

/* 0x001b4190 */
int psVifPktGetWords(PsVifWr *pkt)
{
    return pkt->nwords;
}

/* 0x001b4198 */
PsVifWr *psVifPktInit(PsVifWr *pkt, int *start)
{
    pkt->nwords = 0;
    pkt->unk_0c = 0;
    pkt->last = 0;
    pkt->nqw = 0;
    pkt->gtag = 0;
    pkt->nreg = 0;
    pkt->cur = pkt->start = start;
    return pkt;
}

/* out[0] bytes used, out[1] capacity. */
/* 0x001b41c0 */
void psPktBufGetUsage(PsPktBuf *buf, int *out)
{
    int used;
    int cap;

    cap = buf->cap;
    used = buf->cur - buf->base;
    out[1] = cap;
    out[0] = used;
}

/* Asserts on overrun. */
/* 0x001b41e0 */
void psPktBufClose(PsPktBuf *buf)
{
    if (buf->cap < buf->cur - buf->base) {
        printf("psPktBufClose: DMA packet buf over run!\n");
        PS_ASSERT_FAIL("ps_pkt.c", 598);
    }
}

/* 0x001b4238 */
void psPktBufReset(PsPktBuf *buf)
{
    buf->unk_14 = 0;
    buf->cur = buf->base;
    buf->unk_18 = 0;
}

/*
 * Header at the first 16-byte boundary, data 48 bytes after.
 * Bug: the header clear swaps value and length, so clears nothing.
 */
/* 0x001b4250 */
PsPktBuf *psPktBufInit(unsigned int addr, unsigned int size)
{
    PsPktBuf *buf;

    buf = (PsPktBuf *)((addr + 15) & ~15);
    memset(buf, 40, 0);
    buf->orig = addr;
    buf->size = size;
    buf->base = (unsigned int)buf + 48;
    buf->cap = size - 48;
    psPktBufReset(buf);
    return buf;
}
