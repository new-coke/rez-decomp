/* EE-IOP stream joint links. */

#ifndef REZ_SJX_H
#define REZ_SJX_H

#include "types_cri.h"

struct SjObj;

extern void *sjx_dtx;
extern int sjx_dtx_bsize;
extern char sjx_dtx_buf[];
extern char *sjx_dtx_sbuf;
extern int sjx_init_count;
extern char *sjx_iop_buf;
extern char *sjx_iop_buf_aligned;
extern SjxObj sjx_obj_tbl[];

void sjx_exec_server(void);
void SJX_Init(void);
void sjx_lib_finish(void);
void *sjx_create(struct SjObj *sj, void *sjrmt, int id);
void sjx_destroy(void *obj);
void sjx_reset(SjxObj *sjx);

#endif /* REZ_SJX_H */
