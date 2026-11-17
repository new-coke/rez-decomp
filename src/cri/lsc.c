/*
 * Stream play list: eight lists of up to sixteen files (or sector ranges) streamed back to back
 * into one joint via ADXSTM, heard as one stream. Lists can repeat; callback when empty.
 */

#include "types_cri.h"
#include "lsc.h"
#include <stdarg.h>
#include "libc.h"
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
#include "pfs_cvfs.h"
#include "ps2rna.h"
#include "rofs_conv.h"
#include "rofs_dir.h"
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

/* Play list as the stream-entry code sees it (same 1080 bytes as LscObj). */
typedef struct {
    signed char used;   /* 00 */
    signed char stat;   /* 01 */
    signed char ready;  /* 02 */
    signed char cbflag; /* 03 */
    signed char pause;  /* 04 */
    char pad_05[3];
    void *sj;           /* 08 */
    char pad_0c[8];
    int fill_thr;          /* 14 */
    int rq_size;          /* 18 */
    char pad_1c[4];
    int rdidx;          /* 20 */
    int nstm;           /* 24 */
    void *fp;
    int fsize;          /* 2c: sectors */
    char pad_30[8];
    LscStm stm[16];     /* 38 */
} LscObjView;

#define CRI_SW16(x) (((x) >> 8 & 0xff) | ((x) << 8 & 0xff00))

#define CRI_SW32(x) (((x) >> 24 & 0xff) | ((x) >> 8 & 0xff00) | ((x) << 8 & 0xff0000) | (x) << 24)

#define ROFS_U32(p) (((RofsU32 *)(p))->v)

/* ROFS volume entry, 32 bytes */
typedef struct {
    char name[8];       /* 00 */
    char pad_08[4];
    void *arg;          /* 0c */
    char pad_10[12];
    void *dir;          /* 1c */
} LscRofsVol;

#define ROFS_SW32(x) (((x) & 0xff) << 24 | ((x) >> 8 & 0xff) << 16 | ((x) >> 16 & 0xff) << 8 | ((x) >> 24 & 0xff))

/* Same swap on a value in memory (low byte read as a byte). */
#define ROFS_SW32M(x) ((unsigned char)(x) << 24 | ((x) >> 8 & 0xff) << 16 | ((x) >> 16 & 0xff) << 8 | ((x) >> 24 & 0xff))

extern void (*lsc_stm_fn)(void *a, void *b);
extern void *lsc_stm_obj;
extern void *lsc_stm_obj2;
extern void (*lsc_error_fn)(void *obj, const char *msg);
extern void *lsc_error_obj;
extern char lsc_error_buf[];

extern void lsc_open_entry(LscObjView *lsc);

extern void lsc_poll_stream(LscObjView *lsc);
extern void lsc_close_entry(LscObjView *lsc);

extern unsigned long __moddi3(unsigned long a, unsigned long b);

extern unsigned long __divdi3(unsigned long a, unsigned long b);

extern int sceCdRead(int lsn, int nsct, void *buf, SrdMode *mode);

extern void *RFHN_CreateDirhn(void *area, int ndir);

extern void lsc_exec_handle(LscObjView *lsc);
extern void lsc_set_error_cb(void (*fn)(void *obj, const char *msg), void *obj);

/* 0 if none. */
/* 0x001cfca8 */
LscObj *lsc_alloc(void)
{
    int i;
    LscObj *lsc;

    lsc = 0;
    for (i = 0; i < 8; i++) {
        if (lsc_obj_tbl[i].used == 0) {
            lsc = &lsc_obj_tbl[i];
            break;
        }
    }
    return lsc;
}

/* Keeps sj at least 80% full by default; 0 when sj is null or no list is free. */
/* 0x001cfd00 */
LscObj *LSC_Create(SjObj *sj)
{
    LscObj *lsc;
    int n;
    int i;
    int lk[2];

    if (sj == 0) {
        lsc_error("E0001: Illigal parameter=sj (LSC_Create)");
        return 0;
    }
    lsc_lock(lk);
    lsc = lsc_alloc();
    if (lsc == 0) {
        lsc_error("E0002: Not enough instance (LSC_Create)");
    } else {
        lsc->stat = 0;
        lsc->sj = sj;
        n = sj->vtbl->getnum(sj, 0);
        n += sj->vtbl->getnum(sj, 1);
        lsc->bufsize = n;
        lsc->min = n * 8 / 10;
        for (i = 0; i < 16; i++)
            lsc->ent[i].stat = 0;
        lsc->used = 1;
    }
    lsc_unlock(lk);
    return lsc;
}

/* 0x001cfe08 */
void lsc_destroy(LscObj *lsc)
{
    int lk[2];

    if (lsc) {
        lsc_lock(lk);
        lsc_stop(lsc);
        lsc->used = 0;
        memset(lsc, 0, sizeof(LscObj));
        lsc_unlock(lk);
    }
}

/* Opens it once for its size; returns its stream id. */
/* 0x001cfe58 */
int lsc_entry_fname(LscObj *lsc, const char *fname)
{
    void *hn;
    int nsct;

    hn = adxstm_open_fname_rt(fname, lsc->sj);
    if (hn == 0)
        lsc_error("E0004: Can not open '%s'", fname);
    nsct = (adxstm_get_fsize_byte(hn) + 2047) / 2048;
    adxstm_close(hn);
    return lsc_entry_file(lsc, fname, 0, 0, nsct);
}

/* nsct sectors from sector x at the back, next stream id; wakes an empty list; -1 full/bad args. */
/* 0x001cff00 */
int lsc_entry_file(LscObj *lsc, const char *fname, void *dir, int x, int nsct)
{
    LscEnt *ent;
    int idx;
    int prev;
    int id;

    if (lsc == 0) {
        lsc_error("E0003: Illigal parameter lsc=NULL");
        return -1;
    }
    if (lsc->nstm >= 16)
        return -1;
    if (fname == 0) {
        lsc_error("E0011: Illigal parameter fname=%s", 0);
        return -1;
    }
    ent = &lsc->ent[lsc->wr];
    idx = (lsc->wr + 15) % 16;
    prev = lsc->ent[idx].id;
    id = prev + 1;
    if (prev == 0x7fffffff)
        id = 0;
    ent->id = id;
    strncpy(ent->fname, fname, 40);
    ent->ofst = x;
    ent->nsct = nsct;
    ent->dir = dir;
    ent->stat = 0;
    ent->rdsct = 0;
    lsc->wr = (lsc->wr + 1) % 16;
    lsc->nstm++;
    if (lsc->stat == 1)
        lsc->stat = 2;
    return id;
}

/* Streams its files (state 2) or waits for files (state 1). */
/* 0x001d0060 */
void lsc_start(LscObj *lsc)
{
    int lk[2];

    if (lsc == 0) {
        lsc_error("E0003: Illigal parameter lsc=NULL");
        return;
    }
    lsc_lock(lk);
    if (lsc->stat != 0)
        lsc_stop(lsc);
    if (lsc->nstm > 0)
        lsc->stat = 2;
    else
        lsc->stat = 1;
    lsc_unlock(lk);
}

/* Closes the file being streamed, empties the list. */
/* 0x001d00d0 */
void lsc_stop(LscObj *lsc)
{
    int lk[2];

    if (lsc == 0) {
        lsc_error("E0003: Illigal parameter lsc=NULL");
        return;
    }
    lsc_lock(lk);
    if (lsc->stm) {
        if (lsc->reading == 1) {
            adxstm_stop(lsc->stm);
            lsc->reading = 0;
        }
        adxstm_close(lsc->stm);
        lsc->stm = 0;
    }
    lsc->fsize = 0;
    lsc->wr = 0;
    lsc->rd = 0;
    lsc->nstm = 0;
    lsc->unk_34 = 0;
    lsc->stat = 0;
    lsc_unlock(lk);
}

/* 1 pause, 0 resume. */
/* 0x001d0160 */
void lsc_set_flag04(LscObj *lsc, int flag)
{
    if (lsc == 0)
        lsc_error("E0003: Illigal parameter lsc=NULL");
    else if (flag == 1)
        lsc->unk_04 = flag;
    else
        lsc->unk_04 = 0;
}

/* 0x001d0190 */
void lsc_exec_server(void)
{
    int i;
    int lk[2];

    lsc_lock(lk);
    for (i = 0; i < 8; i++) {
        if (lsc_obj_tbl[i].used == 1)
            lsc_exec_handle(&lsc_obj_tbl[i]);
    }
    lsc_unlock(lk);
}

/* 0 stopped, 1 out of files, 2 streaming, 3 read error; -1 on error. */
/* 0x001d0200 */
int lsc_get_stat(LscObj *lsc)
{
    if (lsc == 0) {
        lsc_error("E0003: Illigal parameter lsc=NULL");
        return -1;
    }
    return lsc->stat;
}

/* -1 on error. */
/* 0x001d0230 */
int lsc_get_nstm(LscObj *lsc)
{
    if (lsc == 0) {
        lsc_error("E0003: Illigal parameter lsc=NULL");
        return -1;
    }
    return lsc->nstm;
}

/* From the front; -1 on error. */
/* 0x001d0260 */
int lsc_get_stm_id(LscObj *lsc, int no)
{
    int idx;

    if (lsc == 0) {
        lsc_error("E0003: Illigal parameter lsc=NULL");
        return -1;
    }
    if (no < 0 || no >= lsc->nstm) {
        lsc_error("E0009: Illigal parameter no=%d", no);
        return -1;
    }
    idx = (lsc->rd + no) % 16;
    return lsc->ent[idx].id;
}

/* 0 on error. */
/* 0x001d02f0 */
char * lsc_get_fname(LscObj *lsc, int id)
{
    int i;

    if (lsc == 0) {
        lsc_error("E0003: Illigal parameter lsc=NULL");
        return 0;
    }
    for (i = 0; i < 16; i++) {
        if (lsc->ent[i].id == id)
            break;
    }
    if (i == 16) {
        lsc_error("E0012: Can not find stream ID =%d", id);
        return 0;
    }
    return lsc->ent[i].fname;
}

/* 0 waiting, 1 streaming, 2 done; -1 on error. */
/* 0x001d0378 */
int lsc_get_stm_stat(LscObj *lsc, int id)
{
    int i;

    if (lsc == 0) {
        lsc_error("E0003: Illigal parameter lsc=NULL");
        return -1;
    }
    for (i = 0; i < 16; i++) {
        if (lsc->ent[i].id == id)
            break;
    }
    if (i == 16) {
        lsc_error("E0012: Can not find stream ID =%d", id);
        return -1;
    }
    return lsc->ent[i].stat;
}

/* Sectors of stream id read so far; 0 on error. */
/* 0x001d0400 */
int lsc_get_stm_word3c(LscObj *lsc, int id)
{
    int i;

    if (lsc == 0) {
        lsc_error("E0003: Illigal parameter lsc=NULL");
        return 0;
    }
    for (i = 0; i < 16; i++) {
        if (lsc->ent[i].id == id)
            break;
    }
    if (i == 16) {
        lsc_error("E0012: Can not find stream ID =%d", id);
        return 0;
    }
    return lsc->ent[i].rdsct;
}

/* Bytes, 0 .. joint size. */
/* 0x001d0488 */
void lsc_set_min_fill(LscObj *lsc, int min)
{
    if (lsc == 0)
        lsc_error("E0003: Illigal parameter lsc=NULL");
    else if (min < 0 || min > lsc->bufsize)
        lsc_error("E0010: Illigal parameter min=%d", min);
    else
        lsc->min = min;
}

/* -1 on error. */
/* 0x001d04c8 */
int lsc_get_min_fill(LscObj *lsc)
{
    if (lsc == 0) {
        lsc_error("E0003: Illigal parameter lsc=NULL");
        return -1;
    }
    return lsc->min;
}

/* Called when a list runs out of files; null fn clears all three. */
/* 0x001d04f8 */
void lsc_set_stm_cb(void (*fn)(void *obj, void *obj2), void *obj, void *obj2)
{
    if (fn == 0) {
        lsc_stm_fn = 0;
        lsc_stm_obj = 0;
        lsc_stm_obj2 = 0;
    } else {
        lsc_stm_fn = fn;
        lsc_stm_obj = obj;
        lsc_stm_obj2 = obj2;
    }
}

/* 0x001d0520 */
void lsc_call_stm_cb(void)
{
    if (lsc_stm_fn)
        lsc_stm_fn(lsc_stm_obj, lsc_stm_obj2);
}

/* Repeat: each finished file is re-added at the back. */
/* 0x001d0548 */
void lsc_set_flag(LscObj *lsc, int flag)
{
    if (lsc == 0)
        lsc_error("E0003: Illigal parameter lsc=NULL");
    else
        lsc->flag = flag;
}

/* Stream joint lock; lk unused. */
/* 0x001d0568 */
void lsc_lock(void *lk)
{
    do {
        sj_lock();
    } while (0);
}

/* 0x001d0588 */
void lsc_unlock(void *lk)
{
    sj_unlock();
}

/* Null fn clears both. */
/* 0x001d0590 */
void lsc_set_error_cb(void (*fn)(void *obj, const char *msg), void *obj)
{
    if (fn == 0) {
        lsc_error_fn = 0;
        lsc_error_obj = 0;
    } else {
        lsc_error_fn = fn;
        lsc_error_obj = obj;
    }
}

/* printf-style. */
/* 0x001d05b0 */
void lsc_error(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsprintf(lsc_error_buf, fmt, ap);
    if (lsc_error_fn)
        lsc_error_fn(lsc_error_obj, lsc_error_buf);
}

/* 0x001d0618 */
void lsc_lib_init_hook(void)
{
}

/* 0x001d0620 */
void lsc_lib_finish_hook(void)
{
}

/* First call clears lists and error callback; counts calls. */
/* 0x001d0628 */
void lsc_lib_init(void)
{
    int lock[4];

    lsc_ver_str;
    lsc_lock(lock);
    if (lsc_init_count == 0) {
        memset(lsc_obj_tbl, 0, 8640);
        lsc_lib_init_hook();
        lsc_set_error_cb(0, 0);
    }
    lsc_init_count++;
    lsc_unlock(lock);
}

/* Last user: destroys every list. */
/* 0x001d0690 */
void lsc_lib_finish(void)
{
    int lock[4];
    int i;

    lsc_lock(lock);
    if (--lsc_init_count == 0) {
        for (i = 0; i < 8; i++) {
            if (lsc_obj_tbl[i].used == 1)
                lsc_destroy(&lsc_obj_tbl[i]);
        }
        memset(lsc_obj_tbl, 0, 8640);
        lsc_lib_finish_hook();
        lsc_set_error_cb(0, 0);
    }
    lsc_unlock(lock);
}

/* Front file as a real-time stream into the joint; error if a file is open with the list empty. */
/* 0x001d0740 */
void lsc_open_entry(LscObjView *lsc)
{
    LscStm *stm;
    int size;

    stm = &lsc->stm[lsc->rdidx];
    if (lsc->nstm > 0) {
        if (lsc->fp == 0) {
            lsc->fp = ADXSTM_OpenFnameRangeExRt(stm->fname.s, stm->ofst, stm->nsct, stm->range_nsct, lsc->sj);
            if (lsc->fp == 0)
                lsc_error("E0004: Can not open '%s'", stm->fname.s);
            size = adxstm_get_fsize_byte(lsc->fp);
            lsc->ready = 0;
            lsc->fsize = (size + 2047) / 2048;
            stm->rdsct = 0;
        }
        if (lsc->ready == 0) {
            adxstm_set_thresholds(lsc->fp, lsc->fill_thr, lsc->rq_size);
            adxstm_seek(lsc->fp, 0);
            adxstm_start(lsc->fp);
            lsc->ready = 1;
        }
        stm->stat = 1;
    } else if (lsc->fp != 0) {
        lsc_error("E0006: Illigal member nstm='%d' fp='%08x'h", lsc->nstm, lsc->fp);
    }
}

/* Records sectors read; done at file end; read error -> list error state. */
/* 0x001d0860 */
void lsc_poll_stream(LscObjView *lsc)
{
    LscStm *stm;
    int st;

    if (lsc->fp == 0) {
        lsc_error("E0007: lsc->fp=NULL");
        return;
    }
    stm = &lsc->stm[lsc->rdidx];
    st = adxstm_get_stat(lsc->fp);
    if (st == 4) {
        lsc->stat = 3;
    } else if (st == 2) {
        stm->rdsct = adxstm_tell(lsc->fp);
    } else if (st == 3) {
        stm->rdsct = lsc->fsize;
        stm->stat = 2;
    }
}

/* Next file; when out: callback, wait (state 1). Repeating lists re-add the file at the back. */
/* 0x001d0908 */
void lsc_close_entry(LscObjView *lsc)
{
    LscStm *stm;
    int ofst;
    int nsct;
    int arg;

    arg = 0;
    ofst = 0;
    nsct = 0;
    if (lsc->fp != 0) {
        adxstm_close(lsc->fp);
        lsc->fp = 0;
        if (lsc->cbflag == 1) {
            stm = &lsc->stm[lsc->rdidx];
            lsc_fname_buf = stm->fname;
            ofst = stm->ofst;
            nsct = stm->nsct;
            arg = stm->range_nsct;
        }
        lsc->rdidx = (lsc->rdidx + 1) % 16;
        lsc->nstm--;
        if (lsc->nstm <= 0) {
            lsc_call_stm_cb();
            lsc->stat = 1;
        }
        if (lsc->cbflag == 1)
            lsc_entry_file(lsc, &lsc_fname_buf, ofst, nsct, arg);
    }
}

/* Streaming, unpaused lists: polls the front file, closes it when done, opens the next. */
/* 0x001d0a80 */
void lsc_exec_handle(LscObjView *lsc)
{
    if (lsc->pause != 1 && lsc->stat == 2 && lsc->nstm > 0) {
        if (lsc->stm[lsc->rdidx].stat == 1)
            lsc_poll_stream(lsc);
        if (lsc->stm[lsc->rdidx].stat == 2)
            lsc_close_entry(lsc);
        if (lsc->stm[lsc->rdidx].stat == 0)
            lsc_open_entry(lsc);
    }
}
