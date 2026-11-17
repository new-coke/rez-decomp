/* RPC client for the IOP peripheral (vibration) service; a semaphore guards the reply buffer. */

#include "libc.h"
#include "ul_iop_rpc.h"
#include "kernel_syscalls.h"
#include "libkernl_glue.h"
#include "libkernl_sifrpc.h"

typedef struct IopRpcSemaParam {
    int count;
    int max_count;
    int init_count;
    int wait_threads;
    int attr;
    int option;
} IopRpcSemaParam;

typedef struct IopRpcClient {
    char pad_00[36];
    int server;
} IopRpcClient;

typedef struct IopRpcWord { char b[4]; } IopRpcWord;

typedef struct IopRpcPair {
    int id;
    IopRpcWord act;
} IopRpcPair;

extern int iop_rpc_sema;
extern void *iop_rpc_recv_dst;
extern int iop_rpc_recv_size;
extern char iop_rpc_recv_data[];
extern char iop_rpc_client[];
extern char iop_rpc_send_buf[];
extern int iop_rpc_recv_buf[];
extern int CreateSema(IopRpcSemaParam *);

/* Deletes the library semaphore. */
/* 0x00180d18 */
static void iop_rpc_lock(void)
{
    DeleteSema(iop_rpc_sema);
}

/*
 * Async completion (interrupt context): copies the reply if a buffer was given, signals the
 * semaphore.
 */
/* 0x00180d20 */
static void iop_rpc_release(void)
{
    if (iop_rpc_recv_dst)
        memcpy(iop_rpc_recv_dst, iop_rpc_recv_data, iop_rpc_recv_size);
    iSignalSema(iop_rpc_sema);
}

/*
 * cmd bit 0x8000: synchronous, returns the result word, reply in recv. Else takes the semaphore
 * and returns at once; iop_rpc_release delivers. -2 if the call cannot start.
 */
/* 0x00180d50 */
static int iop_rpc_call(int cmd, void *send, int size, void *recv, int recv_size)
{
    int st[8];
    int ret;

    iop_rpc_recv_size = recv_size;
    iop_rpc_recv_dst = recv;
    if (send)
        memcpy(iop_rpc_send_buf, send, size);
    if ((cmd & 0x8000) == 0) {
        ReferSemaStatus(iop_rpc_sema, st);
        for (; st[0] > 0; st[0]--)
            WaitSema(iop_rpc_sema);
        ret = sceSifCallRpc(iop_rpc_client, cmd, 1, iop_rpc_send_buf, size, iop_rpc_recv_buf, recv_size + 4,
                         iop_rpc_release, 0);
        if (ret < 0)
            return -2;
    } else {
        ret = sceSifCallRpc(iop_rpc_client, cmd, 0, iop_rpc_send_buf, size, iop_rpc_recv_buf, recv_size + 4, 0, 0);
        if (ret < 0)
            return -2;
        ret = iop_rpc_recv_buf[0];
        if (recv)
            memcpy(recv, &iop_rpc_recv_buf[1], recv_size);
    }
    return ret;
}

/* Binds, waiting a vblank between attempts, then sends the start command with mode. */
/* 0x00180ec0 */
void iop_rpc_init(int mode)
{
    IopRpcSemaParam sema;
    int args[4];

    sema.max_count = 1;
    sema.init_count = 1;
    iop_rpc_sema = CreateSema(&sema);
    while (sceSifBindRpc(&(*(IopRpcClient *)&iop_rpc_client), 0x10001, 0) >= 0 && (*(IopRpcClient *)&iop_rpc_client).server == 0) /* steer(cast) */
        crt_wait_vblank();
    args[0] = mode;
    iop_rpc_call(0x8001, args, 4, 0, 0);
}

/* Four actuator bytes. */
/* 0x00180f70 */
int iop_rpc_set_act(int pad, IopRpcWord *val)
{
    IopRpcPair req;

    req.id = pad;
    req.act = *val;
    return iop_rpc_call(5, &req, 8, 0, 0);
}

/* 136 bytes. */
/* 0x00180fb8 */
int iop_rpc_get_info(int recv)
{
    return iop_rpc_call(3, 0, 0, recv, 136);
}

/* 0x00180fe8 */
int iop_rpc_send_word(int val, int recv)
{
    int word = val;

    return iop_rpc_call(4, &word, 4, recv, 7);
}

/* Op 0: waits, result word in *out; op 1: whether a call is pending; else -11. */
/* 0x00181018 */
int iop_rpc_status(int op, int *out)
{
    if (op != 0) {
        if (op != 1)
            return -11;
        return sceSifCheckStatRpc(iop_rpc_client);
    }
    WaitSema(iop_rpc_sema);
    *out = iop_rpc_recv_buf[0];
    SignalSema(iop_rpc_sema);
    return 0;
}

/* Stop command, then deletes the semaphore. */
/* 0x00181080 */
static int iop_rpc_shutdown(void)
{
    int ret = iop_rpc_call(0x8002, 0, 0, 0, 0);

    iop_rpc_lock();
    return ret;
}
