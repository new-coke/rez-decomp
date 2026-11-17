/*
 * DVD device for cvFs ("CDV"): ISO 9660 paths, sector reads via SRD, one read at a time over 40
 * files. A start-up file list caches positions and sizes; "DVD-ROM" opens the whole disc.
 */

#include "types_cri.h"
#include "dvci.h"
#include "libc.h"
#include "htci_filelist.h"
#include "libcdvd.h"
#include "libkernl_cache.h"
#include "srd.h"

static void dvci_stop_tr(DvciHn *hn);
static void dvci_make_cd_path(const char *fname, char *path);
static void dvci_find_file(const char *fname, DvciFile *file);

extern char *volatile dvci_ver_str;
extern char dvci_vtbl[];
extern void (*dvci_error_fn)(void *obj, const char *msg, void *h);
extern void *dvci_error_obj;
extern char dvci_path_buf[];
extern DvciMode dvci_read_mode;

extern char dvci_cur_dir[];
extern int sceCdRead(unsigned int lsn, int nsct, void *buf, DvciMode *mode);
extern char dvci_filelist_path[];

extern char dvci_filelist_buf[];
extern int dvci_read_mode_flag;

/* Current directory cleared. */
/* 0x001cde78 */
void *dvci_get_interface(void)
{
    char *ver;

    ver = dvci_ver_str;
    dvci_set_cur_dir(0);
    return dvci_vtbl;
}

/* Runs the sector read server once. */
/* 0x001cdea8 */
static void dvci_lock(void)
{
    srd_exec_server();
}

/* 0x001cdeb0 */
static void dvci_error(void *hn, const char *msg)
{
    if (dvci_error_fn)
        dvci_error_fn(dvci_error_obj, msg, hn);
}

/* 0x001cdee0 */
static int dvci_is_any_reading(void)
{
    int i;

    for (i = 0; i < 40; i++) {
        if (dvci_obj_tbl[i].used == 1 && dvci_obj_tbl[i].stat == 2)
            return 1;
    }
    return 0;
}

/* Drive error: state 3. Done: invalidates buffer cache lines, advances, state 1. */
/* 0x001cdf30 */
static void dvci_poll_handle(DvciHn *hn)
{
    int ret;

    if (hn->stat == 2) {
        ret = srd_get_stat();
        if (ret == 9)
            hn->stat = 3;
        if (ret == 3) {
            InvalidDCache(hn->buf, hn->buf + (hn->rqsct << 11) - 1);
            hn->stat = 1;
            hn->cursct += hn->rqsct;
        }
    }
}

/* 0x001cdfc0 */
static void dvci_exec_server(void)
{
    int i;

    for (i = 0; i < 40; i++) {
        if (dvci_obj_tbl[i].used == 1)
            dvci_poll_handle(&dvci_obj_tbl[i]);
    }
}

/* 0x001ce020 */
static void dvci_set_error_cb(void (*fn)(void *obj, const char *msg, void *h), void *obj)
{
    dvci_error_fn = fn;
    dvci_error_obj = obj;
}

/* '/' -> '\\', upper case. */
/* 0x001ce038 */
static void dvci_normalize_path(char *s)
{
    unsigned int i;
    unsigned int n;

    n = strlen(s);
    for (i = 0; i < n; i++) {
        if (s[i] == '/')
            s[i] = '\\';
        if (((unsigned char *)s)[i] >= 'a' && ((unsigned char *)s)[i] <= 'z')
            s[i] = ((unsigned char *)s)[i] - 32;
    }
}

/* Bytes, from the file cache or a disc search; 0 on error. */
/* 0x001ce0b0 */
static int dvCiGetFileSize(const char *fname)
{
    DvciFile file;

    if (fname == 0) {
        dvci_error(0, "E0092901:fname is null.(dvCiGetFileSize)");
        return 0;
    }
    dvci_find_file(fname, &file);
    if (file.size != 0)
        return file.size;
    srd_wait();
    dvci_make_cd_path(fname, dvci_path_buf);
    if (sceCdSearchFile(&file, dvci_path_buf) == 0) {
        printf("DVCI: sceCdSearchFile failed. \"%s\"\n", dvci_path_buf);
        dvci_error(0, "E0092902:can't find file.(dvCiGetFileSize)");
        return 0;
    }
    return file.size;
}

/* Last free one of forty; 0 if none. */
/* 0x001ce158 */
static DvciHn *dvci_alloc_handle(void)
{
    int i;
    DvciHn *hn;

    hn = 0;
    for (i = 0; i < 40; i++) {
        if (dvci_obj_tbl[i].used == 0)
            hn = &dvci_obj_tbl[i];
    }
    return hn;
}

/* 0x001ce190 */
static void dvci_clear_handle(DvciHn *hn)
{
    memset(hn, 0, sizeof(DvciHn));
}

/* rw must be 0. File cache, else disc search (prints a cache miss); 0 on error. */
/* 0x001ce1a0 */
static DvciHn *dvCiOpen(const char *fname, void *dir, int rw)
{
    DvciHn *hn;

    if (fname == 0) {
        dvci_error(0, "E0092908:fname is null.(dvCiOpen)");
        return 0;
    }
    if (rw != 0) {
        dvci_error(0, "E0092909:rw is illigal.(dvCiOpen)");
        return 0;
    }
    hn = dvci_alloc_handle();
    if (hn == 0) {
        dvci_error(0, "E0092910:not enough handle resource.(dvCiOpen)");
        return 0;
    }
    dvci_find_file(fname, &hn->file);
    if (hn->file.size == 0) {
        dvci_make_cd_path(fname, dvci_path_buf);
        srd_wait();
        printf("DVCI: File cache was not hit. \"%s\"\n", fname);
        if (sceCdSearchFile(&hn->file, dvci_path_buf) == 0) {
            dvci_error(0, "E0092911:sceCdSearchFile fail.(dvCiOpen)");
            dvci_clear_handle(hn);
            return 0;
        }
        hn->cached = 0;
    } else {
        hn->cached = 1;
    }
    hn->used = 1;
    hn->fsize_sct = (hn->file.size + 2047) >> 11;
    hn->fsize = hn->file.size;
    hn->cursct = 0;
    hn->buf = 0;
    hn->rqsct = 0;
    hn->stat = 0;
    return hn;
}

/* Stops a transfer in progress. */
/* 0x001ce2e0 */
static void dvci_close(DvciHn *hn)
{
    if (hn) {
        if ((unsigned char)hn->stat >= 2)
            dvci_stop_tr(hn);
        hn->used = 0;
        dvci_clear_handle(hn);
    }
}

/* Sectors; whence 0 start, 1 current, 2 end; clamped. */
/* 0x001ce338 */
static int dvci_seek(DvciHn *hn, int ofst, int whence)
{
    if (hn == 0) {
        dvci_error(0, "E0092912:handl is null.");
        return 0;
    }
    if (whence == 0)
        hn->cursct = ofst;
    else if (whence == 2)
        hn->cursct = hn->fsize_sct + ofst;
    else if (whence == 1)
        hn->cursct = hn->cursct + ofst;
    hn->cursct = (hn->cursct < hn->fsize_sct) ? hn->cursct : hn->fsize_sct;
    hn->cursct = (hn->cursct >= 0) ? hn->cursct : 0;
    return hn->cursct;
}

/* Sectors. */
/* 0x001ce3d0 */
static int dvci_tell(DvciHn *hn)
{
    if (hn == 0) {
        dvci_error(0, "E0092912:handl is null.");
        return 0;
    }
    return hn->cursct;
}

/* Up to the rest of the file, current read mode; 0 when another file reads or the drive is busy. */
/* 0x001ce408 */
static int dvCiReqRd(DvciHn *hn, int nsct, char *buf)
{
    int rest;
    unsigned int lsn;

    if (hn == 0) {
        dvci_error(0, "E0092912:handl is null.");
        return 0;
    }
    if (nsct < 0) {
        dvci_error(hn, "E0092913:nsct < 0.(dvCiReqRd)");
        return 0;
    }
    if (buf == 0) {
        dvci_error(hn, "E0092914:buf is null.(dvCiReqRd)");
        return 0;
    }
    if (hn->stat == 2)
        return 0;
    if (nsct == 0) {
        hn->stat = 1;
        return 0;
    }
    if (dvci_is_any_reading() == 1)
        return 0;
    hn->buf = buf;
    rest = hn->fsize_sct - hn->cursct;
    if (nsct >= rest)
        nsct = rest;
    hn->rqsct = nsct;
    hn->mode.trycount = dvci_read_mode.trycount;
    hn->mode.spindlctrl = dvci_read_mode.spindlctrl;
    hn->mode.datapattern = dvci_read_mode.datapattern;
    if (sceCdSync(1) == 1)
        return 0;
    dvci_exec_server();
    lsn = hn->file.lsn + hn->cursct;
    InvalidDCache(hn->buf, hn->buf + (hn->rqsct << 11) - 1);
    if (srd_req_cd_read(lsn, hn->rqsct, hn->buf, &hn->mode) == 0)
        return 0;
    dvci_lock();
    hn->stat = 2;
    return hn->rqsct;
}

/* Polls once, cancels a running read, invalidates buffer cache lines, idles. */
/* 0x001ce570 */
static void dvci_stop_tr(DvciHn *hn)
{
    if (hn == 0) {
        dvci_error(0, "E0092912:handl is null.");
    } else if (hn->stat != 0) {
        dvci_poll_handle(hn);
        if (hn->stat == 2)
            srd_cancel();
        InvalidDCache(hn->buf, hn->buf + (hn->rqsct << 11) - 1);
        hn->stat = 0;
    }
}

/* 0 idle, 1 read done, 2 reading, 3 read error. */
/* 0x001ce5f8 */
static int dvci_get_stat(DvciHn *hn)
{
    if (hn == 0) {
        dvci_error(0, "E0092912:handl is null.");
        return 0;
    }
    return hn->stat;
}

/* 0x001ce630 */
static int dvci_get_sct_len(void)
{
    return 2048;
}

/* Bytes of the current request. */
/* 0x001ce638 */
static int dvci_get_num_tr(DvciHn *hn)
{
    if (hn == 0) {
        dvci_error(0, "E0092912:handl is null.");
        return 0;
    }
    return hn->rqsct << 11;
}

/* Current dir + '\\' (unless leading) + name + ";1" (unless present), normalised. */
/* 0x001ce670 */
static void dvci_make_cd_path(const char *fname, char *path)
{
    strcpy(path, dvci_cur_dir);
    if (fname[0] != '/' && fname[0] != '\\')
        strcat(path, "\\");
    strcat(path, fname);
    if (strcmp(path + (strlen(path) - 2), ";1") != 0)
        strcat(path, ";1");
    dvci_normalize_path(path);
}

/* Upper case, '\\' as '/'. */
/* 0x001ce710 */
static char dvci_norm_char(char c)
{
    char norm;

    norm = c;
    if (c >= 'a' && c <= 'z')
        norm = c - 32;
    if (c == '\\')
        norm = '/';
    return norm;
}

/* 0x001ce748 */
static int dvci_cmp_char(char c1, char c2)
{
    if (dvci_norm_char(c1) != dvci_norm_char(c2))
        return -1;
    return 0;
}

/* Ignores case and separator style; 0 equal, else -1. */
/* 0x001ce798 */
static int dvci_cmp_path(const char *path1, const char *path2)
{
    int n;
    int i;

    n = strlen(path1);
    if (n != strlen(path2))
        return -1;
    for (i = 0; i < n; i++) {
        if (dvci_cmp_char(path1[i], path2[i]) != 0)
            return -1;
    }
    return 0;
}

/* One name per line; buf: nmax {sector, size} entries, then nmax names; installed if none. */
/* 0x001ce838 */
static int dvci_parse_filelist(char *buf, char *text, int nmax, int namelen)
{
    int n;
    int i;
    int st;
    char *dst;
    int len;

    n = 0;
    st = 0;
    for (i = 0; text[i] != 0; i++) {
        if (text[i] == '\n' || text[i] == 0) {
            len = i - st;
            if (text[i - 1] == '\r')
                text[i - 1] = 0;
            dst = buf + (nmax * 8 + (namelen * n + n));
            n++;
            memcpy(dst, text + st, len);
            dvci_normalize_path(dst);
            st = i + 1;
            if (n == nmax)
                break;
        }
    }
    if (dvci_file_cache.buf == 0) {
        dvci_file_cache.buf = buf;
        dvci_file_cache.namelen = namelen;
        dvci_file_cache.nmax = nmax;
        dvci_file_cache.nfile = n;
    }
    return n;
}

/* First two sectors; "HST:" names from the host, else the disc; 1 ok, 0 failure. */
/* 0x001ce970 */
static int dvci_read_filelist(const char *fname, char *buf)
{
    DvciFile file;
    DvciMode mode;
    char head[5];

    __builtin_memcpy(head, fname, 5);
    head[4] = 0;
    if (strcasecmp(head, "HST:") == 0)
        return htci_read_filelist(fname + 4, buf);
    memset(dvci_filelist_path, 0, 128);
    dvci_make_cd_path(fname, dvci_filelist_path);
    if (sceCdSearchFile(&file, dvci_filelist_path) == 0)
        return 0;
    mode.trycount = 0;
    mode.spindlctrl = 1;
    mode.datapattern = 0;
    mode.pad = 0;
    if (sceCdRead(file.lsn, 2, buf, &mode) == 0)
        return 0;
    sceCdSync(0);
    return 1;
}

/* Disc search per listed file; prints each result and the total; returns the count found. */
/* 0x001cea60 */
static int dvci_fill_cache(char *buf, int n)
{
    int nfound;
    int nmiss;
    int i;
    unsigned int *ent;
    char *name;
    DvciFile file;

    nmiss = 0;
    nfound = 0;
    for (i = 0; i < n; i++) {
        ent = (unsigned int *)(buf + i * 8);
        name = buf + (dvci_file_cache.nmax * 8 + (dvci_file_cache.namelen * i + i));
        if (name[0] == 0)
            continue;
        dvci_make_cd_path(name, dvci_filelist_path);
        if (sceCdSearchFile(&file, dvci_filelist_path) == 1) {
            ent[0] = file.lsn;
            ent[1] = file.size;
            printf("DVCI: \"%s\" found.\n", name);
            nfound++;
        } else {
            printf("DVCI: \"%s\" Not found.\n", name);
            nmiss++;
        }
    }
    printf("DVCI: Total %d files.\n", nfound);
    if (nmiss > 0)
        printf("DVCI: Error, Can't load all cache file.%c", 10);
    return nfound;
}

/* Zeros when absent. */
/* 0x001cebb0 */
static void dvci_cache_lookup(DvciFile *file, const char *name, char *buf, int n)
{
    int i;
    unsigned int *ent;

    for (i = 0; i < n; i++) {
        ent = (unsigned int *)(buf + i * 8);
        if (dvci_cmp_path(name, buf + (dvci_file_cache.nmax * 8 + (dvci_file_cache.namelen * i + i))) == 0) {
            file->lsn = ent[0];
            file->size = ent[1];
            return;
        }
    }
    file->lsn = 0;
    file->size = 0;
}

/* 0x001cec80 */
static void dvci_clear_cache(void)
{
    memset(&dvci_file_cache, 0, sizeof(DvciCache));
}

/* "DVD-ROM" = whole disc; zeros when not cached. */
/* 0x001cec98 */
static void dvci_find_file(const char *fname, DvciFile *file)
{
    if (strcmp("DVD-ROM", fname) == 0) {
        file->lsn = 0;
        file->size = 0x7fffffff;
        printf("DVCI: CD/DVD-ROM Image opened%c\n", '.');
    } else {
        file->lsn = 0;
        file->size = 0;
        if (dvci_file_cache.buf)
            dvci_cache_lookup(file, fname, dvci_file_cache.buf, dvci_file_cache.nfile);
    }
}

/* Cache in buf (entries as size allows, names up to namelen); returns bytes used, 0 on error. */
/* 0x001ced40 */
int dvci_load_filelist(const char *fname, char *buf, unsigned int size, int namelen)
{
    int nmax;
    int n;

    memset(dvci_filelist_buf, 0, 4096);
    if (dvci_file_cache.buf == 0)
        dvci_clear_cache();
    if (fname == 0 || buf == 0 || size == 0) {
        dvci_clear_cache();
        return 0;
    }
    if (dvci_read_filelist(fname, dvci_filelist_buf) == 0) {
        printf("DVCI: Can't read filelist file.(%s)\n", fname);
        dvci_error(0, "E0111501:can't read filelist.(dvCiLoadDirInfo)");
        return 0;
    }
    nmax = size / (namelen + 9);
    n = dvci_parse_filelist(buf, dvci_filelist_buf, nmax, namelen);
    n = dvci_fill_cache(buf, n);
    return size / (namelen + 9) * 8 + n * (namelen + 1);
}

/* As dvci_load_filelist with 129-character names, 137 bytes per file. */
/* 0x001cee68 */
static int dvci_load_filelist_fixed(const char *fname, char *buf, int size)
{
    int recsz;

    memset(dvci_filelist_buf, 0, 4096);
    if (dvci_file_cache.buf == 0)
        dvci_clear_cache();
    if (fname == 0 || buf == 0 || size == 0) {
        dvci_clear_cache();
        return 0;
    }
    if (dvci_read_filelist(fname, dvci_filelist_buf) == 0) {
        printf("DVCI: Can't read filelist file.(%s)\n", fname);
        dvci_error(0, "E0111501:can't read filelist.(dvCiLoadDirInfo)");
        return 0;
    }
    recsz = 137;
    return dvci_fill_cache(buf, dvci_parse_filelist(buf, dvci_filelist_buf, size / recsz, 129)) * recsz;
}

/* 0x001cef68 */
int dvci_is_cached(const char *fname)
{
    DvciFile file;

    dvci_find_file(fname, &file);
    return file.lsn != 0;
}

/* For all reads; plus a flag. */
/* 0x001cef90 */
void dvci_set_read_mode(int trycount, int spindlctrl, int datapattern, int flag)
{
    dvci_read_mode.trycount = trycount;
    dvci_read_mode.spindlctrl = spindlctrl;
    dvci_read_mode.datapattern = datapattern;
    dvci_read_mode_flag = flag;
}

/* Adds a leading '\\', drops a trailing separator; null clears it. */
/* 0x001cefb0 */
void dvci_set_cur_dir(const char *dir)
{
    char c;

    if (dir == 0) {
        dvci_cur_dir[0] = 0;
        return;
    }
    if (dir[0] != '/' && dir[0] != '\\')
        __builtin_strcpy(dvci_cur_dir, "\\");
    else
        dvci_cur_dir[0] = 0;
    strcat(dvci_cur_dir, dir);
    c = dir[strlen(dir) - 1];
    if (c == '/' || c == '\\')
        dvci_cur_dir[strlen(dvci_cur_dir) - 1] = 0;
}

/* 0x001cf070 */
static char *dvci_get_cur_dir(void)
{
    return dvci_cur_dir;
}
