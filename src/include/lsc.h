/* Stream play list. */

#ifndef REZ_LSC_H
#define REZ_LSC_H

#include "types_cri.h"

struct SjObj;

extern char adx_num_buf[];
extern LscFname lsc_fname_buf;
extern int lsc_init_count;
extern LscObj lsc_obj_tbl[8];
extern volatile int lsc_ver_str;
extern int rofs_cvfs_if_tbl[];
extern int rofs_cvfs_rec_tbl[];
extern int rofs_saved_vsync_cb;
extern volatile int sj_lock_count;

LscObj *lsc_alloc(void);
LscObj *LSC_Create(struct SjObj *sj);
void lsc_destroy(LscObj *lsc);
int lsc_entry_fname(LscObj *lsc, const char *fname);
int lsc_entry_file(LscObj *lsc, const char *fname, void *dir, int x, int nsct);
void lsc_start(LscObj *lsc);
void lsc_stop(LscObj *lsc);
void lsc_set_flag04(LscObj *lsc, int flag);
void lsc_exec_server(void);
int lsc_get_stat(LscObj *lsc);
int lsc_get_nstm(LscObj *lsc);
int lsc_get_stm_id(LscObj *lsc, int no);
char * lsc_get_fname(LscObj *lsc, int id);
int lsc_get_stm_stat(LscObj *lsc, int id);
int lsc_get_stm_word3c(LscObj *lsc, int id);
void lsc_set_min_fill(LscObj *lsc, int min);
int lsc_get_min_fill(LscObj *lsc);
void lsc_set_stm_cb(void (*fn)(void *obj, void *obj2), void *obj, void *obj2);
void lsc_call_stm_cb(void);
void lsc_set_flag(LscObj *lsc, int flag);
void lsc_lock(void *lk);
void lsc_unlock(void *lk);
void lsc_error(const char *fmt, ...);
void lsc_lib_init_hook(void);
void lsc_lib_finish_hook(void);
void lsc_lib_init(void);
void lsc_lib_finish(void);


#endif /* REZ_LSC_H */
