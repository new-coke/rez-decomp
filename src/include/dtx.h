/* IOP data exchange over RPC. */

#ifndef REZ_DTX_H
#define REZ_DTX_H

#include "types_cri.h"

extern DtxObj dtx_obj_tbl[8];
extern int dtx_send_sw;

int dtx_rpc_create(unsigned int id, char *eewk, char *iopwk, int wklen);
DtxObj *DTX_Create(unsigned int id, char *eewk, char *iopwk, int wklen);
void dtx_destroy(DtxObj *dtx);
void dtx_set_send_cb(DtxObj *dtx, void (*fn)(), void *obj);
void dtx_set_recv_cb(DtxObj *dtx, void (*fn)(), void *obj);
void DTX_Init(void);
void DTX_Finish(void);
int dtx_call_iop(int fno, int *in, int nin, int *out, int nout);
void dtx_exec_server(void);

#endif /* REZ_DTX_H */
