/* Double-buffered DMA packet builder. */

#ifndef REZ_PS_GXPKT_H
#define REZ_PS_GXPKT_H

struct NjGxPkt;
struct NjGxTag;

unsigned int *psGxPktAddRegAD(struct NjGxPkt *pkt, unsigned long reg, unsigned long data);
unsigned int *psGxPktAddRegADGifTag(struct NjGxPkt *pkt, unsigned long lo);
unsigned int *psGxPktAddGifTag(struct NjGxPkt *pkt, struct NjGxTag *tag);
unsigned int *psGxPktOpenTag(struct NjGxPkt *pkt, struct NjGxTag *tag);
unsigned int *psGxPktBegin(struct NjGxPkt *pkt);
void psGxPktSend(struct NjGxPkt *pkt);
struct NjGxPkt *psGxPktInitChannel(struct NjGxPkt *pkt, char *buf, unsigned int size, int ch);

#endif /* REZ_PS_GXPKT_H */
