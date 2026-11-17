/*
 * PS2 sound output: eight handles playing ADXT's 16-bit PCM. Per channel: DMA from the decoder
 * joint to an EE joint, linked to an IOP buffer played at 48 kHz; settings go to the IOP over DTX.
 */

#include "types_cri.h"
#include "ps2rna.h"
#include "libc.h"
#include "cri_lock.h"
#include "dtr.h"
#include "dtx.h"
#include "kernel_syscalls.h"
#include "libkernl_iopheap.h"
#include "sjrmt.h"
#include "sjuni.h"
#include "sjx.h"

/* IOP work area once; per channel an IOP joint, a 16 KiB EE joint and a link. Hangs on failure. */
/* 0x001d0b48 */
static void ps2rna_init_psj(void)
{
    int i;
    char *wk;
    PsjObj *psj;

    if (ps2rna_psj_work == 0) {
        ps2rna_psj_work = sceSifAllocIopHeap(ps2rna_psj_wksize + 64);
        ps2rna_psj_work_aligned = (char *)((unsigned int)(ps2rna_psj_work + 64) & 0xffffffc0);
        ps2rna_psj_work_own = 1;
    }
    memset(ps2rna_psj_tbl, 0, 192);
    wk = ps2rna_psj_work_aligned;
    for (i = 0; i < ps2rna_npsj; i++) {
        psj = &ps2rna_psj_tbl[i];
        psj->used = 0;
        if ((unsigned int)wk & 63) {
            printf("E0110101: ps2rna_init_psj wk size error\n");
            for (;;)
                ;
        }
        psj->sjrmt = sjrmt_create(1, wk, 256);
        if (psj->sjrmt == 0) {
            printf("E0110102: ps2rna_init_psj: can't creat SJUNI_CreaetRmt\n");
            for (;;)
                ;
        }
        wk += 256;
        if ((unsigned int)wk & 63) {
            printf("E0110103: ps2rna_init_psj: wk size error\n");
            for (;;)
                ;
        }
        psj->sjuni = sjuni_create(1, &ps2rna_sjuni_buf[i * 256], 256);
        if (psj->sjuni == 0) {
            printf("E0110104: ps2rna_init_psj: can't creat SJUNI_Creaet\n");
            for (;;)
                ;
        }
        psj->cklen = 16384;
        psj->ckdata = wk;
        psj->sjuni->vtbl->putchunk(psj->sjuni, 0, &psj->ckdata);
        wk += 16384;
        psj->sjx = sjx_create(psj->sjuni, psj->sjrmt, 1);
        if (psj->sjx == 0) {
            printf("E0110105: ps2rna_init_psj: can't creat SJX_Create\n");
            for (;;)
                ;
        }
    }
}

/* Frees the work area if ps2rna_init_psj allocated it. */
/* 0x001d0d68 */
static void ps2rna_finish_psj(void)
{
    int i;
    PsjObj *psj;

    for (i = 0; i < ps2rna_npsj; i++) {
        psj = &ps2rna_psj_tbl[i];
        if (psj->sjrmt != 0)
            sjrmt_destroy(psj->sjrmt);
        if (psj->sjuni != 0)
            psj->sjuni->vtbl->destroy(psj->sjuni);
        if (psj->sjx != 0)
            sjx_destroy(psj->sjx);
        memset(psj, 0, 24);
    }
    if (ps2rna_psj_work_own == 1) {
        sceSifFreeSysMemory(ps2rna_psj_work);
        ps2rna_psj_work = 0;
        ps2rna_psj_work_own = 0;
    }
}

/* Emptied; 0 when all are taken. */
/* 0x001d0e40 */
static PsjObj *ps2rna_alloc_psj(void)
{
    PsjObj *psj;
    int i;

    psj = 0;
    for (i = 0; i < ps2rna_npsj; i++) {
        psj = &ps2rna_psj_tbl[i];
        if (psj->used == 0)
            break;
    }
    if (i == 8)
        return 0;
    psj->sjuni->vtbl->reset(psj->sjuni);
    psj->sjuni->vtbl->putchunk(psj->sjuni, 0, &psj->ckdata);
    sjrmt_reset(psj->sjrmt);
    psj->used = 1;
    return psj;
}

/* 0x001d0ef8 */
static void ps2rna_free_psj(PsjObj *psj)
{
    psj->used = 0;
}

/* Ignores the IOP's answer (hangs on a null buffer). */
/* 0x001d0f00 */
static void ps2rna_dtx_recv_cb(void *obj, int *buf)
{
    int i;

    if (buf == 0) {
        for (;;)
            ;
    }
    for (i = 0; i < *buf; i++)
        ;
}

/*
 * DTX fill: per handle, one command per setting changed since the last round (2 play, 3 channels,
 * 4 rate, 5 volume via ps2rna_vol_tbl), at most 128; count at the buffer start.
 */
/* 0x001d0f58 */
static void ps2rna_dtx_send_cb(void *obj, int *buf)
{
    Ps2rnaObj *rna;
    Ps2rnaCmd *cmds;
    int n;
    int i;

    n = 0;
    cmds = (Ps2rnaCmd *)(buf + 4);
    for (i = 0; i < 8; i++) {
        rna = &ps2rna_obj_tbl[i];
        if (rna->used == 1) {
            if (n == 128)
                goto end;
            if (rna->play != rna->play_sent) {
                cmds[n].cmd = 2;
                cmds[n].id = rna->id;
                cmds[n].val = rna->play;
                n++;
                rna->play_sent = rna->play;
            }
            if (n == 128)
                goto end;
            if (rna->nch_cur != rna->nch_sent) {
                cmds[n].cmd = 3;
                cmds[n].id = rna->id;
                cmds[n].val = rna->nch_cur;
                n++;
                rna->nch_sent = rna->nch_cur;
            }
            if (n == 128)
                goto end;
            if (rna->sfreq != rna->sfreq_sent) {
                cmds[n].val = rna->sfreq;
                cmds[n].id = rna->id;
                cmds[n].cmd = 4;
                rna->sfreq_sent = rna->sfreq;
                n++;
            }
            if (n == 128)
                goto end;
            if (rna->vol != rna->vol_sent) {
                cmds[n].val2 = ps2rna_vol_tbl[-rna->vol];
                cmds[n].cmd = 5;
                cmds[n].val = 0;
                rna->vol_sent = rna->vol;
                cmds[n].id = rna->id;
                n++;
            }
            if (n == 128)
                goto end;
        }
    }
end:
    *buf = n;
}

/* First call: IOP joints, links, DTX, channel buffers, command exchange 1; hangs on failure. */
/* 0x001d10b0 */
void PS2RNA_Init(void)
{
    ps2rna_ver_str;
    if (ps2rna_init_count == 0) {
        sjrmt_lib_init();
        SJX_Init();
        DTX_Init();
        ps2rna_init_psj();
        memset(ps2rna_dtx_buf, 0, 2256);
        ps2rna_dtx_bsize = 2176;
        ps2rna_dtx_sbuf = (char *)((unsigned int)(ps2rna_dtx_buf + 64) & 0xffffffc0);
        if (ps2rna_iop_buf == 0) {
            ps2rna_iop_buf = sceSifAllocIopHeap(2256);
            if (ps2rna_iop_buf == 0) {
                printf("E0100301: PS2RNA_Init can't allocate IOP Heap\n");
                for (;;)
                    ;
            }
        }
        ps2rna_iop_buf_aligned = (char *)((unsigned int)(ps2rna_iop_buf + 64) & 0xffffffc0);
        ps2rna_dtx = DTX_Create(1, ps2rna_dtx_sbuf, ps2rna_iop_buf_aligned, ps2rna_dtx_bsize);
        if (ps2rna_dtx == 0) {
            printf("E0100302: PS2RNA_Init can't create DTX\n");
            for (;;)
                ;
        }
        dtx_set_send_cb(ps2rna_dtx, ps2rna_dtx_recv_cb, 0);
        dtx_set_recv_cb(ps2rna_dtx, ps2rna_dtx_send_cb, 0);
        dtx_call_iop(10, 0, 0, 0, 0);
    }
    ps2rna_init_count++;
}

/* Last user: destroys the exchange, frees IOP memory and buffers, shuts DTX, links, IOP joints. */
/* 0x001d1218 */
void ps2rna_lib_finish(void)
{
    if (--ps2rna_init_count == 0) {
        dtx_destroy(ps2rna_dtx);
        ps2rna_dtx = 0;
        sceSifFreeSysMemory(ps2rna_iop_buf);
        ps2rna_iop_buf = 0;
        ps2rna_finish_psj();
        DTX_Finish();
        sjx_lib_finish();
        sjrmt_lib_finish();
    }
}

/* Per channel a buffer, IOP side and DMA transfer; play and transfer off; 0 on failure. */
/* 0x001d1278 */
Ps2rnaObj *ps2rna_create(SjObj **sj, int nch)
{
    Ps2rnaObj *rna;
    int i;
    int n;
    int arg[4];
    int rcv[4];

    rna = ps2rna_obj_tbl;
    for (i = 0; i < 8; i++) {
        rna = &ps2rna_obj_tbl[i];
        if (rna->used == 0)
            break;
    }
    if (i == 8)
        return 0;
    memset(rna, 0, 84);
    rna->nch = nch;
    for (i = 0; i < nch; i++)
        rna->psj[i] = ps2rna_alloc_psj();
    arg[0] = nch;
    arg[1] = 0;
    for (i = 0; i < nch; i++)
        arg[2 + i] = (int)rna->psj[i]->sjrmt;
    rna->id = (void *)dtx_call_iop(8, arg, 4, rcv, 1);
    if (rna->id == 0) {
        printf("E0100401: can't create PS2RNA of IOP\n");
        return 0;
    }
    for (i = 0; i < nch; i++)
        rna->dtr[i] = dtr_create(sj[i], rna->psj[i]->sjuni);
    for (i = 0; i < nch; i++)
        rna->sj[i] = sj[i];
    rna->play = 0;
    rna->play_sent = 0;
    rna->nch_cur = nch;
    rna->nch_sent = nch;
    rna->vol = 0;
    rna->vol_sent = 0;
    for (i = 0; i < nch; i++) {
        rna->outpan[i] = 0;
        rna->unk_40[i] = 0;
    }
    rna->transfer_sw = 0;
    rna->unk_49 = 1;
    rna->unk_4c = 0;
    n = sj[0]->vtbl->getnum(sj[0], 1);
    n += sj[0]->vtbl->getnum(sj[0], 0);
    rna->used = 1;
    rna->bufsize = n;
    return rna;
}

/* 0x001d14f0 */
void ps2rna_destroy(Ps2rnaObj *rna)
{
    int i;
    int arg[4];

    for (i = 0; i < rna->nch; i++) {
        if (rna->dtr[i] != 0)
            dtr_destroy(rna->dtr[i]);
    }
    arg[0] = (int)rna->id;
    dtx_call_iop(9, arg, 1, 0, 0);
    for (i = 0; i < rna->nch; i++) {
        if (rna->psj[i] != 0)
            ps2rna_free_psj(rna->psj[i]);
    }
    rna->used = 0;
}

/* Unimplemented in the library: prints a message and hangs. */
/* 0x001d15c8 */
void PS2RNA_Start(Ps2rnaObj *rna)
{
    printf("PS2RNA_Start: not implemented\n");
    for (;;)
        ;
}

/* Transfer and play off. */
/* 0x001d1600 */
void ps2rna_stop(Ps2rnaObj *rna)
{
    ps2rna_set_transfer_sw(rna, 0);
    ps2rna_set_play_sw(rna, 0);
}

/* On starts each channel's DMA; off waits for a DMA in flight and stops. */
/* 0x001d1630 */
void ps2rna_set_transfer_sw(Ps2rnaObj *rna, int sw)
{
    int n;
    int i;

    n = rna->nch_cur;
    rna->transfer_sw = sw;
    if (sw == 1) {
        for (i = 0; i < rna->nch; i++)
            dtr_start(rna->dtr[i]);
    } else {
        for (i = 0; i < n; i++) {
            if (rna->dtr[i]->busy == 1) {
                while (sceSifDmaStat(rna->dtr[i]->sema) >= 0)
                    ;
            }
            rna->dtr[i]->busy = 0;
            dtr_stop(rna->dtr[i]);
            rna->transfer_sw = 0;
            rna->unk_49 = 1;
        }
    }
}

/* Off also empties every channel buffer. */
/* 0x001d1738 */
void ps2rna_set_play_sw(Ps2rnaObj *rna, int sw)
{
    int i;

    rna->play = sw;
    if (sw == 0) {
        for (i = 0; i < rna->nch; i++) {
            rna->psj[i]->sjuni->vtbl->reset(rna->psj[i]->sjuni);
            rna->psj[i]->sjuni->vtbl->putchunk(rna->psj[i]->sjuni, 0, &rna->psj[i]->ckdata);
            sjrmt_reset(rna->psj[i]->sjrmt);
        }
    }
}

/* Unimplemented in the library: prints a message and hangs. */
/* 0x001d17e0 */
static void PS2RNA_ClearBuf(Ps2rnaObj *rna)
{
    printf("PS2RNA_ClearBuf: not implemented\n");
    for (;;)
        ;
}

/* Unimplemented in the library: prints a message and hangs. */
/* 0x001d1818 */
static void PS2RNA_SetPcmType(Ps2rnaObj *rna)
{
    printf("PS2RNA_SetPcmType: not implemented\n");
    for (;;)
        ;
}

/* Always 0 at 48000 Hz in this version. */
/* 0x001d1850 */
void ps2rna_get_time(Ps2rnaObj *rna, int *type, int *rate)
{
    *type = 0;
    *rate = 48000;
}

/* Channel 0's EE buffer, samples. */
/* 0x001d1860 */
unsigned int ps2rna_get_free_smpl(Ps2rnaObj *rna)
{
    return (unsigned int)(16384 - rna->psj[0]->sjuni->vtbl->getnum(rna->psj[0]->sjuni, 0)) / 2;
}

/* Channel 0's EE buffer, samples. */
/* 0x001d1898 */
unsigned int ps2rna_get_data_smpl(Ps2rnaObj *rna)
{
    return (unsigned int)rna->psj[0]->sjuni->vtbl->getnum(rna->psj[0]->sjuni, 0) / 2;
}

/* Playing and transferring with under 64 input bytes: pads each channel with silence to 1024. */
/* 0x001d18c8 */
static void ps2rna_pad_input(Ps2rnaObj *rna)
{
    SjChunk ck;
    int n;
    int rest;
    int i;

    if (rna->play == 1 && rna->transfer_sw == 1) {
        n = rna->sj[0]->vtbl->getnum(rna->sj[0], 1);
        if (n > 0 && n < 64) {
            rest = 1024 - n;
            for (i = 0; i < rna->nch; i++) {
                rna->sj[i]->vtbl->getchunk(rna->sj[i], 0, rest, &ck);
                memset(ck.data, 0, ck.len);
                rna->sj[i]->vtbl->putchunk(rna->sj[i], 1, &ck);
                rna->sj[i]->vtbl->getchunk(rna->sj[i], 0, rest - ck.len, &ck);
                memset(ck.data, 0, ck.len);
                rna->sj[i]->vtbl->putchunk(rna->sj[i], 1, &ck);
            }
        }
    }
}

/* Pads every handle's input, then the transfer and link servers. */
/* 0x001d1a10 */
void ps2rna_exec_server(void)
{
    int i;

    cri_lock_enter();
    for (i = 0; i < 8; i++) {
        if (ps2rna_obj_tbl[i].used == 1)
            ps2rna_pad_input(&ps2rna_obj_tbl[i]);
    }
    cri_lock_leave();
    dtr_exec_server();
    sjx_exec_server();
}

/* Unimplemented in the library: prints a message and hangs. */
/* 0x001d1a88 */
void PS2RNA_SetStartSmpl(Ps2rnaObj *rna)
{
    printf("PS2RNA_SetStartSmpl: not implemented\n");
    for (;;)
        ;
}

/* Sent with the next commands. */
/* 0x001d1ac0 */
void ps2rna_set_nch(Ps2rnaObj *rna, int n)
{
    rna->nch_cur = n;
}

/* Sent with the next commands. */
/* 0x001d1ac8 */
void ps2rna_set_sfreq(Ps2rnaObj *rna, int sfreq)
{
    rna->sfreq = sfreq;
}

/* Clamped to -999..0; sent with the next commands. */
/* 0x001d1ad0 */
void ps2rna_set_out_vol(Ps2rnaObj *rna, int vol)
{
    if (vol > 0)
        vol = 0;
    if (vol < -999)
        vol = -999;
    rna->vol = vol;
}

/* Clamped to -15..15. */
/* 0x001d1af0 */
void ps2rna_set_out_pan(Ps2rnaObj *rna, int ch, int pan)
{
    int clamped;

    clamped = pan;
    if (clamped < -15)
        clamped = -15;
    if (clamped > 15)
        clamped = 15;
    rna->outpan[ch] = clamped;
}

/* 16 only; else prints a message and hangs. */
/* 0x001d1b20 */
void PS2RNA_SetBitPerSmpl(Ps2rnaObj *rna, int bps)
{
    if (bps != 16) {
        printf("PS2RNA_SetBitPerSmpl: not support %d bps\n", bps);
        for (;;)
            ;
    }
}

/* Unimplemented in the library: prints a message and hangs. */
/* 0x001d1b68 */
static void PS2RNA_GetStartSmpl(Ps2rnaObj *rna)
{
    printf("PS2RNA_GetStartSmpl: not implemented\n");
    for (;;)
        ;
}

/* 0x001d1ba0 */
int ps2rna_get_sfreq(Ps2rnaObj *rna)
{
    return rna->sfreq;
}

/* 0x001d1ba8 */
int ps2rna_get_out_vol(Ps2rnaObj *rna)
{
    return rna->vol;
}

/* 0x001d1bb0 */
int ps2rna_get_out_pan(Ps2rnaObj *rna, int ch)
{
    return rna->outpan[ch];
}

/* Always 16. */
/* 0x001d1bc0 */
int ps2rna_get_bit_per_smpl(Ps2rnaObj *rna)
{
    return 16;
}

/* Unimplemented in the library: prints a message and hangs. */
/* 0x001d1bc8 */
static void PS2RNA_IsOverflow(Ps2rnaObj *rna)
{
    printf("PS2RNA_IsOverflow: not implemented\n");
    for (;;)
        ;
}

/* Unimplemented in the library: prints a message and hangs. */
/* 0x001d1c00 */
static void PS2RNA_ClearOverflow(Ps2rnaObj *rna)
{
    printf("PS2RNA_ClearOverflow: not implemented\n");
    for (;;)
        ;
}

/* Unimplemented in the library: prints a message and hangs. */
/* 0x001d1c38 */
static void PS2RNA_Flush(Ps2rnaObj *rna)
{
    printf("PS2RNA_Flush: not implemented\n");
    for (;;)
        ;
}

/* Unimplemented in the library: prints a message and hangs. */
/* 0x001d1c70 */
int PS2RNA_DiscardData(Ps2rnaObj *rna)
{
    printf("PS2RNA_DiscardData: not implemented\n");
    for (;;)
        ;
}

/* Unimplemented in the library: prints a message and hangs. */
/* 0x001d1ca8 */
static void PS2RNA_SetTotalNumSmpl(Ps2rnaObj *rna)
{
    printf("PS2RNA_SetTotalNumSmpl: not implemented\n");
    for (;;)
        ;
}

/* Unimplemented in the library: prints a message and hangs. */
/* 0x001d1ce0 */
void PS2RNA_SetStmHdInfo(Ps2rnaObj *rna)
{
    printf("PS2RNA_SetStmHdInfo: not implemented\n");
    for (;;)
        ;
}

/* EE stream joint of a channel buffer. */
/* 0x001d1d18 */
SjObj *ps2rna_get_sjuni(Ps2rnaObj *rna, int ch)
{
    return rna->psj[ch]->sjuni;
}

/* IOP stream joint of a channel buffer. */
/* 0x001d1d30 */
void *ps2rna_get_sjrmt(Ps2rnaObj *rna, int ch)
{
    return rna->psj[ch]->sjrmt;
}
