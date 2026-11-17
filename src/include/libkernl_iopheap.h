/* IOP heap client. */

#ifndef REZ_LIBKERNL_IOPHEAP_H
#define REZ_LIBKERNL_IOPHEAP_H

int sceSifInitIopHeap(void);
int sceSifAllocIopHeap(int size);
int sceSifFreeIopHeap(int addr);
int sceSifFreeSysMemory(int addr);

#endif /* REZ_LIBKERNL_IOPHEAP_H */
