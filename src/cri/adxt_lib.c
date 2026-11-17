/*
 * ADX playback library setup and shutdown of the sub-libraries in order; SVM server
 * registration; play by file name or AFS entry through a handle's play list.
 */

#include "cri.h"
#include "libc.h"
#include "adxt_lib.h"
#include "adx_error.h"
#include "adx_lock.h"
#include "adxf.h"
#include "adxm.h"
#include "adxrna.h"
#include "adxsjd.h"
#include "adxstm.h"
#include "adxt.h"
#include "lsc.h"
#include "sjmem.h"
#include "sjrbf.h"
#include "sjuni.h"
#include "svm.h"
#include "dtx.h"

extern char adxt_obj_tbl[];
extern char adxt_afs_fname[];

/* 1: adxt_lib_init registers the playback server with SVM. */
/* 0x001c5ed8 */
static void adxt_set_server_flag(int x)
{
    adxt_server_flag = x;
}

/* -> ADX error. */
/* 0x001c5ee0 */
static void adxt_error_callback(void *obj, const char *msg)
{
    adx_error(msg);
}

/* The vsync play time counts these calls. */
/* 0x001c5ee8 */
static void adxt_count_exec_server(void)
{
    adxt_exec_count++;
    adxt_exec_server();
}

/* First call: sub-libraries, handle table, playback server (if enabled); counts calls. */
/* 0x001c5ef8 */
void adxt_lib_init(void)
{
    if (adxt_init_count == 0) {
        adx_lock_init();
        adx_lock();
        sjuni_lib_init();
        sjrbf_lib_init();
        sjmem_lib_init();
        adx_error_init();
        adxstm_lib_init();
        adxsjd_lib_init();
        adxf_lib_init();
        adxrna_init();
        memset(adxt_obj_tbl, 0, 1312);
        lsc_lib_init();
        if (adxt_server_flag == 1)
            adxt_svm_server_id = SVM_SetCbSvr(2, adxt_svm_server, 0);
        adxt_exec_count = 0;
        adx_unlock();
    }
    adxt_init_count++;
}

/* Whatever the call count. */
/* 0x001c5fb8 */
static void adxt_lib_reinit(void)
{
    adxt_init_count = 0;
    adxt_lib_init();
}

/* Last user: removes the server, shuts sub-libraries down in reverse, dtx_send_sw held off. */
/* 0x001c5fc0 */
static void adxt_lib_finish(void)
{
    if (--adxt_init_count == 0) {
        dtx_send_sw = 0;
        adx_lock();
        SVM_DelCbSvr(2, adxt_svm_server_id);
        svm_lib_finish();
        lsc_lib_finish();
        adx_unlock();
        adxrna_finish();
        adx_lock();
        adxf_lib_finish();
        adxsjd_lib_finish();
        adxstm_lib_finish();
        adx_error_finish();
        sjmem_lib_finish();
        sjrbf_lib_finish();
        sjuni_lib_finish();
        adx_unlock();
        dtx_send_sw = 1;
    }
}

/* Reports the name on failure. */
/* 0x001c6068 */
static void adxt_entry_fname(AdxtObj *adxt, const char *fname)
{
    if (lsc_entry_fname(adxt->stm, fname) < 0)
        adx_error_cat("E0010601:Can't entry file", fname);
}

/* Reports the ids on failure. */
/* 0x001c60b0 */
static void ADXT_EntryAfs(AdxtObj *adxt, int ptid, int flid)
{
    char buf[16];
    void *dir;
    int ofst;
    int nsct;
    void *stm;

    stm = adxt->stm;
    if (adxf_get_pt_file_info_dir(ptid, flid, adxt_afs_fname, &dir, &ofst, &nsct) == 0) {
        lsc_entry_file(stm, adxt_afs_fname, dir, ofst, nsct);
    } else {
        adx_format_2num(ptid, flid, buf, 16);
        adx_error_cat("E0071301 ADXT_EntryAfs: can't entry", buf);
    }
}

/* Play mode 4: empties joints, starts decoder and list, refill at the read-ahead size. */
/* 0x001c6160 */
static void adxt_start_lsc(AdxtObj *adxt)
{
    int i;
    void *stm;

    stm = adxt->stm;
    adxt_stop_unlocked(adxt);
    adx_lock();
    for (i = 0; i < adxt->nch; i++)
        adxt->sjo[i]->vtbl->reset(adxt->sjo[i]);
    adxt->sji = adxt->sjf;
    adxt->sjf->vtbl->reset(adxt->sjf);
    adxsjd_set_input(adxt->sjd, adxt->sji);
    adxsjd_start(adxt->sjd);
    adxt->stat = 1;
    adxt->pmode = 4;
    lsc_set_min_fill(stm, adxt->minsct << 11);
    lsc_start(stm);
    adxt->unk_98 = 1;
    adxt->lpcnt = 0;
    adx_unlock();
}

/* 1: the list repeats its files. */
/* 0x001c6240 */
static void adxt_set_lsc_flag(AdxtObj *adxt, int x)
{
    lsc_set_flag(adxt->stm, x);
}

/* Repeats: sole play list entry, then starts. */
/* 0x001c6248 */
static void adxt_play_fname(AdxtObj *adxt, const char *fname)
{
    void *stm;

    stm = adxt->stm;
    lsc_stop(stm);
    adxt_entry_fname(adxt, fname);
    lsc_set_flag(stm, 1);
    adxt_start_lsc(adxt);
}

/* 0x001c62a8 */
void adxt_stream_stop_hook(void)
{
}

/* 0x001c62b0 */
static int adxt_get_lsc_nstm(AdxtObj *adxt)
{
    return lsc_get_nstm(adxt->stm);
}
