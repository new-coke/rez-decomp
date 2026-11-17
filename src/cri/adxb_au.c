/* Sun AU input for ADXB: 16-bit, 8-bit or mu-law samples. */

#include "types_cri.h"
#include "adxb.h"
#include "libc.h"
#include "adxb_au.h"
#include "adxd.h"

static inline int RD32(unsigned char *src)
{
    return src[0] | src[1] << 8 | src[2] << 16 | src[3] << 24;
}

/*
 * ".snd" or ".sd": offset (inside len), size, encoding (1 mu-law, 2 8-bit, 3 16-bit; others
 * refused), rate, channels. Returns the data pointer, or 0.
 */
/* 0x001d9700 */
static unsigned char *adxb_read_au_header(unsigned char *hdr, int len, int *rate, int *nch, int *bits,
                            int *frames, int *fmt)
{
    unsigned char *p;
    int magic;
    unsigned int ofs;
    unsigned int size;
    unsigned int enc;
    int x;

    p = hdr;
    magic = RD32(p);
    p += 4;
    if (magic != 0x0064732e && magic != 0x646e732e)
        return 0;
    ofs = RD32(p);
    p += 4;
    ofs = CRI_SW32(ofs);
    if (len < (int)ofs)
        return 0;
    size = RD32(p);
    p += 4;
    enc = RD32(p);
    p += 4;
    size = CRI_SW32(size);
    enc = CRI_SW32(enc);
    switch (enc) {
    case 1:
        *fmt = 2;
        *bits = 8;
        break;
    case 2:
        *fmt = 1;
        *bits = 8;
        break;
    case 3:
        *fmt = 0;
        *bits = 16;
        break;
    default:
        return 0;
    }
    x = RD32(p);
    p += 4;
    *rate = CRI_SW32(x);
    x = RD32(p);
    *nch = CRI_SW32(x);
    if (*fmt == 2)
        *frames = size / *nch;
    else if (*fmt == 1)
        *frames = size / *nch;
    else if (*fmt == 0)
        *frames = (size >> 1) / *nch;
    else
        *frames = 0x7fff0000;
    return hdr + ofs;
}

/* ".snd" or ".sd". */
/* 0x001d9a10 */
int adxb_is_au(void *hdr)
{
    if (memcmp(hdr, ".snd", 4) == 0 || memcmp(hdr, ".sd", 4) == 0)
        return 1;
    return 0;
}

/* Header of at least 8 bytes; returns 0, or -1 when unusable. */
/* 0x001d9a68 */
static int adxb_parse_au_header(unsigned char *hdr, int len, short *ofs, signed char *type, signed char *sample_bits,
                 signed char *blk, signed char *nch, int *sfreq, int *total_smpl, int *spb, int *pcm_fmt)
{
    int rate;
    int ch;
    int bits;
    int frames;
    unsigned char *data;

    if (len < 8) {
        *ofs = 0;
        return -1;
    }
    data = adxb_read_au_header(hdr, len, &rate, &ch, &bits, &frames, pcm_fmt);
    if (data == 0)
        return -1;
    *ofs = data - hdr;
    if (*ofs <= 0)
        return -1;
    *sfreq = rate;
    *nch = ch;
    *sample_bits = bits;
    *total_smpl = frames;
    *type = -1;
    *blk = *nch * *sample_bits / 8;
    *spb = 1;
    return 0;
}

/* Format 4, no loop; returns the data offset, or 0. */
/* 0x001d9b78 */
int adxb_decode_header_au(AdxbObj *adxb, void *hdr, int len)
{
    short ofs;
    int pcm_fmt;

    adxb->hdrdec = 1;
    if (adxb_parse_au_header(hdr, len, &ofs, &adxb->type, &adxb->sample_bits, &adxb->blk, &adxb->nch, &adxb->total, &adxb->total_smpl,
                     &adxb->spb, &pcm_fmt) < 0)
        return 0;
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
    adxb->format = 4;
    adxb->dec.state = &adxb->dec_state;
    adxb->pcm_fmt = pcm_fmt;
    memset(&adxb->dec_state, 0, 4);
    return ofs;
}

/* Byte-swaps big-endian samples into the output window; stereo split. */
/* 0x001d9c80 */
static void adxb_exec_au16(AdxbObj *adxb)
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
                dstl[i] = (src[i * 2] >> 8 & 0xff) | src[i * 2] << 8;
                dstr[i] = (src[i * 2 + 1] >> 8 & 0xff) | src[i * 2 + 1] << 8;
            }
        } else {
            for (i = 0; i < n; i++)
                dstl[i] = (src[i] >> 8 & 0xff) | src[i] << 8;
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

/* Widens to 16 bits into the output window; stereo split. */
/* 0x001d9e18 */
static void adxb_exec_au8(AdxbObj *adxb)
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
                dstl[i] = src[i * 2] << 8;
                dstr[i] = src[i * 2 + 1] << 8;
            }
        } else {
            for (i = 0; i < n; i++)
                dstl[i] = src[i] << 8;
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

/* Expands through adxb_mulaw_tbl into the output window; stereo split. */
/* 0x001d9f90 */
static void adxb_exec_au_mulaw(AdxbObj *adxb)
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
                dstl[i] = adxb_mulaw_tbl[src[i * 2]];
                dstr[i] = adxb_mulaw_tbl[src[i * 2 + 1]];
            }
        } else {
            for (i = 0; i < n; i++)
                dstl[i] = adxb_mulaw_tbl[src[i]];
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

/* 0x001da130 */
void adxb_exec_au(AdxbObj *adxb)
{
    if (adxb->pcm_fmt == 2)
        adxb_exec_au_mulaw(adxb);
    else if (adxb->pcm_fmt == 1)
        adxb_exec_au8(adxb);
    else
        adxb_exec_au16(adxb);
}
