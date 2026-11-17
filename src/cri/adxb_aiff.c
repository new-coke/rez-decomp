/* AIFF input for ADXB: COMM and SSND chunks, big-endian 16-bit or 8-bit samples. */

#include "types_cri.h"
#include "libc.h"
#include "adxb_aiff.h"
#include "adxd.h"

static inline int RD32(unsigned char *src)
{
    return src[0] | src[1] << 8 | src[2] << 16 | src[3] << 24;
}

static inline int RD16(unsigned char *src)
{
    return src[0] | src[1] << 8;
}

static inline int swap32(int x)
{
    int byte0;
    int bytes01;
    int bytes012;

    byte0 = x >> 24 & 0xff;
    bytes01 = byte0 | (x >> 8 & 0xff00);
    bytes012 = bytes01 | (x << 8 & 0xff0000);
    return bytes012 | x << 24;
}

/* Sample data, or 0. Rate from the top 16 mantissa bits of the 80-bit float only. */
/* 0x001d8dd8 */
static unsigned char *adxb_read_aiff_chunks(unsigned char *hdr, int *rate, int *nch, int *bits, int *nframes)
{
    unsigned char *p;
    unsigned char *end;
    unsigned char *data;
    int id;
    int size;
    int form;
    int comm;
    int ssnd;
    int exp_raw;
    int raw16;
    int raw32;
    unsigned int ssnd_ofs;
    unsigned short mt;
    unsigned short ex;

    p = hdr;
    id = RD32(p);
    p += 4;
    size = RD32(p);
    p += 4;
    size = swap32(size);
    form = RD32(p);
    comm = 0;
    ssnd = 0;
    data = 0;
    ssnd_ofs = 0; /* steer(dead) */
    p += 4;
    if (id != 0x4d524f46)
        return 0;
    if (form != 0x46464941)
        return 0;
    end = p + size - 4;
    while (p < end) {
        id = RD32(p);
        p += 4;
        size = RD32(p);
        p += 4;
        size = swap32(size);
        switch (id) {
        case 0x4d4d4f43:
            if (comm)
                continue;
            if (size < 18)
                return 0;
            raw16 = RD16(p);
            p += 2;
            comm = 1;
            *nch = CRI_SW16(raw16);
            raw32 = RD32(p);
            p += 4;
            *nframes = CRI_SW32(raw32);
            raw16 = RD16(p);
            p += 2;
            *bits = CRI_SW16(raw16);
            exp_raw = p[0];
            exp_raw |= p[1] << 8;
            p += 2;
            ex = CRI_SW16(exp_raw);

            raw16 = RD16(p);
            p += 8;
            mt = CRI_SW16(raw16);
            *rate = mt >> (16398 - (ex & 0xff));
            if (ssnd)
                return data;
            break;
        case 0x444e5353:
            if (ssnd)
                continue;
            ssnd = 1;
            ssnd_ofs = RD32(p);
            p += 4;
            ssnd_ofs = CRI_SW32(ssnd_ofs);
            data = p + ssnd_ofs;
            if (comm)
                return data;
            break;
        default:
            p += (size + 1) & ~1;
            break;
        }
    }
    return data;
}

/* "FORM" with form type "AIFF". */
/* 0x001d9150 */
int adxb_is_aiff(void *hdr)
{
    if (memcmp(hdr, "FORM", 4) == 0 && memcmp((char *)hdr + 8, "AIFF", 4) == 0)
        return 1;
    return 0;
}

/* Header of at least 4096 bytes; returns 0, or -1 when unusable. */
/* 0x001d91b0 */
static int adxb_parse_aiff_header(unsigned char *hdr, int len, short *ofs, signed char *type, signed char *sample_bits,
                 signed char *blk, signed char *nch, int *sfreq, int *total_smpl, int *spb)
{
    int rate;
    int ch;
    int bits;
    int frames;
    unsigned char *data;

    if (len < 4096) {
        *ofs = 0;
        return -1;
    }
    data = adxb_read_aiff_chunks(hdr, &rate, &ch, &bits, &frames);
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

/* Format 3, no loop; returns the data offset, or 0. */
/* 0x001d92b8 */
int adxb_decode_header_aiff(AdxbObj *adxb, void *hdr, int len)
{
    short ofs;

    adxb->hdrdec = 1;
    if (adxb_parse_aiff_header(hdr, len, &ofs, &adxb->type, &adxb->sample_bits, &adxb->blk, &adxb->nch, &adxb->total, &adxb->total_smpl,
                     &adxb->spb) < 0)
        return 0;
    adxb->dec.nch = adxb->nch;
    adxb->dec.blk = adxb->blk;
    adxb->dec.spb = adxb->spb;
    adxb->dec.obuf = adxb->obuf;
    adxb->dec.obsize = adxb->obsize;
    adxb->dec.obsize2 = adxb->obsize2;
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
    adxb->dec.state = &adxb->dec_state;
    adxb->wpos = 0;
    adxb->format = 3;
    adxb->wr_nsmpl = 0;
    if (adxb->sample_bits == 8)
        adxb->pcm_fmt = 1;
    else
        adxb->pcm_fmt = 0;
    memset(&adxb->dec_state, 0, 4);
    return ofs;
}

/* Byte-swaps big-endian samples into the output window; stereo split. */
/* 0x001d93d0 */
static void adxb_exec_aiff16(AdxbObj *adxb)
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
/* 0x001d9568 */
static void adxb_exec_aiff8(AdxbObj *adxb)
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

/* 0x001d96e0 */
void adxb_exec_aiff(AdxbObj *adxb)
{
    if (adxb->pcm_fmt == 1)
        adxb_exec_aiff8(adxb);
    else
        adxb_exec_aiff16(adxb);
}
