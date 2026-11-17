/*
 * Memory card library, EE side: remote calls to the IOP card server. Commands but sceMcInit run in
 * the background under the library lock (sceMcSync). 0 started, -200 busy, -100 not initialised,
 * -210 empty name.
 */

#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "sdk_compat.h"
#include "libc.h"
#include "libmc.h"
#include "kernel_syscalls.h"
#include "libkernl_patch.h"
#include "libkernl_sifcmd.h"
#include "libkernl_sifrpc.h"

typedef struct {
    int n0;
    int n1;
    unsigned char *dst0;
    unsigned char *dst1;
    unsigned char data0[64];
    unsigned char data1[64];
} McRecvPkt;

typedef struct {
    int fd;
    char pad_04[8];
    int size;
    char pad_10[4];
    unsigned int head;
    unsigned char *buf;
    char pad_1c[4];
    unsigned char data[16];
} McWriteCmd;

typedef struct {
    char pad_00[4];
    int port;
    int slot;
    int format;
    int free;
    int type;
    char pad_18[4];
    void *reply;
    char pad_20[16];
} McInfoCmd;

#define UNCACHED(p) ((char *)((unsigned int)(p) | 0x20000000))

extern int libmc_sema;
extern SifClientA libmc_client;
extern int libmc_cmd_buf[12];
extern int libmc_reply_buf[16];
extern int CreateSema(SemaParamB *p);

extern int libmc_cur_cmd;
extern char libmc_bounce_buf[];
extern int SetAlarm(int cause, void (*fn)(int, void *, int), int arg);
extern int *libmc_info_type;
extern int *libmc_info_free;
extern int *libmc_info_format;
extern McNameCmd libmc_name_cmd;
extern char libmc_dir_buf[];
extern McTblEntry libmc_file_info;

/*
 * Finishes any running command; a failed bind stops for good. Server answer, call error - 100,
 * or -120/-121 when mcserv.irx/mcman.irx is too old.
 */
/* steer(cast) */
/* 0x001e3558 */
int sceMcInit(void)
{
    int i;
    int ret;
    SemaParamB sema_param;

    if (libmc_sema < 0) {
        sema_param.option = 0;
        sema_param.init = 1;
        sema_param.max = 1;
        libmc_sema = CreateSema(&sema_param);
    }
    sceMcSync(0, 0, 0);
    WaitSema(libmc_sema);
    ((void (*)(int))sceSifInitRpc)(0);
    while (1) {
        if (sceSifBindRpc(&libmc_client, 0x80000400, 0) < 0) {
            printf("bind error libmc \n");
            while (1)
                ;
        }
        if (libmc_client.server != 0)
            break;
        i = 0x100000;
        while (--i)
            ;
    }
    ret = sceSifCallRpc(&libmc_client, 254, 0, libmc_cmd_buf, 48, libmc_reply_buf, 12, 0, 0);
    SignalSema(libmc_sema);
    if (ret < 0) {
        libmc_client.server = 0;
        return ret - 100;
    }
    if (libmc_reply_buf[1] < 522) {
        printf("libmc: too old release of mcserv.irx\n");
        libmc_client.server = 0;
        return -120;
    }
    if (libmc_reply_buf[2] < 526) {
        printf("libmc: too old release of mcman.irx\n");
        libmc_client.server = 0;
        return -121;
    }
    return libmc_reply_buf[0];
}

/* Out: reply buffer and running-command word; lock semaphore id at the end of the reply buffer. */
/* 0x001e3710 */
static SifClientA *libmc_get_rpc(int **reply, int **cmd)
{
    *reply = libmc_reply_buf;
    *cmd = &libmc_cur_cmd;
    libmc_reply_buf[15] = libmc_sema;
    return &libmc_client;
}

/* In the background. */
/* 0x001e3740 */
static int libmc_rpc20(int param)
{
    SifClientA *cd;
    int ret;

    if (PollSema(libmc_sema) < 0)
        return -200;
    cd = &libmc_client;
    if (cd->server == 0) {
        SignalSema(libmc_sema);
        return -100;
    }
    libmc_cmd_buf[5] = param;
    ret = sceSifCallRpc(cd, 20, 1, libmc_cmd_buf, 48, libmc_reply_buf, 4, 0, 0);
    if (ret == 0)
        libmc_cur_cmd = 20;
    else
        SignalSema(libmc_sema);
    return ret;
}

/* Waits; returns the server's answer. */
/* 0x001e37f8 */
static int libmc_rpc21(int param)
{
    SifClientA *cd;
    int ret;

    if (PollSema(libmc_sema) < 0)
        return -200;
    cd = &libmc_client;
    if (cd->server == 0) {
        SignalSema(libmc_sema);
        return -100;
    }
    libmc_cmd_buf[1] = param;
    ret = sceSifCallRpc(cd, 21, 0, libmc_cmd_buf, 48, libmc_reply_buf, 4, 0, 0);
    if (ret != 0) {
        SignalSema(libmc_sema);
        return ret;
    }
    SignalSema(libmc_sema);
    return libmc_reply_buf[0];
}

/* name cut to 1023. */
/* 0x001e38b8 */
int sceMcOpen(int port, int slot, const char *name, int mode)
{
    SifClientA *cd;
    McOpenCmd *cmd;
    int ret;

    if (PollSema(libmc_sema) < 0)
        return -200;
    cd = &libmc_client;
    if (cd->server == 0) {
        SignalSema(libmc_sema);
        return -100;
    }
    if (name == 0 || name[0] == 0) {
        SignalSema(libmc_sema);
        return -210;
    }
    strncpy(libmc_name_cmd.name, name, 1023);
    cmd = &libmc_name_cmd;
    cmd->port = port;
    cmd->mode = mode;
    cmd->slot = slot;
    cmd->name[1023] = 0;
    ret = sceSifCallRpc(cd, 2, 1, cmd, 1044, libmc_reply_buf, 4, 0, 0);
    if (ret == 0)
        libmc_cur_cmd = 2;
    else
        SignalSema(libmc_sema);
    return ret;
}

/* An open with the create-directory mode. */
/* 0x001e39f0 */
int sceMcMkdir(int port, int slot, const char *name)
{
    int ret;

    ret = sceMcOpen(port, slot, name, 64);
    if (ret == 0)
        libmc_cur_cmd = 11;
    return ret;
}

/* 0x001e3a28 */
int sceMcClose(int fd)
{
    SifClientA *cd;
    int ret;

    if (PollSema(libmc_sema) < 0)
        return -200;
    cd = &libmc_client;
    if (cd->server == 0) {
        SignalSema(libmc_sema);
        return -100;
    }
    libmc_cmd_buf[0] = fd;
    ret = sceSifCallRpc(cd, 3, 1, libmc_cmd_buf, 48, libmc_reply_buf, 4, 0, 0);
    if (ret == 0)
        libmc_cur_cmd = 3;
    else
        SignalSema(libmc_sema);
    return ret;
}

/* 0x001e3ae0 */
int sceMcSeek(int fd, int offset, int origin)
{
    SifClientA *cd;
    int ret;

    if (PollSema(libmc_sema) < 0)
        return -200;
    cd = &libmc_client;
    if (cd->server == 0) {
        SignalSema(libmc_sema);
        return -100;
    }
    libmc_cmd_buf[0] = fd;
    libmc_cmd_buf[4] = offset;
    libmc_cmd_buf[5] = origin;
    ret = sceSifCallRpc(cd, 4, 1, libmc_cmd_buf, 48, libmc_reply_buf, 4, 0, 0);
    if (ret == 0)
        libmc_cur_cmd = 4;
    else
        SignalSema(libmc_sema);
    return ret;
}

/* Copies the unaligned start and end into the caller's buffer. */
/* 0x001e3bb8 */
static void libmc_read_done(unsigned int addr)
{
    McRecvPkt *pkt;
    unsigned char *dst;
    int i;

    pkt = (McRecvPkt *)(addr | 0x20000000);
    if (pkt->n0 != 0) {
        dst = pkt->dst0;
        for (i = 0; i < pkt->n0; i++)
            *dst++ = pkt->data0[i];
    }
    if (pkt->n1 != 0) {
        dst = pkt->dst1;
        for (i = 0; i < pkt->n1; i++)
            *dst++ = pkt->data1[i];
    }
}

/* Server DMAs whole quadwords; unaligned ends via libmc_bounce_buf (libmc_read_done). */
/* 0x001e3c48 */
int sceMcRead(int fd, void *buf, int size)
{
    SifClientA *cd;
    int ret;

    if (PollSema(libmc_sema) < 0)
        return -200;
    cd = &libmc_client;
    if (cd->server == 0) {
        SignalSema(libmc_sema);
        return -100;
    }
    libmc_cmd_buf[0] = fd;
    libmc_cmd_buf[7] = (int)libmc_bounce_buf;
    libmc_cmd_buf[6] = (int)buf;
    libmc_cmd_buf[3] = size;
    sceSifWriteBackDCache(buf, size);
    sceSifWriteBackDCache(libmc_bounce_buf, 192);
    ret = sceSifCallRpc(cd, 5, 1, libmc_cmd_buf, 48, libmc_reply_buf, 4, libmc_read_done, libmc_bounce_buf);
    if (ret == 0)
        libmc_cur_cmd = 5;
    else
        SignalSema(libmc_sema);
    return ret;
}

/* Bytes before buf's first 16-byte boundary (or all, if 16 or less) go in the command. */
/* 0x001e3d68 */
int sceMcWrite(int fd, unsigned char *buf, int size)
{
    unsigned int i;
    int ret;

    if (PollSema(libmc_sema) < 0)
        return -200;
    if (libmc_client.server == 0) {
        SignalSema(libmc_sema);
        return -100;
    }
    (*(McWriteCmd *)&libmc_cmd_buf).fd = fd; /* steer(cast) */
    if (size < 17) {
        (*(McWriteCmd *)&libmc_cmd_buf).head = size; /* steer(cast) */
        (*(McWriteCmd *)&libmc_cmd_buf).size = 0; /* steer(cast) */
        (*(McWriteCmd *)&libmc_cmd_buf).buf = 0; /* steer(cast) */
    } else {
        {
            int head = (int)buf - 16;
            (*(McWriteCmd *)&libmc_cmd_buf).head = (((int)buf - 1) & 0xfffffff0) - head; /* steer(cast) */
        }
        (*(McWriteCmd *)&libmc_cmd_buf).size = size - (*(McWriteCmd *)&libmc_cmd_buf).head; /* steer(cast) */
        (*(McWriteCmd *)&libmc_cmd_buf).buf = buf + (*(McWriteCmd *)&libmc_cmd_buf).head; /* steer(cast) */
    }
    for (i = 0; i < (*(McWriteCmd *)&libmc_cmd_buf).head; i++) /* steer(cast) */
        (*(McWriteCmd *)&libmc_cmd_buf).data[i] = buf[i]; /* steer(cast) */
    FlushCache(0);
    ret = sceSifCallRpc(&libmc_client, 6, 1, &(*(McWriteCmd *)&libmc_cmd_buf), 48, libmc_reply_buf, 4, 0, 0); /* steer(cast) */
    if (ret == 0)
        libmc_cur_cmd = 6;
    else
        SignalSema(libmc_sema);
    return ret;
}

/* Wakes thread tid. */
/* 0x001e3ee8 */
static void libmc_int_handler(int ch, void *cb_arg, int tid)
{
    iWakeupThread(tid);
    ExitHandler();
}

/* cause kernel alarm ticks (hblank periods). */
/* 0x001e3f10 */
static void libmc_wait_int(unsigned short cause)
{
    SetAlarm(cause, libmc_int_handler, GetThreadId());
    SleepThread();
}

/* Mode 0 waits. Releases the lock when done. 1 finished, 0 running, -1 no command. */
/* 0x001e3f58 */
int sceMcSync(int mode, int *cmd, int *result)
{
    int ret;

    if (libmc_cur_cmd == 0)
        return -1;
    ret = sceSifCheckStatRpc(&libmc_client);
    if (mode == 0 && ret != 0) {
        while (sceSifCheckStatRpc(&libmc_client) != 0)
            libmc_wait_int(60);
        ret = 0;
    }
    ret = ret == 0;
    if (cmd != 0)
        *cmd = libmc_cur_cmd;
    if (ret != 0) {
        libmc_cur_cmd = 0;
        if (result != 0)
            *result = libmc_reply_buf[0];
        SignalSema(libmc_sema);
    }
    return ret;
}

/* Stores type, free space and format state. */
/* 0x001e4038 */
static void libmc_get_info_done(unsigned int addr)
{
    int *res;

    res = (int *)(addr | 0x20000000);
    if (libmc_info_type != 0)
        *libmc_info_type = res[0];
    if (libmc_info_free != 0)
        *libmc_info_free = res[1];
    if (libmc_info_format != 0)
        *libmc_info_format = res[36];
}

/* Free space in clusters; any pointer may be 0. Stored when the command finishes. */
/* 0x001e4090 */
int sceMcGetInfo(int port, int slot, int *type, int *free, int *format)
{
    int ret;

    if (PollSema(libmc_sema) < 0)
        return -200;
    if (libmc_client.server == 0) {
        SignalSema(libmc_sema);
        return -100;
    }
    (*(McInfoCmd *)&libmc_cmd_buf).port = port; /* steer(cast) */
    (*(McInfoCmd *)&libmc_cmd_buf).slot = slot; /* steer(cast) */
    (*(McInfoCmd *)&libmc_cmd_buf).reply = libmc_bounce_buf; /* steer(cast) */
    if (type)
        (*(McInfoCmd *)&libmc_cmd_buf).type = 1; /* steer(cast) */
    else
        (*(McInfoCmd *)&libmc_cmd_buf).type = 0; /* steer(cast) */
    if (free)
        (*(McInfoCmd *)&libmc_cmd_buf).free = 1; /* steer(cast) */
    else
        (*(McInfoCmd *)&libmc_cmd_buf).free = 0; /* steer(cast) */
    if (format)
        (*(McInfoCmd *)&libmc_cmd_buf).format = 1; /* steer(cast) */
    else
        (*(McInfoCmd *)&libmc_cmd_buf).format = 0; /* steer(cast) */
    libmc_info_type = type;
    libmc_info_free = free;
    libmc_info_format = format;
    sceSifWriteBackDCache(libmc_bounce_buf, 192);
    ret = sceSifCallRpc(&libmc_client, 1, 1, &(*(McInfoCmd *)&libmc_cmd_buf), 48, libmc_reply_buf, 4, libmc_get_info_done, libmc_bounce_buf); /* steer(cast) */
    if (ret == 0)
        libmc_cur_cmd = 1;
    else
        SignalSema(libmc_sema);
    return ret;
}

/* Wildcards allowed; at most maxent 64-byte entries; flags 0 starts a new listing. */
/* 0x001e4218 */
int sceMcGetDir(int port, int slot, const char *name, int flags, int maxent, void *table)
{
    SifClientA *cd;
    McNameCmd *cmd;
    int ret;

    if (PollSema(libmc_sema) < 0)
        return -200;
    cd = &libmc_client;
    if (cd->server == 0) {
        SignalSema(libmc_sema);
        return -100;
    }
    if (name == 0 || name[0] == 0) {
        SignalSema(libmc_sema);
        return -210;
    }
    cmd = &libmc_name_cmd;
    cmd->port = port;
    cmd->slot = slot;
    cmd->flags = flags;
    cmd->maxent = maxent;
    cmd->table = table;
    strncpy(cmd->name, name, 1023);
    cmd->name[1023] = 0;
    if (maxent >= 0)
        sceSifWriteBackDCache(table, maxent << 6);
    ret = sceSifCallRpc(cd, 13, 1, cmd, 1044, libmc_reply_buf, 4, 0, 0);
    if (ret == 0)
        libmc_cur_cmd = 13;
    else
        SignalSema(libmc_sema);
    return ret;
}

/* Previous directory (cut to 1023) into buf. */
/* 0x001e4370 */
static void libmc_chdir_done(char *buf)
{
    int n;

    if (buf != 0) {
        n = strlen(UNCACHED(libmc_dir_buf)) < 1024 ? strlen(UNCACHED(libmc_dir_buf)) : 1023;
        memcpy(buf, UNCACHED(libmc_dir_buf), n);
        buf[n] = 0;
    }
}

/* With cur, the previous directory is copied there when done. */
/* 0x001e43f8 */
int sceMcChdir(int port, int slot, const char *name, char *cur)
{
    SifClientA *cd;
    McNameCmd *cmd;
    int ret;

    if (PollSema(libmc_sema) < 0)
        return -200;
    cd = &libmc_client;
    if (cd->server == 0) {
        SignalSema(libmc_sema);
        return -100;
    }
    if (name == 0 || name[0] == 0) {
        SignalSema(libmc_sema);
        return -210;
    }
    cmd = &libmc_name_cmd;
    cmd->port = port;
    cmd->slot = slot;
    cmd->table = libmc_dir_buf;
    strncpy(cmd->name, name, 1023);
    cmd->name[1023] = 0;
    sceSifWriteBackDCache(libmc_dir_buf, 1024);
    ret = sceSifCallRpc(cd, 12, 1, cmd, 1044, libmc_reply_buf, 4, libmc_chdir_done, cur);
    if (ret == 0)
        libmc_cur_cmd = 12;
    else
        SignalSema(libmc_sema);
    return ret;
}

/* 0x001e4548 */
int sceMcFormat(int port, int slot)
{
    SifClientA *cd;
    int ret;

    if (PollSema(libmc_sema) < 0)
        return -200;
    cd = &libmc_client;
    if (cd->server == 0) {
        SignalSema(libmc_sema);
        return -100;
    }
    libmc_cmd_buf[1] = port;
    libmc_cmd_buf[2] = slot;
    ret = sceSifCallRpc(cd, 16, 1, libmc_cmd_buf, 48, libmc_reply_buf, 4, 0, 0);
    if (ret == 0)
        libmc_cur_cmd = 16;
    else
        SignalSema(libmc_sema);
    return ret;
}

/* File or empty directory. */
/* 0x001e4610 */
int sceMcDelete(int port, int slot, const char *name)
{
    SifClientA *cd;
    McOpenCmd *cmd;
    int ret;

    if (PollSema(libmc_sema) < 0)
        return -200;
    cd = &libmc_client;
    if (cd->server == 0) {
        SignalSema(libmc_sema);
        return -100;
    }
    if (name == 0 || name[0] == 0) {
        SignalSema(libmc_sema);
        return -210;
    }
    strncpy(libmc_name_cmd.name, name, 1023);
    cmd = &libmc_name_cmd;
    cmd->port = port;
    cmd->name[1023] = 0;
    cmd->mode = 0;
    cmd->slot = slot;
    ret = sceSifCallRpc(cd, 15, 1, cmd, 1044, libmc_reply_buf, 4, 0, 0);
    if (ret == 0)
        libmc_cur_cmd = 15;
    else
        SignalSema(libmc_sema);
    return ret;
}

/* 0x001e4738 */
static int sceMcFlush(int fd)
{
    SifClientA *cd;
    int ret;

    if (PollSema(libmc_sema) < 0)
        return -200;
    cd = &libmc_client;
    if (cd->server == 0) {
        SignalSema(libmc_sema);
        return -100;
    }
    libmc_cmd_buf[0] = fd;
    ret = sceSifCallRpc(cd, 10, 1, libmc_cmd_buf, 48, libmc_reply_buf, 4, 0, 0);
    if (ret == 0)
        libmc_cur_cmd = 10;
    else
        SignalSema(libmc_sema);
    return ret;
}

/* flags selects creation time, modification time, attributes. */
/* 0x001e47f0 */
int sceMcSetFileInfo(int port, int slot, const char *name, McTblEntry *info, unsigned int flags)
{
    SifClientA *cd;
    McNameCmd *cmd;
    int ret;

    if (PollSema(libmc_sema) < 0)
        return -200;
    cd = &libmc_client;
    if (cd->server == 0) {
        SignalSema(libmc_sema);
        return -100;
    }
    if (name == 0 || name[0] == 0) {
        SignalSema(libmc_sema);
        return -210;
    }
    cmd = &libmc_name_cmd;
    cmd->port = port;
    cmd->slot = slot;
    flags &= 7;
    cmd->flags = flags;
    libmc_file_info = *info;
    cmd->table = &libmc_file_info;
    strncpy(cmd->name, name, 1023);
    cmd->name[1023] = 0;
    FlushCache(0);
    ret = sceSifCallRpc(cd, 14, 1, cmd, 1044, libmc_reply_buf, 4, 0, 0);
    if (ret == 0)
        libmc_cur_cmd = 14;
    else
        SignalSema(libmc_sema);
    return ret;
}

/*
 * newname at most 31; an entry change of the name field, reported by sceMcSync as its own command.
 */
/* 0x001e49c8 */
int sceMcRename(int port, int slot, const char *name, const char *newname)
{
    SifClientA *cd;
    McNameCmd *cmd;
    int ret;

    if (PollSema(libmc_sema) < 0)
        return -200;
    cd = &libmc_client;
    if (cd->server == 0) {
        SignalSema(libmc_sema);
        return -100;
    }
    if (name == 0 || name[0] == 0 || newname == 0) {
        SignalSema(libmc_sema);
        return -210;
    }
    cmd = &libmc_name_cmd;
    cmd->port = port;
    cmd->slot = slot;
    cmd->flags = 16;
    strncpy(cmd->name, name, 1023);
    cmd->name[1023] = 0;
    strncpy(libmc_file_info.name, newname, 32);
    libmc_file_info.name[31] = 0;
    cmd->table = &libmc_file_info;
    FlushCache(0);
    ret = sceSifCallRpc(cd, 14, 1, cmd, 1044, libmc_reply_buf, 4, 0, 0);
    if (ret == 0)
        libmc_cur_cmd = 19;
    else
        SignalSema(libmc_sema);
    return ret;
}

/* 0x001e4b30 */
int sceMcUnformat(int port, int slot)
{
    SifClientA *cd;
    int ret;

    if (PollSema(libmc_sema) < 0)
        return -200;
    cd = &libmc_client;
    if (cd->server == 0) {
        SignalSema(libmc_sema);
        return -100;
    }
    libmc_cmd_buf[1] = port;
    libmc_cmd_buf[2] = slot;
    ret = sceSifCallRpc(cd, 17, 1, libmc_cmd_buf, 48, libmc_reply_buf, 4, 0, 0);
    if (ret == 0)
        libmc_cur_cmd = 17;
    else
        SignalSema(libmc_sema);
    return ret;
}

/* Entries the directory can still hold. */
/* 0x001e4bf8 */
static int sceMcGetEntSpace(int port, int slot, const char *name)
{
    SifClientA *cd;
    McNameCmd *cmd;
    int ret;

    if (PollSema(libmc_sema) < 0)
        return -200;
    cd = &libmc_client;
    if (cd->server == 0) {
        SignalSema(libmc_sema);
        return -100;
    }
    if (name == 0 || name[0] == 0) {
        SignalSema(libmc_sema);
        return -210;
    }
    cmd = &libmc_name_cmd;
    cmd->port = port;
    cmd->slot = slot;
    strncpy(cmd->name, name, 1023);
    cmd->name[1023] = 0;
    ret = sceSifCallRpc(cd, 18, 1, cmd, 1044, libmc_reply_buf, 4, 0, 0);
    if (ret == 0)
        libmc_cur_cmd = 18;
    else
        SignalSema(libmc_sema);
    return ret;
}
