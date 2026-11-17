/* ps layer types: DMA packets, display lists, VIF/GIF writers, heaps, VRAM heap, surfaces. */

#ifndef REZ_TYPES_PS_H
#define REZ_TYPES_PS_H

#include "types_main.h"
#include "qword.h"

typedef struct HeapStack {
    int idx;
    int tab[8];
} HeapStack;


/* DMA packet buffer (ps_pkt.c) */
typedef struct PsPktBuf {
    unsigned int orig;      /* 0 */
    unsigned int size;      /* 4 */
    int base;               /* 8 */
    unsigned int cap;       /* 12 */
    int cur;                /* 16 */
    int unk_14; /* 20: cleared by psPktBufReset */
    int unk_18; /* 24: cleared by psPktBufReset */
} PsPktBuf;

/* VIF/GIF packet writer embedded in a display list (PsDispList.vif) */
typedef struct PsVifPkt {
    int start;              /* 0 */
    int cur;                /* 4 */
    int nwords;             /* 8 */
    char pad_0c[4];
    int last;               /* 16 */
    int nqw;                /* 20 */
    int gtag;               /* 24 */
    int nreg;               /* 28 */
} PsVifPkt;

typedef struct PsGifTag {
    unsigned long nloop:15;
    unsigned long eop:1;
    unsigned long pad:30;
    unsigned long pre:1;
    unsigned long prim:11;
    unsigned long flg:2;
    unsigned long nreg:4;
    unsigned long regs;
} PsGifTag;

/* EE DMA tag (QWC, PCE, ID, IRQ, ADDR, SPR) followed by two data words */
typedef struct PsDmaTag {
    unsigned long qwc:16;
    unsigned long pad:10;
    unsigned long pce:2;
    unsigned long id:3;
    unsigned long irq:1;
    unsigned long addr:31;
    unsigned long spr:1;
    unsigned int data0;
    unsigned int data1;
} PsDmaTag;

typedef struct PsGsReg {
    unsigned long val;
    char pad_08[8];
} PsGsReg;

typedef struct PsDispList {
    PsDmaTag *tags;         /* 0 */
    int tagsize;               /* 4 */
    PsPktBuf *pkt;          /* 8 */
    PsDmaTag *head;         /* 12 */
    PsDmaTag *end_tag;      /* 16 */
    PsDmaTag *cur_tag;      /* 20 */
    unsigned int ntags;     /* 24 */
    unsigned int maxtags;   /* 28 */
    int flags;              /* 32 */
#ifdef VERSION_US
    int us_unk24[3];        /* 36: three more words; the members below move by 12 */
#endif
    PsVifPkt vif;           /* 36 */
    unsigned char dirty[108]; /* 68: [0] = count, then GS register indices */
    PsGsReg reg[99];        /* 176 */
} PsDispList;

typedef struct UlTexture UlTexture;

/* Texture table ulTextureTable (ulTextureInitTable; the same table as UlTexPool of types_ul.h) */
typedef struct UlTexMgr {
    UlTexture *tex;             /* 0 */
    int list;                   /* 4 */
    int base;                   /* 8 */
    int max;                    /* 12 */
    int next;                   /* 16 */
    char pad_14[8];
    UlTexture **freelist;       /* 28 */
} UlTexMgr;

/*
 * Heap: 64-byte header, then 64-byte-headed blocks in a circular doubly linked list; each block
 * starts with magic 0xa5a5 and its in-use flag.
 */
typedef struct HeapBlk {
    unsigned short magic;       /* 0 */
    short used;                 /* 2 */
    unsigned int size;          /* 4: block size including the 64-byte header */
    struct HeapBlk *prev;       /* 8 */
    struct HeapBlk *next;       /* 12 */
    struct Heap *heap;          /* 16 */
    const char *name;           /* 20 */
    unsigned int line;          /* 24 */
    char pad_1c[36];
} HeapBlk;

typedef struct Heap {
    struct Heap *self;          /* 0 */
    unsigned int size;          /* 4 */
    HeapBlk *root;              /* 8 */
    int rootsize;                   /* 12 */
    HeapBlk *first;             /* 16 */
    unsigned int avail;         /* 20 */
    HeapBlk *rover;             /* 24 */
    unsigned int align;         /* 28 */
    char pad_20[32];
} Heap;

/* ps_vheap.c control block; only base and size used. */
typedef struct PsVramHeap {
    char pad_00[12];
    int base;               /* 12 */
    int size;               /* 16 */
    char pad_14[12];
} PsVramHeap;

/* psSurfState: VRAM heap in use and its extent. */
typedef struct PsVramState {
    int base;               /* 0 */
    int size;               /* 4 */
    int rsv_size;               /* 8 */
    char pad_0c[12];
    PsVramHeap *heap;       /* 24 */
} PsVramState;

/* PsVifPkt with typed pointers, for the helpers that write through cur and last. */
typedef struct PsVifWr {
    int *start;             /* 0 */
    int *cur;               /* 4 write pointer */
    int nwords;             /* 8 words written */
    int unk_0c; /* 12: cleared by psVifPktInit */
    int *last;              /* 16 last VIF code word written */
    int nqw;                /* 20 quadwords of the open GIF packet */
    PsGifTag *gtag;         /* 24 open GIF tag */
    int nreg;               /* 28 register descriptors / quadwords written after the tag */
} PsVifWr;

/* A VIF code word (DIRECT/UNPACK: 16-bit immediate, 8-bit NUM, 8-bit command). */
typedef struct PsVifCode {
    unsigned imm:16;
    unsigned num:8;
    unsigned cmd:8;
} PsVifCode;

typedef unsigned int PsQword REZ_QWORD;

/* Texture surface (first 124 bytes of the ul_texture.c record): GS TEX0/TEX1/MIPTBP, VRAM. */
typedef struct PsSurf {
    unsigned long tex0;         /* 0 */
    unsigned long tex1;         /* 8 */
    unsigned long miptbp1;      /* 16 */
    unsigned long miptbp2;      /* 24 */
    void *blk;                  /* 32 VRAM heap block, 0 = none */
    int base;                   /* 36 block address, words */
    int total;                  /* 40 words allocated (all levels plus the CLUT) */
    int w;                      /* 44 */
    int h;                      /* 48 */
    int mxl;                    /* 52 highest mipmap level */
    int psm;                    /* 56 pixel storage mode */
    int cpsm;                   /* 60 CLUT pixel storage mode */
    int off[7];                 /* 64 start of each level (relative, then absolute) */
    int size[7];                /* 92 words of each level */
    int end;                    /* 120 words of all levels (then the CLUT address) */
} PsSurf;

/*
 * Texture record (ul_texture.c); 0xb0 bytes. flags: bit 0 in use, bit 1 locked
 * (ulTextureCreateLocked / ulTextureFreeLocked), bit 2 image set (ulTextureReload reloads it).
 */
struct UlTexture {
    PsSurf surf;                /* padded to 0x80 */
    char name[32];
    unsigned int flags;
    int image;                  /* what ulTextureReload uploads */
    int id;
    int slot;
};

typedef struct PadAct {
    int w[4];
} PadAct;

/* 76 bytes */
typedef struct PadInfo {
    char pad_00[1];
    unsigned char id;           /* 0x01 */
    unsigned char cur;
    unsigned char nmodes;
    unsigned char modes[4];
    int nact;
    PadAct act[4];
} PadInfo;

/* 100 bytes */
typedef struct PsPadRec {
    unsigned char data[32];     /* scePadRead */
    PadInfo *info;
    unsigned char state;
    unsigned char id;           /* 4 digital, 7 analog */
    unsigned char flags;        /* 1 pressure, 2 actuators */
    char pad_27[1];
    unsigned int held;
    unsigned int up;            /* ~held */
    unsigned int pressed;
    unsigned int released;
    unsigned int repeat;
    unsigned int repeat_cnt;
    unsigned char stick[4];     /* 128 centre */
    char pad_44[28];
    unsigned char port;         /* 0x60 */
    unsigned char slot;
    char pad_62[2];
} PsPadRec;

#endif /* REZ_TYPES_PS_H */
