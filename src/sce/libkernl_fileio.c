/*
 * File I/O client, EE side: remote calls to the IOP file server. Descriptors are slots in a
 * 32-entry table; no-wait slots return at once and finish via the end-of-call command.
 */

#include "types_libc.h"
#include "types_sce.h"
#include <stdarg.h>
#include "libc.h"
#include "libkernl_fileio.h"
#include "kernel_syscalls.h"
#include "libkernl_intr.h"
#include "libkernl_sifcmd.h"
#include "libkernl_sifrpc.h"

typedef struct FioEnd {
    int sema;               /* 0x00 semaphore, negated for a no-wait call */
    int fno;                /* 0x04 call number */
    char *dest;             /* 0x08 */
    int size;               /* 0x0c */
} FioEnd;

typedef struct FioReadEnd {
    int n1;                 /* 0x00 */
    int n2;                 /* 0x04 */
    char *dest1;            /* 0x08 */
    char *dest2;            /* 0x0c */
    char buf1[64];          /* 0x10 */
    char buf2[64];          /* 0x50 */
} FioReadEnd;

extern int sif_fio_slot_sema;
extern int sif_fio_nowait_sema;
extern int CreateSema(SemaParam *p);
extern SlotEntry sif_fio_slots[32];
extern char sif_fio_recv_buf[];
extern char sif_fio_read_end[];

extern int sif_fio_lock_sema;
extern int sif_fio_inited;
extern RpcPkt sif_fio_client;
extern char sif_fio_cmd_data[];
extern char sif_fio_peer_ver[];
extern char sif_fio_ver_reply[];

extern char *sif_fio_ver_tag2;
extern FioArg sif_fio_arg;
extern volatile int sif_fio_nowait_semas[32];
extern void *sif_fio_ioctl_argp;
extern char sif_fio_nowait_result[];

/* Slot table and no-wait table locks, on first use. */
/* 0x001f9408 */
static void sif_fio_create_sema(void)
{
    SemaParam sema_param;

    if (sif_fio_slot_sema == -1) {
        sema_param.init_count = 1;
        sema_param.max_count = 1;
        sema_param.option = 0;
        sif_fio_slot_sema = CreateSema(&sema_param);
        sif_fio_nowait_sema = CreateSema(&sema_param);
    }
}

/* 0 if all 32 are taken. */
/* 0x001f9468 */
static SlotEntry *sif_fio_alloc_slot(void)
{
    SlotEntry *slot;

    sif_fio_create_sema();
    WaitSema(sif_fio_slot_sema);
    for (slot = sif_fio_slots; slot < sif_fio_slots + 32; slot++) {
        if (slot->state == 0) {
            slot->state = 0x10000000;
            SignalSema(sif_fio_slot_sema);
            return slot;
        }
    }
    SignalSema(sif_fio_slot_sema);
    return 0;
}

/* 0 for an invalid descriptor. */
/* 0x001f94f0 */
static SlotEntry *sif_fio_get_slot(unsigned int i)
{
    SlotEntry *slot;

    sif_fio_create_sema();
    WaitSema(sif_fio_slot_sema);
    if (i >= 32) {
        SignalSema(sif_fio_slot_sema);
        return 0;
    }
    slot = &sif_fio_slots[i];
    SignalSema(sif_fio_slot_sema);
    return slot;
}

/*
 * Copies result data (unaligned read edges, dir entry, status, up to 1024 bytes of ioctl/link
 * output), then wakes the caller or, for no-wait, frees its sif_fio_nowait_semas entry.
 */
/* 0x001f9560 */
static void sif_fio_end_cmd_handler(void *data, void *argp)
{
    FioEnd rd;
    char *dst;
    unsigned int len;
    FioReadEnd *rend;
    FioEnd *hdr = (FioEnd *)sif_fio_recv_buf;
    char *dstp;
    long id;
    int i;

    *(Bytes4 *)&rd.sema = *(Bytes4 *)((unsigned int)&hdr->sema | 0x20000000);
    *(Bytes4 *)&rd.fno = *(Bytes4 *)((unsigned int)&hdr->fno | 0x20000000);
    *(Bytes4 *)&rd.dest = *(Bytes4 *)((unsigned int)&hdr->dest | 0x20000000);
    *(Bytes4 *)&rd.size = *(Bytes4 *)((unsigned int)&hdr->size | 0x20000000);
    if (rd.sema >= 0)
        memcpy(rd.dest, (void *)((unsigned int)(sif_fio_recv_buf + 16) | 0x20000000), rd.size);
    switch (rd.fno) {
    case 2:
        rend = (FioReadEnd *)((unsigned int)sif_fio_read_end | 0x20000000);
        if (rend->n1 > 0) {
            dstp = rend->dest1;
            for (i = 0; i < rend->n1; i++)
                dstp[i] = rend->buf1[i];
        }
        if (rend->n2 > 0) {
            dstp = rend->dest2;
            for (i = 0; i < rend->n2; i++)
                dstp[i] = rend->buf2[i];
        }
        break;
    case 11:
        __builtin_memcpy(&dst, (void *)((unsigned int)sif_fio_read_end | 0x20000000), 4);
        __builtin_memcpy(dst, (void *)((unsigned int)(sif_fio_read_end + 4) | 0x20000000), 324);
        break;
    case 12:
        __builtin_memcpy(&dst, (void *)((unsigned int)sif_fio_read_end | 0x20000000), 4);
        __builtin_memcpy(dst, (void *)((unsigned int)(sif_fio_read_end + 4) | 0x20000000), 64);
        break;
    case 23:
    case 25:
    case 26:
        __builtin_memcpy(&dst, (void *)((unsigned int)sif_fio_read_end | 0x20000000), 4);
        __builtin_memcpy(&len, (void *)((unsigned int)(sif_fio_read_end + 4) | 0x20000000), 4);
        if (len > 1024)
            len = 1024;
        memcpy(dst, (void *)((unsigned int)(sif_fio_read_end + 8) | 0x20000000), len);
        break;
    }
    if (rd.sema < 0) {
        int sema = -rd.sema;

        rd.sema = sema;
        id = sema;
        for (i = 0; i < 32; i++) {
            if ((*(int (*)[32])&sif_fio_nowait_semas)[i] == id) { /* steer(cast) */
                *(volatile int *)&(*(int (*)[32])&sif_fio_nowait_semas)[i] = -1;   /* steer(volatile) */ /* steer(cast) */
                break;
            }
        }
    } else {
        iSignalSema(rd.sema);
    }
}

/* On first use. */
/* 0x001f9920 */
static void sif_fio_create_lock(void)
{
    SemaParam sema_param;

    if (sif_fio_lock_sema == -1) {
        sema_param.init_count = 1;
        sema_param.max_count = 1;
        sema_param.option = 0;
        sif_fio_lock_sema = CreateSema(&sema_param);
    }
}

/* Guards the shared argument block; op unused. Returns 0. */
/* 0x001f9970 */
static int sif_fio_lock(int op)
{
    sif_fio_create_lock();
    WaitSema(sif_fio_lock_sema);
    return 0;
}

/* 0x001f99a0 */
static void sif_fio_unlock(void)
{
    SignalSema(sif_fio_lock_sema);
}

/* Busy-waits for the bind, frees all slots, fetches the version tag. -1 if binding fails. */
/* steer(cast) */
/* 0x001f99b0 */
static int sceFsInit(void)
{
    int i;
    SlotEntry *slot;
    char *buf;

    ((void (*)(int))sceSifInitRpc)(0);
    DIntr();
    sceSifAddCmdHandler(0x80000011, sif_fio_end_cmd_handler, sif_fio_cmd_data);
    EIntr();
    for (;;) {
        if (sceSifBindRpc(&sif_fio_client, 0x80000001, 0) < 0)
            return -1;
        if (sif_fio_client.server)
            break;
        i = 0x100000;
        while (i--)
            ;
    }
    sif_fio_create_sema();
    WaitSema(sif_fio_slot_sema);
    for (slot = sif_fio_slots; slot < sif_fio_slots + 32; slot++)
        slot->state = 0;
    SignalSema(sif_fio_slot_sema);
    buf = sif_fio_recv_buf;
    if (sceSifCallRpc(&sif_fio_client, 255, 0, &buf, 4, sif_fio_ver_reply, 4, 0, 0) < 0)
        return 0xfffeffff;
    *(Bytes4 *)sif_fio_peer_ver = *(Bytes4 *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_inited = 1;
    return 0;
}

/* 1 when the tag is neither libkernl_ver_tag ("2300") nor the alternative, unless those match. */
/* 0x001f9b50 */
static int sif_fio_version_mismatch(void)
{
    int ret = 0;
    char *ver = libkernl_ver_tag;
    char *tag = sif_fio_peer_ver;

    if (memcmp(tag, ver, 4) && memcmp(tag, sif_fio_ver_tag2, 4))
        ret = memcmp(ver, sif_fio_ver_tag2, 4) != 0;
    return ret;
}

/* Needed after an IOP reboot; the next call starts the client anew. */
/* 0x001f9be0 */
int sceFsReset(void)
{
    sif_fio_inited = 0;
    memset(sif_fio_peer_ver, 0, 4);
    return 0;
}

/* name cut to 1023. Error on a version mismatch, -19 no free slot, -11 call failed. */
/* 0x001f9c18 */
int sceOpen(const char *name, int flags, ...)
{
    SemaParam sema_param;
    int result;
    va_list ap;
    FioArg *arg;
    SlotEntry *slot;
    int sema;
    int ok;
    int index;
    int mode;
    int i;

    va_start(ap, flags);
    arg = &sif_fio_arg;
    sif_fio_lock(0);
    if (!sif_fio_inited)
        sceFsInit();
    if (sif_fio_version_mismatch()) {
        sif_fio_unlock();
        return 0xfffefffc;
    }
    slot = sif_fio_alloc_slot();
    if (!slot) {
        sif_fio_unlock();
        return -19;
    }
    mode = *(int *)ap;
    for (i = 0; i < 1024; i++) {
        if (!(arg->name[i] = name[i]))
            break;
    }
    if (i == 1024)
        arg->name[1023] = 0;
    index = slot - sif_fio_slots;
    arg->fd = flags & 0x0fffffff;
    arg->param = mode;
    *(int *)(arg->name + 1024) = index;
    sema_param.max_count = 1;
    sema_param.init_count = 0;
    sema_param.option = 0;
    sema = CreateSema(&sema_param);
    arg->rbuf = &result;
    arg->sema = sema;
    arg->rsize = 4;
    if (sceSifCallRpc(&sif_fio_client, 0, 0, &sif_fio_arg, 1048, sif_fio_ver_reply, 4, 0, 0) < 0) {
        DeleteSema(sema);
        sif_fio_unlock();
        return -11;
    }
    ok = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!ok) {
        DeleteSema(sema);
        return -11;
    }
    WaitSema(sema);
    DeleteSema(sema);
    if (result < 0) {
        WaitSema(sif_fio_slot_sema);
        slot->state = 0;
        SignalSema(sif_fio_slot_sema);
        return result;
    }
    ok = index;
    WaitSema(sif_fio_slot_sema);
    slot->fd = result;
    slot->state |= flags;
    SignalSema(sif_fio_slot_sema);
    return ok;
}

/* -1 not started, -9 bad descriptor, -11 call failed, or the server's error. */
/* 0x001f9ea0 */
int sceClose(int fd)
{
    SemaParam sema_param;
    int result;
    SlotEntry *slot;
    FioArg *arg;
    int sema;
    int ok;

    arg = &sif_fio_arg;
    slot = sif_fio_get_slot(fd);
    sif_fio_lock(1);
    if (!sif_fio_inited) {
        sif_fio_unlock();
        return -1;
    }
    if (!slot || !slot->state) {
        sif_fio_unlock();
        return -9;
    }
    arg->fd = slot->fd;
    arg->param = slot - sif_fio_slots;
    sema_param.max_count = 1;
    sema_param.init_count = 0;
    sema_param.option = 0;
    sema = CreateSema(&sema_param);
    sif_fio_arg.sema = sema;
    arg->rbuf = &result;
    arg->rsize = 4;
    if (sceSifCallRpc(&sif_fio_client, 1, 0, arg, 20, sif_fio_ver_reply, 4, 0, 0) < 0) {
        DeleteSema(sema);
        sif_fio_unlock();
        return -11;
    }
    slot->state = 0;
    ok = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!ok) {
        DeleteSema(sema);
        return -11;
    }
    WaitSema(sema);
    DeleteSema(sema);
    if (result < 0)
        return result;
    return 0;
}

/* New position; 0 for no-wait; -1/-9/-11 as sceClose. */
/* 0x001fa020 */
int sceLseek(int fd, int offset, int whence)
{
    SemaParam sema_param;
    int result;
    SlotEntry *slot;
    FioArg *arg;
    int sema;
    int state;
    int ok;
    int i;

    arg = &sif_fio_arg;
    slot = sif_fio_get_slot(fd);
    sif_fio_lock(4);
    if (!sif_fio_inited) {
        sif_fio_unlock();
        return -1;
    }
    if (!slot || !(state = slot->state)) {
        sif_fio_unlock();
        return -9;
    }
    arg->fd = slot->fd;
    arg->param = offset;
    sema_param.init_count = 0;
    *(int *)arg->name = whence;
    sema_param.option = 0;
    *(int *)(arg->name + 4) = slot - sif_fio_slots;
    sema_param.max_count = 1;
    sema = CreateSema(&sema_param);
    sif_fio_arg.sema = sema;
    arg->rbuf = &result;
    arg->rsize = 4;
    if (state & 0x8000) {
        WaitSema(sif_fio_nowait_sema);
        for (i = 0; i < 32; i++) {
            if (sif_fio_nowait_semas[i] == -1) {
                sif_fio_nowait_semas[i] = arg->sema;
                arg->sema = -arg->sema;
                break;
            }
        }
        SignalSema(sif_fio_nowait_sema);
    }
    if (sceSifCallRpc(&sif_fio_client, 4, 0, &sif_fio_arg, 28, sif_fio_ver_reply, 4, 0, 0) < 0) {
        DeleteSema(sema);
        sif_fio_unlock();
        return -11;
    }
    ok = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!ok) {
        DeleteSema(sema);
        return -11;
    }
    if ((short)state & 0x8000) {
        DeleteSema(sema);
        return 0;
    }
    WaitSema(sema);
    DeleteSema(sema);
    return result;
}

/* Writes buf back from the cache unless opened uncached. Bytes read; 0 for no-wait; -1/-9/-11. */
/* 0x001fa258 */
int sceRead(int fd, void *buf, int size)
{
    SemaParam sema_param;
    int result;
    SlotEntry *slot;
    FioArg *arg;
    int sema;
    int state;
    int ok;
    int i;

    arg = &sif_fio_arg;
    slot = sif_fio_get_slot(fd);
    sif_fio_lock(2);
    if (!sif_fio_inited) {
        sif_fio_unlock();
        return -1;
    }
    if (!slot || !(state = slot->state)) {
        sif_fio_unlock();
        return -9;
    }
    arg->fd = slot->fd;
    *(int *)(arg->name + 8) = slot - sif_fio_slots;
    arg->param = (int)buf;
    *(int *)arg->name = size;
    sema_param.max_count = 1;
    sema_param.init_count = 0;
    sema_param.option = 0;
    sema = CreateSema(&sema_param);
    sif_fio_arg.sema = sema;
    arg->rbuf = &result;
    arg->rsize = 4;
    if (state & 0x8000) {
        WaitSema(sif_fio_nowait_sema);
        for (i = 0; i < 32; i++) {
            if (sif_fio_nowait_semas[i] == -1) {
                sif_fio_nowait_semas[i] = arg->sema;
                arg->sema = -arg->sema;
                break;
            }
        }
        SignalSema(sif_fio_nowait_sema);
    }
    if (!(state & 0x20000000))
        sceSifWriteBackDCache(buf, size);
    sceSifWriteBackDCache(sif_fio_recv_buf, 164);
    sceSifWriteBackDCache(arg, 32);
    if (sceSifCallRpc(&sif_fio_client, 2, 0, &sif_fio_arg, 32, sif_fio_ver_reply, 4, 0, 0) < 0) {
        DeleteSema(sema);
        sif_fio_unlock();
        return -11;
    }
    ok = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!ok) {
        DeleteSema(sema);
        return -11;
    }
    if ((short)state & 0x8000) {
        DeleteSema(sema);
        return 0;
    }
    WaitSema(sema);
    DeleteSema(sema);
    return result;
}

/* Bytes before buf's first 16-byte boundary go in the argument block (DMA moves quadwords). */
/* 0x001fa4c8 */
int sceWrite(int fd, char *buf, int size)
{
    SemaParam sema_param;
    int result;
    SlotEntry *slot;
    FioWriteArg *arg;
    int sema;
    int state;
    int ok;
    int i;
    int n;
    int j;

    arg = &sif_fio_arg;
    slot = sif_fio_get_slot(fd);
    sif_fio_lock(3);
    if (!sif_fio_inited) {
        sif_fio_unlock();
        return -1;
    }
    if (!slot || !(state = slot->state)) {
        sif_fio_unlock();
        return -9;
    }
    arg->handle = slot->fd;
    arg->index = slot - sif_fio_slots;
    arg->size = size;
    arg->buf = buf;
    sema_param.max_count = 1;
    sema_param.init_count = 0;
    sema_param.option = 0;
    sema = CreateSema(&sema_param);
    sif_fio_arg.sema = sema;
    arg->rbuf = &result;
    arg->rsize = 4;
    if (state & 0x8000) {
        WaitSema(sif_fio_nowait_sema);
        for (i = 0; i < 32; i++) {
            if (sif_fio_nowait_semas[i] == -1) {
                sif_fio_nowait_semas[i] = arg->sema;
                arg->sema = -arg->sema;
                break;
            }
        }
        SignalSema(sif_fio_nowait_sema);
    }
    if (!((unsigned int)buf & 15))
        n = 0;
    else {
        unsigned int head = (unsigned int)buf - 16;
        n = (((unsigned int)buf >> 4) << 4) - head;
    }
    if (size < n)
        n = size;
    if (!(state & 0x20000000))
        sceSifWriteBackDCache(buf, size);
    buf = (char *)((unsigned int)buf | 0x20000000);
    arg->head = n;
    for (j = 0; j < n; j++)
        arg->headbuf[j] = buf[j];
    do { } while (0);
    if (sceSifCallRpc(&sif_fio_client, 3, 0, &sif_fio_arg, 48, sif_fio_ver_reply, 4, 0, 0) < 0) {
        DeleteSema(sema);
        sif_fio_unlock();
        return -11;
    }
    ok = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!ok) {
        DeleteSema(sema);
        return -11;
    }
    if ((short)state & 0x8000) {
        DeleteSema(sema);
        return 0;
    }
    WaitSema(sema);
    DeleteSema(sema);
    return result;
}

/*
 * Local: 1 = any no-wait call running into *argp, 2/3 = 32/64-bit result of the last no-wait
 * call. Others go to the server with 1024 bytes of argp. -9 bad descriptor, -11 call failed.
 */
/* 0x001fa788 */
int sceIoctl(int fd, int cmd, void *argp)
{
    SemaParam sema_param;
    int result;
    SlotEntry *slot;
    FioFdIoctlArg *arg;
    int sema;
    int ok = 1;
    int i;
    int st;

    arg = &sif_fio_arg;
    slot = sif_fio_get_slot(fd);
    sif_fio_lock(5);
    sif_fio_ioctl_argp = argp;
    if (!sif_fio_inited)
        sceFsInit();
    if (!slot || !slot->state) {
        sif_fio_unlock();
        return -9;
    }
    arg->buf = 0;
    arg->buflen = 0;
    switch (cmd) {
    case 1:
        WaitSema(sif_fio_nowait_sema);
        for (i = 0; i < 32; i++) {
            if (sif_fio_nowait_semas[i] != -1)
                break;
        }
        if (i == 32)
            *(int *)sif_fio_ioctl_argp = 0;
        else
            *(int *)sif_fio_ioctl_argp = 1;
        SignalSema(sif_fio_nowait_sema);
        sif_fio_unlock();
        return 0;
    case 2:
        *(int *)argp = *(int *)((unsigned int)sif_fio_nowait_result | 0x20000000);
        sif_fio_unlock();
        return 0;
    case 3:
        *(long *)argp = *(long *)((unsigned int)sif_fio_nowait_result | 0x20000000);
        sif_fio_unlock();
        return 0;
    }
    arg->handle = slot->fd;
    arg->cmd = cmd;
    if (!argp) {
        arg->arglen = 0;
    } else {
        arg->arglen = 1024;
        *(FioIoctlBuf *)arg->data = *(FioIoctlBuf *)argp;
    }
    sema_param.max_count = 1;
    sema_param.init_count = 0;
    sema_param.option = 0;
    sema = CreateSema(&sema_param);
    arg->sema = sema;
    arg->rbuf = &result;
    arg->rsize = 4;
    sceSifWriteBackDCache(&sif_fio_arg, 1056);
    if ((ok = sceSifCallRpc(&sif_fio_client, 5, 0, &sif_fio_arg, 1056, sif_fio_ver_reply, 4, 0, 0)) < 0) {
        DeleteSema(sema);
        sif_fio_unlock();
        return -11;
    }
    st = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!st) {
        DeleteSema(sema);
        return -11;
    }
    WaitSema(sema);
    DeleteSema(sema);
    return result;
}

/* Each buffer at most 1024. Bug: -22 for oversized buffers returns with the client lock held. */
/* 0x001faad8 */
static int sceIoctl2(int fd, int cmd, const void *argdata, unsigned int arglen, void *buf,
                 unsigned int buflen)
{
    SemaParam sema_param;
    int result;
    SlotEntry *slot;
    FioFdIoctlArg *arg;
    int sema;
    int ok;

    arg = &sif_fio_arg;
    slot = sif_fio_get_slot(fd);
    sif_fio_lock(26);
    if (!sif_fio_inited)
        sceFsInit();
    if (!slot || !slot->state) {
        sif_fio_unlock();
        return -9;
    }
    if (arglen > 1024 || buflen > 1024)
        return -22;
    if (!argdata)
        arg->arglen = 0;
    else
        memcpy(arg->data, argdata, arglen);
    arg->handle = slot->fd;
    arg->cmd = cmd;
    arg->arglen = arglen;
    sema_param.max_count = 1;
    sema_param.init_count = 0;
    sema_param.option = 0;
    sema = CreateSema(&sema_param);
    arg->buflen = buflen;
    arg->rbuf = &result;
    arg->rsize = 4;
    arg->buf = buf;
    arg->sema = sema;
    sceSifWriteBackDCache(&sif_fio_arg, 1056);
    if (sceSifCallRpc(&sif_fio_client, 26, 0, &sif_fio_arg, 1056, sif_fio_ver_reply, 4, 0, 0) < 0) {
        DeleteSema(sema);
        sif_fio_unlock();
        return -11;
    }
    ok = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!ok) {
        DeleteSema(sema);
        return -11;
    }
    WaitSema(sema);
    DeleteSema(sema);
    return result;
}

/* name cut to 1023; waits. -11 if the call fails. */
/* 0x001facb0 */
static int sif_fio_path_call(const char *name, int fno)
{
    SemaParam sema_param;
    int result;
    FioPathArg *arg;
    int sema;
    int ok;
    int i;

    arg = &sif_fio_arg;
    sif_fio_lock(fno);
    if (!sif_fio_inited)
        sceFsInit();
    for (i = 0; i < 1024; i++) {
        if (!(arg->name[i] = name[i]))
            break;
    }
    if (i == 1024) {
        arg->name[1023] = 0;
        i = 1023;
    }
    sema_param.max_count = 1;
    sema_param.init_count = 0;
    sema_param.option = 0;
    sema = CreateSema(&sema_param);
    arg->rbuf = &result;
    arg->sema = sema;
    arg->rsize = 4;
    if (sceSifCallRpc(&sif_fio_client, fno, 0, &sif_fio_arg, i + 13, sif_fio_ver_reply, 4, 0, 0) < 0) {
        DeleteSema(sema);
        sif_fio_unlock();
        return -11;
    }
    ok = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!ok) {
        DeleteSema(sema);
        return -11;
    }
    WaitSema(sema);
    DeleteSema(sema);
    return result;
}

/* 0x001fae58 */
static int sceRemove(const char *name)
{
    return sif_fio_path_call(name, 6);
}

/* -11 if the call fails. */
/* 0x001fae78 */
static int sceMkdir(const char *name, int mode)
{
    SemaParam sema_param;
    int result;
    FioTwoPathArg *arg;
    int sema;
    int ok;
    int i;

    arg = &sif_fio_arg;
    sif_fio_lock(7);
    if (!sif_fio_inited)
        sceFsInit();
    for (i = 0; i < 1024; i++) {
        if (!(arg->name[i] = name[i]))
            break;
    }
    if (i == 1024) {
        arg->name[1023] = 0;
        i = 1023;
    }
    arg->mode = mode;
    sema_param.max_count = 1;
    sema_param.init_count = 0;
    sema_param.option = 0;
    sema = CreateSema(&sema_param);
    arg->rbuf = &result;
    arg->sema = sema;
    arg->rsize = 4;
    if (sceSifCallRpc(&sif_fio_client, 7, 0, &sif_fio_arg, i + 17, sif_fio_ver_reply, 4, 0, 0) < 0) {
        DeleteSema(sema);
        sif_fio_unlock();
        return -11;
    }
    ok = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!ok) {
        DeleteSema(sema);
        return -11;
    }
    WaitSema(sema);
    DeleteSema(sema);
    return result;
}

/* 0x001fb028 */
static int sceRmdir(const char *name)
{
    return sif_fio_path_call(name, 8);
}

/* Options at most 1024 (-7 otherwise); -11 if the call fails. */
/* 0x001fb048 */
static int sceFormat(const char *name1, const char *name2, const char *data, int len)
{
    SemaParam sema_param;
    int result;
    FioCtlArg *arg;
    int sema;
    int ok;
    int i;

    arg = &sif_fio_arg;
    sif_fio_lock(14);
    if (!sif_fio_inited)
        sceFsInit();
    for (i = 0; i < 1024; i++) {
        if (!(arg->name1[i] = name1[i]))
            break;
    }
    if (i == 1024)
        arg->name1[1023] = 0;
    if (!name2) {
        arg->name2[0] = 0;
    } else {
        for (i = 0; i < 1024; i++) {
            if (!(arg->name2[i] = name2[i]))
                break;
        }
        if (i == 1024)
            arg->name2[1023] = 0;
    }
    if (len > 1024) {
        sif_fio_unlock();
        return -7;
    }
    for (i = 0; i < len; i++)
        arg->data[i] = data[i];
    arg->len = len;
    sema_param.max_count = 1;
    sema_param.init_count = 0;
    sema_param.option = 0;
    sema = CreateSema(&sema_param);
    arg->rbuf = &result;
    arg->sema = sema;
    arg->rsize = 4;
    sceSifWriteBackDCache(&sif_fio_arg, 3088);
    if (sceSifCallRpc(&sif_fio_client, 14, 0, &sif_fio_arg, 3088, sif_fio_ver_reply, 4, 0, 0) < 0) {
        DeleteSema(sema);
        sif_fio_unlock();
        return -11;
    }
    ok = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!ok) {
        DeleteSema(sema);
        return -11;
    }
    WaitSema(sema);
    DeleteSema(sema);
    return result;
}

/* Waits; -1 if the call fails. */
/* 0x001fb2b8 */
static int sif_fio_call15(int x)
{
    SemaParam sema_param;
    int result;
    FioArg *arg;
    int sema;
    int ok;

    arg = &sif_fio_arg;
    sif_fio_lock(15);
    if (!sif_fio_inited)
        sceFsInit();
    arg->fd = x;
    sema_param.max_count = 1;
    sema_param.init_count = 0;
    sema_param.option = 0;
    sema = CreateSema(&sema_param);
    sif_fio_arg.sema = sema;
    arg->rbuf = &result;
    arg->rsize = 4;
    if (sceSifCallRpc(&sif_fio_client, 15, 0, arg, 16, sif_fio_ver_reply, 4, 0, 0) < 0) {
        DeleteSema(sema);
        sif_fio_unlock();
        return -1;
    }
    ok = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!ok) {
        DeleteSema(sema);
        return -1;
    }
    WaitSema(sema);
    DeleteSema(sema);
    return result;
}

/* 0x001fb3d8 */
static int sif_fio_path_call16(const char *name)
{
    return sif_fio_path_call(name, 16);
}

/* -19 if no slot is free. */
/* 0x001fb3f8 */
static int sceDopen(const char *name)
{
    SlotEntry *slot;
    int ret;

    slot = sif_fio_alloc_slot();
    if (!slot)
        return -19;
    ret = sif_fio_path_call(name, 9);
    if (ret < 0) {
        WaitSema(sif_fio_slot_sema);
        slot->state = 0;
        SignalSema(sif_fio_slot_sema);
        return ret;
    }
    WaitSema(sif_fio_slot_sema);
    slot->fd = ret;
    ret = slot - sif_fio_slots;
    SignalSema(sif_fio_slot_sema);
    return ret;
}

/* Returns as sceClose. */
/* 0x001fb498 */
static int sceDclose(int fd)
{
    SemaParam sema_param;
    int result;
    SlotEntry *slot;
    FioArg *arg;
    int sema;
    int ok;

    arg = &sif_fio_arg;
    slot = sif_fio_get_slot(fd);
    sif_fio_lock(10);
    if (!sif_fio_inited) {
        sif_fio_unlock();
        return -1;
    }
    if (!slot || !slot->state) {
        sif_fio_unlock();
        return -9;
    }
    arg->fd = slot->fd;
    sema_param.max_count = 1;
    sema_param.init_count = 0;
    sema_param.option = 0;
    sema = CreateSema(&sema_param);
    sif_fio_arg.sema = sema;
    arg->rbuf = &result;
    arg->rsize = 4;
    if (sceSifCallRpc(&sif_fio_client, 10, 0, arg, 20, sif_fio_ver_reply, 4, 0, 0) < 0) {
        DeleteSema(sema);
        sif_fio_unlock();
        return -11;
    }
    slot->state = 0;
    ok = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!ok) {
        DeleteSema(sema);
        return -11;
    }
    WaitSema(sema);
    DeleteSema(sema);
    if (result < 0)
        return result;
    return 0;
}

/* -1/-9/-11 as sceClose. Bug: if the call cannot be sent, waits on a semaphore nothing signals. */
/* 0x001fb600 */
static int sceDread(int fd, void *buf)
{
    SemaParam sema_param;
    int result;
    SlotEntry *slot;
    FioArg *arg;
    int sema;
    int ok;

    arg = &sif_fio_arg;
    slot = sif_fio_get_slot(fd);
    sif_fio_lock(11);
    if (!sif_fio_inited) {
        sif_fio_unlock();
        return -1;
    }
    if (!slot || !slot->state) {
        sif_fio_unlock();
        return -9;
    }
    arg->fd = slot->fd;
    arg->param = (int)buf;
    sema_param.max_count = 1;
    sema_param.init_count = 0;
    sema_param.option = 0;
    sema = CreateSema(&sema_param);
    sif_fio_arg.sema = sema;
    arg->rbuf = &result;
    arg->rsize = 4;
    if (sceSifCallRpc(&sif_fio_client, 11, 0, arg, 32, sif_fio_ver_reply, 4, 0, 0) < 0) {
        WaitSema(sema);
        sif_fio_unlock();
        return -11;
    }
    ok = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!ok) {
        DeleteSema(sema);
        return -11;
    }
    WaitSema(sema);
    DeleteSema(sema);
    return result;
}

/* The server stores the status record at the address passed in mode. -11 if the call fails. */
/* 0x001fb758 */
static int sceGetstat(const unsigned char *name, int mode)
{
    SemaParam sema_param;
    int result;
    FioPathArg2u *arg;
    int sema;
    int ok;
    int i;

    arg = &sif_fio_arg;
    sif_fio_lock(12);
    if (!sif_fio_inited)
        sceFsInit();
    for (i = 0; i < 1024; i++) {
        if (!(arg->name[i] = name[i]))
            break;
    }
    if (i == 1024) {
        arg->name[1023] = 0;
        i = 1023;
    }
    arg->mode = mode;
    sema_param.max_count = 1;
    sema_param.init_count = 0;
    sema_param.option = 0;
    sema = CreateSema(&sema_param);
    arg->rbuf = &result;
    arg->sema = sema;
    arg->rsize = 4;
    if (sceSifCallRpc(&sif_fio_client, 12, 0, &sif_fio_arg, i + 17, sif_fio_ver_reply, 4, 0, 0) < 0) {
        DeleteSema(sema);
        sif_fio_unlock();
        return -11;
    }
    ok = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!ok) {
        DeleteSema(sema);
        return -11;
    }
    WaitSema(sema);
    DeleteSema(sema);
    return result;
}

/* Fields selected by mask. */
/* 0x001fb8f8 */
static int sceChstat(const unsigned char *name, const void *stat, int mask)
{
    SemaParam sema_param;
    int result;
    FioStatArg *arg;
    int sema;
    int ok;
    int i;

    arg = &sif_fio_arg;
    sif_fio_lock(13);
    if (!sif_fio_inited)
        sceFsInit();
    for (i = 0; i < 1024; i++) {
        if (!(arg->name[i] = name[i]))
            break;
    }
    if (i == 1024) {
        arg->name[1023] = 0;
        i = 1023;
    }
    arg->stat = *(FioStatBuf *)stat;
    arg->mask = mask;
    sema_param.max_count = 1;
    sema_param.init_count = 0;
    sema_param.option = 0;
    sema = CreateSema(&sema_param);
    arg->rbuf = &result;
    arg->sema = sema;
    arg->rsize = 4;
    sceSifWriteBackDCache(&sif_fio_arg, 1104);
    if (sceSifCallRpc(&sif_fio_client, 13, 0, &sif_fio_arg, i + 81, sif_fio_ver_reply, 4, 0, 0) < 0) {
        DeleteSema(sema);
        sif_fio_unlock();
        return -11;
    }
    ok = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!ok) {
        DeleteSema(sema);
        return -11;
    }
    WaitSema(sema);
    DeleteSema(sema);
    return result;
}

/* -11 if the call fails. */
/* 0x001fbb38 */
static int sceRename(const char *name1, const char *name2)
{
    SemaParam sema_param;
    int result;
    FioPath2Arg *arg;
    int sema;
    int ok;
    int i;

    arg = &sif_fio_arg;
    sif_fio_lock(17);
    if (!sif_fio_inited)
        sceFsInit();
    for (i = 0; i < 1024; i++) {
        if (!(arg->name1[i] = name1[i]))
            break;
    }
    if (i == 1024)
        arg->name1[1023] = 0;
    for (i = 0; i < 1024; i++) {
        if (!(arg->name2[i] = name2[i]))
            break;
    }
    if (i == 1024)
        arg->name2[1023] = 0;
    sema_param.max_count = 1;
    sema_param.init_count = 0;
    sema_param.option = 0;
    sema = CreateSema(&sema_param);
    arg->rbuf = &result;
    arg->sema = sema;
    arg->rsize = 4;
    sceSifWriteBackDCache(&sif_fio_arg, 2060);
    if (sceSifCallRpc(&sif_fio_client, 17, 0, &sif_fio_arg, 2060, sif_fio_ver_reply, 4, 0, 0) < 0) {
        DeleteSema(sema);
        sif_fio_unlock();
        return -11;
    }
    ok = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!ok) {
        DeleteSema(sema);
        return -11;
    }
    WaitSema(sema);
    DeleteSema(sema);
    return result;
}

/* 0x001fbd28 */
static int sceChdir(const char *name)
{
    return sif_fio_path_call(name, 18);
}

/* -11 if the call fails. */
/* 0x001fbd48 */
static int sceSync(const char *name, int x)
{
    SemaParam sema_param;
    int result;
    FioArg *arg;
    int sema;
    int ok;
    int i;

    arg = &sif_fio_arg;
    sif_fio_lock(19);
    if (!sif_fio_inited)
        sceFsInit();
    for (i = 0; i < 1024; i++) {
        if (!(arg->name[i] = name[i]))
            break;
    }
    if (i == 1024)
        arg->name[1023] = 0;
    arg->param = x;
    sema_param.max_count = 1;
    sema_param.init_count = 0;
    sema_param.option = 0;
    sema = CreateSema(&sema_param);
    arg->rbuf = &result;
    arg->sema = sema;
    arg->rsize = 4;
    if (sceSifCallRpc(&sif_fio_client, 19, 0, &sif_fio_arg, 1044, sif_fio_ver_reply, 4, 0, 0) < 0) {
        DeleteSema(sema);
        sif_fio_unlock();
        return -11;
    }
    ok = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!ok) {
        DeleteSema(sema);
        return -11;
    }
    WaitSema(sema);
    DeleteSema(sema);
    return result;
}

/* Options at most 1024 (-7 otherwise); -11 if the call fails. */
/* 0x001fbee0 */
static int sceMount(const char *name1, const char *name2, int cmd, const char *data, int len)
{
    SemaParam sema_param;
    int result;
    FioCtl2Arg *arg;
    int sema;
    int ok;
    int i;

    arg = &sif_fio_arg;
    sif_fio_lock(20);
    if (!sif_fio_inited)
        sceFsInit();
    for (i = 0; i < 1024; i++) {
        if (!(arg->name1[i] = name1[i]))
            break;
    }
    if (i == 1024)
        arg->name1[1023] = 0;
    for (i = 0; i < 1024; i++) {
        if (!(arg->name2[i] = name2[i]))
            break;
    }
    if (i == 1024)
        arg->name2[1023] = 0;
    if (len > 1024) {
        sif_fio_unlock();
        return -7;
    }
    for (i = 0; i < len; i++)
        arg->data[i] = data[i];
    arg->len = len;
    arg->cmd = cmd;
    sema_param.max_count = 1;
    sema_param.init_count = 0;
    sema_param.option = 0;
    sema = CreateSema(&sema_param);
    arg->rbuf = &result;
    arg->sema = sema;
    arg->rsize = 4;
    sceSifWriteBackDCache(&sif_fio_arg, 3092);
    if (sceSifCallRpc(&sif_fio_client, 20, 0, &sif_fio_arg, 3092, sif_fio_ver_reply, 4, 0, 0) < 0) {
        DeleteSema(sema);
        sif_fio_unlock();
        return -11;
    }
    ok = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!ok) {
        DeleteSema(sema);
        return -11;
    }
    WaitSema(sema);
    DeleteSema(sema);
    return result;
}

/* 0x001fc150 */
static int sceUmount(const char *name)
{
    return sif_fio_path_call(name, 21);
}

/* 0x001fc170 */
static long sceLseek64(int fd, long offset, int whence)
{
    SemaParam sema_param;
    long result;
    SlotEntry *slot;
    FioSeek64Arg *arg;
    int sema;
    int state;
    int ok;
    int i;

    arg = &sif_fio_arg;
    slot = sif_fio_get_slot(fd);
    sif_fio_lock(22);
    if (!sif_fio_inited) {
        sif_fio_unlock();
        return -1;
    }
    if (!slot || !(state = slot->state)) {
        sif_fio_unlock();
        return -9;
    }
    arg->handle = slot->fd;
    arg->offset = offset;
    sema_param.init_count = 0;
    arg->whence = whence;
    sema_param.option = 0;
    arg->index = slot - sif_fio_slots;
    sema_param.max_count = 1;
    sema = CreateSema(&sema_param);
    sif_fio_arg.sema = sema;
    arg->rbuf = &result;
    arg->rsize = 8;
    if (state & 0x8000) {
        WaitSema(sif_fio_nowait_sema);
        for (i = 0; i < 32; i++) {
            if (sif_fio_nowait_semas[i] == -1) {
                sif_fio_nowait_semas[i] = arg->sema;
                arg->sema = -arg->sema;
                break;
            }
        }
        SignalSema(sif_fio_nowait_sema);
    }
    if (sceSifCallRpc(&sif_fio_client, 22, 0, &sif_fio_arg, 32, sif_fio_ver_reply, 4, 0, 0) < 0) {
        DeleteSema(sema);
        sif_fio_unlock();
        return -11;
    }
    ok = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!ok) {
        DeleteSema(sema);
        return -11;
    }
    if ((short)state & 0x8000) {
        DeleteSema(sema);
        return 0;
    }
    WaitSema(sema);
    DeleteSema(sema);
    return result;
}

/*
 * Buffers at most 1024. Bug: -22 for oversized buffers leaves the client lock held, as sceIoctl2.
 */
/* 0x001fc3a8 */
static int sceDevctl(const char *name, int cmd, const char *argdata, unsigned int arglen, void *buf,
                 unsigned int buflen)
{
    SemaParam sema_param;
    int result;
    FioIoctlArg *arg;
    int sema;
    int ok;
    unsigned int i;

    sif_fio_lock(23);
    arg = &sif_fio_arg;
    if (!sif_fio_inited)
        sceFsInit();
    for (i = 0; (int)i < 1024; i++) {
        if (!(arg->name[i] = name[i]))
            break;
    }
    if (i == 1024)
        arg->name[1023] = 0;
    if (arglen > 1024 || buflen > 1024)
        return -22;
    for (i = 0; i < arglen; i++)
        arg->data[i] = argdata[i];
    arg->arglen = arglen;
    arg->cmd = cmd;
    sema_param.max_count = 1;
    sema_param.init_count = 0;
    sema_param.option = 0;
    sema = CreateSema(&sema_param);
    arg->buflen = buflen;
    arg->rbuf = &result;
    arg->rsize = 4;
    arg->buf = buf;
    arg->sema = sema;
    sceSifWriteBackDCache(&sif_fio_arg, 2076);
    if (sceSifCallRpc(&sif_fio_client, 23, 0, &sif_fio_arg, 2076, sif_fio_ver_reply, 4, 0, 0) < 0) {
        DeleteSema(sema);
        sif_fio_unlock();
        return -11;
    }
    ok = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!ok) {
        DeleteSema(sema);
        return -11;
    }
    WaitSema(sema);
    DeleteSema(sema);
    return result;
}

/* name2 -> name1. -11 if the call fails. */
/* 0x001fc5d8 */
static int sceSymlink(const char *name1, const char *name2)
{
    SemaParam sema_param;
    int result;
    FioPath2Arg *arg;
    int sema;
    int ok;
    int i;

    arg = &sif_fio_arg;
    sif_fio_lock(17);
    if (!sif_fio_inited)
        sceFsInit();
    for (i = 0; i < 1024; i++) {
        if (!(arg->name1[i] = name1[i]))
            break;
    }
    if (i == 1024)
        arg->name1[1023] = 0;
    for (i = 0; i < 1024; i++) {
        if (!(arg->name2[i] = name2[i]))
            break;
    }
    if (i == 1024)
        arg->name2[1023] = 0;
    sema_param.max_count = 1;
    sema_param.init_count = 0;
    sema_param.option = 0;
    sema = CreateSema(&sema_param);
    arg->rbuf = &result;
    arg->sema = sema;
    arg->rsize = 4;
    if (sceSifCallRpc(&sif_fio_client, 24, 0, &sif_fio_arg, 2060, sif_fio_ver_reply, 4, 0, 0) < 0) {
        DeleteSema(sema);
        sif_fio_unlock();
        return -11;
    }
    ok = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!ok) {
        DeleteSema(sema);
        return -11;
    }
    WaitSema(sema);
    DeleteSema(sema);
    return result;
}

/* At most 1023 bytes. -11 if the call fails. */
/* 0x001fc7b8 */
static int sceReadlink(const char *name, char *buf, unsigned int size)
{
    SemaParam sema_param;
    int result;
    FioArg *arg;
    int sema;
    int ok;
    int i;

    arg = &sif_fio_arg;
    sif_fio_lock(17);
    if (!sif_fio_inited)
        sceFsInit();
    for (i = 0; i < 1024; i++) {
        if (!(arg->name[i] = name[i]))
            break;
    }
    if (i == 1024)
        arg->name[1023] = 0;
    if (size >= 1024)
        size = 1023;
    arg->param = (int)buf;
    arg->fd = size;
    sceSifWriteBackDCache(buf, size);
    sema_param.max_count = 1;
    sema_param.init_count = 0;
    sema_param.option = 0;
    sema = CreateSema(&sema_param);
    arg->rbuf = &result;
    arg->sema = sema;
    arg->rsize = 4;
    if (sceSifCallRpc(&sif_fio_client, 25, 0, &sif_fio_arg, 2060, sif_fio_ver_reply, 4, 0, 0) < 0) {
        DeleteSema(sema);
        sif_fio_unlock();
        return -11;
    }
    ok = *(int *)((unsigned int)sif_fio_ver_reply | 0x20000000);
    sif_fio_unlock();
    if (!ok) {
        DeleteSema(sema);
        return -11;
    }
    WaitSema(sema);
    DeleteSema(sema);
    return result;
}
