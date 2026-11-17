/* DMA controller library: reset, global environment, chain/normal/interleave sends and receives. */

#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "sdk_compat.h"
#include "libc.h"
#include "libc_internal.h"
#include "libdma.h"
#include "kernel_syscalls.h"
#include "libpad.h"

static int sceDmaPutEnv(DmaEnv *env);

typedef struct {
    int count;
    int max;
    int init;
    int waiting;
    unsigned int attr;
    unsigned int option;
} SemaParamA;

extern int libdma_chan_addrs[];
extern int libdma_chan_enabled[10];
extern int libdma_debug;
extern unsigned char libdma_sts_tbl[];
extern unsigned char libdma_std_tbl[];
extern unsigned char libdma_mfd_tbl[];
extern DmaEnv libdma_cur_env;
extern int CreateSema(SemaParamA *p);
extern int SetAlarm(int cause, void (*fn)(int, void *, int), int arg);

/* n 0..9, else 0. */
/* 0x001e1478 */
int sceDmaGetChan(unsigned int n)
{
    if (n < 10)
        return libdma_chan_addrs[n];
    return 0;
}

/*
 * Clears channels, acks and masks interrupts, zero environment; mode 1 enables DMA. Returns prior
 * enable.
 */
/* 0x001e14a0 */
int sceDmaReset(int mode)
{
    DmaEnv env;
    DmaChanRegs *dch;
    int old;
    int i;

    old = DREG(0x1000e000) & 1;
    for (i = 0; i < 10; i++) {
        if (libdma_chan_enabled[i]) {
            dch = libdma_chan_addrs[i];
            dch->sadr = 0;
            dch->chcr = 0;
            dch->tadr = 0;
            dch->madr = 0;
            dch->as1 = 0;
            dch->as0 = 0;
        }
    }
    DREG(0x1000e010) = 0xff1f;
    DREG(0x1000e010) = DREG(0x1000e010) & 0xff1f0000;
    libpad_bzero(&env, sizeof(env));
    sceDmaPutEnv(&env);
    if (mode == 1)
        DREG(0x1000e000) |= 1;
    return old;
}

/* Returns the previous mode. */
/* 0x001e1580 */
static int sceDmaDebug(int mode)
{
    int old;

    old = libdma_debug;
    libdma_debug = mode;
    return old;
}

/*
 * Stall source/drain, MFIFO drain, release cycle, priority, skip/transfer counts, MFIFO ring.
 * -1..-4 for an out-of-range stall source, stall drain, MFIFO drain or release cycle.
 */
/* 0x001e1590 */
static int sceDmaPutEnv(DmaEnv *env)
{
    unsigned int ctrl;
    unsigned int pcr;
    unsigned int sqwc;
    unsigned int rbor;
    unsigned int rbsr;

    ctrl = DREG(0x1000e000);
    pcr = DREG(0x1000e020);
    sqwc = DREG(0x1000e030);
    rbor = DREG(0x1000e050);
    rbsr = DREG(0x1000e040);
    if (env->sts >= 10)
        return -1;
    if (env->std >= 10)
        return -2;
    if (env->mfd >= 10)
        return -3;
    if (env->rcyc >= 7)
        return -4;
    ctrl = (ctrl & 0xffffffcf) | (libdma_sts_tbl[env->sts] << 4);
    ctrl = (ctrl & 0xffffff3f) | (libdma_std_tbl[env->std] << 6);
    ctrl = (ctrl & 0xfffffff3) | (libdma_mfd_tbl[env->mfd] << 2);
    if (env->rcyc) {
        ctrl |= 2;
        ctrl = (ctrl & 0xfffffcff) | ((env->rcyc - 1) << 8);
    } else {
        ctrl &= 0xfffffffd;
    }
    pcr = (env->express << 16) | env->notify;
    sqwc = (env->tqwc << 16) | env->sqwc;
    rbor = (unsigned int)env->rbadr;
    rbsr = env->rbmsk;
    DREG(0x1000e000) = ctrl;
    DREG(0x1000e020) = pcr;
    DREG(0x1000e030) = sqwc;
    DREG(0x1000e050) = rbor;
    DREG(0x1000e040) = rbsr;
    libdma_cur_env = *env;
    return 0;
}

/* 0x001e1768 */
static DmaEnv *sceDmaGetEnv(DmaEnv *env)
{
    *env = libdma_cur_env;
    return env;
}

/* Not set when addr is all ones; returns the previous one. */
/* 0x001e17a0 */
static unsigned int sceDmaPutStallAddr(unsigned int addr)
{
    unsigned int old;

    old = DREG(0x1000e060);
    if (addr != 0xffffffff)
        DREG(0x1000e060) = addr;
    return old;
}

/*
 * Waits for the previous transfer first; after about 16 million polls reports a timeout and
 * stops the channel. Every send and receive below waits the same way.
 */
/* 0x001e17c8 */
void sceDmaSend(DmaChanRegs *dch, void *tag)
{
    int cnt = 0x1000000;

    while (dch->chcr & 0x100) {
        if (--cnt < 0) {
            printf("libdma: sync timeout\n");
            {
                DmaChcr_a chcr;

                *(unsigned int *)&chcr = dch->chcr;
                if (chcr.STR) {
                    do {
                        chcr.STR = 0;
                    } while (chcr.STR);
                    dch->chcr = *(unsigned int *)&chcr;
                }
            }
        }
    }
    if (dch->tadr != 0xffffffff)
        dch->tadr = (unsigned int)tag;
    {
        DmaChcr_a chcr;

        *(unsigned int *)&chcr = dch->chcr;
        dch->qwc = 0;
        chcr.MOD = 1;
        chcr.DIR = 1;
        chcr.STR = 1;
        dch->chcr = *(unsigned int *)&chcr;
    }
}

/* size in quadwords. */
/* 0x001e18a0 */
void sceDmaSendN(DmaChanRegs *dch, void *addr, int size)
{
    int cnt = 0x1000000;

    while (dch->chcr & 0x100) {
        if (--cnt < 0) {
            printf("libdma: sync timeout\n");
            {
                DmaChcr_a chcr;

                *(unsigned int *)&chcr = dch->chcr;
                if (chcr.STR) {
                    do {
                        chcr.STR = 0;
                    } while (chcr.STR);
                    dch->chcr = *(unsigned int *)&chcr;
                }
            }
        }
    }
    if (dch->madr != 0xffffffff)
        dch->madr = (unsigned int)addr;
    {
        DmaChcr_a chcr;

        *(unsigned int *)&chcr = dch->chcr;
        dch->qwc = size;
        chcr.MOD = 0;
        chcr.DIR = 1;
        chcr.STR = 1;
        dch->chcr = *(unsigned int *)&chcr;
    }
}

/* size in quadwords. */
/* 0x001e1988 */
static void sceDmaSendI(DmaChanRegs *dch, void *addr, int size)
{
    int cnt = 0x1000000;

    while (dch->chcr & 0x100) {
        if (--cnt < 0) {
            printf("libdma: sync timeout\n");
            {
                DmaChcr_a chcr;

                *(unsigned int *)&chcr = dch->chcr;
                if (chcr.STR) {
                    do {
                        chcr.STR = 0;
                    } while (chcr.STR);
                    dch->chcr = *(unsigned int *)&chcr;
                }
            }
        }
    }
    if (dch->madr != 0xffffffff)
        dch->madr = (unsigned int)addr;
    {
        DmaChcr_a chcr;

        *(unsigned int *)&chcr = dch->chcr;
        dch->qwc = size;
        chcr.MOD = 2;
        chcr.DIR = 1;
        chcr.STR = 1;
        dch->chcr = *(unsigned int *)&chcr;
    }
}

/* 0x001e1a70 */
static void sceDmaRecv(DmaChanRegs *dch)
{
    int cnt = 0x1000000;

    while (dch->chcr & 0x100) {
        if (--cnt < 0) {
            printf("libdma: sync timeout\n");
            {
                DmaChcr_a chcr;

                *(unsigned int *)&chcr = dch->chcr;
                if (chcr.STR) {
                    do {
                        chcr.STR = 0;
                    } while (chcr.STR);
                    dch->chcr = *(unsigned int *)&chcr;
                }
            }
        }
    }
    {
        DmaChcr_a chcr;

        *(unsigned int *)&chcr = dch->chcr;
        dch->qwc = 0;
        chcr.MOD = 1;
        chcr.DIR = 0;
        chcr.STR = 1;
        dch->chcr = *(unsigned int *)&chcr;
    }
}

/* size in quadwords. */
/* 0x001e1b38 */
static void sceDmaRecvN(DmaChanRegs *dch, void *addr, int size)
{
    int cnt = 0x1000000;

    while (dch->chcr & 0x100) {
        if (--cnt < 0) {
            printf("libdma: sync timeout\n");
            {
                DmaChcr_a chcr;

                *(unsigned int *)&chcr = dch->chcr;
                if (chcr.STR) {
                    do {
                        chcr.STR = 0;
                    } while (chcr.STR);
                    dch->chcr = *(unsigned int *)&chcr;
                }
            }
        }
    }
    if (dch->madr != 0xffffffff)
        dch->madr = (unsigned int)addr;
    {
        DmaChcr_a chcr;

        *(unsigned int *)&chcr = dch->chcr;
        dch->qwc = size;
        chcr.MOD = 0;
        chcr.DIR = 0;
        chcr.STR = 1;
        dch->chcr = *(unsigned int *)&chcr;
    }
}

/* size in quadwords. */
/* 0x001e1c28 */
static void sceDmaRecvI(DmaChanRegs *dch, void *addr, int size)
{
    int cnt = 0x1000000;

    while (dch->chcr & 0x100) {
        if (--cnt < 0) {
            printf("libdma: sync timeout\n");
            {
                DmaChcr_a chcr;

                *(unsigned int *)&chcr = dch->chcr;
                if (chcr.STR) {
                    do {
                        chcr.STR = 0;
                    } while (chcr.STR);
                    dch->chcr = *(unsigned int *)&chcr;
                }
            }
        }
    }
    if (dch->madr != 0xffffffff)
        dch->madr = (unsigned int)addr;
    {
        DmaChcr_a chcr;

        *(unsigned int *)&chcr = dch->chcr;
        dch->qwc = size;
        chcr.MOD = 2;
        chcr.DIR = 0;
        chcr.STR = 1;
        dch->chcr = *(unsigned int *)&chcr;
    }
}

/*
 * Mode 1: 1 while transferring. Else waits up to cnt polls (0 = about 16 million), then times out.
 */
/* 0x001e1d18 */
int sceDmaSync(DmaChanRegs *dch, int mode, int cnt)
{
    if (mode == 1)
        return (dch->chcr >> 8) & 1;
    if (cnt == 0)
        cnt = 0x1000000;
    while (dch->chcr & 0x100) {
        if (--cnt < 0) {
            printf("libdma: sync timeout\n");
            {
                DmaChcr_a chcr;

                *(unsigned int *)&chcr = dch->chcr;
                if (chcr.STR) {
                    do {
                        chcr.STR = 0;
                    } while (chcr.STR);
                    dch->chcr = *(unsigned int *)&chcr;
                }
            }
        }
    }
    return 0;
}

/* Waits for the channel address to pass addr (mode 1: 1 while not). */
/* 0x001e1de0 */
static int sceDmaWatch(DmaChanRegs *dch, void *addr, int mode, int cnt)
{
    if (mode == 1)
        return dch->madr < (unsigned int)addr;
    if (cnt == 0)
        cnt = 0x1000000;
    while (dch->madr < (unsigned int)addr) {
        if (--cnt < 0) {
            printf("libdma: sync timeout\n");
            {
                DmaChcr_a chcr;

                *(unsigned int *)&chcr = dch->chcr;
                if (chcr.STR) {
                    do {
                        chcr.STR = 0;
                    } while (chcr.STR);
                    dch->chcr = *(unsigned int *)&chcr;
                }
            }
        }
    }
    return 0;
}

/*
 * Holds the controller while changing the channel; returns the old control word for sceDmaRestart.
 */
/* 0x001e1eb0 */
static unsigned int sceDmaPause(DmaChanRegs *dch)
{
    unsigned int old;

    if (DREG(0x1000f520) == 0)
        DREG(0x1000f590) = 0x1000;
    old = dch->chcr;
    dch->chcr = old & 0xfffffeff;
    DREG(0x1000f590) = 0;
    return old;
}

/* Returns whether it was transferring. */
/* 0x001e1ef8 */
static int sceDmaRestart(DmaChanRegs *dch, unsigned int chcr)
{
    unsigned int old;

    old = dch->chcr;
    dch->chcr = chcr;
    return (old >> 8) & 1;
}

/* Wakes the waiting thread through its semaphore. */
/* 0x001e1f10 */
static void libdma_wait_int_handler(int ch, void *cb_arg, int sema)
{
    iSignalSema(sema);
    ExitHandler();
}

/* Sleeps cause kernel alarm ticks (hblank periods). CD and memory card libraries poll this way. */
/* 0x001e1f38 */
void libdma_wait_int(unsigned short cause)
{
    SemaParamA sema_param;
    int sema;

    sema_param.max = 1;
    sema_param.init = 0;
    sema_param.option = 0;
    sema = CreateSema(&sema_param);
    SetAlarm(cause, libdma_wait_int_handler, sema);
    WaitSema(sema);
    DeleteSema(sema);
}
