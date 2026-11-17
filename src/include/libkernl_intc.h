/* Enable/disable DMA and INTC interrupts (hand-written assembly). */

#ifndef REZ_LIBKERNL_INTC_H
#define REZ_LIBKERNL_INTC_H

int DisableDmac(int ch);
void DisableIntc(int n);
int EnableDmac(int ch);
int EnableIntc(int);

#endif /* REZ_LIBKERNL_INTC_H */
