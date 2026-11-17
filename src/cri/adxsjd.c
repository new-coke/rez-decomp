/*
 * ADX stream joint decoder: eight handles; header and data from an input joint, 16-bit samples
 * per channel to output joints; counts samples so the caller can stop at a limit (loop end).
 */

#include "cri.h"
#include "libc.h"
#include "adxsjd.h"
#include "adx_lock.h"
#include "sjrbf.h"
#include "sjuni.h"

static int adxsjd_get_write_cb(AdxsjdObj *sjd, int *wpos, int *nsmpl, int *rest);

extern void adxb_lib_init(void);
extern void *adxb_create(int nch, void *buf, int bsize, int xsize);
extern void adxb_set_get_wr_cb(void *adxb, void (*fn)(void), void *obj);
extern void adxb_destroy(void *adxb);
extern void adxb_release_ahx(void *adxb, SjObj *sj);
extern void adxb_set_max_dec_smpl(void *adxb, int n);
extern void adxb_stop(void *adxb);
extern int adxb_decode_header(void *adxb, unsigned char *data, int len);
extern int adxb_get_format(void *adxb);
extern int adxb_get_nch(void *adxb);
extern int adxb_get_obuf(void *adxb);
extern void adxb_set_input(void *adxb, unsigned char *data, int len);
extern void adxb_start(void *adxb);
extern int adxb_get_total_smpl(void *adxb);
extern int adxb_get_dec_nbyte(void *adxb);
extern int adxb_get_dec_nsmpl(void *adxb);
extern void adxb_reset_after_end(void *adxb);
extern int adxb_get_stat(void *adxb);
extern void adxb_exec_handle(void *adxb);
extern int adxb_get_sfreq(void *adxb);
extern int adxb_get_out_bps(void *adxb);
extern int adxb_get_blk_smpl(void *adxb);
extern int adxb_get_blk_len(void *adxb);
extern int adxb_get_cutoff(void *adxb);
extern int adxb_get_loop_flag(void *adxb);
extern int adxb_get_hdr_word20(void *adxb);
extern int adxb_get_loop_start_smpl(void *adxb);
extern int adxb_get_loop_start_ofst(void *adxb);
extern int adxb_get_loop_end_smpl(void *adxb);
extern int adxb_get_loop_end_ofst(void *adxb);
extern int adxb_get_sample_bits(void *adxb);

/* 0x001c76b0 */
void adxsjd_lib_init(void)
{
    adxb_lib_init();
    memset(adxsjd_obj_tbl, 0, 1248);
}

/* 0x001c76e0 */
void adxsjd_lib_finish(void)
{
    memset(adxsjd_obj_tbl, 0, 1248);
}

/* No sample or step limit, counters cleared. */
/* 0x001c76f8 */
static void adxsjd_reset_state(AdxsjdObj *sjd)
{
    sjd->smpl_limit = -1;
    sjd->max_dec_smpl = 0x7fffffff;
    sjd->hdr_size = 0;
    sjd->decoded_smpl = 0;
    sjd->decoded_bytes = 0;
    sjd->smpl_pos = 0;
    sjd->limit_count = 0;
    sjd->byte_count = 0;
    sjd->eos_flag = 0;
}

/* Decoder on the first output joint's (ring buffer) buffer; 0 if none free or create fails. */
/* steer(cast) */
/* 0x001c7730 */
AdxsjdObj *adxsjd_create(SjObj *sji, int nch, SjObj **sjo)
{
    int i;
    AdxsjdObj *sjd;
    SjObj *sj;
    void *buf;
    int bsize;
    int xsize;

    sj = sjo[0];
    for (i = 0; i < 8; i++) {
        if (adxsjd_obj_tbl[i].used == 0)
            break;
    }
    if (i == 8)
        return 0;
    sjd = &adxsjd_obj_tbl[i];
    buf = sjrbf_get_buf(sj);
    bsize = ((unsigned int (*)(SjObj *))sjrbf_get_bsize)(sj) / 2;
    xsize = ((unsigned int (*)(SjObj *))sjrbf_get_xsize)(sj) / 2;
    sjd->adxb = adxb_create(nch, buf, bsize, bsize + xsize);
    adxb_set_get_wr_cb(sjd->adxb, adxsjd_get_write_cb, sjd);
    if (sjd->adxb == 0)
        return 0;
    sjd->sji = sji;
    sjd->nch = nch;
    for (i = 0; i < nch; i++)
        sjd->sjo[i] = sjo[i];
    sjd->stat = 0;
    adxsjd_reset_state(sjd);
    sjd->used = 1;
    sjd->limit_fn = 0;
    sjd->limit_obj = 0;
    sjd->filter_fn = 0;
    sjd->filter_obj = 0;
    return sjd;
}

/* 0x001c7898 */
void adxsjd_destroy(AdxsjdObj *sjd)
{
    if (sjd != 0) {
        if (sjd->adxb != 0)
            adxb_destroy(sjd->adxb);
        adx_lock();
        memset(sjd, 0, 156);
        adx_unlock();
    }
}

/* 0 stopped, 1 reading header, 2 decoding, 3 at end. */
/* 0x001c78f8 */
int adxsjd_get_stat(AdxsjdObj *sjd)
{
    return sjd->stat;
}

/* Also passed to the decoder's AHX side. */
/* 0x001c7900 */
void adxsjd_set_input(AdxsjdObj *sjd, SjObj *sj)
{
    sjd->sji = sj;
    adxb_release_ahx(sjd->adxb, sj);
}

/* 0x001c7918 */
static void adxsjd_set_output(AdxsjdObj *sjd, int ch, SjObj *sj)
{
    sjd->sjo[ch] = sj;
}

/* Per decode step. */
/* 0x001c7928 */
void adxsjd_set_max_dec_smpl(AdxsjdObj *sjd, int n)
{
    sjd->max_dec_smpl = n;
    adxb_set_max_dec_smpl(sjd->adxb, n);
}

/* From the stream header. */
/* 0x001c7940 */
void adxsjd_start(AdxsjdObj *sjd)
{
    adxsjd_reset_state(sjd);
    sjd->stat = 1;
}

/* 0x001c7970 */
void adxsjd_stop(AdxsjdObj *sjd)
{
    adxb_stop(sjd->adxb);
    sjd->stat = 0;
}

/*
 * Up to 4096 header bytes from the input; AU ends when input runs dry, SPSD keeps a 64-byte
 * copy. Header bytes are consumed (AHX leaves them for its decoder).
 */
/* 0x001c79a0 */
static void adxsjd_decode_header(AdxsjdObj *sjd)
{
    SjChunk ck;
    SjChunk tail;
    SjObj *sj;
    void *adxb;
    int n;

    sj = sjd->sji;
    adxb = sjd->adxb;
    sj->vtbl->getchunk(sj, 1, 4096, &ck);
    if (ck.len < 16 || (n = adxb_decode_header(adxb, ck.data, ck.len)) == 0 || ck.len < n) {
        sj->vtbl->ungetchunk(sj, 1, &ck);
        return;
    }
    sjd->hdr_size = n;
    if (adxb_get_format(adxb) == 4)
        sjd->eos_flag = 1;
    if (adxb_get_format(adxb) == 2)
        memcpy(sjd->hdr, ck.data, (ck.len > 64) ? 64 : ck.len);
    if (adxb_get_format(adxb) == 10) {
        sj->vtbl->ungetchunk(sj, 1, &ck);
    } else {
        sj_split_chunk(&ck, n, &ck, &tail);
        sj->vtbl->putchunk(sj, 0, &ck);
        sj->vtbl->ungetchunk(sj, 1, &tail);
    }
    sjd->stat = 2;
}

/* Decoder get-write callback: free chunk per output joint; position, size, samples before limit. */
/* 0x001c7b18 */
static int adxsjd_get_write_cb(AdxsjdObj *sjd, int *wpos, int *nsmpl, int *rest)
{
    int i;
    int n;
    SjObj *sj;

    sj = sjd->sjo[0];
    for (i = 0; i < adxb_get_nch(sjd->adxb); i++)
        sjd->sjo[i]->vtbl->getchunk(sjd->sjo[i], 0, 16384, &sjd->ck[i]);
    *wpos = (unsigned int)(sjd->ck[0].data - sjrbf_get_buf(sj)) / 2;
    n = (unsigned int)sjd->ck[0].len / 2;
    *nsmpl = (n < sjd->max_dec_smpl) ? n : sjd->max_dec_smpl;
    if (sjd->smpl_limit >= 0)
        *rest = sjd->smpl_limit - sjd->limit_count;
    else
        *rest = 0x1fffffff;
    return adxb_get_obuf(sjd->adxb);
}

/* Ends at an ADX end block, when all samples are out, or (AU) when input runs dry. */
/* 0x001c7c28 */
static void adxsjd_feed_decoder(AdxsjdObj *sjd)
{
    void *adxb;
    SjObj *sj;
    unsigned short sig;
    int n;

    adxb = sjd->adxb;
    sj = sjd->sji;
    if (sjd->eos_flag == 1) {
        if (sj->vtbl->getnum(sj, 1) == 0) {
            sjd->stat = 3;
            return;
        }
    }
    sj->vtbl->getchunk(sj, 1, 0x7fffffff, &sjd->cki);
    if ((adxb_get_format(adxb) == 0 && sjd->cki.len >= 2
         && (sig = *(unsigned short *)sjd->cki.data, (unsigned short)((sig << 8) | (sig >> 8)) == 0x8001))
        || adxsjd_get_total_smpl(sjd) <= sjd->smpl_pos) {
        sjd->stat = 3;
        sj->vtbl->ungetchunk(sj, 1, &sjd->cki);
        return;
    }
    n = adxsjd_get_blk_smpl(sjd);
    if ((int)((unsigned int)sjd->sjo[0]->vtbl->getnum(sjd->sjo[0], 0) / 2) < n) {
        sj->vtbl->ungetchunk(sj, 1, &sjd->cki);
        return;
    }
    if (adxb_get_format(adxb) == 10)
        sj->vtbl->ungetchunk(sj, 1, &sjd->cki);
    adxb_set_input(adxb, sjd->cki.data, sjd->cki.len);
    adxb_start(adxb);
}

/* Consumes input, passes samples on (through the filter callback), calls the limit callback. */
/* 0x001c7df0 */
static void adxsjd_put_decoded(AdxsjdObj *sjd)
{
    SjChunk head;
    SjChunk tail;
    void *adxb;
    SjObj *sj;
    int nsmpl;
    int nbyte;
    int n;
    int i;

    adxb = sjd->adxb;
    sj = sjd->sji;
    nsmpl = adxb_get_total_smpl(adxb);
    nbyte = adxb_get_dec_nbyte(adxb);
    n = adxb_get_dec_nsmpl(adxb);
    nsmpl -= sjd->smpl_pos;
    n = (n < nsmpl) ? n : nsmpl;
    sj_split_chunk(&sjd->cki, nbyte, &head, &tail);
    sj->vtbl->putchunk(sj, 0, &head);
    sj->vtbl->ungetchunk(sj, 1, &tail);
    for (i = 0; i < adxb_get_nch(sjd->adxb); i++) {
        sj_split_chunk(&sjd->ck[i], n * 2, &head, &tail);
        if (sjd->filter_fn != 0)
            ((void (*)(int, int, unsigned char *, int))sjd->filter_fn)(sjd->filter_obj, i, head.data, head.len);
        sjd->sjo[i]->vtbl->putchunk(sjd->sjo[i], 1, &head);
        sjd->sjo[i]->vtbl->ungetchunk(sjd->sjo[i], 0, &tail);
    }
    sjd->decoded_smpl += n;
    sjd->decoded_bytes += nbyte;
    sjd->smpl_pos += n;
    sjd->limit_count += n;
    sjd->byte_count += nbyte;
    if (sjd->smpl_limit >= 0 && sjd->limit_count >= sjd->smpl_limit && sjd->limit_fn != 0)
        ((void (*)(int))sjd->limit_fn)(sjd->limit_obj);
    adxb_reset_after_end(adxb);
}

/* AHX moves data itself: only adds decoded samples (up to the total) and bytes to the counters. */
/* 0x001c7fd8 */
static void adxsjd_count_skipped(AdxsjdObj *sjd)
{
    void *adxb;
    int nsmpl;
    int nbyte;
    int n;

    adxb = sjd->adxb;
    nsmpl = adxb_get_total_smpl(adxb);
    nbyte = adxb_get_dec_nbyte(adxb);
    n = adxb_get_dec_nsmpl(adxb);
    nsmpl -= sjd->smpl_pos;
    if (n < nsmpl)
        nsmpl = n;
    sjd->decoded_smpl += nsmpl;
    sjd->decoded_bytes += nbyte;
    sjd->smpl_pos += nsmpl;
}

/* AHX decodes only update the counters. */
/* 0x001c8068 */
static void adxsjd_exec_decode(AdxsjdObj *sjd)
{
    void *adxb;

    adxb = sjd->adxb;
    if (adxb_get_stat(adxb) == 0)
        adxsjd_feed_decoder(sjd);
    adxb_exec_handle(adxb);
    if (adxb_get_stat(adxb) == 3)
        adxsjd_put_decoded(sjd);
    if (((short *)adxb)[84] == 10)
        adxsjd_count_skipped(sjd);
}

/* 0x001c80f0 */
void adxsjd_exec_handle(AdxsjdObj *sjd)
{
    if (sjd->stat == 2)
        adxsjd_exec_decode(sjd);
    else if (sjd->stat == 1)
        adxsjd_decode_header(sjd);
}

/* 0x001c8120 */
void adxsjd_exec_server(void)
{
    int i;

    for (i = 0; i < 8; i++) {
        if (adxsjd_obj_tbl[i].used == 1)
            adxsjd_exec_handle(&adxsjd_obj_tbl[i]);
    }
}

/* 0x001c8180 */
static int adxsjd_get_decoded_bytes(AdxsjdObj *sjd)
{
    return sjd->decoded_bytes;
}

/* 0x001c8188 */
int adxsjd_get_decoded_smpl(AdxsjdObj *sjd)
{
    return sjd->decoded_smpl;
}

/* 0x001c8190 */
void adxsjd_set_smpl_pos(AdxsjdObj *sjd, int x)
{
    sjd->smpl_pos = x;
}

/* 0x001c8198 */
static int adxsjd_get_smpl_pos(AdxsjdObj *sjd)
{
    return sjd->smpl_pos;
}

/* Sees each channel's decoded samples. */
/* 0x001c81a0 */
void adxsjd_set_filter_cb(AdxsjdObj *sjd, int fn, int obj)
{
    sjd->filter_fn = fn;
    sjd->filter_obj = obj;
}

/* Called at the sample limit. */
/* 0x001c81b0 */
void adxsjd_set_limit_cb(AdxsjdObj *sjd, int fn, int obj)
{
    sjd->limit_fn = fn;
    sjd->limit_obj = obj;
}

/* Negative = none. */
/* 0x001c81c0 */
void adxsjd_set_smpl_limit(AdxsjdObj *sjd, int x)
{
    sjd->smpl_limit = x;
}

/* 0x001c81c8 */
static int adxsjd_get_smpl_limit(AdxsjdObj *sjd)
{
    return sjd->smpl_limit;
}

/* Count checked against the limit. */
/* 0x001c81d0 */
void adxsjd_set_limit_count(AdxsjdObj *sjd, int x)
{
    sjd->limit_count = x;
}

/* 0x001c81d8 */
static int adxsjd_get_limit_count(AdxsjdObj *sjd)
{
    return sjd->limit_count;
}

/* Input bytes since the caller's last reset. */
/* 0x001c81e0 */
void adxsjd_set_byte_count(AdxsjdObj *sjd, int x)
{
    sjd->byte_count = x;
}

/* 0x001c81e8 */
static int adxsjd_get_byte_count(AdxsjdObj *sjd)
{
    return sjd->byte_count;
}

/* 0x001c81f0 */
int adxsjd_get_format(AdxsjdObj *sjd)
{
    return adxb_get_format(sjd->adxb);
}

/* 0x001c8210 */
int adxsjd_get_sfreq(AdxsjdObj *sjd)
{
    return adxb_get_sfreq(sjd->adxb);
}

/* 0x001c8230 */
int adxsjd_get_nch(AdxsjdObj *sjd)
{
    return adxb_get_nch(sjd->adxb);
}

/* 0x001c8250 */
int adxsjd_get_out_bps(AdxsjdObj *sjd)
{
    return adxb_get_out_bps(sjd->adxb);
}

/* 0x001c8270 */
int adxsjd_get_blk_smpl(AdxsjdObj *sjd)
{
    return adxb_get_blk_smpl(sjd->adxb);
}

/* 0x001c8290 */
static int adxsjd_get_blk_len(AdxsjdObj *sjd)
{
    return adxb_get_blk_len(sjd->adxb);
}

/* 0x001c82b0 */
int adxsjd_get_total_smpl(AdxsjdObj *sjd)
{
    return adxb_get_total_smpl(sjd->adxb);
}

/* 0x001c82d0 */
static int adxsjd_get_cutoff(AdxsjdObj *sjd)
{
    return adxb_get_cutoff(sjd->adxb);
}

/* 0x001c82f0 */
int adxsjd_get_loop_flag(AdxsjdObj *sjd)
{
    return adxb_get_loop_flag(sjd->adxb);
}

/* 0x001c8310 */
static int adxsjd_get_hdr_word20(AdxsjdObj *sjd)
{
    return adxb_get_hdr_word20(sjd->adxb);
}

/* 0x001c8330 */
int adxsjd_get_loop_start_smpl(AdxsjdObj *sjd)
{
    return adxb_get_loop_start_smpl(sjd->adxb);
}

/* 0x001c8350 */
int adxsjd_get_loop_start_ofst(AdxsjdObj *sjd)
{
    return adxb_get_loop_start_ofst(sjd->adxb);
}

/* 0x001c8370 */
int adxsjd_get_loop_end_smpl(AdxsjdObj *sjd)
{
    return adxb_get_loop_end_smpl(sjd->adxb);
}

/* 0x001c8390 */
int adxsjd_get_loop_end_ofst(AdxsjdObj *sjd)
{
    return adxb_get_loop_end_ofst(sjd->adxb);
}

/* Bytes. */
/* 0x001c83b0 */
int adxsjd_get_hdr_size(AdxsjdObj *sjd)
{
    return sjd->hdr_size;
}

/* 0x001c83b8 */
int adxsjd_get_sample_bits(AdxsjdObj *sjd)
{
    return adxb_get_sample_bits(sjd->adxb);
}

/* SPSD streams only. */
/* 0x001c83d8 */
unsigned char *adxsjd_get_hdr_copy(AdxsjdObj *sjd)
{
    return sjd->hdr;
}
