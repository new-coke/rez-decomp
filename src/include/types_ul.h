/* ul library types: GX globals, projection, frustum, texture pool, packets, clipper, vheap. */

#ifndef REZ_TYPES_UL_H
#define REZ_TYPES_UL_H

#include "types_main.h"
#include "types_ps.h"

typedef struct McDirStat {
    char pad_00[20];
    unsigned short status;
} McDirStat;

typedef struct McDirName {
    char pad_00[32];
    char name[32];
} McDirName;

typedef struct { char b[96]; } McImgBytes;

typedef struct {
    int tag;
    int size;
    int unk_08; /* returned in out[0] by ulsnd_chunk_spud */
    int unk_0c; /* returned in out[1] by ulsnd_chunk_spud and ulsnd_chunk_iopd */
} UlChunk;

typedef struct Rel { char pad_00[28]; int off; } Rel;

typedef struct PicHdr {
    char pad_00[12];
    int count;              /* 12 */
    int w;                  /* 16 */
    int h;                  /* 20 */
    int size;               /* 24 */
} PicHdr;

typedef struct UlTexBlk { char pad_00[12]; int id; } UlTexBlk;

typedef struct Frustum {
    float dist;
    float xscale; /* 4 */
    float yscale; /* 8 */
} Frustum;

/* VIF packet word: integer here, float elsewhere. */
typedef union GxWord {
    unsigned int u;
    float f;
} GxWord;

typedef struct GxPkt {
    char pad_00[4];
    int cur;                /* 4 */
    int cnt;                /* 8 */
} GxPkt;

typedef union GxW {
    int i;
    float f;
} GxW;

/* Texture pool entry (176 bytes). */
typedef struct UlTex {
    char pad_00[32];
    int blk;             /* 32 */
    int base;                /* 36 GS words */
    int total;                /* 40 size, GS words */
    int w;                  /* 44 */
    int h;                  /* 48 */
    int mxl;                /* 52 */
    int psm;                /* 56 */
    char pad_3c[68];
    char name[32];          /* 128 */
    int flags;              /* 160, bit 0 = in use */
    char pad_a4[4];
    int id;               /* 168 */
    char pad_ac[4];
} UlTex;

/* ulTextureGetInfo result (8 words). */
typedef struct UlTexInfo {
    char pad_00[4];
    int total;              /* 4 sum of the entry sizes * 4 */
    char pad_08[4];
    int used;               /* 12 */
    int cap;                /* 16 */
    UlTex *last;            /* 20 entry with the highest base */
    int end;                /* 24 base + size of that entry, else 0 */
    char pad_1c[4];
} UlTexInfo;

/* ulTextureTable (ulTextureInitTable). */
typedef struct UlTexPool {
    UlTex *first;           /* 0 */
    void **list;            /* 4 */
    void *base;             /* 8 */
    int count;              /* 12 */
    int used;               /* 16 */
    char pad_14[8];
    UlTex **table;          /* 28 */
} UlTexPool;

/*
 * Projection block (68 bytes): ulGxState.cam, also symbol ulGxProj. ulGxInitProjection fills it
 * with 640, 224, 2048, 2048, 1, -1, 500, 2.
 */
typedef struct UlGxCam {
    float width;            /* 0 (640.0; ulGxSetScreenSize) */
    float height;           /* 4 (224.0) */
    float cx;               /* 8 GS coordinates (2048.0; ulGxSetScreenCenter) */
    float cy;               /* 12 (2048.0) */
    float xscale;           /* 16 (1.0; ulGxSetAspect) */
    float yscale;           /* 20 */
    float fov;              /* 24 (ulGxSetFov) */
    float znear;              /* 28 |near| (ulGxSetDepthRange) */
    float zfar;               /* 32 |far| */
    float dist;             /* 36 (500.0) */
    float unk_28; /* 40: 2.0 in ulGxInitProjection */
    float rnear;            /* 44 -1 / znear */
    float rfar;             /* 48 -1 / zfar */
    float scale_x;            /* 52 (x, y, 2^20, 0; ulGxSetScreenScale) */
    float scale_y;            /* 56 */
    float pix_sx;           /* 60 (ulGxSetPixelScale) */
    float pix_sy;           /* 64 */
} UlGxCam;

/* Texture record, GX view: GS TEX0/TEX1/MIPTBP as 64-bit words. */
typedef struct UlGxTexRec {
    unsigned long tex0;       /* 0 */
    unsigned long tex1;       /* 8 */
    unsigned long miptbp1;      /* 16 */
    unsigned long miptbp2;      /* 24 */
    char pad_20[20];
    int nextra;             /* 52 extra register words (miptbp1/miptbp2) */
} UlGxTexRec;

/* Entry of a texture list (12 bytes). */
typedef struct UlGxTexEnt {
    int a;                  /* 0 */
    int b;                  /* 4 */
    UlGxTexRec *rec;        /* 8 */
} UlGxTexEnt;

typedef struct UlGxTexList {
    UlGxTexEnt *ent;        /* 0 */
    int count;              /* 4 */
} UlGxTexList;

/* UlGxBig.sub target (one field known). */
typedef struct UlGxSub {
    char pad_00[20];
    int unk_14; /* 20: fifth argument of ulGxEmitStrip / ulGxEmitTexStrip */
} UlGxSub;

/* GX draw argument: 32-bit vertex list and 16-bit display list, either NULL. */
typedef struct UlGxDrawable {
    unsigned int *vlist;    /* 0 */
    short *dlist;           /* 4 */
    char pad_08[12];
    float radius;              /* 20 */
} UlGxDrawable;

/* ulGxState's 44-byte frustum block, also symbol ulGxFrustumState (ulGxInitFrustum defaults). */
typedef struct UlGxFrustum {
    float dist;             /* 0 (500) */
    float xscale;           /* 4 (ulGxSetAspect) */
    float yscale;           /* 8 */
    float znear;              /* 12 |near| (ulGxSetDepthRange) */
    float zfar;               /* 16 |far| */
    float cx;               /* 20 (2048) */
    float cy;               /* 24 (2048) */
    float unk_1c; /* 28: 0 in ulGxInitFrustum */
    float depth_max;              /* 32 (2^20) */
    float ext_x;              /* 36 (2040; frustum extent argument of ulGxMakeFrustumMatrix) */
    float ext_y;              /* 40 (2040) */
} UlGxFrustum;

/* psDisplayListGetVifPkt packet buffer. */
typedef struct UlGxBuf {
    int start;
    void *cur;               /* 4 */
    int nwords;                 /* 8 */
    char pad_0c[12];
    unsigned long *gtag;     /* 24 */
    int nqw;                /* 28 */
} UlGxBuf;

/*
 * ulGxState: 14 matrices, then VU1 vectors. ulGxSetMatrix sets 0-2 (proj), 11 (frustum); rebuilds
 * 3 = m1*m0, 4 = m2*m1, 5 = m2*m1*m0, 6 = [1760]*rot(m0), 12 = m3*m1*m0; uploads to VU.
 */
/* BEGIN UlGxBig */
typedef struct UlGxBig {
    char pad_00[896];
    float xmin;               /* 896 clip/guard-band min x (16; VU1 quadword 0x38) */
    float ymin;               /* 900 (16) */
    float dnear;              /* 904 2^20 / |near|: depth at near plane */
    float znear;              /* 908 |near| (ulGxSetDepthRange) */
    float xmax;               /* 912 (4080; VU1 quadword 0x39) */
    float ymax;               /* 916 (4080) */
    float dfar;               /* 920 2^20 / |far| */
    float zfar;               /* 924 |far| */
    float scr_x0;             /* 928 cx - width / 2 (VU1 quadword 0x3a) */
    float scr_y0;             /* 932 */
    float neg_znear;          /* 936 -|near| */
    float inv_znear;          /* 940 1 / |near| */
    float scr_x1;             /* 944 */
    float scr_y1;             /* 948 */
    char pad_3b8[88];
    float col_max_r;              /* 1040  */
    float col_max_g;              /* 1044  */
    float col_max_b;              /* 1048  */
    float col_max_a;              /* 1052  */
    float col_one_r;              /* 1056  */
    float col_one_g;              /* 1060  */
    float col_one_b;              /* 1064  */
    float col_one_a;              /* 1068  */
    unsigned long tag_tri;    /* 1072 GIF tag, untextured triangles */
    unsigned long tag_tri_regs;/* 1080 its registers */
    unsigned long tag_tex;    /* 1088 GIF tag, textured triangles */
    unsigned long tag_tex_regs;/* 1096 its registers */
    unsigned long tag2;      /* 1104  */
    unsigned long tag2_regs;      /* 1112  */
    unsigned long tag3;      /* 1120  */
    unsigned long tag3_regs;      /* 1128  */
    float scale_x;            /* 1136 (x, y, 2^20, 0; VU1 quadword 0x47; ulGxSetScreenScale) */
    float scale_y;            /* 1140 */
    float scale_z;            /* 1144 */
    float scale_w;            /* 1148 */
    float offset_x;           /* 1152 VU1 quadword 0x48 */
    float offset_y;           /* 1156 */
    float offset_z;           /* 1160 */
    float offset_w;           /* 1164 */
    float nscale_x;           /* 1168 clip box -> -1..1 (ulGxCalcScaleOffset; VU1 quadword 0x49) */
    float nscale_y;           /* 1172 */
    float nscale_z;           /* 1176 */
    char pad_49c[4];
    float noffset_x;          /* 1184 VU1 quadword 0x4a */
    float noffset_y;          /* 1188 */
    float noffset_z;          /* 1192 */
    char pad_4ac[4];
    unsigned long prim_tag;   /* 1200 GIF tag of ulGxBegin* lists (PRIM from bit 47) */
    unsigned long prim_regs;  /* 1208 registers (1042) */
    char pad_4c0[16];
    float k_255;              /* 1232  */
    float k_128;              /* 1236  */
    float k_inv256;              /* 1240  */
    float k_inv65536;              /* 1244  */
    float col_scale;              /* 1248  */
    float col_scale_g;              /* 1252  */
    float col_scale_b;              /* 1256  */
    float alpha_scale;              /* 1260  */
    float map_scale_x;              /* 1264  */
    float map_scale_y;              /* 1268  */
    float map_scale_z;              /* 1272  */
    float map_scale_w;              /* 1276  */
    float map_ofs_x;              /* 1280  */
    float map_ofs_y;              /* 1284  */
    float map_ofs_z;              /* 1288  */
    float map_ofs_w;              /* 1292  */
    char pad_510[80];
    float fog_scale;          /* 1376 fog = depth*scale+offset, 0..255 (ulGxSetFogRange) */
    float fog_offset;         /* 1380 */
    float fog_max;            /* 1384 255 */
    float fog_offset2;        /* 1388 fog_offset copy */
    char pad_570[128];
    int unk_5f0; /* 1520: 64 in ulGxResetGlobals */
    float unk_5f4; /* 1524: 1/65536 in ulGxResetGlobals */
    float uvscale;            /* 1528 UV scale of the textured runs (1/256 or 1/1024) */
    char pad_5fc[4];
    UlGxSub * sub;            /* 1536  */
    int drawflags;            /* 1540 ulGxDrawFlagWord (ulGxGetDrawFlags); bit 0: shape format */
    UlGxCam cam;              /* 1544 also symbol ulGxProj */
    int vbuf;                 /* 1612 (ulGxSetVertexBuffer) */
    int vbuf_size;            /* 1616 */
    char pad_654[4];
    void * slot[32];          /* 1624 object pointers */
    int nslot;                /* 1752 used slots */
    char pad_6dc[396];
    unsigned long alpha;      /* 2152 entry of ulGxAlphaTable */
    unsigned long tex0;      /* 2160  */
    unsigned long tex1;       /* 2168 K field set by ulGxSetTextureLod */
    char pad_880[8];
    unsigned long prim;       /* 2184 FGE 0x20, ABE 0x40, IIP 0x08 */
    char pad_890[4];
    UlGxTexList * texlist;    /* 2196 */
    int texidx;                /* 2200 */
    char pad_89c[4];
    float col0;               /* 2208 vertex colour, 4 floats */
    char pad_8a4[44];
    UlGxFrustum frustum;      /* 2256 44 bytes, also symbol ulGxFrustumState */
    int prim3d;                /* 2300  */
    int prim2d;               /* 2304 2D ulGxBegin* PRIM bits (IIP 8; ulGxEnableFog toggles FGE) */
    int tex_mode;             /* 2308 (ulGxDlSetTexture) */
    int alpha_mode;           /* 2312 (ulGxDlSetAlphaMode) */
    int render_flags;         /* 2316 (ulGxSetRenderFlags) */
    char pad_910[8];
    int unk_918; /* 2328: cleared by ulGxClearCounters; never read */
    int unk_91c; /* 2332: cleared by ulGxClearCounters; never read */
    int nvtx;                 /* 2336 statistics, cleared by ulGxClearCounters */
    int ntri;                 /* 2340 */
    int nstrip;                /* 2344  */
    int unk_92c; /* 2348: cleared by ulGxClearCounters; never read */
    int vlist_ticks;          /* 2352 timer ticks (ulGxDrawShapeClip/1) */
    int dlist_ticks;          /* 2356 timer ticks */
    int unk_938; /* 2360: cleared by ulGxClearCounters; never read */
} UlGxBig;
/* END UlGxBig */

/* Clip-space vertex (64 bytes); last word = clipper outcode. */
typedef struct UlClipVtx {
    float f[15];            /* 0..59 */
    int flags;              /* 60 */
} UlClipVtx;

/* Data-file section header: block count, then the sub-block chain ulBlkNext walks. */
typedef struct UlSecHdr {
    char pad_00[10];
    unsigned short count;   /* 10 */
    char pad_0c[4];
} UlSecHdr;

/* Loader table slot (12 bytes); loaded object goes to obj. */
typedef struct UlSlot {
    int a;                  /* 0 */
    int b;                  /* 4 */
    void *obj;              /* 8 */
} UlSlot;

/* ulConsoleCtx debug console, built over a caller buffer by ulConsoleInit; cells follow. */
typedef struct UlConsole {
    int cols;               /* 0 */
    int rows;               /* 4 */
    int bpc;                /* 8 bytes per cell (1) */
    int x;                  /* 12 cursor column */
    int y;                  /* 16 cursor row */
    int cw;                 /* 20 (16) */
    int ch;                 /* 24 (16) */
    char pad_1c[4];
    int col_r;                 /* 32 r, g, b, a bytes (from a packed word) */
    int col_g;                 /* 36 */
    int col_b;                 /* 40 */
    int col_a;                 /* 44 */
    short unk_30; /* 48 (6720): 6720 in ulConsoleInit */
    char pad_32[2];
    int size;               /* 52 cols * rows */
    char *cur;              /* 56 write pointer */
    float scale;            /* 60 */
    float inv;              /* 64 1 / scale */
    char cells[1];          /* 68 */
} UlConsole;

#define PSVH_MAGIC 0x12345678

/* Vheap block header (32 bytes): circular doubly linked list in address order, with sentinel. */
typedef struct PsVBlk {
    int magic;              /* 0 PSVH_MAGIC while used as a header */
    int used;               /* 4 */
    struct PsVBlk *prev;    /* 8 */
    struct PsVBlk *next;    /* 12 */
    int addr;               /* 16 in managed space */
    int size;               /* 20 */
    struct PsVHeap *heap;   /* 24 owner, set on allocation */
    char pad_1c[4];
} PsVBlk;

/* Heap control block (32 bytes), then the sentinel and header slots. */
typedef struct PsVHeap {
    PsVBlk *first;          /* 0 first header slot, after the sentinel */
    PsVBlk *last;           /* 4 */
    PsVBlk *cur;            /* 8 rover: next allocation search start */
    int base;               /* 12 managed space start */
    int size;               /* 16 managed space size */
    int nslots;             /* 20 header slots */
    int align;              /* 24 allocation granularity, power of two */
    char pad_1c[4];
    PsVBlk sentinel;        /* 32 */
} PsVHeap;

/* Heap statistics result. */
typedef struct PsVStat {
    int used_bytes;         /* 0 */
    int free_bytes;         /* 4 */
    int max_free;           /* 8 largest free block */
    int used_count;         /* 12 */
    int free_count;         /* 16 */
    int total_count;        /* 20 */
} PsVStat;

/* Section sub-block: type, payload size, payload (ulBlkNext); texture blocks keyed by key. */
typedef struct UlBlk {
    unsigned int type;
    unsigned int size;          /* 0x04: payload bytes after this header */
    char pad_08[4];
    int key;                    /* 0x0c */
} UlBlk;

/* ulGxProj projection constants. */
typedef struct UlCam {
    float width;               /* 0 */
    float height;               /* 4 */
    float cx;               /* 8 */
    float cy;              /* 12 */
    float xscale;              /* 16 */
    float yscale;              /* 20 */
    float fov;              /* 24 */
    float znear;              /* 28 */
    float zfar;              /* 32 */
    float dist;              /* 36 */
    char pad_28[4];
} UlCam;

/* Clip rectangle and depth range inside ulGxState. */
typedef struct UlGxClip {
    char pad_00[896];
    float xlo;              /* 896 */
    float ylo;              /* 900 */
    char pad_388[8];
    float xhi;              /* 912 */
    float yhi;              /* 916 */
    char pad_398[652];
    float zhi;              /* 1572 */
    float zlo;              /* 1576 */
} UlGxClip;

/* ulGxState as the display-list handlers see it. */
typedef struct UlGxCtx {
    char pad_00[1612];
    char *vtx;              /* 1612 transformed vertices, 64-byte records */
    char pad_650[720];
    int nvtx;               /* 2336 */
} UlGxCtx;

/* VIF packet in progress (psGsGetRefPkt): cursor, quadword count, last GIF tag. */
typedef struct UlPkt {
    char pad_00[4];
    PsGifTag *cur;          /* 4 write pointer */
    char pad_08[12];
    int nqw;                /* 20 quadwords written */
    PsGifTag *gtag;         /* 24 last GIF tag */
} UlPkt;

/* GX context fields of the strip emitters. */
typedef struct UlGxStrip {
    char pad_00[1200];
    PsGifTag tag;           /* 1200 GIF tag template */
    char pad_4c0[968];
    unsigned long prim;     /* 2184 */
} UlGxStrip;

/* Transformed vertex (64 bytes): clip-space position from f[4] on, outcode in code. */
typedef struct UlGxVtx {
    float f[15];
    int code;               /* 60 */
} UlGxVtx;

/* libgraph.h sceGifTag: a GIF tag as bit fields (two 64-bit words). */
typedef struct UlGxGifTag {
    unsigned long NLOOP:15;
    unsigned long EOP:1;
    unsigned long pad16:16;
    unsigned long id:14;
    unsigned long PRE:1;
    unsigned long PRIM:11;
    unsigned long FLG:2;
    unsigned long NREG:4;
    unsigned long REGS0:4;
    unsigned long REGS1:4;
    unsigned long REGS2:4;
    unsigned long REGS3:4;
    unsigned long REGS4:4;
    unsigned long REGS5:4;
    unsigned long REGS6:4;
    unsigned long REGS7:4;
    unsigned long REGS8:4;
    unsigned long REGS9:4;
    unsigned long REGS10:4;
    unsigned long REGS11:4;
    unsigned long REGS12:4;
    unsigned long REGS13:4;
    unsigned long REGS14:4;
    unsigned long REGS15:4;
} UlGxGifTag;

/* Packet writer returned by psDisplayListOpenRefPkt / psDisplayListGetVifPkt. */
typedef struct UlGxVifPkt {
    int start;
    char *cur;                  /* 4 write pointer */
    int words;                  /* 8 */
    char pad_0c[12];
    UlGxGifTag *gtag;           /* 24 open GIF tag */
    int nqw;                    /* 28 quadwords since the open tag */
} UlGxVifPkt;

/* ulGxClipTriangle work area: 16-vertex pool, two pointer lists the passes ping-pong between. */
typedef struct UlGxClipBuf {
    UlClipVtx pool[16];         /* 0 */
    UlClipVtx *in_list[16];           /* 1024 */
    UlClipVtx *out_list[16];           /* 1088 */
    UlClipVtx **out;            /* 1152 result list (closed: out[n] = out[0]) */
    int nout;                      /* 1156 */
    int used;                   /* 1160 pool entries used */
} UlGxClipBuf;

/* 44 bytes */
typedef struct FsSlot {
    char used;
    char dev;                   /* resolved by ulFsMakePath */
    char idx;
    char pad_03[33];
    void *adxf;                 /* 0x24: AdxfObj */
    char pad_28[4];
} FsSlot;

/* steer(alias): ulFsAllocSlot reads used unsigned */
typedef struct FsSlotView {
    unsigned char used;
    char pad_01[43];
} FsSlotView;

typedef struct FsCtx {
    int max;
    int nopen;
    int next;
    int *ent;                   /* entry table: count, then 28-byte entries */
    int dev;                    /* 0 disc, 1 host */
    int bs_dev;                 /* host device whose paths take '\\' */
    char pad_18[1];
    char cwd[256];              /* 0x19 */
    char root[256];
    char path[256];
    unsigned char flags[3];
    char pad_31c[4];
    FsSlot slot[1];             /* 0x320: max of them */
} FsCtx;

#endif /* REZ_TYPES_UL_H */
