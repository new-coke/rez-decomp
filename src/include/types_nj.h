/* Ninja (nj/) and ul graphics types: matrices, library state, view, lights, chunk models. */

#ifndef REZ_TYPES_NJ_H
#define REZ_TYPES_NJ_H

#include "qword.h"
#include "types_main.h"
#include "types_ps.h"

typedef struct NjVec4 {
    float x, y, z, w;
} NjVec4;

/* 16-byte aligned for VU0. */
typedef struct NjMatrix {
    float m[16];
} __attribute__((aligned(16))) NjMatrix;

/* nj_bezier_point curve; npts -> control point count. */
typedef struct NjCurve {
    char pad_00[4];
    int *npts;
} NjCurve;

/* 16-byte aligned: copies as one block. */
typedef struct NjColor {
    float r, g, b, a;
} __attribute__((aligned(16))) NjColor;

/* ulGxState (used members). */
typedef struct NjSys {
    char pad_00[128];
    NjMatrix proj_mtx;        /* 0x80: matrix used by the projection in nj_project_point */
    char pad_c0[256];
    NjMatrix light_col_mtx;       /* 0x1c0: light colour matrix (uploaded to VU1 by nj_lights_upload) */
    char pad_200[448];
    NjColor ambient_col;        /* 0x3c0: ambient light colour */
    char pad_3d0[304];
    NjVec4 screen_ofs;         /* 0x500: x, y offsets (+0.5) uploaded to VU1 by nj_set_screen_offset */
    char pad_510[464];
    NjMatrix light_dir_mtx;       /* 0x6e0: light direction matrix (negated unit vectors as columns) */
    char pad_720[64];
    NjMatrix light_col_copy;       /* 0x760: copy of the light colour matrix */
} NjSys;

/* screen_cfg: written by the view set-up functions (njClipZ .. nj_view_defaults). */
typedef struct NjView {
    float sx;           /* 0x00: xscale * dist */
    float sy;           /* 0x04: -(yscale * dist) */
    float cx;           /* 0x08: screen centre x (320) */
    float cy;           /* 0x0c: screen centre y (240) */
    float inv_sx;           /* 0x10: 1 / sx */
    float inv_sy;           /* 0x14: 1 / sy */
    float dist;           /* 0x18: projection distance */
    float xscale;           /* 0x1c: x scale */
    float yscale;           /* 0x20: y scale */
    float aspect;           /* 0x24: yscale / xscale */
    float width;           /* 0x28: screen width (640) */
    float height;           /* 0x2c: screen height (480) */
} NjView;

/* ulGxProj (ulGxGetPixelScale .. ulGxMakeProjMatrix). */
typedef struct NjProjView {
    float width;
    float height;
    float cx;           /* 0x08: screen centre x */
    float cy;           /* 0x0c: screen centre y */
    float xscale;
    float yscale;
    float fov;
    float znear;
    float zfar;
    float dist;           /* 0x24: projection distance */
} NjProjView;

/* 44-byte screen block: ulGxGetFrustum copies it out of ulGxState, ulGxSetFrustum applies it. */
typedef struct NjScreen {
    float dist;           /* 0x00: projection distance */
    char pad_04[16];
    float ofs_x;           /* 0x14: x offset (2048-centred) */
    float ofs_y;           /* 0x18: y offset (2048-centred) */
    char pad_1c[16];
} NjScreen;

/* nj_view_apply argument. */
typedef struct NjViewDesc {
    char pad_00[8];
    float cx;           /* 0x08: centre x */
    float cy;           /* 0x0c: centre y */
    char pad_10[8];
    float dist;           /* 0x18: projection distance */
} NjViewDesc;

typedef struct NjCamera {
    float px, py, pz;       /* 0x00: position */
    float vx, vy, vz;       /* 0x0c: view vector (or target when mode == 2) */
    short roll;             /* 0x18: 16-bit roll angle */
    short ang;              /* 0x1a: 16-bit field-of-view angle */
    int mode;               /* 0x1c: 2 = v is the target point, otherwise a direction */
} NjCamera;

/* nj_lights slot (64 bytes); nj_lights_clear clears 0..n. */
typedef struct NjLight {
    float px;           /* 0x00: position x (type 1, nj_light_set_point) */
    float py;           /* 0x04: position y */
    float pz;           /* 0x08: position z */
    char pad_0c[4];
    float dx;           /* 0x10: unit direction x (type 2, njCnkSetSimpleMultiLightVectorEx) */
    float dy;           /* 0x14: unit direction y */
    float dz;           /* 0x18: unit direction z */
    char pad_1c[4];
    float r;           /* 0x20: colour r (nj_light_set_ambient / nj_light_set_color) */
    float g;           /* 0x24: colour g */
    float b;           /* 0x28: colour b */
    float a;           /* 0x2c: colour a (1.0) */
    int type;             /* 0x30: type (1 point, 2 directional, 3 ambient: nj_lights_upload uses 2 and 3) */
    int enable;             /* 0x34: enable flag */
    float range_a;           /* 0x38: range (nj_light_set_range) */
    float range_b;           /* 0x3c: range (nj_light_set_range) */
} NjLight;

typedef int NjQword REZ_QWORD;

/* Walked by nj_object_walk. */
typedef struct NjObject {
    unsigned int evalflags; /* 0x00: 1 no translate, 2 no rotate, 4 no scale, 8 no model, 0x10 no
                               children, 0x20 rotation order, 0x100 children only when draw returned 0,
                               0x400 / 0x1000 no rotate, 0x800 save the current matrix first */
    void *model;            /* 0x04 */
    float pos[3];           /* 0x08 */
    int ang[3];             /* 0x14 */
    float scl[3];           /* 0x20 */
    struct NjObject *child;     /* 0x2c */
    struct NjObject *sibling;   /* 0x30 */
} NjObject;

/* Chunk object (56 bytes, copied whole by nj_compile_object_node): NjObject plus one word. */
typedef struct NjCnkObject {
    unsigned int evalflags;     /* 0x00: 0x2000 = skip (nj_compile_object_size, nj_compile_object) */
    NjCnkModel *model;          /* 0x04 */
    float pos[3];               /* 0x08 */
    int ang[3];                 /* 0x14 */
    float scl[3];               /* 0x20 */
    struct NjCnkObject *child;  /* 0x2c */
    struct NjCnkObject *sibling;/* 0x30 */
    char pad_34[4];
} NjCnkObject;

/* Strip routine context (nj_cnk_vu1_strips_env and siblings). */
typedef struct NjCnkCtx {
    char pad_00[1040];
    NjQword vu1_tag;            /* 0x410: uploaded to VU1 by nj_cnk_vu1_strips_lit */
    NjQword vu1_tex_tag;            /* 0x420: uploaded to VU1 by nj_cnk_vu1_strips_env */
    char pad_430[128];
    unsigned long tag[2];       /* 0x4b0: GIF tag for the strips (with prim) */
    char pad_4c0[396];
    char *vbuf;                 /* 0x64c: transformed vertices (64 bytes each) */
    char pad_650[568];
    unsigned long prim;         /* 0x888: GS primitive bits */
    char pad_890[124];
    int strip_flags;                /* 0x90c: passed to nj_cnk_vu1_is_lit by the strip routines */
    int vtx_type;                /* 0x910 */
} NjCnkCtx;

typedef union NjPkt {
    NjQword q;
    int i[4];
    unsigned int u[4];
    float f[4];
} NjPkt;

/* psDisplayListGetVifPkt writer (PsVifWr); write pointer advanced as an integer. */
typedef struct NjVifWr {
    int start;              /* 0x00 */
    int cur;                /* 0x04: write pointer */
    int nwords;             /* 0x08: words written */
} NjVifWr;

/* GIF tag and GS PRIM words, ORed in code order. */
#define NJ_GIFTAG(nloop, eop, pre, prim, flg, nreg) \
    ((unsigned long)(nloop) | (unsigned long)(eop) << 15 | (unsigned long)(pre) << 46 | \
     (unsigned long)(prim) << 47 | (unsigned long)(flg) << 58 | (unsigned long)(nreg) << 60)
#define NJ_PRIM(prim, iip, tme, fge, abe, aa1, fst, ctxt, fix) \
    ((unsigned long)(prim) | (unsigned long)(iip) << 3 | (unsigned long)(tme) << 4 | \
     (unsigned long)(fge) << 5 | (unsigned long)(abe) << 6 | (unsigned long)(aa1) << 7 | \
     (unsigned long)(fst) << 8 | (unsigned long)(ctxt) << 9 | (unsigned long)(fix) << 10)

/* ulGxState as the chunk renderer uses it (same object as NjSys). */
typedef struct NjSysView {
    char pad_00[976];
    NjQword mat_col_q;        /* 0x3d0: material colour uploaded to VU1 (nj_cnk_strip_state) */
    NjQword col2_q;        /* 0x3e0: second colour (ambient / zero) */
    char pad_3f0[16];
    int vu1_param;            /* 0x400: VU1 parameter quadword (4 words); low 3 bits: lighting mode */
    int strip_prim;            /* 0x404: GS PRIM of the current chunk strip */
    char pad_408[4];
    int unk_40c;            /* 0x40c: 0x8000 unless material flag 0x1000 */
    int vu1_tag[4];         /* 0x410: VU1 GIF tag, untextured strips */
    int vu1_tex_tag[4];         /* 0x420: VU1 GIF tag, textured strips */
    unsigned long gouraud_tag[2]; /* 0x430: GIF tag template, Gouraud chunk strips */
    unsigned long tex_tag[2]; /* 0x440: GIF tag template, textured chunk strips */
    char pad_450[32];
    float scr_scale_x;          /* 0x470: screen x scale */
    float scr_scale_y;          /* 0x474: screen y scale */
    float depth_scale;          /* 0x478: depth scale */
    char pad_47c[4];
    float scr_org_x;          /* 0x480: screen origin x */
    float scr_org_y;          /* 0x484: screen origin y */
    char pad_488[40];
    unsigned long batch_tag[2]; /* 0x4b0: GIF tag of the current batch */
    char pad_4c0[80];
    float scale_x;          /* 0x510: x scale / 640 (nj_set_screen_scale) */
    float scale_y;          /* 0x514: y scale / 240 */
    float inv_depth;          /* 0x518: -1 / depth of the model centre */
    float inv_depth_fx;          /* 0x51c: inv_depth * 2^20 */
    float unk_520;          /* 0x520: -1728 */
    float unk_524;          /* 0x524: -1928 */
    char pad_528[4];
    float depth;          /* 0x52c: -depth of the model centre */
    char pad_530[200];
    float uv_scale;          /* 0x5f8: texture coordinate scale of the current chunk strip */
    char pad_5fc[80];
    char *vbuf;          /* 0x64c: vertex buffer (64-byte entries) */
    char pad_650[8];
    short *cache[32];     /* 0x658: chunk cache list (CNK cache / draw) */
    int ncache;            /* 0x6d8: cached chunk count */
    char pad_6dc[396];
    unsigned long alpha;  /* 0x868: GS ALPHA value of the current material (from ulGxAlphaTable) */
    char pad_870[24];
    unsigned long prim;  /* 0x888: GS PRIM word of the current chunk material */
    char pad_890[16];
    float mat_col[12];      /* 0x8a0: material colours (diffuse, ambient, specular; r g b a) */
    char pad_8d0[48];
    int prim_bits;            /* 0x900: GS PRIM bits added to every primitive (ulGxPrimBits) */
    char pad_904[4];
    int mat_flags;            /* 0x908: current material flags */
    int strip_flags;            /* 0x90c: current strip flags */
    int vtx_type;            /* 0x910: current vertex chunk type */
    int vtx_attr;            /* 0x914: vertex attribute bits (1 normals, 2 colours) */
    char pad_918[8];
    int nvtx;            /* 0x920: vertices processed */
    int ntri;            /* 0x924: triangles sent */
    int nstrip;
    char pad_92c[4];
    int vlist_ticks;
    int vu1_ticks;            /* 0x934: time spent in the VU1 chunk drawer (psTimerDiff) */
} NjSysView;

#define NJ_SYS_VIEW ((NjSysView *)&ulGxState)

typedef struct NjVec2 {
    float x, y;
} NjVec2;

/* Screen vertex with a packed colour (16 bytes, nj_screen_strip_draw). */
typedef struct NjColVtx {
    float x, y, z;
    unsigned char b, g, r, a;
} NjColVtx;

/* Textured vertex (24 bytes, nj_tex_strip_vertices): position, UV, packed colour. */
typedef struct NjVtxUV {
    float x, y, z;
    float u, v;
    unsigned int col;
} NjVtxUV;

/* njDrawTexture vertex (24 bytes): position, q-scaled UV, packed BGRA. */
typedef struct NjTexVtx {
    float x, y, z;          /* z is also Q (1/w) */
    float u, v;
    unsigned char b, g, r, a;
} NjTexVtx;

/* ulGxClipTriangle output: polygon vertices, then the out pointer. */
typedef struct NjClipOut {
    char pad_00[1152];
    void *out;
} __attribute__((aligned(16))) NjClipOut;

/* Chunk strip index entry (6 bytes: vertex index and two more shorts). */
typedef struct NjCnkIdx {
    char pad_00[6];
} NjCnkIdx;

/* CPU-lit path vertex chunk (nj_cnk_vu1_vtxsrc, set by nj_cnk_vu1_vlist_raw). */
typedef struct NjVtxSrc {
    int *pos;               /* 0x00: first vertex (0 when the chunk type is unknown) */
    int type;               /* 0x04: chunk type */
    int stride;             /* 0x08: bytes per vertex */
    int *vtx;                /* 0x0c: positions */
    int *nrm;                /* 0x10: normals */
    int *col;                /* 0x14: colours */
} NjVtxSrc;

/* nj_draw_points list: pts 12 bytes each, col ARGB words. */
typedef struct NjPointList {
    NjVec3 *pts;
    unsigned int *col;
} NjPointList;

#define NJ_SYS_COUNT ((NjSysCount *)&ulGxState)

#endif /* REZ_TYPES_NJ_H */
