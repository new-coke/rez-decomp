/* ADX thread manager. */

#ifndef REZ_ADXM_H
#define REZ_ADXM_H

void adxm_thread_init(int *prm);
int adxm_call_int_handler(int cause);
void adxm_run_svr_threads(void);
int adxt_svm_server(void);

#endif /* REZ_ADXM_H */
