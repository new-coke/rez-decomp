/* Data transfer: sixteen handles moving data between stream joints by SIF DMA, 64-byte units. */

#include "types_cri.h"
#include "dtr.h"
#include "libc.h"
#include "cri_lock.h"
#include "dtx.h"
#include "kernel_syscalls.h"
#include "libkernl_cache.h"
#include "sjuni.h"

/* DMA done: consume input, add output; next DMA as large as both allow. Hangs if misaligned. */
/* 0x001d7d08 */
void DTR_ExecHndl(DtrObj *dtr)
{
    SjChunk cks;
    SjChunk cks_rest;
    SjChunk ckd;
    SjChunk ckd_rest;
    int len;

    if (dtr->stat != 1)
        return;
    if (dtr->busy == 1 && sceSifDmaStat(dtr->sema) < 0) {
        dtr->sjin->vtbl->putchunk(dtr->sjin, 0, &dtr->ckin);
        dtr->ckin.data = 0;
        dtr->ckin.len = 0;
        dtr->sjout->vtbl->putchunk(dtr->sjout, 1, &dtr->ckout);
        dtr->ckout.data = 0;
        dtr->ckout.len = 0;
        dtr->xfer_total += dtr->ckin.len;
        dtr->busy = 0;
        if (dtr->ckin.len != 0) {
            printf("DTR_ExecHndl: Internal Error\n");
            for (;;)
                ;
        }
    }
    if (dtr->busy != 0)
        return;
    dtr->sjin->vtbl->getchunk(dtr->sjin, 1, 0x7fffffff, &cks);
    dtr->sjout->vtbl->getchunk(dtr->sjout, 0, 0x7fffffff, &ckd);
    len = CRI_MIN(cks.len, ckd.len);
    len = len / dtr->unit * dtr->unit;
    sj_split_chunk(&cks, len, &cks, &cks_rest);
    dtr->sjin->vtbl->ungetchunk(dtr->sjin, 1, &cks_rest);
    sj_split_chunk(&ckd, len, &ckd, &ckd_rest);
    dtr->sjout->vtbl->ungetchunk(dtr->sjout, 0, &ckd_rest);
    if (len <= 0)
        return;
    if (cks.len & 0x3f) {
        printf("DTR_ExecHndl: Internal Error len\n");
        printf("cks.data=%08x, ckd.data=%08x, cks.len=%d\n", cks.data, ckd.data, cks.len);
        for (;;)
            ;
    }
    if ((int)cks.data & 0x3f) {
        printf("DTR_ExecHndl: Internal Error cks.data\n");
        printf("cks.data=%08x, ckd.data=%08x, cks.len=%d\n", cks.data, ckd.data, cks.len);
        for (;;)
            ;
    }
    if ((int)ckd.data & 0x3f) {
        printf("DTR_ExecHndl: Internal Error ckd.data\n");
        printf("cks.data=%08x, ckd.data=%08x, cks.len=%d\n", cks.data, ckd.data, cks.len);
        for (;;)
            ;
    }
    SyncDCache(cks.data, cks.data + (cks.len - 1));
    dtr->dma_data = (int)cks.data & 0x0fffffff;
    dtr->dma_addr = (int)ckd.data;
    dtr->dma_size = cks.len;
    dtr->dma_mode = 0;
    dtr->sema = sceSifSetDma(&dtr->dma_data, 1);
    if (dtr->sema == 0) {
        printf("E0101701 DTR_ExecHndl: can't use DMA\n");
        dtr->sjin->vtbl->ungetchunk(dtr->sjin, 1, &cks);
        dtr->sjout->vtbl->ungetchunk(dtr->sjout, 0, &ckd);
        return;
    }
    dtr->ckin = cks;
    dtr->ckout = ckd;
    dtr->busy = 1;
}

/* Every handle, then the DTX server. */
/* 0x001d8068 */
void dtr_exec_server(void)
{
    DtrObj *dtr;
    int i;

    dtr = dtr_obj_tbl;
    for (i = 0; i < 16; i++, dtr++) {
        if (dtr->used == 1)
            DTR_ExecHndl(dtr);
    }
    dtx_exec_server();
}

/* First call clears the handles; counts calls. */
/* 0x001d80c8 */
static void DTR_Init(void)
{
    printf("DTR_Init: in (%d)\n", dtr_init_count);
    if (dtr_init_count == 0)
        memset(dtr_obj_tbl, 0, sizeof(DtrObj) * 16);
    dtr_init_count++;
    printf("DTR_Init: out\n");
}

/* 0x001d8120 */
static void dtr_finish(void)
{
    dtr_init_count--;
}

/* Stopped, 64-byte units; 0 when all sixteen are in use. */
/* 0x001d8130 */
DtrObj *dtr_create(SjObj *sjin, SjObj *sjout)
{
    DtrObj *dtr;
    int i;

    sj_lock();
    dtr = dtr_obj_tbl;
    for (i = 0; i < 16; i++) {
        dtr = &dtr_obj_tbl[i];
        if (dtr->used == 0)
            break;
    }
    if (i == 16) {
        dtr = 0;
    } else {
        memset(dtr, 0, sizeof(DtrObj));
        dtr->stat = 0;
        dtr->unit = 64;
        dtr->sjin = sjin;
        dtr->busy = 0;
        dtr->sjout = sjout;
        dtr->unk_30 = 1;
        dtr->unk_34 = 0;
        dtr->used = 1;
    }
    sj_unlock();
    return dtr;
}

/* 0x001d8200 */
void dtr_destroy(DtrObj *dtr)
{
    sj_lock();
    dtr->used = 0;
    sj_unlock();
}

/* Byte count cleared. */
/* 0x001d8228 */
void dtr_start(DtrObj *dtr)
{
    dtr->xfer_total = 0;
    dtr->stat = 1;
}

/* Returns held chunks to their joints. */
/* 0x001d8238 */
void dtr_stop(DtrObj *dtr)
{
    dtr->stat = 0;
    if (dtr->ckin.data != 0) {
        dtr->sjin->vtbl->ungetchunk(dtr->sjin, 1, &dtr->ckin);
        dtr->ckin.data = 0;
        dtr->ckin.len = 0;
    }
    if (dtr->ckout.data != 0) {
        dtr->sjout->vtbl->ungetchunk(dtr->sjout, 0, &dtr->ckout);
        dtr->ckout.data = 0;
        dtr->ckout.len = 0;
    }
}
