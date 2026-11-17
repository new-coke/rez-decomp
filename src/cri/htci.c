/*
 * Host file device for cvFs ("host:" on the dev PC) via IOP file calls and SRD; 13 open files.
 * A host file list keeps the game's files open to skip host round trips.
 */

#include "types_cri.h"
#include "htci.h"
#include "libc.h"
#include "htci_filelist.h"
#include "libkernl_fileio.h"
#include "srd.h"

static void htci_stop_tr(HtciObj *hn);

/* Directory and device prefix reset. */
/* 0x001d82b0 */
static int *htci_get_interface(void)
{
    htci_ver_str;
    htci_set_cur_dir(0);
    htci_set_dev_prefix(0);
    return htci_vtbl;
}

/* Busy-wait between host status polls. */
/* 0x001d82e8 */
static void htci_delay(void)
{
    int i;

    for (i = 0x8000; i != 0; i--)
        ;
}

/* 0x001d8318 */
void htci_error(void *hn, const char *msg)
{
    if (htci_error_fn != 0)
        htci_error_fn(htci_error_obj, msg, hn);
}

/* 0x001d8348 */
static int htci_is_any_reading(void)
{
    int i;

    for (i = 0; i < 13; i++) {
        if (htci_obj_tbl[i].used == 1 && htci_obj_tbl[i].stat == 2)
            return 1;
    }
    return 0;
}

/* 1 when the host is not busy with it. */
/* 0x001d8390 */
static int htci_is_host_idle(HtciObj *hn)
{
    int ret;

    srd_lock();
    ret = sceIoctl(hn->fd, 1, &hn->st);
    srd_unlock();
    if (ret < 0)
        return 0;
    return hn->st != 1;
}

/* Host busy with none of the open files. */
/* 0x001d83e8 */
static int htci_all_host_idle(void)
{
    int i;

    for (i = 0; i < 13; i++) {
        if (htci_obj_tbl[i].used == 1 && htci_is_host_idle(&htci_obj_tbl[i]) == 0)
            return 0;
    }
    return 1;
}

/* Waits for the sector read driver. */
/* 0x001d8458 */
void htci_srd_wait(void)
{
    srd_wait();
}

/* As htci_srd_wait; fd unused; returns 0. */
/* 0x001d8460 */
int htci_srd_wait_fd(int fd)
{
    srd_wait();
    return 0;
}

/* Host idle: seek, then SRD read, then completion (advances the position). */
/* 0x001d8480 */
static void htci_exec_handle(HtciObj *hn)
{
    if (hn->stat != 2)
        return;
    if (htci_all_host_idle() == 0)
        return;
    if (hn->st != 0)
        return;
    if (hn->req == 0) {
        hn->req = 1;
    } else if (hn->req == 1) {
        hn->req = 2;
    } else if (hn->req == 2) {
        hn->stat = 1;
        hn->req = 0;
        hn->pos += hn->nsct;
    }
    srd_lock();
    if (hn->req == 1) {
        sceLseek(hn->fd, hn->pos << 11, 0);
    } else if (hn->req == 2) {
        if (srd_req_host_read(hn->fd, hn->buf, hn->nsct << 11) == 0)
            printf("HTCI: Request Failed. Fatal Error.\n");
    }
    srd_unlock();
}

/* While SRD is not busy, steps every open file. */
/* 0x001d8590 */
static void htci_exec_server(void)
{
    int st;
    int i;

    st = srd_get_stat();
    if (st == 0 || st == 3 || st == 9) {
        for (i = 0; i < 13; i++) {
            if (htci_obj_tbl[i].used == 1)
                htci_exec_handle(&htci_obj_tbl[i]);
        }
    }
}

/* 0x001d8610 */
static void htci_set_error_cb(HtciErrFn fn, void *obj)
{
    htci_error_fn = fn;
    htci_error_obj = obj;
}

/* File cache, else open and seek to the end; 0 on error. */
/* 0x001d8628 */
static int htCiGetFileSize(const char *fname)
{
    int info[2];
    int fd;
    int size;

    if (fname == 0) {
        htci_error(0, "E0092704:fname is null.(htCiGetFileSize)");
        return 0;
    }
    htci_cache_lookup(fname, info);
    htci_make_host_path(fname, htci_path_buf);
    if (info[1] == 0) {
        htci_srd_wait();
        srd_lock();
        fd = sceOpen(htci_path_buf, 1);
        srd_unlock();
        if (fd < 0) {
            printf("HTCI: sceOpen failed. \"%s\"\n", htci_path_buf);
            htci_error(0, "E0092705:sceOpen fail.(htCiGetFileSize)");
            return 0;
        }
        if (fd == 0x10004) {
            printf("HTCI: sceOpen failed. IOP Module version is invalid.\n");
            htci_error(0, "E0082101:sceOpen fail.(htCiGetFileSize)");
            return 0;
        }
        if (fd == 19) {
            printf("HTCI: sceOpen failed. file desc is insufficient.\n");
            htci_error(0, "E0082101:sceOpen fail.(htCiGetFileSize)");
            return 0;
        }
        htci_srd_wait_fd(fd);
        srd_lock();
        size = sceLseek(fd, 0, 2);
        if (size < 0) {
            htci_error(0, "E0092706:sceLseek fail.(htCiGetFileSize)");
            sceClose(fd);
            srd_unlock();
            return 0;
        }
        htci_srd_wait_fd(fd);
        if (sceClose(fd) < 0) {
            htci_error(0, "E0092707:sceClose fail.(htCiGetFileSize)");
            srd_unlock();
            return 0;
        }
        srd_unlock();
    } else
        size = info[1];
    return size;
}

/* 0 on error; leaves it positioned at the end. */
/* 0x001d87b8 */
int htci_get_host_fsize(int fd)
{
    int ret;

    htci_srd_wait_fd(fd);
    srd_lock();
    ret = sceLseek(fd, 0, 0);
    srd_unlock();
    if (ret < 0)
        return 0;
    htci_srd_wait_fd(fd);
    srd_lock();
    ret = sceLseek(fd, 0, 2);
    srd_unlock();
    if (ret < 0)
        ret = 0;
    return ret;
}

/* Last free one of thirteen; 0 if none. */
/* 0x001d8848 */
static HtciObj *htci_alloc_handle(void)
{
    HtciObj *htci;
    int i;

    htci = 0;
    for (i = 0; i < 13; i++) {
        if (htci_obj_tbl[i].used == 0)
            htci = &htci_obj_tbl[i];
    }
    return htci;
}

/* 0x001d8880 */
static void htci_clear_handle(HtciObj *hn)
{
    memset(hn, 0, sizeof(HtciObj));
}

/* rw must be 0. Descriptor and size from the file cache, else opens (prints a cache miss). */
/* 0x001d8890 */
static HtciObj *htCiOpen(const char *fname, void *dir, int rw)
{
    union {
        long l;
        int w[2];
    } info;
    HtciObj *hn;

    info.l = 0;
    if (fname == 0) {
        htci_error(0, "E0092708:fname is null.(htCiOpen)");
        return 0;
    }
    if (rw != 0) {
        htci_error(0, "E0092709:rw is illigal.(htCiOpen)");
        return 0;
    }
    hn = htci_alloc_handle();
    if (hn == 0) {
        htci_error(0, "E0092710:not enough handle resource.(htCiOpen)");
        return 0;
    }
    htci_cache_lookup(fname, info.w);
    htci_make_host_path(fname, htci_path_buf);
    if (info.w[1] == 0) {
        printf("HTCI: File cache was not hit. \"%s\"\n", fname);
        hn->fsize = htCiGetFileSize(fname);
        if (hn->fsize == 0)
            return 0;
        htci_srd_wait();
        srd_lock();
        hn->fd = sceOpen(htci_path_buf, htci_open_mode);
        srd_unlock();
        if (hn->fd < 0) {
            htci_error(0, "E0092711:sceOpen fail.(htCiOpen)");
            htci_clear_handle(hn);
            return 0;
        }
        htci_srd_wait_fd(hn->fd);
        hn->cache = 0;
    } else {
        hn->cache = 1;
        hn->fd = info.w[0];
        hn->fsize = info.w[1];
    }
    hn->pos = 0;
    hn->used = 1;
    hn->buf = 0;
    hn->nsct = 0;
    hn->fsct = (hn->fsize + 2047) / 2048;
    hn->req = 0;
    hn->st = 0;
    hn->stat = 0;
    return hn;
}

/* Stops a running request; closes the host file unless cached. */
/* 0x001d8a28 */
static void htci_close(HtciObj *hn)
{
    int ret;

    if (hn == 0)
        return;
    if (hn->stat != 0 && hn->stat != 1)
        htci_stop_tr(hn);
    if (hn->cache == 0) {
        htci_srd_wait();
        srd_lock();
        ret = sceClose(hn->fd);
        srd_unlock();
        if (ret < 0)
            printf("HTCI: Failed sceClose\n");
    }
    hn->used = 0;
    htci_clear_handle(hn);
}

/* Sectors; whence 0 start, 1 current, 2 end; clamped. */
/* 0x001d8ac8 */
static int htci_seek(HtciObj *hn, int ofs, int whence)
{
    int pos;

    if (hn == 0) {
        htci_error(0, "E0092712:handl is null.");
        return 0;
    }
    if (whence == 0)
        hn->pos = ofs;
    else if (whence == 2)
        hn->pos = hn->fsct + ofs;
    else if (whence == 1)
        hn->pos = hn->pos + ofs;
    pos = hn->pos;
    if (pos >= hn->fsct)
        pos = hn->fsct;
    if (pos < 0)
        pos = 0;
    hn->pos = pos;
    return pos;
}

/* Sectors. */
/* 0x001d8b60 */
static int htci_tell(HtciObj *hn)
{
    if (hn == 0) {
        htci_error(0, "E0092712:handl is null.");
        return 0;
    }
    return hn->pos;
}

/* Up to the rest of the file, when nothing else reads; returns sectors requested, or 0. */
/* 0x001d8b98 */
static int htCiReqRd(HtciObj *hn, int nsct, void *buf)
{
    int n;

    if (hn == 0) {
        htci_error(0, "E0092712:handl is null.");
        return 0;
    }
    if (nsct < 0) {
        htci_error(hn, "E0092713:nsct < 0.(htCiReqRd)");
        return 0;
    }
    if (buf == 0) {
        htci_error(hn, "E0092714:buf is null.(htCiReqRd)");
        return 0;
    }
    if (htci_is_any_reading() == 1)
        return 0;
    if (srd_get_stat() == 2)
        return 0;
    if (htci_all_host_idle() == 0)
        return 0;
    if (hn->stat != 0 && hn->stat != 1)
        return 0;
    if (nsct == 0) {
        hn->stat = 1;
        return 0;
    }
    n = hn->fsct - hn->pos;
    hn->stat = 2;
    hn->buf = buf;
    hn->req = 0;
    hn->st = 0;
    if (nsct < n)
        n = nsct;
    hn->nsct = n;
    return n;
}

/* Waits up to 20 polls for a busy host read, then idles. */
/* 0x001d8ca0 */
static void htci_stop_tr(HtciObj *hn)
{
    int i;

    if (hn == 0) {
        htci_error(0, "E0092712:handl is null.");
        return;
    }
    if (hn->stat == 0)
        return;
    if (hn->stat == 2 && hn->st == 1) {
        for (i = 0; i < 20; i++) {
            srd_lock();
            sceIoctl(hn->fd, 1, &hn->st);
            srd_unlock();
            if (hn->st == 0)
                break;
            htci_delay();
        }
    }
    hn->stat = 0;
}

/* 0 idle, 1 read done, 2 reading. */
/* 0x001d8d60 */
static int htci_get_stat(HtciObj *hn)
{
    if (hn == 0) {
        htci_error(0, "E0092712:handl is null.");
        return 0;
    }
    return hn->stat;
}

/* 0x001d8d98 */
static int htci_get_sct_len(void)
{
    return 2048;
}

/* Bytes of the current request. */
/* 0x001d8da0 */
static int htci_get_num_tr(HtciObj *hn)
{
    if (hn == 0) {
        htci_error(0, "E0092712:handl is null.");
        return 0;
    }
    return hn->nsct << 11;
}
