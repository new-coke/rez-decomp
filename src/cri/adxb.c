/*
 * ADX basic decoder: eight handles; recognises ADX, AHX, SPSD, WAVE, AIFF, AU headers and decodes
 * into a 16-bit output ring. ADX blocks go to adxd; AHX to an optional decoder attached by hooks.
 */

#include "types_cri.h"
#include "adxb.h"
#include "libc.h"
#include "adx_decode.h"
#include "adx_error.h"
#include "adxb_aiff.h"
#include "adxb_au.h"
#include "adxb_spsd.h"
#include "adxb_wave.h"
#include "adxd.h"

/* Through the AHX hook, if attached. */
/* 0x001d4938 */
void adxb_release_ahx(AdxbObj *adxb)
{
    if (adxb->ahx != 0)
        adxb_ahx_release_fn(adxb->ahx);
}

/* Per decode step; also sets the limit in 96-sample AHX blocks. */
/* 0x001d4960 */
void adxb_set_max_dec_smpl(AdxbObj *adxb, int n)
{
    adxb->max_dec_smpl = n;
    adxb->max_dec_blk = n / 96;
}

/* 0x001d4980 */
static void adxb_exec_ahx(AdxbObj *adxb)
{
    adxb_ahx_exec_fn(adxb);
}

/* 0x001d49a0 */
void adxb_lib_init(void)
{
    adxd_lib_init();
    memset(adxb_obj_tbl, 0, 1472);
}

/* Default get-write callback. */
/* 0x001d49d0 */
static char *adxb_default_get_wr(void *obj, int *wpos, int *nfree, int *nleft)
{
    AdxbObj *adxb = obj;

    *wpos = adxb->wpos;
    *nfree = adxb->obsize - adxb->wpos;
    *nleft = adxb->total_smpl - adxb->wr_nsmpl;
    return adxb->obuf;
}

/* Default add-write callback. */
/* 0x001d4a00 */
static void adxb_default_add_wr(void *obj, int nbyte, int n)
{
    AdxbObj *adxb = obj;

    adxb->wpos += n;
    adxb->wr_nsmpl += n;
}

/* 0 when no handle is free or adxd_create fails. */
/* 0x001d4a20 */
AdxbObj *adxb_create(void *obj, char *obuf, int obsize, int obsize2)
{
    AdxbObj *adxb;
    int i;

    for (i = 0; i < 8; i++) {
        if (adxb_obj_tbl[i].used == 0)
            break;
    }
    if (i == 8)
        return 0;
    adxb = &adxb_obj_tbl[i];
    memset(adxb, 0, 184);
    adxb->used = 1;
    adxb->adxd = adxd_create();
    if (adxb->adxd == 0) {
        adxb_destroy(adxb);
        return 0;
    }
    adxb->obj = obj;
    adxb->obuf = obuf;
    adxb->obsize = obsize;
    adxb->obsize2 = obsize2;
    adxb->getwr = adxb_default_get_wr;
    adxb->addwr = adxb_default_add_wr;
    adxb->getwrobj = adxb;
    adxb->addwrobj = adxb;
    return adxb;
}

/* 0x001d4b28 */
void adxb_destroy(AdxbObj *adxb)
{
    if (adxb != 0) {
        adxd_destroy(adxb->adxd);
        memset(adxb, 0, 184);
        adxb->used = 0;
    }
}

/* Returns the data offset, or 0. AHX (encoding type 5 and up) needs an attached AHX decoder. */
/* 0x001d4b68 */
static int ADXB_DecodeHeaderAdx(AdxbObj *adxb, void *hdr, int len)
{
    short ofs;

    adxb->hdrdec = 1;
    if (adxb_parse_adx_header(hdr, len, &ofs, &adxb->type, &adxb->sample_bits, &adxb->blk, &adxb->nch, &adxb->total, &adxb->total_smpl,
                     &adxb->spb) < 0)
        return 0;
    if (adxb->type >= 5) {
        if (adxb->ahx == 0) {
            adx_error_cat("E1060101 ADXB_DecodeHeaderAdx: ", "can't play AHX data by this handle");
            return 0;
        }
        adxb->sample_bits = 8;
        adxb->spb = 96;
        adxb->format = 10;
        adxb->blk = adxb->nch * 192;
    } else {
        if (adxb_parse_cutoff(hdr, len, &adxb->cutoff) < 0)
            return 0;
        adxd_set_coef(adxb->adxd, adxb->total, adxb->cutoff);
        adxb_parse_loop_info(hdr, len, &adxb->loop_ins_smpl, &adxb->loop_flag, &adxb->loop_type, &adxb->loop_start_smpl, &adxb->loop_start_ofst, &adxb->loop_end_smpl, &adxb->loop_end_ofst);
        adxb->format = 0;
    }
    adxb->wpos = 0;
    adxb->dec.nch = adxb->nch;
    adxb->dec.blk = adxb->blk;
    adxb->dec.spb = adxb->spb;
    adxb->dec.obuf = adxb->obuf;
    adxb->dec.obsize = adxb->obsize;
    adxb->dec.obsize2 = adxb->obsize2;
    adxb->dec.unk_2c = adxb->unk_48;
    adxb->dec.unk_2e = adxb->unk_4a;
    adxb->dec.state = &adxb->dec_state;
    memset(&adxb->dec_state, 0, 4);
    return ofs;
}

/* Returns the data offset, or 0 for an unknown format or bad header. */
/* 0x001d4d00 */
int adxb_decode_header(AdxbObj *adxb, unsigned short *hdr, int len)
{
    if ((unsigned short)(*hdr >> 8 | *hdr << 8) == 0x8000)
        return ADXB_DecodeHeaderAdx(adxb, hdr, len);
    if (adxb_is_spsd(hdr))
        return adxb_decode_header_spsd(adxb, hdr, len);
    if (adxb_is_wave(hdr))
        return adxb_decode_header_wave(adxb, hdr, len);
    if (adxb_is_aiff(hdr))
        return adxb_decode_header_aiff(adxb, hdr, len);
    if (adxb_is_au(hdr))
        return adxb_decode_header_au(adxb, hdr, len);
    return 0;
}

/* 0x001d4df8 */
void adxb_set_get_wr_cb(AdxbObj *adxb, char *(*fn)(void *, int *, int *, int *), void *obj)
{
    adxb->getwr = fn;
    adxb->getwrobj = obj;
}

/* 0x001d4e08 */
static void adxb_set_add_wr_cb(AdxbObj *adxb, void (*fn)(void *, int, int), void *obj)
{
    adxb->addwr = fn;
    adxb->addwrobj = obj;
}

/* 0x001d4e18 */
char *adxb_get_obuf(AdxbObj *adxb)
{
    return adxb->obuf;
}

/* 0 ADX, 1 WAVE, 2 SPSD, 3 AIFF, 4 AU, 10 AHX. */
/* 0x001d4e20 */
int adxb_get_format(AdxbObj *adxb)
{
    return adxb->format;
}

/* Kept in adxb->total. */
/* 0x001d4e28 */
int adxb_get_sfreq(AdxbObj *adxb)
{
    return adxb->total;
}

/* 0x001d4e30 */
int adxb_get_nch(AdxbObj *adxb)
{
    return adxb->nch;
}

/* Bits per encoded sample. */
/* 0x001d4e38 */
int adxb_get_sample_bits(AdxbObj *adxb)
{
    return adxb->sample_bits;
}

/* Bits per output sample. */
/* 0x001d4e40 */
int adxb_get_out_bps(AdxbObj *adxb)
{
    if (adxb->format == 0)
        return 16;
    if (adxb->format == 2) {
        if (adxb->pcm_fmt == 2)
            return 4;
        if (adxb->pcm_fmt == 1)
            return 8;
        return 16;
    }
    if (adxb->format == 1) {
        if (adxb->pcm_fmt == 2)
            return 4;
        return 16;
    }
    return 16;
}

/* 0x001d4ea0 */
int adxb_get_blk_smpl(AdxbObj *adxb)
{
    return adxb->spb;
}

/* Bytes. */
/* 0x001d4ea8 */
int adxb_get_blk_len(AdxbObj *adxb)
{
    return adxb->blk;
}

/* 0x001d4eb0 */
int adxb_get_total_smpl(AdxbObj *adxb)
{
    return adxb->total_smpl;
}

/* 0x001d4eb8 */
int adxb_get_cutoff(AdxbObj *adxb)
{
    return adxb->cutoff;
}

/* Loop alignment sample count. */
/* 0x001d4ec0 */
int adxb_get_hdr_word20(AdxbObj *adxb)
{
    return adxb->loop_ins_smpl;
}

/* Positive when it loops. */
/* 0x001d4ec8 */
int adxb_get_loop_flag(AdxbObj *adxb)
{
    return adxb->loop_flag;
}

/* 0x001d4ed0 */
int adxb_get_loop_start_smpl(AdxbObj *adxb)
{
    return adxb->loop_start_smpl;
}

/* Byte position in the file. */
/* 0x001d4ed8 */
int adxb_get_loop_start_ofst(AdxbObj *adxb)
{
    return adxb->loop_start_ofst;
}

/* 0x001d4ee0 */
int adxb_get_loop_end_smpl(AdxbObj *adxb)
{
    return adxb->loop_end_smpl;
}

/* Byte position in the file. */
/* 0x001d4ee8 */
int adxb_get_loop_end_ofst(AdxbObj *adxb)
{
    return adxb->loop_end_ofst;
}

/* 0x001d4ef0 */
int adxb_get_stat(AdxbObj *adxb)
{
    return adxb->stat;
}

/* len in bytes, kept as blocks (ADX) or sample frames (PCM). */
/* 0x001d4ef8 */
void adxb_set_input(AdxbObj *adxb, void *buf, int len)
{
    if (adxb->format == 0) {
        adxb->dec.buf = buf;
        adxb->dec.len = len / adxb->blk;
        adxb->dec.unk_34 = 0;
        adxb->dec_nsmpl = 0;
        adxb->dec_nbyte = 0;
    } else {
        adxb->dec.buf = buf;
        adxb->dec.len = len / (adxb->sample_bits / 8 * adxb->nch);
        adxb->dec.unk_34 = 0;
        adxb->dec_nsmpl = 0;
        adxb->dec_nbyte = 0;
    }
}

/* 0x001d4f80 */
void adxb_start(AdxbObj *adxb)
{
    if (adxb->stat == 0)
        adxb->stat = 1;
}

/* 0x001d4f98 */
void adxb_stop(AdxbObj *adxb)
{
    adxd_stop(adxb->adxd);
    adxb->stat = 0;
}

/* After status 3, back to idle. */
/* 0x001d4fc8 */
void adxb_reset_after_end(AdxbObj *adxb)
{
    if (adxb->stat == 3) {
        adxd_reset_after_end(adxb->adxd);
        adxb->wpos = 0;
        adxb->stat = 0;
    }
}

/* 0x001d5008 */
int adxb_get_dec_nbyte(AdxbObj *adxb)
{
    return adxb->dec_nbyte;
}

/* 0x001d5010 */
int adxb_get_dec_nsmpl(AdxbObj *adxb)
{
    return adxb->dec_nsmpl;
}

/* 0x001d5018 */
static void *adxb_get_adxd(AdxbObj *adxb)
{
    return adxb->adxd;
}

/* 0x001d5020 */
static void adxb_decode_mono(AdxbObj *adxb, int n)
{
    AdxbDec *dec;
    void *adxd;
    short *out;

    dec = &adxb->dec;
    adxd = adxb->adxd;
    out = (short *)dec->obuf;
    adxd_setup_mono(adxd, dec->buf, n, out + dec->wpos);
    adxd_start(adxd);
}

/* 0x001d5068 */
static void adxb_decode_stereo(AdxbObj *adxb, int n)
{
    AdxbDec *dec;
    void *adxd;

    dec = &adxb->dec;
    adxd = adxb->adxd;
    adxd_setup_stereo(adxd, adxb->dec.buf, n * 2, (short *)dec->obuf + dec->wpos,
                 (short *)dec->obuf + dec->wpos + dec->obsize2);
    adxd_start(adxd);
}

/* As many blocks as input, samples left and output space allow. */
/* 0x001d50c0 */
static void adxb_start_adx_decode(AdxbObj *adxb)
{
    AdxbDec *dec;
    int n, spb, x, y, nblk_left, pad, nblk_ring, has_room, nblk_free;
    int nch, nleft, obsize, wpos, nfree, ring_smpl;

    dec = &adxb->dec;
    nch = dec->nch;
    n = dec->len;
    n /= nch;
    spb = dec->spb;
    nleft = dec->nleft;
    obsize = dec->obsize;
    wpos = dec->wpos;
    x = nleft + spb - 1;
    nblk_left = x / spb;
    pad = x % spb + 1;
    pad = spb - pad;
    y = obsize - wpos + spb - 1;
    nfree = dec->nfree;
    nblk_ring = y / spb;
    ring_smpl = nblk_ring * spb;
    if (nblk_left < y / spb)
        nblk_ring += (wpos + ring_smpl - pad < obsize);
    has_room = nleft < nfree;
    nfree += has_room * pad;
    nblk_free = nfree / spb;
    if (nblk_free < n)
        n = nblk_free;
    if (nblk_left < n)
        n = nblk_left;
    if (nblk_ring < n)
        n = nblk_ring;
    if (nch == 2)
        adxb_decode_stereo(adxb, n);
    else
        adxb_decode_mono(adxb, n);
}

/* 0x001d5198 */
static void adxb_copy_words(short *dst, short *src, int n)
{
    while (n > 0) {
        *dst++ = *src++;
        n--;
    }
}

/* Moves the n samples past each channel ring end (at ofs) to its start; right is len after left. */
/* 0x001d51c8 */
static void adxb_move_stereo(short *buf, int ofs, int len, int n)
{
    adxb_copy_words(buf, &buf[ofs], n);
    adxb_copy_words(&buf[len], &buf[len + ofs], n);
}

/* Moves the n samples past the ring end (at ofs) to its start. */
/* 0x001d5238 */
static void adxb_move_mono(short *buf, int ofs, int len, int n)
{
    adxb_copy_words(buf, &buf[ofs], n);
}

/* Records bytes and samples (less final partial block padding); wraps the ring overhang. */
/* 0x001d5248 */
static void adxb_end_adx_decode(AdxbObj *adxb)
{
    AdxbDec *dec;
    int obsize;
    int obsize2;
    short *obuf;
    int spb;
    int x;
    int nblk;
    int pad;
    int blk;
    int wpos;
    int n;
    int rem;

    dec = &adxb->dec;
    obsize = adxb->obsize;
    obuf = (short *)dec->obuf;
    blk = dec->blk;
    spb = dec->spb;
    x = dec->nleft + spb - 1;
    obsize2 = adxb->obsize2;
    nblk = x / spb;
    rem = x % spb + 1;
    wpos = dec->wpos;
    pad = spb - rem;
    n = adxd_get_num_blk(adxb->adxd);
    spb = n * spb / dec->nch;
    adxb->dec_nbyte = n * blk;
    pad = (n < nblk) ? spb : spb - pad;
    wpos += pad;
    adxb->dec_nsmpl = pad;
    if (wpos >= obsize) {
        wpos -= obsize;
        if (dec->nch == 2)
            adxb_move_stereo(obuf, obsize, obsize2, wpos);
        else
            adxb_move_mono(obuf, obsize, obsize2, wpos);
    }
}

/* Started: get-write window, decode (status 2); finished (status 3): add-write callback. */
/* 0x001d53c8 */
static void adxb_exec_adx(AdxbObj *adxb)
{
    int st;

    if (adxb->stat == 1) {
        if (adxd_get_stat(adxb->adxd) == 0) {
            adxb->getwr(adxb->getwrobj, &adxb->dec.wpos, &adxb->dec.nfree, &adxb->dec.nleft);
            adxb_start_adx_decode(adxb);
            adxb->stat = 2;
        }
    }
    if (adxb->stat == 2) {
        adxd_exec_handle(adxb->adxd);
        st = adxd_get_stat(adxb->adxd);
        if (st == 3) {
            adxb_end_adx_decode(adxb);
            adxd_reset_after_end(adxb->adxd);
            adxb->addwr(adxb->addwrobj, adxb->dec_nbyte, adxb->dec_nsmpl);
            adxb->stat = st;
        }
    }
}

/* 0x001d5498 */
void adxb_exec_handle(AdxbObj *adxb)
{
    if (adxb->format == 0)
        adxb_exec_adx(adxb);
    else if (adxb->format == 10)
        adxb_exec_ahx(adxb);
    else if (adxb->format == 2)
        adxb_exec_spsd(adxb);
    else if (adxb->format == 3)
        adxb_exec_aiff(adxb);
    else if (adxb->format == 4)
        adxb_exec_au(adxb);
    else if (adxb->format == 1)
        adxb_exec_wave(adxb);
}

/* 0x001d5508 */
static void adxb_exec_server(void)
{
    int i;

    for (i = 0; i < 8; i++) {
        if (adxb_obj_tbl[i].used == 1)
            adxb_exec_handle(&adxb_obj_tbl[i]);
    }
}
