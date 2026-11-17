/* Double-buffered DMA packet builder: one half is written while the other is sent. */

#include "types_nj.h"
#include "ninja.h"
#include "libc.h"
#include "ps_gxpkt.h"
#include "kernel_syscalls.h"
#include "ps_dma.h"

extern NjGxPkt *psGxPktCur;

/* 0x001b2690 */
static unsigned int *psGxPktAddWords(NjGxPkt *pkt, unsigned int *src, int n)
{
    unsigned int *dst = pkt->cur;
    int i;

    for (i = 0; i < n; i++)
        *dst++ = *src++;
    pkt->cur = dst;
    pkt->count += n;
    return dst;
}

/* 0x001b26d8 */
static unsigned int *psGxPktAddWord(NjGxPkt *pkt, unsigned int word)
{
    *pkt->cur = word;
    pkt->cur++;
    pkt->count++;
    return pkt->cur;
}

/* Remembered in pkt->vif so its quadword count can be filled in later. */
/* 0x001b2700 */
static unsigned int *psGxPktAddVifCode(NjGxPkt *pkt, unsigned int code)
{
    *pkt->cur = code;
    pkt->vif = (unsigned short *)pkt->cur;
    pkt->cur++;
    pkt->count++;
    return pkt->cur;
}

/* No VIF code open. */
/* 0x001b2728 */
static unsigned int *psGxPktOpenCnt(NjGxPkt *pkt)
{
    NjGxTag tag;

    tag.lo = 0x10000000;
    psGxPktOpenTag(pkt, &tag);
    pkt->vif = 0;
    pkt->count = 0;
    return pkt->cur;
}

/* Pads to a quadword and writes the count into the tag; an empty tag is dropped. */
/* 0x001b2768 */
static unsigned int *psGxPktCloseTag(NjGxPkt *pkt)
{
    int i, n;

    if (pkt->count) {
        if (pkt->count & 3) {
            n = 4 - (pkt->count & 3);
            for (i = 0; i < n; i++) {
                *pkt->cur++ = 0;
                pkt->count++;
            }
        }
        pkt->vif = 0;
        *pkt->tag = (pkt->count + 3) >> 2;
    } else {
        pkt->cur = (unsigned int *)pkt->tag;
    }
    return pkt->cur;
}

/* Data, then GS register address. */
/* 0x001b27e8 */
unsigned int *psGxPktAddRegAD(NjGxPkt *pkt, unsigned long reg, unsigned long data)
{
    ((NjGxQword *)pkt->cur)->l[0] = data;
    ((NjGxQword *)pkt->cur)->l[1] = reg;
    pkt->cur += 4;
    pkt->nq++;
    return pkt->cur;
}

/* 0x001b2818 */
static unsigned int *psGxPktAddQword(NjGxPkt *pkt, NjGxTag *qw)
{
    *(NjGxTag *)pkt->cur = *qw;
    pkt->cur += 4;
    pkt->nq++;
    return pkt->cur;
}

/* Adds n to the open VIF code's count; appends an empty GIF tag. */
/* 0x001b2850 */
static unsigned int *psGxPktAddVifGifTag(NjGxPkt *pkt, int n)
{
    NjGxTag tag;

    tag.lo = 0;
    if (pkt->vif)
        *pkt->vif += n;
    return psGxPktAddGifTag(pkt, &tag);
}

/* One register descriptor; lo is the lower half (e.g. NLOOP). */
/* 0x001b2888 */
unsigned int *psGxPktAddRegADGifTag(NjGxPkt *pkt, unsigned long lo)
{
    NjGxTag tag;

    tag.lo = lo | 0x1000000000000000UL;
    tag.hi = 14;
    return psGxPktAddGifTag(pkt, &tag);
}

/* Remembered in pkt->gif. */
/* 0x001b28c0 */
unsigned int *psGxPktAddGifTag(NjGxPkt *pkt, NjGxTag *tag)
{
    *(NjGxTag *)pkt->cur = *tag;
    pkt->gif = (NjGxQword *)pkt->cur;
    pkt->cur += 4;
    pkt->nq++;
    return pkt->cur;
}

/* DMA cnt tag, three zero words, DIRECTHL whose size psGxPktCloseDirect fills in. */
/* 0x001b28f8 */
static unsigned int *psGxPktOpenDirect(NjGxPkt *pkt)
{
    NjGxTag tag;

    tag.lo = 0x10000000;
    psGxPktOpenTag(pkt, &tag);
    *pkt->cur++ = 0;
    *pkt->cur++ = 0;
    *pkt->cur++ = 0;
    pkt->count = 3;
    pkt->vif = (unsigned short *)pkt->cur;
    *pkt->cur++ = 0x51000000;
    pkt->count++;
    pkt->nq = 0;
    return pkt->cur;
}

/* EOP on the last GIF tag, counts into DIRECTHL and DMA tag; no GIF tag drops the DMA tag. */
/* 0x001b2980 */
static unsigned int *psGxPktCloseDirect(NjGxPkt *pkt)
{
    unsigned short *tag;

    if (pkt->gif) {
        pkt->gif->l[0] |= 0x8000;
        pkt->gif = 0;
        tag = pkt->tag;
        ((NjGxQword *)pkt->vif)->h[0] = pkt->nq;
        *tag = (pkt->count + pkt->nq * 4) >> 2;
        pkt->count = 0;
    } else {
        pkt->cur = (unsigned int *)pkt->tag;
        pkt->count--;
    }
    return pkt->cur;
}

/* Becomes the open tag. */
/* 0x001b29f0 */
unsigned int *psGxPktOpenTag(NjGxPkt *pkt, NjGxTag *tag)
{
    *(NjGxTag *)pkt->cur = *tag;
    pkt->count = 0;
    {
        unsigned int *pos = pkt->cur;
        pkt->cur = pos + 4;
        pkt->tag = (unsigned short *)pos;
    }
    return pkt->cur;
}

/* pkt->count words past the start. */
/* 0x001b2a20 */
static unsigned int *psGxPktGetEnd(NjGxPkt *pkt)
{
    return pkt->start + pkt->count;
}

/* Two halves split buf; complains without memory. */
/* 0x001b2a38 */
static void psGxPktInit(NjGxPkt *pkt, char *buf, unsigned int size)
{
    memset(pkt, 0, sizeof(NjGxPkt));
    pkt->size = size;
    pkt->buf[0] = buf;
    pkt->buf[1] = buf + size / 2;
    if (!buf || !size)
        printf("ERROR: psGxPktInit: packet size is not enough!\n");
}

/* Becomes psGxPktCur. */
/* 0x001b2ab8 */
unsigned int *psGxPktBegin(NjGxPkt *pkt)
{
    psGxPktCur = pkt;
    pkt->count = 0;
    pkt->tag = 0;
    pkt->cur = (unsigned int *)pkt->buf[pkt->idx];
    pkt->start = pkt->cur;
    pkt->vif = 0;
    pkt->gif = 0;
    return pkt->cur;
}

/* End tag, cache flush, send; switches halves and clears psGxPktCur. */
/* 0x001b2af0 */
void psGxPktSend(NjGxPkt *pkt)
{
    NjGxTag tag;

    tag.lo = 0x70000000;
    psGxPktOpenTag(pkt, &tag);
    if (pkt->tag) {
        FlushCache(0);
        psDmaSend(pkt->ch, pkt->start);
    }
    pkt->tag = 0;
    pkt->cur = 0;
    pkt->start = 0;
    pkt->idx ^= 1;
    psGxPktCur = 0;
}

/* Only when a tag is open. */
/* 0x001b2b60 */
static void psGxPktResend(NjGxPkt *pkt)
{
    if (pkt->tag) {
        FlushCache(0);
        psDmaSend(pkt->ch, pkt->start);
    }
}

/* 0 without memory. */
/* 0x001b2ba8 */
NjGxPkt *psGxPktInitChannel(NjGxPkt *pkt, char *buf, unsigned int size, int ch)
{
    memset(pkt, 0, sizeof(NjGxPkt));
    pkt->buf[0] = buf;
    pkt->size = size;
    pkt->buf[1] = buf + size / 2;
    if (!buf || !size) {
        printf("ERROR: psGxPktInit: packet size is not enough!\n");
        return 0;
    }
    pkt->ch = ch;
    return pkt;
}
