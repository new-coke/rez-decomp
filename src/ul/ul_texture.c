/*
 * Texture manager: fixed table of records with VRAM surfaces, from data-file texture blocks;
 * upload via display list or direct DMA, reload after VRAM rebuilds, free per list or singly.
 */

#include "types_ps.h"
#include "types_ul.h"
#include "libc.h"
#include "libc_internal.h"
#include "ul_texture.h"
#include "ps_displist.h"
#include "ps_gs.h"
#include "ps_gxpkt.h"
#include "ps_image.h"
#include "ps_pkt.h"
#include "ps_surf.h"
#include "ps_vheap.h"
#include "ul_blk.h"
#include "ul_gx.h"

typedef struct TexListEnt {
    int a;
    int b;
    int tex;
} TexListEnt;

typedef struct TexList {
    TexListEnt *tab;
    int count;
} TexList;

typedef struct UlTextureTexListEnt {
    int a;
    int b;
    UlTexture *obj;
} UlTextureTexListEnt;

typedef struct TexSurf {
    char pad_00[44];
    int w;                  /* 44 */
    int h;                  /* 48 */
} TexSurf;

typedef struct { int w[8]; } UlTexName;

typedef struct UlTextureTexSurf {
    char pad_00[44];
    int w;                  /* 44 */
    char pad_30[16];
    int h;                  /* 64 */
} UlTextureTexSurf;

extern UlTexMgr ulTextureTable;
extern char ulTextureAssertStr[];
extern int ulTextureMax;

/* 0 when idx is out of range. */
/* 0x001b79d8 */
int ulTexListGet(TexList *list, int idx)
{
    if (idx >= 0 && idx < list->count)
        return list->tab[idx].tex;
    return 0;
}

/* In use only; else 0. */
/* 0x001b7a10 */
UlTexture *ulTextureFindById(int id)
{
    UlTexture *tex;
    int i;

    tex = ulTextureTable.tex;
    for (i = 0; i < ulTextureTable.max; i++, tex++) {
        if ((tex->flags & 1) && tex->id == id)
            return tex;
    }
    return 0;
}

/* The id ulTextureFindById matches. */
/* 0x001b7a68 */
void ulTextureSetId(UlTexture *tex, int id)
{
    if (!tex) {
        __assert("ul_texture.c", 135, ulTextureAssertStr);
        return;
    }
    tex->id = id;
}

/* 0x001b7a90 */
void ulTexListInit(int *list, int ent, int count)
{
    list[0] = ent;
    list[1] = count;
}

/* 0x001b7aa0 */
void ulTexListReload(TexList *list)
{
    UlTextureTexListEnt *ent;
    int i;

    ent = list->tab;
    for (i = 0; i < list->count; i++) {
        ulTextureLoadImage(ent->obj, ent->obj->image);
        ent++;
    }
}

/* GS words; 0 unless in use. */
/* 0x001b7b08 */
int ulTextureGetSize(UlTexture *tex)
{
    if (tex->flags & 1)
        return tex->surf.total;
    return 0;
}

/* GS words; 0 unless in use. */
/* 0x001b7b28 */
int ulTextureGetAddr(UlTexture *tex)
{
    if (tex->flags & 1)
        return tex->surf.base;
    return 0;
}

/* 0 unless in use. */
/* 0x001b7b48 */
int ulTextureGetWidth(UlTexture *tex)
{
    if (tex->flags & 1)
        return tex->surf.w;
    return 0;
}

/* 0 unless in use. */
/* 0x001b7b68 */
int ulTextureGetHeight(UlTexture *tex)
{
    if (tex->flags & 1)
        return tex->surf.h;
    return 0;
}

/* 0x001b7b88 */
void ulTextureStub(void)
{
}

/* 0x001b7b90 */
void ulTextureSetName(UlTexture *tex, char *name)
{
    strncpy(tex->name, name, 31);
}

/* 0x001b7ba0 */
void ulTextureSetImage(UlTexture *tex, int image)
{
    tex->image = image;
}

/* 0x001b7ba8 */
void ulTextureClearVramHeap(void * heap)
{
    psSurfClearVramHeap(heap);
}

/* steer(cast) */
/* 0x001b7bb0 */
int ulTextureInitVramHeap(int work, int size, int vram, int vsize, int n)
{
    return ((int (*)(int, int, int, int, int))psSurfInitVramHeap)(work, size, vram, vsize, n);
}

/* At the header's offset, counted from 32 bytes before desc. */
/* 0x001b7bd0 */
char *ulTextureDescImage(char *desc)
{
    return desc + (*(int *)(desc + 20) - 32);
}

/* Swaps with the last used free-list entry so used entries stay packed, then clears. */
/* 0x001b7be0 */
void ulTextureFreeSlot(UlTexture *tex)
{
    UlTexMgr *mgr;
    UlTexture **tab;
    UlTexture *last;
    int slot;

    mgr = &ulTextureTable;
    if (!(tex->flags & 1)) {
        __assert("ul_texture.c", 369, ulTextureAssertStr);
        return;
    }
    tab = mgr->freelist;
    slot = tex->slot;
    last = tab[mgr->next - 1];
    tab[mgr->next - 1] = tex;
    tab[slot] = last;
    last->slot = slot;
    memset(tex, 0, sizeof(UlTexture));
    mgr->next--;
}

/* 0 when full. */
/* 0x001b7c80 */
UlTexture *ulTextureAllocSlot(void)
{
    UlTexture *tex;

    if (ulTextureTable.next < ulTextureTable.max) {
        tex = ulTextureTable.freelist[ulTextureTable.next];
        tex->flags |= 1;
        tex->slot = ulTextureTable.next;
        ulTextureTable.next++;
        return tex;
    }
    return 0;
}

/* Only with flag 4 and an image. */
/* 0x001b7cd8 */
void ulTextureReload(UlTexture *tex)
{
    int image;

    if (tex->flags & 4) {
        image = tex->image;
        if (image)
            ulTextureLoadImage(tex, image);
    }
}

/* Direct DMA. */
/* 0x001b7d08 */
void ulTextureLoadImageDirect(void *tex, void *image)
{
    psImagePktAddTexture(tex, image);
}

/* Via the display list. */
/* 0x001b7d10 */
void ulTextureLoadImage(void *tex, void *image)
{
    psImageDlAddTexture(tex, image);
}

/* Locked; descriptor follows the block header. */
/* 0x001b7d18 */
int ulTextureCreateFromHeader(char *hdr)
{
    return ulTextureCreateLocked(hdr + 32);
}

/* Empty surface sized and formatted from desc's header. */
/* 0x001b7d38 */
int ulTextureCreateFromDesc(char *desc)
{
    int req[5];

    req[0] = *(short *)(desc + 16);
    req[1] = *(short *)(desc + 18);
    req[2] = (unsigned char)desc[22];
    req[3] = (unsigned char)desc[20] | ((unsigned char)desc[21] << 8);
    return ulTextureCreateSurface(req);
}

/*
 * Locked: only ulTextureFreeLocked releases it. Bug: a failed surface allocation leaks the record.
 */
/* 0x001b7d88 */
UlTexture *ulTextureCreateLocked(char *desc)
{
    UlTexture *tex;

    tex = ulTextureAllocSlot();
    if (tex && psSurfAlloc(desc, tex)) {
        tex->flags |= 2;
        tex->image = ulTextureDescImage(desc);
        return tex;
    }
    return 0;
}

/* VRAM surface plus the image data after desc. 0 when the table or VRAM is full. */
/* 0x001b7df0 */
UlTexture *ulTextureCreate(char *desc)
{
    UlTexture *tex;

    tex = ulTextureAllocSlot();
    if (tex == 0)
        return 0;
    if (psSurfAlloc(desc, tex)) {
        tex->image = ulTextureDescImage(desc);
        return tex;
    }
    ulTextureFreeSlot(tex);
    return 0;
}

/*
 * req: width, height, format, two packed format bytes. 0 with a warning if VRAM is full (record
 * leaks).
 */
/* 0x001b7e60 */
UlTexture *ulTextureCreateSurface(int *req)
{
    UlTexture *tex;
    int surf_req[5];

    tex = ulTextureAllocSlot();
    if (tex) {
        surf_req[0] = req[0];
        surf_req[1] = req[1];
        surf_req[2] = req[2];
        surf_req[3] = req[3] & 0xff;
        surf_req[4] = (req[3] & 0xff00) >> 8;
        if (psSurfAlloc(surf_req, tex))
            return tex;
        printf("WARNING in ulTextureCreateSurface : teture memory not enough!\n");
    }
    return 0;
}

/* Last entry first; clears the entries. Returns 1. */
/* 0x001b7ee8 */
int ulTexListFree(TexList *list)
{
    int i;
    UlTextureTexListEnt *ent;
    UlTextureTexListEnt *end;

    if (list->tab) {
        i = 0;
        end = list->tab + list->count;
        ent = end - 1;
        for (; i < list->count; ) {
            i++;
            if (ent->obj) {
                ulTextureFree(ent->obj);
                ent->obj = 0;
            }
            ent--;
        }
    }
    return 1;
}

/* Locked textures are refused with an error. */
/* 0x001b7f70 */
void ulTextureFree(UlTexture *tex)
{
    if (tex->flags & 2) {
        printf("ERROR: this texture is locked texture!\n");
        return;
    }
    psSurfFree(tex);
    ulTextureFreeSlot(tex);
}

/* Locked; a failed allocation releases the record. */
/* 0x001b7fc0 */
UlTexture *ulTextureCreateSurfaceLocked(int *req)
{
    UlTexture *tex;
    int surf_req[5];

    tex = ulTextureAllocSlot();
    if (tex) {
        surf_req[0] = req[0];
        surf_req[1] = req[1];
        surf_req[2] = req[2];
        surf_req[3] = req[3] & 0xff;
        surf_req[4] = (req[3] & 0xff00) >> 8;
        if (psSurfAlloc(surf_req, tex)) {
            tex->flags |= 2;
            return tex;
        }
        ulTextureFreeSlot(tex);
    }
    return 0;
}

/* Unlocked ones refused. Bug: the record never returns to the free list. */
/* 0x001b8058 */
void ulTextureFreeLocked(UlTexture *tex)
{
    if (!(tex->flags & 2)) {
        printf("ERROR: not locked texture!\n");
        return;
    }
    psSurfFree(tex);
    memset(tex, 0, 176);
}

/* Searches only the used part of the free list. */
/* 0x001b80b0 */
UlTexture *ulTextureFindUsedById(int id)
{
    UlTexMgr *mgr;
    UlTexture **p;
    UlTexture *tex;
    int i;

    mgr = &ulTextureTable;
    p = mgr->freelist;
    for (i = 0; i < mgr->next; i++) {
        tex = *p;
        if ((tex->flags & 1) && tex->id == id)
            return tex;
        p++;
    }
    return 0;
}

/* 0 past the last. */
/* 0x001b8110 */
UlTexture *ulTextureGetUsed(int i)
{
    if (i < ulTextureTable.next)
        return ulTextureTable.freelist[i];
    return 0;
}

/* 0x001b8148 */
int ulTextureGetRecords(void)
{
    return (*(int *)&ulTextureTable); /* steer(cast) */
}

/* 0x001b8158 */
int ulTextureGetMax(void)
{
    return ulTextureMax;
}

/* 0 unless in use. Bug: lets i == max through, one past the end. */
/* 0x001b8168 */
UlTexture *ulTextureGet(int i)
{
    UlTexture *tex;

    if (ulTextureTable.max < i)
        return 0;
    tex = &ulTextureTable.tex[i];
    if (!(tex->flags & 1))
        tex = 0;
    return tex;
}

/* 0x001b81a8 */
int ulTextureGetCount(void)
{
    return (*(UlTexPool *)&ulTextureTable).used; /* steer(cast) */
}

/* Count, capacity, VRAM used, highest-placed texture and its end address. */
/* 0x001b81b8 */
void ulTextureGetInfo(UlTexInfo *info)
{
    UlTex *best;
    UlTex *tex;
    int i;

    info->used = (*(UlTexPool *)&ulTextureTable).used; /* steer(cast) */
    info->cap = (*(UlTexPool *)&ulTextureTable).count; /* steer(cast) */
    info->total = 0;
    best = 0;
    for (i = 0; i < (*(UlTexPool *)&ulTextureTable).used; i++) { /* steer(cast) */
        tex = (*(UlTexPool *)&ulTextureTable).table[i]; /* steer(cast) */
        info->total += tex->total << 2;
        if (best) {
            if (best->base < tex->base)
                best = tex;
        } else {
            best = tex;
        }
    }
    info->last = best;
    if (best)
        info->end = best->base + best->total;
    else
        info->end = 0;
}

/* Size, mip levels, format, VRAM address, name. */
/* 0x001b8260 */
void ulTextureDump(void)
{
    UlTex *tex;
    int i;

    printf("---- texture info ----\n");
    printf("size      mxl psm  addr       name\n");
    tex = (*(UlTexPool *)&ulTextureTable).first; /* steer(cast) */
    for (i = 0; i < (*(UlTexPool *)&ulTextureTable).count; i++, tex++) { /* steer(cast) */
        if (tex->flags & 1)
            printf("%3d x %3d, %2d, %2d, 0x%05x, %s\n", tex->w, tex->h, tex->mxl, tex->psm,
                   psVHeapBlockAddr(tex->blk), tex->name);
    }
}

/* 0x001b8318 */
void ulTextureRestoreVramHeap(void)
{
    psGsRestoreVramHeap();
}

/* steer(cast) */
/* 0x001b8320 */
int ulTextureGetVramHeap(int heap)
{
    return ((int (*)(int))psSurfGetVramHeap)(heap);
}

/* 0x001b8340 */
int ulTextureSetVramHeap(int heap)
{
    return psSurfSetVramHeap(heap);
}

/* Records themselves untouched. */
/* 0x001b8360 */
void ulTextureClearTable(void)
{
    memset(&ulTextureTable, 0, 32);
}

/* n records, then n free-list pointers to them in order. */
/* 0x001b8378 */
void ulTextureInitTable(UlTex *buf, int n)
{
    UlTexPool *pool = &(*(UlTexPool *)&ulTextureTable); /* steer(cast) */
    UlTex **tbl;
    UlTex *tex;
    int i;

    memset(pool, 0, 32);
    (*(UlTexPool *)&ulTextureTable).first = buf; /* steer(cast) */
    pool->base = buf;
    pool->count = n;
    pool->used = 0;
    tbl = (UlTex **)(buf + n);
    pool->list = (void **)tbl;
    pool->table = tbl;
    tex = buf;
    for (i = 0; i < pool->count; i++) {
        pool->table[i] = tex;
        tex++;
    }
}

/* Empty surface per texture block of the section, into its slot table. */
/* 0x001b8428 */
void ulTextureCreateSection(UlSlot **table, UlSecHdr *hdr)
{
    void *blk;
    UlSlot *slot;
    int i;

    blk = ulBlkNext(hdr);
    slot = *table;
    for (i = 0; i < hdr->count; i++) {
        slot->obj = ulTextureCreateFromDesc(blk);
        blk = ulBlkNext(blk);
        slot++;
    }
}

/* 0 if none. */
/* 0x001b84b0 */
UlBlk *ulTextureFindBlock(UlSecHdr *hdr, int key)
{
    UlBlk *blk;
    int i;

    blk = ulBlkNext(hdr);
    for (i = 0; i < hdr->count; i++) {
        if (blk->key == key)
            return blk;
        blk = ulBlkNext(blk);
    }
    return 0;
}

/* blk->off bytes in; 0 for no block. */
/* 0x001b8520 */
char *ulTextureBlockImageOrNull(Rel *blk)
{
    if (blk)
        return (char *)blk + blk->off;
    return 0;
}

/* Flushes the texture cache; reloads textures ending above VRAM address limit (overwritten). */
/* 0x001b8540 */
void ulTextureReloadSectionAbove(UlSecHdr *hdr, UlSlot **table, int limit)
{
    void *blk;
    UlSlot *slot;
    int i;

    psGsWriteReg(63, 0);
    blk = ulBlkNext(hdr);
    slot = *table;
    for (i = 0; i < hdr->count; i++) {
        if (limit < ((UlTex *)slot->obj)->base + ((UlTex *)slot->obj)->total)
            psImageDlAddTexture(slot->obj, ulTextureBlockImage(blk));
        blk = ulBlkNext(blk);
        slot++;
    }
}

/* Flushes the texture cache first. */
/* 0x001b8600 */
void ulTextureReloadSection(UlSecHdr *hdr, UlSlot **table)
{
    void *blk;
    UlSlot *slot;
    int i;

    psGsWriteReg(63, 0);
    blk = ulBlkNext(hdr);
    slot = *table;
    for (i = 0; i < hdr->count; i++) {
        psImageDlAddTexture(slot->obj, ulTextureBlockImage(blk));
        blk = ulBlkNext(blk);
        slot++;
    }
}

/* Flushes the texture cache first. */
/* 0x001b8698 */
void ulTextureReloadBlock(void *dst, void *src)
{
    psGsWriteReg(63, 0);
    psImageDlAddTexture(dst, ulTextureBlockImageOrNull(src));
}

/* Via the display list. Warns if surf is smaller than the pictures or no is past the last. */
/* 0x001b86e0 */
void ulTextureLoadPicture(TexSurf *surf, PicHdr *pic, int no)
{
    if (surf->w < pic->w || surf->h < pic->h) {
        printf("WARNINIG: texture surface is too small!\n");
    } else if (no >= pic->count) {
        printf("WARNINIG: pictureNo is too large! [%d]\n", no);
    } else {
        ulTextureUpload(surf, pic->w, pic->h, (char *)pic + 32 + pic->size * no);
    }
}

/* Direct DMA. */
/* 0x001b8748 */
void ulTextureLoadPictureDirect(TexSurf *surf, PicHdr *pic, int no)
{
    if (surf->w < pic->w || surf->h < pic->h) {
        printf("WARNINIG: texture surface is too small!\n");
    } else if (no >= pic->count) {
        printf("WARNINIG: pictureNo is too large! [%d]\n", no);
    } else {
        ulTextureUploadDirect(surf, pic->w, pic->h, (char *)pic + 32 + pic->size * no);
    }
}

/* blk->off bytes in. */
/* 0x001b87b0 */
char *ulTextureBlockImage(Rel *blk)
{
    return (char *)blk + blk->off;
}

/* Queues the image upload on the display list; takes the block's id. */
/* 0x001b87c0 */
UlTex *ulTextureLoadBlock(UlTexBlk *blk)
{
    UlTex *tex;

    tex = ulTextureCreateFromDesc(blk);
    ulTextureLoadImage(tex, ulTextureBlockImage(blk));
    tex->id = blk->id;
    return tex;
}

/* 0x001b8810 */
void ulTextureLoadSection(UlSecHdr *hdr, UlSlot **table)
{
    void *blk;
    UlSlot *slot;
    int i;

    blk = ulBlkNext(hdr);
    slot = *table;
    for (i = 0; i < hdr->count; i++) {
        slot->obj = ulTextureLoadBlock(blk);
        blk = ulBlkNext(blk);
        slot++;
    }
}

/* Direct DMA. */
/* 0x001b8890 */
UlTex *ulTextureLoadBlockDirect(UlTexBlk *blk)
{
    UlTex *tex;

    tex = ulTextureCreateFromDesc(blk);
    ulTextureLoadImageDirect(tex, ulTextureBlockImage(blk));
    tex->id = blk->id;
    return tex;
}

/* Direct DMA; names from the 32-byte name table after the section header. */
/* 0x001b88e0 */
void ulTextureLoadSectionDirect(UlSecHdr *hdr, UlSlot **table)
{
    void *blk;
    UlSlot *slot;
    int i;
    UlTexName *rec;

    blk = ulBlkNext(hdr);
    slot = *table;
    rec = (UlTexName *)((char *)hdr + 12);
    for (i = 0; i < hdr->count; i++) {
        slot->obj = ulTextureLoadBlockDirect(blk);
        if (slot->obj)
            ulTextureSetName(slot->obj, &rec[i]);
        blk = ulBlkNext(blk);
        slot++;
    }
}

/*
 * 32-bit pixels via the display list: transfer setup, then an image packet referencing data.
 * Returns 0.
 */
/* 0x001b8988 */
int ulTextureUploadBand(void *dev, unsigned long packed, int y, int x, int width, int height, char *data)
{
    unsigned long tag[2];
    unsigned long img[2];
    unsigned long trxpos;
    unsigned long trxreg;
    unsigned short *cnt;
    unsigned long *pkt;
    void *buf;
    int qwc;

    trxreg = width | ((unsigned long)height << 32);
    img[1] = 0;
    trxpos = ((unsigned long)y << 32) | ((unsigned long)x << 48);
    qwc = width * height / 4;
    tag[0] = 0x1000000000000004UL;
    tag[1] = 14;
    img[0] = (qwc | 0x8000) | (0x8000UL << 44);
    buf = psDisplayListOpenRefPkt(dev);
    cnt = psVifPktOpenGifPkt(buf);
    psVifPktAddGifTag(buf, tag);
    psVifPktAddRegAD(buf, 80, packed);
    psVifPktAddRegAD(buf, 81, trxpos);
    psVifPktAddRegAD(buf, 82, trxreg);
    psVifPktAddRegAD(buf, 83, 0);
    psVifPktCountGifQwords(buf);
    pkt = psVifPktAddGifTag(buf, img);
    psVifPktAdvanceGif(buf, pkt + 2, 0);
    psVifPktCloseGifPkt(buf);
    *cnt += qwc;
    psDisplayListCloseRefPkt(dev);
    psDisplayListAddRefPkt(dev, data, qwc);
    return 0;
}

/*
 * Bands of 16 x 240 pixels, stepping by 16 up to n; height unused. Buffer from surf's
 * address/width.
 */
/* 0x001b8b00 */
void ulTextureUpload(UlTextureTexSurf *surf, int n, int height, char *data)
{
    void *dev;
    unsigned long packed;
    int wb;
    int hb;
    int y;

    y = 0;
    dev = psGsGetDisplayList();
    wb = surf->w / 64;
    hb = surf->h / 64;
    if (wb == 0)
        wb = 1;
    packed = ((unsigned long)hb << 32) | ((unsigned long)wb << 48);
    for (y = 0; y < n; y += 16) {
        ulTextureUploadBand(dev, packed, y, 0, 16, 240, data);
        data += 15360;
    }
}

/* Direct DMA packet dev: transfer setup, then a DMA tag sending straight from data. */
/* 0x001b8bc8 */
int ulTextureUploadBandDirect(void *dev, unsigned long packed, int y, int x, int width, int height, unsigned int data)
{
    unsigned long img[2];
    unsigned long cnt_tag[2];
    unsigned long ref_tag[2];
    unsigned long trxpos;
    unsigned long trxreg;
    int qwc;

    qwc = width * height / 4;
    trxpos = ((unsigned long)y << 32) | ((unsigned long)x << 48);
    trxreg = width | ((unsigned long)height << 32);
    cnt_tag[0] = 0x10000006UL;
    img[0] = (qwc | 0x8000) | (0x8000UL << 44);
    ref_tag[0] = (qwc | 0x30000000) | ((unsigned long)data << 32);
    psGxPktOpenTag(dev, cnt_tag);
    psGxPktAddRegADGifTag(dev, 4);
    psGxPktAddRegAD(dev, 80, packed);
    psGxPktAddRegAD(dev, 81, trxpos);
    psGxPktAddRegAD(dev, 82, trxreg);
    psGxPktAddRegAD(dev, 83, 0);
    psGxPktAddGifTag(dev, img);
    psGxPktOpenTag(dev, ref_tag);
    return 0;
}

/* Direct DMA through ulGxLockBlock1. */
/* 0x001b8cd8 */
void ulTextureUploadDirect(UlTextureTexSurf *surf, int n, int height, char *data)
{
    unsigned long packed;
    char *dev;
    int wb;
    int hb;
    int y;

    y = 0;
    wb = surf->w / 64;
    hb = surf->h / 64;
    if (wb == 0)
        wb = 1;
    dev = ulGxLockBlock1;
    packed = ((unsigned long)hb << 32) | ((unsigned long)wb << 48);
    for (y = 0; y < n; y += 16) {
        ulTextureUploadBandDirect(dev, packed, y, 0, 16, 240, data);
        data += 15360;
    }
}
