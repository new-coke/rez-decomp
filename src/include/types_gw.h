/* Gindows (debug window system) types; ulsnd and uliop driver records. */

#ifndef REZ_TYPES_GW_H
#define REZ_TYPES_GW_H

#include "types_main.h"

/* Quad primitive screen vertex (16 bytes). */
typedef struct GwVtx {
    float x;
    float y;
    float z;
    unsigned col;
} GwVtx;

/* Choice box drop-down string item. */
typedef struct GwChoiceItem GwChoiceItem;

struct GwChoiceItem {
    char *str; /* 0 */
    char pad_04[4];
    GwChoiceItem *next; /* 8 */
};

typedef struct GwCmd GwCmd;

/* Command definition (12 bytes); the registry keeps 16-byte copies in a name-sorted list. */
typedef struct GwCmdDef {
    char *name; /* 0 */
    char *help; /* 4 */
    int (*fn)(int argc, char **argv); /* 8 */
} GwCmdDef;

struct GwCmd {
    GwCmdDef def; /* 0 */
    GwCmd *next; /* 12 */
};

/* Console line: attribute, text length, text. */
typedef struct GwConLine {
    int attr; /* 0 */
    int len; /* 4 */
    char text[1]; /* 8 */
    char pad_09[3];
} GwConLine;

/* Keyboard edit line. */
typedef struct GwLine {
    char text[127]; /* 0 */
    char pad_7f[1];
    int cursor; /* 128 */
    int status; /* 132 0 none, 1 char typed, 2 Enter */
} GwLine;

/*
 * gw_console_state. Line records live in gw_console_buf, addressed through gw_console_lines
 * (nlines entries, a power of two).
 */
typedef struct GwConsole {
    int hist[64]; /* 0 string pointers */
    int hist_idx; /* 256 */
    int hist_next; /* 260 next slot to write */
    GwLine line; /* 264 136 bytes */
    char pad_190[4];
    int top; /* 404 first visible line */
    int head; /* 408 newest line */
    int tail; /* 412 oldest line */
    int count; /* 416 line records in use */
    int used; /* 420 line buffer bytes used */
    int visible; /* 424 */
    int follow; /* 428 */
    int serial_out; /* 432 VarDef "serial_out" */
    int attr; /* 436 colour */
    void *frame; /* 440 'Console' window */
    void *backlog_win; /* 444 'Console Backlog' */
    void *log_win; /* 448 'Console Log' */
    void *command_win; /* 452 'Console Command' */
    int menu; /* 456 */
    int font; /* 460 */
    int x; /* 464 */
    int y; /* 468 */
    int w; /* 472 */
    int h; /* 476 */
    int bufsize; /* 480 */
    int nlines; /* 484 */
} GwConsole;

/* Font glyph vertex (24 bytes); two per character (top-left, bottom-right). */
typedef struct GwGlyphVtx {
    float x; /* 0 */
    float y; /* 4 */
    float z; /* 8 */
    int u; /* 12 */
    int v; /* 16 */
    unsigned col; /* 20 */
} GwGlyphVtx;

/* Menu item (24 bytes), doubly linked through prev/next. */
typedef struct GwItem GwItem;

typedef void (*GwItemFn)(GwItem *item, int arg, int data);

struct GwItem {
    char *name; /* 0 */
    short type; /* 4 0 item, 1 submenu, 2 separator, 3 value item, 4 static table end */
    short flags; /* 6 bit 15 hidden, low nibble value */
    GwItemFn cb; /* 8 (item, window arg, data) */
    union { int data; GwItem *sub; } val;
    GwItem *prev; /* 16 */
    GwItem *next; /* 20 */
};

/* Static menu table row (16 bytes), ends with type 4. */
typedef struct GwItemDef {
    char *name; /* 0 */
    short type; /* 4 */
    short val; /* 6 */
    GwItemFn cb; /* 8 */
    int data; /* 12 */
} GwItemDef;

/* gw_keyboard_state: down[] keys held, key last key code, ch its character (0 none). */
typedef struct GwKbd {
    unsigned char down[6]; /* 0 */
    char pad_06[6];
    unsigned char unk_0c; /* 12: cleared by gw_keyboard_clear */
    unsigned char key_press; /* 13 */
    unsigned char key; /* 14 last key code */
    unsigned char unk_0f; /* 15: cleared by gw_keyboard_clear */
    unsigned char mods; /* 16 */
    unsigned char unk_11; /* 17: cleared by gw_keyboard_clear */
    unsigned char unk_12; /* 18: cleared by gw_keyboard_clear */
    unsigned char ch; /* 19 */
    char pad_14[8];
    int serial_in; /* 28 VarDef "serial_in" */
    int inject_ch; /* 32 */
    int inject_key; /* 36 */
} GwKbd;

/* String list node (choice box items). */
typedef struct GwListNode GwListNode;

struct GwListNode {
    char *str; /* 0 */
    GwListNode *prev; /* 4 */
    GwListNode *next; /* 8 */
};

/* Scroll bar float range. */
typedef struct GwRange {
    float val; /* 0 */
    float min; /* 4 */
    float max; /* 8 */
    float step; /* 12 line step */
    float page; /* 16 page step */
} GwRange;

/* Texture-draw vertex (24 bytes). */
typedef struct GwTexVtx {
    float x; /* 0 */
    float y; /* 4 */
    float z; /* 8 */
    float u; /* 12 */
    float v; /* 16 */
    unsigned col; /* 20 */
} GwTexVtx;

/*
 * Variable definition (40 bytes, static tables). kind: 0 group, 1 function, 2 int, 3 float,
 * 4 colour, 5 degrees (16.16 fixed), 6 bool, 7 bit field, 9 end. flag bit 0: store fval via ptr.
 */
typedef struct GwVarDef {
    unsigned kind; /* 0 */
    unsigned flags; /* 4 */
    char *name; /* 8 */
    char *help; /* 12 */
    void *ptr; /* 16 variable, or child table for groups */
    void (*cb)(); /* 20 on change */
    float fval; /* 24 initial value */
    char pad_1c[12];
} GwVarDef;

/* Var editor tree node: definition copy plus link; kind 0 group, else variable. */
typedef struct GwVarNode GwVarNode;

struct GwVarNode {
    unsigned kind; /* 0 */
    unsigned flags; /* 4 */
    char *name; /* 8 */
    char *help; /* 12 */
    GwVarNode *child; /* 16 group: first child; variable: its address */
    void (*cb)(); /* 20 on change */
    unsigned mask; /* 24 bit mask (definitions: initial value) */
    int shift; /* 28 lowest set bit of mask */
    char pad_20[8];
    GwVarNode *next; /* 40 */
};

typedef void (*GwCb)(void *);

typedef void (*GwCbF)(void *, float);

/*
 * Window header, bytes 0..163; kinds add words from 164 (GwChoice) or go on as a GwWin.
 * kind: 1 frame, 2 button, 3 scroll bar, 4 popup menu, 6 slider, 7 text field, 9 choice box.
 */
#define GW_HEADER \
    int flags;          /* 0 */ \
    int kind;           /* 4 */ \
    int unk_08;         /* 8 */ \
    GwWin *parent;      /* 12 */ \
    GwWin *child;       /* 16 */ \
    GwWin *prev;        /* 20 */ \
    GwWin *next;        /* 24 */ \
    int x;              /* 28 */ \
    int y;              /* 32 */ \
    int w;              /* 36 */ \
    int h;              /* 40 */ \
    char *title;        /* 44 */ \
    int font;           /* 48 */ \
    int cw;             /* 52 cell width */ \
    int ch;             /* 56 cell height */ \
    unsigned col[4];    /* 60..75 */ \
    int translucent;    /* 76 */ \
    GwFn update;        /* 80 class */ \
    GwFnF draw;         /* 84 class (scale) */ \
    GwFn destroy;       /* 88 class */ \
    GwFn update_handler; /* 92 user */ \
    GwFn draw_handler;  /* 96 user */ \
    GwFn close_handler; /* 100 user */ \
    int user;           /* 104 */ \
    int unk_6c;         /* 108 */ \
    int unk_70;         /* 112 */ \
    int slot[4];        /* 116..131 */ \
    float scale;        /* 132 */ \
    unsigned color;     /* 136 draw colour */ \
    GwWin *owner;       /* 140 */ \
    short drag;         /* 144 */ \
    short unk_92;       /* 146 */ \
    int unk_94[4];      /* 148..163 */

typedef struct GwWin GwWin;
typedef void (*GwFn)(GwWin *);
typedef void (*GwFnF)(GwWin *, float);
typedef void (*GwBtnFn)(GwWin *, int);
typedef struct GwBtn GwBtn;
typedef struct GwFrame GwFrame;
typedef struct GwPop GwPop;
typedef struct GwSb GwSb;
typedef struct GwChoice GwChoice;
typedef struct GwChoiceMenu GwChoiceMenu;
/* Choice box pop-up list (plain window). */
typedef GwWin GwPopup;
struct GwBtn {
    int flags;          /* 0 */
    int kind;           /* 4 */
    char pad_08[4];
    GwWin *parent;      /* 12 */
    char pad_10[12];
    int x;              /* 28 */
    int y;              /* 32 */
    int w;              /* 36 */
    int h;              /* 40 */
    char *title;        /* 44 */
    char pad_30[32];
    GwFn update;        /* 80 class */
    GwFn draw;          /* 84 class (scale) */
    char pad_58[16];
    int pos;            /* 104 */
    int box;            /* 108 title-bar box: 0 close, 1 maximize, 2 minimize */
    char pad_70[4];
    unsigned idx;       /* 116 arrow direction (scroll bar) */
    char pad_78[44];
    GwBtnFn cb;         /* 164 (win, event) */
    int state;          /* 168 */
    int toggle;         /* 172 */
    int icon;           /* 176 (gw_icon_button_create) */
    float icon_scale;   /* 180 */
    int checked;        /* 184 toggle state */
};
struct GwFrame {
    int flags;          /* 0 */
    int kind;           /* 4 */
    char pad_08[4];
    GwFrame *parent;      /* 12 */
    GwFrame *child;       /* 16 */
    GwFrame *prev;        /* 20 */
    GwFrame *next;        /* 24 */
    int x;              /* 28 */
    int y;              /* 32 */
    int w;              /* 36 */
    int h;              /* 40 */
    char *title;        /* 44 */
    int font;           /* 48 */
    int cw;             /* 52 cell width */
    int ch;             /* 56 cell height */
    unsigned col[4];    /* 60..72 */
    int translucent;    /* 76 */
    GwFn update;        /* 80 class */
    GwFnF draw;         /* 84 class (scale) */
    GwFn destroy;       /* 88 class */
    GwFn update_handler; /* 92 user */
    GwFn draw_handler;  /* 96 user */
    GwFn close_handler; /* 100 user */
    int user;           /* 104 */
    char pad_6c[8];
    int slot[4];        /* 116..128 */
    float scale;        /* 132 */
    unsigned color;     /* 136 draw colour */
    GwFrame *owner;       /* 140 */
    short drag;         /* 144 */
    short state;        /* 146 0 normal, 2 minimized, 3 maximized */
    int saved[4];       /* 148..160 saved x, y, w, h */
    int minw;           /* 164 resize minimum */
    int minh;           /* 168 */
    int scroll_x;       /* 172 bottom scroll bar value */
    int scroll_y;       /* 176 right scroll bar value */
    GwFrame *btn[3];      /* 180..188 close, maximize, minimize */
    GwFrame *hscroll;     /* 192 bottom (orientation 0) */
    GwFrame *vscroll;     /* 196 right (orientation 1) */
    char pad_c8[12];
    int has_popup;        /* 212 */
};
struct GwPop {
    int flags;          /* 0 */
    int kind;           /* 4 */
    char pad_08[4];
    GwPop *parent;      /* 12 */
    GwPop *child;       /* 16 open submenu */
    char pad_14[8];
    int x;              /* 28 */
    int y;              /* 32 */
    int w;              /* 36 */
    int h;              /* 40 */
    char pad_2c[36];
    GwFn update;        /* 80 class */
    GwFn draw;          /* 84 class (scale) */
    GwFn destroy;       /* 88 class */
    char pad_5c[48];
    int owner;         /* 140 */
    char pad_90[20];
    int sel;            /* 164 */
    GwItem *list;       /* 168 */
};
/*
 * Scroll bar (kind 3, flags 0x4012c): orient 0..3 (0, 2 sized by sx; 1, 3 by sy), arrow buttons
 * btn0/btn1, thumb window ctl, value range r.
 */
struct GwSb {
    int flags;          /* 0 */
    int kind;           /* 4 */
    char pad_08[4];
    GwWin *parent;      /* 12 */
    char pad_10[12];
    int x;              /* 28 */
    int y;              /* 32 */
    int w;              /* 36 */
    int h;              /* 40 */
    char pad_2c[36];
    GwFn update;        /* 80 class */
    GwFn draw;          /* 84 class (scale) */
    char pad_58[28];
    int which;          /* 116 */
    char pad_78[44];
    unsigned orient;    /* 164 */
    int sflags;         /* 168 bit 1: disabled */
    int pos;            /* 172 */
    GwWin *ctl;         /* 176 thumb */
    GwBtn *btn0;        /* 180 */
    GwBtn *btn1;        /* 184 */
    GwRange r;          /* 188..207 */
    int ox;             /* 208 */
    int oy;             /* 212 */
    int sx;             /* 216 */
    int sy;             /* 220 */
    int user;           /* 224 */
};
/* Choice box (kind 9): one-line selector over a string list. */
struct GwChoice {
    GW_HEADER
    GwListNode *list;   /* 164 */
    int count;          /* 168 */
    int sel;            /* 172 -1 none */
    int cb;             /* 176 (gw_choicebox_set_callback) */
    GwBtn *arrow;      /* 180 drop-down arrow */
    GwPopup *popup;    /* 184 */
};
struct GwChoiceMenu {
    int flags;          /* 0 */
    int kind;           /* 4 */
    char pad_08[4];
    GwWin *parent;      /* 12 */
    char pad_10[12];
    int x;              /* 28 */
    int y;              /* 32 */
    int w;              /* 36 */
    int h;              /* 40 */
    char pad_2c[12];
    int ch;             /* 56 */
    char pad_3c[20];
    GwFn update;        /* 80 class */
    GwFn draw;          /* 84 class (scale) */
    GwFn destroy;       /* 88 class */
    char pad_5c[24];
    GwChoiceMenu *owner;    /* 116 (popup list) */
    int hot;            /* 120 row under pointer (popup list) */
    char pad_7c[40];
    GwChoiceItem *items; /* 164 */
    int count;          /* 168 */
    int sel;            /* 172 */
    void (*cb)(GwChoiceMenu *c, int sel); /* 176 */
    GwWin *popup;       /* 180 */
    int open;           /* 184 */
};

/* Gindows window (216 bytes): header, then the frame-window words. */
struct GwWin {
    GW_HEADER
    int minw;           /* 164: resize minimum */
    int minh;           /* 168 */
    int scroll_x;       /* 172: bottom scroll bar value */
    int scroll_y;       /* 176: right scroll bar value */
    GwWin *btn[3];      /* 180..188: close, maximize, minimize */
    GwWin *hscroll;     /* 192: bottom (orientation 0) */
    GwWin *vscroll;     /* 196: right (orientation 1) */
    char pad_c8[12];
    int has_popup;      /* 212 */
};

/* gw_mouse_state (cleared as 136 bytes). */
typedef struct GwMouse {
    int x;              /* 0 */
    int y;              /* 4 */
    int stat;           /* 8 'stat = %08X' */
    int cursor;         /* 12 'cursor = %2d' */
    int prev_x;         /* 16 */
    int prev_y;         /* 20 */
    int dx;             /* 24 'move speed' */
    int dy;             /* 28 */
    int drag_x;         /* 32 'drag pos' */
    int drag_y;         /* 36 */
    int wheel;          /* 40 'wheel' */
    float fx;           /* 44 */
    float fy;           /* 48 */
    float vx;           /* 52 */
    float vy;           /* 56 */
    int pad_port;       /* 60 pad record id (gw_get_pad_port) */
    int enabled;        /* 64 */
    GwWin *under;       /* 68 window under the pointer ('window = %08x') */
    char pad_48[12];
    int area;           /* 84 'area = %d' */
    int on;             /* 88 'on' */
    int press;          /* 92 'press' */
    int release;        /* 96 'release' */
    int repeat;         /* 100 'repeat' */
    int repeat_cnt;     /* 104 */
    int repeat_delay;   /* 108 */
    int repeat_rate;    /* 112 */
    int dclick;         /* 116 'dclick' */
    int dclick_cnt;     /* 120 */
    int dclick_time;    /* 124 */
    int dclick_win;     /* 128 */
    int idle;           /* 132 */
} GwMouse;

/* Variable editor window state (216 bytes). */
typedef struct GwVarWin {
    int x;              /* 0 initial rectangle */
    int y;              /* 4 */
    int w;              /* 8 */
    int h;              /* 12 */
    void *frame;     /* 16 list window */
    void *view;   /* 20 */
    int sb;          /* 24 */
    void *trans_btn;        /* 28 */
    void *down_btn;        /* 32 */
    void *up_btn;        /* 36 */
    void *parent_btn;        /* 40 */
    GwFn close;         /* 44 on close */
    char path[128];     /* 48 current group path */
    GwVarNode **root;   /* 176 */
    int list;         /* 180 */
    int sel;         /* 184 */
    int count;         /* 188 */
    int top;         /* 192 */
    int cur;         /* 196 */
    int hi;         /* 200 */
    int rows;         /* 204 */
    int lock;         /* 208 */
    int trans;          /* 212 toggled by the 'TRANS' button */
} GwVarWin;

typedef struct UlHandle {
    unsigned char used;
    unsigned char id;
} UlHandle;

/* HCMD ring buffer */
typedef struct UlBuf {
    int tag;   /* "HCLS" */
    int size;  /* bytes used */
    int count; /* commands queued */
} UlBuf;

/* 20-byte record (ulsnd_work.rec[16]); kind -1 free, -2 reserved, -3 closed */
typedef struct UlRec {
    signed char kind;
    signed char status;    /* 1 read by ulsnd_rec_get_state (3 = done) */
    signed char sync;  /* 2 1: the start call loops until the load is done */
    signed char unk_03; /* cleared by ulsnd_rec_alloc / ulsnd_rec_start_mlt */
    signed char state; /* 4 1 MLT load (ulsnd_rec_start_mlt), 2 bank load (ulsnd_rec_start_bank) */
    signed char table; /* 5 bank kind 0..3 (sdBankDownload); table ulsnd_set_channel_on writes */
    signed char index; /* 6 entry in that table */
    char pad_07[1];
    int unk_08; /* third argument of ulsnd_rec_setup */
    int dma_id;
    int data;          /* 16 sound data ('MLT'/bank tags checked on start) */
} UlRec;

/* Shared block channel entry: destination, size limit, in-use word. */
typedef struct UlShChan {
    int addr;
    int size;
    int on;
} UlShChan;

/* Channel entry with IOP and SPU destinations (kinds 1, 2). */
typedef struct UlShChanSpu {
    int iop_addr;
    int iop_size;
    int spu_addr;
    int spu_size;
    int on;
} UlShChanSpu;

/*
 * IOP/EE shared block (ulsnd_work.shared, ulsnd_shared): 'MAP' header built from a sound bank,
 * then the channel tables of kinds 0..4.
 */
typedef struct UlShared {
    char pad_00[36];            /* 'MAP' header, written word by word */
    UlShChan kind0[16];         /* 0x024 */
    char pad_e4[8];
    UlShChanSpu kind1[16];        /* 0x0ec */
    char pad_22c[8];
    UlShChanSpu kind2[16];        /* 0x234 */
    char pad_374[8];
    UlShChan kind3[16];         /* 0x37c */
    char pad_43c[12];
    UlShChan kind4[16];         /* 0x448 */
} UlShared;

/* 64-byte control block (flag in w[15]) */
typedef struct IopBlk {
    int w[16];
} IopBlk;

/* 24-byte record of the IOP wave-data DMA list (ulsnd_work.rec24) */
typedef struct UlWave {
    signed char kind;
    char pad_01[3];
    int size;
    int ofs;
    int src;
    int dst;
    int rec;
} UlWave;

/* 16-byte entry of the SPU DMA list (ulsnd_work.dma) */
typedef struct UlDma {
    int src;
    int dst;
    int size;
    int reserved;
} UlDma;

typedef struct UlSnd {
    int calls;                /* 0x000 */
    unsigned char standby;    /* 0x004 */
    unsigned char dbg;        /* 0x005 */
    char pad_06[2];
    void (*err_cb)();         /* 0x008 */
    int iop_total;            /* 0x00c */
    int total;                /* 0x010 */
    int bufsize;              /* 0x014 */
    int spu_size;               /* 0x018 */
    int unk_1c; /* 0x01c: third argument of ulsnd_configure (0x40000 default, game 0x20000); sent to the IOP */
    char pad_20[4];
    int client[10];           /* 0x024 sce RPC client data (0x28 bytes) */
    char hnd[32];             /* 0x04c handle table, accessed as ulsnd_handles */
    char pad_6c[16];
    IopBlk *iop_info;               /* 0x07c */
    IopBlk *sync_blk;               /* 0x080 */
    IopBlk *mlt_sync;               /* 0x084 */
    IopBlk *wave_sync;               /* 0x088 */
    IopBlk *iop_blk;               /* 0x08c IOP-side block (used through its uncached alias) */
    signed char phase;        /* 0x090 load sequence phase */
    signed char queued;       /* 0x091 records queued to load (start order kept in rec.kind) */
    signed char n_wave_iop;   /* 0x092 IOP wave-list entries */
    signed char n_wave_spu;   /* 0x093 SPU wave-list entries */
    signed char n_dma;        /* 0x094 DMA list entries queued */
    signed char n_dma_sent;   /* 0x095 DMA list entries already sent */
    signed char spu_state;    /* 0x096 SPU send state */
    char pad_97[1];
    UlRec *cur_rec;              /* 0x098 current record being started */
    UlRec rec[16];            /* 0x09c */
    UlWave wave_iop[32];      /* 0x1dc */
    UlWave wave_spu[32];      /* 0x4dc */
    int *flags[22];           /* 0x7dc completion flags of the DMA list entries */
    UlDma dma[22];            /* 0x834 */
    UlShared *shared;           /* 0x994 */
    int *spu_status;                /* 0x998 */
    unsigned char nbuf;       /* 0x99c */
    unsigned char cur;        /* 0x99d */
    char pad_99e[2];
    int off;                  /* 0x9a0 */
    int ncmd;                    /* 0x9a4 */
    UlBuf *bufs[7];           /* 0x9a8 */
} UlSnd;

/* libuliop module table (uliop_work) */
typedef struct UlIop {
    char pad_00[4];
    unsigned char init;       /* 0x04 */
    unsigned char dbg;        /* 0x05 */
    char pad_06[2];
    void (*err_cb)(int);      /* 0x08 */
    unsigned char dev;        /* 0x0c boot device: 0 cdrom0:, 1 host0:, 2 hd0:, 3 usblan0: */
    char pad_0d[3];
    unsigned char rebooted;   /* 0x10 */
    char pad_11[3];
    int count;                /* 0x14 */
    char path[260];           /* 0x18 module directory prefix */
    char ent[16][20];         /* 0x11c module names */
} UlIop;

/* Gindows config, copied into gw_config at init (48 bytes). */
typedef struct GwConf {
    int heap;           /* 0 heap base */
    int heap_size;      /* 4 */
    int w;              /* 8 screen width */
    int h;              /* 12 screen height */
    int font;           /* 16 */
    int font_mode;            /* 20 */
    int pad_port;       /* 24 pad record the pointer reads (gw_get_pad_port) */
    float scale;        /* 28 */
    int console_conf[4];       /* 32..44 */
} GwConf;

/* Heap block header (32 bytes) before every allocation. */
typedef struct GwMem GwMem;
struct GwMem {
    unsigned short magic;   /* 0 0xa5a5 */
    unsigned short used;    /* 2 */
    unsigned size;          /* 4 including header */
    GwMem *prev;            /* 8 */
    GwMem *next;            /* 12 */
    char *file;             /* 16 allocation site */
    unsigned line;          /* 20 */
    char pad_18[8];
};

/* Button/icon window flags */
#define GW_F_DISABLED 0x20000000
#define GW_F_PRESSED 0x40000
#define GW_F_RELEASED 0x40

/* Texture viewer state (gwu_texview.c), in the window user data. */
typedef struct GwTexView {
    float x;            /* 0 image origin */
    float y;            /* 4 */
    int tex_info[2];        /* 8 */
    int index;          /* 16 */
    int mode;           /* 20 1 = show image and info */
    int port;            /* 24 pad port for zoom keys */
    float scale;        /* 28 */
    int level;          /* 32 0..4 */
} GwTexView;

/* Slider (GwWin block; own words from 164). */
typedef struct GwSlider GwSlider;
typedef void (*GwSliderFn)(GwSlider *);

/* Slider creation values, copied whole into the window. */
typedef struct GwSliderVals {
    float val;          /* 0 */
    float min;          /* 4 */
    float max;          /* 8 */
    float fine_step;         /* 12 */
    float step;         /* 16 */
} GwSliderVals;

struct GwSlider {
    char pad_00[164];
    int vertical;       /* 164 0 horizontal, 1 vertical */
    char pad_a8[4];
    GwWin *thumb;       /* 172 */
    GwSliderVals vals;     /* 176..195 */
    int x0;             /* 196 track rectangle */
    int y0;             /* 200 */
    int len_x;          /* 204 */
    int len_y;          /* 208 */
    char pad_d4[4];
    GwSliderFn cb;      /* 216 on change */
};

/* Timing bar: start/end in scaled ticks, colour. */
typedef struct GwProfEntry {
    unsigned short start;   /* 0 */
    unsigned short end;     /* 2 */
    int color;              /* 4 */
} GwProfEntry;

/*
 * Bar group (1044-byte allocation). Two 64-entry pages alternate: one fills while the last
 * frame's is shown.
 */
typedef struct GwProfGroup GwProfGroup;
struct GwProfGroup {
    char *name;                 /* 0 */
    GwProfGroup *next;          /* 4 */
    GwProfEntry ent[2][64];       /* 8 */
    int prev_count;             /* 1032 entries of the page shown, minus one */
    int count;                  /* 1036 allocated this frame */
    int cur;                    /* 1040 last started entry */
};

/* Profiler (44-byte allocation, gw_profiler_main). */
typedef struct GwProf {
    GwWin *win;                 /* 0 graph window */
    GwBtn *btn[4];              /* 4..19 scale +/-, period +/- */
    int page;                   /* 20 page being filled (0/1) */
    int t0;                     /* 24 counter at frame start */
    GwProfGroup *groups;        /* 28 */
    int period;                 /* 32 frames between page swaps */
    int cnt;                    /* 36 frames to next swap */
    float scale;                /* 40 horizontal */
} GwProf;

typedef void (*GwColorFn)(GwWin *w);

/* Colour edit window user block (80 bytes); rec[] = A, R, G, B. */
typedef struct GwColorChan {
    float val;                  /* 0 0..255 */
    GwWin *edit;                /* 4 text field */
    GwSlider *slider;           /* 8 */
} GwColorChan;

typedef struct GwColorEdit {
    char pad_00[12];
    GwColorChan rec[4];         /* 12..59 */
    unsigned color;             /* 60 packed */
    GwWin *preview;             /* 64 preview panel */
    GwWin *ok;                  /* 68 */
    GwColorFn on_change;        /* 72 */
    GwColorFn on_close;         /* 76 on destroy */
} GwColorEdit;

/* Memory dump window user block. */
typedef struct GwDump {
    unsigned addr;              /* 0 first address shown */
    unsigned end;               /* 4 last address (0xffffffff when below addr) */
    unsigned type;              /* 8 0 BYTE, 1 WORD, 2 LONG, 3 FLOAT */
    int stride;                 /* 12 bytes per row: 8 or 16 */
    GwWin *addr_edit;           /* 16 */
    char pad_14[16];
    GwWin *toggle;              /* 36 '> <' toggle (row width) */
    GwWin *frame;               /* 40 'dump frame' view window */
    GwWin *choice;              /* 44 'Dump Type' choice box */
} GwDump;

/*
 * Var list entry. kind: 0 group, 1 command, 2 int, 5 angle (16-bit fixed, in degrees), 6 bool,
 * 7 bit field (mask val, shift min), 8 enum (strings in val). Value at *ptr, or val if ptr is 0.
 */
typedef struct GwVar GwVar;
struct GwVar {
    unsigned kind;      /* 0 */
    int flags;          /* 4 */
    char *name;         /* 8 */
    int help;           /* 12 */
    int *ptr;           /* 16 */
    void (*cb)(int);    /* 20 called with the new value */
    int val;            /* 24 inline value / mask / string table */
    int min;            /* 28 (shift for bit fields); float for angle entries */
    int max;            /* 32 float for angle entries */
    int step;           /* 36 float for angle entries */
    GwVar *next;        /* 40 */
};

/* Texture viewer window state (window user pointer). */
typedef struct GwTexWin {
    int x;              /* 0 rectangle */
    int y;              /* 4 */
    int w;              /* 8 */
    int h;              /* 12 */
    GwWin *frame;       /* 16 */
    GwWin *ctl[13];     /* 20..71 controls, in creation order */
    int lay[5];         /* 72..91 layout results */
    int index;          /* 92 */
    int count;          /* 96 */
    unsigned id;        /* 100 */
    float scx;          /* 104 scroll */
    float scy;          /* 108 */
    float sc2x;         /* 112 */
    float sc2y;         /* 116 */
    unsigned back;      /* 120 */
    unsigned flip;      /* 124 0x40000 FlipU | 0x20000 FlipV */
    int clamp;          /* 128 */
    int flip_u;         /* 132 */
    int flip_v;         /* 136 */
    int tr;             /* 140 */
    int ss;             /* 144 */
    int bi;             /* 148 */
    int zi;             /* 152 zoom table index */
    float zoom;         /* 156 */
    float draw_scx;           /* 160 copy of scx */
    float draw_scy;           /* 164 copy of scy */
    float draw_sc2x;           /* 168 copy of sc2x */
    float draw_sc2y;           /* 172 copy of sc2y */
} GwTexWin;

/* Viewer rectangle corner (24 bytes). */
typedef struct GwSpritePt {
    float x;
    float y;
    float z;
    float u;
    float v;
    unsigned col;
} GwSpritePt;

/* Texture manager record (viewer fields). */
typedef struct GwTex {
    char pad_00[52];
    int mxl;            /* 52 mip levels */
    int psm;            /* 56 pixel storage mode */
    int cpsm;           /* 60 palette storage mode */
    char pad_40[104];
    unsigned id;        /* 168 */
} GwTex;

/* Peripheral info window user block. */
typedef struct GwPeri {
    int port;           /* 0 0 or 1 */
    int id;             /* 4 */
    int state;          /* 8 */
    int vib;            /* 12 */
    GwWin *prev_btn;    /* 16 '<' button */
    GwWin *next_btn;    /* 20 '>' button */
    GwWin *vib_btn;     /* 24 'PRS' vibration button */
} GwPeri;

/* Actuator table of a pad (reached through GwPadInfo.act). */
typedef struct GwActInfo {
    char pad_00[1];
    unsigned char info[7]; /* 1..7 */
    int nact;              /* 8 */
    struct {
        int func;
        int sub;
        int size;
        int curr;       /* current in units of 10 mA */
    } act[1];             /* 12.. */
} GwActInfo;

/* Pad state (fields the info window shows). */
typedef struct GwPadInfo {
    char pad_00[8];
    unsigned char pressure[12];    /* 8..19 button pressures */
    char pad_14[12];
    GwActInfo *act;         /* 32 */
    char pad_24[2];
    unsigned char flags;    /* 38 */
    char pad_27[1];
    int on;                 /* 40 */
    char pad_2c[4];
    int press;              /* 48 */
    char pad_34[4];
    int repeat;             /* 56 */
    char pad_3c[4];
    unsigned char stick[4]; /* 64..67 */
} GwPadInfo;

struct GwWin;

/* Text field (kind 7, "Text Field", 364-byte GwWin block; own words from 164). */
typedef struct GwTextField GwTextField;
typedef void (*GwTextFieldFn)(GwTextField *);
typedef void (*GwTextFieldFnF)(GwTextField *, float);

struct GwTextField {
    char pad_00[36];
    int w;                  /* 36 */
    int h;                  /* 40 */
    char pad_2c[36];
    GwTextFieldFn update;   /* 80 */
    GwTextFieldFnF draw;    /* 84 */
    GwTextFieldFn destroy;  /* 88 */
    char pad_5c[72];
    int mode;               /* 164 0 plain text, 1 masked (one '*' per character) */
    char pad_a8[4];
    int blink;              /* 172 frame counter, bit 4 toggles the caret */
    char text[128];         /* 176..303 */
    int cursor;             /* 304 caret position (text length after set) */
    int state;              /* 308 2 = editing finished */
    char pad_138[4];
    GwTextFieldFn on_enter; /* 316 on enter/cancel */
    int xoff;               /* 320 text origin x */
};

#endif /* REZ_TYPES_GW_H */
