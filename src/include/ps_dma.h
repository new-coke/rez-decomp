/* DMA channel table and transfers. */

#ifndef REZ_PS_DMA_H
#define REZ_PS_DMA_H

void psDmaSend(int ch, int tags);
void psDmaSendN(int ch, void *addr, int n);
void psDmaInit(void);

#endif /* REZ_PS_DMA_H */
