/* ADX basic decoder. */

#ifndef REZ_ADXB_H
#define REZ_ADXB_H

#include "types_cri.h"

extern short adxb_mulaw_tbl[256];
extern AdxbObj adxb_obj_tbl[];

void adxb_release_ahx(AdxbObj *adxb);
void adxb_set_max_dec_smpl(AdxbObj *adxb, int n);
void adxb_lib_init(void);
AdxbObj *adxb_create(void *obj, char *obuf, int obsize, int obsize2);
void adxb_destroy(AdxbObj *adxb);
int adxb_decode_header(AdxbObj *adxb, unsigned short *hdr, int len);
void adxb_set_get_wr_cb(AdxbObj *adxb, char *(*fn)(void *, int *, int *, int *), void *obj);
char *adxb_get_obuf(AdxbObj *adxb);
int adxb_get_format(AdxbObj *adxb);
int adxb_get_sfreq(AdxbObj *adxb);
int adxb_get_nch(AdxbObj *adxb);
int adxb_get_sample_bits(AdxbObj *adxb);
int adxb_get_out_bps(AdxbObj *adxb);
int adxb_get_blk_smpl(AdxbObj *adxb);
int adxb_get_blk_len(AdxbObj *adxb);
int adxb_get_total_smpl(AdxbObj *adxb);
int adxb_get_cutoff(AdxbObj *adxb);
int adxb_get_hdr_word20(AdxbObj *adxb);
int adxb_get_loop_flag(AdxbObj *adxb);
int adxb_get_loop_start_smpl(AdxbObj *adxb);
int adxb_get_loop_start_ofst(AdxbObj *adxb);
int adxb_get_loop_end_smpl(AdxbObj *adxb);
int adxb_get_loop_end_ofst(AdxbObj *adxb);
int adxb_get_stat(AdxbObj *adxb);
void adxb_set_input(AdxbObj *adxb, void *buf, int len);
void adxb_start(AdxbObj *adxb);
void adxb_stop(AdxbObj *adxb);
void adxb_reset_after_end(AdxbObj *adxb);
int adxb_get_dec_nbyte(AdxbObj *adxb);
int adxb_get_dec_nsmpl(AdxbObj *adxb);
void adxb_exec_handle(AdxbObj *adxb);

extern void (*adxb_ahx_release_fn)(void *ahx);
extern int (*adxb_ahx_exec_fn)(AdxbObj *b);

#endif /* REZ_ADXB_H */
