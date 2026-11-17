/*
 * SIF RPC, EE side: clients bind to IOP servers by number; EE servers run IOP requests on a thread.
 */

#include "types_libc.h"
#include "types_sce.h"
#include "libkernl_sifrpc.h"
#include "kernel_syscalls.h"
#include "libkernl_intr.h"
#include "libkernl_patch.h"
#include "libkernl_sifcmd.h"

/* Word view of an RpcPkt for sceSifCallRpc's header writes. */
typedef struct {
    volatile int cur_pkt, cur_pkt_id;
    char pad_08[12];
    int pkt_addr;
    int pkt_id;
    int client;
    volatile int cmd;
    int server;
    int rbuf;
    int rsize;
    char pad_30[4];
    int call_server;
    char pad_38[8];
} RpcPktW;

extern int CreateSema(SemaParam *p);

/* And the command layer under it. */
/* 0x001f8648 */
void sceSifExitRpc(void)
{
    sceSifExitCmd();
    libkernl_sif_rpc_inited = 0;
}

/* 64-byte packet, next id (skips 1 on wrap). 0 if all are in use. */
/* 0x001f8670 */
static RpcPkt *libkernl_sif_alloc_packet(RpcData *rd)
{
    RpcPkt *pkt;
    int i;
    int pid;

    DIntr();
    pkt = (RpcPkt *)rd->pkt_table;
    for (i = 0; i < rd->pkt_table_len; i++, pkt++) {
        do { } while (0);
        if (!(pkt->rid & 1)) {
            pkt->rid = (i << 16) | 5;
            if (++rd->pid == 1)
                pid = rd->pid++;
            else
                pid = rd->pid;
            pkt->pkt_addr = pkt;
            pkt->pkt_id = pid;
            EIntr();
            return pkt;
        }
    }
    EIntr();
    return 0;
}

/* 0x001f8718 */
static void libkernl_sif_free_packet(RpcPkt *pkt)
{
    pkt->pkt_id = 0;
    pkt->rid &= ~1;
}

/* 0x001f8738 */
static RpcPkt *libkernl_sif_next_rdata(RpcData *rd)
{
    int index;
    RpcPkt *pkt;

    index = rd->rdata_table_idx % rd->rdata_table_len;
    pkt = (RpcPkt *)((char *)rd->rdata_table + index * 64);
    rd->rdata_table_idx = index + 1;
    return pkt;
}

/* The next receive-data entry when rid is out of range. */
/* 0x001f8768 */
static RpcPkt *libkernl_sif_get_rdata(RpcData *rd, int rid)
{
    if (rid < 0 || rid >= rd->client_table_len)
        return libkernl_sif_next_rdata(rd);
    return (RpcPkt *)((char *)rd->client_table + rid * 64);
}

/*
 * Call: runs the client's end function; bind: stores server and buffers. Wakes, frees the packet.
 */
/* 0x001f87a8 */
void libkernl_sif_end_cmd_handler(RpcPkt *req, RpcData *rd)
{
    RpcPkt *cd;

    switch ((unsigned int)req->cmd) {
    case 0x8000000a:
        if (((RpcPkt *)req->client)->client)
            ((void (*)(void *))((RpcPkt *)req->client)->client)(((RpcPkt *)req->client)->cmd);
        break;
    case 0x80000009:
        cd = (RpcPkt *)req->client;
        *(int *)&cd->server = *(int *)&req->server;
        *(int *)&cd->pkt_addr = *(int *)&req->buff;
        cd->pkt_id = *(int *)&req->cbuff;
        break;
    case 0x8000000b:
        break;
    }
    cd = (RpcPkt *)req->client;
    if (cd->sema >= 0)
        iSignalSema(cd->sema);
    libkernl_sif_free_packet(cd->cur_pkt);
    cd->cur_pkt = 0;
}

/* Sends the requested EE range back with the answer. */
/* 0x001f8860 */
void libkernl_sif_rdata_cmd_handler(RpcPkt *req, RpcData *rd)
{
    RpcPkt *pkt;
    void *req_p14, *req_p1c;

    pkt = libkernl_sif_next_rdata(rd);
    req_p14 = req->pkt_addr;
    req_p1c = *(void **)&req->client;
    pkt->pkt_addr = req_p14;
    *(void **)&pkt->client = req_p1c;
    pkt->cmd = (void *)0x8000000c;
    isceSifSendCmd(0x80000008, pkt, 64, req->cmd, req->server, (int)req->buff);
}

/* IOP src -> EE dest; waits unless mode bit 0. -1 no packet, -2 send failed, -3 no semaphore. */
/* 0x001f88c0 */
static int sceSifGetOtherData(RpcPkt *rd, void *src, void *dest, int size, unsigned int mode)
{
    SemaParam sema_param;
    RpcPkt *pkt;

    pkt = libkernl_sif_alloc_packet(&libkernl_sif_rpc_data);
    if (!pkt)
        return -1;
    rd->cur_pkt = pkt;
    rd->cur_pkt_id = pkt->pkt_id;
    pkt->cmd = src;
    pkt->server = dest;
    pkt->buff = (void *)size;
    pkt->pkt_addr = pkt;
    pkt->client = rd;
    if (!(mode & 1)) {
        sema_param.max_count = 1;
        sema_param.init_count = 0;
        if ((rd->sema = CreateSema(&sema_param)) < 0) {
            libkernl_sif_free_packet(pkt);
            return -3;
        }
        if (!sceSifSendCmd(0x8000000c, pkt, 64, 0, 0, 0)) {
            libkernl_sif_free_packet(pkt);
            DeleteSema(rd->sema);
            return -2;
        }
        WaitSema(rd->sema);
        DeleteSema(rd->sema);
        return 0;
    } else {
        rd->sema = -1;
        if (!sceSifSendCmd(0x8000000c, pkt, 64, 0, 0, 0)) {
            libkernl_sif_free_packet(pkt);
            return -2;
        }
        return 0;
    }
}

/* In any of rd's server queues, or 0. */
/* 0x001f8a18 */
static RpcServer *libkernl_sif_find_server(unsigned int rpc_number, RpcData *rd)
{
    RpcQueue *qd;
    RpcServer *sd;

    for (qd = rd->queue; qd; qd = qd->next) {
        for (sd = qd->link; sd; sd = sd->link) {
            if (sd->rpc_number == rpc_number)
                return sd;
        }
    }
    return 0;
}

/* Answers with the server and buffers, or zeros if none has that number. */
/* 0x001f8a68 */
void libkernl_sif_bind_cmd_handler(RpcPkt *req, RpcData *rd)
{
    RpcPkt *pkt;
    RpcServer *sd;
    void *req_p1c, *req_p14;

    pkt = libkernl_sif_next_rdata(rd);
    req_p14 = req->pkt_addr;
    req_p1c = *(void **)&req->client;
    *(void **)&pkt->client = req_p1c;
    pkt->pkt_addr = req_p14;
    pkt->cmd = (void *)0x80000009;
    sd = libkernl_sif_find_server((unsigned int)req->cmd, rd);
    if (sd == 0) {
        pkt->server = 0;
        pkt->buff = 0;
        pkt->cbuff = 0;
    } else {
        pkt->server = sd;
        pkt->buff = sd->buff;
        pkt->cbuff = sd->cbuff;
    }
    isceSifSendCmd(0x80000008, pkt, 64, 0, 0, 0);
}

/*
 * Waits unless mode bit 0. cd->server stays 0 if the server is not registered yet (callers
 * retry). -1 no packet, -2 send failed, -3 no semaphore.
 */
/* 0x001f8b18 */
int sceSifBindRpc(RpcPkt *cd, unsigned int rpc_number, unsigned int mode)
{
    SemaParam sema_param;
    RpcPkt *pkt;

    cd->rid = 0;
    cd->server = 0;
    pkt = libkernl_sif_alloc_packet(&libkernl_sif_rpc_data);
    if (!pkt)
        return -1;
    cd->cur_pkt = pkt;
    cd->cur_pkt_id = pkt->pkt_id;
    pkt->cmd = (void *)rpc_number;
    pkt->pkt_addr = pkt;
    pkt->client = cd;
    if (!(mode & 1)) {
        sema_param.max_count = 1;
        sema_param.init_count = 0;
        if ((cd->sema = CreateSema(&sema_param)) < 0) {
            libkernl_sif_free_packet(pkt);
            return -3;
        }
        if (!sceSifSendCmd(0x80000009, pkt, 64, 0, 0, 0)) {
            libkernl_sif_free_packet(pkt);
            DeleteSema(cd->sema);
            return -2;
        }
        WaitSema(cd->sema);
        DeleteSema(cd->sema);
        return 0;
    } else {
        cd->sema = -1;
        if (!sceSifSendCmd(0x80000009, pkt, 64, 0, 0, 0)) {
            libkernl_sif_free_packet(pkt);
            return -2;
        }
        return 0;
    }
}

/* Queues the server with the request's parameters; wakes the queue thread if idle. */
/* 0x001f8c58 */
void libkernl_sif_call_cmd_handler(RpcCallReq *req, RpcData *rd)
{
    RpcServer *sd;
    RpcQueue *qd;

    sd = req->server;
    qd = sd->base;
    if (!qd->start)
        qd->start = sd;
    else
        qd->end->next = sd;
    *(int *)&qd->end = (int)sd;     /* steer(alias) */
    sd->pkt_addr = req->pkt_addr;
    sd->client = req->client;
    sd->fno = req->fno;
    sd->size = req->size;
    sd->receive = req->receive;
    sd->rsize = req->rsize;
    sd->rmode = req->rmode;
    sd->rid = req->rid;
    if (qd->thread_id >= 0 && !qd->active)
        iWakeupThread(qd->thread_id);
}

/*
 * Mode bit 1 skips the cache write-back; bit 0 returns at once and runs end_func(end_param) on
 * the answer, else waits. -1 no packet, -2 send failed, -3 no semaphore.
 */
/* 0x001f8ce8 */
int sceSifCallRpc(RpcPkt *cd, unsigned int fno, unsigned int mode, void *send, int ssize,
                 void *receive, int rsize, void (*end_func)(void *), void *end_param)
{
    SemaParam sema_param;
    RpcPkt *pkt;
    RpcPktW *client_w;
    RpcPktW *pkt_w;
    int id;

    pkt = libkernl_sif_alloc_packet(&libkernl_sif_rpc_data);
    if (!pkt)
        return -1;
    client_w = (RpcPktW *)cd;
    pkt_w = (RpcPktW *)pkt;
    /* packet id read before the client record update */
    id = pkt_w->pkt_id;
    client_w->cmd = (int)end_param;
    client_w->cur_pkt = (int)pkt;
    client_w->cur_pkt_id = id;
    client_w->client = (int)end_func;
    pkt_w->cmd = fno;
    pkt_w->server = ssize;
    pkt_w->rbuf = (int)receive;
    pkt_w->rsize = rsize;
    pkt_w->pkt_addr = (int)pkt;
    pkt_w->call_server = client_w->server;
    pkt_w->client = (int)cd;
    if (!(mode & 2)) {
        if (send == receive) {
            sceSifWriteBackDCache(send, ssize < rsize ? rsize : ssize);
        } else {
            if (ssize > 0)
                sceSifWriteBackDCache(send, ssize);
            if (rsize > 0)
                sceSifWriteBackDCache(receive, rsize);
        }
    }
    if (mode & 1) {
        if (!end_func)
            pkt->rmode = 0;
        else
            pkt->rmode = (void *)1;
        cd->sema = -1;
        if (sceSifSendCmd(0x8000000a, pkt, 64, send, cd->pkt_addr, ssize))
            return 0;
        libkernl_sif_free_packet(pkt);
        return -2;
    } else {
        sema_param.max_count = 1;
        sema_param.init_count = 0;
        if ((cd->sema = CreateSema(&sema_param)) < 0) {
            libkernl_sif_free_packet(pkt);
            return -3;
        }
        pkt->rmode = (void *)1;
        if (!sceSifSendCmd(0x8000000a, pkt, 64, send, cd->pkt_addr, ssize)) {
            DeleteSema(cd->sema);
            libkernl_sif_free_packet(pkt);
            return -2;
        }
        WaitSema(cd->sema);
        DeleteSema(cd->sema);
        return 0;
    }
}

/* 1 while cd's packet is in use and still carries cd's id. */
/* 0x001f8ed8 */
int sceSifCheckStatRpc(RpcPkt *cd)
{
    RpcPkt *pkt;

    pkt = (RpcPkt *)cd->cur_pkt;
    if (pkt == 0)
        goto no;
    if (cd->cur_pkt_id != pkt->pkt_id)
        goto no;
    if (pkt->rid & 1)
        goto yes;
no:
    return 0;
yes:
    return 1;
}

/* Empty queue for thread_id, appended to the queue list. */
/* 0x001f8f18 */
void sceSifSetRpcQueue(RpcQueue *qd, int thread_id)
{
    RpcData *rd;
    RpcQueue *qp;

    ((int (*)(void))DIntr)(); /* steer(cast) */
    rd = &libkernl_sif_rpc_data;
    qd->thread_id = thread_id;
    qd->active = 0;
    qd->link = 0;
    qd->start = 0;
    qd->end = 0;
    *(int *)&qd->next = (int)(0);
    if (!rd->queue) {
        rd->queue = qd;
    } else {
        qp = rd->queue;
        while (qp->next)
            qp = qp->next;
        qp->next = qd;
    }
    EIntr();
}

/* func handles calls with buff; cfunc and cbuff handle cancellation. */
/* 0x001f8fb0 */
void sceSifRegisterRpc(RpcServer *sd, unsigned int sid, void *func, void *buff, void *cfunc,
                  void *cbuff, RpcQueue *qd)
{
    RpcServer *srv;

    DIntr();
    ((union { RpcServer *p; } *)&sd->next)->p = 0; /* steer(alias) */
    ((union { RpcServer *p; } *)&sd->link)->p = 0;
    sd->rpc_number = sid;
    sd->func = func;
    sd->buff = buff;
    sd->cfunc = cfunc;
    sd->cbuff = cbuff;
    sd->base = qd;
    if (!qd->link) {
        qd->link = sd;
    } else {
        srv = qd->link;
        while (srv->link)
            srv = srv->link;
        srv->link = sd;
    }
    EIntr();
}

/* Returns the entry it was linked from, 0 if not found. */
/* 0x001f9080 */
static RpcServer *sceSifRemoveRpc(RpcServer *sd, RpcQueue *qd)
{
    RpcServer *srv;

    DIntr();
    srv = qd->link;
    if (srv == sd) {
        qd->link = srv->link;
    } else {
        for (; srv; srv = srv->link) {
            if (srv->link == sd) {
                srv->link = sd->link;
                break;
            }
        }
    }
    EIntr();
    return srv;
}

/* Returns the entry it was linked from, 0 if not found. */
/* 0x001f9118 */
static RpcQueue *sceSifRemoveRpcQueue(RpcQueue *qd)
{
    RpcQueue *qp;

    ((int (*)(void))DIntr)(); /* steer(cast) */
    qp = libkernl_sif_rpc_data.queue;
    if (qp == qd) {
        libkernl_sif_rpc_data.queue = qp->next;
    } else {
        for (; qp; qp = qp->next) {
            if (qp->next == qd) {
                qp->next = qd->next;
                break;
            }
        }
    }
    EIntr();
    return qp;
}

/* Marks the queue busy, or idle if none is left. */
/* 0x001f91a8 */
static RpcServer *sceSifGetNextRequest(RpcQueue *qd)
{
    RpcServer *sd;

    DIntr();
    sd = qd->start;
    if (!sd) {
        qd->active = 0;
    } else {
        qd->active = 1;
        qd->start = sd->next;
    }
    EIntr();
    return sd;
}

/*
 * Calls func(fno, buff, size), answers with a "request finished" command (retried), or with
 * plain DMA when no reply command was wanted (retried after a busy wait).
 */
/* 0x001f9200 */
static void sceSifExecRequest(RpcServer *sd)
{
    DmaDesc dma[2];
    void *rec;
    int size;
    RpcPkt *pkt;
    int n;
    int i;
    unsigned int ret;

    size = 0;
    rec = ((void *(*)(int, void *, int))sd->func)(sd->fno, sd->buff, sd->size);
    if (rec)
        size = sd->rsize;
    if (sd->size > 0)
        sceSifWriteBackDCache(sd->buff, sd->size);
    if (size > 0)
        sceSifWriteBackDCache(rec, size);
    DIntr();
    if (sd->rid & 4)
        pkt = libkernl_sif_get_rdata(&libkernl_sif_rpc_data, (unsigned int)sd->rid >> 16);
    else
        pkt = libkernl_sif_next_rdata(&libkernl_sif_rpc_data);
    ((int (*)(void))EIntr)(); /* steer(cast) */
    ((int *)pkt)[8] = 0x8000000a; /* answer to a call */
    pkt->client = *(RpcPkt **)&sd->client;
    if (sd->rmode) {
        while (!sceSifSendCmd(0x80000008, pkt, 64, rec, (void *)sd->receive, size))
            ;
    } else {
        pkt->pkt_id = 0;
        pkt->rid = 0;
        n = 0;
        if (size > 0) {
            n = 1;
            dma[0].data = rec;
            dma[0].addr = (void *)sd->receive;
            dma[0].size = size;
            dma[0].mode = 0;
        }
        dma[n].data = pkt;
        dma[n].addr = sd->pkt_addr;
        do { } while (0);
        dma[n].size = 64;
        dma[n].mode = 0;
        n++;
        do {
            ret = sceSifSetDma(dma, n);
            if (!ret) {
                i = 0x100000;
                while (i--)
                    ;
            }
        } while (!ret);
    }
}

/* Runs every pending request, then sleeps until woken. */
/* 0x001f93c8 */
void sceSifRpcLoop(RpcQueue *qd)
{
    RpcServer *sd;

    for (;;) {
        while ((sd = sceSifGetNextRequest(qd)) != 0)
            sceSifExecRequest(sd);
        SleepThread();
    }
}
