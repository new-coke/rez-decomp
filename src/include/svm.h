/* Server manager: servers by kind, lock and error callbacks. */

#ifndef REZ_SVM_H
#define REZ_SVM_H

#include "types_cri.h"

extern SvmCb svm_cb2_tbl[];
extern SvmCb svm_cb_tbl[][4];
extern char svm_error_buf[];
extern void *volatile svm_error_obj;
extern volatile int svm_exec_count[];
extern int svm_executing[];
extern int svm_init_count;
extern void *volatile svm_lock_obj;
extern char svm_num_buf[];
extern void *volatile svm_unlock_obj;

void svm_lock(void);
void svm_unlock(void);
void svm_error_printf(const char *fmt, ...);
void svm_error(const char *msg);
int SVM_SetCbSvr(int svtype, SvmFn fn, void *obj);
void SVM_DelCbSvr(int svtype, unsigned int id);
void SVM_SetCbSvrId(int svtype, unsigned int id, SvmFn fn, void *obj);
void svm_set_svr_border_cb(int n, SvmFn fn, void *obj);
void svm_set_lock_cb(void (*fn)(void *obj), void *obj);
void svm_set_unlock_cb(void (*fn)(void *obj), void *obj);
int svm_exec_vint(void);
int svm_exec_usr_vsync(void);
int svm_exec_vsync(void);
int svm_exec_main(void);
int svm_exec_mw_idle(void);
int svm_exec_usr_idle(void);
void svm_lib_init(void);
void svm_lib_finish(void);

extern void (*svm_lock_fn)(void *obj);
extern void (*svm_unlock_fn)(void *obj);
extern void (*svm_error_fn)(void *obj, char *msg);
extern void svm_set_error_cb(void (*fn)(void *obj, char *msg), void *obj);

#endif /* REZ_SVM_H */
