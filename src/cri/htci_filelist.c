/* Host device paths ("host:" + prefix + current dir) and a host file list kept open as a cache. */

#include "types_cri.h"
#include "libc.h"
#include "htci_filelist.h"
#include "htci.h"
#include "libkernl_fileio.h"

static void htci_clear_cache(void);

extern char htci_dev_prefix[];
extern char htci_cur_dir[];
extern DvciCache htci_file_cache;
extern char htci_filelist_buf[];

/* ':' within the first 16 characters. */
/* 0x001cf080 */
static int htci_has_device(const char *s)
{
    unsigned int n;
    unsigned int i;

    n = strlen(s);
    if (n > 16)
        n = 16;
    for (i = 0; i < n; i++) {
        if (s[i] == ':')
            return 1;
    }
    return 0;
}

/* 0x001cf0e8 */
static int htci_is_host_path(const char *path)
{
    char head[6];

    memset(head, 0, 6);
    __builtin_memcpy(head, path, 4);
    head[5] = 0;
    return strcmp(head, "host") == 0;
}

/* Prefix unless the name has a device; "host" paths add current dir and '/'; '\\' -> '/'. */
/* 0x001cf140 */
void htci_make_host_path(const char *fname, char *out)
{
    int n;
    int i;
    char c;

    out[0] = 0;
    if (htci_has_device(fname) == 0)
        strcpy(out, htci_dev_prefix);
    if (htci_cur_dir[0] == 0 && (fname[0] == '\\' || fname[0] == '/'))
        fname++;
    if (htci_is_host_path(out) == 1) {
        strcat(out, htci_cur_dir);
        c = htci_cur_dir[strlen(htci_cur_dir) - 1];
        if (c != '\\' && c != '/' && c != 0 && fname[0] != '\\' && fname[0] != '/')
            strcat(out, "/");
    }
    strcat(out, fname);
    n = strlen(out);
    for (i = 0; i < n; i++) {
        if (out[i] == '\\')
            out[i] = '/';
    }
}

/* '\\' as '/'. */
/* 0x001cf270 */
static char htci_norm_char(char c)
{
    if (c == '\\')
        return '/';
    return c;
}

/* 0x001cf288 */
static int htci_cmp_char(char c1, char c2)
{
    if (htci_norm_char(c1) != htci_norm_char(c2))
        return -1;
    return 0;
}

/* Ignores separator style; 0 equal, else -1. */
/* 0x001cf2d8 */
static int htci_cmp_path(const char *path1, const char *path2)
{
    int n;
    int i;

    n = strlen(path1);
    if (n != strlen(path2))
        return -1;
    for (i = 0; i < n; i++) {
        if (htci_cmp_char(path1[i], path2[i]) != 0)
            return -1;
    }
    return 0;
}

/* One name per line; buf: nmax {fd, size} entries, then nmax names; becomes the cache if unset. */
/* 0x001cf378 */
static int htci_parse_filelist(char *buf, char *text, int nmax, int namelen)
{
    int n;
    int i;
    int st;
    int len;
    char *dst;

    st = 0;
    i = 0;
    n = 0;
    for (; text[i] != 0; i++) {
        if (text[i] == '\\')
            text[i] = '/';
        if (text[i] == '\n' || text[i] == 0) {
            len = i - st;
            if (text[i - 1] == '\r')
                text[i - 1] = 0;
            dst = buf + (nmax * 8 + (namelen * n + n));
            n++;
            memcpy(dst, text + st, len);
            st = i + 1;
            if (n == nmax)
                break;
        }
    }
    if (htci_file_cache.buf == 0) {
        htci_file_cache.buf = buf;
        htci_file_cache.namelen = namelen;
        htci_file_cache.nmax = nmax;
        htci_file_cache.nfile = n;
    }
    return n;
}

/* Up to 4096 bytes; 1 ok, 0 failure. */
/* 0x001cf4b8 */
int htci_read_filelist(const char *fname, char *buf)
{
    int fd;
    int size;
    char path[256];

    memset(path, 0, 256);
    htci_make_host_path(fname, path);
    fd = sceOpen(path, 1);
    if (fd < 0)
        return 0;
    size = sceLseek(fd, 0, 2);
    if (size < 0)
        return 0;
    if (sceLseek(fd, 0, 0) < 0)
        return 0;
    if (size > 4096)
        size = 4096;
    if (sceRead(fd, buf, size) < 0) {
        sceClose(fd);
        return 0;
    }
    sceClose(fd);
    return 1;
}

/* At most 13 files: opens each for fd and size (prints results), reopens in htci_open_mode. */
/* 0x001cf588 */
static int htci_open_cache(char *buf, int n)
{
    int nmiss;
    int i;
    int nok;
    int fd;
    int *ent;
    char *name;
    char path[256];

    nmiss = 0;
    i = 0;
    nok = 0;
    memset(path, 0, 256);
    if (n >= 14)
        n = 13;
    for (; i < n; i++) {
        ent = (int *)(buf + i * 8);
        name = buf + (htci_file_cache.nmax * 8 + (htci_file_cache.namelen * i + i));
        htci_make_host_path(name, path);
        if (name[0] == 0)
            continue;
        fd = sceOpen(path, 1);
        if (fd < 0) {
            printf("HTCI: \"%s\" can't Open.(sceOpen)\n", path);
            nmiss++;
            continue;
        }
        ent[0] = fd;
        nok++;
        ent[1] = htci_get_host_fsize(fd);
        printf("HTCI: \"%s\" Opened.(sceOpen)\n", name);
        sceClose(fd);
        htci_srd_wait_fd(sceOpen(path, htci_open_mode));
    }
    printf("HTCI: Total %d files.\n", nok);
    if (nmiss > 0)
        printf("HTCI: Error, Can't load all cache file.%c", 10);
    return nok;
}

/* 0 when there is none. */
/* 0x001cf708 */
static int htci_close_cache(void)
{
    char *buf;
    unsigned int n;
    unsigned int i;
    int nok;
    int *ent;
    char *name;
    char path[256];

    buf = htci_file_cache.buf;
    nok = 0;
    if (buf == 0)
        return 0;
    n = htci_file_cache.nfile;
    for (i = 0; i < n; i++) {
        ent = (int *)(buf + i * 8);
        name = buf + (htci_file_cache.nmax * 8 + (htci_file_cache.namelen * i + i));
        htci_make_host_path(name, path);
        if (name[0] == 0)
            continue;
        if (sceClose(ent[0]) < 0) {
            printf("HTCI: \"%s\" can't Close.(sceClose)\n", path);
        } else {
            nok++;
            printf("HTCI: \"%s\" Closed.(sceClose)\n", path);
        }
    }
    printf("HTCI: Total %d files\n", nok);
    htci_clear_cache();
    return 1;
}

/* Zeros when absent. */
/* 0x001cf838 */
static void htci_cache_search(HtciFile *file, const char *name, char *buf, int n)
{
    int i;
    unsigned int *ent;

    for (i = 0; i < n; i++) {
        ent = (unsigned int *)(buf + i * 8);
        if (htci_cmp_path(name, buf + (htci_file_cache.nmax * 8 + (htci_file_cache.namelen * i + i))) == 0) {
            file->fd = ent[0];
            file->size = ent[1];
            return;
        }
    }
    file->fd = 0;
    file->size = 0;
}

/* 0x001cf908 */
static void htci_clear_cache(void)
{
    memset(&htci_file_cache, 0, sizeof(DvciCache));
}

/* Zeros when absent. */
/* 0x001cf920 */
void htci_cache_lookup(const char *fname, HtciFile *file)
{
    file->fd = 0;
    file->size = 0;
    if (htci_file_cache.buf)
        htci_cache_search(file, fname, htci_file_cache.buf, htci_file_cache.nfile);
}

/* Closes the current cache, loads a list into buf (names up to namelen); bytes used, 0 on error. */
/* 0x001cf958 */
static int htci_load_filelist(const char *fname, char *buf, unsigned int size, int namelen)
{
    int nmax;
    int n;

    memset(htci_filelist_buf, 0, 4096);
    if (htci_file_cache.buf == 0)
        htci_clear_cache();
    htci_srd_wait();
    htci_close_cache();
    if (fname == 0 || buf == 0 || size == 0)
        return 0;
    if (htci_read_filelist(fname, htci_filelist_buf) == 0) {
        printf("HTCI: Can't read filelist file.(%s)\n", fname);
        htci_error(0, "E0111501:can't read filelist.(htCiLoadDirInfo)");
        return 0;
    }
    nmax = size / (namelen + 9);
    n = htci_parse_filelist(buf, htci_filelist_buf, nmax, namelen);
    n = htci_open_cache(buf, n);
    return size / (namelen + 9) * 8 + n * (namelen + 1);
}

/* As htci_load_filelist with 257-character names, 8 bytes per file. */
/* 0x001cfa80 */
static int htci_load_filelist_fixed(const char *fname, char *buf, unsigned int size)
{
    memset(htci_filelist_buf, 0, 4096);
    if (htci_file_cache.buf == 0)
        htci_clear_cache();
    htci_srd_wait();
    htci_close_cache();
    if (fname == 0 || buf == 0 || size == 0)
        return 0;
    if (htci_read_filelist(fname, htci_filelist_buf) == 0) {
        printf("HTCI: Can't read filelist file.(%s)\n", fname);
        htci_error(0, "E0111501:can't read filelist.(htCiLoadDirInfo)");
        return 0;
    }
    return htci_open_cache(buf, htci_parse_filelist(buf, htci_filelist_buf, size / 8, 257)) * 8;
}

/* 0x001cfb70 */
static int htci_is_cached(const char *fname)
{
    HtciFile file;

    htci_cache_lookup(fname, &file);
    return file.size != 0;
}

/* 1 -> mode 1, else 0x8001. */
/* 0x001cfb98 */
static void htci_set_open_mode(int mode)
{
    if (mode == 0)
        htci_open_mode = 0x8001;
    else if (mode == 1)
        htci_open_mode = 1;
    else
        htci_open_mode = 0x8001;
}

/* Null clears it. Bug: the '/' before a relative dir is overwritten (copied over, not appended). */
/* 0x001cfbc8 */
void htci_set_cur_dir(const char *dir)
{
    if (dir == 0) {
        htci_cur_dir[0] = 0;
        return;
    }
    if (dir[0] != '/' && dir[0] != '\\')
        __builtin_strcpy(htci_cur_dir, "/");
    else
        htci_cur_dir[0] = 0;
    strcpy(htci_cur_dir, dir);
}

/* 0x001cfc28 */
static char *htci_get_cur_dir(void)
{
    return htci_cur_dir;
}

/* Null = default "host:". */
/* 0x001cfc38 */
void htci_set_dev_prefix(const char *prefix)
{
    if (prefix)
        strcpy(htci_dev_prefix, prefix);
    else
        __builtin_strcpy(htci_dev_prefix, "host:");
}

/* 0x001cfc88 */
static char *htci_get_dev_prefix(void)
{
    return htci_dev_prefix;
}

/* 0x001cfc98 */
static int htci_get_open_mode(void)
{
    return htci_open_mode;
}
