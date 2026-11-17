/*
 * Sony EE library types (libgraph, libpad, libdma, libmc, libscf; kernel thread, TTY, SIF, RPC,
 * file I/O): registers, request packets, argument blocks.
 */

#ifndef REZ_TYPES_SCE_H
#define REZ_TYPES_SCE_H

#include "types_main.h"

typedef struct DmaChcr_a {
    unsigned int DIR : 1;
    unsigned int p0 : 1;
    unsigned int MOD : 2;
    unsigned int ASP : 2;
    unsigned int TTE : 1;
    unsigned int TIE : 1;
    unsigned int STR : 1;
    unsigned int p1 : 7;
    unsigned int TAG : 16;
} DmaChcr_a;

typedef struct {
    int port;
    int slot;
    int mode;
    char pad_0c[8];
    char name[1024];
} McOpenCmd;

/* ---- Sony libgraph ---- */
typedef unsigned long u_long64;     /* 64-bit on the EE */

typedef struct GsGParam {           /* libgraph_gparam */
    short interlace;                /* 0x00 */
    short omode;                    /* 0x02 */
    short ffmode;                   /* 0x04 */
    short version;                  /* 0x06: GS CSR bits 16..23 */
    int vsync_cb;
    int vsync_hid;
} GsGParam;

typedef struct {
    u_long64 SCAX0:11;
    u_long64 pad11:5;
    u_long64 SCAX1:11;
    u_long64 pad27:5;
    u_long64 SCAY0:11;
    u_long64 pad43:5;
    u_long64 SCAY1:11;
    u_long64 pad59:5;
} GsScissor;

typedef struct GsDispEnv {         /* 40 bytes */
    u_long64 pmode;                 /* 0x00 */
    u_long64 smode2;                /* 0x08 */
    u_long64 dispfb;                /* 0x10 */
    u_long64 display;               /* 0x18 */
    u_long64 bgcolor;               /* 0x20 */
} GsDispEnv;

typedef struct GsGifTag {           /* 16 bytes */
    u_long64 NLOOP:15;
    u_long64 EOP:1;
    u_long64 pad16:16;
    u_long64 id:14;
    u_long64 PRE:1;
    u_long64 PRIM:11;
    u_long64 FLG:2;
    u_long64 NREG:4;
    u_long64 REGS0:4;
    u_long64 REGS1:4;
    u_long64 REGS2:4;
    u_long64 REGS3:4;
    u_long64 REGS4:4;
    u_long64 REGS5:4;
    u_long64 REGS6:4;
    u_long64 REGS7:4;
    u_long64 REGS8:4;
    u_long64 REGS9:4;
    u_long64 REGS10:4;
    u_long64 REGS11:4;
    u_long64 REGS12:4;
    u_long64 REGS13:4;
    u_long64 REGS14:4;
    u_long64 REGS15:4;
} GsGifTag;

typedef struct {
    u_long64 FBP:9;
    u_long64 pad09:55;
} GsFbpReg;

typedef struct GsDispEnvB {         /* GsDispEnv with DISPFB as a bit-field */
    u_long64 pmode;
    u_long64 smode2;
    GsFbpReg dispfb;
    u_long64 display;
    u_long64 bgcolor;
} GsDispEnvB;

typedef struct GsDBuffA {           /* two display envs, two GIF draw packets */
    GsDispEnvB disp[2];             /* 0x000 */
    GsGifTag giftag0;               /* 0x050 */
    GsFbpReg frame0;                /* 0x060: draw env 0 starts with FRAME_1 */
    char draw0[120];
    char clear0[96];                /* 0x0e0 */
    GsGifTag giftag1;               /* 0x140 */
    GsFbpReg frame1;                /* 0x150 */
    char draw1[120];
    char clear1[96];                /* 0x1d0 */
} GsDBuffA;

#define GS_REG(off) (*(volatile u_long64 *)(0x12000000 + (off)))
#define GS_CSR (*(volatile u_long64 *)0x12001000)

/* ---- Sony libpad ---- */
typedef struct PadSlot {            /* 28 bytes, libpad_slots[port][slot] */
    void *frames;
    int *dma_buf;                    /* EE DMA buffer (counter at 0) */
    int *iop_buf;                    /* IOP buffer address */
    int dma_id;                     /* SIF DMA id */
    int open;
    int unk_14; /* cleared by libpad_reset */
    int unk_18; /* cleared by libpad_reset */
} PadSlot;

typedef struct SifDmaA {            /* SIF DMA request */
    void *data;
    int addr;
    int size;
    int mode;
} SifDmaA;

typedef struct PadRpcBuf {          /* libpad_rpc_buf, 128-byte RPC buffer */
    int cmd;
    int port;
    int slot;
    int data;
    int param;
    int result;
    char pad_18[4];
    int vref_result;
    char pad_20[96];
} PadRpcBuf;

typedef struct PadFrame {           /* 128 bytes, two per open slot; higher count is current */
    unsigned char data[32];
    char pad_20[16];
    unsigned char comb_tbl[4][4];     /* 0x30: table counted by unk_6a */
    unsigned char act_tbl[4][4];     /* 0x40: table counted by unk_6b */
    unsigned short mode_tbl[4];       /* 0x50: table counted by unk_68 */
    int count;
    char pad_5c[4];
    int len;
    unsigned char model;
    unsigned char mode_id;
    unsigned char mask_ver;
    unsigned char unk_67; /* cleared by scePadPortOpen */
    unsigned char mode_cnt;
    unsigned char mode_cur;
    unsigned char comb_cnt;
    unsigned char act_cnt;
    char pad_6c[4];
    unsigned char state;
    unsigned char req_state;
    unsigned char stable;
    char pad_73[6];
    unsigned char mask0;
    unsigned char mask1;
    unsigned char mask2;
    unsigned char mask3;
    char pad_7d[3];
} PadFrame;

/* ---- Sony libdma ---- */
#define DREG(a) (*(volatile unsigned int *)(a))

typedef struct DmaChanRegs {
    volatile unsigned int chcr;     /* 0x00 */
    char pad_04[12];
    volatile unsigned int madr;     /* 0x10 */
    char pad_14[12];
    volatile unsigned int qwc;      /* 0x20 */
    char pad_24[12];
    volatile unsigned int tadr;     /* 0x30 */
    char pad_34[12];
    volatile unsigned int as0;      /* 0x40 */
    char pad_44[12];
    volatile unsigned int as1;      /* 0x50 */
    char pad_54[44];
    volatile unsigned int sadr;     /* 0x80 */
} DmaChanRegs;

typedef struct DmaEnv {             /* 20 bytes, copied to libdma_cur_env */
    unsigned char sts;
    unsigned char std;
    unsigned char mfd;
    unsigned char rcyc;
    unsigned short express;
    unsigned short notify;
    unsigned short sqwc;
    unsigned short tqwc;
    void *rbadr;
    unsigned int rbmsk;
} DmaEnv;

/* ---- kernel parameter blocks ---- */
typedef struct SemaParamB {
    int count;
    int max;
    int init;
    int waiting;
    unsigned int attr;
    unsigned int option;
} SemaParamB;

typedef struct SifClientA {
    char pad_00[36];
    void *server;                   /* 0x24: non-zero once bound */
    char pad_28[8];
} SifClientA;

typedef struct McNameCmd {          /* libmc request with a path, libmc_name_cmd (1044 bytes) */
    int port;
    int slot;
    int flags;
    int maxent;
    void *table;
    char name[1024];
} McNameCmd;

typedef struct McTblEntry {         /* 64-byte directory entry, libmc_file_info */
    char pad_00[32];
    char name[32];
} McTblEntry;

/* ---- Sony libscf ---- */
typedef struct ScfOsdConfig {       /* system configuration word (GetOsdConfigParam) */
    unsigned int spdifMode:1;
    unsigned int screenType:2;
    unsigned int videoOutput:1;
    unsigned int japLanguage:1;
    unsigned int ps1drvConfig:8;
    unsigned int version:3;
    unsigned int language:5;
    int timezoneOffset:11;
} ScfOsdConfig;

typedef struct ScfOsdConfig2 {      /* extended configuration bytes (GetOsdConfigParam2) */
    unsigned char :4;           /* bits 0-3 unused */
    unsigned char summerTime:1;
    unsigned char timeNotation:1;
    unsigned char dateNotation:2;
} ScfOsdConfig2;

typedef struct ScfT10kConfig {      /* libscf_t10k_config: values for a "T" (development) ROM */
    short timezone;
    unsigned char screenType;
    unsigned char dateNotation;
    unsigned char language;
    unsigned char spdifMode;
    unsigned char summerTime;
    unsigned char timeNotation;
} ScfT10kConfig;

typedef struct ScfClock {           /* CD/RTC clock record (BCD or binary fields) */
    unsigned char stat;
    unsigned char second;
    unsigned char minute;
    unsigned char hour;
    unsigned char pad;
    unsigned char day;
    unsigned char month;
    unsigned char year;
} ScfClock;

#define SCF_ASSERT(e, line, s) ((e) ? (void)0 : __assert("libscf.c", line, s))

/* Thread operations queued from interrupt handlers, run by a kernel-helper thread. */
typedef struct DeferCmd {
    unsigned char type;     /* 0 wake up, 1 rotate ready queue, 2 suspend */
    unsigned char arg;      /* thread id or priority */
} DeferCmd;

typedef struct DeferQueue {
    int rd;                 /* 0x00 read count (index = rd & 0x1ff) */
    int wr;                 /* 0x04 write count */
    DeferCmd cmd[512];        /* 0x08 */
} DeferQueue;

typedef struct ThreadParam {
    int status;             /* 0x00 */
    void (*entry)(void *);  /* 0x04 */
    void *stack;            /* 0x08 */
    int stack_size;         /* 0x0c */
    void *gp;               /* 0x10 */
    int init_priority;      /* 0x14 */
    int current_priority;   /* 0x18 */
    unsigned int attr;      /* 0x1c */
    unsigned int option;    /* 0x20 */
    int wait_type;          /* 0x24 */
    int wait_id;            /* 0x28 */
    int wakeup_count;       /* 0x2c */
} ThreadParam;

typedef struct SemaParam {
    int current_count;      /* 0x00 */
    int max_count;          /* 0x04 */
    int init_count;         /* 0x08 */
    int wait_threads;       /* 0x0c */
    unsigned int attr;      /* 0x10 */
    unsigned int option;    /* 0x14 */
} SemaParam;

/* TTY channel byte ring: header, then size data bytes. */
typedef struct ByteRing {
    int size;               /* 0x00 data bytes */
    int count;              /* 0x04 bytes held */
    unsigned char *rp;      /* 0x08 read pointer */
    unsigned char *wp;      /* 0x0c write pointer */
    unsigned char buf[1];   /* 0x10 */
} ByteRing;

/* libkernl_tty_ctl: socket, send and receive bookkeeping. */
typedef struct TtyCtl {
    volatile int sock;      /* 0x00 channel socket */
    volatile int wlen;      /* 0x04 bytes left to send */
    volatile int rlen;      /* 0x08 bytes received into rbuf */
    volatile int busy;      /* 0x0c send in progress */
    unsigned char *wptr;    /* 0x10 send position */
    unsigned char *rbuf;    /* 0x14 receive packet buffer */
    ByteRing *ring;         /* 0x18 received characters */
} TtyCtl;

/* TTY packet header (12 bytes), data follows. */
typedef struct TtyPkt {
    unsigned short len;     /* 0x00 total length */
    unsigned short rsvd;    /* 0x02 */
    unsigned short proto;   /* 0x04 0x210 */
    unsigned char src;      /* 0x06 'E' */
    unsigned char dest;     /* 0x07 'H' */
    int unk_08; /* 0x08: cleared by libkernl_tty_open */
} TtyPkt;

/* Command data and callback arg of libkernl_sif_cmd_set_word / _store_word. */
typedef struct CmdData {
    char pad_00[16];
    int index;              /* 0x10 */
    int value;              /* 0x14 */
} CmdData;

typedef struct CmdArg {
    char pad_00[8];
    int value;              /* 0x08 */
    char pad_0c[16];
    int *table;             /* 0x1c */
} CmdArg;

/* Command handler slot; libkernl_sif_cmd_ctl state (sceSifInitCmd). */
typedef struct CmdHandler {
    void (*func)(void *data, void *arg);
    void *arg;
} CmdHandler;

typedef struct CmdCtl {
    void *pktbuf;           /* 0x00 uncached packet buffer */
    void *cbuf;             /* 0x04 uncached receive buffer */
    void *iopbuf;           /* 0x08 peer receive buffer address */
    CmdHandler *sys_tbl;    /* 0x0c system handlers (negative ids) */
    int sys_size;           /* 0x10 */
    CmdHandler *usr_tbl;    /* 0x14 user handlers */
    int usr_size;           /* 0x18 */
    int *sregs;             /* 0x1c software registers */
} CmdCtl;

/* Command packet: 16-byte header, then payload (one address). */
typedef struct CmdPkt {
    unsigned int psize : 8; /* 0x00 packet size */
    unsigned int dsize : 24; /* extra data size */
    void *dest;             /* 0x04 */
    int cid;                /* 0x08 */
    unsigned int opt;       /* 0x0c */
    void *addr;             /* 0x10 */
} CmdPkt;

/* SIF DMA system call transfer descriptor. */
typedef struct DmaDesc {
    void *data;             /* 0x00 source */
    void *addr;             /* 0x04 destination */
    int size;               /* 0x08 */
    int mode;               /* 0x0c */
} DmaDesc;

/* libkernl_sif_rpc_data (sceSifInitRpc). */
typedef struct RpcData {
    int pid;                /* 0x00 last packet id */
    void *pkt_table;        /* 0x04 64-byte packets */
    int pkt_table_len;      /* 0x08 */
    int unk_0c; /* cleared by sceSifInitRpc */
    int unk_10; /* cleared by sceSifInitRpc */
    void *rdata_table;      /* 0x14 */
    int rdata_table_len;    /* 0x18 */
    void *client_table;     /* 0x1c */
    int client_table_len;   /* 0x20 */
    int rdata_table_idx;    /* 0x24 */
    struct RpcQueue *queue; /* 0x28 server queues */
} RpcData;

/* 64-byte RPC packet or client record; slot meanings differ by packet kind. */
typedef struct RpcPkt {
    void *cur_pkt;              /* 0x00 client: packet address */
    int cur_pkt_id;
    int sema;               /* 0x08 client: semaphore to signal, < 0 none */
    char pad_0c[4];
    unsigned int rid;       /* 0x10 bit 0 in use, packet index << 16 */
    void *pkt_addr;              /* 0x14 */
    int pkt_id;                /* 0x18 record id */
    struct RpcPkt *client;     /* 0x1c client: end function; request: client */
    void *cmd;              /* 0x20 client: end parameter; request: command id */
    void *server;              /* 0x24 */
    void *buff;              /* 0x28 */
    void *cbuff;              /* 0x2c */
    void *rmode;              /* 0x30 */
    char pad_34[12];
} RpcPkt;

/* RPC server and its serving queue. */
typedef struct RpcServer {
    unsigned int rpc_number; /* 0x00 */
    void *func;             /* 0x04 */
    void *buff;             /* 0x08 */
    int size;               /* 0x0c */
    void *cfunc;            /* 0x10 */
    void *cbuff;            /* 0x14 */
    char pad_18[4];
    int client;             /* 0x1c */
    void *pkt_addr;         /* 0x20 */
    int fno;                /* 0x24 */
    int receive;            /* 0x28 */
    int rsize;              /* 0x2c */
    int rmode;              /* 0x30 */
    int rid;                /* 0x34 */
    struct RpcServer *link; /* 0x38 next server of the queue */
    struct RpcServer *next; /* 0x3c next pending request */
    struct RpcQueue *base;  /* 0x40 */
} RpcServer;

/* Call request (command 0x8000000a), server view. */
typedef struct RpcCallReq {
    char pad_00[16];
    int rid;                /* 0x10 */
    void *pkt_addr;         /* 0x14 */
    char pad_18[4];
    int client;             /* 0x1c */
    int fno;                /* 0x20 */
    int size;               /* 0x24 */
    int receive;            /* 0x28 */
    int rsize;              /* 0x2c */
    int rmode;              /* 0x30 */
    struct RpcServer *server; /* 0x34 */
} RpcCallReq;

typedef struct RpcQueue {
    int thread_id;          /* 0x00 */
    int active;             /* 0x04 */
    RpcServer *link;        /* 0x08 servers */
    RpcServer *start;       /* 0x0c pending requests */
    RpcServer *end;         /* 0x10 */
    struct RpcQueue *next;  /* 0x14 */
} RpcQueue;

/* Four bytes copied as a unit, byte aligned. */
typedef struct Bytes4 {
    char b[4];
} Bytes4;

/* sif_fio_slots entry (16 bytes). */
typedef struct SlotEntry {
    int fd;
    int state;              /* 0x04 0 = free */
    char pad_08[8];
} SlotEntry;

/* File I/O argument block (sif_fio_arg); layout after 0x0c depends on the call. */
typedef struct FioArg {
    int sema;               /* 0x00 semaphore signalled on completion */
    void *rbuf;             /* 0x04 where the result word goes */
    int rsize;              /* 0x08 */
    int fd;                 /* 0x0c */
    int param;                 /* 0x10 */
    char name[1024];        /* 0x14 */
    char pad_414[4];
} FioArg;

/* Path calls (sif_fio_arg). */
typedef struct FioPathArg {
    int sema;               /* 0x00 */
    void *rbuf;             /* 0x04 */
    int rsize;              /* 0x08 */
    char name[1024];        /* 0x0c */
} FioPathArg;

/* 252 bytes copied as a unit (byte aligned). */
typedef struct SifArgBuf {
    char b[252];
} SifArgBuf;

/* Module loader arguments (sif_loadfile_arg, 512 bytes). */
typedef struct LoadArg {
    int id;                 /* 0x000 module id / result */
    int len;                /* 0x004 argument length / result */
    char path[252];         /* 0x008 */
    char args[252];         /* 0x104 */
} LoadArg;

/* Loader block as the memory peek/poke calls use it. */
typedef struct IopMemArg {
    int addr;               /* 0x00 address; the value read comes back here */
    unsigned int width;     /* 0x04 0 byte, 1 halfword, 2 word */
    int value;              /* 0x08 value to write */
} IopMemArg;

/* Reset packet (104 bytes); header word accessed as 64-bit. */
typedef struct ResetPkt {
    unsigned long psize : 8; /* 0x00 */
    unsigned long dsize : 24;
    void *dest;             /* 0x04 */
    int cid;                /* 0x08 */
    unsigned int opt;       /* 0x0c */
    int size;               /* 0x10 */
    int mode;               /* 0x14 */
    char arg[80];           /* 0x18 */
} ResetPkt;

/* Path calls with one extra word (sif_fio_arg). */
typedef struct FioTwoPathArg {
    int sema;               /* 0x00 */
    void *rbuf;             /* 0x04 */
    int rsize;              /* 0x08 */
    int mode;               /* 0x0c */
    char name[1024];        /* 0x10 */
} FioTwoPathArg;

/* Same, unsigned name (some path calls). */
typedef struct FioPathArg2u {
    int sema;               /* 0x00 */
    void *rbuf;             /* 0x04 */
    int rsize;              /* 0x08 */
    int mode;               /* 0x0c */
    unsigned char name[1024]; /* 0x10 */
} FioPathArg2u;

/* Two-path calls (sif_fio_arg). */
typedef struct FioPath2Arg {
    int sema;               /* 0x000 */
    void *rbuf;             /* 0x004 */
    int rsize;              /* 0x008 */
    char name1[1024];       /* 0x00c */
    char name2[1024];       /* 0x40c */
} FioPath2Arg;

/* Two-path calls with a data buffer (sif_fio_arg). */
typedef struct FioCtlArg {
    int sema;               /* 0x000 */
    void *rbuf;             /* 0x004 */
    int rsize;              /* 0x008 */
    char name1[1024];       /* 0x00c */
    char name2[1024];       /* 0x40c */
    char data[1024];        /* 0x80c */
    int len;                /* 0xc0c */
} FioCtlArg;

/* 64 bytes copied as a unit (byte aligned). */
typedef struct FioStatBuf {
    char b[64];
} FioStatBuf;

/* Status-change call (sif_fio_arg). */
typedef struct FioStatArg {
    int sema;               /* 0x00 */
    void *rbuf;             /* 0x04 */
    int rsize;              /* 0x08 */
    int mask;               /* 0x0c */
    FioStatBuf stat;           /* 0x10 */
    unsigned char name[1024]; /* 0x50 */
} FioStatArg;

/* Call 20: two names, a command word, a data buffer (sif_fio_arg). */
typedef struct FioCtl2Arg {
    int sema;               /* 0x000 */
    void *rbuf;             /* 0x004 */
    int rsize;              /* 0x008 */
    char name1[1024];       /* 0x00c */
    char name2[1024];       /* 0x40c */
    char data[1024];        /* 0x80c */
    int cmd;                /* 0xc0c */
    int len;                /* 0xc10 */
} FioCtl2Arg;

/* Call 23: name, command with argument data, reply buffer. */
typedef struct FioIoctlArg {
    int sema;               /* 0x000 */
    void *rbuf;             /* 0x004 */
    int rsize;              /* 0x008 */
    char name[1024];        /* 0x00c */
    char data[1024];        /* 0x40c */
    int cmd;                /* 0x80c */
    unsigned int arglen;    /* 0x810 */
    void *buf;              /* 0x814 */
    unsigned int buflen;    /* 0x818 */
} FioIoctlArg;

/* Call 26: descriptor command with argument data, reply buffer. */
typedef struct FioFdIoctlArg {
    int sema;               /* 0x000 */
    void *rbuf;             /* 0x004 */
    int rsize;              /* 0x008 */
    int handle;             /* 0x00c */
    int cmd;                /* 0x010 */
    char data[1024];        /* 0x014 */
    void *buf;              /* 0x414 */
    unsigned int buflen;    /* 0x418 */
    unsigned int arglen;    /* 0x41c */
} FioFdIoctlArg;

/* 64-bit seek, call 22 (sif_fio_arg). */
typedef struct FioSeek64Arg {
    int sema;               /* 0x00 */
    void *rbuf;             /* 0x04 */
    int rsize;              /* 0x08 */
    int handle;             /* 0x0c */
    long offset;            /* 0x10 */
    int whence;             /* 0x18 */
    int index;              /* 0x1c */
} FioSeek64Arg;

/* Write, call 3 (sif_fio_arg). */
typedef struct FioWriteArg {
    int sema;               /* 0x00 */
    void *rbuf;             /* 0x04 */
    int rsize;              /* 0x08 */
    int handle;             /* 0x0c */
    char *buf;              /* 0x10 */
    int size;               /* 0x14 */
    int head;               /* 0x18 unaligned head bytes sent inline */
    char headbuf[16];       /* 0x1c */
    int index;              /* 0x2c */
} FioWriteArg;

/* 1024 bytes copied as a unit (byte aligned). */
typedef struct FioIoctlBuf {
    char b[1024];
} FioIoctlBuf;

/* 64-bit integer as two signed 32-bit halves (little-endian), as the division helpers split it. */
typedef union LWords {
    struct {
        int low;
        int high;
    } s;
    long ll;
} LWords;

#endif /* REZ_TYPES_SCE_H */
