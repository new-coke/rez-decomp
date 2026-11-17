/* DMA tag lists, ref/call packets, GS register cache. */

#ifndef REZ_PS_DISPLIST_H
#define REZ_PS_DISPLIST_H
#include "types_ps.h"

struct PsDispList;
struct PsVifPkt;
struct PsDmaTag;
struct PsPktBuf;

void psDisplayListFlushGsRegs(struct PsDispList *dl, struct PsVifPkt *pkt);
int psDisplayListIsDirty(struct PsDispList *dl);
void psDisplayListSetGsReg(struct PsDispList *dl, int idx, unsigned long val);
void psDisplayListUpdateGsReg(struct PsDispList *dl, int idx, unsigned long val);
struct PsVifPkt *psDisplayListGetVifPkt(struct PsDispList *dl);
struct PsDmaTag *psDisplayListAddRefPkt(struct PsDispList *dl, int addr, int qwc);
void psDisplayListCloseRefPkt(struct PsDispList *dl);
struct PsVifPkt *psDisplayListOpenRefPkt(struct PsDispList *dl);
struct PsDmaTag *psDisplayListAddTag(struct PsDispList *dl, unsigned long hdr, int data0, int data1);
int psDisplayListLock(struct PsDispList *dl);
int psDisplayListUnLock(struct PsDispList *dl);
void psDisplayListFlushInt(struct PsDispList *dl, int ch);
void psDisplayListFlush(struct PsDispList *dl, int ch);
void psDisplayListAppend(struct PsDispList *dl, struct PsDispList *next);
void psDisplayListClose(struct PsDispList *dl);
void psDisplayListOpen(struct PsDispList *dl);
void psDisplayListGetUsage(struct PsDispList *dl, int *out);
struct PsDispList *psDisplayListInit(struct PsDispList *dl, int size, struct PsPktBuf *pkt);

extern char ps_displist_assert_tag[];

#define PD_ASSERT_FAIL(file, line) psAssertFail(file, line, ps_displist_assert_tag)

#endif /* REZ_PS_DISPLIST_H */
