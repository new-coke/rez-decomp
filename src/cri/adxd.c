/* Low level ADX decoder: eight objects, each with one stream's coefficients and history. */

#include "types_cri.h"
#include "adxd.h"
#include "libc.h"
#include "adx_decode.h"

/* 0x001d7210 */
void adxd_lib_init(void)
{
    memset(adxd_obj_tbl, 0, 416);
}

/* Default coefficients: 500 Hz cutoff at 44100 Hz. 0 when all eight are in use. */
/* 0x001d7228 */
void *adxd_create(void)
{
    AdxdObj *adxd;
    int i;

    for (i = 0; i < 8; i++) {
        if (adxd_obj_tbl[i].used == 0)
            break;
    }
    if (i == 8)
        return 0;
    adxd = &adxd_obj_tbl[i];
    memset(adxd, 0, 52);
    adxd->used = 1;
    adxd->id = i;
    adxd->unk_08 = 0;
    adxd->stat = 0;
    adx_calc_coef(500, 44100, &adxd->coef1, &adxd->coef2);
    memset(adxd->histl, 0, 8);
    return adxd;
}

/* 0x001d72e8 */
void adxd_set_coef(void *obj, int sfreq, int cutoff)
{
    AdxdObj *adxd;

    adxd = obj;
    adx_calc_coef(cutoff, sfreq, &adxd->coef1, &adxd->coef2);
}

/* 0x001d7300 */
void adxd_destroy(void *obj)
{
    AdxdObj *adxd;

    adxd = obj;
    if (adxd != 0) {
        adxd->used = 0;
        memset(adxd, 0, 52);
    }
}

/* Unnamed setting; create clears it. */
/* 0x001d7320 */
static void adxd_set_unk_08(void *obj, int val)
{
    AdxdObj *adxd = obj;

    adxd->unk_08 = val;
}

/* 0 idle, 1 started, 2 decoding, 3 finished. */
/* 0x001d7328 */
int adxd_get_stat(void *obj)
{
    AdxdObj *adxd = obj;

    return adxd->stat;
}

/* Idle decoders only; returns 1 if accepted. */
/* 0x001d7330 */
int adxd_setup_mono(void *obj, void *src, int n, short *dst)
{
    AdxdObj *adxd = obj;

    if (adxd->stat == 0) {
        adxd->src = src;
        adxd->mode = 1;
        adxd->nblk = n;
        adxd->dstl = dst;
        adxd->dstr = 0;
        return 1;
    }
    return 0;
}

/* Idle decoders only; returns 1 if accepted. */
/* 0x001d7360 */
int adxd_setup_stereo(void *obj, void *src, int n, short *dstl, short *dstr)
{
    AdxdObj *adxd = obj;

    if (adxd->stat == 0) {
        adxd->mode = 2;
        adxd->src = src;
        adxd->nblk = n;
        adxd->dstl = dstl;
        adxd->dstr = dstr;
        return 1;
    }
    return 0;
}

/* 0x001d7390 */
void adxd_start(void *obj)
{
    AdxdObj *adxd = obj;

    if (adxd->stat == 0) {
        adxd->ndec = 0;
        adxd->stat = 1;
    }
}

/* Also clears the history. */
/* 0x001d73b0 */
void adxd_stop(void *obj)
{
    AdxdObj *adxd = obj;

    adxd->stat = 0;
    memset(adxd->histl, 0, 8);
}

/* Finished -> idle. */
/* 0x001d73c8 */
void adxd_reset_after_end(void *obj)
{
    AdxdObj *adxd = obj;

    if (adxd->stat == 3)
        adxd->stat = 0;
}

/* Blocks of the last decode. */
/* 0x001d73e0 */
int adxd_get_num_blk(void *obj)
{
    AdxdObj *adxd = obj;

    return adxd->ndec;
}

/* Stereo decode stopped after an odd block count. */
/* 0x001d73e8 */
static void adxd_set_odd_flag(void)
{
    adxd_odd_flag = 1;
}

/* Decodes the whole request in one go and finishes. */
/* 0x001d73f8 */
void adxd_exec_handle(void *obj)
{
    AdxdObj *adxd = obj;

    if (adxd->stat == 1)
        adxd->stat = 2;
    if (adxd->stat == 2) {
        if (adxd->mode == 1) {
            adxd->ndec = adxd_decode_mono(adxd->src, adxd->nblk, adxd->dstl, adxd->histl, adxd->coef1, adxd->coef2);
        } else {
            adxd->ndec = adxd_decode_stereo(adxd->src, adxd->nblk, adxd->dstl, adxd->histl, adxd->dstr, adxd->histr, adxd->coef1, adxd->coef2);
            if (adxd->ndec % 2 == 1)
                adxd_set_odd_flag();
        }
        adxd->stat = 3;
    }
}

/* 0x001d74c0 */
static void adxd_exec_server(void)
{
    int i;

    for (i = 0; i < 8; i++) {
        if (adxd_obj_tbl[i].used == 1)
            adxd_exec_handle(&adxd_obj_tbl[i]);
    }
}
