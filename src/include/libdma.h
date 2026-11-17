/* DMA controller library. */

#ifndef REZ_LIBDMA_H
#define REZ_LIBDMA_H

struct DmaEnv;
struct DmaChanRegs;

int sceDmaGetChan(unsigned int n);
int sceDmaReset(int mode);
void sceDmaSend(struct DmaChanRegs *dch, void *tag);
void sceDmaSendN(struct DmaChanRegs *dch, void *addr, int size);
int sceDmaSync(struct DmaChanRegs *dch, int mode, int cnt);
void libdma_wait_int(unsigned short cause);

#endif /* REZ_LIBDMA_H */
