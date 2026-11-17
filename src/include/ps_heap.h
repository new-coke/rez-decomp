/* Main-memory heaps. */

#ifndef REZ_PS_HEAP_H
#define REZ_PS_HEAP_H

struct Heap;
struct HeapBlkV;

void psHeapGetStat(struct Heap *heap, unsigned int *info);
void psHeapFree(void *ptr);
void *psHeapCalloc(struct Heap *heap, unsigned int size);
void *psHeapAlloc(struct HeapBlkV *heap, unsigned int size);
void psHeapRelease(void);
struct Heap *psHeapInit(struct Heap *base, unsigned int size);

#endif /* REZ_PS_HEAP_H */
