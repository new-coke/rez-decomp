/*
 * ADX playback: eight handles (game music and voice). Input joint from a file stream or play
 * list, joint decoder, output joints, sound output; server step: header, buffering, play, end,
 * loops, stall and starvation watch. Play time in samples or vsyncs.
 */

#include "cri.h"
#include "adxrna.h"
#include "types_cri.h"
#include "libc.h"
#include "adxt.h"
#include "adx_error.h"
#include "adx_lock.h"
#include "adx_peak.h"
#include "adxf.h"
#include "adxstm.h"
#include "cvfs.h"
#include "dvci.h"
#include "lsc.h"
#include "ps2rna.h"
#include "rofs_mai.h"
#include "sjrbf.h"
#include "sjrmt.h"
#include "sjuni.h"
#include "sjx.h"
#include "dtx.h"
#include "rofs_if.h"
#include "rofs_pfs.h"

/* Playback handle as the play-state code sees it (same 164 bytes as AdxtObj). */
typedef struct {
    signed char used;           /* 00 */
    signed char stat;
    signed char pmode;          /* 02 */
    signed char maxnch;
    void *sjd;                /* 04: SJ decoder */
    void *stm;                /* 08 */
    void *rna;                /* 0c */
    char pad_10[4];
    SjObj *sji;               /* 14 */
    SjObj *sjo[2];            /* 18: per channel */
    char pad_20[24];
    int svrfreq;                  /* 38 */
    char pad_3c[4];
    short outvol;                /* 40 */
    short outpan0;                /* 42: channel 0, -128 = default */
    short outpan1;                /* 44: channel 1 */
    char pad_46[2];
    int maxdecsmpl;
    int lpcnt;                  /* 4c */
    int lp_skiplen;                  /* 50 */
    char pad_54[12];
    short ercode;                /* 60: -1 no data, -2 decoder stalled */
    char pad_62[2];
    int edecpos;                  /* 64: last decoded count */
    short edeccnt;                /* 68: stall counter */
    short eshrtcnt;                /* 6a: data starvation counter */
    signed char lpflg;          /* 6c */
    signed char autorcvr;          /* 6d: error recovery mode */
    char pad_6e[2];
    signed char pstwait_flag;          /* 70 */
    signed char pstready_flag;
    signed char pause_flag;
    char pad_73[1];
    int amp;
    char pad_78[16];
    int time_ofst;                  /* 88 */
    int lesct;
    int trpnsmpl;
    char pad_94[4];
    signed char lnkflg;          /* 98 */
    char pad_99[3];
    int tvofst;                  /* 9c */
    int svcnt;                  /* a0 */
} AdxtObjView;

extern AdxtObj adxt_obj_tbl[];
extern void *adxsjd_create(SjObj *sji, int nch, SjObj **sjo);
extern void adxsjd_destroy(void *sjd);
extern void adxsjd_set_input(void *sjd, SjObj *sj);
extern void adxsjd_start(void *sjd);
extern void adxsjd_stop(void *sjd);
extern int adxt_vsync_rate;
extern int adxt_vsync_mode;
extern int adxsjd_get_total_smpl(void *sjd);
extern int adxsjd_get_sfreq(void *sjd);
extern int adxsjd_get_out_bps(void *sjd);
extern int adxsjd_get_decoded_smpl(void *sjd);
extern float adxt_vsync_time;
extern int adxsjd_get_nch(void *sjd);
extern int adxsjd_get_hdr_size(void *sjd);
extern int adxsjd_get_sample_bits(void *sjd);
extern int adxt_svr_stat0;
extern int adxt_svr_stat1;
extern int adxt_svr_stat2;
extern int adxt_svr_stat3;
extern int adxt_svr_stat4;
extern int adxt_svr_stat5;
extern void adxsjd_exec_server(void);
extern void adx_set_error_cb(void);

extern void adxsjd_set_filter_cb(void *h, int a, int b);

extern int adxb_parse_adx_header(unsigned char *hdr, int len, short *hdrlen, signed char *a, signed char *b,
                        signed char *c, signed char *d, int *e, int *f, int *g);

extern void adxb_set_mono(void);
extern void (*adxt_destroy_cb)(void);
extern int adxsjd_get_loop_start_smpl(void *sjd);
extern int adxsjd_get_loop_start_ofst(void *sjd);
extern int adxsjd_get_loop_end_smpl(void *sjd);
extern void adxsjd_set_smpl_limit(void *sjd, int n);
extern void adxsjd_set_limit_count(void *sjd, int n);
extern void adxsjd_set_byte_count(void *sjd, int n);
extern void adxsjd_set_smpl_pos(void *sjd, int n);
extern int adxb_parse_end_block(char *data, int len, short *hdrlen);
extern int adxb_find_word80(char *data, int len, short *ofs);
extern void adxsjd_exec_handle(void *sjd);
extern int adxsjd_get_stat(void *sjd);
extern void adxsjd_set_max_dec_smpl(void *sjd, int v);
extern int adxsjd_get_loop_flag(void *sjd);
extern int adxsjd_get_blk_smpl(void *sjd);
extern int adxsjd_get_loop_end_ofst(void *sjd);
extern void adxsjd_set_limit_cb(void *sjd, void (*fn)(AdxtObjView *), AdxtObjView *obj);
extern void adxt_apply_pan(AdxtObjView *adxt);
extern int adxsjd_get_format(void *sjd);
extern int adxsjd_get_hdr_copy(void *sjd);
extern void adxt_stm_end_cb(AdxtObjView *adxt);
extern void adxt_trap_entry(AdxtObjView *adxt);
extern void adxt_seek_next_block(AdxtObjView *adxt);
extern int adxt_dec_end_nch;
extern int adxt_dec_end_nbyte;
extern int adxt_play_end_free;
extern void adxt_restart(AdxtObjView *adxt);
extern void adxt_check_stm_error(AdxtObjView *adxt);
extern void adxt_stat_decinfo(AdxtObjView *adxt);
extern void adxt_check_start(AdxtObjView *adxt);
extern void adxt_check_dec_end(AdxtObjView *adxt);
extern void adxt_check_play_end(AdxtObjView *adxt);
extern void adxt_exec_play_end(AdxtObjView *adxt);
extern void adxt_exec_handle(AdxtObjView *adxt);

/* Channel 0 data/free in the EE joint and IOP buffer, their sums, the handle state. */
/* 0x001c9158 */
void adxt_debug_print(AdxtObj *adxt)
{
    void *rna;
    void *iop;
    SjObj *sj;
    int ntmp0;
    int ntmp2;
    int niop0;
    int niop2;

    rna = (void *)adxt->rna;
    iop = ps2rna_get_sjrmt(rna, 0);
    sj = ps2rna_get_sjuni(rna, 0);
    ntmp0 = sj->vtbl->getnum(sj, 1);
    ntmp2 = sj->vtbl->getnum(sj, 0);
    niop0 = sjrmt_get_num_data(iop, 1);
    niop2 = sjrmt_get_num_data(iop, 0);
    printf("ntmp0=%4d, ntmp2=%4d, sum=%4d\n", ntmp0, ntmp2, ntmp0 + ntmp2);
    printf("niop0=%4d, niop2=%4d, sum=%4d\n", niop0, niop2, niop0 + niop2);
    printf("stat=%4d, ttl=%4d\n", adxt->stat, ntmp0 + ntmp2 + niop0 + niop2);
}

/*
 * On the game's work area: 16576 bytes of output buffer per channel plus input buffer. Defaults:
 * 60 server calls/s, loop on, stop on error. 0 if no handle is free or anything fails.
 */
/* 0x001c9260 */
AdxtObj *adxt_create(int maxnch, void *work, int worksize)
{
    int i;
    int k;
    AdxtObj *adxt;
    unsigned char *buf;
    int size;

    buf = (unsigned char *)(((unsigned int)work + 63) & 0xffffffc0);
    size = worksize - (buf - (unsigned char *)work);
    for (k = 0; k < 8; k++) {
        if (adxt_obj_tbl[k].used == 0)
            break;
    }
    if (k == 8)
        return 0;
    adxt = &adxt_obj_tbl[k];
    memset(adxt, 0, 164);
    adxt->obuf_size = 8192;
    adxt->unk_28 = 36;
    adxt->obuf_dist = 8288;
    adxt->work = buf;
    adxt->nch = maxnch;
    adxt->wbuf = buf + maxnch * 16576;
    adxt->wsize = ((unsigned int)(size - maxnch * 16576 - 36) / 2048) * 2048;
    adxt->sji = 0;
    adxt->sjf = sjrbf_create(adxt->wbuf, adxt->wsize, 36);
    if (adxt->sjf == 0) {
        adxt_destroy(adxt);
        return 0;
    }
    for (i = 0; i < maxnch; i++) {
        adxt->sjo[i] = sjrbf_create(adxt->work + adxt->obuf_dist * i * 2,
                                    adxt->obuf_size * 2, (adxt->obuf_dist - adxt->obuf_size) * 2);
        if (adxt->sjo[i] == 0) {
            adxt_destroy(adxt);
            return 0;
        }
    }
    adxt->sjd = adxsjd_create(adxt->sjf, maxnch, adxt->sjo);
    if (adxt->sjd == 0) {
        adxt_destroy(adxt);
        return 0;
    }
    adxt->rna = (int)adxrna_create(adxt->sjo, maxnch);
    if (adxt->rna == 0) {
        adxt_destroy(adxt);
        return 0;
    }
    adxt->stm = LSC_Create(adxt->sjf);
    if (adxt->stm == 0) {
        adxt_destroy(adxt);
        return 0;
    }
    adxt->svr_freq = 60;
    adxt->maxsct = adxt->wsize / 2048;
    adxt->minsct = adxt->maxsct * 0.85;
    adxt->outvol = 0;
    for (i = 0; i < maxnch; i++)
        adxt->outpan[i] = -128;
    adxt->unk_54 = 0;
    adxt->lpflg = 1;
    adxt->unk_58 = 0;
    adxt->unk_5c = 0;
    adxt->ercode = 0;
    adxt->unk_64 = 0;
    adxt->unk_68 = 0;
    adxt->unk_6a = 0;
    adxt->rcvr_mode = 1;
    adxt->pause = 0;
    adxt->time_ofst = 0;
    adxt->unk_98 = 1;
    adxt->used = 1;
    return adxt;
}

/* Calls the destroy callback, stops, destroys output, decoder, streams, joints, peak meter. */
/* steer(cast) */
/* 0x001c94e8 */
void adxt_destroy(AdxtObj *adxt)
{
    int i;

    if (adxt != 0) {
        ((void (*)(AdxtObj *))adxt_call_destroy_cb)(adxt);
        if (adxt->used == 1)
            adxt_stop(adxt);
        if (adxt->rna != 0)
            adxrna_destroy((void *)adxt->rna);
        if (adxt->sjd != 0)
            adxsjd_destroy(adxt->sjd);
        if (adxt->fstm != 0)
            adxstm_close((void *)adxt->fstm);
        adx_lock();
        if (adxt->stm != 0)
            lsc_destroy(adxt->stm);
        if (adxt->sjf != 0)
            adxt->sjf->vtbl->destroy(adxt->sjf);
        for (i = 0; i < adxt->nch; i++) {
            if (adxt->sjo[i] != 0)
                adxt->sjo[i]->vtbl->destroy(adxt->sjo[i]);
            if (adxt->sjx[i] != 0)
                adxt->sjx[i]->vtbl->destroy(adxt->sjx[i]);
            if (adxt->sjy[i] != 0)
                adxt->sjy[i]->vtbl->destroy(adxt->sjy[i]);
        }
        if (adxt->pkm != 0)
            adx_peak_destroy(adxt->pkm);
        memset(adxt, 0, 164);
        adxt->used = 0;
        adx_unlock();
    }
}

/* 0x001c96a8 */
void adxt_destroy_all(void)
{
    int i;

    for (i = 0; i < 8; i++) {
        if (adxt_obj_tbl[i].used == 1)
            adxt_destroy(&adxt_obj_tbl[i]);
    }
}

/* 0x001c9708 */
void adxt_destroy_all_2(void)
{
    adxt_destroy_all();
}

/* Empties outputs, resets play time and loop count; waits for the header (state 1). */
/* 0x001c9710 */
void adxt_start_decode(AdxtObj *adxt, SjObj *sj)
{
    int i;

    for (i = 0; i < adxt->nch; i++)
        adxt->sjo[i]->vtbl->reset(adxt->sjo[i]);
    adxsjd_set_input(adxt->sjd, sj);
    adxt->sji = sj;
    adxsjd_start(adxt->sjd);
    adxt->ready = 0;
    adxt->stat = 1;
    adxt->unk_8c = 0x7fffffff;
    adxt->vsync_base = adxt_exec_count;
    adxt->unk_90 = -1;
    adxt->vsync_time = 0;
    adxt->lpcnt = 0;
}

/* Refill level and read size from the read-ahead settings; reads the first 25 sectors. */
/* 0x001c97d8 */
void adxt_start_stm(AdxtObj *adxt)
{
    adxstm_set_thresholds((void *)adxt->fstm, adxt->minsct << 11, adxt->maxsct << 11);
    adxstm_set_size_sct((void *)adxt->fstm, 25);
    adxstm_start((void *)adxt->fstm);
    adxt_start_decode(adxt, adxt->sjf);
    adxt->pmode = 0;
}

/* Caller fills sj (play mode 3). */
/* 0x001c9838 */
void adxt_start_sj(AdxtObj *adxt, SjObj *sj)
{
    int i;

    adxt_stop(adxt);
    adx_lock();
    for (i = 0; i < adxt->nch; i++)
        adxt->sjo[i]->vtbl->reset(adxt->sjo[i]);
    adxsjd_set_input(adxt->sjd, sj);
    adxt->sji = sj;
    adxsjd_start(adxt->sjd);
    adxt->pmode = 3;
    adxt->stat = 1;
    adxt->unk_98 = 1;
    adxt->lpcnt = 0;
    adxt->vsync_time = 0;
    adxt->vsync_base = adxt_exec_count;
    adx_unlock();
}

/* Empties EE and IOP buffers, closes the file stream; mode 2 destroys the input joint. */
/* 0x001c98f8 */
void adxt_stop_unlocked(AdxtObj *adxt)
{
    int i;
    int n;
    AdxrnaObj *rna;
    SjObj *sj;

    adx_lock();
    adxrna_set_transfer_sw((void *)adxt->rna, 0);
    rna = (AdxrnaObj *)adxt->rna;
    n = adxt_get_nch(adxt);
    for (i = 0; i < n; i++)
        sjrmt_reset(ps2rna_get_sjrmt(rna, i));
    for (i = 0; i < n; i++)
        sjx_reset(rna->chobj[i][3]);
    for (i = 0; i < n; i++) {
        sj = ps2rna_get_sjuni(rna, i);
        sj->vtbl->reset(sj);
    }
    adxrna_set_play_sw((void *)adxt->rna, 0);
    adxsjd_stop(adxt->sjd);
    if (adxt->fstm != 0) {
        adxstm_stop((void *)adxt->fstm);
        adxstm_close((void *)adxt->fstm);
        if (adxt->sji != 0)
            adxt->sji->vtbl->reset(adxt->sji);
        adxt->fstm = 0;
    }
    if (adxt->pmode == 2 && adxt->sji != 0)
        adxt->sji->vtbl->destroy(adxt->sji);
    adxt->sji = 0;
    adxt->stat = 0;
    adx_unlock();
}

/* Play list (mode 4) is stopped first. */
/* 0x001c9a78 */
void adxt_stop(AdxtObj *adxt)
{
    adx_lock();
    if (adxt->pmode == 4) {
        lsc_stop(adxt->stm);
        if (adxt->sji != 0)
            adxt->sji->vtbl->reset(adxt->sji);
    }
    adxt_stop_unlocked(adxt);
    adx_unlock();
}

/* 0 stopped, 1 reading header, 2 buffering, 3 playing, 4 decoding done, 5 play end. */
/* 0x001c9ad8 */
int adxt_get_stat(AdxtObj *adxt)
{
    return adxt->stat;
}

/* adxt_get_time_vsync counting: 0 off, 1 NTSC (59.94 Hz), 2 PAL (50 Hz). */
/* 0x001c9ae0 */
void adxt_set_vsync_mode(int mode)
{
    if (mode == 1)
        adxt_vsync_rate = 5994;
    else if (mode == 2)
        adxt_vsync_rate = 5000;
    adxt_vsync_mode = mode;
}

/* Samples and rate: sound output while playing, whole stream at end, else 0; plus time offset. */
/* 0x001c9b08 */
void adxt_get_time_raw(AdxtObj *adxt, int *ncount, int *tscale)
{
    if (adxt->stat == 3 || adxt->stat == 4) {
        adxrna_get_time((void *)adxt->rna, ncount, tscale);
    } else if (adxt->stat == 5) {
        *ncount = adxsjd_get_total_smpl(adxt->sjd);
        *tscale = adxsjd_get_sfreq(adxt->sjd);
        *ncount *= 16 / adxsjd_get_out_bps(adxt->sjd);
    } else {
        *ncount = 0;
        *tscale = 1;
    }
    *ncount += adxt->time_ofst;
}

/* As adxt_get_time_raw; while playing, decoded samples minus those queued in the output. */
/* 0x001c9bd0 */
void adxt_get_time_dec(AdxtObj *adxt, int *ncount, int *tscale)
{
    int n;

    if (adxt->stat == 3 || adxt->stat == 4) {
        *tscale = adxsjd_get_sfreq(adxt->sjd);
        n = adxsjd_get_decoded_smpl(adxt->sjd);
        *ncount = n - adxrna_get_free_smpl((void *)adxt->rna);
    } else if (adxt->stat == 5) {
        *ncount = adxsjd_get_total_smpl(adxt->sjd);
        *tscale = adxsjd_get_sfreq(adxt->sjd);
        *ncount *= 16 / adxsjd_get_out_bps(adxt->sjd);
    } else {
        *ncount = 0;
        *tscale = 1;
    }
    *ncount += adxt->time_ofst;
}

/*
 * With a vsync mode: time in vsyncs (1/100 units, rate adxt_vsync_rate), re-synced when it drifts
 * over 60 ms from the decoded time. Otherwise adxt_get_time_dec.
 */
/* 0x001c9cb8 */
void adxt_get_time_vsync(AdxtObj *adxt, int *ncount, int *tscale)
{
    int nc;
    int ts;

    if (adxt_vsync_mode == 0) {
        adxt_get_time_dec(adxt, ncount, tscale);
        return;
    }
    adxt_vsync_time = 0;
    if (adxt->stat == 3 || adxt->stat == 4) {
        if (adxt->pause == 0)
            *ncount = (adxt_exec_count - adxt->vsync_base) * 100 + adxt->vsync_time;
        else
            *ncount = adxt->vsync_time;
        adxt_get_time_dec(adxt, &nc, &ts);
        adxt_vsync_time = ((float)nc / (float)ts - (float)*ncount / (float)adxt_vsync_rate) * 1000.0f;
        if (adxt_vsync_time > 60.0f || adxt_vsync_time < -60.0f) {
            adxrna_get_time((void *)adxt->rna, &nc, &ts);
            adxt->vsync_base = adxt_exec_count;
            adxt->vsync_time = (float)nc / (float)ts * (float)adxt_vsync_rate;
        }
    } else if (adxt->stat == 5) {
        nc = adxsjd_get_total_smpl(adxt->sjd);
        ts = adxsjd_get_sfreq(adxt->sjd);
        nc *= 16 / adxsjd_get_out_bps(adxt->sjd);
        *ncount = (float)nc / (float)ts * (float)adxt_vsync_rate;
        *ncount += adxt->vsync_time + 1;
    } else {
        *ncount = 0;
    }
    *ncount += adxt->time_ofst;
    *tscale = adxt_vsync_rate;
}

/* Hundredths of a second. */
/* 0x001c9ef8 */
int adxt_get_time_csec(AdxtObj *adxt)
{
    int nc;
    int ts;

    adxt_get_time_vsync(adxt, &nc, &ts);
    return (double)nc / (double)ts * 100.0;
}

/* 0 before the header is read. */
/* 0x001c9f58 */
int adxt_get_total_smpl(AdxtObj *adxt)
{
    if (adxt->stat >= 2)
        return adxsjd_get_total_smpl(adxt->sjd);
    return 0;
}

/* 0 before the header is read. */
/* 0x001c9f90 */
int adxt_get_sfreq(AdxtObj *adxt)
{
    if (adxt->stat >= 2)
        return adxsjd_get_sfreq(adxt->sjd);
    return 0;
}

/* 0 before the header is read. */
/* 0x001c9fc8 */
int adxt_get_nch(AdxtObj *adxt)
{
    if (adxt->stat >= 2)
        return adxsjd_get_nch(adxt->sjd);
    return 0;
}

/* 0 before the header is read. */
/* 0x001ca000 */
int adxt_get_hdr_size(AdxtObj *adxt)
{
    if (adxt->stat >= 2)
        return adxsjd_get_hdr_size(adxt->sjd);
    return 0;
}

/* Bits per encoded sample; 0 before the header is read. */
/* 0x001ca038 */
int adxt_get_sample_bits(AdxtObj *adxt)
{
    if (adxt->stat >= 2)
        return adxsjd_get_sample_bits(adxt->sjd);
    return 0;
}

/* -128 = default; reports a channel the handle lacks. */
/* 0x001ca070 */
void ADXT_SetOutPan(AdxtObj *adxt, int ch, int pan)
{
    adxt->outpan[ch] = pan;
    if (ch < adxt->nch)
        adxrna_set_out_pan((void *)adxt->rna, ch, pan);
    else
        adx_error("E8101208 ADXT_SetOutPan: parameter error");
}

/* 0x001ca0a0 */
int adxt_get_out_pan(AdxtObj *adxt, int ch)
{
    return adxt->outpan[ch];
}

/* 0x001ca0b0 */
void adxt_set_out_vol(AdxtObj *adxt, int vol)
{
    adxt->outvol = vol;
    adxrna_set_out_vol((void *)adxt->rna, vol);
}

/* 0x001ca0c8 */
int adxt_get_out_vol(AdxtObj *adxt)
{
    return adxt->outvol;
}

/* Server calls per second (default 60); sizes decode steps and watchdog timeouts. */
/* 0x001ca0d0 */
void adxt_set_svr_freq(AdxtObj *adxt, int x)
{
    adxt->svr_freq = x;
}

/* Read-ahead from seconds of nch-channel ADX (32 samples/18-byte block), capped at input size. */
/* 0x001ca0d8 */
void adxt_set_reload_time(AdxtObj *adxt, float sec, int nch, int sfreq)
{
    int n;

    nch = nch * 18;
    n = (int)(sec * (float)sfreq) / 32 * nch / 2048;
    adxt->minsct = (n < adxt->maxsct) ? n : adxt->maxsct;
}

/* 0x001ca130 */
void adxt_set_reload_sct(AdxtObj *adxt, int nsct)
{
    adxt->minsct = nsct;
}

/* 0 without one. */
/* 0x001ca138 */
int adxt_get_ibuf_sct(AdxtObj *adxt)
{
    SjObj *sj;

    sj = adxt->sji;
    if (sj != 0)
        return sj->vtbl->getnum(sj, 1) / 2048;
    return 0;
}

/* 0 without one. */
/* 0x001ca180 */
int adxt_get_obuf_smpl(AdxtObj *adxt, int ch)
{
    SjObj *sj;

    sj = adxt->sjo[ch];
    if (sj != 0)
        return sj->vtbl->getnum(sj, 1) / 2;
    return 0;
}

/* Seconds, counted as ADX blocks; 0 before the header or without a joint. */
/* 0x001ca1c8 */
float adxt_get_ibuf_time(AdxtObj *adxt)
{
    float ret;
    int n;
    int nch;
    int sf;

    ret = 0;
    if (adxt_get_stat(adxt) >= 2 && adxt->sji != 0) {
        n = adxt->sji->vtbl->getnum(adxt->sji, 1);
        nch = adxt_get_nch(adxt);
        sf = adxt_get_sfreq(adxt);
        ret = (float)(n / (nch * 18) * 32) / (float)sf;
    }
    return ret;
}

/* At least the read-ahead size. */
/* 0x001ca288 */
int adxt_is_ibuf_full(AdxtObj *adxt)
{
    SjObj *sj;

    sj = adxt->sji;
    if (sj != 0)
        return sj->vtbl->getnum(sj, 1) >= adxt->minsct << 11;
    return 0;
}

/* Watchdog action: 0 nothing, 1 stop, 2 restart from the top. */
/* 0x001ca2d8 */
void adxt_set_recover_mode(AdxtObj *adxt, int x)
{
    adxt->rcvr_mode = x;
}

/* 0x001ca2e0 */
int adxt_is_play_end(AdxtObj *adxt)
{
    return adxt->stat == 5;
}

/* fs, decoder, file and stream servers, every handle, sound output and play list servers. */
/* 0x001ca2f0 */
void adxt_exec_server(void)
{
    int i;

    adx_lock();
    cvfs_exec_server();
    adxsjd_exec_server();
    adxt_svr_stat1 = 0;
    adxt_svr_stat2 = 0;
    adxf_exec_server();
    adxt_svr_stat3 = 0;
    adxstm_exec_server();
    adxt_svr_stat4 = 0;
    for (i = 0; i < 8; i++) {
        if (adxt_obj_tbl[i].used == 1)
            adxt_exec_handle(&adxt_obj_tbl[i]);
    }
    adxrna_exec_server();
    lsc_exec_server();
    adxt_svr_stat5 = 0;
    adxt_svr_stat0 = 0;
    adx_unlock();
}

/* 0 none, -1 input starved or read error, -2 decoder stalled. */
/* 0x001ca398 */
int adxt_get_error(AdxtObj *adxt)
{
    return adxt->ercode;
}

/* Also the watchdog counters. */
/* 0x001ca3a0 */
void adxt_clear_error(AdxtObj *adxt)
{
    adxt->ercode = 0;
    adxt->unk_64 = 0;
    adxt->unk_68 = 0;
    adxt->unk_6a = 0;
}

/* 0x001ca3b8 */
int adxt_get_loop_count(AdxtObj *adxt)
{
    return adxt->lpcnt;
}

/* 0x001ca3c0 */
void adxt_set_loop_flag(AdxtObj *adxt, int x)
{
    adxt->lpflg = x;
}

/* 0x001ca3c8 */
SjObj *adxt_get_input_sj(AdxtObj *adxt)
{
    return adxt->sji;
}

/* Holds a buffered stream from starting while x is set. */
/* 0x001ca3d0 */
void adxt_set_start_hold(AdxtObj *adxt, int x)
{
    adxt->start_hold = x;
}

/* Enough buffered to start. */
/* 0x001ca3d8 */
int adxt_is_ready(AdxtObj *adxt)
{
    return adxt->ready;
}

/* sw 1 pause, 0 resume. Playing: stops/empties or restarts the output; re-syncs vsync time. */
/* 0x001ca3e0 */
void adxt_pause(AdxtObj *adxt, int sw)
{
    int stat;
    int i;
    int n;
    int nc;
    int ts;
    AdxrnaObj *rna;
    SjObj *sj;

    stat = adxt_get_stat(adxt);
    if (sw == adxt->pause)
        return;
    adx_lock();
    adxt->pause = sw;
    if (stat == 3 || stat == 4) {
        if (sw == 1) {
            adxrna_set_transfer_sw((void *)adxt->rna, 0);
            rna = (AdxrnaObj *)adxt->rna;
            n = adxt_get_nch(adxt);
            for (i = 0; i < n; i++)
                sjrmt_reset(ps2rna_get_sjrmt(rna, i));
            for (i = 0; i < n; i++)
                sjx_reset(rna->chobj[i][3]);
            for (i = 0; i < n; i++) {
                sj = ps2rna_get_sjuni(rna, i);
                sj->vtbl->reset(sj);
            }
            adxrna_set_play_sw((void *)adxt->rna, 0);
            adxrna_set_transfer_sw((void *)adxt->rna, 1);
            adxrna_get_time((void *)adxt->rna, &nc, &ts);
            adxt->vsync_time = (float)nc / (float)ts * (float)adxt_vsync_rate;
        } else {
            adxrna_set_play_sw((void *)adxt->rna, 1);
            adxrna_get_time((void *)adxt->rna, &nc, &ts);
            adxt->vsync_base = adxt_exec_count;
            adxt->vsync_time = (float)nc / (float)ts * (float)adxt_vsync_rate;
        }
    }
    adx_unlock();
}

/* 0x001ca5d0 */
void adxt_nop(void)
{
}

/* 0x001ca5d8 */
int adxt_return_zero(void)
{
    return 0;
}

/* 0x001ca5e0 */
void adxt_nop_2(void)
{
}

/* 0x001ca5e8 */
void adxt_nop_3(void)
{
}

/* 0x001ca5f0 */
void adxt_nop_4(void)
{
}

/* 0x001ca5f8 */
void adxt_nop_5(void)
{
}

/* 0x001ca600 */
void adxt_nop_6(void)
{
}

/* 0x001ca608 */
void adxt_nop_7(void)
{
}

/* 0x001ca610 */
void adxt_set_error_cb(void)
{
    adx_set_error_cb();
}

/* 0x001ca618 */
int adxt_return_16384(void)
{
    return 16384;
}

/* Paused: discards queued output, runs the server once, re-syncs vsync time; 0 if not paused. */
/* 0x001ca620 */
int adxt_discard_data(AdxtObjView *adxt)
{
    int ret;
    int ticks[2];

    if (adxt->pause_flag == 0)
        return 0;
    ret = adxrna_discard_data(adxt->rna);
    adxt_exec_server();
    adxrna_get_time(adxt->rna, &ticks[0], &ticks[1]);
    adxt->tvofst = (int)((float)ticks[0] / (float)ticks[1] * (float)adxt_vsync_rate);
    adxt->svcnt = adxt_exec_count;
    return ret;
}

/* Samples added to the play time. */
/* 0x001ca6b8 */
void adxt_set_time_ofst(AdxtObjView *adxt, int ofst)
{
    adxt->time_ofst = ofst;
}

/* Linked playback: at a stream's end, play the next one in the data. */
/* 0x001ca6c0 */
void adxt_set_flag98(AdxtObjView *adxt, int flag)
{
    adxt->lnkflg = flag;
}

/* 0x001ca6c8 */
int adxt_get_flag98(AdxtObjView *adxt)
{
    return adxt->lnkflg;
}

/* Sees each channel's decoded samples. */
/* 0x001ca6d0 */
void adxt_set_filter_cb(AdxtObjView *adxt, int fn, int obj)
{
    adxsjd_set_filter_cb(adxt->sjd, fn, obj);
}

/* 0x001ca6d8 */
int adxt_get_decoded_smpl(AdxtObjView *adxt)
{
    return adxsjd_get_decoded_smpl(adxt->sjd);
}

/* 1 with *hdrlen if the data starts with an ADX header. */
/* 0x001ca6f8 */
int adxt_is_adx_header(unsigned char *hdr, int len, int *hdrlen)
{
    unsigned short sig;
    short hl;
    signed char type;
    signed char sample_bits;
    signed char blk;
    signed char nch;
    int sfreq;
    int total_smpl;
    int spb;

    if (len < 2)
        return 0;
    sig = *(unsigned short *)hdr;
    if ((unsigned short)((sig << 8) | (sig >> 8)) != 0x8000)
        return 0;
    if (adxb_parse_adx_header(hdr, len, &hl, &type, &sample_bits, &blk, &nch, &sfreq, &total_smpl, &spb) < 0)
        return 0;
    *hdrlen = hl;
    return 1;
}

/* 1 with the data length in *n if the data starts with an ADX end block. */
/* 0x001ca788 */
int adxt_is_end_block(unsigned char *hdr, int len, int *n)
{
    unsigned short sig;

    if (len < 2)
        return 0;
    sig = *(unsigned short *)hdr;
    if ((unsigned short)((sig << 8) | (sig >> 8)) != 0x8001)
        return 0;
    *n = len;
    return 1;
}

/* Zeroed ADX blocks of nch channels, two passes for the ring wrap; returns samples written. */
/* 0x001ca7c0 */
int adxt_fill_silence(AdxtObjView *adxt, int nch, int nsmpl)
{
    SjObj *sj;
    int blksz;
    int nbyte;
    int n;
    int total;
    SjCk ck;
    SjCk tail;

    sj = adxt->sji;
    if (sj == 0)
        return 0;
    blksz = nch * 18;
    nbyte = nsmpl / 32 * blksz;
    sj->vtbl->getchunk(sj, 0, nbyte, &ck);
    n = ck.len / blksz * blksz;
    memset(ck.data, 0, n);
    sj_split_chunk(&ck, n, &ck, &tail);
    total = n;
    sj->vtbl->putchunk(sj, 1, &ck);
    sj->vtbl->ungetchunk(sj, 0, &tail);
    sj->vtbl->getchunk(sj, 0, nbyte - total, &ck);
    n = ck.len / blksz * blksz;
    memset(ck.data, 0, n);
    sj_split_chunk(&ck, n, &ck, &tail);
    total += n;
    sj->vtbl->putchunk(sj, 1, &ck);
    sj->vtbl->ungetchunk(sj, 0, &tail);
    return total / blksz * 32;
}

/* Stereo streams mix down to mono. */
/* 0x001ca968 */
void adxt_set_output_mono(void)
{
    adxb_set_mono();
}

/* 0x001ca970 */
void adxt_call_destroy_cb(void)
{
    if (adxt_destroy_cb)
        adxt_destroy_cb();
}

/*
 * Loop trap at the loop end. Looping off (no stream): plays on. Else skips input past the loop
 * end, resets the decoder to the loop start with the loop length as limit (mode 2 re-reads).
 */
/* 0x001ca998 */
void adxt_trap_entry(AdxtObjView *adxt)
{
    void *sjd;
    SjObj *sj;
    int top;
    int len;
    int end;
    SjCk ck;

    sjd = adxt->sjd;
    sj = adxt->sji;
    top = adxsjd_get_loop_start_smpl(sjd);
    len = adxsjd_get_loop_start_ofst(sjd);
    end = adxsjd_get_loop_end_smpl(sjd);
    if (adxt->stm == 0 && adxt->lpflg == 0) {
        adxsjd_set_smpl_limit(adxt->sjd, -1);
        return;
    }
    sj->vtbl->getchunk(sj, 1, adxt->lp_skiplen, &ck);
    if (ck.len < adxt->lp_skiplen)
        adx_error("E8101201 adxt_trap_entry: not enough data");
    sj->vtbl->putchunk(sj, 0, &ck);
    adxsjd_set_limit_count(sjd, 0);
    adxt->trpnsmpl = end - top;
    adxsjd_set_smpl_limit(sjd, end - top);
    adxsjd_set_byte_count(sjd, len);
    adxsjd_set_smpl_pos(sjd, top);
    if (adxt->pmode == 2) {
        sj->vtbl->reset(sj);
        sj->vtbl->getchunk(sj, 1, len, &ck);
        sj->vtbl->putchunk(sj, 0, &ck);
    }
    adxt->lpcnt++;
}

/* At the loop-end sector: looping off reads on to EOF, else seeks back to the loop start sector. */
/* 0x001cab20 */
void adxt_stm_end_cb(AdxtObjView *adxt)
{
    void *stm;
    int len;

    stm = adxt->stm;
    len = adxsjd_get_loop_start_ofst(adxt->sjd);
    if (adxt->lpflg == 0) {
        adxsjd_set_smpl_limit(adxt->sjd, -1);
        adxstm_set_size_sct(adxt->stm, 0x7fffffff);
    } else {
        adxstm_seek(stm, len / 2048);
    }
}

/* Default: mono centred, stereo -15/+15. */
/* 0x001caba8 */
void adxt_apply_pan(AdxtObjView *adxt)
{
    if (adxsjd_get_nch(adxt->sjd) == 1) {
        if (adxt->outpan0 == -128)
            adxrna_set_out_pan(adxt->rna, 0, 0);
        else
            adxrna_set_out_pan(adxt->rna, 0, adxt->outpan0);
    } else {
        if (adxt->outpan0 == -128)
            adxrna_set_out_pan(adxt->rna, 0, -15);
        else
            adxrna_set_out_pan(adxt->rna, 0, adxt->outpan0);
        if (adxt->outpan1 == -128)
            adxrna_set_out_pan(adxt->rna, 1, 15);
        else
            adxrna_set_out_pan(adxt->rna, 1, adxt->outpan1);
    }
}

/*
 * Linked playback end trap: skips input to the next stream's header and restarts the decoder with
 * its length as limit; turns linked playback off when none follows.
 */
/* 0x001cac68 */
void adxt_seek_next_block(AdxtObjView *adxt)
{
    void *sjd;
    SjObj *sj;
    short n;
    short second_ofs;
    int found1;
    int found2;
    SjCk first;
    SjCk first_tail;
    SjCk second;
    SjCk second_tail;
    short ofs[2];
    int len;

    sjd = adxt->sjd;
    sj = adxt->sji;
    if (adxt->lnkflg == 0)
        return;
    sj->vtbl->getchunk(sj, 1, 0x7fffffff, &first);
    sj->vtbl->getchunk(sj, 1, 0x7fffffff, &second);
    len = first.len;
    if (adxb_parse_end_block(first.data, len, &ofs[0]) != 0) {
        adxt->lnkflg = 0;
        sj->vtbl->ungetchunk(sj, 1, &second);
        sj->vtbl->ungetchunk(sj, 1, &first);
        return;
    }
    n = ofs[0];
    found1 = adxb_find_word80(first.data + n, first.len - n, &ofs[0]);
    if (found1 == 0)
        found2 = -1;
    else
        found2 = adxb_find_word80(second.data, second.len, &ofs[1]);
    n += ofs[0];
    second_ofs = ofs[1];
    if (found1 != 0 && found2 != 0) {
        sj->vtbl->ungetchunk(sj, 1, &second);
        sj->vtbl->ungetchunk(sj, 1, &first);
        adxt->lnkflg = 0;
        return;
    } else if (found1 == 0) {
        sj->vtbl->ungetchunk(sj, 1, &second);
        sj_split_chunk(&first, n, &first, &first_tail);
        sj->vtbl->putchunk(sj, 0, &first);
        sj->vtbl->ungetchunk(sj, 1, &first_tail);
    } else {
        sj->vtbl->putchunk(sj, 0, &first);
        sj_split_chunk(&second, second_ofs, &second, &second_tail);
        sj->vtbl->putchunk(sj, 0, &second);
        sj->vtbl->ungetchunk(sj, 1, &second_tail);
    }
    adxsjd_stop(sjd);
    adxsjd_start(sjd);
    adxsjd_exec_handle(sjd);
    if (adxsjd_get_stat(sjd) != 2) {
        adxt->lnkflg = 0;
        return;
    }
    adxsjd_set_max_dec_smpl(sjd, adxt->maxdecsmpl);
    adxsjd_set_smpl_limit(sjd, adxsjd_get_total_smpl(sjd));
    adxsjd_set_byte_count(sjd, 0);
    adxsjd_set_limit_count(sjd, 0);
}

/*
 * After the header: refuses too many channels, sizes decode steps, arms the loop trap (and stream
 * end callback) or the end trap, sets up the output, starts buffering (state 2).
 */
/* 0x001caf28 */
void adxt_stat_decinfo(AdxtObjView *adxt)
{
    int sctsz = 2048;
    void *sjd;
    int nch;
    int sfreq;
    int lp;
    int n;
    int len;
    int loop_end;
    char buf[32];

    sjd = adxt->sjd;
    if (adxsjd_get_stat(sjd) != 2)
        return;
    nch = adxsjd_get_nch(sjd);
    if (adxt->maxnch < nch) {
        adx_format_2num(nch, adxt->maxnch, buf, 16);
        adx_error_cat("E9081001 adxt_stat_decinfo: can't play this number of channels", buf);
        adxt_stop(adxt);
        return;
    }
    sfreq = adxsjd_get_sfreq(sjd);
    lp = adxsjd_get_loop_flag(sjd);
    if (lp > 0)
        adxt->maxdecsmpl = sfreq / adxt->svrfreq * 3;
    else
        adxt->maxdecsmpl = sfreq / adxt->svrfreq * 3 / 2;
    n = adxsjd_get_blk_smpl(sjd) * 2;
    adxt->maxdecsmpl = (adxt->maxdecsmpl + n) / n * n;
    adxsjd_set_max_dec_smpl(sjd, adxt->maxdecsmpl);
    if (lp > 0) {
        if (adxt->pmode == 2) {
            adxt->lp_skiplen = 0;
        } else {
            loop_end = adxsjd_get_loop_end_ofst(sjd);
            adxt->lp_skiplen = (sctsz - loop_end % sctsz) % sctsz;
            adxt->lesct = (loop_end + 2047) / sctsz;
            adxstm_set_size_sct(adxt->stm, adxt->lesct);
            adxstm_set_end_cb(adxt->stm, adxt_stm_end_cb, adxt);
        }
        adxt->trpnsmpl = adxsjd_get_loop_end_smpl(sjd);
        adxsjd_set_smpl_limit(sjd, adxt->trpnsmpl);
        adxsjd_set_byte_count(sjd, 0);
        adxsjd_set_limit_count(sjd, 0);
        adxsjd_set_limit_cb(sjd, adxt_trap_entry, adxt);
    } else {
        if (adxt->stm)
            adxstm_set_size_sct(adxt->stm, 0x7fffffff);
        adxsjd_set_smpl_limit(sjd, adxsjd_get_total_smpl(sjd));
        adxsjd_set_byte_count(sjd, 0);
        adxsjd_set_limit_count(sjd, 0);
        adxsjd_set_limit_cb(sjd, adxt_seek_next_block, adxt);
    }
    sfreq = adxsjd_get_sfreq(sjd);
    nch = adxsjd_get_nch(sjd);
    len = adxsjd_get_total_smpl(sjd);
    adxrna_set_bit_per_smpl(adxt->rna, adxsjd_get_out_bps(sjd));
    adxrna_set_sfreq(adxt->rna, sfreq);
    adxrna_set_nch(adxt->rna, nch);
    adxrna_set_total_smpl(adxt->rna, len);
    adxrna_set_out_vol(adxt->rna, adxt->outvol);
    adxt_apply_pan(adxt);
    if (adxt->amp)
        adx_peak_set_sfreq(adxt->amp, sfreq);
    if (adxsjd_get_format(sjd) == 2)
        adxrna_set_stm_hd_info(adxt->rna, adxsjd_get_hdr_copy(sjd));
    adxrna_set_transfer_sw(adxt->rna, 1);
    adxt->stat = 2;
}

/* Output full or decoding done: ready, and unless held, play (state 3); done pads silence. */
/* 0x001cb248 */
void adxt_check_start(AdxtObjView *adxt)
{
    void *rna;
    void *sjd;
    int nout;
    int nroom;
    int nch;
    int nbyte;
    int i;
    SjObj *sj;
    SjCk ck;

    rna = adxt->rna;
    sjd = adxt->sjd;
    nout = adxrna_get_free_smpl(rna);
    nroom = adxrna_get_data_smpl(rna);
    if (!(nout < adxt->maxdecsmpl * 2 && adxsjd_get_blk_smpl(sjd) < nroom) || adxsjd_get_stat(adxt->sjd) == 3) {
        if (adxt->pstwait_flag == 0) {
            if (adxt->pause_flag == 0) {
                adxrna_set_play_sw(rna, 1);
                adxt->tvofst = 0;
                adxt->svcnt = adxt_exec_count;
            }
            adxt->stat = 3;
        }
        adxt->pstready_flag = 1;
    }
    if (adxsjd_get_stat(adxt->sjd) == 3) {
        nch = adxt_get_nch(adxt);
        nbyte = adxt->maxdecsmpl * nch * 2;
        for (i = 0; i < nch; i++) {
            sj = adxt->sjo[i];
            sj->vtbl->getchunk(sj, 0, nbyte, &ck);
            memset(ck.data, 0, ck.len);
            sj->vtbl->putchunk(sj, 1, &ck);
        }
    }
}

/* Decoding done and every output joint under 64 bytes: stops feeding the output (state 4). */
/* 0x001cb3c0 */
void adxt_check_dec_end(AdxtObjView *adxt)
{
    int i;
    int nch;

    if (adxsjd_get_stat(adxt->sjd) != 3)
        return;
    nch = adxsjd_get_nch(adxt->sjd);
    adxt_dec_end_nch = nch;
    for (i = 0; i < nch; i++) {
        adxt_dec_end_nbyte = adxt->sjo[i]->vtbl->getnum(adxt->sjo[i], 1);
        if (adxt_dec_end_nbyte >= 64)
            break;
    }
    if (i == nch) {
        adxrna_set_transfer_sw(adxt->rna, 0);
        adxt->stat = 4;
    }
}

/* 512 or fewer samples queued: stops the output, play end (state 5). */
/* 0x001cb480 */
void adxt_check_play_end(AdxtObjView *adxt)
{
    adxt_play_end_free = adxrna_get_free_smpl(adxt->rna);
    if (adxrna_get_free_smpl(adxt->rna) <= 512) {
        adxrna_set_play_sw(adxt->rna, 0);
        adxt->stat = 5;
    }
}

/* 0x001cb4d8 */
void adxt_exec_play_end(AdxtObjView *adxt)
{
}

/* From the top: stops output and decoder, empties joints, rewinds the stream, decodes again. */
/* 0x001cb4e0 */
void adxt_restart(AdxtObjView *adxt)
{
    int i;

    adx_lock();
    adxrna_set_transfer_sw(adxt->rna, 0);
    adxrna_set_play_sw(adxt->rna, 0);
    adxsjd_stop(adxt->sjd);
    if (adxt->stm) {
        adxstm_stop(adxt->stm);
        adxt->sji->vtbl->reset(adxt->sji);
    }
    for (i = 0; i < adxt->maxnch; i++)
        adxt->sjo[i]->vtbl->reset(adxt->sjo[i]);
    if (adxt->stm) {
        adxstm_seek(adxt->stm, 0);
        adxstm_start(adxt->stm);
    }
    adxt_start_decode(adxt, adxt->sji);
    adx_unlock();
}

/*
 * Watchdog: -2 decoder stall (no samples 5 s while playing), -1 starved input (under 64 bytes 5 s
 * playing, 20 s before) or read error. Recovery: 1 stop, 2 restart; a stall always stops.
 */
/* 0x001cb5c0 */
void adxt_check_error(AdxtObjView *adxt)
{
    int stat;
    int n;
    int nbyte;
    SjObj *sj;

    stat = adxt->stat;
    if (stat == 3 && adxt->pause_flag == 0 && adxsjd_get_stat(adxt->sjd) != stat) {
        n = adxsjd_get_decoded_smpl(adxt->sjd);
        if (adxt->edecpos == n) {
            adxt->edeccnt++;
            if (adxt->svrfreq * 5 < adxt->edeccnt)
                adxt->ercode = -2;
        } else {
            adxt->edeccnt = 0;
        }
        adxt->edecpos = n;
        if (adxt->ercode != 0) {
            if (adxt->autorcvr == 1 || adxt->autorcvr == 2)
                adxt_stop(adxt);
            if (adxt->autorcvr != 0) {
                adxt->ercode = 0;
                adxt->edeccnt = 0;
            }
        }
    } else {
        adxt->edeccnt = 0;
    }
    if ((stat == 1 || stat == 2 || stat == 3) && adxt->pause_flag == 0 && adxsjd_get_stat(adxt->sjd) != 3) {
        sj = adxt->sji;
        if (sj)
            nbyte = sj->vtbl->getnum(sj, 1);
        else
            nbyte = 0;
        if (nbyte < 64) {
            adxt->eshrtcnt++;
            if (stat == 3) {
                if (adxt->svrfreq * 5 < adxt->eshrtcnt)
                    adxt->ercode = -1;
            } else {
                if (adxt->svrfreq * 20 < adxt->eshrtcnt)
                    adxt->ercode = -1;
            }
            if (adxt->ercode != 0) {
                if (adxt->autorcvr == 1)
                    adxt_stop(adxt);
                else if (adxt->autorcvr == 2)
                    adxt_restart(adxt);
                if (adxt->autorcvr != 0) {
                    adxt->ercode = 0;
                    adxt->eshrtcnt = 0;
                }
            }
        } else {
            adxt->eshrtcnt = 0;
        }
    } else {
        adxt->eshrtcnt = 0;
    }
    if (adxt->stm && adxstm_get_stat(adxt->stm) == 4) {
        if (adxt->autorcvr == 1)
            adxt_stop(adxt);
        else if (adxt->autorcvr == 2)
            adxt_restart(adxt);
        if (adxt->autorcvr != 0) {
            adxt->ercode = 0;
            adxt->eshrtcnt = 0;
        }
    }
}

/* -1 on a stream read error. */
/* 0x001cb808 */
void adxt_check_stm_error(AdxtObjView *adxt)
{
    if (adxt->stm && adxstm_get_stat(adxt->stm) == 4)
        adxt->ercode = -1;
}

/* 0x001cb850 */
void adxt_check_stm_error_2(AdxtObjView *adxt)
{
    adxt_check_stm_error(adxt);
}

/* State step, then the stream read error check. */
/* 0x001cb858 */
void adxt_exec_handle(AdxtObjView *adxt)
{
    if (adxt->stat == 3)
        adxt_check_dec_end(adxt);
    else if (adxt->stat == 1)
        adxt_stat_decinfo(adxt);
    else if (adxt->stat == 2)
        adxt_check_start(adxt);
    else if (adxt->stat == 4)
        adxt_check_play_end(adxt);
    else if (adxt->stat == 5)
        adxt_exec_play_end(adxt);
    adxt_check_stm_error(adxt);
}
