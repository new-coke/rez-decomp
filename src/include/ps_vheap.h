/* Video memory heap. */

#ifndef REZ_PS_VHEAP_H
#define REZ_PS_VHEAP_H

struct PsVHeap;
struct PsVStat;
struct PsVBlk;

void psVHeapDump(struct PsVHeap *heap);
unsigned int psVHeapBlockAddr(struct PsVBlk *blk);
void psVHeapFree(struct PsVBlk *blk);
struct PsVBlk *psVHeapAlloc(struct PsVHeap *heap, int size);
void psVHeapClear(void *heap);
struct PsVHeap *psVHeapInit(struct PsVHeap *heap, int sz, int base, int size, int align);

#endif /* REZ_PS_VHEAP_H */
