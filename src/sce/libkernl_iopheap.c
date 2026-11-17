/* IOP heap client: alloc, free and file load into IOP memory via the IOP heap server. */

#include "types_libc.h"
#include "types_sce.h"
#include "libkernl_iopheap.h"
#include "libkernl_sifrpc.h"

typedef struct HeapPathArg {
    int addr;               /* 0x00 */
    char name[252];         /* 0x04 */
} HeapPathArg;

extern int sif_iopheap_inited;
extern RpcPkt sif_iopheap_client;
extern int sif_iopheap_sbuf[];
extern int sif_iopheap_rbuf[];

extern HeapPathArg sif_iopheap_path_arg;

/* Busy-waits until the server answers. -1 if binding fails. */
/* 0x001fc980 */
int sceSifInitIopHeap(void)
{
    int i;

    for (;;) {
        if (sceSifBindRpc(&sif_iopheap_client, 0x80000003, 0) < 0)
            return -1;
        if (sif_iopheap_client.server) {
            sif_iopheap_inited = 0;
            break;
        }
        i = 0x100000;
        while (i--)
            ;
    }
    return 0;
}

/* IOP address, or 0. */
/* 0x001fca08 */
int sceSifAllocIopHeap(int size)
{
    if (sif_iopheap_inited < 0)
        return 0;
    sif_iopheap_sbuf[0] = size;
    if (sceSifCallRpc(&sif_iopheap_client, 1, 0, sif_iopheap_sbuf, 4, sif_iopheap_rbuf, 4, 0, 0) < 0)
        return 0;
    return sif_iopheap_rbuf[0];
}

/* IOP address, or 0. */
/* 0x001fca78 */
static int sceSifAllocSysMemory(int type, int size, int addr)
{
    if (sif_iopheap_inited < 0)
        return 0;
    sif_iopheap_sbuf[0] = size;
    sif_iopheap_sbuf[1] = type;
    sif_iopheap_sbuf[2] = addr;
    if (sceSifCallRpc(&sif_iopheap_client, 4, 0, sif_iopheap_sbuf, 12, sif_iopheap_rbuf, 4, 0, 0) < 0)
        return 0;
    return sif_iopheap_rbuf[0];
}

/* 0 if the client is not set up, -1 if the call fails. */
/* 0x001fcaf8 */
int sceSifFreeIopHeap(int addr)
{
    if (sif_iopheap_inited < 0)
        return 0;
    sif_iopheap_sbuf[0] = addr;
    if (sceSifCallRpc(&sif_iopheap_client, 2, 0, sif_iopheap_sbuf, 4, sif_iopheap_rbuf, 4, 0, 0) < 0)
        return -1;
    return sif_iopheap_rbuf[0];
}

/* 0x001fcb70 */
int sceSifFreeSysMemory(int addr)
{
    return sceSifFreeIopHeap(addr);
}

/* name cut to 251. 0 if the client is not set up, -1 if the call fails. */
/* 0x001fcb90 */
static int sceSifLoadIopHeap(const char *name, int addr)
{
    int i;

    if (sif_iopheap_inited < 0)
        return 0;
    for (i = 0; i < 252; i++) {
        if (!(sif_iopheap_path_arg.name[i] = name[i]))
            break;
    }
    if (i == 252) {
        i = 251;
        sif_iopheap_path_arg.name[251] = 0;
    }
    sif_iopheap_path_arg.addr = addr;
    sif_iopheap_path_arg.name[251] = 0;
    if (sceSifCallRpc(&sif_iopheap_client, 3, 0, &sif_iopheap_path_arg, i + 5, sif_iopheap_rbuf, 4, 0, 0) < 0)
        return -1;
    return sif_iopheap_rbuf[0];
}
