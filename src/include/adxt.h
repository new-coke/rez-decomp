/* ADX playback. */

#ifndef REZ_ADXT_H
#define REZ_ADXT_H

#include "types_cri.h"

struct SjObj;

void adxt_debug_print(AdxtObj *adxt);
AdxtObj *adxt_create(int maxnch, void *work, int worksize);
void adxt_destroy(AdxtObj *adxt);
void adxt_destroy_all(void);
void adxt_destroy_all_2(void);
void adxt_start_decode(AdxtObj *adxt, struct SjObj *sj);
void adxt_start_stm(AdxtObj *adxt);
void adxt_start_sj(AdxtObj *adxt, struct SjObj *sj);
void adxt_stop_unlocked(AdxtObj *adxt);
void adxt_stop(AdxtObj *adxt);
int adxt_get_stat(AdxtObj *adxt);
void adxt_set_vsync_mode(int mode);
void adxt_get_time_raw(AdxtObj *adxt, int *ncount, int *tscale);
void adxt_get_time_dec(AdxtObj *adxt, int *ncount, int *tscale);
void adxt_get_time_vsync(AdxtObj *adxt, int *ncount, int *tscale);
int adxt_get_time_csec(AdxtObj *adxt);
int adxt_get_total_smpl(AdxtObj *adxt);
int adxt_get_sfreq(AdxtObj *adxt);
int adxt_get_nch(AdxtObj *adxt);
int adxt_get_hdr_size(AdxtObj *adxt);
int adxt_get_sample_bits(AdxtObj *adxt);
void ADXT_SetOutPan(AdxtObj *adxt, int ch, int pan);
int adxt_get_out_pan(AdxtObj *adxt, int ch);
void adxt_set_out_vol(AdxtObj *adxt, int vol);
int adxt_get_out_vol(AdxtObj *adxt);
void adxt_set_svr_freq(AdxtObj *adxt, int x);
void adxt_set_reload_time(AdxtObj *adxt, float sec, int nch, int sfreq);
void adxt_set_reload_sct(AdxtObj *adxt, int nsct);
int adxt_get_ibuf_sct(AdxtObj *adxt);
int adxt_get_obuf_smpl(AdxtObj *adxt, int ch);
float adxt_get_ibuf_time(AdxtObj *adxt);
int adxt_is_ibuf_full(AdxtObj *adxt);
void adxt_set_recover_mode(AdxtObj *adxt, int x);
int adxt_is_play_end(AdxtObj *adxt);
void adxt_exec_server(void);
int adxt_get_error(AdxtObj *adxt);
void adxt_clear_error(AdxtObj *adxt);
int adxt_get_loop_count(AdxtObj *adxt);
void adxt_set_loop_flag(AdxtObj *adxt, int x);
struct SjObj *adxt_get_input_sj(AdxtObj *adxt);
void adxt_set_start_hold(AdxtObj *adxt, int x);
int adxt_is_ready(AdxtObj *adxt);
void adxt_pause(AdxtObj *adxt, int sw);
void adxt_nop(void);
int adxt_return_zero(void);
void adxt_nop_2(void);
void adxt_nop_3(void);
void adxt_nop_4(void);
void adxt_nop_5(void);
void adxt_nop_6(void);
void adxt_nop_7(void);
void adxt_set_error_cb(void);
int adxt_return_16384(void);
int adxt_is_adx_header(unsigned char *hdr, int len, int *hdrlen);
int adxt_is_end_block(unsigned char *hdr, int len, int *n);
void adxt_set_output_mono(void);
void adxt_call_destroy_cb(void);

#endif /* REZ_ADXT_H */
