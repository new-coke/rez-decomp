/*
 * ADX file streams: forty handles reading a file or sector range via cvFs into a stream joint
 * whenever it falls below its refill level, with retries. Real-time range plus file-read range.
 */

#include "cri.h"
#include "libc.h"
#include "adxstm.h"
#include "adx_error.h"
#include "adx_lock.h"
#include "cvfs.h"
#include "sjuni.h"

extern int adxstm_retry_count;

/* 0x001c83e0 */
static void adxstm_set_rt_count(int x)
{
    adxstm_rt_count = x;
}

/* File-read range ends at the table end. */
/* 0x001c83e8 */
static void adxstm_set_nrt_count(int x)
{
    adxstm_nrt_count = x;
    adxstm_nrt_first = 40 - x;
}

/* Returns 1. */
/* 0x001c8400 */
int adxstm_lib_init(void)
{
    memset(adxstm_obj_tbl, 0, 2560);
    return 1;
}

/* 0x001c8430 */
static void adxstm_nop(void)
{
}

/* Shuts cvFs down. */
/* 0x001c8438 */
void adxstm_lib_finish(void)
{
    cvfs_lib_finish();
}

/* fsize in bytes; stopped, 512-sector request limit. */
/* 0x001c8440 */
static void adxstm_setup_range(AdxstmObj *stm, void *fd, int ofst, int fsize, int x)
{
    stm->sj = x;
    stm->fd = fd;
    stm->ofst = ofst;
    stm->maxsct = 512;
    stm->stat = 1;
    stm->rdflg = 0;
    stm->start_sct = 0;
    stm->fsize = fsize;
    stm->fsct = (fsize + 2047) / 2048;
    stm->used = 1;
}

/* 0 if none free. */
/* 0x001c8490 */
static AdxstmObj *adxstm_alloc_rt(void *fd, int ofst, int fsize, int x)
{
    int i;
    AdxstmObj *stm;

    stm = 0;
    for (i = 0; i < adxstm_rt_count; i++) {
        stm = &adxstm_obj_tbl[adxstm_rt_first + i];
        if (stm->used == 0)
            break;
    }
    if (i == adxstm_rt_count)
        return 0;
    adxstm_setup_range(stm, fd, ofst, fsize, x);
    return stm;
}

/* 0 if none free. */
/* 0x001c8540 */
static AdxstmObj *adxstm_alloc_nrt(void *fd, int ofst, int fsize, int x)
{
    int i;
    AdxstmObj *stm;

    stm = 0;
    for (i = 0; i < adxstm_nrt_count; i++) {
        stm = &adxstm_obj_tbl[adxstm_nrt_first + i];
        if (stm->used == 0)
            break;
    }
    if (i == adxstm_nrt_count)
        return 0;
    adxstm_setup_range(stm, fd, ofst, fsize, x);
    return stm;
}

/* 0x001c85f0 */
static void adxstm_clear_handle(AdxstmObj *stm)
{
    memset(stm, 0, 64);
}

/* File-read range handle at the range start; reports the file name on failure. */
/* 0x001c8600 */
AdxstmObj *ADXSTM_OpenFnameRange(const char *fname, void *dir, int ofst, int nsct, int x)
{
    void *fd;
    AdxstmObj *stm;

    fd = cvFsOpen(fname, dir, 0);
    if (fd == 0) {
        adx_error_cat("E0041206 ADXSTM_OpenFnameRang: can't open ", fname);
        return 0;
    }
    stm = adxstm_alloc_nrt(fd, ofst, nsct << 11, x);
    if (stm != 0)
        adxstm_seek(stm, 0);
    return stm;
}

/* As ADXSTM_OpenFnameRange, real-time range. */
/* 0x001c8698 */
AdxstmObj *ADXSTM_OpenFnameRangeExRt(const char *fname, void *dir, int ofst, int nsct, int x)
{
    void *fd;
    AdxstmObj *stm;

    fd = cvFsOpen(fname, dir, 0);
    if (fd == 0) {
        adx_error_cat("E00041205 ADXSTM_OpenFnameRangeExRt: can't open ", fname);
        return 0;
    }
    stm = adxstm_alloc_rt(fd, ofst, nsct << 11, x);
    if (stm != 0)
        adxstm_seek(stm, 0);
    return stm;
}

/* Whole file (dir or current); file-read range; reports the file name on failure. */
/* 0x001c8730 */
AdxstmObj *ADXSTM_OpenFnameEx(const char *fname, void *dir, int x)
{
    int fsize;
    void *fd;
    AdxstmObj *stm;

    if (dir == 0)
        fsize = cvFsGetFileSize(fname);
    else
        fsize = cvFsGetFileSizeEx(fname, dir);
    fd = cvFsOpen(fname, dir, 0);
    if (fd == 0) {
        adx_error_cat("E00041205 ADXSTM_OpenFnameEx: can't open ", fname);
        return 0;
    }
    stm = adxstm_alloc_nrt(fd, 0, fsize, x);
    if (stm != 0)
        adxstm_seek(stm, 0);
    return stm;
}

/* Whole file, real-time range. */
/* 0x001c87f0 */
AdxstmObj *adxstm_open_fname_rt(const char *fname, int x)
{
    int fsize;
    void *fd;
    AdxstmObj *stm;

    fsize = cvFsGetFileSize(fname);
    fd = cvFsOpen(fname, 0, 0);
    if (fd == 0) {
        adx_error_cat("E00041205 ADXSTM_OpenFnameEx: can't open ", fname);
        return 0;
    }
    stm = adxstm_alloc_rt(fd, 0, fsize, x);
    if (stm != 0)
        adxstm_seek(stm, 0);
    return stm;
}

/* Current directory. */
/* 0x001c8888 */
static AdxstmObj *adxstm_open_fname_range_rt(const char *fname, int ofst, int nsct, int x)
{
    return ADXSTM_OpenFnameRangeExRt(fname, 0, ofst, nsct, x);
}

/* Stops, closes the file, clears the handle. */
/* 0x001c88b8 */
void adxstm_close(AdxstmObj *stm)
{
    if (stm != 0) {
        adxstm_stop(stm);
        if (stm->fd != 0)
            cvFsClose(stm->fd);
        adxstm_clear_handle(stm);
    }
}

/* 1 stopped, 2 reading, 3 at end, 4 read error. */
/* 0x001c8910 */
int adxstm_get_stat(AdxstmObj *stm)
{
    return stm->stat;
}

/* Returns the resulting sector within the range. */
/* 0x001c8918 */
int adxstm_seek(AdxstmObj *stm, int sct)
{
    cvFsSeek(stm->fd, stm->ofst + sct, 0);
    return cvFsTell(stm->fd) - stm->ofst;
}

/* Within the range; 0 without a file. */
/* 0x001c8960 */
int adxstm_tell(AdxstmObj *stm)
{
    int pos;

    if (stm->fd != 0)
        return cvFsTell(stm->fd) - stm->ofst;
    return 0;
}

/* An empty file ends at once; returns 1. */
/* 0x001c89a0 */
int adxstm_start(AdxstmObj *stm)
{
    if (stm->fsize == 0) {
        stm->start_sct = 0;
        stm->stat = 3;
        stm->total_rd = 0;
    } else {
        stm->total_rd = 0;
        stm->stat = 2;
        stm->start_sct = adxstm_tell(stm);
        stm->rtycnt = 0;
    }
    return 1;
}

/* Cancels a transfer in progress. */
/* 0x001c8a00 */
void adxstm_stop(AdxstmObj *stm)
{
    adx_lock();
    if (stm->rdflg == 1 && stm->fd != 0) {
        cvFsStopTr(stm->fd);
        stm->rdflg = 0;
    }
    adx_unlock();
    stm->stat = 1;
}

/* Called at the end of the range. */
/* 0x001c8a60 */
void adxstm_set_end_cb(AdxstmObj *stm, int fn, int obj)
{
    stm->end_cb = fn;
    stm->end_obj = obj;
}

/* Negative = whole file (byte size rounded up). */
/* 0x001c8a70 */
void adxstm_set_size_sct(AdxstmObj *stm, int nsct)
{
    if (nsct >= 0)
        stm->fsct = nsct;
    else
        stm->fsct = (stm->fsize + 2047) / 2048;
}

/* Counts a server pass without a usable stream joint. */
/* 0x001c8aa8 */
static void adxstm_count_retry(void)
{
    adxstm_retry_count++;
}

/*
 * Completes a finished read (data to the joint; end callback at range end) or retries a failed
 * one (error after adxstm_retry_limit); below refill level starts a read of up to maxsct sectors.
 */
/* 0x001c8ab8 */
static void adxstm_exec_read(AdxstmObj *stm)
{
    SjChunk head;
    SjChunk tail;
    SjObj *sj;
    int st;
    int n;
    int rest;
    int nb;

    sj = (SjObj *)stm->sj;
    if (stm->rdflg == 1) {
        st = cvFsGetStat(stm->fd);
        if (st == 1) {
            nb = stm->nrd << 11;
            sj_split_chunk(&stm->ck, nb, &head, &tail);
            sj->vtbl->putchunk(sj, 1, &head);
            sj->vtbl->ungetchunk(sj, 0, &tail);
            if (adxstm_tell(stm) == stm->fsct && stm->end_cb != 0)
                ((void (*)(int))stm->end_cb)(stm->end_obj);
            stm->rdflg = 0;
            stm->total_rd += nb;
            if (adxstm_tell(stm) >= (stm->fsize + 2047) / 2048)
                stm->stat = 3;
            stm->rtycnt = 0;
        } else if (st == 3) {
            sj->vtbl->ungetchunk(sj, 0, &stm->ck);
            if (adxstm_retry_limit >= 0) {
                if (stm->rtycnt >= adxstm_retry_limit)
                    stm->stat = 4;
                else
                    stm->rtycnt++;
            }
            stm->rdflg = 0;
        }
    }
    if (stm->rdflg != 0)
        return;
    if (sj == 0 || sj->vtbl == 0) {
        adxstm_count_retry();
        return;
    }
    if (sj->vtbl->getnum(sj, 1) < stm->fill_thr) {
        sj->vtbl->getchunk(sj, 0, stm->rq_size, &stm->ck);
        n = stm->ck.len / 2048;
        rest = stm->fsct;
        rest -= adxstm_tell(stm);
        if (n >= rest)
            n = rest;
        if (n >= stm->maxsct)
            n = stm->maxsct;
        if (n == 0) {
            sj->vtbl->ungetchunk(sj, 0, &stm->ck);
            return;
        }
        stm->nrd = cvFsReqRd(stm->fd, n, stm->ck.data);
        if (stm->nrd <= 0) {
            sj->vtbl->ungetchunk(sj, 0, &stm->ck);
            if (cvFsGetStat(stm->fd) == 3) {
                if (adxstm_retry_limit >= 0) {
                    if (stm->rtycnt >= adxstm_retry_limit)
                        stm->stat = 4;
                    else
                        stm->rtycnt++;
                }
            }
        } else {
            stm->rdflg = 1;
        }
    }
}

/* 0x001c8d90 */
static void adxstm_exec_handle(AdxstmObj *stm)
{
    if (stm->stat == 2)
        adxstm_exec_read(stm);
}

/* 0x001c8db0 */
void adxstm_exec_server(void)
{
    int i;
    AdxstmObj *stm;

    adx_lock();
    stm = adxstm_obj_tbl;
    for (i = 0; i < 40; i++, stm++) {
        if (stm->used == 1)
            adxstm_exec_handle(stm);
    }
    adx_unlock();
}

/* Sectors read since start; returns 1. */
/* 0x001c8e10 */
int adxstm_update_nread(AdxstmObj *stm, int *n)
{
    *n = adxstm_tell(stm) - stm->start_sct;
    return 1;
}

/* Returns 1. */
/* 0x001c8e50 */
static int adxstm_get_thresholds(AdxstmObj *stm, int *refill_level, int *read_size)
{
    *refill_level = stm->fill_thr;
    *read_size = stm->rq_size;
    return 1;
}

/* 0x001c8e68 */
static SjObj *adxstm_get_sj(AdxstmObj *stm)
{
    return (SjObj *)stm->sj;
}

/* Reads start when the joint holds less than refill_level; read_size bytes max; returns 1. */
/* 0x001c8e70 */
int adxstm_set_thresholds(AdxstmObj *stm, int refill_level, int read_size)
{
    stm->fill_thr = refill_level;
    stm->rq_size = read_size;
    return 1;
}

/* Sectors; returns 1. */
/* 0x001c8e80 */
int adxstm_set_max_sct(AdxstmObj *stm, int nsct)
{
    stm->maxsct = nsct;
    return 1;
}

/* 0x001c8e90 */
static void adxstm_nop_2(void)
{
}

/* 0x001c8e98 */
int adxstm_get_fsize_byte(AdxstmObj *stm)
{
    return stm->fsize;
}

/* cvFs state of the file. */
/* 0x001c8ea0 */
static void adxstm_get_fs_stat(AdxstmObj *stm, int *st)
{
    *st = cvFsGetStat(stm->fd);
}

/* 0x001c8ed0 */
static int adxstm_get_zero(AdxstmObj *stm, int *x)
{
    *x = 0;
    return 1;
}

/* Returns 1. */
/* 0x001c8ee0 */
static int adxstm_get_file_sct(const char *fname, int *nsct)
{
    *nsct = (cvFsGetFileSize(fname) + 2047) / 2048;
    return 1;
}

/* Returns 1. */
/* 0x001c8f28 */
static int adxstm_get_file_size(const char *fname, int *size)
{
    *size = cvFsGetFileSize(fname);
    return 1;
}

/* Refill level and read size both = its current free space. */
/* 0x001c8f58 */
void adxstm_set_sj(AdxstmObj *stm, SjObj *sj)
{
    int n;

    stm->sj = (int)sj;
    n = sj->vtbl->getnum(sj, 0);
    stm->rq_size = n;
    stm->fill_thr = n;
}

/* Bytes from a sector count. */
/* 0x001c8f98 */
void adxstm_set_size_from_sct(AdxstmObj *stm, int nsct)
{
    stm->fsize = nsct << 11;
}

/* Also seeks to it. */
/* 0x001c8fa8 */
void adxstm_set_ofst(AdxstmObj *stm, int ofst)
{
    stm->ofst = ofst;
    adxstm_seek(stm, 0);
}

/* Retries before a read fails; negative = no limit. */
/* 0x001c8fb8 */
void adxstm_set_retry_limit(int n)
{
    adxstm_retry_limit = n;
}

/* cvFs error callback -> ADX error. */
/* 0x001c8fc0 */
void adxstm_error_callback(void *obj, const char *msg)
{
    adx_error(msg);
}
