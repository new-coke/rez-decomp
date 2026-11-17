/* ADX stream joint decoder. */

#ifndef REZ_ADXSJD_H
#define REZ_ADXSJD_H

#include "cri.h"

struct SjObj;

void adxsjd_lib_init(void);
void adxsjd_lib_finish(void);
AdxsjdObj *adxsjd_create(struct SjObj *sji, int nch, struct SjObj **sjo);
void adxsjd_destroy(AdxsjdObj *sjd);
int adxsjd_get_stat(AdxsjdObj *sjd);
void adxsjd_set_input(AdxsjdObj *sjd, struct SjObj *sj);
void adxsjd_set_max_dec_smpl(AdxsjdObj *sjd, int n);
void adxsjd_start(AdxsjdObj *sjd);
void adxsjd_stop(AdxsjdObj *sjd);
void adxsjd_exec_handle(AdxsjdObj *sjd);
void adxsjd_exec_server(void);
int adxsjd_get_decoded_smpl(AdxsjdObj *sjd);
void adxsjd_set_smpl_pos(AdxsjdObj *sjd, int x);
void adxsjd_set_filter_cb(AdxsjdObj *sjd, int fn, int obj);
void adxsjd_set_limit_cb(AdxsjdObj *sjd, int fn, int obj);
void adxsjd_set_smpl_limit(AdxsjdObj *sjd, int x);
void adxsjd_set_limit_count(AdxsjdObj *sjd, int x);
void adxsjd_set_byte_count(AdxsjdObj *sjd, int x);
int adxsjd_get_format(AdxsjdObj *sjd);
int adxsjd_get_sfreq(AdxsjdObj *sjd);
int adxsjd_get_nch(AdxsjdObj *sjd);
int adxsjd_get_out_bps(AdxsjdObj *sjd);
int adxsjd_get_blk_smpl(AdxsjdObj *sjd);
int adxsjd_get_total_smpl(AdxsjdObj *sjd);
int adxsjd_get_loop_flag(AdxsjdObj *sjd);
int adxsjd_get_loop_start_smpl(AdxsjdObj *sjd);
int adxsjd_get_loop_start_ofst(AdxsjdObj *sjd);
int adxsjd_get_loop_end_smpl(AdxsjdObj *sjd);
int adxsjd_get_loop_end_ofst(AdxsjdObj *sjd);
int adxsjd_get_hdr_size(AdxsjdObj *sjd);
int adxsjd_get_sample_bits(AdxsjdObj *sjd);
unsigned char *adxsjd_get_hdr_copy(AdxsjdObj *sjd);

#endif /* REZ_ADXSJD_H */
