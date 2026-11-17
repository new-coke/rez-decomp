/* Virtual heap for a range outside EE memory (VRAM); headers in a separate buffer; next fit. */

#include "types_ul.h"
#include "libc.h"
#include "libc_internal.h"
#include "ps_vheap.h"

static unsigned int psVHeapBlockSize(PsVBlk *blk);

extern char psVHeapAssertStr[];

/* Used bytes and count, free bytes and count, largest free, total blocks. */
/* 0x001c1df8 */
static void psVHeapGetStat(PsVHeap *heap, PsVStat *out)
{
    PsVBlk *blk;
    PsVBlk *end;
    unsigned int sz;

    memset(out, 0, 24);
    end = &heap->sentinel;
    blk = heap->sentinel.next;
    do {
        sz = psVHeapBlockSize(blk);
        if (blk->used) {
            out->used_count++;
            out->used_bytes += sz;
        } else {
            out->free_count++;
            out->free_bytes += sz;
            if (out->max_free < sz)
                out->max_free = sz;
        }
        blk = blk->next;
    } while (blk != end);
    out->total_count = out->used_count + out->free_count;
}

/* 0x001c1eb8 */
void psVHeapDump(PsVHeap *heap)
{
    PsVBlk *blk;
    PsVBlk *end = &heap->sentinel;

    printf("======== Virtual Heap Status ========\n");
    printf("ADDR       SIZE       USE\n");
    printf("-------------------------------------\n");
    blk = heap->sentinel.next;
    do {
        printf("0x%08x 0x%08x  %1d\n", psVHeapBlockAddr(blk), psVHeapBlockSize(blk), blk->used);
        blk = blk->next;
    } while (blk != end);
    printf("=====================================\n");
}

/* 0xffffffff for NULL. */
/* 0x001c1f60 */
static unsigned int psVHeapBlockSize(PsVBlk *blk)
{
    if (blk)
        return blk->size;
    return 0xffffffff;
}

/* 0xffffffff for NULL. */
/* 0x001c1f80 */
unsigned int psVHeapBlockAddr(PsVBlk *blk)
{
    if (blk)
        return blk->addr;
    return 0xffffffff;
}

/* One free block over base/size. */
/* 0x001c1fa0 */
static void psVHeapReset(PsVHeap *heap)
{
    PsVBlk *blk = heap->first;

    blk->magic = PSVH_MAGIC;
    blk->used = 0;
    blk->addr = heap->base;
    blk->next = &heap->sentinel;
    blk->prev = &heap->sentinel;
    blk->size = heap->size;
    heap->cur = blk;
}

/* Asserts an owning heap; merges free neighbours, clears their headers, moves the rover off. */
/* 0x001c1fd8 */
void psVHeapFree(PsVBlk *blk)
{
    PsVHeap *heap;

    if (blk) {
        heap = blk->heap;
        if (heap == 0)
            __assert("ps_vheap.c", 173, psVHeapAssertStr);
        if (blk->magic == PSVH_MAGIC && blk->used == 1) {
            if (blk->prev->used == 0) {
                PsVBlk *merged = blk;

                blk->next->prev = blk->prev;
                blk->prev->next = blk->next;
                blk->prev->size += blk->size;
                if (blk == heap->cur)
                    heap->cur = blk->prev;
                blk = blk->prev;
                memset(merged, 0, 32);
            }
            if (blk->next->used == 0) {
                PsVBlk *n = blk->next;

                n->next->prev = blk;
                blk->size += blk->next->size;
                if (blk->next == heap->cur)
                    heap->cur = blk;
                blk->next = blk->next->next;
                memset(n, 0, 32);
            }
            blk->used = 0;
        }
    }
}

/* Slot with magic != PSVH_MAGIC: after the rover to the end, then from the start. NULL if none. */
/* 0x001c2100 */
static PsVBlk *psVHeapFindSlot(PsVHeap *heap)
{
    PsVBlk *cur = heap->cur;
    PsVBlk *last = heap->last;
    PsVBlk *p;

    for (p = cur + 1; p <= last; p++) {
        if (p->magic != PSVH_MAGIC)
            return p;
    }
    for (p = heap->first; p < heap->cur; p++) {
        if (p->magic != PSVH_MAGIC)
            return p;
    }
    return 0;
}

/* Next fit from the rover, size rounded to granularity; the remainder goes to a spare header. */
/* 0x001c2180 */
PsVBlk *psVHeapAlloc(PsVHeap *heap, int size)
{
    PsVBlk *blk = heap->cur;
    PsVBlk *start = blk;
    PsVBlk *n;
    int magic;
    unsigned int sz = (size + heap->align - 1) & -heap->align;

    do {
        magic = blk->magic;
        if (magic == PSVH_MAGIC && blk->used == 0 && blk->size >= sz) {
            if (blk->size != sz) {
                n = psVHeapFindSlot(heap);
                if (n) {
                    n->magic = magic;
                    n->used = 0;
                    blk->next->prev = n;
                    n->addr = blk->addr + sz;
                    n->next = blk->next;
                    n->size = blk->size - sz;
                    blk->next = n;
                    n->prev = blk;
                }
            }
            blk->size = sz;
            blk->used = 1;
            heap->cur = blk->next;
            blk->heap = heap;
            return blk;
        }
        blk = blk->next;
    } while (blk != start);
    return 0;
}

/* Header and sentinel. Called through psSurfClearVramHeap and ulTextureClearVramHeap. */
/* 0x001c2290 */
void psVHeapClear(void *heap)
{
    memset(heap, 0, 64);
}

/* Sentinel plus a free block over [base, base + size); rest is spare headers. 0 if NULL/small. */
/* 0x001c22a0 */
PsVHeap *psVHeapInit(PsVHeap *heap, int sz, int base, int size, int align)
{
    PsVBlk *sentinel = &heap->sentinel;
    PsVBlk *blk = sentinel + 1;
    int n;

    if (heap == 0)
        return 0;
    n = (unsigned int)(sz - 64) >> 5;
    if (n <= 0)
        return 0;
    heap->nslots = n;
    heap->base = base;
    heap->size = size;
    sentinel->magic = PSVH_MAGIC;
    sentinel->used = 1;
    sentinel->addr = 0;
    sentinel->size = 0;
    blk->magic = PSVH_MAGIC;
    sentinel->next = blk;
    sentinel->prev = blk;
    blk->addr = base;
    blk->size = size;
    blk->used = 0;
    heap->last = &blk[n] - 1;
    heap->first = blk;
    blk->next = sentinel;
    blk->prev = sentinel;
    heap->cur = blk;
    heap->align = align;
    return heap;
}
