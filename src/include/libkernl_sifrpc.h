/* SIF remote procedure calls. */

#ifndef REZ_LIBKERNL_SIFRPC_H
#define REZ_LIBKERNL_SIFRPC_H

struct RpcPkt;
struct RpcData;
struct RpcServer;
struct RpcCallReq;
struct RpcQueue;

void sceSifExitRpc(void);
void libkernl_sif_end_cmd_handler(struct RpcPkt *req, struct RpcData *rd);
void libkernl_sif_rdata_cmd_handler(struct RpcPkt *req, struct RpcData *rd);
void libkernl_sif_bind_cmd_handler(struct RpcPkt *req, struct RpcData *rd);
int sceSifBindRpc(struct RpcPkt *cd, unsigned int rpc_number, unsigned int mode);
void libkernl_sif_call_cmd_handler(struct RpcCallReq *req, struct RpcData *rd);
int sceSifCallRpc(struct RpcPkt *cd, unsigned int fno, unsigned int mode, void *send, int ssize,
                 void *receive, int rsize, void (*end_func)(void *), void *end_param);
int sceSifCheckStatRpc(struct RpcPkt *cd);
void sceSifSetRpcQueue(struct RpcQueue *qd, int thread_id);
void sceSifRegisterRpc(struct RpcServer *sd, unsigned int sid, void *func, void *buff, void *cfunc,
                  void *cbuff, struct RpcQueue *qd);
void sceSifRpcLoop(struct RpcQueue *qd);

#endif /* REZ_LIBKERNL_SIFRPC_H */
