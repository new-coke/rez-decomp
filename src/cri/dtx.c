/*
 * IOP data exchange (EE end): SIF RPC client and server, eight exchange objects passing a work
 * area back and forth by DMA, one round at a time, via a fill and an answer callback.
 */

#include "types_cri.h"
#include "dtx.h"
#include "libc.h"
#include "cri_lock.h"
#include "kernel_syscalls.h"
#include "libkernl_cache.h"
#include "libkernl_sifrpc.h"

extern void (*dtx_recv_fn_tbl[64])(void *obj, int *buf, unsigned int n);
extern void *dtx_recv_obj_tbl[64];
extern int dtx_rpc_id;
extern volatile int dtx_rpc_thread_up;
extern char dtx_rpc_server_data[];
extern char dtx_rpc_buf[];

extern char dtx_rpc_client[];
extern int dtx_rpc_sbuf[];
extern int dtx_rpc_rbuf[];
extern int dtx_send_count;
extern int dtx_recv_count;
extern int dtx_exec_count;
extern int dtx_init_count;

/* IOP requests: 2 create (handle in buf), 3 destroy, 1024 + n -> dtx_recv_fn_tbl[n](buf, words). */
/* 0x001cd5a8 */
static int *dtx_rpc_server(unsigned int cmd, int *buf, unsigned int size)
{
    unsigned int n;

    sj_lock();
    switch (cmd) {
    default:
        n = cmd - 1024;
        if (n < 64 && dtx_recv_fn_tbl[n])
            dtx_recv_fn_tbl[n](dtx_recv_obj_tbl[n], buf, size / 4);
        break;
    case 2:
        buf[0] = (int)DTX_Create(buf[0], (char *)buf[1], (char *)buf[2], buf[3]);
        break;
    case 3:
        dtx_destroy((DtxObj *)buf[0]);
        break;
    case 0:
    case 1:
        break;
    }
    sj_unlock();
    return buf;
}

/* Registers dtx_rpc_server under dtx_rpc_id; serves forever. */
/* 0x001cd678 */
static int dtx_rpc_thread(void)
{
    int qd[8];
    int spare_qd[8];

    sceSifSetRpcQueue(qd, GetThreadId());
    sceSifSetRpcQueue(spare_qd, GetThreadId());
    sceSifRegisterRpc(dtx_rpc_server_data, dtx_rpc_id, dtx_rpc_server, dtx_rpc_buf, 0, 0, qd);
    dtx_rpc_thread_up = 1;
    sceSifRpcLoop(qd);
    return 0;
}

/* IOP end of exchange id; returns its handle. */
/* 0x001cd700 */
int dtx_rpc_create(unsigned int id, char *eewk, char *iopwk, int wklen)
{
    int *sbuf;

    sbuf = dtx_rpc_sbuf;
    sbuf[0] = id;
    sbuf[1] = (int)eewk;
    sbuf[2] = (int)iopwk;
    sbuf[3] = wklen;
    sceSifCallRpc(dtx_rpc_client, 2, 0, sbuf, 16, dtx_rpc_rbuf, 4, 0, 0);
    return dtx_rpc_rbuf[0];
}

/* 0x001cd768 */
static void dtx_rpc_destroy(int svr)
{
    dtx_rpc_sbuf[0] = svr;
    sceSifCallRpc(dtx_rpc_client, 3, 0, dtx_rpc_sbuf, 4, dtx_rpc_rbuf, 0, 0, 0);
}

/* Default answer callback: counts calls. */
/* 0x001cd7b8 */
static void dtx_default_send_cb(void)
{
    dtx_send_count++;
}

/* Default fill callback: 0xaa bytes with "Hello from EE (n)" at the start. */
/* 0x001cd7c8 */
static void dtx_default_recv_cb(void *obj, char *buf, int len)
{
    int i;

    for (i = 0; i < len; i++)
        buf[i] = 0xaa;
    sprintf(buf, "Hello from EE (%d)", dtx_recv_count);
    dtx_recv_count++;
}

/* 0 for id >= 8. */
/* 0x001cd830 */
static DtxObj *dtx_get_obj(unsigned int id)
{
    if (id >= 8)
        return 0;
    return &dtx_obj_tbl[id];
}

/* 0x001cd858 */
static void dtx_clear_cbs(DtxObj *dtx)
{
    dtx->fn20 = 0;
    dtx->obj24 = 0;
    dtx->fn28 = 0;
    dtx->obj2c = 0;
}

/*
 * EE work area 64-byte aligned, multiple of 64; IOP work area 32-byte aligned. Last 64 bytes hold
 * the round counters, read through an uncached pointer.
 */
/* 0x001cd870 */
DtxObj *DTX_Create(unsigned int id, char *eewk, char *iopwk, int wklen)
{
    DtxObj *dtx;

    if ((int)eewk & 0x3f) {
        printf("eewk is not alignment 64 byte\n");
        return 0;
    }
    if ((int)iopwk & 0x1f) {
        printf("iopwk is not alignment 32 byte\n");
        return 0;
    }
    if (wklen & 0x3f) {
        printf("wklen=%d is not 64*N\n", wklen);
        return 0;
    }
    if (id >= 8) {
        printf("illeagal ID (%d) \n", id);
        return 0;
    }
    dtx = &dtx_obj_tbl[id];
    if (dtx->used)
        return 0;
    dtx->svr = dtx_rpc_create(id, eewk, iopwk, wklen);
    if (dtx->svr == 0) {
        printf("DTX_Create: can't create DTX of server\n");
        return 0;
    }
    dtx->ucwk = (int *)((int)(eewk + (wklen - 64)) | 0x20000000);
    dtx->wklen = wklen;
    dtx->iopwk = iopwk;
    dtx->seq = 0;
    dtx->len = wklen - 64;
    dtx->eewk = eewk;
    dtx->busy = 0;
    memset(eewk, 0, wklen - 64);
    SyncDCache(dtx->eewk, dtx->eewk + dtx->len + 63);
    InvalidDCache(dtx->eewk, dtx->eewk + dtx->len + 63);
    dtx->fn20 = dtx_default_send_cb;
    dtx->fn28 = dtx_default_recv_cb;
    dtx->obj24 = 0;
    dtx->obj2c = 0;
    dtx->send_on = 1;
    dtx->used = 1;
    return dtx;
}

/* Waits for the round in progress first. */
/* 0x001cda08 */
void dtx_destroy(DtxObj *dtx)
{
    dtx->send_on = 0;
    while (dtx->busy == 1)
        dtx_exec_server();
    dtx_rpc_destroy(dtx->svr);
    memset(dtx, 0, sizeof(DtxObj));
}

/* Gets the IOP's answer in the work area. */
/* 0x001cda70 */
void dtx_set_send_cb(DtxObj *dtx, void (*fn)(), void *obj)
{
    dtx->fn20 = fn;
    dtx->obj24 = obj;
}

/* Fills the work area before sending. */
/* 0x001cda80 */
void dtx_set_recv_cb(DtxObj *dtx, void (*fn)(), void *obj)
{
    dtx->fn28 = fn;
    dtx->obj2c = obj;
}

/* IOP counter past ours: answer callback. Idle with sending on: fill and DMA the next round. */
/* 0x001cda90 */
static void dtx_exec_obj(DtxObj *dtx)
{
    dtx_exec_count++;
    if (dtx->busy == 1 && dtx->seq < dtx->ucwk[15]) {
        InvalidDCache(dtx->eewk, dtx->eewk + (dtx->len - 1));
        dtx->fn20(dtx->obj24, dtx->eewk, dtx->len);
        dtx->seq = dtx->ucwk[15];
        dtx->busy = 0;
    }
    if (dtx->busy == 0 && dtx->send_on == 1 && dtx_send_sw == 1) {
        dtx->fn28(dtx->obj2c, dtx->eewk, dtx->len);
        dtx->seq++;
        dtx->ucwk[15] = dtx->seq;
        SyncDCache(dtx->eewk, dtx->eewk + dtx->len - 1);
        InvalidDCache(dtx->eewk, dtx->eewk + dtx->len + 63);
        dtx->dma.mode = 0;
        dtx->dma.data = (int)dtx->eewk & 0x0fffffff;
        dtx->dma.addr = dtx->iopwk;
        dtx->dma.size = dtx->wklen;
        dtx->dmaid = sceSifSetDma(&dtx->dma, 1);
        dtx->busy = 1;
    }
}

/* First call binds the RPC client, retrying until the IOP answers (hangs on failure); counts. */
/* steer(cast) */
/* 0x001cdbe8 */
void DTX_Init(void)
{
    int i;

    if (dtx_init_count == 0) {
        for (;;) {
            if (sceSifBindRpc(&(*(struct { int unk00[9] ; void *serve ; } *)&dtx_rpc_client), dtx_rpc_id, 0) < 0) {
                printf("E0092101: DTX_Init bind errr\n");
                for (;;)
                    ;
            }
            if ((*(struct { int unk00[9] ; void *serve ; } *)&dtx_rpc_client).serve != 0)
                break;
            i = 0x10000;
            while (i--)
                ;
        }
    }
    dtx_init_count++;
}

/* Last user: destroys every exchange object. */
/* 0x001cdc90 */
void DTX_Finish(void)
{
    int i;
    DtxObj *tab;

    if (--dtx_init_count == 0) {
        tab = dtx_obj_tbl;
        for (i = 0; i < 8; i++) {
            if (tab[i].used == 1)
                dtx_destroy(&tab[i]);
        }
        tab = 0;
    }
}

/* IOP function fno: nin words in, nout words to out; returns the first. */
/* 0x001cdd08 */
int dtx_call_iop(int fno, int *in, int nin, int *out, int nout)
{
    int i;

    for (i = 0; i < nin; i++)
        dtx_rpc_sbuf[i] = in[i];
    sceSifCallRpc(dtx_rpc_client, fno + 1024, 0, dtx_rpc_sbuf, nin * 4, dtx_rpc_rbuf, nout * 4, 0, 0);
    for (i = 0; i < nout; i++)
        out[i] = dtx_rpc_rbuf[i];
    return dtx_rpc_rbuf[0];
}

/* 0x001cde10 */
void dtx_exec_server(void)
{
    int i;

    sj_lock();
    for (i = 0; i < 8; i++) {
        if (dtx_obj_tbl[i].used == 1)
            dtx_exec_obj(&dtx_obj_tbl[i]);
    }
    sj_unlock();
}
