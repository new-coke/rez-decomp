/* Stream joint links: 16, carrying EE joint chunks to IOP joints and back over DTX exchange 0. */

#include "types_cri.h"
#include "sjx.h"
#include "libc.h"
#include "cri_lock.h"
#include "dtx.h"
#include "libkernl_iopheap.h"

/* Each returned record's chunk (current generation) to its link's EE joint; hangs on null buf. */
/* 0x001d3588 */
static void sjx_dtx_recv_cb(void *obj, int *buf)
{
    SjxRec *rec;
    int n;
    int i;

    if (buf == 0) {
        for (;;)
            ;
    }
    n = buf[0];
    sj_lock();
    for (i = 0; i < n; i++) {
        rec = &((SjxRec *)(buf + 4))[i];
        if (rec->flag == 0 && rec->uid == rec->sjx->uid)
            rec->sjx->sj->vtbl->putchunk(rec->sjx->sj, rec->id, &rec->ck);
    }
    sj_unlock();
}

/* Every available EE chunk into records (at most 128); count at the buffer start. */
/* 0x001d3628 */
static void sjx_dtx_send_cb(void *obj, int *buf)
{
    SjxRec *recs;
    SjxObj *sjx;
    SjChunk ck;
    int n;
    int i;

    n = 0;
    recs = (SjxRec *)(buf + 4);
    sj_lock();
    for (i = 0; i < 16; i++) {
        sjx = &sjx_obj_tbl[i];
        if (sjx->used == 1) {
            for (;;) {
                if (n == 128)
                    goto end;
                sjx->sj->vtbl->getchunk(sjx->sj, sjx->id, 0x7fffffff, &ck);
                if (ck.len == 0)
                    break;
                recs[n].flag = 0;
                recs[n].sjx = (SjxObj *)sjx->iopid;
                recs[n].id = sjx->id;
                recs[n].ck = ck;
                recs[n].uid = sjx->uid;
                n++;
            }
        }
    }
end:
    sj_unlock();
    *buf = n;
}

/* 0x001d3738 */
void sjx_exec_server(void)
{
    dtx_exec_server();
}

/* First call: DTX, exchange 0 with EE/IOP buffers and callbacks; hangs on failure. Counts. */
/* 0x001d3740 */
void SJX_Init(void)
{
    if (sjx_init_count == 0) {
        memset(sjx_dtx_buf, 0, 2256);
        DTX_Init();
        sjx_dtx_bsize = 2176;
        sjx_dtx_sbuf = (char *)((unsigned int)(sjx_dtx_buf + 64) & 0xffffffc0);
        if (sjx_iop_buf == 0) {
            sjx_iop_buf = sceSifAllocIopHeap(2256);
            if (sjx_iop_buf == 0) {
                printf("E0100301: SJX_Init can't allocate IOP Heap\n");
                for (;;)
                    ;
            }
        }
        sjx_iop_buf_aligned = (char *)((unsigned int)(sjx_iop_buf + 64) & 0xffffffc0);
        sjx_dtx = DTX_Create(0, sjx_dtx_sbuf, sjx_iop_buf_aligned, sjx_dtx_bsize);
        if (sjx_dtx == 0) {
            printf("E0100302: SJX_Init can't create DTX\n");
            for (;;)
                ;
        }
        dtx_set_send_cb(sjx_dtx, sjx_dtx_recv_cb, 0);
        dtx_set_recv_cb(sjx_dtx, sjx_dtx_send_cb, 0);
        dtx_call_iop(3, 0, 0, 0, 0);
    }
    sjx_init_count++;
}

/* Last user: destroys exchange 0, frees its IOP memory, shuts DTX down. */
/* 0x001d3890 */
void sjx_lib_finish(void)
{
    if (--sjx_init_count == 0) {
        dtx_destroy(sjx_dtx);
        sjx_dtx = 0;
        if (sjx_iop_buf != 0) {
            sceSifFreeSysMemory(sjx_iop_buf);
            sjx_iop_buf = 0;
        }
        DTX_Finish();
    }
}

/* Plus its IOP end; 0 on failure. */
/* 0x001d38e0 */
void *sjx_create(SjObj *sj, void *sjrmt, int id)
{
    SjxObj *sjx;
    int i;
    int arg[4];

    for (i = 0, sjx = sjx_obj_tbl; i < 16; i++) {
        sjx = &sjx_obj_tbl[i];
        if (sjx->used == 0)
            break;
    }
    if (i == 16)
        return 0;
    memset(sjx, 0, 20);
    sjx->id = id;
    sjx->sj = sj;
    sjx->sjrmt = sjrmt;
    arg[0] = (int)sj;
    arg[1] = (int)sjrmt;
    arg[2] = id;
    arg[3] = (int)sjx;
    sjx->iopid = dtx_call_iop(0, arg, 4, arg, 1);
    if (sjx->iopid == 0) {
        printf("E0100401: can't create SJX of IOP\n");
        return 0;
    }
    sjx->used = 1;
    return sjx;
}

/* And its IOP end. */
/* 0x001d39d8 */
void sjx_destroy(void *obj)
{
    SjxObj *sjx;
    int arg[4];

    sjx = obj;
    arg[0] = sjx->iopid;
    dtx_call_iop(1, arg, 1, 0, 0);
    sjx->used = 0;
}

/* Bumps the generation (drops records in flight) and tells the IOP end. */
/* 0x001d3a20 */
void sjx_reset(SjxObj *sjx)
{
    int arg[4];

    arg[0] = sjx->iopid;
    sjx->uid++;
    arg[1] = sjx->uid;
    dtx_call_iop(2, arg, 2, 0, 0);
}
