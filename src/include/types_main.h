/* Library types: newlib FILE, ps GS set-up and double buffers, Ninja packets and display lists. */

#ifndef REZ_TYPES_MAIN_H
#define REZ_TYPES_MAIN_H

#include "types_common.h"

typedef struct __sFILE_a {          /* newlib FILE (used fields) */
    unsigned char *_p;
    int _r;
    int _w;
    short _flags;
    short _file;
    struct {
        unsigned char *_base;
        int _size;
    } _bf;
    int _lbfsize;
    void *_cookie;
    void *_read;
    void *_write;
    void *_seek;
    void *_close;
    struct {
        unsigned char *_base;
        int _size;
    } _ub;
    unsigned char *_up;
    int _ur;
    unsigned char _ubuf[3];
    unsigned char _nbuf[1];
    struct {
        unsigned char *_base;
        int _size;
    } _lb;
    int _blksize;
    int _offset;
    struct _reent_a *_data;
} FILE_a;

/* ulGxClipTriangle output (caller's stack). */
typedef struct NjClipBuf {
    char pad_00[1152];
    int out; /* 0x480: passed to vu0_clip_emit_tex_fog / vu0_clip_gather_st */
    char pad_484[12];
} NjClipBuf;

typedef struct NjGsClear {
    char pad_00[32];
    unsigned char rgbaq[8];         /* 0x20: clear colour (psGsSetClearColor) */
    char pad_28[56];
} NjGsClear;

/*
 * psSurfTexShadow (896 bytes, cleared by psGsInit): GS register shadows and the double-buffer
 * record db passed to sceGsResetGraph / sceGsPutDrawEnv.
 */
typedef struct NjGsDrawEnv {
    unsigned long reg[16];
} NjGsDrawEnv;

/* GIF tag bit fields (psGsInit builds one field by field). */
typedef struct NjGifTagBits {
    unsigned long nloop : 15;
    unsigned long eop : 1;
    unsigned long pad : 30;
    unsigned long pre : 1;
    unsigned long prim : 11;
    unsigned long flg : 2;
    unsigned long nreg : 4;
    unsigned long regs0 : 4;
    unsigned long regs : 60;
} NjGifTagBits;

/* GIF tag + one quadword (psGsInit: register 34; sceGsSwapDBuff sets the low two bits). */
typedef struct NjGsRegPkt {
    NjGifTagBits tag;
    unsigned long reg; /* 0x10: 34 */
    unsigned long val; /* 0x18: bits 0..1 = 2 / 3 by field */
} NjGsRegPkt;

/* Frame and texture buffer addresses (psGsInit prints them). */
typedef struct NjGsVram {
    int fbuf0;
    int fbuf1;
    int zbuf;
    int sysTex;
    int userTex;
} NjGsVram;

typedef struct NjGsDBuff {
    unsigned long disp[2][5]; /* 0x000 */
    unsigned long tag0[2]; /* 0x050 */
    NjGsDrawEnv draw0; /* 0x060 */
    NjGsClear clear0; /* 0x0e0 */
    unsigned long tag1[2]; /* 0x140 */
    NjGsDrawEnv draw1; /* 0x150 */
    NjGsClear clear1; /* 0x1d0 */
    NjGsRegPkt pkt; /* 0x230: one-register GIF packet sent by psDmaSendN (register 34) */
    int interlace; /* 0x250: interlace flag (sceGsSwapDBuff) */
    int width; /* 0x254: width */
    int height; /* 0x258: height */
    int org_x; /* 0x25c: 2048 - width / 2 */
    int org_y; /* 0x260: 2048 - height / 2 */
    int cx; /* 0x264: centre x (2048) */
    int cy; /* 0x268: centre y (2048) */
    int frame; /* 0x26c: frame counter (bit 0 selects the draw environment) */
    int field; /* 0x270: field */
    int unk_274; /* 0x274: set to 1 by psGsInit; never read */
    char pad_278[8];
    unsigned long frame_regs[2]; /* 0x280: frame registers of the two buffers (psGsGetFrameReg) */
    NjGsVram vram; /* 0x290 */
    char pad_2a4[52];
    void *user_tex; /* 0x2d8: user texture area (psSurfInitVramHeap, made current by psSurfSetVramHeap) */
    void *sys_tex; /* 0x2dc: system texture area (psSurfInitVramHeap) */
} NjGsDBuff;

/* psGsInit screen set-up. */
typedef struct NjGsInit {
    int interlace; /* 0x00: interlace (to sceGsResetGraph) */
    int mode; /* 0x04: video mode */
    int field_mode; /* 0x08: field / frame mode (also stored in db.interlace) */
    int w; /* 0x0c */
    int h; /* 0x10 */
    int heap_buf; /* 0x14: passed to psSurfInitVramHeap */
    int heap_buf_size; /* 0x18: passed to psSurfInitVramHeap */
} NjGsInit;

typedef struct NjGsState {
    unsigned long tex0; /* 0x00: shadow of GS register 6 (psGsSetTexFunc) */
    char pad_08[8];
    unsigned long tex1; /* 0x10: shadow of GS register 20 (psGsSetTexLod, psGsSetTexFilter) */
    char pad_18[136];
    NjGsDBuff db; /* 0xa0 */
} NjGsState;

/* Texture for psImagePktAddTexture / psImageDlAddTexture. */
typedef struct NjGsTex {
    char pad_00[44];
    int w; /* 0x2c: width of level 0 */
    int h; /* 0x30: height of level 0 */
    int nmip; /* 0x34: last mip level */
    int psm; /* 0x38: pixel format (19 / 20: indexed) */
    int cpsm; /* 0x3c: colour table format */
    int dbp[7]; /* 0x40: GS address of each level */
    int size[7]; /* 0x5c: size of each level in words */
    int cbp; /* 0x78: GS address of the colour table */
} NjGsTex;

/* Packet quadword lanes: 64, 32, 16-bit or float. */
typedef union NjGxQword {
    unsigned long l[2];
    unsigned int w[4];
    unsigned short h[8];
    float f[4];
} NjGxQword;

/* Double-buffered DMA packet builder (52 bytes). */
typedef struct NjGxPkt {
    char *buf[2]; /* 0x00: the two halves of the packet memory */
    unsigned int size; /* 0x08 */
    int idx; /* 0x0c: half in use */
    unsigned int *start; /* 0x10 */
    unsigned int *cur; /* 0x14: write pointer */
    unsigned int count; /* 0x18: words since the DMA tag */
    char pad_1c[4];
    unsigned short *tag; /* 0x20: open DMA tag (quadword count written on close) */
    unsigned short *vif; /* 0x24: open VIF code (quadword count written on close) */
    NjGxQword *gif; /* 0x28: last GIF tag (end of packet bit set on close) */
    int nq; /* 0x2c: quadwords since the VIF code */
    int ch; /* 0x30: DMA channel */
} NjGxPkt;

typedef struct NjGxTag {
    unsigned long lo;
    unsigned long hi;
} NjGxTag;

/* ulGxState counters beyond NjSys (ninja.h). */
typedef struct NjSysCount {
    char pad_00[2340];
    int ntri; /* 0x924: triangles drawn (strip length - 2 per strip) */
    int nstrip; /* 0x928: strips drawn */
} NjSysCount;

/* Filled by nj_compile_vlist_info from the first vertex chunk header. */
typedef struct NjVlistInfo {
    int *data; /* 0x00: first vertex */
    int type; /* 0x04: chunk type (34, 35, 41, 42) */
    int stride; /* 0x08: words per vertex (3, 4, 6, 7) */
    int *pos; /* 0x0c: position */
    int *nrm; /* 0x10: normal (41, 42) */
    int *col; /* 0x14: colour (35, 42) */
} NjVlistInfo;

/* VU1 strip header quadword (nj_cnk_vu1_strip_lit and siblings). */
typedef struct NjVuHdr {
    float mode; /* -1.0 for a strip, 1.0 for a clipped fan */
    int nloop; /* vertex count | 0x8000 */
    float sign; /* winding */
    int prim; /* GS prim bits | 4 (strip) or | 5 (fan) */
} NjVuHdr;

typedef struct VifBuf {
    char pad_00[4];
    unsigned int *buf; /* 4 */
} VifBuf;

/* PsDispList as the GS register writer sees it: last value and a flag per GS register. */
typedef struct NjDisplayList {
#ifdef VERSION_US
    char pad_00[0x50];              /* 12 more bytes, as in PsDispList */
#else
    char pad_00[0x44];
#endif
    unsigned char flag[0x6c];       /* 0x44: cleared by psGsWriteReg when the register is written */
    unsigned long reg[128][2]; /* 0xb0: register values (psGsWriteReg) */
} NjDisplayList;

/* DMA tag (psGsAddCallTag writes the 31-bit address). */
typedef struct NjDmaTag {
    unsigned long ctrl : 32;  /* quadword count, id, ... */
    unsigned long addr : 31;    /* bits 32..62 */
    unsigned long spr : 1;
} NjDmaTag;

typedef struct GxDesc {
    int mode; /* 0x00 */
    int tv_mode;
    int field_mode;
    int ntex;
    int size1;
    int size2;
    void *gxWork; /* 0x18 */
    int gxWorkSize; /* 0x1c */
} GxDesc;

#endif /* REZ_TYPES_MAIN_H */
