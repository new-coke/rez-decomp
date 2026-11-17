/*
 * IOP module loader client: load, start, stop, find modules; read/write IOP memory. Every call
 * returns a negative error if binding, the version check or the remote call fails.
 */

#include "types_libc.h"
#include "types_sce.h"
#include "libc.h"
#include "libkernl_loadfile.h"
#include "libkernl_sifrpc.h"
#include "libkernl_fileio.h"

extern int sif_loadfile_inited;
extern RpcPkt sif_loadfile_client;
extern char sif_loadfile_peer_ver[];

extern char *sif_loadfile_ver_tag2;
extern LoadArg sif_loadfile_arg;

/* Busy-waits for the bind; version tag into sif_loadfile_peer_ver. -1 if binding fails. */
/* 0x001fcc80 */
static int sif_loadfile_init(void)
{
    int i;

    if (sif_loadfile_inited < 0) {
        for (;;) {
            if (sceSifBindRpc(&sif_loadfile_client, 0x80000006, 0) < 0)
                return -1;
            if (sif_loadfile_client.server) {
                sif_loadfile_inited = 0;
                if (sceSifCallRpc(&sif_loadfile_client, 255, 0, 0, 0, ((char *)&sif_loadfile_arg), 4, 0, 0) < 0) /* steer(cast) */
                    return 0xfffeffff;
                *(Bytes4 *)sif_loadfile_peer_ver = *(Bytes4 *)((char *)&sif_loadfile_arg); /* steer(cast) */
                return 0;
            }
            i = 0x100000;
            while (i--)
                ;
        }
    }
    return 0;
}

/* 1 when the tag is neither libkernl_ver_tag ("2300") nor the alternative, unless those match. */
/* 0x001fcd80 */
static int sif_loadfile_version_mismatch(void)
{
    int ret = 0;
    char *ver = libkernl_ver_tag;
    char *tag = sif_loadfile_peer_ver;

    if (memcmp(tag, ver, 4) && memcmp(tag, sif_loadfile_ver_tag2, 4))
        ret = memcmp(ver, sif_loadfile_ver_tag2, 4) != 0;
    return ret;
}

/* Needed after an IOP reboot; the next call binds again. */
/* 0x001fce10 */
int sceSifLoadFileReset(void)
{
    sif_loadfile_inited = -1;
    memset(sif_loadfile_peer_ver, 0, 4);
    return 0;
}

/* Arguments cut to 252; module result in *result. Returns the id. */
/* 0x001fce48 */
static int sif_loadfile_start_module(int id, int len, const char *args, int *result)
{
    LoadArg *arg;
    int ret;

    if (sif_loadfile_init() < 0)
        return 0xffff0000;
    if (sif_loadfile_version_mismatch())
        return 0xfffefffc;
    arg = &sif_loadfile_arg;
    sif_loadfile_arg.id = id;
    if (args) {
        if (len > 252) {
            *(SifArgBuf *)arg->args = *(SifArgBuf *)args;
            sif_loadfile_arg.len = 252;
        } else {
            memcpy(arg->args, args, len);
            arg->len = len;
        }
    } else {
        arg->len = 0;
    }
    if (sceSifCallRpc(&sif_loadfile_client, 6, 0, &sif_loadfile_arg, 512, &sif_loadfile_arg, 8, 0, 0) < 0)
        return 0xfffeffff;
    ret = sif_loadfile_arg.id;
    *result = sif_loadfile_arg.len;
    return ret;
}

/* Arguments cut to 252; module result in *result. Returns the id. */
/* 0x001fd050 */
static int sceSifStopModule(int id, int len, const char *args, int *result)
{
    LoadArg *arg;
    int ret;

    if (sif_loadfile_init() < 0)
        return 0xffff0000;
    if (sif_loadfile_version_mismatch())
        return 0xfffefffc;
    arg = &sif_loadfile_arg;
    sif_loadfile_arg.id = id;
    if (args) {
        if (len > 252) {
            *(SifArgBuf *)arg->args = *(SifArgBuf *)args;
            sif_loadfile_arg.len = 252;
        } else {
            memcpy(arg->args, args, len);
            arg->len = len;
        }
    } else {
        arg->len = 0;
    }
    if (sceSifCallRpc(&sif_loadfile_client, 7, 0, &sif_loadfile_arg, 512, &sif_loadfile_arg, 8, 0, 0) < 0)
        return 0xfffeffff;
    ret = sif_loadfile_arg.id;
    *result = sif_loadfile_arg.len;
    return ret;
}

/* 0x001fd258 */
static int sceSifUnloadModule(int id)
{
    if (sif_loadfile_init() < 0)
        return 0xffff0000;
    if (sif_loadfile_version_mismatch())
        return 0xfffefffc;
    sif_loadfile_arg.id = id;
    if (sceSifCallRpc(&sif_loadfile_client, 8, 0, &sif_loadfile_arg, 4, &sif_loadfile_arg, 4, 0, 0) < 0)
        return 0xfffeffff;
    return sif_loadfile_arg.id;
}

/* name cut to 251. */
/* 0x001fd2e8 */
static int sceSifSearchModuleByName(const char *name)
{
    if (sif_loadfile_init() < 0)
        return 0xffff0000;
    if (sif_loadfile_version_mismatch())
        return 0xfffefffc;
    strncpy(sif_loadfile_arg.path, name, 252);
    sif_loadfile_arg.path[251] = 0;
    if (sceSifCallRpc(&sif_loadfile_client, 9, 0, &sif_loadfile_arg, 512, &sif_loadfile_arg, 4, 0, 0) < 0)
        return 0xfffeffff;
    return sif_loadfile_arg.id;
}

/* id holds the IOP address. */
/* 0x001fd388 */
static int sceSifSearchModuleByAddress(int id)
{
    if (sif_loadfile_init() < 0)
        return 0xffff0000;
    if (sif_loadfile_version_mismatch())
        return 0xfffefffc;
    sif_loadfile_arg.id = id;
    if (sceSifCallRpc(&sif_loadfile_client, 10, 0, &sif_loadfile_arg, 4, &sif_loadfile_arg, 4, 0, 0) < 0)
        return 0xfffeffff;
    return sif_loadfile_arg.id;
}

/* 0x001fd418 */
int sif_loadfile_start_module_noresult(int id, int len, const char *args)
{
    int result;

    return sif_loadfile_start_module(id, len, args, &result);
}

/* 0x001fd438 */
static int sif_loadfile_start_module2(int id, int len, const char *args, int *result)
{
    return sif_loadfile_start_module(id, len, args, result);
}

/*
 * Path cut to 251, arguments to 252, loader function fno; module result in *result. Returns the id.
 */
/* 0x001fd458 */
static int sif_loadfile_load_start(const char *path, int len, const char *args, int *result, int fno)
{
    int ret;

    if (sif_loadfile_init() < 0)
        return 0xffff0000;
    if (sif_loadfile_version_mismatch())
        return 0xfffefffc;
    strncpy(sif_loadfile_arg.path, path, 252);
    sif_loadfile_arg.path[251] = 0;
    if (args) {
        if (len > 252) {
            *(SifArgBuf *)sif_loadfile_arg.args = *(SifArgBuf *)args;
            sif_loadfile_arg.id = 252;
        } else {
            memcpy(sif_loadfile_arg.args, args, len);
            sif_loadfile_arg.id = len;
        }
    } else {
        sif_loadfile_arg.args[0] = 0;
        sif_loadfile_arg.id = 0;
    }
    if (sceSifCallRpc(&sif_loadfile_client, fno, 0, &sif_loadfile_arg, 512, &sif_loadfile_arg, 8, 0, 0) < 0)
        return 0xfffeffff;
    ret = sif_loadfile_arg.id;
    *result = sif_loadfile_arg.len;
    return ret;
}

/* Returns the module id. */
/* 0x001fd680 */
int sceSifLoadModule(const char *path, int len, const char *args)
{
    int result;

    return sif_loadfile_load_start(path, len, args, &result, 0);
}

/* 0x001fd6a0 */
static int sceSifLoadStartModule(const char *path, int len, const char *args, int *result)
{
    return sif_loadfile_load_start(path, len, args, result, 0);
}

/* Strings cut to 251; replies in out[0], out[1]. Error if the call fails or out[0] is 0. */
/* 0x001fd6c0 */
static int sif_loadfile_call_2str(const char *name1, const char *name2, int *out, int fno)
{
    if (sif_loadfile_init() < 0)
        return 0xffff0000;
    if (sif_loadfile_version_mismatch())
        return 0xfffefffc;
    strncpy(sif_loadfile_arg.path, name1, 252);
    sif_loadfile_arg.path[251] = 0;
    strncpy(sif_loadfile_arg.args, name2, 252);
    sif_loadfile_arg.args[251] = 0;
    if (sceSifCallRpc(&sif_loadfile_client, fno, 0, &sif_loadfile_arg, 512, &sif_loadfile_arg, 16, 0, 0) < 0)
        return 0xfffeffff;
    if (!sif_loadfile_arg.id)
        return 0xfffefffd;
    out[0] = sif_loadfile_arg.id;
    out[1] = sif_loadfile_arg.len;
    return 0;
}

/* Section name2 of EE program name1. */
/* 0x001fd7c8 */
static int sceSifLoadElfPart(const char *name1, const char *name2, int *out)
{
    return sif_loadfile_call_2str(name1, name2, out, 1);
}

/* Every section, into EE memory. */
/* 0x001fd7e8 */
static int sceSifLoadElf(const char *name, int *out)
{
    return sif_loadfile_call_2str(name, "all", out, 1);
}

/* width 0/1/2 = byte/halfword/word. Error for a bad width or failed call. */
/* 0x001fd810 */
static int sif_loadfile_peek(int addr, void *dst, unsigned int width)
{
    if (sif_loadfile_init() < 0)
        return 0xffff0000;
    if (width >= 3)
        return 0xfffefffe;
    (*(IopMemArg *)&sif_loadfile_arg).addr = addr; /* steer(cast) */
    (*(IopMemArg *)&sif_loadfile_arg).width = width; /* steer(cast) */
    if (sceSifCallRpc(&sif_loadfile_client, 3, 0, &(*(IopMemArg *)&sif_loadfile_arg), 32, &(*(IopMemArg *)&sif_loadfile_arg), 32, 0, 0) < 0) /* steer(cast) */
        return 0xfffeffff;
    if (width == 0)
        *(unsigned char *)dst = *(unsigned char *)&(*(IopMemArg *)&sif_loadfile_arg).addr; /* steer(cast) */
    else if (width == 1)
        *(unsigned short *)dst = *(unsigned short *)&(*(IopMemArg *)&sif_loadfile_arg).addr; /* steer(cast) */
    else if (width == 2)
        *(int *)dst = (*(IopMemArg *)&sif_loadfile_arg).addr; /* steer(cast) */
    else
        return 0xfffefffe;
    return 0;
}

/* width 0/1/2 = byte/halfword/word. Error for a bad width or failed call. */
/* 0x001fd900 */
static int sif_loadfile_poke(int addr, void *src, unsigned int width)
{
    IopMemArg *arg;

    if (sif_loadfile_init() < 0)
        return 0xffff0000;
    arg = &(*(IopMemArg *)&sif_loadfile_arg); /* steer(cast) */
    (*(IopMemArg *)&sif_loadfile_arg).addr = addr; /* steer(cast) */
    arg->width = width;
    if (width == 0)
        *(unsigned char *)&arg->value = *(unsigned char *)src;
    else if (width == 1)
        *(unsigned short *)&arg->value = *(unsigned short *)src;
    else if (width == 2)
        arg->value = *(int *)src;
    else
        return 0xfffefffe;
    if (sceSifCallRpc(&sif_loadfile_client, 2, 0, &(*(IopMemArg *)&sif_loadfile_arg), 32, &(*(IopMemArg *)&sif_loadfile_arg), 16, 0, 0) < 0) /* steer(cast) */
        return 0xfffeffff;
    return 0;
}
