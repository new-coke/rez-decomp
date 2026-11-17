/* GS library: reset and video mode, default double-buffer environments and clear packets, vsync. */

#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "qword.h"
#include "sdk_compat.h"
#include "libc.h"
#include "libgraph.h"
#include "kernel_syscalls.h"
#include "libkernl_glue.h"
#include "libkernl_intc.h"

typedef int GsQword REZ_QWORD;

typedef struct {
    char disp[2][40];   /* 0x000 */
    u_long64 draw0[30]; /* 0x050 */
    u_long64 draw1[1];  /* 0x140 */
} GsDBuff;

typedef struct {
    char pad_00[32];
    u_long64 xyoffset;  /* 0x20 */
    char pad_28[8];
    GsScissor scissor;  /* 0x30 */
} GsDrawEnvA;

typedef struct { u_long64 AC:1; u_long64 pad1:63; } GsPrmodecontA;
typedef struct { u_long64 CLAMP:1; u_long64 pad1:63; } GsColclampA;
typedef struct { u_long64 DTHE:1; u_long64 pad1:63; } GsDtheA;

typedef struct GsDrawEnv1A {        /* A+D body: 8 (value, address) pairs */
    u_long64 frame1, frame1addr;
    u_long64 zbuf1, zbuf1addr;
    u_long64 xyoffset1, xyoffset1addr;
    u_long64 scissor1, scissor1addr;
    GsPrmodecontA prmodecont; u_long64 prmodecontaddr;
    GsColclampA colclamp; u_long64 colclampaddr;
    GsDtheA dthe; u_long64 dtheaddr;
    u_long64 test1, test1addr;
} GsDrawEnv1A;

#define GS_SET_FRAME(fbp, fbw, psm, fbmsk) \
    ((u_long64)(fbp) | ((u_long64)(fbw) << 16) | ((u_long64)(psm) << 24) | ((u_long64)(fbmsk) << 32))

#define GS_SET_ZBUF(zbp, psm, zmsk) ((u_long64)(zbp) | ((u_long64)(psm) << 24) | ((u_long64)(zmsk) << 32))
#define GS_SET_XYOFFSET(ofx, ofy) ((u_long64)(ofx) | ((u_long64)(ofy) << 32))

#define GS_SET_SCISSOR(scax0, scax1, scay0, scay1) \
    ((u_long64)(scax0) | ((u_long64)(scax1) << 16) | ((u_long64)(scay0) << 32) | ((u_long64)(scay1) << 48))

#define GS_SET_TEST(ate, atst, aref, afail, date, datm, zte, ztst) \
    ((u_long64)(ate) | ((u_long64)(atst) << 1) | ((u_long64)(aref) << 4) | ((u_long64)(afail) << 12) | \
     ((u_long64)(date) << 14) | ((u_long64)(datm) << 15) | ((u_long64)(zte) << 16) | ((u_long64)(ztst) << 17))

typedef struct GsClearA {           /* A+D body: 6 (value, address) pairs */
    u_long64 testa, testaaddr;
    u_long64 prim, primaddr;
    u_long64 rgbaq, rgbaqaddr;
    u_long64 xyz2a, xyz2aaddr;
    u_long64 xyz2b, xyz2baddr;
    u_long64 testb, testbaddr;
} GsClearA;

#define GS_SET_RGBAQ(r, g, b, a, q) \
    ((u_long64)(r) | ((u_long64)(g) << 8) | ((u_long64)(b) << 16) | ((u_long64)(a) << 24) | ((u_long64)(q) << 32))

#define GS_SET_XYZ(x, y, z) ((u_long64)(x) | ((u_long64)(y) << 16) | ((u_long64)(z) << 32))

extern GsGParam libgraph_gparam;
extern int AddIntcHandler(int cause, int (*fn)(int), int next);

/*
 * Mode 0 resets, masks interrupts, removes the vsync callback, sets the video mode; 1 only
 * flushes; 5 sets the mode without reset. Kept in libgraph_gparam with the GS revision.
 */
/* 0x001dee50 */
void sceGsResetGraph(short mode, short inter, short omode, short ffmd)
{
    GsGParam *gparam;

    switch (mode) {
    case 0:
        gparam = sceGsGetGParam();
        GS_CSR = 0x200;
        gparam->interlace = inter;
        gparam->omode = omode;
        gparam->version = (GS_CSR >> 16) & 0xff;
        GsPutIMR(0xff00);
        gparam->ffmode = ffmd != 0;
        if (gparam->vsync_cb != 0) {
            DisableIntc(2);
            RemoveIntcHandler(2, gparam->vsync_hid);
            gparam->vsync_cb = 0;
            gparam->vsync_hid = 0;
        }
        SetGsCrt(inter & 1, omode & 0xff, ffmd & 1);
        break;
    case 1:
        GS_CSR = 0x100;
        break;
    case 5:
        gparam = sceGsGetGParam();
        gparam->ffmode = ffmd != 0;
        gparam->interlace = inter;
        gparam->omode = omode;
        gparam->version = (GS_CSR >> 16) & 0xff;
        SetGsCrt(inter & 1, omode & 0xff, ffmd & 1);
        break;
    }
}

/* 0x001defe0 */
GsGParam *sceGsGetGParam(void)
{
    return &libgraph_gparam;
}

/* Over the GIF DMA channel; may be in scratchpad. -1 if the channel stays busy. */
/* 0x001df058 */
int sceGsPutDrawEnv(u_long64 *giftag)
{
    unsigned int n;

    n = 0;
    while (*(volatile int *)0x1000a000 & 0x100) {
        if (n++ > 0x1000000) {
            printf("sceGsPutDrawEnv: DMA Ch.2 does not terminate\r\n");
            return -1;
        }
    }
    *(volatile int *)0x1000a020 = (int)(*giftag & 0x7fff) + 1;
    if (((unsigned int)giftag & 0x70000000) == 0x70000000)
        *(volatile int *)0x1000a010 = ((unsigned int)giftag & 0x0fffffff) | 0x80000000;
    else
        *(volatile int *)0x1000a010 = (unsigned int)giftag & 0x0fffffff;
    *(volatile int *)0x1000a000 = 0x101;
    return 0;
}

/*
 * Clear packets when clear is set. Frame mode or no interlace: each buffer shows the other's frame.
 */
/* 0x001df140 */
void sceGsSetDefDBuff(GsDBuffA *db, short psm, short width, short height, short ztest, short zpsm, short clear)
{
    GsGParam *gparam;
    short fbp;

    gparam = sceGsGetGParam();
    sceGsSetDefDispEnv(&db->disp[0], psm, width, height, 0, 0);
    sceGsSetDefDispEnv(&db->disp[1], psm, width, height, 0, 0);
    sceGsSetDefDrawEnv(&db->frame0, psm, width, height, ztest, zpsm);
    sceGsSetDefDrawEnv(&db->frame1, psm, width, height, ztest, zpsm);
    if (clear) {
        sceGsSetDefClear(db->clear0, ztest, 2048 - (width >> 1), 2048 - (height >> 1), width, height, 0, 0, 0, 0, 0);
        sceGsSetDefClear(db->clear1, ztest, 2048 - (width >> 1), 2048 - (height >> 1), width, height, 0, 0, 0, 0, 0);
    }
    *(GsQword *)&db->giftag0 = REZ_QWORD_ZERO(GsQword);
    *(GsQword *)&db->giftag1 = REZ_QWORD_ZERO(GsQword);
    db->giftag0.NLOOP = clear ? 14 : 8;
    db->giftag0.EOP = 1;
    db->giftag0.NREG = 1;
    db->giftag0.REGS0 = 0xe;
    db->giftag1.NLOOP = clear ? 14 : 8;
    db->giftag1.EOP = 1;
    db->giftag1.NREG = 1;
    db->giftag1.REGS0 = 0xe;
    fbp = libgraph_fb_pages(psm, width, height);
    if ((*(u_long64 *)gparam & 0x0000ffff0000ffffUL) == 0x100000001UL || gparam->interlace == 0) {
        short half = fbp >> 1;

        db->disp[1].dispfb.FBP = half;
        db->frame0.FBP = fbp >> 1;
    }
}

/* Shows buffer id & 1 and sends its draw environment. */
/* 0x001df3d8 */
int libgraph_swap_dbuff(GsDBuff *db, int id)
{
    id &= 1;
    sceGsPutDispEnv(db->disp[id]);
    if (id) {
        do {
            return sceGsPutDrawEnv(db->draw1);
        } while (0);
    }
    return sceGsPutDrawEnv(db->draw0);
}

/* Interlaced: returns the field displayed (0 or 1), else 1. */
/* 0x001df438 */
int sceGsSyncV(void)
{
    GsGParam *gparam;
    long ret;

    gparam = sceGsGetGParam();
    if (gparam->vsync_cb == 0) {
        crt_wait_vblank();
        if (gparam->interlace == 1)
            return (GS_CSR >> 13) & 1;
    } else {
        ret = (crt_wait_vblank_flag() >> 13) & 1;
        if (gparam->interlace == 1)
            return ret;
    }
    return 1;
}

/* Vblank-start handler, 0 removes; returns the previous one. */
/* 0x001df7e8 */
int sceGsSyncVCallback(int (*fn)(int))
{
    GsGParam *gparam;
    int old;

    gparam = sceGsGetGParam();
    old = gparam->vsync_cb;
    if (fn == 0) {
        DisableIntc(2);
        RemoveIntcHandler(2, gparam->vsync_hid);
        gparam->vsync_hid = 0;
        gparam->vsync_cb = 0;
    } else {
        if (old != 0) {
            DisableIntc(2);
            RemoveIntcHandler(2, gparam->vsync_hid);
        }
        gparam->vsync_cb = (int)fn;
        gparam->vsync_hid = AddIntcHandler(2, fn, -1);
        EnableIntc(2);
    }
    return old;
}

/* Returns the previous mask. */
/* 0x001df888 */
int sceGsPutIMR(int imr)
{
    int old;

    old = GsGetIMR();
    GsPutIMR(imr);
    return old;
}

/* XYOFFSET so (cx, cy) is the scissor centre; optional half-pixel Y. */
/* 0x001df8c8 */
void sceGsSetHalfOffset(GsDrawEnvA *env, short cx, short cy, short half)
{
    u_long64 ofx;
    u_long64 ofy;

    ofy = (cy - (u_long64)(env->scissor.SCAY1 + 1) / 2) << 4;
    ofx = (cx - (u_long64)(env->scissor.SCAX1 + 1) / 2) << 4;
    if (half)
        env->xyoffset = ofx | ((ofy + 8) << 32);
    else
        env->xyoffset = ofx | (ofy << 32);
}

/* 0x001df950 */
int sceGsGetIMR(void)
{
    return GsGetIMR();
}

/* Interrupt-handler sceGsPutIMR. */
/* 0x001df970 */
int isceGsPutIMR(int imr)
{
    int old;

    old = iGsGetIMR();
    iGsPutIMR(imr);
    return old;
}

/*
 * Read circuit 2, display area centred on NTSC (2) or PAL (3) moved by (dx, dy), largest
 * horizontal magnification that fits. Other video modes print an error.
 */
/* 0x001df9b0 */
void sceGsSetDefDispEnv(GsDispEnv *disp, short psm, short width, short height, short dx, short dy)
{
    GsGParam *gparam;

    gparam = sceGsGetGParam();
    disp->pmode = 0x66;
    if (gparam->interlace != 0) {
        if (gparam->ffmode != 0)
            disp->smode2 = 3;
        else
            disp->smode2 = 1;
    } else {
        disp->smode2 = 2;
    }
    disp->dispfb = ((u_long64)(psm & 0xf) << 15) | ((u_long64)(((width + 63) >> 6) & 0x3f) << 9);
    if (gparam->omode == 2) {
        if (gparam->interlace == 1) {
            disp->display = ((u_long64)((width + 2559) / width - 1) << 23) | ((u_long64)((height << (gparam->ffmode ? 1 : 0)) - 1) << 44) |
                (((u_long64)636 + dx * ((width + 2559) / width)) & 0xfff) | ((u_long64)((dy + 50) & 0xfff) << 12) |
                ((u_long64)((width + 2559) / width * width - 1) << 32);
        } else {
            disp->display = ((u_long64)((width + 2559) / width - 1) << 23) | ((u_long64)(height - 1) << 44) |
                (((u_long64)636 + dx * ((width + 2559) / width)) & 0xfff) | ((u_long64)((dy + 25) & 0xfff) << 12) |
                ((u_long64)((width + 2559) / width * width - 1) << 32);
        }
    } else if (gparam->omode == 3) {
        if (gparam->interlace == 1) {
            disp->display = ((u_long64)((width + 2559) / width - 1) << 23) | ((u_long64)((height << (gparam->ffmode ? 1 : 0)) - 1) << 44) |
                (((u_long64)656 + dx * ((width + 2559) / width)) & 0xfff) | ((u_long64)((dy + 72) & 0xfff) << 12) |
                ((u_long64)((width + 2559) / width * width - 1) << 32);
        } else {
            disp->display = ((u_long64)((width + 2559) / width - 1) << 23) | ((u_long64)(height - 1) << 44) |
                (((u_long64)656 + dx * ((width + 2559) / width)) & 0xfff) | ((u_long64)((dy + 36) & 0xfff) << 12) |
                ((u_long64)((width + 2559) / width * width - 1) << 32);
        }
    } else {
        printf("sceGsDefDispEnv:Not support displaymode for %d!!\n", gparam->omode);
    }
    disp->bgcolor = 0;
}

/* Read circuit 2, or circuit 1 on the first GS revision. */
/* 0x001dfc20 */
void sceGsPutDispEnv(GsDispEnv *disp)
{
    GsGParam *gparam;

    gparam = sceGsGetGParam();
    if (gparam->version == 1) {
        GS_REG(0x00) = disp->pmode;
        GS_REG(0x70) = disp->dispfb;
        GS_REG(0x80) = disp->display;
        GS_REG(0xc0) = disp->bgcolor;
    } else {
        GS_REG(0x00) = disp->pmode;
        GS_REG(0x20) = disp->smode2;
        GS_REG(0x90) = disp->dispfb;
        GS_REG(0xa0) = disp->display;
        GS_REG(0xe0) = disp->bgcolor;
    }
}

/*
 * 2048-word pages: one frame in interlaced field mode, else two. 16-bit 64x64 pages, 32-bit 64x32.
 */
/* 0x001dfce0 */
short libgraph_fb_pages(short psm, short width, short height)
{
    GsGParam *gparam;
    int bw;
    int bh;

    gparam = sceGsGetGParam();
    bw = (width + 63) / 64;
    if (psm & 2)
        bh = (height + 63) / 64;
    else
        bh = (height + 31) / 32;
    if ((*(u_long64 *)gparam & 0x0000ffff0000ffffUL) == 1)
        return bw * bh;
    return bw * bh * 2;
}

/*
 * Depth buffer after the frame buffers (writes off when ztest is 0), area centred on 2048, colour
 * clamp, dither for 16-bit. ztest 0 = always pass. Returns 8 (GS settings).
 */
/* 0x001dfda8 */
int sceGsSetDefDrawEnv(GsDrawEnv1A *draw, short psm, short width, short height, short ztest, short zpsm)
{
    draw->frame1addr = 0x4c;
    draw->frame1 = GS_SET_FRAME(0, ((width + 63) >> 6) & 0x3f, psm & 0xf, 0);
    draw->zbuf1addr = 0x4e;
    if (ztest == 0)
        draw->zbuf1 = GS_SET_ZBUF(libgraph_fb_pages(psm, width, height), zpsm & 0xf, 1);
    else
        draw->zbuf1 = GS_SET_ZBUF(libgraph_fb_pages(psm, width, height), zpsm & 0xf, 0);
    draw->xyoffset1addr = 0x18;
    draw->xyoffset1 = GS_SET_XYOFFSET((2048 - (u_long64)(short)(width >> 1)) << 4, (2048 - (u_long64)(short)(height >> 1)) << 4);
    draw->scissor1addr = 0x40;
    draw->scissor1 = GS_SET_SCISSOR(0, width - 1, 0, height - 1);
    draw->prmodecontaddr = 0x1a;
    draw->prmodecont.AC = 1;
    draw->colclampaddr = 0x46;
    draw->colclamp.CLAMP = 1;
    draw->dtheaddr = 0x45;
    if (psm & 2)
        draw->dthe.DTHE = 1;
    else
        draw->dthe.DTHE = 0;
    draw->test1addr = 0x47;
    if (ztest != 0)
        draw->test1 = GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, ztest & 3);
    else
        draw->test1 = GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1);
    SYNC_L();
    return 8;
}

/*
 * One sprite of (r, g, b, a) at z with depth test always, then ztest back. Returns 6 (GS settings).
 */
/* 0x001dff90 */
int sceGsSetDefClear(GsClearA *cp, short ztest, short x, short y, short width, short height,
                 unsigned char r, unsigned char g, unsigned char b, unsigned char a, unsigned int z)
{
    cp->testaaddr = 0x47;
    cp->testa = GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1);
    cp->prim = 6;
    cp->primaddr = 0;
    cp->rgbaqaddr = 1;
    cp->rgbaq = GS_SET_RGBAQ(r, g, b, a, 0x3f800000);
    cp->xyz2a = GS_SET_XYZ(x << 4, y << 4, z);
    cp->xyz2aaddr = 5;
    cp->xyz2baddr = 5;
    cp->xyz2b = GS_SET_XYZ((x + width) << 4, (y + height) << 4, z);
    cp->testbaddr = 0x47;
    if (ztest != 0)
        cp->testb = GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, ztest & 3);
    else
        cp->testb = GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1);
    SYNC_L();
    return 6;
}
