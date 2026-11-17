/* Low level ADX decoder. */

#ifndef REZ_ADXD_H
#define REZ_ADXD_H
#include "types_cri.h"

extern int adxd_mono_flag;
extern float adxd_nibble_hi_tbl[256];
extern float adxd_nibble_lo_tbl[256];
extern AdxdObj adxd_obj_tbl[];
extern int adxd_odd_flag;

void adxd_lib_init(void);
void *adxd_create(void);
void adxd_set_coef(void *obj, int sfreq, int cutoff);
void adxd_destroy(void *obj);
int adxd_get_stat(void *obj);
int adxd_setup_mono(void *obj, void *src, int n, short *dst);
int adxd_setup_stereo(void *obj, void *src, int n, short *dstl, short *dstr);
void adxd_start(void *obj);
void adxd_stop(void *obj);
void adxd_reset_after_end(void *obj);
int adxd_get_num_blk(void *obj);
void adxd_exec_handle(void *obj);

#endif /* REZ_ADXD_H */
