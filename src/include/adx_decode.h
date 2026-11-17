/* ADX header parsing, ADPCM block decoding. */

#ifndef REZ_ADX_DECODE_H
#define REZ_ADX_DECODE_H

void adx_calc_coef(int cutoff, int sfreq, short *coef1, short *coef2);
int adxb_find_word80(short *src, int len, short *out);
int adxb_parse_adx_header(void *hdr, int len, short *ofs, signed char *type, signed char *sample_bits,
                 signed char *blk, signed char *nch, int *sfreq, int *total_smpl, int *spb);
int adxb_parse_cutoff(void *hdr, int len, short *cutoff);
int adxb_parse_loop_info(void *hdr, int len, int *align_smpl, short *loop_flag, short *loop_type, int *loop_start_smpl, int *loop_start_ofst,
                 int *loop_end_smpl, int *loop_end_ofst);
int adxb_parse_end_block(void *hdr, int len, short *ofs);
void adxb_set_mono(int flag);
int adxd_decode_mono(unsigned char *src, int nblk, short *dst, short *hist, int coef1_q12, int coef2_q12);
int adxd_decode_stereo(unsigned char *src, int n, short *dstl, short *histl, short *dstr, short *histr,
                 short coef1_q12, short coef2_q12);

#endif /* REZ_ADX_DECODE_H */
