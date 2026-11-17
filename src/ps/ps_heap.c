/*
 * Main-memory heaps: ring of blocks with 64-byte magic-checked headers, next-fit from a rover,
 * merging free, a stack of current heaps; debug variants tag blocks with file and line.
 */

#include "types_ps.h"
#include "libc.h"
#include "ps_heap.h"
#include "ul_module.h"

static void *psHeapCallocCurrent(unsigned int size);
static void *psHeapAllocCurrent(unsigned int size);
static int psHeapGetCurrent(void);

typedef struct HeapBlkV {
    unsigned short magic;
    short used;
    unsigned int size;
    struct HeapBlkV *prev;
    struct HeapBlkV *next;
    struct HeapBlkV *heap;
    const char *name;
    struct HeapBlkV *rover;       /* block: line number; heap header: rover */
    unsigned int align;
} HeapBlkV;

extern HeapStack psHeapStack;

/* 0x001b69c0 */
static void *psHeapCallocDebug(Heap *heap, unsigned int size, const char *name, int line)
{
    void *ptr;
    HeapBlk *blk;

    ptr = psHeapCalloc(heap, size);
    if (ptr) {
        blk = (HeapBlk *)ptr - 1;
        blk->name = name;
        blk->line = line;
    }
    return ptr;
}

/* 0x001b6a08 */
static void *psHeapAllocDebug(Heap *heap, unsigned int size, const char *name, int line)
{
    void *ptr;
    HeapBlk *blk;

    ptr = psHeapAlloc(heap, size);
    if (ptr) {
        blk = (HeapBlk *)ptr - 1;
        blk->name = name;
        blk->line = line;
    }
    return ptr;
}

/* 0x001b6a50 */
static void *psHeapCallocCurrentDebug(unsigned int size, const char *name, int line)
{
    void *ptr;
    HeapBlk *blk;

    ptr = psHeapCallocCurrent(size);
    if (ptr) {
        blk = (HeapBlk *)ptr - 1;
        blk->name = name;
        blk->line = line;
    }
    return ptr;
}

/* 0x001b6a98 */
static void *psHeapAllocCurrentDebug(unsigned int size, const char *name, int line)
{
    void *ptr;
    HeapBlk *blk;

    ptr = psHeapAllocCurrent(size);
    if (ptr) {
        blk = (HeapBlk *)ptr - 1;
        blk->name = name;
        blk->line = line;
    }
    return ptr;
}

/* info: used bytes, free bytes, largest free, used count, free count. Stops on a bad header. */
/* 0x001b6ae0 */
void psHeapGetStat(Heap *heap, unsigned int *info)
{
    HeapBlk *blk;
    unsigned int largest;
    unsigned int free_bytes;
    unsigned int used_bytes;
    int nfree;
    int nused;

    if (heap == 0 || info == 0)
        return;
    largest = 0;
    free_bytes = 0;
    used_bytes = 0;
    blk = heap->root;
    heap = (Heap *)blk;
    do { nfree = 0; } while (0);
    nused = 0;
    blk = blk->next;
    while (blk != (HeapBlk *)heap) {
        if (blk->magic != 0xa5a5)
            return;
        if (blk->used == 1) {
            nused++;
            used_bytes += blk->size;
        } else {
            nfree++;
            free_bytes += blk->size;
            if (largest < blk->size)
                largest = blk->size;
        }
        blk = blk->next;
    }
    info[0] = used_bytes;
    info[1] = free_bytes;
    info[2] = largest;
    info[3] = nused;
    info[4] = nfree;
}

/* heap 0 = current heap. */
/* 0x001b6b78 */
static void psHeapDump(Heap *heap)
{
    Heap *target;
    HeapBlk *blk;
    int magic;

    magic = (int)heap & 1;
    if (heap)
        target = heap;
    else
        target = (Heap *)psHeapGetCurrent();
    magic &= 1;
    target = (Heap *)((int)target + (magic >> 1)); /* steer(zero) */
    ulDebugPrintf("==== Heap Status ====\n");
    ulDebugPrintf("ADDR       SIZE       USE\n");
    for (blk = target->root->next; blk != target->root; blk = blk->next) {
        if (blk->magic == 0xa5a5) {
            int used = blk->used;
            blk = (HeapBlk *)((int)blk + ((magic & (int)blk) >> 1)); /* steer(zero) */
            ulDebugPrintf("0x%08x 0x%08x %2d\n", (char *)blk + 64, blk->size, used);
        } else
            ulDebugPrintf("Illgal memoy block!\n");
    }
}

/* Header included; -1 if the header is invalid or the block is free. */
/* 0x001b6c48 */
static unsigned int psHeapGetSize(void *ptr)
{
    HeapBlk *blk;

    blk = (HeapBlk *)ptr - 1;
    if (blk->magic == 0xa5a5) {
        if (blk->used != 0)
            goto ok;
    }
    return -1;
ok:
    return blk->size;
}

/* Fills the payload with 0xff before checking the header (magic 0xa5a5); merges free neighbours. */
/* 0x001b6c78 */
void psHeapFree(void *ptr)
{
    HeapBlk *blk;
    HeapBlk *prev;
    Heap *heap;
    unsigned int size;
    HeapBlk *old_rover;

    if (ptr == 0)
        return;
    blk = (HeapBlk *)ptr - 1;
    memset(ptr, 255, blk->size - 64);
    if (blk->magic != 0xa5a5)
        return;
    if (blk->used == 0)
        return;
    prev = blk->prev;
    heap = blk->heap;
    if (prev->used == 0) {
        size = blk->size;
        blk->next->prev = prev;
        old_rover = heap->rover;
        blk->prev->next = blk->next;
        blk->prev->size += size;
        if (old_rover == blk)
            heap->rover = blk->prev;
        blk->used = 0;
        blk->magic = 0;
        blk = blk->prev;
    }
    if (blk->next->used == 0) {
        HeapBlk *rover;

        blk->next->next->prev = blk;
        rover = heap->rover;
        blk->size += blk->next->size;
        if (rover == blk->next)
            heap->rover = blk;
        blk->next = blk->next->next;
    }
    blk->used = 0;
}

/* 0x001b6d68 */
static void *psHeapCallocCurrent(unsigned int size)
{
    void *ptr;

    ptr = psHeapAlloc(psHeapGetCurrent(), size);
    if (ptr)
        memset(ptr, 0, size);
    return ptr;
}

/* 0x001b6dc0 */
static void *psHeapAllocCurrent(unsigned int size)
{
    return psHeapAlloc(psHeapGetCurrent(), size);
}

/* Clears 32 bytes at a time from the end; a tail under 32 bytes is left uncleared. */
/* 0x001b6df0 */
void *psHeapCalloc(Heap *heap, unsigned int size)
{
    int *ptr;
    int *dst;
    unsigned int n;
    unsigned int i;

    ptr = psHeapAlloc(heap, size);
    if (ptr) {
        n = size >> 5;
        dst = ptr + n * 8;
        for (i = 0; i < n; i++) {
            *--dst = 0;
            *--dst = 0;
            *--dst = 0;
            *--dst = 0;
            *--dst = 0;
            *--dst = 0;
            *--dst = 0;
            *--dst = 0;
        }
    }
    return ptr;
}

/* Next fit from the rover; size + 64-byte header rounded to heap->align; splits a larger rest. */
/* 0x001b6e70 */
void *psHeapAlloc(HeapBlkV *heap, unsigned int size)
{
    unsigned int need;
    unsigned int rem;
    HeapBlkV *blk;
    HeapBlkV *next_blk;

    need = (heap->align + size + 63) & -heap->align;
    blk = heap->rover;
    do {
        if (*(unsigned int *)blk == 0xa5a5 && blk->size >= need) {
            rem = blk->size - need;
            if (((volatile HeapBlkV *)heap)->align < rem) {
                next_blk = (HeapBlkV *)((char *)blk + need);
                next_blk->size = rem;
                blk->next->prev = next_blk;
                next_blk->magic = 0xa5a5;
                next_blk->used = 0;
                next_blk->next = blk->next;
                blk->next = next_blk;
                next_blk->prev = blk;
                blk->size = need;
                blk->used = 1;
                blk->heap = heap;
            } else {
                blk->used = 1;
                blk->heap = heap;
            }
            blk->rover = (HeapBlkV *)-1;
            heap->rover = blk->next;
            blk->name = 0;
            return (char *)blk + 64;
        }
        blk = blk->next;
    } while (blk != ((volatile HeapBlkV *)heap)->rover);
    return 0;
}

/* Top of psHeapStack. */
/* 0x001b6f28 */
static int psHeapGetCurrent(void)
{
    return psHeapStack.tab[psHeapStack.idx];
}

/* Replaces the top of the heap stack; returns the old one. */
/* 0x001b6f48 */
static int psHeapSetCurrent(int heap)
{
    HeapStack *stack;
    int old;

    stack = &psHeapStack;
    old = psHeapGetCurrent();
    stack->tab[stack->idx] = heap;
    return old;
}

/* 0x001b6f90 */
void psHeapRelease(void)
{
}

/* Header, used "root memory block", one free block, in a ring; alignment 64. 0 if size < 128. */
/* 0x001b6f98 */
Heap *psHeapInit(Heap *base, unsigned int size)
{
    HeapBlk *root;
    HeapBlk *blk;

    if (size < 128)
        return 0;
    base->align = 64;
    base->size = size;
    base->avail = size - 128;
    root = (HeapBlk *)((char *)base + 64);
    base->rootsize = 64;
    root->size = 64;
    blk = (HeapBlk *)((char *)base + 128);
    root->line = 0;
    root->magic = 0xa5a5;
    root->used = 1;
    root->prev = blk;
    root->next = blk;
    root->name = "root memory block";
    blk->magic = 0xa5a5;
    base->self = base;
    base->root = root;
    base->first = blk;
    base->rover = blk;
    blk->used = 0;
    blk->prev = root;
    blk->name = 0;
    blk->size = base->avail;
    blk->next = root;
    blk->line = 0;
    return base;
}

/* 0x001b7030 */
static int psHeapCreate(void * base, unsigned int size)
{
    return psHeapInit(base, size);
}

/* Eight entries; errors on overflow. */
/* 0x001b7050 */
static void psHeapPush(int heap)
{
    int i;

    i = psHeapStack.idx + 1;
    psHeapStack.idx = i;
    if (i >= 8) {
        printf("ERROR: Heap stack is over flow!\n");
        return;
    }
    psHeapStack.tab[i] = heap;
}

/* Errors on underflow. */
/* 0x001b7088 */
static void psHeapPop(void)
{
    int i;
    int n;

    i = psHeapStack.idx;
    n = i - 1;
    psHeapStack.idx = n;
    psHeapStack.tab[i] = 0;
    if (n < 0)
        printf("ERROR: Heap stack is under flow!\n");
}

/* 0x001b70c0 */
static void psHeapStackExit(void)
{
}

/* 0x001b70c8 */
static void psHeapStackInit(void)
{
    memset(&psHeapStack, 0, 36);
}
