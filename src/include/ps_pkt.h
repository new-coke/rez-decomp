/* VIF packet writer, DMA packet buffers. */

#ifndef REZ_PS_PKT_H
#define REZ_PS_PKT_H
#include "types_ps.h"

struct PsVifWr;
struct PsGifTag;
struct PsGsReg;
struct PsPktBuf;

int *psVifPktAddVifCode(struct PsVifWr *pkt, long code);
void psVifPktAddRegAD(struct PsVifWr *pkt, unsigned long addr, unsigned long data);
void psVifPktAddGsReg(struct PsVifWr *pkt, struct PsGsReg *reg);
void psVifPktAdvanceGif(struct PsVifWr *pkt, int *end, int n);
void psVifPktCountGifQwords(struct PsVifWr *pkt);
struct PsGifTag *psVifPktAddGifTag(struct PsVifWr *pkt, struct PsGifTag *tag);
void psVifPktCloseGifPkt(struct PsVifWr *pkt);
int *psVifPktOpenGifPkt(struct PsVifWr *pkt);
int *psVifPktAddWords(struct PsVifWr *pkt, int *src, int n);
int *psVifPktAddWord(struct PsVifWr *pkt, int word);
int psVifPktGetWords(struct PsVifWr *pkt);
struct PsVifWr *psVifPktInit(struct PsVifWr *pkt, int *start);
void psPktBufGetUsage(struct PsPktBuf *buf, int *out);
void psPktBufClose(struct PsPktBuf *buf);
void psPktBufReset(struct PsPktBuf *buf);
struct PsPktBuf *psPktBufInit(unsigned int addr, unsigned int size);

extern char ps_pkt_assert_tag[];

#define PS_ASSERT_FAIL(file, line) psAssertFail(file, line, ps_pkt_assert_tag)

#endif /* REZ_PS_PKT_H */
