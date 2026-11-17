/* ROFS directories: ISO 9660 dirs into sorted records, binary name search, path table lookup. */

#include "types_cri.h"
#include "libc.h"
#include "rofs_dir.h"
#include "adx_decode.h"
#include "adx_error.h"
#include "adx_lock.h"
#include "adx_peak.h"
#include "adxb.h"
#include "adxb_aiff.h"
#include "adxb_au.h"
#include "adxb_spsd.h"
#include "adxb_wave.h"
#include "adxd.h"
#include "adxstm.h"
#include "cri_lock.h"
#include "cvfs.h"
#include "dtr.h"
#include "dtx.h"
#include "htci.h"
#include "htci_filelist.h"
#include "kernel_syscalls.h"
#include "libcdvd.h"
#include "libgraph.h"
#include "libkernl_cache.h"
#include "libkernl_fileio.h"
#include "libkernl_iopheap.h"
#include "lsc.h"
#include "pfs_cvfs.h"
#include "ps2rna.h"
#include "rofs_conv.h"
#include "rofs_hn.h"
#include "rofs_if.h"
#include "rofs_mai.h"
#include "rofs_pfs.h"
#include "sjmem.h"
#include "sjrbf.h"
#include "sjrmt.h"
#include "sjuni.h"
#include "sjx.h"
#include "srd.h"
#include "svm.h"

#define CRI_MIN(a, b) ((a) < (b) ? (a) : (b))

#define CRI_SW16(x) (((x) >> 8 & 0xff) | ((x) << 8 & 0xff00))

#define CRI_SW32(x) (((x) >> 24 & 0xff) | ((x) >> 8 & 0xff00) | ((x) << 8 & 0xff0000) | (x) << 24)

#define ROFS_U32(p) (((RofsU32 *)(p))->v)

/* volume entry, 32 bytes */
typedef struct {
    char name[8];       /* 00 */
    char pad_08[4];
    void *arg;          /* 0c */
    char pad_10[12];
    void *dir;          /* 1c */
} RofsDirRofsVol;

#define ROFS_SW32(x) (((x) & 0xff) << 24 | ((x) >> 8 & 0xff) << 16 | ((x) >> 16 & 0xff) << 8 | ((x) >> 24 & 0xff))

/* Same swap on a value in memory (low byte read as a byte). */
#define ROFS_SW32M(x) ((unsigned char)(x) << 24 | ((x) >> 8 & 0xff) << 16 | ((x) >> 16 & 0xff) << 8 | ((x) >> 24 & 0xff))

/* Always 0. */
#define AZ(p) ((((int)(p)) & 1) >> 1)

/* ISO 9660 path table record (unaligned) */
typedef struct {
    unsigned char len;
    unsigned char extlen;
    int lsn;
    unsigned short parent;
    char name[1];
} __attribute__((packed)) RofsPtRec;

extern void lsc_set_error_cb(void *fn, void *obj);

extern void lsc_open_entry(LscObj *lsc);

extern void lsc_poll_stream(LscObj *lsc);
extern void lsc_close_entry(LscObj *lsc);

extern unsigned long __moddi3(unsigned long a, unsigned long b);

extern unsigned long __divdi3(unsigned long a, unsigned long b);

extern int sceCdRead(int lsn, int nsct, void *buf, SrdMode *mode);

extern void *RFHN_CreateDirhn(void *area, int ndir);

/* 0x001dd8a8 */
static int rofs_toupper(char c)
{
    if (c >= 'a' && c <= 'z')
        c -= 32;
    return c;
}

/* Ignores case; 1 if equal. */
/* 0x001dd8d0 */
static int rofs_name_equal(char *name1, char *name2, int n)
{
    int c;

    for (; n > 0; n--) {
        if (*name1 != 0) {
            c = rofs_toupper(*name1++);
            if (c == rofs_toupper(*name2++))
                continue;
            return 0;
        }
        if (*name2 == 0)
            return 1;
        return 0;
    }
    return 1;
}

/* 0x001dd978 */
static int rofs_strcspn(char *s, char *set)
{
    int n;
    char *p;

    n = 0;
    while (*s != 0) {
        for (p = set; *p != 0; p++) {
            if (*s == *p)
                return n;
        }
        s++;
        n++;
    }
    return n;
}

/* ";1" dropped for files; "." and ".." for the special names. Length, 0 none, -1 sector padding. */
/* 0x001dd9e0 */
static int rofs_unpack_dir_rec(unsigned char *rec, char *out)
{
    char *name;
    int len;
    int flags;

    if (rec == 0 || out == 0)
        return 0;
    if (rec[0] < 34)
        return -1;
    *(int *)out = ROFS_U32(rec + 2);
    *(int *)(out + 4) = ROFS_U32(rec + 10);
    flags = rec[25];
    out[8] = flags;
    name = out + 10;
    len = rec[32];
    if ((flags & 2) == 0)
        len -= 2;
    strncpy(name, (char *)rec + 33, len);
    name[len] = 0;
    if (out[8] & 2) {
        if (out[10] == 0) {
            out[11] = 0;
            out[10] = '.';
        } else if (out[10] == 1) {
            out[12] = 0;
            out[10] = '.';
            out[11] = '.';
        }
    }
    return rec[0];
}

/* Two sectors at a time; first entry gives the size; kept if loaded. -109 when it did not fit. */
/* 0x001ddad0 */
static int getDirEnt(RofsVol *vol, char *buf, int lsn, RofsDir *dir, int size)
{
    char *p;
    RofsDirEnt *ent;
    int i;
    int max;
    int n;
    int ret;
    int off;
    int nbytes;

    if (vol == 0 || buf == 0 || dir == 0) {
        rofs_error("rofs_dir.c", 292, "getDirEnt", -100);
        return -100;
    }
    if (dir->unk_08 == lsn && strcmp(dir->name, vol->name) == 0)
        return 0;
    dir->unk_08 = lsn;
    strcpy(dir->name, vol->name);
    ent = dir->ent;
    i = 0;
    p = buf;
    max = dir->max;
    memset(dir->ent, 0, max * sizeof(RofsDirEnt));
    while (size > 0) {
        n = 2;
        if (size < n)
            n = size;
        ret = RFMAI_ReadWG(vol, lsn, n, buf);
        if (ret != n) {
            dir->unk_08 = 0;
            return rofs_error("rofs_dir.c", 326, "getDirEnt", ret);
        }
        p = buf;
        while (i < max) {
            ret = rofs_unpack_dir_rec(p, ent);
            if (ret <= 0) {
                if (ret != -1)
                    break;
                off = (p - buf + 2048) & 0xfffff800;
                p = buf + off;
                if (p >= buf + (n << 11) || size == off)
                    break;
                continue;
            }
            if (AZ(p))
                dir->n = ret + 1;
            nbytes = n << 11;
            if (i == 0)
                size = ent->size;
            p += ret;
            ent++;
            i++;
            {
                int end = (int)(buf + nbytes);
                int inside = p < (char *)end;

                end = inside + AZ(inside);
                if (!end)
                    break;
            }
        }
        if (i >= max)
            break;
        lsn += n;
        size -= n << 11;
    }
    dir->n = i;
    if (i == max && size != 0) {
        if (size != 2048 || (p < buf + 2048 && *p != 0)) {
            dir->unk_08 = 0;
            return -109;
        }
    }
    return 0;
}

/* Vsync counter running meanwhile. */
/* 0x001ddd38 */
int rofs_read_dir(RofsVol *vol, char *buf, int lsn, RofsDir *dir, int size)
{
    int ret;

    rofs_install_count_cb();
    ret = getDirEnt(vol, buf, lsn, dir, size);
    rofs_restore_cb();
    return ret;
}

/* 0x001dddb0 */
static int rofs_has_dot(const char *s)
{
    while (*s) {
        if (*s++ == '.')
            return 1;
    }
    return 0;
}

/* Binary search, no case; extensionless names get a '.'. Entry + 1 ("." 1, ".." 2), 0 not found. */
/* 0x001dddf0 */
int RFDIR_SearchName(const char *fname, RofsDir *dir, int reserved)
{
    RofsDirEnt *ent;
    unsigned int lo;
    unsigned int hi;
    unsigned int mid;
    int found;
    unsigned int n;
    int ret;
    RofsDirEnt *pent;
    char name[512];
    char fname2[512];

    ent = dir->ent;
    n = dir->n;
    if (fname != 0) {
        if (strcmp(fname, ".") == 0)
            return 1;
        if (strcmp(fname, "..") == 0)
            return 2;
        strcpy(name, fname);
        hi = n;
        lo = 2;
        rofs_to_upper(name);
        mid = 0;
        strcpy(fname2, name);
        found = 0;
        if (rofs_has_dot(fname2) != 1)
            strcat(fname2, ".");
        while (lo <= hi) {
            mid = (lo + hi) >> 1;
            pent = &ent[mid];
            ret = strcmp(pent->name, fname2);
            if (pent->flags & 2)
                ret = strcmp(pent->name, name);
            else
                ret = strcmp(pent->name, fname2);
            if (ret == 0) {
                found = 1;
                break;
            }
            if (ret < 0)
                lo = mid + 1;
            else
                hi = mid - 1;
        }
        if (found == 1)
            return mid + 1;
        return 0;
    }
    return 0;
}

/*
 * Path table read a sector at a time (a record cut by the sector end moves to the buffer front),
 * one '/' or '\\' component at a time; returns the directory's first sector, or 0.
 */
/* 0x001ddf70 */
static int rofs_search_path_table(void *vol, unsigned char *buf, int lsn, int ptsize, const char *path)
{
    unsigned char *p;
    unsigned char *end;
    unsigned char *rebased;
    int pos;
    int seg;
    int idx;
    int parent;
    const char *name;
    int n;
    int ret;
    unsigned char *cut_rec;

    parent = 1;
    p = buf + 2048;
    pos = 0;
    seg = 0;
    end = p;
    idx = 1;
    name = path + 1;
    do {
        if (p >= end || p + (p[0] + 8 + (p[0] & 1)) >= end) {
            rebased = p - 2048;
            n = buf - rebased;
            cut_rec = p;
            p = buf - n;
            memcpy(p, cut_rec, n);
            ret = RFMAI_ReadWG(vol, lsn, 1, buf);
            if (ret != 1)
                return rofs_error("rofs_dir.c", 596, "RFMAI_ReadWG", ret);
            lsn++;
        }
        if (((RofsPtRec *)p)->parent == parent) {
            if (idx == 1 || (p[0] == seg && rofs_name_equal(p + 8, name, p[0]) != 0)) {
                parent = idx;
                name += seg;
                if (*name == 0)
                    return ((RofsPtRec *)p)->lsn;
                if (idx != 1)
                    name++;
                seg = rofs_strcspn(name, "/\\");
            }
        }
        pos += p[0] + 8 + (p[0] & 1);
        p += p[0] + 8 + (p[0] & 1);
        idx++;
    } while (pos < ptsize);
    return 0;
}

/* Vsync counter running meanwhile. */
/* 0x001de140 */
int RFDIR_SearchPath(void *vol, void *buf, void *ptloc, void *ptsize, void *path)
{
    int ret;

    rofs_install_count_cb();
    ret = rofs_search_path_table(vol, buf, ptloc, ptsize, path);
    rofs_restore_cb();
    return ret;
}
