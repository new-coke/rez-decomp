/*
 * GS setup and state: double buffer, main display list, VRAM heaps for system and user textures,
 * frame flip, shadowed render state (alpha/depth test, texture function, filter, clamp, blend).
 */

#include "types_nj.h"
#include "ninja.h"
#include "ps_gs.h"
#include "libc.h"
#include "kernel_syscalls.h"
#include "libgraph.h"
#include "ps_displist.h"
#include "ps_dma.h"
#include "ps_log.h"
#include "ps_pkt.h"
#include "ps_surf.h"

static NjGsDrawEnv *psGsGetDrawEnv(void);

typedef union { unsigned long l; } GsWord;

extern void *psGsMainDisplayList;
extern void *psGsCurDisplayList;
extern void *psGsCurRefPkt;
extern void sceGsSetHalfOffset(NjGsDrawEnv *e, short x, short y, short field);
extern int libgraph_swap_dbuff(NjGsDBuff *db, int id);
extern int psGsPathSynced;
extern NjGxPkt *psGxPktCur;
extern void *psGsPktBuf;
extern char psGsPktMem[];
extern char psGsMainDlMem[];
extern char psGsSysTexHeap[];

/* 0x001b19f0 */
NjDmaTag *psGsAddCallTag(unsigned int addr)
{
    NjDmaTag *tag;

    tag = psDisplayListAddTag(psGsGetDisplayList(), 0x50000000, 0, 0);
    tag->addr = addr;
    return tag;
}

/* 0x001b1a68 */
void *psGsGetMainDisplayList(void)
{
    return psGsMainDisplayList;
}

/* 0x001b1a70 */
void psGsCloseRefPkt(void)
{
    if (psGsCurDisplayList)
        psDisplayListCloseRefPkt(psGsCurDisplayList);
    psGsCurRefPkt = 0;
}

/* 0x001b1a98 */
void *psGsGetRefPkt(void)
{
    return psGsCurRefPkt;
}

/* 0 without a display list. */
/* 0x001b1aa0 */
void *psGsOpenRefPkt(void)
{
    if (psGsCurDisplayList)
        psGsCurRefPkt = psDisplayListOpenRefPkt(psGsCurDisplayList);
    else
        psGsCurRefPkt = 0;
    return psGsCurRefPkt;
}

/* 0x001b1ad8 */
void *psGsGetDisplayList(void)
{
    return psGsCurDisplayList;
}

/* Returns the previous one. */
/* 0x001b1ae0 */
void *psGsSetDisplayList(void *dl)
{
    void *old;

    old = psGsCurDisplayList;
    psGsCurDisplayList = dl;
    return old;
}

/* 0x001b1af0 */
static void psGsWriteSignal(int hi, int lo)
{
    psGsWriteReg(96, (unsigned long)hi << 32 | lo);
}

/* Sets TEST in both draw environments and writes TEST_1. */
/* 0x001b1b00 */
void psGsSetAlphaTest(int ate, int atst, int aref, int afail)
{
    NjGsState *gs = &psSurfTexShadow;
    NjGsDBuff *db = &gs->db;
    unsigned long test_reg, bits;

    test_reg = db->draw0.reg[14];
    bits = test_reg & ~0x3ff1L;
    bits |= (unsigned long)afail << 12;
    bits |= ate | (unsigned long)atst << 1;
    test_reg = bits | (unsigned long)aref << 4;
    db->draw0.reg[14] = test_reg;
    db->draw1.reg[14] = test_reg;
    psGsWriteReg(71, test_reg);
}

/* Copy of psGsSetAlphaTest. */
/* 0x001b1b48 */
static void psGsSetAlphaTest2(int ate, int atst, int aref, int afail)
{
    NjGsState *gs = &psSurfTexShadow;
    NjGsDBuff *db = &gs->db;
    unsigned long test_reg, bits;

    test_reg = db->draw0.reg[14];
    bits = test_reg & ~0x3ff1L;
    bits |= (unsigned long)afail << 12;
    bits |= ate | (unsigned long)atst << 1;
    test_reg = bits | (unsigned long)aref << 4;
    db->draw0.reg[14] = test_reg;
    db->draw1.reg[14] = test_reg;
    psGsWriteReg(71, test_reg);
}

/* Both draw environments. */
/* 0x001b1b90 */
static void psGsSetDither(int on)
{
    NjGsState *gs = &psSurfTexShadow;

    gs->db.draw0.reg[12] = (gs->db.draw0.reg[12] & ~1L) | (on & 1);
    gs->db.draw1.reg[12] = (gs->db.draw1.reg[12] & ~1L) | (on & 1);
}

/* Writes TEST_1 only; the draw environment keeps its old setting. */
/* 0x001b1bc8 */
void psGsSetDepthTest(int zte, int ztst)
{
    NjGsDrawEnv *env = psGsGetDrawEnv();

    psGsWriteReg(71, (zte << 16 | ztst << 17) | (env->reg[14] & ~0x70000L));
}

/* Writes ZBUF_1 from the first draw environment's value. */
/* 0x001b1c20 */
void psGsSetZMask(int zmsk)
{
    psGsWriteReg(78, (psSurfTexShadow.db.draw0.reg[2] & ~(1L << 32)) | (unsigned long)zmsk << 32);
}

/* LCM, L, K in the TEX1 shadow; updates TEX1_1. */
/* 0x001b1c50 */
void psGsSetTexLod(int lcm, int lod_l, int lod_k)
{
    NjGsState *gs = &psSurfTexShadow;

    gs->tex1 = (gs->tex1 & 0xfffff000ffe7fffeUL)
        | ((lcm | (unsigned long)lod_l << 19 | (unsigned long)lod_k << 32) & 0x00000fff00180001UL);
    psDisplayListUpdateGsReg(psGsGetDisplayList(), 20, gs->tex1);
}

/* ALPHA_1 of the current display list. */
/* 0x001b1cd0 */
static void psGsSetBlend(int sel_a, int sel_b, int sel_c, int sel_d, int fix)
{
    psDisplayListUpdateGsReg(psGsGetDisplayList(), 66, (unsigned long)sel_a | (unsigned long)sel_b << 2
        | (unsigned long)sel_c << 4 | (unsigned long)sel_d << 6 | (unsigned long)fix << 32);
}

/* TFX, TCC in the TEX0 shadow; updates TEX0_1. */
/* 0x001b1d50 */
void psGsSetTexFunc(int tfx, int tcc)
{
    NjGsState *gs = &psSurfTexShadow;

    gs->tex0 = (gs->tex0 & ~(7L << 34)) | ((unsigned long)tfx << 35 | (unsigned long)tcc << 34);
    psDisplayListUpdateGsReg(psGsGetDisplayList(), 6, gs->tex0);
}

/* MMAG, MMIN in the TEX1 shadow; updates TEX1_1. */
/* 0x001b1db0 */
void psGsSetTexFilter(int mmag, int mmin)
{
    NjGsState *gs = &psSurfTexShadow;

    gs->tex1 = (mmag << 5 | mmin << 6) | (gs->tex1 & ~0x1e0L);
    psDisplayListUpdateGsReg(psGsGetDisplayList(), 20, gs->tex1);
}

/* CLAMP_1 of the current display list. */
/* 0x001b1e08 */
void psGsSetClamp(int wms, int wmt, int minu, int maxu, int minv, int maxv)
{
    psDisplayListUpdateGsReg(psGsGetDisplayList(), 8, (unsigned long)wms | (unsigned long)wmt << 2
        | (unsigned long)minu << 4 | (unsigned long)maxu << 14 | (unsigned long)minv << 24
        | (unsigned long)maxv << 34);
}

/* Immediate one-register A+D packet; updates the register cache, clears its changed flag. */
/* 0x001b1e98 */
void psGsWriteReg(int reg, unsigned long val)
{
    NjDisplayList *dl;
    void *pkt;

    dl = psGsGetDisplayList();
    pkt = psDisplayListOpenRefPkt(dl);
    psVifPktOpenGifPkt(pkt);
    {
        unsigned long tag[2];

        tag[0] = 0x1000000000000001UL;
        tag[1] = 14;
        psVifPktAddGifTag(pkt, tag);
    }
    psVifPktAddRegAD(pkt, reg, val);
    psVifPktCountGifQwords(pkt);
    psVifPktCloseGifPkt(pkt);
    psDisplayListCloseRefPkt(dl);
    dl->reg[reg][0] = val;
    dl->flag[reg] = 0;
}

/* User texture heap back as the surface VRAM heap. */
/* 0x001b1f50 */
void psGsRestoreVramHeap(void)
{
    psSurfSetVramHeap(psSurfTexShadow.db.user_tex);
}

/* Clear colour of both frame buffers. */
/* 0x001b1f60 */
void psGsSetClearColor(int r, int g, int b)
{
    NjGsDBuff *db = &psSurfTexShadow.db;

    db->clear0.rgbaq[0] = r;
    db->clear0.rgbaq[1] = g;
    db->clear0.rgbaq[2] = b;
    db->clear1.rgbaq[0] = r;
    db->clear1.rgbaq[1] = g;
    db->clear1.rgbaq[2] = b;
}

/* 0x001b1f88 */
void psGsGetVramLayout(NjGsVram *out)
{
    NjGsState *gs = &psSurfTexShadow;

    *out = gs->db.vram;
}

/* GS coordinates. */
/* 0x001b1fc0 */
static void psGsGetScreenCenter(float *x, float *y)
{
    NjGsState *gs = &psSurfTexShadow;

    *x = (float)gs->db.cx;
    *y = (float)gs->db.cy;
}

/* GS coordinates. */
/* 0x001b1fe8 */
void psGsGetScreenOrigin(float *x, float *y)
{
    NjGsState *gs = &psSurfTexShadow;

    *x = (float)gs->db.org_x;
    *y = (float)gs->db.org_y;
}

/* draw0 on odd frames, draw1 on even. */
/* 0x001b2010 */
static NjGsDrawEnv *psGsGetDrawEnv(void)
{
    NjGsState *gs = &psSurfTexShadow;

    if (gs->db.frame & 1)
        return &gs->db.draw0;
    return &gs->db.draw1;
}

/* From the GS CSR. */
/* 0x001b2038 */
static int psGsGetField(void)
{
    return (int)(*(volatile unsigned long *)0x12001000 >> 13) & 1;
}

/* -1 for any buffer but 0 or 1. */
/* 0x001b2058 */
unsigned long psGsGetFrameReg(int i)
{
    NjGsDBuff *db = &psSurfTexShadow.db;

    switch (i) {
    case 0:
        return db->frame_regs[0];
    case 1:
        return db->frame_regs[1];
    }
    return -1;
}

/*
 * The game's flip (libgraph_swap_dbuff is the library's). Interlaced: records both FRAME values
 * and the next half-pixel offset; else sends the field's SCANMSK packet.
 */
/* 0x001b2088 */
void sceGsSwapDBuff(void)
{
    NjGsDBuff *db = &psSurfTexShadow.db;
    long scan_mask;
    int ret;

    db->field = psGsGetField() ^ 1;
    if (db->interlace) {
        db->frame_regs[1] = (db->frame & 1) ? db->draw1.reg[0] : db->draw0.reg[0];
        db->frame_regs[0] = (db->frame & 1) ? db->draw0.reg[0] : db->draw1.reg[0];
        sceGsSetHalfOffset((db->frame & 1) ? &db->draw1 : &db->draw0, db->cx, db->cy, db->field);
    } else {
        scan_mask = (db->field & 1) ? 2 : 3;
        db->pkt.val = (db->pkt.val & ~3L) | scan_mask;
        iFlushCache(0);
        psDmaSendN(2, &db->pkt, 2);
    }
    iFlushCache(0);
    ret = libgraph_swap_dbuff(db, db->frame);
    if (ret)
        printf("sceGsSwapDBuff: ret = %d\n", ret);
    db->frame++;
}

/* sceGsSwapDBuff after vsync, without recording FRAME values or errors. */
/* steer(cast) */
/* 0x001b2188 */
static void psGsSwapDBuffVSync(void)
{
    NjGsDBuff *db = &psSurfTexShadow.db;
    long scan_mask;

    ((int (*)(int))sceGsSyncV)(0);
    db->field = ((int)(*(volatile unsigned long *)0x12001000 >> 13) & 1) ^ 1;
    if (db->interlace) {
        sceGsSetHalfOffset((db->frame & 1) ? &db->draw1 : &db->draw0, db->cx, db->cy, db->field);
    } else {
        scan_mask = (db->field & 1) ? 2 : 3;
        db->pkt.val = (db->pkt.val & ~3L) | scan_mask;
        FlushCache(0);
        psDmaSendN(2, &db->pkt, 2);
    }
    iFlushCache(0);
    libgraph_swap_dbuff(db, db->frame);
    db->frame++;
}

/* Sets psGsPathSynced. */
/* 0x001b2260 */
void psGsSyncPath(void)
{
    sceGsSyncPath(0, 0);
    psGsPathSynced = 1;
}

/* Records the field. */
/* steer(cast) */
/* 0x001b2288 */
void psGsVSyncField(void)
{
    NjGsDBuff *db = &psSurfTexShadow.db;

    db->field = ((int (*)(int))sceGsSyncV)(0);
}

/* 0x001b22b8 */
static NjGxPkt *psGxPktGetCurrent(void)
{
    return psGxPktCur;
}

/* 0x001b22c0 */
void psGsStub(void)
{
}

/* ALPHA_1 src over dest by src alpha, TEXA 0x80 for 16-bit texels, DIMX; one GIF packet. */
/* 0x001b22c8 */
void psGsInitMainDisplayList(void)
{
    void *dl, *pkt;

    dl = psGsGetMainDisplayList();
    psDisplayListUpdateGsReg(dl, 66, 68);
    psDisplayListUpdateGsReg(dl, 59, 0x8000000000UL);
    psDisplayListUpdateGsReg(dl, 68, 0x2031203131203120UL);
    pkt = psDisplayListOpenRefPkt(dl);
    psVifPktOpenGifPkt(pkt);
    psDisplayListFlushGsRegs(dl, pkt);
    psVifPktCloseGifPkt(pkt);
    psDisplayListCloseRefPkt(dl);
}

/*
 * Double buffer cfg->w x cfg->h, z greater, 24-bit z; SCANMSK packet, default render state;
 * system and user texture heaps after the z buffer.
 */
/* steer(cast) */
/* 0x001b2370 */
NjGsState *psGsInit(NjGsInit *cfg)
{
    NjGsState *gs = &psSurfTexShadow;
    NjGsDBuff *db = &gs->db;
    NjGsVram *vram;
    NjGsRegPkt *reg_pkt;

    psDmaInit();
    sceGsResetPath();
    psGsPktBuf = psPktBufInit(psGsPktMem, 16384);
    /* header plus 256 tags */
#ifdef VERSION_US
    psGsMainDisplayList = psDisplayListInit(psGsMainDlMem, 5872, psGsPktBuf);
#else
    psGsMainDisplayList = psDisplayListInit(psGsMainDlMem, 5856, psGsPktBuf);
#endif
    memset(gs, 0, sizeof(NjGsState));
    sceGsResetGraph(0, cfg->interlace, cfg->mode, cfg->field_mode);
    sceGsSetDefDBuff(db, 0, cfg->w, cfg->h, 3, 49, 1);
    FlushCache(0);
    sceGsPutDrawEnv(db->tag0);
    sceGsPutDrawEnv(db->tag1);
    db->interlace = cfg->field_mode;
    db->width = cfg->w;
    db->height = cfg->h;
    db->cx = 2048;
    db->cy = 2048;
    db->org_x = db->cx - db->width / 2;
    db->org_y = db->cy - db->height / 2;
    db->frame_regs[1] = (db->frame & 1) ? db->draw1.reg[0] : db->draw0.reg[0];
    db->frame_regs[0] = (db->frame & 1) ? db->draw0.reg[0] : db->draw1.reg[0];
    reg_pkt = &db->pkt;
    reg_pkt->tag.nloop = 1;
    reg_pkt->tag.eop = 1;
    reg_pkt->tag.flg = 0;
    reg_pkt->tag.nreg = 1;
    reg_pkt->reg = 34;
    reg_pkt->tag.regs0 = 14;
    vram = &gs->db.vram;
    vram->fbuf0 = (int)(((GsWord *)&db->draw0.reg[0])->l & 0x1ff) << 11;
    vram->fbuf1 = (int)(((GsWord *)&db->draw1.reg[0])->l & 0x1ff) << 11;
    vram->zbuf = (int)(((GsWord *)&db->draw0.reg[2])->l & 0x1ff) << 11;
    vram->sysTex = db->width * db->height + vram->zbuf;
    vram->userTex = vram->sysTex + 12288;
    psLogPrintf(1, "fbuf0   = %08x\n", vram->fbuf0);
    psLogPrintf(1, "fbuf1   = %08x\n", vram->fbuf1);
    psLogPrintf(1, "zbuf    = %08x\n", vram->zbuf);
    psLogPrintf(1, "sysTex  = %08x\n", vram->sysTex);
    psLogPrintf(1, "userTex = %08x\n", vram->userTex);
    db->unk_274 = 1;
    db->frame = 0;
    psDisplayListOpen(psGsMainDisplayList);
    ((void (*)(NjGsState *))psGsInitMainDisplayList)(gs); /* steer(cast) */
    psDisplayListClose(psGsMainDisplayList);
    psDisplayListFlush(psGsMainDisplayList, 1);
    sceGsSyncPath(0, 0);
    gs->db.sys_tex = ((void *(*)(int, int, int, int, int))psSurfInitVramHeap)((int)psGsSysTexHeap, 224, vram->sysTex, 12288, 5);
    gs->db.user_tex = ((void *(*)(int, int, int, int, int))psSurfInitVramHeap)(cfg->heap_buf, cfg->heap_buf_size, vram->userTex, 0x100000 - vram->userTex, 0);
    psSurfSetVramHeap(gs->db.user_tex);
    return gs;
}
