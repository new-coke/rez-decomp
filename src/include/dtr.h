/* Stream joint transfer by SIF DMA. */

#ifndef REZ_DTR_H
#define REZ_DTR_H

#include "types_cri.h"

struct SjObj;

extern int dtr_init_count;
extern DtrObj dtr_obj_tbl[];

void DTR_ExecHndl(DtrObj *dtr);
void dtr_exec_server(void);
DtrObj *dtr_create(struct SjObj *sjin, struct SjObj *sjout);
void dtr_destroy(DtrObj *dtr);
void dtr_start(DtrObj *dtr);
void dtr_stop(DtrObj *dtr);

#endif /* REZ_DTR_H */
