/* SIF command layer; RPC layer start-up. */

#ifndef REZ_LIBKERNL_SIFCMD_H
#define REZ_LIBKERNL_SIFCMD_H
#include "types_libc.h"
#include "types_sce.h"

struct CmdData;
struct CmdArg;
struct CmdHandler;
struct CmdPkt;

extern RpcData libkernl_sif_rpc_data;
extern int libkernl_sif_rpc_inited;

void sceSifInitCmd(void);
void sceSifExitCmd(void);
void sceSifAddCmdHandler(int cid, void (*func)(void *, void *), void *cb_arg);
void sceSifRemoveCmdHandler(int cid);
unsigned int sceSifSendCmd(int cid, struct CmdPkt *pkt, int pktsize, void *src, void *dest, int size);
unsigned int isceSifSendCmd(int cid, struct CmdPkt *pkt, int pktsize, void *src, void *dest, int size);
void sceSifInitRpc(void);
int libkernl_sif_cmd_dma_handler(int ch);
void sceSifWriteBackDCache(void *addr, int size);

#endif /* REZ_LIBKERNL_SIFCMD_H */
