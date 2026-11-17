/*
 * Ring buffer stream joint: 64 rings, free side 0, data side 1. An extra area past the end
 * mirrors the start, so a chunk can run past the end and stay contiguous.
 */

#include "types_cri.h"
#include "sjrbf.h"
#include "libc.h"
#include "cri_lock.h"

static void sjrbf_reset(SjRbf *sj);

/* Hangs. */
/* 0x001d22a8 */
static void sjrbf_default_error_cb(void *obj, int err)
{
    for (;;)
        ;
}

/* First call clears the 64 joints; counts calls. */
/* 0x001d22c8 */
void sjrbf_lib_init(void)
{
    if (sjrbf_init_count == 0)
        memset(sjrbf_obj_tbl, 0, 3072);
    sjrbf_init_count++;
}

/* Last user clears the joints. */
/* 0x001d2308 */
void sjrbf_lib_finish(void)
{
    if (--sjrbf_init_count == 0)
        memset(sjrbf_obj_tbl, 0, 3072);
}

/* Empty, bsize ring plus xsize extra area; 0 when all 64 are taken. */
/* 0x001d2338 */
SjRbf *sjrbf_create(char *buf, int bsize, int xsize)
{
    SjRbf *sj;
    int i;

    sj_lock();
    for (i = 0; i < 64; i++) {
        if (sjrbf_obj_tbl[i].used == 0)
            break;
    }
    if (i == 64) {
        sj = 0;
    } else {
        sj = &sjrbf_obj_tbl[i];
        sj->buf = buf;
        sj->bsize = bsize;
        sj->xsize = xsize;
        sj->errobj = sj;
        sj->used = 1;
        sj->vtbl = &sjrbf_vtbl;
        sj->uuid = sjrbf_uuid;
        sj->errfn = sjrbf_default_error_cb;
        sjrbf_reset(sj);
    }
    sj_unlock();
    return sj;
}

/* 0x001d2428 */
static void sjrbf_destroy(SjRbf *sj)
{
    sj_lock();
    if (sj != 0) {
        memset(sj, 0, 48);
        sj->used = 0;
    }
    sj_unlock();
}

/* 0x001d2468 */
static void *sjrbf_get_uuid(SjRbf *sj)
{
    return sj->uuid;
}

/* 0x001d2470 */
static void sjrbf_set_error_cb(SjRbf *sj, SjErrFn fn, void *obj)
{
    sj->errfn = fn;
    sj->errobj = obj;
}

/* No data, all free. */
/* 0x001d2480 */
static void sjrbf_reset(SjRbf *sj)
{
    sj->ndata = 0;
    sj->nfree = sj->bsize;
    sj->wpos = 0;
    sj->rpos = 0;
}

/* Side 0 free, side 1 data; other sides -3. */
/* 0x001d2498 */
static int sjrbf_get_num_data(SjRbf *sj, int id)
{
    if (id == 1)
        return sj->ndata;
    if (id == 0)
        return sj->nfree;
    if (sj->errfn != 0)
        sj->errfn(sj->errobj, -3);
    return 0;
}

/* Up to nbyte contiguous, into the extra area if need be. */
/* 0x001d24e8 */
static void sjrbf_get_chunk(SjRbf *sj, int id, int nbyte, SjChunk *ck)
{
    sj_lock();
    if (id == 0) {
        ck->len = CRI_MIN(CRI_MIN(sj->nfree, sj->bsize - sj->wpos + sj->xsize), nbyte);
        ck->data = sj->buf + sj->wpos;
        sj->wpos = (sj->wpos + ck->len) % sj->bsize;
        sj->nfree -= ck->len;
    } else if (id == 1) {
        ck->len = CRI_MIN(CRI_MIN(sj->ndata, sj->bsize - sj->rpos + sj->xsize), nbyte);
        ck->data = sj->buf + sj->rpos;
        sj->rpos = (sj->rpos + ck->len) % sj->bsize;
        sj->ndata -= ck->len;
    } else {
        ck->len = 0;
        ck->data = 0;
        if (sj->errfn != 0)
            sj->errfn(sj->errobj, -3);
    }
    sj_unlock();
}

/* Side 1 data is also copied between ring start and extra area; side 0 space becomes free. */
/* 0x001d2630 */
static void sjrbf_put_chunk(SjRbf *sj, int id, SjChunk *ck)
{
    int ofs;
    int n;

    if (ck->len > 0 && ck->data != 0) {
        sj_lock();
        if (id == 1) {
            sj->ndata += ck->len;
            ofs = ck->data - sj->buf;
            if (ofs < sj->xsize)
                memcpy(&sj->buf[sj->bsize + ofs], ck->data, CRI_MIN(ck->len, sj->xsize - ofs));
            ofs = ck->data - sj->buf + ck->len;
            if (ofs > sj->bsize) {
                n = CRI_MIN(ck->len, ofs - sj->bsize);
                memcpy(sj->buf, &sj->buf[ofs - n], n);
            }
        } else if (id == 0) {
            sj->nfree += ck->len;
        } else {
            ck->len = 0;
            ck->data = 0;
            if (sj->errfn != 0)
                sj->errfn(sj->errobj, -3);
        }
        sj_unlock();
    }
}

/* -3 if not the last chunk taken. */
/* 0x001d2768 */
static void sjrbf_unget_chunk(SjRbf *sj, int id, SjChunk *ck)
{
    int pos;

    if (ck->len > 0 && ck->data != 0) {
        sj_lock();
        if (id == 0) {
            pos = (sj->wpos + sj->bsize - ck->len) % sj->bsize;
            if (pos == (ck->data - sj->buf) % sj->bsize) {
                sj->wpos = pos;
                sj->nfree += ck->len;
            } else if (sj->errfn != 0) {
                sj->errfn(sj->errobj, -3);
            }
        } else if (id == 1) {
            pos = (sj->rpos + sj->bsize - ck->len) % sj->bsize;
            if (pos == (ck->data - sj->buf) % sj->bsize) {
                sj->rpos = pos;
                sj->ndata += ck->len;
            } else if (sj->errfn != 0) {
                sj->errfn(sj->errobj, -3);
            }
        } else {
            ck->len = 0;
            ck->data = 0;
            if (sj->errfn != 0)
                sj->errfn(sj->errobj, -3);
        }
        sj_unlock();
    }
}

/* *rbyte = how much of nbyte a get would give; 1 if all of it. */
/* 0x001d28c8 */
static int sjrbf_is_get_chunk(SjRbf *sj, int id, int nbyte, int *rbyte)
{
    int n;

    sj_lock();
    if (id == 0) {
        n = sj->bsize - sj->wpos + sj->xsize;
        n = (sj->nfree < n) ? sj->nfree : n;
        n = (n < nbyte) ? n : nbyte;
    } else if (id == 1) {
        n = sj->bsize - sj->rpos + sj->xsize;
        n = (sj->ndata < n) ? sj->ndata : n;
        n = (n < nbyte) ? n : nbyte;
    } else {
        n = 0;
        if (sj->errfn != 0)
            sj->errfn(sj->errobj, -3);
    }
    *rbyte = n;
    sj_unlock();
    return n == nbyte;
}

/* 0x001d2988 */
char *sjrbf_get_buf(SjRbf *sj)
{
    return sj->buf;
}

/* 0x001d2990 */
int sjrbf_get_bsize(SjRbf *sj)
{
    return sj->bsize;
}

/* 0x001d2998 */
int sjrbf_get_xsize(SjRbf *sj)
{
    return sj->xsize;
}
