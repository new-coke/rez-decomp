/* ADX header parsing and ADPCM decoding: 18-byte blocks to 16-bit PCM, optional stereo mix-down. */

#include "types_cri.h"
#include "adxd.h"
#include "libc.h"
#include "adx_decode.h"

/* Prediction coefficients, 4.12 fixed point. */
/* 0x001d6218 */
void adx_calc_coef(int cutoff, int sfreq, short *coef1, short *coef2)
{
    double cos_ofs;
    double root2_less1;
    double z;
    float root2;

    root2 = sqrtf(2.0f);
    cos_ofs = root2 - cosf(cutoff * 6.28318530717958 / sfreq);
    root2_less1 = sqrtf(2.0f) - 1.0;
    z = (cos_ofs - sqrtf((cos_ofs + root2_less1) * (cos_ofs - root2_less1))) / root2_less1;
    *coef1 = (z + z) * 4096.0;
    *coef2 = -z * z * 4096.0;
}

/* Byte offset of the first 16-bit 0x80 in src[0..len); -1 and *out = 0 if none. */
/* 0x001d6410 */
int adxb_find_word80(short *src, int len, short *out)
{
    int n;
    int i;
    int k;

    n = 0x7fffffff;
    k = 0x80;
    for (i = 0; i < len - 1; i += 2) {
        if (*(short *)((char *)src + i) == k) {
            if (i < n)
                n = i;
            break;
        }
    }
    if (n != 0x7fffffff) {
        *out = n;
        return 0;
    }
    *out = 0;
    return -1;
}

/* Big-endian fixed header fields; returns 0, -1 (too short) or -2 (no 0x8000 signature). */
/* 0x001d6488 */
int adxb_parse_adx_header(void *hdr, int len, short *ofs, signed char *type, signed char *sample_bits,
                 signed char *blk, signed char *nch, int *sfreq, int *total_smpl, int *spb)
{
    unsigned char *bytes;

    bytes = hdr;
    if (len < 16)
        return -1;
    if ((bytes[1] | bytes[0] << 8) != 0x8000)
        return -2;
    *ofs = (bytes[2] << 8 | bytes[3]) + 4;
    *type = bytes[4];
    *blk = bytes[5];
    *sample_bits = bytes[6];
    *nch = bytes[7];
    *sfreq = bytes[8] << 24 | bytes[9] << 16 | bytes[10] << 8 | bytes[11];
    *total_smpl = bytes[12] << 24 | bytes[13] << 16 | bytes[14] << 8 | bytes[15];
    if (*sample_bits == 0)
        *spb = 0;
    else
        *spb = (*blk - 2) * 8 / *sample_bits;
    return 0;
}

/* Returns 0, -1 (too short) or -2 (no 0x8000 signature). */
/* 0x001d6590 */
int adxb_parse_cutoff(void *hdr, int len, short *cutoff)
{
    unsigned short *words;

    words = hdr;
    if (len < 18)
        return -1;
    if ((unsigned short)CRI_SW16(words[0]) != 0x8000)
        return -2;
    if ((short)CRI_SW16(words[1]) < 14)
        return -1;
    *cutoff = CRI_SW16(words[8]);
    return 0;
}

/* Returns 0, -1 (too short) or -2 (bad signature or no loop). */
/* 0x001d6610 */
int adxb_parse_loop_info(void *hdr, int len, int *align_smpl, short *loop_flag, short *loop_type, int *loop_start_smpl, int *loop_start_ofst,
                 int *loop_end_smpl, int *loop_end_ofst)
{
    unsigned short *words;
    int *longs;

    words = hdr;
    longs = hdr;
    *loop_flag = 0;
    if (len < 48)
        return -1;
    if ((unsigned short)CRI_SW16(words[0]) != 0x8000)
        return -2;
    if ((short)CRI_SW16(words[1]) < 44)
        return -1;
    *align_smpl = (short)CRI_SW16(words[10]);
    *loop_flag = CRI_SW16(words[11]);
    if (*loop_flag != 1)
        return -2;
    *loop_start_smpl = CRI_SW32(longs[7]);
    *loop_type = CRI_SW16(words[13]);
    *loop_start_ofst = CRI_SW32(longs[8]);
    *loop_end_smpl = CRI_SW32(longs[9]);
    *loop_end_ofst = CRI_SW32(longs[10]);
    return 0;
}

/* End block (0x8001) size with its 4-byte header; 0, -1 short, -2 not an end block. */
/* 0x001d67a0 */
int adxb_parse_end_block(void *hdr, int len, short *ofs)
{
    unsigned short *words;

    words = hdr;
    if (len < 16)
        return -1;
    if ((unsigned short)CRI_SW16(words[0]) != 0x8001)
        return -2;
    *ofs = CRI_SW16(words[1]) + 4;
    return 0;
}

/* 0x001d6800 */
void adxb_set_mono(int flag)
{
    adxd_mono_flag = flag;
}

/* Float decode; stops at a block whose scale has bit 15 set (end/header); hist carries over. */
/* 0x001d6808 */
int adxd_decode_mono(unsigned char *src, int nblk, short *dst, short *hist, int coef1_q12, int coef2_q12)
{
    float coef1;
    float coef2;
    float hist0;
    float hist1;
    float sc;
    float pred_hi;
    float pred_lo;
    float tv;
    int scale;
    int i;
    int j;
    unsigned char nibs;

    coef1 = (float)((short)coef1_q12 << 4) * (1.0f / 65536.0f);
    coef2 = (float)((short)coef2_q12 << 4) * (1.0f / 65536.0f);
    hist0 = hist[0] << 16;
    hist1 = hist[1] << 16;
    for (i = 0; i < nblk; i++) {
        scale = (short)CRI_SW16(*(unsigned short *)src);
        if (scale & 0x8000)
            return i;
        sc = (scale & 0x1fff) + 1;
        src += 2;
        for (j = 0; j < 32; j += 2) {
            nibs = *src++;
            tv = adxd_nibble_hi_tbl[nibs];
            pred_hi = coef1 * hist0 + coef2 * hist1;
            hist1 = hist0;
            hist0 = pred_hi + tv * sc;
            *dst++ = (int)hist0 >> 16;
            tv = adxd_nibble_lo_tbl[nibs];
            pred_lo = coef1 * hist0 + coef2 * hist1;
            hist1 = hist0;
            hist0 = pred_lo + tv * sc;
            *dst++ = (int)hist0 >> 16;
        }
    }
    hist[0] = (int)hist0 >> 16;
    hist[1] = (int)hist1 >> 16;
    return nblk;
}

/* Left/right block pairs; stops at a scale with bit 15 set; returns blocks decoded. */
/* 0x001d6978 */
static int adxd_decode_stereo_pair(unsigned char *src, int n, short *dstl, short *histl, short *dstr, short *histr,
                 int coef1_q12, int coef2_q12)
{
    int bl;
    int br;
    int out;
    float scl;
    float scr;
    float coef1;
    float coef2;
    float histl0;
    float histl1;
    float histr0;
    float histr1;
    float tl;
    float tr;
    int j;
    int i;
    unsigned char *pl;
    unsigned char *pr;
    short scale;
    float predl;
    float predr;

    histl0 = histl[0] * 65536;
    histl1 = histl[1] * 65536;
    histr0 = histr[0] * 65536;
    histr1 = histr[1] * 65536;
    coef1 = ((short)coef1_q12 * 16) / 65536.0f;
    coef2 = ((short)coef2_q12 * 16) / 65536.0f;
    pr = src;
    for (i = 0; i < n / 2; i++) {
        pl = pr;
        pr += 18;
        scale = CRI_SW16(*(unsigned short *)pl);
        if (scale & 0x8000)
            return i * 2;
        scl = (scale & 0x1fff) + 1;
        scale = CRI_SW16(*(unsigned short *)pr);
        if (scale & 0x8000)
            return i * 2;
        scr = (scale & 0x1fff) + 1;
        pl += 2;
        pr += 2;
        for (j = 0; j < 32; j += 2) {
            bl = *pl++;
            tl = adxd_nibble_hi_tbl[bl];
            br = *pr++;
            tr = adxd_nibble_hi_tbl[br];
            predl = coef1 * histl0 + coef2 * histl1 + tl * scl;
            histl1 = histl0;
            histl0 = predl;
            predr = coef1 * histr0 + coef2 * histr1 + tr * scr;
            histr1 = histr0;
            histr0 = predr;
            out = histl0;
            *dstl++ = out >> 16;
            out = histr0;
            *dstr++ = out >> 16;
            tl = adxd_nibble_lo_tbl[bl];
            tr = adxd_nibble_lo_tbl[br];
            predl = coef1 * histl0 + coef2 * histl1 + tl * scl;
            histl1 = histl0;
            histl0 = predl;
            predr = coef1 * histr0 + coef2 * histr1 + tr * scr;
            histr1 = histr0;
            histr0 = predr;
            out = histl0;
            *dstl++ = out >> 16;
            out = histr0;
            *dstr++ = out >> 16;
        }
    }
    histl[0] = (int)histl0 >> 16;
    histl[1] = (int)histl1 >> 16;
    histr[0] = (int)histr0 >> 16;
    histr[1] = (int)histr1 >> 16;
    return n;
}

/* As adxd_decode_stereo_pair, but writes 0.7 * (l + r) to both outputs. */
/* 0x001d6c20 */
static int adxd_decode_stereo_mix(unsigned char *src, int n, short *dstl, short *histl, short *dstr, short *histr,
                 int coef1_q12, int coef2_q12)
{
    float coef1;
    float coef2;
    float histl0;
    float histl1;
    float histr0;
    float histr1;
    float scl;
    float scr;
    float pred;
    float predl_hi;
    float predr_hi;
    float tl;
    float tr;
    float predl_lo;
    float predr_lo;
    int nblk;
    int scale;
    int sl;
    int sr;
    int i;
    int j;
    int mixed;
    unsigned char *pl;
    short *outl;
    unsigned char bl;
    unsigned char br;

    coef1 = (float)((short)coef1_q12 << 4) * (1.0f / 65536.0f);
    coef2 = (float)((short)coef2_q12 << 4) * (1.0f / 65536.0f);
    histl0 = histl[0] << 16;
    histl1 = histl[1] << 16;
    histr0 = histr[0] << 16;
    histr1 = histr[1] << 16;
    outl = dstl;
    for (i = 0; i < n / 2; i++) {
        pl = src;
        src += 18;
        scale = (short)CRI_SW16(*(unsigned short *)pl);
        if (scale & 0x8000)
            return i * 2;
        scl = (scale & 0x1fff) + 1;
        scale = (short)CRI_SW16(*(unsigned short *)src);
        if (scale & 0x8000)
            return i * 2;
        scr = (scale & 0x1fff) + 1;
        pl += 2;
        src += 2;
        for (j = 0; j < 32; j += 2) {
            bl = *pl++;
            br = *src++;
            tl = adxd_nibble_hi_tbl[bl];
            tr = adxd_nibble_hi_tbl[br];
            predl_hi = coef1 * histl0 + coef2 * histl1;
            predr_hi = coef1 * histr0 + coef2 * histr1;
            histl1 = histl0;
            histl0 = predl_hi + tl * scl;
            histr1 = histr0;
            histr0 = predr_hi + tr * scr;
            mixed = (int)((histl0 + histr0) * 0.7f) >> 16;
            *dstr++ = mixed;
            *outl++ = mixed;
            tl = adxd_nibble_lo_tbl[bl];
            tr = adxd_nibble_lo_tbl[br];
            predl_lo = coef1 * histl0 + coef2 * histl1;
            predr_lo = coef1 * histr0 + coef2 * histr1;
            histl1 = histl0;
            histl0 = predl_lo + tl * scl;
            histr1 = histr0;
            histr0 = predr_lo + tr * scr;
            mixed = (int)((histl0 + histr0) * 0.7f) >> 16;
            *dstr++ = mixed;
            *outl++ = mixed;
        }
    }
    histl[0] = (int)histl0 >> 16;
    histl[1] = (int)histl1 >> 16;
    histr[0] = (int)histr0 >> 16;
    histr[1] = (int)histr1 >> 16;
    return n;
}

/* Two channels, or mixed to mono when adxd_mono_flag. */
/* 0x001d6ec8 */
int adxd_decode_stereo(unsigned char *src, int n, short *dstl, short *histl, short *dstr, short *histr,
                 short coef1_q12, short coef2_q12)
{
    if (adxd_mono_flag == 0)
        return adxd_decode_stereo_pair(src, n, dstl, histl, dstr, histr, coef1_q12, coef2_q12);
    return adxd_decode_stereo_mix(src, n, dstl, histl, dstr, histr, coef1_q12, coef2_q12);
}
