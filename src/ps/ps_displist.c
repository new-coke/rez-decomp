/*
 * Display lists: arrays of DMA tags (ref and call packets) plus a GS register cache, so only
 * changed registers are sent. Lists open, close, chain, lock and send on a DMA channel.
 */

#include "types_ps.h"
#include "ps_displist.h"
#include "libc.h"
#include "kernel_syscalls.h"

static PsDmaTag *psDisplayListAddCallTag(PsDispList *dl);
static PsDmaTag *psDisplayListAddEndTag(PsDispList *dl);
static void psDisplayListResetGsRegs(PsDispList *dl);

#ifdef VERSION_US

/* Source line reported by the assertions; differs between builds. */
#define SRC_LINE(early, us) (us)

#else

#define SRC_LINE(early, us) (early)

#endif /* !VERSION_US */

/* DMA tag as one doubleword, for a single store. */
typedef union { unsigned long l; PsDmaTag t; } PsDmaTagWord;

typedef struct {
    unsigned long qwc : 16;
    unsigned long pad : 10;
    unsigned long pce : 2;
    unsigned long id : 3;
    unsigned long irq : 1;
    unsigned long addr : 31;
    unsigned long spr : 1;
} PsDmaTagBits;

#define PS_DL_REG ((int)&((PsDispList *)0)->reg)

extern PsGifTag *psVifPktAddGifTag(PsVifPkt *w, unsigned long *tag);
extern void psVifPktAddGsReg(PsVifPkt *w, PsGsReg *r);
extern void psVifPktCountGifQwords(PsVifPkt *w);
extern void psVifPktAddWord(PsVifPkt *w, int word);
extern unsigned int psVifPktGetWords(PsVifPkt *w);
extern int psVifPktAddVifCode(PsVifPkt *w, long v, int a2, int a3);
extern void psVifPktInit(PsVifPkt *w, unsigned int start);
extern PsDmaTag *psDisplayListAddCallTag(PsDispList *dl);
extern void psLogPrintf(int level, char *fmt, ...);
extern void psDmaSend(int ch, PsDmaTag *tags);
extern PsDmaTag *psDisplayListAddEndTag(PsDispList *dl);
extern void psDisplayListResetGsRegs(PsDispList *dl);

/* Changed GS registers as one A+D GIF tag plus a write per slot; clears the changed state. */
/* 0x001b42c8 */
void psDisplayListFlushGsRegs(PsDispList *dl, PsVifPkt *pkt)
{
    unsigned long tmpl[2];
    PsGifTag *tag;
    int i;

    tmpl[0] = 0x1000000000000000UL;
    tmpl[1] = 14;
    if (dl->flags & 1) {
        tag = psVifPktAddGifTag(pkt, tmpl);
        for (i = 0; i < (char)dl->dirty[0]; i++)
            psVifPktAddGsReg(pkt, &dl->reg[(char)dl->dirty[i + 1]]);
        tag->nloop = (char)dl->dirty[0];
        psVifPktCountGifQwords(pkt);
        dl->dirty[0] = 0;
        dl->flags &= ~1;
    }
}

/* Non-zero when GS register changes are not yet flushed. */
/* 0x001b43c0 */
int psDisplayListIsDirty(PsDispList *dl)
{
    return dl->flags & 1;
}

/* 0x001b43d0 */
static unsigned long psDisplayListGetGsReg(PsDispList *dl, int idx)
{
    return dl->reg[idx].val;
}

/* Sets cached slot idx, marks the list changed and records idx in the changed-slot list once. */
/* 0x001b43e0 */
void psDisplayListSetGsReg(PsDispList *dl, int idx, unsigned long val)
{
    int i;

    ((union { PsGsReg r[99]; } *)dl->reg)->r[idx].val = val;
    dl->flags |= 1;
    for (i = 0; i < (char)dl->dirty[0]; i++) {
        if ((char)dl->dirty[i + 1] == idx)
            break;
    }
    if (i >= (char)dl->dirty[0]) {
        dl->dirty[i + 1] = idx;
        dl->dirty[0]++;
    }
}

/* Sets cached slot idx only if the value differs. */
/* 0x001b4478 */
void psDisplayListUpdateGsReg(PsDispList *dl, int idx, unsigned long val)
{
    if (dl->reg[idx].val != val)
        psDisplayListSetGsReg(dl, idx, val);
}

/* Open VIF packet writer, or 0 (with an error) when no call or ref packet is open. */
/* 0x001b44a0 */
PsVifPkt *psDisplayListGetVifPkt(PsDispList *dl)
{
    if (!(dl->flags & 0x300)) {
        printf("psDisplayListGetVifPkt: VIF Pakcet not open!\n");
        return 0;
    }
    return &dl->vif;
}

/* Pads to a quadword, sizes the DMA tag, ends with a ret tag, moves the buffer past it. */
/* 0x001b44d8 */
static void psDisplayListCloseCallPkt(PsDispList *dl)
{
    PsVifPkt *pkt;
    int n;
    int i;
    char *cur;

    if (dl->flags & 0x200) {
        pkt = &dl->vif;
        n = pkt->nwords & 3;
        if (n) {
            for (i = 0; i < 4 - n; i++)
                psVifPktAddWord(pkt, 0);
        }
    } else {
        printf("psDisplayListCloseCallPkt: call packet not open!\n");
        return;
    }
    pkt = &dl->vif;
    dl->cur_tag->qwc = psVifPktGetWords(pkt) >> 2;
    psVifPktAddVifCode(pkt, 0x60000000, 0, 0);
    dl->flags &= ~0x200;
    cur = dl->vif.cur;
    dl->pkt->cur = cur;
}

/* DMA call tag to the current buffer position; 0 (with an error) if a packet is already open. */
/* 0x001b45b8 */
static PsVifPkt *psDisplayListOpenCallPkt(PsDispList *dl)
{
    PsDmaTag *tag;

    if (!(dl->flags & 0x300)) {
        tag = psDisplayListAddTag(dl, 0x50000000, 0, 0);
        dl->cur_tag = tag;
        tag->addr = dl->pkt->cur;
        psVifPktInit(&dl->vif, dl->pkt->cur);
        dl->flags |= 0x200;
        return &dl->vif;
    }
    printf("psDisplayListOpenCallPkt: call packet cannot open!\n");
    return 0;
}

/* Ref tag for qwc quadwords at addr. Warns if not open or locked; 0 if a packet is open. */
/* 0x001b4678 */
PsDmaTag *psDisplayListAddRefPkt(PsDispList *dl, int addr, int qwc)
{
    PsDmaTag *tag;

    if (!(dl->flags & 2))
        psLogPrintf(4, "psDisplayListOpenRefPkt: displaylist (%08x) is not open!\n", dl);
#ifdef VERSION_US
    if (dl->flags & 8)
        psLogPrintf(4, "psDisplayListOpenRefPkt: displaylist (%08x) locked!\n", dl);
#endif
    if (!(dl->flags & 0x300)) {
        tag = psDisplayListAddCallTag(dl);
        dl->cur_tag = tag;
        tag->addr = addr;
        dl->cur_tag->qwc = qwc;
        return dl->cur_tag;
    }
    printf("psDisplayListAddRefPkt: packet already open!\n");
    return 0;
}

/* Pads to a quadword, stores the size in the DMA tag and moves the buffer past it. */
/* 0x001b4758 */
void psDisplayListCloseRefPkt(PsDispList *dl)
{
    PsVifPkt *pkt;
    int n;
    int i;
    unsigned int words;

    do {
        pkt = &dl->vif;
    } while (0);
    if (dl->flags & 0x100) {
        n = pkt->nwords & 3;
        if (n) {
            for (i = 0; i < 4 - n; i++)
                psVifPktAddWord(pkt, 0);
        }
    } else {
        printf("psDisplayListCloseRefPkt: vifpkt not open!\n");
        return;
    }
    words = psVifPktGetWords(pkt);
    dl->flags &= ~0x100;
    *(char **)&dl->pkt->cur = (char *)dl->vif.cur;
    dl->cur_tag->qwc = words >> 2;
}

/* DMA ref tag to the current buffer position. Warns if not open; 0 if a packet is already open. */
/* 0x001b4828 */
PsVifPkt *psDisplayListOpenRefPkt(PsDispList *dl)
{
    PsDmaTag *tag;

    if (!(dl->flags & 2))
        psLogPrintf(4, "psDisplayListOpenRefPkt: displaylist (%08x) is not open!\n", dl);
    if (!(dl->flags & 0x300)) {
        tag = psDisplayListAddCallTag(dl);
        dl->cur_tag = tag;
        tag->addr = dl->pkt->cur;
        psVifPktInit(&dl->vif, dl->pkt->cur);
        dl->flags |= 0x100;
        return &dl->vif;
    }
    printf("psDisplayListOpenRefPkt: vifpkt cannot open!\n");
    return 0;
}

/* Warns and returns 0 when the tag array is full. */
/* 0x001b4900 */
PsDmaTag *psDisplayListAddTag(PsDispList *dl, unsigned long hdr, int data0, int data1)
{
    PsDmaTag *tag;

    if (dl->ntags >= dl->maxtags) {
        printf("WARNING: DMA Tag is full!\n");
        return 0;
    }
    tag = &dl->tags[dl->ntags];
    tag->data0 = data0;
    tag->data1 = data1;
    *(unsigned long *)tag = hdr;
    dl->ntags++;
    return tag;
}

/* Appends a ref tag, not a call tag. */
/* 0x001b4968 */
static PsDmaTag *psDisplayListAddCallTag(PsDispList *dl)
{
    return psDisplayListAddTag(dl, 0x30000000, 0, 0);
}

/* 0x001b4990 */
static PsDmaTag *psDisplayListAddEndTag(PsDispList *dl)
{
    return psDisplayListAddTag(dl, 0x70000000, 0, 0);
}

#ifdef VERSION_US

/* Also every list chained after dl; an open or already locked list stops the walk with -1. */
/* 0x001b49b8 */
int psDisplayListLock(PsDispList *dl)
{
    while (dl != 0) {
        if (dl->flags & 2) {
            psLogPrintf(4, "psDisplayListLock: displaylist(%08x) not close!\n", dl);
            return -1;
        }
        if (dl->flags & 8) {
            psLogPrintf(4, "psDisplayListLock: displaylist(%08x) already locked!\n", dl);
            return -1;
        }
        dl->flags |= 8;
        dl = (PsDispList *)dl->us_unk24[1];
    }
    return 0;
}

/* An open or unlocked list stops the walk with -1. */
/* 0x001b4a28 */
int psDisplayListUnLock(PsDispList *dl)
{
    while (dl != 0) {
        if (dl->flags & 2) {
            psLogPrintf(4, "psDisplayListUnLock: displaylist(%08x) not close!\n", dl);
            return -1;
        }
        if (!(dl->flags & 8)) {
            psLogPrintf(4, "psDisplayListUnLock: displaylist(%08x) not locked!\n", dl);
            return -1;
        }
        dl->flags &= ~8;
        dl = (PsDispList *)dl->us_unk24[1];
    }
    return 0;
}

#endif /* VERSION_US */

/* Interrupt-handler psDisplayListFlush; errors if the list is still open. */
/* 0x001b4aa0 */
void psDisplayListFlushInt(PsDispList *dl, int ch)
{
    if (dl->flags & 2) {
        printf("psDisplayListFlushInt: displaylist not close!\n");
        return;
    }
    if (dl->ntags) {
        iFlushCache(0);
        psDmaSend(ch, dl->tags);
    }
}

/* Flushes the data cache and sends the tags on channel ch, if any. */
/* 0x001b4b18 */
void psDisplayListFlush(PsDispList *dl, int ch)
{
    if (dl->ntags) {
        FlushCache(0);
        psDmaSend(ch, dl->tags);
    }
}

/* Last tag back to a DMA end tag, ending any chain; asserts without one. */
/* 0x001b4b70 */
static void psDisplayListEnd(PsDispList *dl)
{
    if (dl->end_tag == 0) {
        printf("psDisplayListEnd: DisplayList is illegal!\n");
        PD_ASSERT_FAIL("ps_displist.c", SRC_LINE(534, 622));
        return;
    }
#ifdef VERSION_US
    dl->us_unk24[1] = 0;
#endif
    *(unsigned long *)dl->end_tag = 0x70000000;
}

#ifdef VERSION_US

/* dl's end tag -> next tag to next's first tag; dl remembers next for Lock/UnLock. Both closed. */
/* 0x001b4bc8 */
void psDisplayListAppend(PsDispList *dl, PsDispList *next)
{
    PsDmaTag *end = dl->end_tag;
    PsDmaTag *head;
    union {
        PsDmaTagBits b;
        unsigned long l;
    } bits;

    if (end == 0 || (head = next->head) == 0) {
        printf("psDisplayListAppend: DisplayList not close!\n");
        PD_ASSERT_FAIL("ps_displist.c", 647);
        return;
    }
    bits.l = 0;
    bits.b.id = 2;
    bits.b.addr = (unsigned int)head;
    ((PsDmaTagWord *)end)->l = bits.l;
    dl->us_unk24[1] = (int)next;
}

#else

/* dl's end tag becomes a next tag to src's first tag; asserts unless both are closed. */
/* 0x001b4bc8 */
void psDisplayListAppend(PsDispList *dl, PsDispList *src)
{
    PsDmaTag *tag;
    unsigned int head_addr;
    unsigned long word;
    tag = dl->end_tag;
    if (tag) {
        head_addr = (unsigned int)src->head;
        if (head_addr) {
            word = 0x7fffffff00000000UL;
            word &= (unsigned long)head_addr << 32;
            *(unsigned long *)tag = word | 0x20000000;
            return;
        }
    }
    printf("psDisplayListAppend: DisplayList not close!\n");
    PD_ASSERT_FAIL("ps_displist.c", 557);
}

#endif /* !VERSION_US */

/* Remembers the first tag, appends the end tag; asserts if not open. */
/* 0x001b4c48 */
void psDisplayListClose(PsDispList *dl)
{
    if (!(dl->flags & 2)) {
        printf("psDisplayListClose: DisplayList is not open!\n");
        PD_ASSERT_FAIL("ps_displist.c", SRC_LINE(580, 672));
        return;
    }
    dl->head = dl->tags;
    dl->end_tag = psDisplayListAddEndTag(dl);
#ifdef VERSION_US
    dl->us_unk24[1] = 0;
#endif
    dl->flags = (dl->flags & ~2) | 4;
}

/* Empties the tags, resets the register cache. Warns if locked or already open. */
/* 0x001b4cd0 */
void psDisplayListOpen(PsDispList *dl)
{
#ifdef VERSION_US
    if (dl->flags & 8)
        psLogPrintf(4, "psDisplayListOpen: displaylist(%08x) locked!\n", dl);
    if (dl->flags & 2)
        psLogPrintf(4, "psDisplayListOpen: displaylist(%08x) already opened!\n", dl);
#endif
    dl->ntags = 0;
    psDisplayListResetGsRegs(dl);
    dl->end_tag = 0;
    dl->head = 0;
    dl->flags = (dl->flags | 2) & ~4;
}

/* Each of the 99 slots gets value -1 and its own register number. */
/* 0x001b4d58 */
static void psDisplayListResetGsRegs(PsDispList *dl)
{
    int i;

    for (i = 0; i < 99; i++) {
        *(long *)((char *)dl + i * 16 + PS_DL_REG + 8) = i;
        *(long *)((char *)dl + i * 16 + PS_DL_REG) = -1;
    }
}

/* out[0] tags used, out[1] capacity. */
/* 0x001b4d88 */
void psDisplayListGetUsage(PsDispList *dl, int *out)
{
    out[0] = dl->ntags;
    out[1] = dl->maxtags;
}

/* Header cleared; rest of the size-byte buffer holds the DMA tag array. Records into pkt. */
/* 0x001b4da0 */
PsDispList *psDisplayListInit(PsDispList *dl, int size, PsPktBuf *pkt)
{
    memset(dl, 0, (int)sizeof(PsDispList));
    dl->pkt = pkt;
    dl->tagsize = size - (int)sizeof(PsDispList);
    dl->tags = (PsDmaTag *)((char *)dl + sizeof(PsDispList));
    dl->maxtags = (unsigned int)(size - (int)sizeof(PsDispList)) >> 4;
    psDisplayListResetGsRegs(dl);
    return dl;
}
