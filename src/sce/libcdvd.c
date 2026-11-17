/*
 * CD/DVD library, EE side: remote calls to the IOP driver. Async results come back through a SIF
 * callback and an optional callback thread.
 */

#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libcdvd.h"
#include "kernel_syscalls.h"
#include "libdma.h"
#include "libkernl_intr.h"
#include "libkernl_sifcmd.h"
#include "libkernl_sifrpc.h"
#include "libkernl_tty.h"
#include "cri.h"

static int libcdvd_register_handler(void);

typedef struct {
    int status;
    void (*entry)(void);
    void *stack;
    int stackSize;
    void *gpReg;
    int initPriority;
} ThreadParamA;

typedef struct {
    int n0;
    int n1;
    unsigned char *dst0;
    unsigned char *dst1;
    unsigned char data0[64];
    unsigned char data1[64];
} CdRecvPkt;

typedef struct CdFile_a {
    unsigned int lsn;
    unsigned int size;
    char name[16];
    unsigned char date[8];
    unsigned int flag;
} CdFile_a;

typedef struct CdSearch_a {
    CdFile_a file;
    char name[256];
    void *addr;
} CdSearch_a;

typedef struct {
    unsigned char trycount;
    unsigned char spindlctrl;
    unsigned char datapattern;
    unsigned char pad;
} CdRMode;

typedef struct {
    unsigned int lsn;
    unsigned int sectors;
    void *buf;
    unsigned char mode[4];
    void *res0;
    void *res1;
} CdReadReq;

extern int libcdvd_user_cb;
extern volatile int libcdvd_cur_cmd;
extern volatile int libcdvd_cb_cmd;
extern volatile int libcdvd_ncmd_busy;
extern volatile int libcdvd_ncmd_sema;
extern volatile int libcdvd_cb_sema;
extern int libcdvd_cb_thread_up;
extern int libcdvd_debug;
extern volatile int libcdvd_cb_busy;
extern int libcdvd_thread_id;
extern char libcdvd_thread_status[];
extern ThreadParamA libcdvd_cb_thread_param;
extern char _gp[];

extern volatile int libcdvd_scmd_sema;
extern int CreateSema(SemaParamB *p);
extern int libcdvd_poff_bound;
extern int libcdvd_poff_fn;
extern int libcdvd_poff_arg;
extern int libcdvd_poff_busy;
extern int libcdvd_ncmd_cur;
extern int libcdvd_search_bound;
extern char libcdvd_search_client[];
extern CdSearch_a libcdvd_search_req;
extern int libcdvd_search_reply;

extern int libcdvd_ncmd_bound;
extern char libcdvd_ncmd_client[];
extern int libcdvd_ncmd_reply;
extern char libcdvd_scmd_client[];
extern int libcdvd_scmd_cur;
extern int libcdvd_scmd_bound;
extern int libcdvd_read_flags;
extern int libcdvd_ready_bound;
extern int libcdvd_bound_unk;
extern int libcdvd_init_count;
extern char libcdvd_init_client[];
extern int libcdvd_init_arg;
extern char libcdvd_ready_client[];
extern int libcdvd_ready_arg;
extern int libcdvd_init_reply;
extern int libcdvd_scmd_arg;
extern CdReadReq libcdvd_read_req;
extern char libcdvd_read_res[];
extern int libcdvd_read_res2;

/* Returns the previous callback; 0 and no change while a command runs. */
/* steer(cast) */
/* 0x001e1fa0 */
static int sceCdCallback(int func)
{
    int old;

    if (sceCdSync(1))
        return 0;
    ((int (*)(void))DIntr)();
    old = libcdvd_user_cb;
    libcdvd_user_cb = func;
    EIntr();
    return old;
}

/*
 * Releases the N-command lock; with a callback hands the command to the callback thread (which
 * clears busy), else clears busy itself. Command 11 only clears the state.
 */
/* 0x001e1ff8 */
static void libcdvd_notify(int *cmd)
{
    libcdvd_cur_cmd = *cmd;
    libcdvd_cb_cmd = libcdvd_cur_cmd;
    if (libcdvd_cur_cmd == 11) {
        libcdvd_cur_cmd = 0;
        libcdvd_ncmd_busy = 0;
        return;
    }
    iSignalSema(libcdvd_ncmd_sema);
    if (libcdvd_cb_thread_up != 0 && libcdvd_user_cb != 0) {
        iSignalSema(libcdvd_cb_sema);
    } else {
        libcdvd_ncmd_busy = 0;
    }
    libcdvd_cur_cmd = 0;
}

/* Calls the user callback per finished command, then clears busy. Command -1 ends the thread. */
/* 0x001e2098 */
static void libcdvd_cb_thread(void)
{
    for (;;) {
        WaitSema(libcdvd_cb_sema);
        if (libcdvd_cur_cmd == -1) {
            libcdvd_ncmd_busy = 0;
            libcdvd_cur_cmd = 0;
            libcdvd_cb_thread_up = 0;
            libcdvd_cb_busy = 0;
            ExitDeleteThread();
        }
        if (libcdvd_debug > 0)
            libkernl_printf_line("sceCdCbfunc= %d sceCdCbfunc_num= %d\n", libcdvd_user_cb, libcdvd_cb_cmd);
        if (libcdvd_user_cb != 0 && libcdvd_cb_cmd != 0)
            ((void (*)(int))libcdvd_user_cb)(libcdvd_cb_cmd);
        libcdvd_ncmd_busy = 0;
    }
}

/* 1 when started; if already running only changes the priority and returns 0. */
/* 0x001e2158 */
static int sceCdInitEeCB(int prio, void *stack, int size)
{
    ThreadParamA *tp;
    int ret;

    ret = 1;
    if (libcdvd_cb_thread_up == 0) {
        libcdvd_thread_id = GetThreadId();
        ReferThreadStatus(libcdvd_thread_id, libcdvd_thread_status);
        tp = &libcdvd_cb_thread_param;
        tp->stackSize = size;
        tp->gpReg = _gp;
        tp->entry = libcdvd_cb_thread;
        tp->stack = stack;
        tp->initPriority = prio;
        libcdvd_cb_thread_up = CreateThread(tp);
        _StartThread(libcdvd_cb_thread_up, 0);
    } else {
        ChangeThreadPriority(libcdvd_cb_thread_up, prio);
        ret = 0;
    }
    return ret;
}

/* Copies the unaligned start and end blocks the driver could not DMA, then reports done. */
/* 0x001e2230 */
static void libcdvd_sif_handler(unsigned int addr)
{
    CdRecvPkt *pkt;
    unsigned char *dst;
    int i;

    pkt = (CdRecvPkt *)(addr | 0x20000000);
    if (pkt->n0 > 0) {
        dst = pkt->dst0;
        for (i = 0; i < pkt->n0; i++)
            dst[i] = pkt->data0[i];
    }
    if (pkt->n1 > 0) {
        dst = pkt->dst1;
        for (i = 0; i < pkt->n1; i++)
            dst[i] = pkt->data1[i];
    }
    libcdvd_notify(&libcdvd_cur_cmd);
}

/* If either command lock is missing: N and S locks (free), callback wake-up semaphore (empty). */
/* 0x001e22d0 */
static void libcdvd_create_semas(void)
{
    SemaParamB sema_param;

    if (libcdvd_ncmd_sema == -1 || libcdvd_scmd_sema == -1) {
        sema_param.option = 0;
        sema_param.init = 1;
        sema_param.max = 1;
        libcdvd_ncmd_sema = CreateSema(&sema_param);
        libcdvd_scmd_sema = CreateSema(&sema_param);
        sema_param.init = 0;
        libcdvd_cb_sema = CreateSema(&sema_param);
        libcdvd_ncmd_busy = 0;
    }
}

/* Stops the callback thread, deletes the semaphores, removes the power-off handler. */
/* 0x001e2368 */
static void libcdvd_shutdown(void)
{
    if (libcdvd_cb_thread_up) {
        libcdvd_cur_cmd = -1;
        do { } while (0);
        SignalSema((*(int *)&libcdvd_cb_sema)); /* steer(cast) */
    }
    DeleteSema((*(int *)&libcdvd_ncmd_sema)); /* steer(cast) */
    DeleteSema((*(int *)&libcdvd_scmd_sema)); /* steer(cast) */
    DeleteSema((*(int *)&libcdvd_cb_sema)); /* steer(cast) */
    DIntr();
    sceSifRemoveCmdHandler(0x80000012);
    EIntr();
}

/* Called with cb_arg when the IOP reports power-off. Returns the previous handler. */
/* steer(cast) */
/* 0x001e23e8 */
static int sceCdPOffCallback(int func, int cb_arg)
{
    int old;

    if (libcdvd_poff_bound < 0)
        libcdvd_register_handler();
    ((int (*)(void))DIntr)();
    old = libcdvd_poff_fn;
    libcdvd_poff_arg = cb_arg;
    libcdvd_poff_fn = func;
    EIntr();
    return old;
}

/* Runs the user's handler unless it is being changed. */
/* 0x001e2458 */
static void libcdvd_user_handler(void)
{
    if ((*(void (*(*))(int))&libcdvd_poff_fn) != 0 && libcdvd_poff_busy == 0) /* steer(cast) */
        (*(void (*(*))(int))&libcdvd_poff_fn)(libcdvd_poff_arg); /* steer(cast) */
}

/* For the IOP's power-off command; returns 1. */
/* 0x001e2498 */
static int libcdvd_register_handler(void)
{
    libcdvd_poff_busy = 1;
    DIntr();
    sceSifAddCmdHandler(0x80000012, libcdvd_user_handler, 0);
    ((int (*)(void))EIntr)(); /* steer(cast) */
    libcdvd_poff_busy = 0;
    libcdvd_poff_bound = 1;
    return 1;
}

/* Fills position, size and name. Driver result (1 found), or 0 if busy or the call fails. */
/* steer(cast) */
/* 0x001e2500 */
int sceCdSearchFile(CdFile_a *file, const char *name)
{
    int i;
    int ret;

    libcdvd_create_semas();
    if (libcdvd_ncmd_sema != PollSema(libcdvd_ncmd_sema))
        return 0;
    libcdvd_ncmd_cur = 1;
    ReferThreadStatus(libcdvd_thread_id, libcdvd_thread_status);
    if (sceCdSync(1)) {
        SignalSema(libcdvd_ncmd_sema);
        return 0;
    }
    ((void (*)(int))sceSifInitRpc)(0);
    if (libcdvd_search_bound < 0) {
        while (1) {
            if (sceSifBindRpc(libcdvd_search_client, 0x80000597, 0) < 0) {
                if (libcdvd_debug > 0)
                    ((void (*)(const char *, ...))libkernl_printf_line)("Libcdvd bind err CdSearchFile\n"); /* steer(cast) */
                {
                    int n = 0x100000;
                    while (n--)
                        ;
                }
                continue;
            }
            if (((SifClientA *)libcdvd_search_client)->server != 0) {
                libcdvd_search_bound = 0;
                break;
            }
            {
                int n = 0x100000;
                while (n--)
                    ;
            }
        }
    }
    for (i = 0; i < 256; i++) {
        if ((libcdvd_search_req.name[i] = name[i]) == 0)
            break;
    }
    if (i == 256)
        libcdvd_search_req.name[255] = 0;
    libcdvd_search_req.addr = &libcdvd_search_req;
    if (libcdvd_debug > 0)
        ((void (*)(const char *, ...))libkernl_printf_line)("ee call cmd search %s\n", libcdvd_search_req.name); /* steer(cast) */
    sceSifWriteBackDCache(&libcdvd_search_req, sizeof(CdSearch_a));
    if (sceSifCallRpc(libcdvd_search_client, 0, 0, &libcdvd_search_req, sizeof(CdSearch_a), &libcdvd_search_reply, 4, 0, 0) < 0) {
        SignalSema(libcdvd_ncmd_sema);
        return 0;
    }
    memcpy(file, (void *)((unsigned int)&libcdvd_search_req | 0x20000000), sizeof(CdFile_a));
    if (libcdvd_debug > 0) {
        ((void (*)(const char *, ...))libkernl_printf_line)("search name %s\n", file->name); /* steer(cast) */
        if (libcdvd_debug > 0) {
            ((void (*)(const char *, ...))libkernl_printf_line)("search size %d\n", file->size); /* steer(cast) */
            if (libcdvd_debug > 0)
                ((void (*)(const char *, ...))libkernl_printf_line)("search loc lbn %d\n", file->lsn); /* steer(cast) */
        }
    }
    ret = *(int *)((unsigned int)&libcdvd_search_reply | 0x20000000);
    SignalSema(libcdvd_ncmd_sema);
    return ret;
}

/*
 * N commands move the pickup (reads). Fails if another holds the lock or is running; binds the
 * client on first use. 1 with the lock held, else 0.
 */
/* steer(cast) */
/* 0x001e2800 */
static int libcdvd_ncmd_lock(int mode)
{
    int i;

    libcdvd_create_semas();
    if (libcdvd_ncmd_sema != PollSema(libcdvd_ncmd_sema)) {
        if (libcdvd_debug > 0)
            libkernl_printf_line("Ncmd fail sema cur_cmd:%d keep_cmd:%d\n", mode, libcdvd_ncmd_cur);
        return 0;
    }
    libcdvd_ncmd_cur = mode;
    ReferThreadStatus(libcdvd_thread_id, libcdvd_thread_status);
    if (sceCdSync(1)) {
        SignalSema(libcdvd_ncmd_sema);
        return 0;
    }
    ((void (*)(int))sceSifInitRpc)(0);
    if (libcdvd_ncmd_bound < 0) {
        while (1) {
            if (sceSifBindRpc(libcdvd_ncmd_client, 0x80000595, 0) < 0) {
                if (libcdvd_debug > 0)
                    libkernl_printf_line("Libcdvd bind err N CMD\n");
                i = 0x100000;
                while (i--)
                    ;
                continue;
            }
            if (((SifClientA *)libcdvd_ncmd_client)->server != 0)
                break;
            i = 0x100000;
            while (i--)
                ;
        }
        libcdvd_ncmd_bound = 0;
    }
    return 1;
}

/* Disc type; sceCdRead refuses on 6. 0 on failure. */
/* 0x001e2970 */
static int libcdvd_ncmd14(void)
{
    int ret;

    if (libcdvd_ncmd_lock(2) == 0)
        return 0;
    if (sceSifCallRpc(libcdvd_ncmd_client, 14, 0, 0, 0, &libcdvd_ncmd_reply, 4, 0, 0) < 0) {
        SignalSema((*(int *)&libcdvd_ncmd_sema)); /* steer(cast) */
        return 0;
    }
    ret = *(int *)((unsigned int)&libcdvd_ncmd_reply | 0x20000000);
    SignalSema((*(int *)&libcdvd_ncmd_sema)); /* steer(cast) */
    return ret;
}

/* Mode 0 waits for no N command (sleeping between checks), returns 0; else 1 while one runs. */
/* 0x001e2a08 */
int sceCdSync(int mode)
{
    if (mode == 0) {
        if (libcdvd_debug > 0)
            libkernl_printf_line("N cmd wait\n");
        while (libcdvd_ncmd_busy != 0 || sceSifCheckStatRpc(libcdvd_ncmd_client) != 0)
            libdma_wait_int(60);
        return 0;
    }
    if (libcdvd_ncmd_busy != 0 || sceSifCheckStatRpc(libcdvd_ncmd_client) != 0)
        return 1;
    return 0;
}

/* For S commands (status/control, no pickup move). */
/* 0x001e2aa8 */
static int sceCdSyncS(int mode)
{
    if (mode == 0) {
        if (libcdvd_debug > 0)
            libkernl_printf_line("S cmd wait\n");
        while (sceSifCheckStatRpc(libcdvd_scmd_client) != 0)
            libdma_wait_int(60);
        return 0;
    }
    return sceSifCheckStatRpc(libcdvd_scmd_client);
}

/* Fails if another S command holds it or runs; binds on first use. 1 with the lock held, else 0. */
/* steer(cast) */
/* 0x001e2b18 */
static int libcdvd_scmd_lock(int mode)
{
    int i;

    libcdvd_create_semas();
    if (libcdvd_scmd_sema != PollSema(libcdvd_scmd_sema)) {
        if (libcdvd_debug > 0)
            libkernl_printf_line("Scmd fail sema cur_cmd:%d keep_cmd:%d\n", mode, libcdvd_scmd_cur);
        return 0;
    }
    libcdvd_scmd_cur = mode;
    ReferThreadStatus(libcdvd_thread_id, libcdvd_thread_status);
    if (sceCdSyncS(1)) {
        SignalSema(libcdvd_scmd_sema);
        return 0;
    }
    ((void (*)(int))sceSifInitRpc)(0);
    if (libcdvd_scmd_bound < 0) {
        while (1) {
            if (sceSifBindRpc(libcdvd_scmd_client, 0x80000593, 0) < 0) {
                if (libcdvd_debug > 0)
                    libkernl_printf_line("Libcdvd bind err S cmd\n");
                i = 0x100000;
                while (i--)
                    ;
                continue;
            }
            if (((SifClientA *)libcdvd_scmd_client)->server != 0)
                break;
            i = 0x100000;
            while (i--)
                ;
        }
        libcdvd_scmd_bound = 0;
    }
    return 1;
}

/*
 * Mode 5 shuts down. Forgets client bindings, binds the init server. 1, 2 when a driver size is
 * below 512, 0 on failure. Driver status 254 turns on trace output.
 */
/* steer(cast) */
/* 0x001e2c88 */
int sceCdInit(int mode)
{
    int i;
    int rpc_ret;
    int ret;
    int st, reply1, reply2;

    if (sceCdSyncS(1))
        return 0;
    ((void (*)(int))sceSifInitRpc)(0);
    libcdvd_thread_id = GetThreadId();
    (*(volatile int *)&libcdvd_poff_busy) = 1; /* steer(cast) */
    libcdvd_search_bound = -1;
    libcdvd_ncmd_bound = -1;
    (*(volatile int *)&libcdvd_scmd_bound) = -1; /* steer(cast) */
    libcdvd_ready_bound = -1;
    libcdvd_bound_unk = -1;
    libcdvd_poff_bound = -1;
    libcdvd_read_flags = 0;
    libcdvd_init_count++;
    while (1) {
        if ((rpc_ret = sceSifBindRpc(libcdvd_init_client, 0x80000592, 0)) < 0) {
            if (libcdvd_debug > 0)
                libkernl_printf_line("Libcdvd bind err %d CD_Init %d\n", rpc_ret, libcdvd_init_count);
            i = 0x100000;
            while (i--)
                ;
            continue;
        }
        if (((SifClientA *)libcdvd_init_client)->server != 0) {
            libcdvd_bound_unk = 0;
            break;
        }
        i = 0x100000;
        while (i--)
            ;
    }
    libcdvd_init_arg = mode;
    sceSifWriteBackDCache(&libcdvd_init_arg, 4);
    if (sceSifCallRpc(libcdvd_init_client, 0, 0, &libcdvd_init_arg, 4, (*(int (*)[4])&libcdvd_init_reply), 16, 0, 0) < 0) { /* steer(cast) */
        (*(volatile int *)&libcdvd_poff_busy) = 0; /* steer(cast) */
        return 0;
    }
    reply1 = *(int *)((unsigned int)&(*(int (*)[4])&libcdvd_init_reply)[1] | 0x20000000); /* steer(cast) */
    reply2 = *(int *)((unsigned int)&(*(int (*)[4])&libcdvd_init_reply)[2] | 0x20000000); /* steer(cast) */
    st = *(int *)((unsigned int)&(*(int (*)[4])&libcdvd_init_reply)[3] | 0x20000000); /* steer(cast) */
    ret = 1;
    if (st != 255) {
        if (st == 254)
            libcdvd_debug = 1;
        else if (reply1 / 256 < 2 || reply2 / 256 < 2)
            ret = 2;
    }
    (*(volatile int *)&libcdvd_poff_busy) = 0; /* steer(cast) */
    switch (mode) {
    case 5:
        if (libcdvd_debug > 0)
            libkernl_printf_line("Libcdvd Exit\n");
        libcdvd_shutdown();
        libcdvd_ncmd_sema = -1;
        libcdvd_scmd_sema = -1;
        libcdvd_cb_sema = -1;
        break;
    case 0:
    case 1:
    default:
        libcdvd_create_semas();
        libcdvd_register_handler();
        break;
    }
    return ret;
}

/* Mode 0 waits, 1 checks. Busy or failed: 6 (not ready), or -1 for mode 8. */
/* steer(cast) */
/* 0x001e2f68 */
int sceCdDiskReady(int mode)
{
    int i;
    int ret;

    if (libcdvd_debug > 0)
        libkernl_printf_line("DiskReady 0\n");
    libcdvd_create_semas();
    if (libcdvd_scmd_sema != PollSema(libcdvd_scmd_sema))
        return 6;
    if (sceCdSyncS(1)) {
        SignalSema(libcdvd_scmd_sema);
        if (mode == 8)
            return -1;
        return 6;
    }
    ((void (*)(int))sceSifInitRpc)(0);
    if (libcdvd_ready_bound < 0) {
        while (1) {
            if (sceSifBindRpc(libcdvd_ready_client, 0x8000059a, 0) < 0) {
                if (libcdvd_debug > 0)
                    libkernl_printf_line("Libcdvd bind err CdDiskReady\n");
                i = 0x100000;
                while (i--)
                    ;
                continue;
            }
            if (((SifClientA *)libcdvd_ready_client)->server != 0) {
                libcdvd_ready_bound = 0;
                break;
            }
            i = 0x100000;
            while (i--)
                ;
        }
    }
    libcdvd_ready_arg = mode;
    sceSifWriteBackDCache(&libcdvd_ready_arg, 4);
    if (sceSifCallRpc(libcdvd_ready_client, 0, 0, &libcdvd_ready_arg, 4, &libcdvd_init_reply, 4, 0, 0) < 0) {
        SignalSema(libcdvd_scmd_sema);
        if (mode == 8)
            return -1;
        return 6;
    }
    if (libcdvd_debug > 0)
        libkernl_printf_line("DiskReady ended\n");
    ret = *(int *)((unsigned int)&libcdvd_init_reply | 0x20000000);
    SignalSema(libcdvd_scmd_sema);
    return ret;
}

/* 0 on failure. */
/* 0x001e3160 */
int libcdvd_scmd34(int param)
{
    int *argp;
    int ret;

    argp = &libcdvd_scmd_arg;
    if (libcdvd_scmd_lock(34) == 0)
        return 0;
    libcdvd_scmd_arg = param;
    sceSifWriteBackDCache(argp, 4);
    if (sceSifCallRpc(libcdvd_scmd_client, 34, 0, argp, 4, &libcdvd_init_reply, 4, 0, 0) < 0) {
        SignalSema((*(int *)&libcdvd_scmd_sema)); /* steer(cast) */
        return 0;
    }
    ret = *(int *)((unsigned int)&libcdvd_init_reply | 0x20000000);
    SignalSema((*(int *)&libcdvd_scmd_sema)); /* steer(cast) */
    return ret;
}

/* Sector sizes 2048, 2328 or 2340. Runs in the background (sceCdSync). 1 if started. */
/* 0x001e3228 */
int sceCdRead(unsigned int lsn, unsigned int sectors, void *buf, CdRMode *mode)
{
    CdReadReq *req;
    int size;

    req = &libcdvd_read_req;
    if (!((*(volatile int *)&libcdvd_read_flags) & 1)) { /* steer(cast) */
        if (libcdvd_ncmd14() == 6)
            return 0;
    }
    if (libcdvd_ncmd_lock(4) == 0)
        return 0;
    req->lsn = lsn;
    req->sectors = sectors;
    req->buf = buf;
    req->mode[0] = mode->trycount;
    req->mode[1] = mode->spindlctrl;
    req->mode[2] = mode->datapattern;
    req->res0 = libcdvd_read_res;
    req->res1 = &libcdvd_read_res2;
    switch (mode->datapattern) {
    case 0:
        size = sectors << 11;
        break;
    case 1:
        size = sectors * 2328;
        break;
    case 2:
        size = sectors * 2340;
        break;
    default:
        size = sectors << 11;
        break;
    }
    libcdvd_read_res2 = 0;
    if (!((*(volatile int *)&libcdvd_read_flags) & 2)) /* steer(cast) */
        sceSifWriteBackDCache(buf, size);
    sceSifWriteBackDCache(libcdvd_read_res, 144);
    sceSifWriteBackDCache(req, 24);
    sceSifWriteBackDCache(&libcdvd_read_res2, 4);
    if (libcdvd_debug > 0)
        libkernl_printf_line("call cdread cmd\n");
    libcdvd_cur_cmd = 1;
    libcdvd_ncmd_busy = 1;
    if (sceSifCallRpc(libcdvd_ncmd_client, 1, 1, req, 24, 0, 0, libcdvd_sif_handler, libcdvd_read_res) < 0) {
        libcdvd_cur_cmd = 0;
        libcdvd_ncmd_busy = 0;
        SignalSema(libcdvd_ncmd_sema);
        return 0;
    }
    if (libcdvd_debug > 0)
        libkernl_printf_line("cdread end\n");
    return 1;
}

/* -1 if it cannot be asked. */
/* 0x001e3408 */
int sceCdGetError(void)
{
    int ret;

    if (libcdvd_scmd_lock(3) == 0)
        return -1;
    if (sceSifCallRpc(libcdvd_scmd_client, 4, 0, 0, 0, &libcdvd_init_reply, 4, 0, 0) < 0) {
        SignalSema((*(int *)&libcdvd_scmd_sema)); /* steer(cast) */
        return -1;
    }
    ret = *(int *)((unsigned int)&libcdvd_init_reply | 0x20000000);
    SignalSema((*(int *)&libcdvd_scmd_sema)); /* steer(cast) */
    return ret;
}

/* 0 on failure. */
/* 0x001e34a0 */
int sceCdBreak(void)
{
    int ret;

    if (libcdvd_scmd_lock(30) == 0)
        return 0;
    libcdvd_cur_cmd = 8;
    if (sceSifCallRpc(libcdvd_scmd_client, 22, 0, 0, 0, &libcdvd_init_reply, 4, 0, 0) < 0) {
        SignalSema(libcdvd_scmd_sema);
        libcdvd_cur_cmd = 0;
        return 0;
    }
    libcdvd_cur_cmd = 0;
    ret = *(int *)((unsigned int)&libcdvd_init_reply | 0x20000000);
    SignalSema(libcdvd_scmd_sema);
    return ret;
}
