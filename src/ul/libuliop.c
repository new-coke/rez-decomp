/*
 * IOP boot and module loader: reboots the IOP with the game's image, picks the boot device and
 * loads the .irx drivers for sound, memory card and disc, tracking loaded modules.
 */

#include "types_gw.h"
#include "libuliop.h"
#include "sdk_compat.h"
#include <stdarg.h>
#include "libc.h"
#include "kernel_syscalls.h"
#include "libcdvd.h"
#include "libkernl_fileio.h"
#include "libkernl_iopheap.h"
#include "libkernl_iopreset.h"
#include "libkernl_loadfile.h"
#include "libkernl_sifcmd.h"
#include "libkernl_tty.h"
#include "ul_console.h"

static int uliop_load_module(char *name, int arglen, int args);
static int uliop_find_module(char *name);
static int uliop_set_error_callback(void (*cb)(int));
static void uliop_report_error(int err);
static void uliop_error_callback(int err);
static void uliop_printf(char *fmt, ...);

extern char uliop_path_buf[];
extern char uliop_work_path[];

/*
 * Into uliop_path_buf. Disc (dev 0): upper-case, '/' -> '\\', ";1" added without a ';' suffix
 * (ISO 9660). Other devices: '\\' -> '/'.
 */
/* 0x0018ad18 */
static char *uliop_normalize_path(char *path)
{
    int i;
    int found;
    int c;
    char *cur;

    strcpy(uliop_path_buf, path);
    if (uliop_work.dev == 0) {
        i = 0;
        found = 0;
        if (uliop_path_buf[i] != 0) {
            if (uliop_path_buf[i] == ':')
                i = 1;
            else {
            colon:
                i++;
                if (uliop_path_buf[i] != 0) {
                    if (uliop_path_buf[i] != ':')
                        goto colon;
                    i++;
                }
            }
        }
        if (uliop_path_buf[i] == 0)
            i = 0;
        while (uliop_path_buf[i] != 0) {
            if (uliop_path_buf[i] == ';') {
                found = 1;
                break;
            }
            cur = uliop_path_buf + i;
            c = (unsigned char)*cur;
            if ((unsigned int)(c - 97) < 26) {
                int upper = c - 32;
                *cur = upper;
                c = upper;
            }
            if ((char)c == '/')
                *cur = '\\';
            i++;
        }
        if (found == 0)
            strcat(uliop_path_buf, ";1");
    } else {
        for (i = 0; uliop_path_buf[i] != 0; i++) {
            if (uliop_path_buf[i] == '\\')
                uliop_path_buf[i] = '/';
        }
    }
    return uliop_path_buf;
}

/*
 * img relative to the module directory; dvd 0 = CD. load_mem2mb loads util\\mem2MB.irx first;
 * without img only re-inits SIF/file/disc. A refused reboot request is retried.
 */
/* steer(cast) */
/* 0x0018ae68 */
int uliop_reboot_iop(char *img, int dvd, int load_mem2mb)
{
    char path[512];
    int err = 0;

    uliop_work.rebooted = 1;
    if (img) {
        ((int (*)(int))sceSifInitRpc)(0);
        sceSifInitIopHeap();
        sceSifLoadFileReset();
        sceFsReset();
        sceCdInit(1);
        if (dvd)
            libcdvd_scmd34(2);
        else
            libcdvd_scmd34(1);
        sceCdDiskReady(0);
        if (load_mem2mb) {
            err = uliop_load_module("util\\mem2MB.irx", 0, 0);
            if (err)
                return err;
        }
        strcpy(path, uliop_work_path);
        strcat(path, img);
#ifdef VERSION_US
        while (sceSifRebootIop(uliop_normalize_path(path)) == 0)
            ;
#else
        if (sceSifRebootIop(uliop_normalize_path(path)) == 0)
            for (;;)
                ;
#endif
        while (sceSifSyncIop() == 0)
            ;
        printf("uliop : IOP Reboot Success !!\n");
        libkernl_printf_line("uliop : IOP Reboot Success !!\n");
    }
    ((int (*)(int))sceSifInitRpc)(0);
    sceSifInitIopHeap();
    sceSifLoadFileReset();
    sceFsReset();
    sceCdInit(1);
    if (dvd)
        libcdvd_scmd34(2);
    else
        libcdvd_scmd34(1);
    return err;
}

/*
 * Device from the prefix: cdrom0: 0 (default), host0: 1, hd0: 2, usblan0: 3. Fails before
 * uliop_init.
 */
/* 0x0018afe0 */
int uliop_set_path(char *path)
{
    UlIop *iop = &uliop_work;

    if (iop->init == 0) {
        uliop_error_callback(0x80100001);
        return 0x80100001;
    }
    strcpy(iop->path, path);
    iop->dev = 0;
    if (strncmp(path, "cdrom0:", 7) == 0)
        iop->dev = 0;
    else if (strncmp(path, "host0:", 6) == 0)
        iop->dev = 1;
    else if (strncmp(path, "hd0:", 4) == 0)
        iop->dev = 2;
    else if (strncmp(path, "usblan0:", 8) == 0)
        iop->dev = 3;
    return 0;
}

/* Records up to 15 chars of name; an already loaded module returns uliop_find_module's warning. */
/* 0x0018b0c8 */
static int uliop_load_module(char *name, int arglen, int args)
{
    char path[512];
    UlIop *iop = &uliop_work;
    char *full;
    int err;
    int id;

    if (iop->init == 0) {
        uliop_error_callback(0x80100001);
        return 0x80100001;
    }
    err = uliop_find_module(name);
    if (err) {
        uliop_error_callback(err);
        return err;
    }
    sceCdDiskReady(0);
    strcpy(path, iop->path);
    strcat(path, name);
    full = uliop_normalize_path(path);
    id = sceSifLoadModule(full, arglen, args);
    if (id < 0) {
        libkernl_printf_line("uliop : Module Load Error %d(%08x) [%s]\n", id, id, full);
        uliop_error_callback(0x80200001);
        return 0x80200001;
    }
    NOP8();
    {
        char (*names)[20] = iop->ent;

        strncpy(names[iop->count], name, 15);
    }
    iop->count = iop->count + 1;
    return 0;
}

/*
 * DMA-copies size bytes at src into an IOP heap buffer and starts it.
 * Bug: inverted allocation check reports success as out of memory.
 */
/* 0x0018b220 */
static int uliop_load_module_buffer(int src, int size)
{
    int list[4];
    int buf;
    int id;

    if (uliop_work.init == 0) {
        uliop_error_callback(0x80100001);
        return 0x80100001;
    }
    buf = sceSifAllocIopHeap(size);
    if (buf) {
        uliop_error_callback(0x80400001);
        return 0x80400001;
    }
    list[0] = src;
    list[1] = 0;
    list[2] = size;
    list[3] = 0;
    id = sceSifSetDma(list, 1);
    if (id == 0) {
        uliop_error_callback(0x8f000001);
        sceSifFreeSysMemory(0);
        return 0x8f000001;
    }
    while (sceSifDmaStat(id) >= 0)
        ;
    if (sif_loadfile_start_module_noresult(buf, 0, 0) < 0) {
        uliop_error_callback(0x80200001);
        sceSifFreeSysMemory(buf);
        return 0x80200001;
    }
    NOP8();
    sceSifFreeSysMemory(buf);
    return 0;
}

/* First 15 chars; the already-loaded warning if listed, else 0. */
/* 0x0018b358 */
static int uliop_find_module(char *name)
{
    int i;

    if (uliop_work.init == 0) {
        uliop_error_callback(0x80100001);
        return 0x80100001;
    }
    for (i = 0; i < uliop_work.count; i++) {
        if (strncmp(name, uliop_work.ent[i], 15) == 0)
            break;
    }
    return i < uliop_work.count ? 0x10000001 : 0;
}

/* 0x0018b408 */
static int uliop_is_rebooted(void)
{
    return uliop_work.rebooted;
}

/* Formats, then splits at spaces into consecutive terminated strings. Returns the length. */
/* 0x0018b418 */
static int uliop_make_args(char *buf, char *fmt, ...)
{
    va_list ap;
    int n;
    int i;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    n = strlen(buf);
    for (i = 0; i < n; i++) {
        if (buf[i] == 32)
            buf[i] = 0;
    }
    return n + 1;
}

/* Installs the default error reporter. */
/* 0x0018b4a8 */
int uliop_init(void)
{
    UlIop *iop = &uliop_work;

    memset(iop, 0, 2204);
    uliop_set_error_callback(uliop_report_error);
    iop->init = 1;
    return 0;
}

/* 0x0018b4f8 */
static int uliop_set_debug(int dbg)
{
    uliop_work.dbg = dbg;
    return 0;
}

/* 0x0018b508 */
static int uliop_set_error_callback(void (*cb)(int))
{
    uliop_work.err_cb = cb;
    return 0;
}

/* Negative codes print as errors, positive as warnings. */
/* 0x0018b518 */
static void uliop_report_error(int err)
{
    if (err < 0)
        uliop_printf("libuliop:  Error (%08x)\n", err);
    else if (err > 0)
        uliop_printf("libuliop:  Warning (%08x)\n", err);
}

/* 0x0018b548 */
static void uliop_error_callback(int err)
{
    if (uliop_work.err_cb)
        uliop_work.err_cb(err);
}

/* Only while debug output is on. */
/* 0x0018b570 */
static void uliop_dprintf(char *fmt, ...)
{
    char buf[256];
    va_list ap;

    if (uliop_work.dbg) {
        va_start(ap, fmt);
        vsprintf(buf, fmt, ap);
        printf(buf);
    }
}

/* 0x0018b5c8 */
static void uliop_printf(char *fmt, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    printf(buf);
}

/* Only while debug output is on. */
/* 0x0018b610 */
static void uliop_dset_cursor(int x, int y)
{
    if (uliop_work.dbg)
        ulConsoleSetCursor(x, y);
}

/* 0x0018b630 */
static void uliop_set_cursor(int x, int y)
{
    ulConsoleSetCursor(x, y);
}

/* Only while debug output is on. */
/* 0x0018b638 */
static void uliop_dprintf_screen(char *fmt, ...)
{
    char buf[256];
    va_list ap;

    if (uliop_work.dbg) {
        va_start(ap, fmt);
        vsprintf(buf, fmt, ap);
        ulConsolePrintf(buf);
    }
}

/* 0x0018b690 */
static void uliop_printf_screen(char *fmt, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    ulConsolePrintf(buf);
}
