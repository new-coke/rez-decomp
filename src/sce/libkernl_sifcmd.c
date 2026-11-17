/*
 * SIF command layer, EE side: packets (with optional data) to the IOP by DMA, dispatch of IOP
 * packets to handlers, and start-up of the RPC layer.
 */

#include "types_libc.h"
#include "types_sce.h"
#include "libkernl_sifcmd.h"
#include "kernel_syscalls.h"
#include "libkernl_intc.h"
#include "libkernl_intr.h"
#include "libkernl_sifrpc.h"

typedef struct SifDmaW { unsigned int data; unsigned int addr; unsigned int size; unsigned int mode; } SifDmaW;

extern int libkernl_sif_sreg_tbl[];
extern int libkernl_sif_cmd_inited;
extern CmdCtl libkernl_sif_cmd_ctl;
extern int libkernl_sif_dmac_hid;
extern CmdPkt libkernl_sif_init_pkt;
extern char libkernl_sif_pkt_buf[];
extern char libkernl_sif_cmd_buf[];
extern CmdHandler libkernl_sif_sys_cmd_tbl[32];
extern int AddDmacHandler(int ch, int (*handler)(int), int next);
extern unsigned int isceSifSetDma(DmaDesc *d, int n);

extern char libkernl_sif_pkt_tbl[];
extern char libkernl_sif_rdata_tbl[];
extern char libkernl_sif_client_tbl[];

/* System handler: the IOP sets a shared status value (cb_arg->table). */
/* 0x001f7d60 */
static void libkernl_sif_cmd_set_word(CmdData *data, CmdArg *cb_arg)
{
    cb_arg->table[data->index] = data->value;
}

/* System handler: the IOP's command buffer address. */
/* 0x001f7d80 */
static void libkernl_sif_cmd_store_word(CmdData *data, CmdArg *cb_arg)
{
    cb_arg->value = data->index;
}

/* One of the 32 values the IOP can set. */
/* 0x001f7d90 */
static int sceSifGetSreg(int i)
{
    return libkernl_sif_sreg_tbl[i];
}

/* 0x001f7da8 */
static int sceSifSetSreg(int i, int val)
{
    libkernl_sif_sreg_tbl[i] = val;
    return val;
}

/* 0x001f7dc8 */
static int *libkernl_sif_cmd_state(void)
{
    return ((int *)&libkernl_sif_cmd_ctl); /* steer(cast) */
}

/*
 * Once: packet buffers, 32-entry system table, status values, SIF DMA handler (channel 5). Sends
 * the EE buffer if the IOP published its own; else waits, publishes both, sends the init packet.
 */
/* 0x001f7dd8 */
void sceSifInitCmd(void)
{
    int i;

    DIntr();
    if (libkernl_sif_cmd_inited) {
        EIntr();
        return;
    }
    libkernl_sif_cmd_inited = 1;
    libkernl_sif_cmd_ctl.pktbuf = (void *)((unsigned int)libkernl_sif_pkt_buf | 0x20000000);
    libkernl_sif_cmd_ctl.cbuf = (void *)((unsigned int)libkernl_sif_cmd_buf | 0x20000000);
    libkernl_sif_cmd_ctl.sys_size = 32;
    libkernl_sif_cmd_ctl.iopbuf = 0;
    libkernl_sif_cmd_ctl.sys_tbl = libkernl_sif_sys_cmd_tbl;
    libkernl_sif_cmd_ctl.usr_tbl = 0;
    libkernl_sif_cmd_ctl.usr_size = 0;
    libkernl_sif_cmd_ctl.sregs = libkernl_sif_sreg_tbl;
    for (i = 0; i < 32; i++) {
        libkernl_sif_sys_cmd_tbl[i].func = 0;
        libkernl_sif_sys_cmd_tbl[i].arg = 0;
    }
    for (i = 0; i < 32; i++)
        libkernl_sif_sreg_tbl[i] = 0;
    libkernl_sif_sys_cmd_tbl[0].func = libkernl_sif_cmd_store_word;
    libkernl_sif_sys_cmd_tbl[1].func = libkernl_sif_cmd_set_word;
    libkernl_sif_sys_cmd_tbl[0].arg = &libkernl_sif_cmd_ctl;
    libkernl_sif_sys_cmd_tbl[1].arg = &libkernl_sif_cmd_ctl;
    EIntr();
    FlushCache(0);
    if (*(volatile unsigned int *)0x1000e010 & 0x20)
        *(volatile unsigned int *)0x1000e010 = 0x20;
    if (!(*(volatile unsigned int *)0x1000c000 & 0x100))
        sceSifSetDChain();
    libkernl_sif_dmac_hid = AddDmacHandler(5, libkernl_sif_cmd_dma_handler, 0);
    EnableDmac(5);
    if ((libkernl_sif_cmd_ctl.iopbuf = (void *)sceSifGetReg(0x80000000)) != 0) {
        libkernl_sif_init_pkt.addr = libkernl_sif_pkt_buf;
        sceSifSendCmd(0x80000000, &libkernl_sif_init_pkt, 20, 0, 0, 0);
        return;
    } else {
        while (!(sceSifGetReg(4) & 0x20000))
            ;
        libkernl_sif_cmd_ctl.iopbuf = (void *)sceSifGetReg(2);
        sceSifSetReg(0x80000000, (int)libkernl_sif_cmd_ctl.iopbuf);
        sceSifSetReg(0x80000001, (int)&libkernl_sif_cmd_ctl);
        libkernl_sif_init_pkt.addr = libkernl_sif_pkt_buf;
        libkernl_sif_init_pkt.opt = 0;
        sceSifSendCmd(0x80000002, &libkernl_sif_init_pkt, 20, 0, 0, 0);
    }
}

/* Stops SIF DMA and removes its handler. */
/* 0x001f8058 */
void sceSifExitCmd(void)
{
    DisableDmac(5);
    RemoveDmacHandler(5, libkernl_sif_dmac_hid);
    libkernl_sif_cmd_inited = 0;
}

/* Returns the previous table. */
/* 0x001f8090 */
static CmdHandler *sceSifSetCmdBuffer(CmdHandler *tbl, int size)
{
    CmdCtl *ctl = &libkernl_sif_cmd_ctl;
    CmdHandler *old;

    old = ctl->usr_tbl;
    ctl->usr_tbl = tbl;
    ctl->usr_size = size;
    return old;
}

/* Returns the previous table. */
/* 0x001f80a8 */
static CmdHandler *sceSifSetSysCmdBuffer(CmdHandler *tbl, int size)
{
    CmdCtl *ctl = &libkernl_sif_cmd_ctl;
    CmdHandler *old;

    old = ctl->sys_tbl;
    ctl->sys_tbl = tbl;
    ctl->sys_size = size;
    return old;
}

/* Top bit set: system table; else user table. */
/* 0x001f80c0 */
void sceSifAddCmdHandler(int cid, void (*func)(void *, void *), void *cb_arg)
{
    if (cid < 0) {
        libkernl_sif_cmd_ctl.sys_tbl[cid & 0x7fffffff].func = func;
        libkernl_sif_cmd_ctl.sys_tbl[cid & 0x7fffffff].arg = cb_arg;
    } else {
        libkernl_sif_cmd_ctl.usr_tbl[cid].func = func;
        libkernl_sif_cmd_ctl.usr_tbl[cid].arg = cb_arg;
    }
}

/* 0x001f80f0 */
void sceSifRemoveCmdHandler(int cid)
{
    if (cid < 0)
        libkernl_sif_cmd_ctl.sys_tbl[cid & 0x7fffffff].func = 0;
    else
        libkernl_sif_cmd_ctl.usr_tbl[cid].func = 0;
}

/*
 * pkt 16..112 bytes, preceded by size bytes src -> dest when size > 0. Mode bit 0: interrupt
 * caller; bit 2: cache write-back first. Returns the DMA queue id, 0 for a bad packet size.
 */
/* 0x001f8118 */
static unsigned int libkernl_sif_send_cmd_mode(int cid, int mode, CmdPkt *pkt, int pktsize, void *src, void *dest,
                          int size)
{
    SifDmaW dma[2];
    int n;

    if ((unsigned int)(pktsize - 16) > 96)
        return 0;
    n = 0;
    if (size > 0) {
        n = 1;
        dma[0].data = (unsigned int)src;
        dma[0].addr = (unsigned int)dest;
        dma[0].size = size;
        dma[0].mode = 0;
        pkt->dsize = size;
        pkt->dest = dest;
        if (mode & 4)
            sceSifWriteBackDCache(src, size);
    } else {
        pkt->dsize = 0;
        pkt->dest = 0;
    }
    pkt->cid = cid;
    pkt->psize = pktsize;
    dma[n].data = (unsigned int)pkt;
    dma[n].addr = (unsigned int)libkernl_sif_cmd_ctl.iopbuf;
    dma[n].size = pktsize;
    dma[n].mode = 0x44;
    sceSifWriteBackDCache(pkt, pktsize);
    n++;
    if (mode & 1)
        return isceSifSetDma((DmaDesc *)dma, n);
    else
        return sceSifSetDma((DmaDesc *)dma, n);
}

/* From a thread. */
/* 0x001f8250 */
unsigned int sceSifSendCmd(int cid, CmdPkt *pkt, int pktsize, void *src, void *dest, int size)
{
    return libkernl_sif_send_cmd_mode(cid, 0, pkt, pktsize, src, dest, size);
}

/* From an interrupt handler. */
/* 0x001f8290 */
unsigned int isceSifSendCmd(int cid, CmdPkt *pkt, int pktsize, void *src, void *dest, int size)
{
    return libkernl_sif_send_cmd_mode(cid, 1, pkt, pktsize, src, dest, size);
}

/*
 * Once: command layer, three uncached 32-entry tables (packets, receive buffers, clients),
 * handlers for end, bind, call, rdata; on first boot signals the IOP and waits.
 */
/* 0x001f84a8 */
void sceSifInitRpc(void)
{
    RpcData *rd;
    CmdPkt *pkt;

    ((int (*)(void))DIntr)(); /* steer(cast) */
    if (libkernl_sif_rpc_inited) {
        EIntr();
    } else {
        libkernl_sif_rpc_inited = 1;
        EIntr();
        sceSifInitCmd();
        ((int (*)(void))DIntr)(); /* steer(cast) */
        rd = &libkernl_sif_rpc_data;
        rd->pkt_table = (void *)((unsigned int)libkernl_sif_pkt_tbl | 0x20000000);
        rd->pkt_table_len = 32;
        rd->unk_0c = 0;
        rd->unk_10 = 0;
        rd->rdata_table = (void *)((unsigned int)libkernl_sif_rdata_tbl | 0x20000000);
        rd->rdata_table_len = 32;
        rd->client_table = (void *)((unsigned int)libkernl_sif_client_tbl | 0x20000000);
        rd->client_table_len = 32;
        rd->rdata_table_idx = 0;
        libkernl_sif_rpc_data.pid = 1;
        sceSifAddCmdHandler(0x80000008, libkernl_sif_end_cmd_handler, rd);
        sceSifAddCmdHandler(0x80000009, libkernl_sif_bind_cmd_handler, rd);
        sceSifAddCmdHandler(0x8000000a, libkernl_sif_call_cmd_handler, rd);
        sceSifAddCmdHandler(0x8000000c, libkernl_sif_rdata_cmd_handler, rd);
        EIntr();
        if (!sceSifGetReg(0x80000002)) {
            pkt = (CmdPkt *)(libkernl_sif_pkt_tbl + 64);
            pkt->opt = 1;
            sceSifSendCmd(0x80000002, pkt, 16, 0, 0, 0);
            while (!sceSifGetSreg(0))
                ;
            sceSifSetReg(0x80000002, 1);
        }
    }
}
