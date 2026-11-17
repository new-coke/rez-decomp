/* RIFF WAVE input for ADXB: "fmt " and "data" chunks, 16-, 8- or 4-bit samples. */

#include "types_cri.h"
#include "adxb_wave.h"
#include "libc.h"
#include "adxd.h"

static inline int RD32(unsigned char *src)
{
    return src[0] | src[1] << 8 | src[2] << 16 | src[3] << 24;
}

/* PCM only; derives sample count and format (16, 8, 4-bit); returns 0, or -1 if unsupported. */
/* 0x001d5918 */
static int adxb_parse_wave_header(unsigned char *hdr, int len, short *ofs, signed char *type, signed char *sample_bits,
                 signed char *blk, signed char *nch, int *sfreq, int *total_smpl, int *spb, short *pcm_fmt)
{
    unsigned char *fmt;
    unsigned char *field;
    int i;
    int size;

    for (i = 0; i < len; i++) {
        if (memcmp(hdr + i, adxb_wave_fmt_tag, 4) == 0)
            break;
    }
    if (i == len)
        return -1;
    if (i & 3)
        return -1;
    fmt = &hdr[i + 8];
    if (*(unsigned short *)fmt >= 2)
        return -1;
    for (i = 0; i < len; i++) {
        if (memcmp(hdr + i, adxb_wave_data_tag, 4) == 0)
            break;
    }
    if (i == len)
        return -1;
    field = &hdr[i + 4];
    size = RD32(field);
    *ofs = i + 8;
    *type = -1;
    field = fmt + 4;
    *sfreq = RD32(field);
    *nch = fmt[2];
    *sample_bits = fmt[14];
    *blk = fmt[12];
    *total_smpl = size / *blk;
    *spb = 1;
    if (*sample_bits == 16) {
        *pcm_fmt = 0;
    } else if (*sample_bits == 8) {
        *pcm_fmt = 1;
    } else if (*sample_bits == 4) {
        *blk = *nch * 2;
        *spb = 4;
        *total_smpl = ((unsigned int)size >> 1) / *nch;
        *sample_bits = 16;
        *pcm_fmt = 2;
    }
    if (*sample_bits == 0)
        return -1;
    if (*blk == 0)
        return -1;
    if ((unsigned char)*nch != 1 && (unsigned char)*nch != 2)
        return -1;
    return *sfreq ? 0 : -1;
}

/* Format 1, no loop; returns the data offset, or 0. */
/* 0x001d5b88 */
int adxb_decode_header_wave(AdxbObj *adxb, void *hdr, int len)
{
    short ofs;

    adxb->hdrdec = 1;
    if (adxb_parse_wave_header(hdr, len, &ofs, &adxb->type, &adxb->sample_bits, &adxb->blk, &adxb->nch, &adxb->total, &adxb->total_smpl,
                     &adxb->spb, &adxb->pcm_fmt) < 0)
        return 0;
    adxb->format = 1;
    adxb->cutoff = 0;
    adxb->unk_4a = 0;
    adxb->unk_48 = 0;
    adxb->loop_type = 0;
    adxb->loop_flag = 0;
    adxb->loop_end_ofst = 0;
    adxb->loop_end_smpl = 0;
    adxb->loop_start_ofst = 0;
    adxb->loop_start_smpl = 0;
    adxb->loop_ins_smpl = 0;
    adxb->dec.unk_2c = 0;
    adxb->dec.unk_2e = 0;
    adxb->wpos = 0;
    adxb->wr_nsmpl = 0;
    adxb->dec.nch = adxb->nch;
    adxb->dec.blk = adxb->blk;
    adxb->dec.spb = adxb->spb;
    adxb->dec.obuf = adxb->obuf;
    adxb->dec.obsize = adxb->obsize;
    adxb->dec.obsize2 = adxb->obsize2;
    adxb->dec.state = &adxb->dec_state;
    memset(&adxb->dec_state, 0, 4);
    return ofs;
}

/* Copies into the output window; stereo split. */
/* 0x001d5c88 */
static void adxb_exec_wave16(AdxbObj *adxb)
{
    AdxbDec *dec;
    short *src;
    short *dstl;
    int n;
    short *dstr;
    int i;

    dec = &adxb->dec;
    src = adxb->dec.buf;
    if (adxb->stat == 1 && adxd_get_stat(adxb->adxd) == 0) {
        adxb->getwr(adxb->getwrobj, &dec->wpos, &dec->nfree, &dec->nleft);
        dstl = (short *)dec->obuf + dec->wpos;
        n = dec->obsize - dec->wpos;
        if (dec->nfree < n)
            n = dec->nfree;
        if (dec->len < n)
            n = dec->len;
        if (adxb->nch == 2) {
            dstr = (short *)dec->obuf + (dec->obsize2 + dec->wpos);
            for (i = 0; i < n; i++) {
                dstl[i] = src[i * 2];
                dstr[i] = src[i * 2 + 1];
            }
        } else {
            for (i = 0; i < n; i++)
                dstl[i] = src[i];
        }
        adxb->stat = 2;
        adxb->dec_nsmpl = n;
        adxb->dec_nbyte = n * (adxb->nch << 1);
    }
    if (adxb->stat == 2) {
        adxb->addwr(adxb->addwrobj, adxb->dec_nbyte, adxb->dec_nsmpl);
        adxb->stat = 3;
    }
}

/* Unsigned 8-bit to signed 16-bit in the output window; stereo split. */
/* 0x001d5e00 */
static void adxb_exec_wave8(AdxbObj *adxb)
{
    AdxbDec *dec;
    unsigned char *src;
    short *dstl;
    int n;
    short *dstr;
    int i;

    dec = &adxb->dec;
    src = adxb->dec.buf;
    if (adxb->stat == 1 && adxd_get_stat(adxb->adxd) == 0) {
        adxb->getwr(adxb->getwrobj, &dec->wpos, &dec->nfree, &dec->nleft);
        dstl = (short *)dec->obuf + dec->wpos;
        n = dec->obsize - dec->wpos;
        if (dec->nfree < n)
            n = dec->nfree;
        if (dec->len < n)
            n = dec->len;
        if (adxb->nch == 2) {
            dstr = (short *)dec->obuf + (dec->obsize2 + dec->wpos);
            for (i = 0; i < n; i++) {
                dstl[i] = (src[i * 2] - 128) << 8;
                dstr[i] = (src[i * 2 + 1] - 128) << 8;
            }
        } else {
            for (i = 0; i < n; i++)
                dstl[i] = (src[i] - 128) << 8;
        }
        adxb->stat = 2;
        adxb->dec_nsmpl = n;
        adxb->dec_nbyte = n * adxb->nch;
    }
    if (adxb->stat == 2) {
        adxb->addwr(adxb->addwrobj, adxb->dec_nbyte, adxb->dec_nsmpl);
        adxb->stat = 3;
    }
}

/* Assembles 16-bit samples from byte pairs in the output window; stereo split. */
/* 0x001d5f88 */
static void adxb_exec_wave4(AdxbObj *adxb)
{
    AdxbDec *dec;
    unsigned char *src;
    short *dstl;
    int n;
    short *dstr;
    int i;

    dec = &adxb->dec;
    src = adxb->dec.buf;
    if (adxb->stat == 1 && adxd_get_stat(adxb->adxd) == 0) {
        adxb->getwr(adxb->getwrobj, &dec->wpos, &dec->nfree, &dec->nleft);
        dstl = (short *)dec->obuf + dec->wpos;
        n = dec->obsize - dec->wpos;
        if (dec->nfree < n)
            n = dec->nfree;
        if (dec->len < n)
            n = dec->len;
        if (adxb->nch == 2) {
            dstr = (short *)dec->obuf + (dec->obsize2 + dec->wpos);
            for (i = 0; i < n; i++) {
                dstl[i] = src[i * 4] | src[i * 4 + 2] << 8;
                dstr[i] = src[i * 4 + 1] | src[i * 4 + 3] << 8;
            }
        } else {
            for (i = 0; i < n; i++)
                dstl[i] = src[i * 2] | src[i * 2 + 1] << 8;
        }
        adxb->stat = 2;
        adxb->dec_nsmpl = n;
        adxb->dec_nbyte = n * (adxb->nch << 1);
    }
    if (adxb->stat == 2) {
        adxb->addwr(adxb->addwrobj, adxb->dec_nbyte, adxb->dec_nsmpl);
        adxb->stat = 3;
    }
}

/* 0x001d6120 */
int adxb_is_wave(void *hdr)
{
    if (memcmp(hdr, "RIFF", 4) == 0 && memcmp((char *)hdr + 8, "WAVE", 4) == 0)
        return 1;
    return 0;
}

/* 0x001d6180 */
void adxb_exec_wave(AdxbObj *adxb)
{
    if (adxb->pcm_fmt == 2)
        adxb_exec_wave4(adxb);
    else if (adxb->pcm_fmt == 1)
        adxb_exec_wave8(adxb);
    else if (adxb->pcm_fmt == 0)
        adxb_exec_wave16(adxb);
}
