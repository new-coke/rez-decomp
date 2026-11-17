/*
 * Controller library, EE side: ports via the IOP pad driver, which DMAs state into a
 * double-buffered frame area per vblank. Modes, actuators, DUALSHOCK 2 pressure mode.
 */

#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libpad.h"
#include "kernel_syscalls.h"
#include "libkernl_cache.h"
#include "libkernl_sifrpc.h"

static int libpad_reset(void);
static int scePadGetModVersion(void);

typedef struct SifClient40_a {      /* 40 bytes */
    char pad_00[36];
    void *server;                   /* 0x24 */
} SifClient40_a;

typedef struct {
    char pad_00[64];
} PadDmaBuf;

typedef struct {
    unsigned char b[12];
} PadVrefParam;

extern PadSlot libpad_slots[2][4];
extern int libpad_debug;
extern int libpad_initialized;
extern SifClient40_a libpad_clients[2];
extern PadRpcBuf libpad_rpc_buf;

extern PadDmaBuf libpad_dma_bufs[2][4];
extern char *libpad_state_names[];
extern const char libpad_empty_str[];
extern char *libpad_req_state_names[];

/*
 * 32-byte block into the IOP buffer half its counter selects; skipped while busy. Typo is original.
 */
/* 0x001e0098 */
static void libpad_send_request(int port, int slot)
{
    union {
        SifDmaA dma;
        char buf[256];
    } xfer;
    int *src;
    int dst;
    int id;

    src = libpad_slots[port][slot].dma_buf;
    if (sceSifDmaStat(libpad_slots[port][slot].dma_id) >= 0) {
        if (libpad_debug)
            printf("libpad: sceSifSetDma faild\n");
        return;
    }
    dst = (int)libpad_slots[port][slot].iop_buf;
    src[0]++;
    dst += (src[0] & 1) << 5;
    SyncDCache(src, src + 8);
    xfer.dma.data = src;
    xfer.dma.addr = dst;
    xfer.dma.size = 32;
    xfer.dma.mode = 0;
    id = sceSifSetDma(&xfer.dma, 1);
    if (id == 0) {
        if (libpad_debug)
            printf("libpad: sceSifSetDma faild\n");
    }
    libpad_slots[port][slot].dma_id = id;
}

/* Busy-waits for both servers; needs padman.irx 4.x (else 0); resets every port. */
/* 0x001e01c8 */
int scePadInit(int mode)
{
    int i;
    int ver;

    libpad_initialized = 1;
    for (;;) {
        sceSifBindRpc(&libpad_clients[0], 0x80000100, 0);
        if (libpad_clients[0].server != 0)
            break;
        i = 0x10000;
        while (i--)
            ;
    }
    for (;;) {
        sceSifBindRpc(&libpad_clients[1], 0x80000101, 0);
        if (libpad_clients[1].server != 0)
            break;
        i = 0x10000;
        while (i--)
            ;
    }
    ver = scePadGetModVersion();
    if ((ver >> 8) != 4) {
        if (libpad_debug) {
            printf("libpad: Module version mismatch ");
            printf("[libpad.a = %d.%d, padman.irx = %d.%d]\n", 4, 0, ver >> 8, ver & 0xff);
        }
        return 0;
    }
    return ((int (*)(int))libpad_reset)(mode); /* steer(cast) */
}

/* Marks all ports closed; driver answer, or 0. */
/* 0x001e0308 */
static int libpad_reset(void)
{
    PadRpcBuf *rpc;
    int i;

    for (i = 0; i < 4; i++) {
        libpad_slots[0][i].open = 0;
        libpad_slots[0][i].unk_18 = 0;
        libpad_slots[0][i].unk_14 = 0;
        libpad_slots[1][i].open = 0;
        libpad_slots[1][i].unk_18 = 0;
        libpad_slots[1][i].unk_14 = 0;
    }
    rpc = &libpad_rpc_buf;
    rpc->cmd = 16;
    rpc->param = 0;
    if (sceSifCallRpc(libpad_clients, 1, 0, rpc, 128, rpc, 128, 0, 0) < 0)
        return 0;
    return rpc->data;
}

/* 1 on success. */
/* 0x001e03a8 */
int scePadEnd(void)
{
    PadRpcBuf *rpc;
    int ret;

    rpc = &libpad_rpc_buf;
    rpc->cmd = 15;
    if (sceSifCallRpc(libpad_clients, 1, 0, rpc, 128, rpc, 128, 0, 0) < 0)
        return 0;
    ret = rpc->data;
    if (ret == 1)
        libpad_initialized = 0;
    return ret;
}

/* addr: 256 bytes, 64-aligned, two frames. 1 ok; 0 misaligned, already open, or call failed. */
/* 0x001e0428 */
int scePadPortOpen(int port, int slot, int addr)
{
    PadFrame *frame;
    PadRpcBuf *rpc;
    int i;

    if (addr & 0x3f) {
        if (libpad_debug)
            printf("libpad: buffer addr is not 64 byte align. %08x\n", addr);
        return 0;
    }
    if (libpad_slots[port][slot].open == 1) {
        if (libpad_debug)
            printf("libpad: pad port is already open [%d][%d]\n", port, slot);
        return 0;
    }
    frame = (PadFrame *)addr;
    for (i = 0; i < 2; i++) {
        frame->count = 0;
        frame->state = 5;
        frame->req_state = 2;
        frame->unk_67 = 0;
        memset(frame, 255, 32);
        frame->len = 0;
        frame++;
    }
    rpc = &libpad_rpc_buf;
    rpc->cmd = 1;
    rpc->port = port;
    rpc->slot = slot;
    rpc->param = addr;
    if (sceSifCallRpc(libpad_clients, 1, 0, rpc, 128, rpc, 128, 0, 0) < 0)
        return 0;
    {
        int *dma_buf;

        dma_buf = (int *)&libpad_dma_bufs[port][slot];
        libpad_slots[port][slot].open = 1;
        libpad_slots[port][slot].iop_buf = *(int **)&rpc->result;
        libpad_slots[port][slot].frames = (void *)addr;
        libpad_slots[port][slot].dma_buf = dma_buf;
        *dma_buf = 0;
        libpad_slots[port][slot].dma_id = 0;
    }
    return rpc->data;
}

/* 0 if it was not open. */
/* 0x001e0610 */
int scePadPortClose(int port, int slot)
{
    PadRpcBuf *rpc;

    if (libpad_slots[port][slot].open == 0)
        return 0;
    rpc = &libpad_rpc_buf;
    rpc->cmd = 14;
    rpc->port = port;
    rpc->slot = slot;
    rpc->param = 1;
    if (sceSifCallRpc(libpad_clients, 1, 0, rpc, 128, rpc, 128, 0, 0) < 0)
        return 0;
    libpad_slots[port][slot].open = 0;
    return rpc->data;
}

/* Higher counter wins. */
/* 0x001e06c8 */
static PadFrame *libpad_get_frame(int port, int slot)
{
    PadFrame *frame;
    int i;

    frame = (PadFrame *)libpad_slots[port][slot].frames;
    SyncDCache(frame, frame + 2);
    i = frame[0].count < frame[1].count;
    return &frame[i];
}

/* 0 if not open. */
/* 0x001e0728 */
int scePadGetFrameCount(int port, int slot)
{
    if (libpad_slots[port][slot].open == 0)
        return 0;
    return libpad_get_frame(port, slot)->count;
}

/* Returns the length, 0 if not open. */
/* 0x001e0778 */
int scePadRead(int port, int slot, unsigned char *data)
{
    PadFrame *frame;

    if (libpad_slots[port][slot].open == 0)
        return 0;
    frame = libpad_get_frame(port, slot);
    memcpy(data, frame, frame->len);
    return frame->len;
}

/* Stable 6 reads as 5 while a mode change is pending; 99 if not open. */
/* 0x001e07f8 */
int scePadGetState(int port, int slot)
{
    PadFrame *frame;

    if (libpad_slots[port][slot].open == 0)
        return 99;
    frame = libpad_get_frame(port, slot);
    if (frame->state == 6) {
        if (frame->req_state == 2)
            return 5;
    }
    return frame->state;
}

/* "EXECCMD", "ERROR", ...; "" if unknown. */
/* 0x001e0870 */
void scePadStateIntToStr(unsigned int state, char *str)
{
    if (state < 8)
        strcpy(str, libpad_state_names[state]);
    else
        str[0] = libpad_empty_str[0];
}

/* 0 if not open. */
/* 0x001e08a8 */
int scePadSetReqState(int port, int slot, int state)
{
    if (libpad_slots[port][slot].open == 0)
        return 0;
    libpad_get_frame(port, slot)->req_state = state;
    return 1;
}

/* Mode change pending; 0 if not open. */
/* 0x001e0910 */
int scePadGetReqState(int port, int slot)
{
    if (libpad_slots[port][slot].open == 0)
        return 0;
    return libpad_get_frame(port, slot)->req_state;
}

/* "" if unknown. */
/* 0x001e0960 */
static void scePadReqIntToStr(unsigned int state, char *str)
{
    if (state < 4)
        strcpy(str, libpad_req_state_names[state]);
    else
        str[0] = libpad_empty_str[0];
}

/*
 * Stable, extended info only: offs -1 = entry count, else byte term (1..4) of entry offs. Else 0.
 */
/* 0x001e0998 */
int scePadInfoComb(int port, int slot, int offs, int term)
{
    PadFrame *frame;

    if (libpad_slots[port][slot].open == 0)
        return 0;
    frame = libpad_get_frame(port, slot);
    if (frame->stable != 1)
        return 0;
    if (frame->model < 2)
        return 0;
    if (offs >= frame->comb_cnt)
        return 0;
    if (offs == -1)
        return frame->comb_cnt;
    switch (term) {
    case 1:
        return frame->comb_tbl[offs][0];
    case 2:
        return frame->comb_tbl[offs][1];
    case 3:
        return frame->comb_tbl[offs][2];
    case 4:
        return frame->comb_tbl[offs][3];
    }
    return 0;
}

/*
 * Stable, reported only: offs -1 = actuator count, else item term (-1..2) of actuator offs. Else 0.
 */
/* 0x001e0ab8 */
int scePadInfoAct(int port, int slot, int offs, int term)
{
    PadFrame *frame;

    if (libpad_slots[port][slot].open == 0)
        return 0;
    frame = libpad_get_frame(port, slot);
    if (frame->stable != 1)
        return 0;
    if (frame->model < 2)
        return 0;
    if (offs == -1)
        return frame->act_cnt;
    if (offs >= frame->act_cnt)
        return 0;
    switch (term) {
    case -1:
        return frame->act_tbl[offs][0];
    case 0:
        return frame->act_tbl[offs][1];
    case 1:
        return frame->act_tbl[offs][2];
    case 2:
        return frame->act_tbl[offs][3];
    }
    return 0;
}

/*
 * Stable, no change pending. term 1 type (0 in config mode), 2 extended type, 3 mode index, 4 mode
 * table entry offs (offs -1: count). 0 without a mode table or for an unknown term.
 */
/* 0x001e0bd8 */
int scePadInfoMode(int port, int slot, int term, int offs)
{
    PadFrame *frame;

    if (libpad_slots[port][slot].open == 0)
        return 0;
    frame = libpad_get_frame(port, slot);
    if (frame->stable != 1)
        return 0;
    if (frame->req_state == 2)
        return 0;
    switch (term) {
    case 1:
        if (frame->mode_id == 0xf3)
            return 0;
        return frame->mode_id >> 4;
    case 2:
        if (frame->model == 1)
            return 0;
        return frame->mode_tbl[frame->mode_cur];
    case 3:
        if (frame->model == 1)
            return 0;
        return frame->mode_cur;
    case 4:
        if (frame->model == 1)
            return 0;
        if (offs == -1)
            return frame->mode_cnt;
        if (offs < frame->mode_cnt)
            return frame->mode_tbl[offs];
        return 0;
    }
    return 0;
}

/* lock locks the mode button. Marks a mode change pending. 1 on success. */
/* 0x001e0d10 */
int scePadSetMainMode(int port, int slot, int offs, int lock)
{
    PadRpcBuf *rpc;

    rpc = &libpad_rpc_buf;
    rpc->data = offs;
    rpc->param = lock;
    rpc->cmd = 6;
    rpc->port = port;
    rpc->slot = slot;
    if (sceSifCallRpc(libpad_clients, 1, 0, rpc, 128, rpc, 128, 0, 0) < 0)
        return 0;
    if (rpc->result == 1) {
        scePadSetReqState(port, slot, 2);
    }
    return rpc->result;
}

/* Six values; 0 if not stable. */
/* 0x001e0dc8 */
int scePadSetActDirect(int port, int slot, unsigned char *data)
{
    int *req;
    unsigned char *act;
    int i;

    if (libpad_get_frame(port, slot)->stable != 1)
        return 0;
    req = libpad_slots[port][slot].dma_buf;
    act = (unsigned char *)(req + 3);
    for (i = 0; i < 6; i++)
        act[i] = data[i];
    req[2] = 6;
    req[1] = 1;
    libpad_send_request(port, slot);
    return 1;
}

/* Maps the six scePadSetActDirect bytes to motors. Marks a mode change pending. 1 on success. */
/* 0x001e0e80 */
int scePadSetActAlign(int port, int slot, unsigned char *data)
{
    PadRpcBuf *rpc;
    unsigned char *req;
    int i;

    rpc = &libpad_rpc_buf;
    rpc->cmd = 8;
    rpc->port = port;
    rpc->slot = slot;
    req = (unsigned char *)&rpc->data;
    for (i = 0; i < 6; i++)
        req[i] = data[i];
    if (sceSifCallRpc(libpad_clients, 1, 0, rpc, 128, rpc, 128, 0, 0) < 0)
        return 0;
    if (rpc->result == 1) {
        scePadSetReqState(port, slot, 2);
    }
    return rpc->result;
}

/* All 18 bits on a DUALSHOCK 2; 0 if not reported. */
/* 0x001e0f58 */
int scePadGetButtonMask(int port, int slot)
{
    PadFrame *frame;

    if (libpad_slots[port][slot].open == 0)
        return 0;
    frame = libpad_get_frame(port, slot);
    if (frame->stable != 1)
        return 0;
    if (frame->model < 2)
        return 0;
    if (frame->mask_ver < 2)
        return 0;
    return (unsigned long)frame->mask0 + ((unsigned long)frame->mask1 << 8) + ((unsigned long)frame->mask2 << 16) + ((unsigned long)frame->mask3 << 24);
}

/* mode 0 turns it off. Marks a mode change pending. 1 on success. */
/* 0x001e1010 */
int scePadSetButtonInfo(int port, int slot, int mode)
{
    PadRpcBuf *rpc;

    rpc = &libpad_rpc_buf;
    rpc->data = mode;
    rpc->cmd = 10;
    rpc->port = port;
    rpc->slot = slot;
    if (sceSifCallRpc(libpad_clients, 1, 0, rpc, 128, rpc, 128, 0, 0) < 0)
        return 0;
    if (rpc->param == 1) {
        scePadSetReqState(port, slot, 2);
    }
    return rpc->param;
}

/* 0x001e10c0 */
int scePadInfoPressMode(int port, int slot)
{
    if (libpad_slots[port][slot].open == 0)
        return 0;
    return scePadGetButtonMask(port, slot) == 0x3ffff;
}

/* All twelve pressure buttons. */
/* 0x001e1120 */
int scePadEnterPressMode(int port, int slot)
{
    if (libpad_slots[port][slot].open == 0)
        return 0;
    return scePadSetButtonInfo(port, slot, 0xfff);
}

/* 0x001e1178 */
int scePadExitPressMode(int port, int slot)
{
    if (libpad_slots[port][slot].open == 0)
        return 0;
    return scePadSetButtonInfo(port, slot, 0);
}

/* 12 bytes. Marks a mode change pending. 1 on success. */
/* 0x001e11d0 */
static int scePadSetVrefParam(int port, int slot, PadVrefParam *data)
{
    PadRpcBuf *rpc;

    rpc = &libpad_rpc_buf;
    rpc->cmd = 11;
    rpc->port = port;
    rpc->slot = slot;
    *(PadVrefParam *)&rpc->data = *data;
    if (sceSifCallRpc(libpad_clients, 1, 0, rpc, 128, rpc, 128, 0, 0) < 0)
        return 0;
    if (rpc->vref_result == 1) {
        scePadSetReqState(port, slot, 2);
    }
    return rpc->vref_result;
}

/* 0 if the call fails. */
/* 0x001e12a0 */
static int scePadGetPortMax(void)
{
    PadRpcBuf *rpc;

    rpc = &libpad_rpc_buf;
    rpc->cmd = 12;
    if (sceSifCallRpc(libpad_clients, 1, 0, rpc, 128, rpc, 128, 0, 0) < 0)
        return 0;
    return rpc->data;
}

/* More than one behind a multitap; 0 if the call fails. */
/* 0x001e1308 */
static int scePadGetSlotMax(int port)
{
    PadRpcBuf *rpc;

    rpc = &libpad_rpc_buf;
    rpc->cmd = 13;
    rpc->port = port;
    if (sceSifCallRpc(libpad_clients, 1, 0, rpc, 128, rpc, 128, 0, 0) < 0)
        return 0;
    return rpc->data;
}

/* Major in the high byte; 0 if the call fails. */
/* 0x001e1370 */
static int scePadGetModVersion(void)
{
    PadRpcBuf *rpc;

    rpc = &libpad_rpc_buf;
    rpc->cmd = 18;
    if (sceSifCallRpc(libpad_clients, 1, 0, rpc, 128, rpc, 128, 0, 0) < 0)
        return 0;
    return rpc->data;
}

/* 0 if the call fails. */
/* 0x001e13d8 */
static int libpad_rpc20(int param)
{
    PadRpcBuf *rpc;

    rpc = &libpad_rpc_buf;
    rpc->cmd = 20;
    rpc->port = param;
    if (sceSifCallRpc(libpad_clients, 1, 0, rpc, 128, rpc, 128, 0, 0) < 0)
        return 0;
    return rpc->slot;
}

/* 0x001e1440 */
void libpad_bzero(unsigned char *p, int n)
{
    while (n--)
        *p++ = 0;
}
