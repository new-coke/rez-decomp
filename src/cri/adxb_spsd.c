/* SPSD input for ADXB: 16-bit PCM. */

#include "types_cri.h"
#include "libc.h"
#include "adxb_spsd.h"
#include "adxd.h"

/* Returns 0. Bug: block size, count, bits forced to 16-bit values; only 16-bit data plays. */
/* 0x001d5568 */
static int adxb_parse_spsd_header(unsigned char *hdr, int len, short *ofs, signed char *type, signed char *sample_bits,
                 signed char *blk, signed char *nch, int *sfreq, int *total_smpl, int *spb, short *pcm_fmt)
{
    *ofs = hdr[7] << 4;
    *nch = (hdr[9] & 3) + 1;
    *sfreq = *(unsigned short *)(hdr + 42);
    switch (hdr[8]) {
    case 0:
        *sample_bits = 16;
        *blk = *nch * 2;
        *spb = 1;
        *pcm_fmt = 0;
        *total_smpl = *(int *)(hdr + 12) / 2;
        break;
    case 1:
        *sample_bits = 8;
        *blk = *nch;
        *spb = 1;
        *pcm_fmt = 1;
        *total_smpl = *(int *)(hdr + 12);
        break;
    case 2:
    case 3:
        *sample_bits = 4;
        *blk = *nch;
        *spb = 2;
        *pcm_fmt = 2;
        *total_smpl = *(int *)(hdr + 12) * 2;
        break;
    }
    *blk = 2;
    *spb = 1;
    *total_smpl = *(int *)(hdr + 12) / 2;
    *sample_bits = 16;
    *type = -1;
    return 0;
}

/* Format 2, no loop; returns the data offset, or 0. */
/* 0x001d5678 */
int adxb_decode_header_spsd(AdxbObj *adxb, void *hdr, int len)
{
    short ofs;

    adxb->hdrdec = 1;
    if (adxb_parse_spsd_header(hdr, len, &ofs, &adxb->type, &adxb->sample_bits, &adxb->blk, &adxb->nch, &adxb->total, &adxb->total_smpl,
                     &adxb->spb, &adxb->pcm_fmt) < 0)
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
    adxb->format = 2;
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
/* 0x001d5778 */
void adxb_exec_spsd(AdxbObj *adxb)
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

/* 0x001d58f0 */
int adxb_is_spsd(void *hdr)
{
    return memcmp(hdr, "SPSD", 4) == 0;
}
