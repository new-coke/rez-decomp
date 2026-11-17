/* Texture surfaces in VRAM: sizes mip levels and clut, allocates, builds TEX0/TEX1/MIPTBP. */

#include "types_ps.h"
#include "libc.h"
#include "ps_surf.h"
#include "ps_displist.h"
#include "ps_gs.h"
#include "ps_vheap.h"

typedef struct PsTexRec {
    unsigned long tex0;      /* 0 */
    unsigned long tex1;      /* 8 */
    unsigned long miptbp1;     /* 16 */
    unsigned long miptbp2;     /* 24 */
    char pad_20[20];
    int nextra;                 /* 52 mip levels beyond the first */
} PsTexRec;

extern PsVramState psSurfState;

/* Keeps the shadow's TFX, MMAG, MMIN. MIPTBP1/2_1 only with more than one level. */
/* 0x001b3578 */
void psSurfBind(PsTexRec *rec)
{
    PsDispList *dl;
    NjGsState *st;
    unsigned long new_tex0, old_tex0, new_tex1, old_tex1;

    st = &psSurfTexShadow;
    dl = psGsGetDisplayList();
    old_tex0 = psSurfTexShadow.tex0;
    new_tex0 = rec->tex0;
    old_tex1 = st->tex1;
    new_tex1 = rec->tex1;
    psSurfTexShadow.tex0 = (old_tex0 & 0x1800000000UL) | (new_tex0 & ~0x1800000000UL);
    st->tex1 = (old_tex1 & 0x1e0UL) | (new_tex1 & ~0x1e0UL);
    psDisplayListSetGsReg(dl, 6, psSurfTexShadow.tex0);
    psDisplayListUpdateGsReg(dl, 20, st->tex1);
    if (rec->nextra > 0) {
        psDisplayListUpdateGsReg(dl, 52, rec->miptbp1);
        psDisplayListUpdateGsReg(dl, 54, rec->miptbp2);
    }
}

/* 32 for CT32/CT24/T8H/T4HL/T4HH, 16 CT16, 8 T8, 4 T4, else 0. */
/* 0x001b3670 */
static int psSurfPsmBits(unsigned int psm)
{
    switch (psm) {
    case 0:
    case 1:
    case 27:
    case 36:
    case 44:
        return 32;
    case 2:
        return 16;
    case 19:
        return 8;
    case 20:
        return 4;
    }
    return 0;
}

/* 0..31; -1 when mask is 0. */
/* 0x001b36c0 */
static int psSurfLowestBit(int mask)
{
    int i;

    for (i = 0; i < 32; i++) {
        if (mask & 1)
            return i;
        mask >>= 1;
    }
    return -1;
}

/* 0x001b36f0 */
static int psSurfStub(void)
{
    return 0;
}

/* 0x001b36f8 */
void psSurfFree(PsSurf *surf)
{
    if (surf->blk) {
        psVHeapFree(surf->blk);
        surf->blk = 0;
    }
}

/* Words: 256 entries for T8/T8H, 16 for T4/T4HL/T4HH, each clut_psm wide; else 0. */
/* 0x001b3730 */
static int psSurfClutSize(unsigned int psm, unsigned int clut_psm)
{
    int bits;
    int n;

    bits = psSurfPsmBits(clut_psm);
    switch (psm) {
    case 19:
    case 27:
        n = bits << 5;
        break;
    case 20:
    case 36:
    case 44:
        n = bits << 1;
        break;
    default:
        n = 0;
        break;
    }
    return n / 4;
}

/* 64-word blocks rounded up; T8 and T4 round to an even count; 0 for unknown psm. */
/* 0x001b37b0 */
static int psSurfWordsToBlocks(unsigned int psm, int words)
{
    switch (psm) {
    case 0:
    case 1:
    case 2:
    case 10:
    case 27:
    case 36:
    case 44:
    case 48:
    case 49:
    case 50:
    case 58:
        return (words + 63) / 64;
    case 19:
    case 20:
        words = (words + 63) / 64;
        return words + (words & 1);
    }
    return 0;
}

/* Levels down to width 8, each at least 64 words; clut after the levels. 0 if no room. */
/* 0x001b3820 */
PsSurf *psSurfAlloc(int *req, PsSurf *surf)
{
    int i;
    int lw;
    int lh;
    int width;
    int height;
    int mxl;
    PsVramHeap *heap;
    int bpp;
    int cum;
    int psm;
    int cpsm;
    int n;
    int end;
    int tw[8];
    int th[8];
    int pg[8];
    int tbw[8];

    heap = psSurfState.heap;
    cum = 0;
    surf->w = req[0];
    surf->h = req[1];
    psm = req[3];
    surf->psm = psm;
    mxl = req[2];
    lw = psSurfLowestBit(req[0]);
    lh = psSurfLowestBit(req[1]);
    cpsm = req[4];
    surf->total = 0;
    surf->cpsm = cpsm;
    bpp = psSurfPsmBits(surf->psm);
    width = req[0];
    height = req[1];
    for (i = 0; i <= mxl; i++, lw--, lh--) {
        tbw[i] = psSurfWordsToBlocks(psm, width);
        tw[i] = lw;
        th[i] = lh;
        surf->off[i] = cum;
        n = width * height * bpp;
        surf->size[i] = n / 32;
        if (n / 32 < 64)
            surf->size[i] = 64;
        do { } while (0);
        width >>= 1;
        height >>= 1;
        cum += surf->size[i];
        surf->total += surf->size[i];
        if (width < 8) {
            mxl = i;
            break;
        }
    }
    surf->mxl = mxl;
    surf->end = cum;
    surf->total += psSurfClutSize(req[3], req[4]);
    surf->blk = psVHeapAlloc(heap, surf->total);
    if (surf->blk == 0)
        return 0;
    surf->base = psVHeapBlockAddr(surf->blk);
    for (i = 0; i <= mxl; i++) {
        surf->off[i] += surf->base;
        pg[i] = surf->off[i] / 64;
    }
    end = surf->end + surf->base;
    surf->tex0 = (unsigned long)pg[0] | ((unsigned long)tbw[0] << 14) | ((unsigned long)psm << 20)
        | ((unsigned long)tw[0] << 26) | ((unsigned long)th[0] << 30) | ((unsigned long)1 << 34)
        | ((unsigned long)(end / 64) << 37) | ((unsigned long)cpsm << 51) | ((unsigned long)1 << 61);
    surf->tex1 = (long)mxl << 2;
    surf->miptbp1 = (unsigned long)pg[1] | ((unsigned long)tbw[1] << 14) | ((unsigned long)pg[2] << 20)
        | ((unsigned long)tbw[2] << 34) | ((unsigned long)pg[3] << 40) | ((unsigned long)tbw[3] << 54);
    surf->miptbp2 = (unsigned long)pg[4] | ((unsigned long)tbw[4] << 14) | ((unsigned long)pg[5] << 20)
        | ((unsigned long)tbw[5] << 34) | ((unsigned long)pg[6] << 40) | ((unsigned long)tbw[6] << 54);
    surf->end = end;
    return surf;
}

/* 0x001b3ba8 */
static int psSurfGetReserved(void)
{
    return psSurfState.rsv_size;
}

/* 0x001b3bb8 */
static void psSurfDumpVramHeap(void)
{
    psVHeapDump(psSurfState.heap);
}

/* 0x001b3bc8 */
PsVramHeap *psSurfGetVramHeap(void)
{
    return psSurfState.heap;
}

/* Returns the previous one. */
/* 0x001b3bd8 */
PsVramHeap *psSurfSetVramHeap(PsVramHeap *heap)
{
    PsVramHeap *old;

    psSurfState.base = heap->base;
    old = psSurfState.heap;
    psSurfState.heap = heap;
    psSurfState.size = heap->size;
    return old;
}

/* 0x001b3c00 */
void psSurfClearVramHeap(void * heap)
{
    psVHeapClear(heap);
}

/* [base, base + size), headers in buf, 64-word units. */
/* 0x001b3c08 */
int psSurfInitVramHeap(int buf, int bufsize, int base, int size)
{
    return psVHeapInit(buf, bufsize, base, size, 64);
}

/* 0x001b3c28 */
static void psSurfClearState(void)
{
    memset(&psSurfState, 0, 28);
}
