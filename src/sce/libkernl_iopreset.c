/* IOP reset: reboots the IOP with a module image and polls for completion. */

#include "types_libc.h"
#include "types_sce.h"
#include "libc.h"
#include "libkernl_iopreset.h"
#include "kernel_syscalls.h"
#include "libkernl_glue.h"
#include "libkernl_sifcmd.h"
#include "libkernl_sifrpc.h"

extern ResetPkt libkernl_reset_pkt;

/*
 * Stops SIF DMA, sends libkernl_reset_pkt, resets the SIF handshake flags. 0 if DMA cannot queue.
 */
/* 0x001fd9e0 */
static int sceSifResetIop(const char *cmdline, int mode)
{
    DmaDesc dma;
    void *iop;
    int i;

    sceSifStopDma();
    iop = (void *)sceSifGetReg(0x80000000);
    libkernl_reset_pkt.mode = mode;
    for (i = 0; cmdline[i]; i++)
        libkernl_reset_pkt.arg[i] = cmdline[i];
    libkernl_reset_pkt.dest = 0;
    libkernl_reset_pkt.size = i;
    libkernl_reset_pkt.cid = 0x80000003;
    libkernl_reset_pkt.dsize = 0;
    libkernl_reset_pkt.psize = 104;
    dma.data = &libkernl_reset_pkt;
    dma.addr = iop;
    dma.size = 104;
    dma.mode = 0x44;
    sceSifWriteBackDCache(&libkernl_reset_pkt, 104);
    sceSifSetReg(4, 0x40000);
    if (sceSifSetDma(&dma, 1)) {
        sceSifSetReg(4, 0x10000);
        sceSifSetReg(4, 0x20000);
        sceSifSetReg(0x80000002, 0);
        sceSifSetReg(0x80000000, 0);
        return 1;
    }
    return 0;
}

/* SIF status flag. */
/* 0x001fdb20 */
static int sceSifIsAliveIop(void)
{
    unsigned int flag = sceSifGetReg(4) & 0x10000;

    return flag != 0;
}

/* 1 once rebooted; also resets the console output state (its IOP side is gone). */
/* 0x001fdb48 */
int sceSifSyncIop(void)
{
    if (sceSifGetReg(4) & 0x40000) {
        crt_tty_reset();
        return 1;
    }
    return 0;
}

/*
 * "rom0:UDNL <img>"; names over 69 chars rejected. Shuts the RPC layer down first (servers drop).
 */
/* steer(cast) */
/* 0x001fdb80 */
int sceSifRebootIop(const char *img)
{
    char param[80];
    const char *udnl = "rom0:UDNL ";
    char *p;
    const char *end;
    const char *base;              /* img less the prefix length, for the length test */

    for (end = img; *end; end++)
        ;
    base = img - 11;
    if ((unsigned int)(end - base) > 80) {
        printf("too long parameter '%s'\n", img);
        return 0;
    }
    ((void (*)(int))sceSifInitRpc)(0);
    sceSifExitRpc();
    p = param;
    while (*udnl)
        *p++ = *udnl++;
    while (*img)
        *p++ = *img++;
    *p = 0;
    return sceSifResetIop(param, 0);
}
