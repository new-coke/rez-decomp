/*
 * Game heaps: first-fit allocation from either end of a named heap, 64-byte block headers tagged
 * with source file and line, delayed frees, and the debug Memory window, heap map, overrun checks.
 */

#include "types_game.h"
#include "debug.h"
#include "libc.h"
#include "gw_command.h"
#include "gw_draw.h"
#include "gw_system.h"
#include "gw_window.h"
#include "nj_system.h"
#include "ul_texture.h"
#include "heap.h"
#include "debugdraw.h"
#include "debugmenu.h"
#include "float_bits.h"
#include "module.h"
#include "system.h"
#include "color.h"

typedef struct HeapFreeBlkView HeapFreeBlkView;
struct HeapFreeBlkView { char pad_00[28]; HeapFreeBlkView *pool_next; int pool_cnt; };
typedef struct HeapBaseNameView HeapBaseNameView;
struct HeapBaseNameView { int magic; int free; char pad_08[4]; int name; int count; int end; HeapBaseNameView *next; };
typedef struct HeapNodeView HeapNodeView;
struct HeapNodeView { int magic; HeapNodeView *next; unsigned int size; };
typedef struct HeapBlkHdrView HeapBlkHdrView;
struct HeapBlkHdrView { int magic; int next; int size; unsigned int ret; };
typedef struct HeapBaseEndView HeapBaseEndView;
struct HeapBaseEndView { int magic; int free; char pad_08[4]; char *name; int count, end; HeapBaseEndView *next; };
typedef struct HeapView HeapView;

struct HeapView {
    int magic;
    int link;
    int size;
    char *name;
    char *src;
    char *end;      /* heap: end address; block: source line */
    HeapView *next;
};

typedef struct HeapBaseNextView HeapBaseNextView;
struct HeapBaseNextView { int magic; int free; char pad_08[4]; int name; int count, end; HeapBaseNextView *next; };
typedef struct { int mode; int name; } HeapDumpArg;

typedef struct HeapBaseView HeapBaseView;
struct HeapBaseView { int magic; int free; char pad_08[4]; int name; int count; int end; HeapBaseView *next; };
typedef struct HeapBaseFreeView HeapBaseFreeView;
struct HeapBaseFreeView { int magic; int free; char pad_08[4]; char *name; };

extern char heap_base_mem[];
extern HeapBase *heap_list;
extern int heap_pool_cnt;
extern void soft_reset(const char *);
extern HeapFreeBlkView *heap_pool;
extern int heap_cur;
extern int heap_free_total;
extern unsigned int system_flags;
extern int heap_alloc_total;
extern void font_set(int);
extern ScreenView screen_layout;
extern int heap_window;
extern char heap_menu_items[];
extern void heap_block_dump(HeapBlkHdr *blk);
extern unsigned int heap_free_size_of(HeapBase *heap);
extern void heap_info_print(HeapBase *heap);

/* "Base" over heap_base_mem (14.5 MB), default and current. */
/* 0x00121738 */
void heap_system_init(void)
{
    HeapBase *heap;

    soft_reset("Heap");
    heap = heap_create(heap_base_mem, 0xe80000, "Base");
    heap_list = heap;
    heap_cur = heap;
    heap_pool = 0;
    heap_pool_cnt = 0;
}

/* 0x00121790 */
void heap_pool_tick(void)
{
    HeapFreeBlkView *p = heap_pool;
    HeapFreeBlkView *prev = p;

    while (p != 0) {
        if (p->pool_cnt-- <= 0) {
            void *mem = (char *)p + 64;
            HeapFreeBlkView *next = p->pool_next;

            if (p == prev) {
                heap_pool = next;
                prev = next;
            } else {
                prev->pool_next = next;
            }
            p = next;
            heap_free(mem, 0);
            heap_pool_cnt = heap_pool_cnt - 1;
        } else {
            prev = p;
            p = p->pool_next;
        }
    }
}

/* Freed after count heap_pool_tick calls. Returns the previous list length. */
/* 0x00121838 */
int heap_pool_add(void *obj, int count)
{
    HeapFreeBlkView *blk;

    if (obj == 0)
        return 0;
    blk = (HeapFreeBlkView *)((char *)obj - 64);
    blk->pool_cnt = count;
    if (heap_pool != 0)
        blk->pool_next = heap_pool;
    else
        blk->pool_next = 0;
    heap_pool = blk;
    return heap_pool_cnt++;
}

/* Base header, then one free block over the rest. Appended to the heap list. */
/* 0x00121880 */
HeapBase *heap_create(HeapBase *heap, int size, const char *name)
{
    HeapBlkHdr *blk = (HeapBlkHdr *)((char *)heap + 64);
    HeapBase *p;

    heap->magic = HEAP_MAGIC_BASE;
    blk->magic = HEAP_MAGIC_FREE;
    *(int *)&heap->end = (int)((char *)heap + size);
    heap->unk_08 = 0;
    heap->count = 0;
    *(int *)&heap->free = (int)blk;
    heap->name = name;
    heap->next = 0;
    blk->src = name;
    ((int *)blk)[8] = 0;
    blk->next = 0;
    blk->ret = 0;
    blk->line = 0;
    ((int *)blk)[7] = 0;
    blk->size = size - 64;
    DEBUG_PRINT("Create Heap ========\n");
    heap_info_print(heap);
    DEBUG_PRINT("====================\n");
    if (heap_list != 0) {
        p = heap_list;
        while (p->next)
            p = p->next;
        p->next = heap;
    }
    return heap;
}

/* Unlinks only. */
/* 0x00121960 */
void heap_destroy(HeapBase *heap)
{
    HeapBase *prev;
    HeapBase *next;

    DEBUG_PRINT("Destroy Heap =======\n");
    heap_info_print(heap);
    DEBUG_PRINT("====================\n");
    if (heap_list != 0) {
        prev = heap_list;
        do {
            next = prev->next;
            if (next == heap) {
                prev->next = next->next;
                break;
            }
            prev = next;
        } while (prev != 0);
    }
}

/* 0 selects the default heap. Returns the previous one. */
/* 0x001219d8 */
int heap_set_current(int heap)
{
    int old = heap_cur;

    if (heap != 0)
        heap_cur = heap;
    else
        heap_cur = heap_list;
    return old;
}

/* Head of the heap list. */
/* 0x00121a00 */
int heap_get_default(void)
{
    return heap_list;
}

/* 0x00121a10 */
int heap_next(int *p)
{
    return p[6];
}

/* 0x00121a18 */
int heap_count(void)
{
    int n = 0;
    HeapBaseNameView *heap;

    for (heap = heap_list; heap != 0; heap = heap->next)
        n++;
    return n;
}

/* For the heap map. Returns mem. */
/* 0x00121a50 */
void *heap_block_tag(void *mem, const char *src, int line)
{
    HeapBlkHdr *blk;

    if (mem == 0)
        return 0;
    blk = (HeapBlkHdr *)((char *)mem - 64);
    blk->src = src;
    blk->line = line;
    return mem;
}

/* Rounded up to whole words. */
/* 0x00121a70 */
void heap_bzero(void *buf, unsigned int n)
{
    int *p = buf;

    if (n != 0) {
        n += 3;
        n >>= 2;
        do {
            *p++ = 0;
        } while (--n != 0);
    }
}

/* Current heap, low end. */
/* 0x00121aa8 */
void *game_malloc(int size)
{
    return heap_alloc_front(heap_cur, size, 0);
}

/* Current heap, high end. */
/* 0x00121ad0 */
void *game_malloc_back(int size)
{
    return heap_alloc_back(heap_cur, size, 0);
}

/* Current heap, low end. */
/* 0x00121af8 */
void *game_calloc(int size)
{
    void *mem = game_malloc(size);

    if (mem != 0)
        heap_bzero(mem, size);
    return mem;
}

/* Current heap, high end. */
/* 0x00121b40 */
void *game_calloc_back(int size)
{
    void *mem = game_malloc_back(size);

    if (mem != 0)
        heap_bzero(mem, size);
    return mem;
}

/*
 * Address-ordered insert, merged with free neighbours. SYS_HEAP_FILL fills it with "UGAK".
 * Overlapping neighbours report a broken list.
 */
/* 0x00121b88 */
void heap_free(void *mem, int ret)
{
    HeapFreeBlk *blk;
    HeapFreeBlk *prev;
    HeapFreeBlk *next;
    HeapBase *heap;
    unsigned int n;
    unsigned int dbg;

    if (mem == 0)
        return;
    heap_free_total = heap_free_total + 1;
    blk = (HeapFreeBlk *)((char *)mem - 64);
    dbg = system_flags & SYS_HEAP_FILL;
    heap = blk->heap;
    *(void **)((char *)mem - 64) = (void *)HEAP_MAGIC_FREE;
    blk->ret = 0;
    if (dbg) {
        n = (blk->size - 64) >> 5;
        if (n != 0)
            nj_fill256(mem, 0x4b414755, n);
    }
    next = (HeapFreeBlk *)heap->free;
    prev = (HeapFreeBlk *)heap;
    while (next != 0 && !(blk < next)) {
        prev = next;
        next = prev->next;
    }
    if (next == 0) {
        if (heap->end < (char *)blk)
            heap_free_error(ret, mem);
        blk->next = 0;
    } else if ((HeapFreeBlk *)((char *)blk + blk->size) == next) {
        blk->size = blk->size + next->size;
        blk->next = next->next;
        if (system_flags & SYS_HEAP_FILL)
            nj_fill256(next, 0x4b414755, 2);
    } else {
        if (next < (HeapFreeBlk *)((char *)blk + blk->size))
            heap_free_error(ret, mem);
        blk->next = next;
    }
    if ((HeapFreeBlk *)((char *)prev + prev->size) == blk) {
        prev->size = prev->size + blk->size;
        prev->next = blk->next;
        if (system_flags & SYS_HEAP_FILL)
            nj_fill256(blk, 0x4b414755, 2);
    } else {
        if (blk < (HeapFreeBlk *)((char *)prev + prev->size))
            heap_free_error(ret, mem);
        prev->next = blk;
    }
    heap->count = heap->count - 1;
}

/* Current heap. */
/* 0x00121d50 */
unsigned int heap_free_size(void)
{
    unsigned int sum = 0;
    HeapNodeView *node;

    for (node = (*(HeapBase * *)&heap_cur)->free; node != 0; node = node->next) /* steer(cast) */
        sum += node->size;
    return sum;
}

/* Current heap. */
/* 0x00121d90 */
unsigned int heap_max_free(void)
{
    unsigned int max = 0;
    HeapNodeView *node;

    for (node = (*(HeapBase * *)&heap_cur)->free; node != 0; node = node->next) /* steer(cast) */
        if (max < node->size)
            max = node->size;
    return max;
}

/* Current heap (fragmentation). */
/* 0x00121dd0 */
int heap_free_blocks(void)
{
    int n = 0;
    HeapNodeView *p;

    for (p = (*(HeapBase * *)&heap_cur)->free; p != 0; p = p->next) /* steer(cast) */
        n++;
    return n;
}

/* Current heap. */
/* 0x00121e10 */
int heap_alloc_count(void)
{
    return (*(HeapBase * *)&heap_cur)->count; /* steer(cast) */
}

/*
 * Rounded up to 64 bytes plus the 64-byte header; ret (caller) kept for the heap map. Failure
 * shows the overflow error with heap statistics and returns 0.
 */
/* 0x00121e20 */
void *heap_alloc_front(HeapBase *heap, int size, int ret)
{
    HeapFreeBlk *prev;
    HeapFreeBlk *blk;
    HeapFreeBlk *rest;
    unsigned int sz;
    unsigned int nsz;

    heap_alloc_total = heap_alloc_total + 1;
    sz = size + 127;
    sz &= 0xffffffc0;
    prev = (HeapFreeBlk *)heap;
    for (blk = prev->next; blk != 0; blk = blk->next) {
        nsz = blk->size;
        if (nsz >= sz) {
            if (nsz == sz) {
                prev->next = blk->next;
            } else {
                rest = (HeapFreeBlk *)((char *)blk + sz);
                prev->next = rest;
                rest->magic = HEAP_MAGIC_FREE;
                rest->size = nsz - sz;
                rest->line = 0;
                rest->next = blk->next;
                blk->size = sz;
                rest->src = 0;
                *(void **)&rest->ret = 0;
            }
            blk->magic = HEAP_MAGIC_HEAP;
            *(void **)&blk->ret = (void *)ret;
            blk->next = 0;
            blk->pool_cnt = 0;
            blk->heap = heap;
            heap->count = heap->count + 1;
            blk->pool_next = 0;
            return (char *)blk + 64;
        }
        prev = blk;
    }
    debug_error("Malloc Over Flow!!!\nReturn %08x\nSize   %8d/%08xh\n\nRemain %08x\nMax    %08x\nDivide %8d\nBlock  %8d\nNum    %8d\n",
                 ret, sz, sz, heap_free_size(), heap_max_free(), heap_free_blocks(), heap_alloc_count(),
                 heap_alloc_total);
    return 0;
}

/* Highest fitting free block, split off its end. Overflow as heap_alloc_front. */
/* 0x00121f80 */
void *heap_alloc_back(HeapBase *heap, int size, int ret)
{
    HeapFreeBlk *prev;
    HeapFreeBlk *node;
    HeapFreeBlk *best;
    HeapFreeBlk *bprev;
    unsigned int sz;

    heap_alloc_total = heap_alloc_total + 1;
    sz = size + 127;
    sz &= 0xffffffc0;
    best = 0;
    bprev = 0;
    prev = (HeapFreeBlk *)heap;
    for (node = prev->next; node != 0; node = node->next) {
        if (node->size >= sz) {
            bprev = prev;
            best = node;
        }
        prev = node;
    }
    if (best) {
        HeapFreeBlk *blk = best;
        if (blk->size == sz) {
            bprev->next = blk->next;
        } else {
            blk->size -= sz;
            blk = (HeapFreeBlk *)((char *)blk + blk->size);
            blk->size = sz;
        }
        blk->line = 0;
        blk->pool_cnt = 0;
        blk->magic = HEAP_MAGIC_HEAP;
        blk->next = 0;
        blk->src = 0;
        heap->count = heap->count + 1;
        *(void **)&blk->ret = (void *)ret;
        *(int *)&blk->heap = (int)heap;
        blk->pool_next = 0;
        return (char *)blk + 64;
    }
    debug_error("Malloc Over Flow!!!\nReturn %08x\nSize   %8d/%08xh\n\nRemain %08x\nMax    %08x\nDivide %8d\nBlock  %8d\nNum    %8d\n",
                 ret, sz, sz, heap_free_size(), heap_max_free(), heap_free_blocks(), heap_alloc_count(),
                 heap_alloc_total);
    return 0;
}

/* 0x001220e8 */
void *heap_calloc_front(void *heap, int size)
{
    void *mem = heap_alloc_front(heap, size, 0);

    if (mem != 0)
        heap_bzero(mem, size);
    return mem;
}

/* 0x00122130 */
void *heap_calloc_back(void *heap, int size)
{
    void *mem = heap_alloc_back(heap, size, 0);

    if (mem != 0)
        heap_bzero(mem, size);
    return mem;
}

/* 0x00122178 */
unsigned int heap_free_size_of(HeapBase *heap)
{
    unsigned int sum = 0;
    HeapNodeView *node;

    for (node = heap->free; node != 0; node = node->next)
        sum += node->size;
    return sum;
}

/* 0x001221b0 */
unsigned int heap_max_free_of(HeapBase *heap)
{
    unsigned int max = 0;
    HeapNodeView *node;

    for (node = heap->free; node != 0; node = node->next)
        if (max < node->size)
            max = node->size;
    return max;
}

/* 0x001221e8 */
int heap_free_blocks_of(HeapBase *heap)
{
    int n = 0;
    HeapNodeView *p;

    for (p = heap->free; p != 0; p = p->next)
        n++;
    return n;
}

/* 0x00122220 */
int heap_alloc_count_of(int *p)
{
    return p[4];
}

/* "free list broken"; ret is the caller. */
/* 0x00122228 */
void heap_free_error(int ret, int ptr)
{
    debug_error("Free Error\nList Broken\nRETURN: %08x\nPTR:    %08x", ret, ptr);
}

/* Caller address in (lo, hi). Returns the count. */
/* 0x00122240 */
int heap_find_in_range(unsigned int lo, unsigned int hi)
{
    HeapBase *heap = heap_cur;
    int n = 0;
    HeapBlkHdrView *blk;

    for (blk = heap->free; blk < heap->end; ) {
        char *start = (char *)blk + 64;
        int size = blk->size;
        char *next = (char *)blk + size;
        int total = size + 64;

        if (blk->magic == 0x50414548) {
            unsigned int ret = blk->ret;

            if (lo < ret && ret < hi) {
                DEBUG_PRINTF("%08x-%08x size %8x ret %08x\n", start, next, total, ret);
                n++;
            }
        }
        blk = (HeapBlkHdrView *)next;
    }
    return n;
}

/* 0x00122318 */
int heap_list_names(void)
{
    HeapBaseEndView *heap;

    if (heap_list != 0) {
        heap = heap_list;
        DEBUG_PRINT("== Base List ===========================\n");
        do {
            DEBUG_PRINTF("%s\n", heap->name);
            heap = heap->next;
        } while (heap != 0);
        DEBUG_PRINT("========================================\n");
    }
    return 0;
}

/* Heap argv[1] or current: per block range, size, source file and line; then statistics. */
/* 0x00122388 */
int heap_map_dump(int kind, char **argv)
{
    HeapView *heap = 0;
    HeapView *p;
    char *next;

    if (1 < kind && heap_list != 0) {
        argv++;
        do { p = heap_list; } while (0);
        do {
            if (strcmp(*argv, p->name) == 0) {
                heap = p;
                break;
            }
            p = p->next;
        } while (p != 0);
    }
    if (heap == 0)
        heap = heap_cur;
    DEBUG_PRINT("== HeapMap =============================\n");
    for (p = (HeapView *)((char *)heap + 64); (char *)p < heap->end; p = (HeapView *)next) {
        char *start;
        int size;
        int rest;

        size = p->size;
        next = (char *)p + size;
        start = (char *)p + 64;
        rest = size - 64;

        if (p->magic == 0x45455246) {
            DEBUG_PRINTF("%08x-%08x %8x  ---- --------\n", start, next, rest);
        } else {
            char *src = p->src;
            int line = (int)p->end;

            if (src == 0)
                src = "";
            if (line < 0)
                DEBUG_PRINTF("%08x-%08x %8x  ---- %s\n", start, next, rest, src);
            else
                DEBUG_PRINTF("%08x-%08x %8x (%4d)%s\n", start, next, rest, line, src);
        }
    }
    DEBUG_PRINT("----------------------------------------\n");
    heap_info_print(heap);
    DEBUG_PRINT("========================================\n");
    return 0;
}

/* 0x00122520 */
void heap_map_dump_all(void)
{
    HeapBaseNextView *heap;
    HeapDumpArg argv;

    if (heap_list != 0) {
        argv.mode = 0;
        heap = heap_list;
        do {
            argv.name = heap->name;
            heap_map_dump(2, &argv);
            heap = heap->next;
        } while (heap != 0);
    }
}

/* Name, range, size, allocations, free size, largest free block, free blocks. */
/* 0x00122578 */
void heap_info_print(HeapBase *heap)
{
    char *name = heap->name;
    char *end;
    int old;

    if (name == 0)
        name = "";
    end = heap->end;
    DEBUG_PRINTF("     Name : %s\n", name);
    DEBUG_PRINTF("    Start : %08x\n", (char *)heap + 64);
    DEBUG_PRINTF("      End : %08x\n", end);
    DEBUG_PRINTF("     Size : %08x\n", end - (char *)heap);
    old = heap_set_current(heap);
    DEBUG_PRINTF("MallocNum : %d\n", heap_alloc_count());
    DEBUG_PRINTF(" FreeSize : %08x\n", heap_free_size());
    DEBUG_PRINTF("  MaxSize : %08x\n", heap_max_free());
    DEBUG_PRINTF("   Divide : %d\n", heap_free_blocks());
    heap_set_current((HeapBase *)old);
}

/* Range, size, source file and line. */
/* 0x00122660 */
void heap_block_dump(HeapBlkHdr *blk)
{
    char *src;
    char *end;
    int total;

    src = blk->src;
    if (src == 0)
        src = "";
    end = (char *)blk + blk->size;
    total = blk->size + 64;
    DEBUG_PRINTF("HEAP_HEAD  %08x\n", blk);
    DEBUG_PRINTF("    start  %08x\n", (char *)blk + 64);
    DEBUG_PRINTF("    end    %08x\n", end);
    DEBUG_PRINTF("    size   %08x\n", total);
    DEBUG_PRINTF("    source %s(%4d)\n", src, blk->line);
}

/* Reports headers that are neither FREE nor HEAP. 1 when intact. */
/* 0x00122708 */
int heap_check(HeapView *heap)
{
    int ok = 1;
    int bad = 0;
    HeapView *blk;
    HeapView *next;

    for (blk = (HeapView *)((char *)heap + 64); (char *)blk < heap->end; blk = next) {
        next = (HeapView *)((char *)blk + blk->size);
        if (blk->magic != 0x45455246 && blk->magic != 0x50414548) {
            const char *msg = "!!!!!!!![ Illegal Memory ]!!!!!!!!\n";

            ok = 0;
            ERROR_PRINTF(msg);
            heap_block_dump(blk);
            bad++;
        }
    }
    if (bad == 0) {
        heap_free_size();
        heap_free_size_of(module_get_heap());
    } else {
        ERROR_PRINTF("OverRunCheck (E) : [%d]\n", bad);
    }
    return ok;
}

/* 1 when all intact. */
/* 0x00122810 */
int heap_check_all(void)
{
    int ok = 1;
    HeapBaseView *heap;

    if (heap_list != 0) {
        heap = heap_list;
        do {
            if (heap_check(heap) == 0)
                ok = 0;
            heap = heap->next;
        } while (heap != 0);
    }
    return ok;
}

/* Every heap. */
/* 0x00122868 */
int heap_check_default(void)
{
    return ((int (*)(int, int))heap_check_all)(0, 0); /* steer(cast) */
}

/* Base heap only. */
/* 0x00122888 */
int heap_is_block(unsigned int addr)
{
    if (addr >= (unsigned int)heap_base_mem && addr < (unsigned int)heap_base_end && *(int *)(addr - 64) == 0x50414548)
        return 1;
    return 0;
}

/* Per heap: largest free and total free against size; text: initial, largest, allocs, blocks. */
/* steer(cast) */
/* 0x001228c8 */
void heap_draw_bars(float x, float y, float z)
{
    HeapView *heap = heap_get_default();
    float dy = 4.0f;
    float text_z = ((float (*)(float, int))float_add_bits16)(z, 2);
    char tag[2];
    char buf[32];

    tag[1] = 0;
    while (heap != 0) {
        int size;
        unsigned int free;
        unsigned int max;
        int nmalloc;

        heap_set_current(heap);
        size = heap->end - (char *)heap;
        free = heap_free_size();
        max = heap_max_free();
        tag[0] = *heap->name;
        debug_draw_gauge_dual(z, x, y, 136.0f, 4.0f,
                     (float)(int)max / (float)size,
                     (float)(int)free / (float)size - (float)(int)max / (float)size,
                     0x80ff0000, 0x800000ff, 0x80ffffff);
        y += dy;
        nmalloc = heap_alloc_count();
        sprintf(buf, "%s %06x B%3d D%2d", tag, max, nmalloc, heap_free_blocks());
        font_set(0);
        font_print(x, y, text_z, COL_WHITE, buf);
        y += 13.0f;
        heap = heap_next(heap);
    }
    heap_set_current(0);
}

/* Texture counts; per heap free size, allocations, free blocks. */
/* 0x00122a90 */
void heap_debug_draw(void *ctx)
{
    int info[8];
    int cell_w, line;
    int y;
    HeapBaseFreeView *heap;

    gw_get_cell_size(&cell_w, &line);
    ulTextureGetInfo(info);
    y = 0;
    gw_win_printf(ctx, 4, y, "Tex Num %8d/%8d", info[3], info[4]);
    y += line;
    gw_win_printf(ctx, 4, y, "Tex Last %08x", info[6]);
    y += line;
    for (heap = heap_list; heap != 0; heap = heap_next(heap)) {
        heap_set_current(heap);
        gw_win_text(ctx, 4, y, heap->name);
        gw_win_printf(ctx, 4, y, "        %08x BLOCK%4d DIV%4d\n", heap_free_size(), heap_alloc_count(), heap_free_blocks());
        y += line;
    }
    heap_set_current(0);
}

/* 0x00122bc8 */
void heap_menu_toggle(void)
{
    int cell_w, cell_h;
    int win_h;
    int win;

    if (heap_window != 0) {
        gw_window_close(heap_window);
        heap_window = 0;
        menu_item_set_value(info_menu_name, "Memory", 0);
        return;
    }
    gw_get_cell_size(&cell_w, &cell_h);
    win_h = cell_h * (heap_count() + 9);
    win = gw_window_open("Memory", 95, (int)(screen_layout.left + 4.0f),
                       (int)(screen_layout.bottom - (float)win_h - 4.0f), cell_w * 35, win_h, 0);
    heap_window = win;
    if (win != 0) {
        gw_window_set_handlers(win, 0, heap_debug_draw, 0);
        menu_item_set_value(info_menu_name, "Memory", 1);
    }
}

/* Memory and HeapMap info menu items; heap console commands. */
/* 0x00122ce0 */
void heap_menu_register(void)
{
    menu_item_add(info_menu_name, "Memory", heap_menu_toggle, 0, 0);
    menu_item_add(info_menu_name, "HeapMap", heap_map_dump_all, 0, 0);
    gw_command_add_table(heap_menu_items);
}
