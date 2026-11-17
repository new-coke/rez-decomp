/* RPC client of the IOP vibration service. */

#ifndef REZ_UL_IOP_RPC_H
#define REZ_UL_IOP_RPC_H

struct IopRpcWord;

void iop_rpc_init(int mode);
int iop_rpc_set_act(int pad, struct IopRpcWord *val);
int iop_rpc_get_info(int recv);
int iop_rpc_send_word(int val, int recv);
int iop_rpc_status(int op, int *out);

#endif /* REZ_UL_IOP_RPC_H */
