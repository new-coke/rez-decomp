/* Peak level meter: up to eight; 16-bit PCM in, one record per period out. */

#include "types_cri.h"
#include "adx_peak.h"
#include "libc.h"
#include "adx_lock.h"

/* Clears the meters on the first call; counts calls. */
/* 0x001d43b0 */
static void adx_peak_lib_init(void)
{
    if (adx_peak_init_count == 0)
        memset(adx_peak_obj_tbl, 0, 384);
    adx_peak_init_count++;
}

/* Clears the meters when the last user finishes. */
/* 0x001d43f0 */
static void adx_peak_lib_finish(void)
{
    if (--adx_peak_init_count == 0)
        memset(adx_peak_obj_tbl, 0, 384);
}

/* 44100 Hz, 0.1 s periods; 0 when all eight are in use. */
/* 0x001d4420 */
static PkmObj *adx_peak_create(int nch, SjObj **in, SjObj **out)
{
    PkmObj *obj;
    int i;
    int j;

    for (i = 0; i < 8; i++) {
        if (adx_peak_obj_tbl[i].used == 0)
            break;
    }
    if (i == 8)
        return 0;
    adx_lock();
    obj = &adx_peak_obj_tbl[i];
    obj->nch = nch;
    for (j = 0; j < nch; j++) {
        obj->in[j] = in[j];
        obj->out[j] = out[j];
        obj->pos[j] = 0;
    }
    obj->nchi = nch;
    obj->rate = 44100;
    obj->stat = 0;
    obj->nrec = 0;
    obj->unk_28 = obj->period = 0.1f;
    obj->used = 1;
    adx_unlock();
    return obj;
}

/* 0x001d4540 */
void adx_peak_destroy(PkmObj *obj)
{
    if (obj != 0) {
        adx_lock();
        memset(obj, 0, 48);
        adx_unlock();
    }
}

/* 0x001d4590 */
static int adx_peak_get_stat(PkmObj *obj)
{
    return obj->stat;
}

/* From position 0 (status 2). */
/* 0x001d4598 */
static void adx_peak_start(PkmObj *obj)
{
    int i;

    for (i = 0; i < obj->nch; i++)
        obj->pos[i] = 0;
    obj->nrec = 0;
    obj->stat = 2;
}

/* 0x001d45d8 */
static void adx_peak_stop(PkmObj *obj)
{
    obj->stat = 0;
}

/* Per channel, one {peak, position, rate, samples} record per period. Hangs if output is full. */
/* 0x001d45e0 */
static void adx_peak_measure(PkmObj *obj)
{
    SjChunk ck;
    SjChunk rk;
    int *rec;
    short *src;
    int n;
    int ch;
    int nper;
    int k;
    int got;
    int peak;
    int cnt;
    int j;
    int i;

    n = (float)obj->rate * obj->period;
    for (ch = 0; ch < obj->nchi; ch++) {
        nper = (int)((unsigned int)obj->in[ch]->vtbl->getnum(obj->in[ch], 1) / 2) / n;
        k = (unsigned int)obj->out[ch]->vtbl->getnum(obj->out[ch], 0) / 16;
        if (nper < k)
            k = nper;
        for (j = 0; j < k; j++) {
            got = 0;
            peak = 0;
            while (got < n) {
                obj->in[ch]->vtbl->getchunk(obj->in[ch], 1, (n - got) * 2, &ck);
                cnt = (unsigned int)ck.len / 2;
                src = (short *)ck.data;
                for (i = 0; i < cnt; i++) {
                    int amp = *src++;
                    if (amp < 0)
                        amp = -amp;
                    if (peak < amp)
                        peak = amp;
                }
                obj->in[ch]->vtbl->putchunk(obj->in[ch], 0, &ck);
                got += cnt;
            }
            obj->out[ch]->vtbl->getchunk(obj->out[ch], 0, 16, &rk);
            if (rk.len == 0) {
                for (;;)
                    ;
            }
            rec = (int *)rk.data;
            rec[0] = peak;
            rec[1] = obj->pos[ch];
            rec[2] = obj->rate;
            rec[3] = n;
            obj->out[ch]->vtbl->putchunk(obj->out[ch], 1, &rk);
            obj->pos[ch] += n;
            obj->nrec++;
        }
    }
}

/* 0x001d4880 */
static void adx_peak_exec_handle(PkmObj *obj)
{
    if (obj->stat == 2)
        adx_peak_measure(obj);
}

/* 0x001d48a0 */
static void adx_peak_exec_server(void)
{
    int i;

    for (i = 0; i < 8; i++) {
        if (adx_peak_obj_tbl[i].used == 1)
            adx_peak_exec_handle(&adx_peak_obj_tbl[i]);
    }
}

/* Channel 0, samples. */
/* 0x001d4900 */
static int adx_peak_get_pos(PkmObj *obj)
{
    return obj->pos[0];
}

/* 0x001d4908 */
void adx_peak_set_sfreq(PkmObj *obj, int rate)
{
    obj->rate = rate;
}

/* 0x001d4910 */
static int adx_peak_get_sfreq(PkmObj *obj)
{
    return obj->rate;
}

/* Seconds per record. */
/* 0x001d4918 */
static void adx_peak_set_period(PkmObj *obj, float period)
{
    obj->period = period;
}

/* 0x001d4920 */
static float adx_peak_get_period(PkmObj *obj)
{
    return obj->period;
}

/* Second period value (create sets it equal to the period). */
/* 0x001d4928 */
static void adx_peak_set_float28(PkmObj *obj, float val)
{
    obj->unk_28 = val;
}

/* 0x001d4930 */
static float adx_peak_get_float28(PkmObj *obj)
{
    return obj->unk_28;
}
