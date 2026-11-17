/* ADX file streams into stream joints. */

#ifndef REZ_ADXSTM_H
#define REZ_ADXSTM_H

#include "cri.h"

struct SjObj;

int adxstm_lib_init(void);
void adxstm_lib_finish(void);
AdxstmObj *ADXSTM_OpenFnameRange(const char *fname, void *dir, int ofst, int nsct, int x);
AdxstmObj *ADXSTM_OpenFnameRangeExRt(const char *fname, void *dir, int ofst, int nsct, int x);
AdxstmObj *ADXSTM_OpenFnameEx(const char *fname, void *dir, int x);
AdxstmObj *adxstm_open_fname_rt(const char *fname, int x);
void adxstm_close(AdxstmObj *stm);
int adxstm_get_stat(AdxstmObj *stm);
int adxstm_seek(AdxstmObj *stm, int sct);
int adxstm_tell(AdxstmObj *stm);
int adxstm_start(AdxstmObj *stm);
void adxstm_stop(AdxstmObj *stm);
void adxstm_set_end_cb(AdxstmObj *stm, int fn, int obj);
void adxstm_set_size_sct(AdxstmObj *stm, int nsct);
void adxstm_exec_server(void);
int adxstm_update_nread(AdxstmObj *stm, int *n);
int adxstm_set_thresholds(AdxstmObj *stm, int refill_level, int read_size);
int adxstm_set_max_sct(AdxstmObj *stm, int nsct);
int adxstm_get_fsize_byte(AdxstmObj *stm);
void adxstm_set_sj(AdxstmObj *stm, struct SjObj *sj);
void adxstm_set_size_from_sct(AdxstmObj *stm, int nsct);
void adxstm_set_ofst(AdxstmObj *stm, int ofst);
void adxstm_set_retry_limit(int n);
void adxstm_error_callback(void *obj, const char *msg);

#endif /* REZ_ADXSTM_H */
