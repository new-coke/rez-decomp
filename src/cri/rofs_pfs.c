/* ROFS image device: reads via the file table (cvFs), sector size conversion; vsync counter. */

#include "types_cri.h"
#include "rofs_pfs.h"
#include "libc.h"
#include "adx_decode.h"
#include "adx_error.h"
#include "adx_lock.h"
#include "adx_peak.h"
#include "adxb.h"
#include "adxb_aiff.h"
#include "adxb_au.h"
#include "adxb_spsd.h"
#include "adxb_wave.h"
#include "adxd.h"
#include "adxstm.h"
#include "cri_lock.h"
#include "cvfs.h"
#include "dtr.h"
#include "dtx.h"
#include "htci.h"
#include "htci_filelist.h"
#include "kernel_syscalls.h"
#include "libcdvd.h"
#include "libgraph.h"
#include "libkernl_cache.h"
#include "libkernl_fileio.h"
#include "libkernl_iopheap.h"
#include "lsc.h"
#include "pfs_cvfs.h"
#include "ps2rna.h"
#include "rofs_conv.h"
#include "rofs_dir.h"
#include "rofs_hn.h"
#include "rofs_if.h"
#include "rofs_mai.h"
#include "sjmem.h"
#include "sjrbf.h"
#include "sjrmt.h"
#include "sjuni.h"
#include "sjx.h"
#include "srd.h"
#include "svm.h"

static int waitRdComplete(void *hn);
static int parentSeek(void *hn, int nsct, int size);

#define CRI_MIN(a, b) ((a) < (b) ? (a) : (b))

#define CRI_SW16(x) (((x) >> 8 & 0xff) | ((x) << 8 & 0xff00))

#define CRI_SW32(x) (((x) >> 24 & 0xff) | ((x) >> 8 & 0xff00) | ((x) << 8 & 0xff0000) | (x) << 24)

#define ROFS_U32(p) (((RofsU32 *)(p))->v)

/* volume entry, 32 bytes */
typedef struct {
    char name[8];       /* 00 */
    char pad_08[4];
    void *arg;          /* 0c */
    char pad_10[12];
    void *dir;          /* 1c */
} RofsPfsRofsVol;

#define ROFS_SW32(x) (((x) & 0xff) << 24 | ((x) >> 8 & 0xff) << 16 | ((x) >> 16 & 0xff) << 8 | ((x) >> 24 & 0xff))

/* Same swap on a value in memory (low byte read as a byte). */
#define ROFS_SW32M(x) ((unsigned char)(x) << 24 | ((x) >> 8 & 0xff) << 16 | ((x) >> 16 & 0xff) << 8 | ((x) >> 24 & 0xff))

extern void lsc_set_error_cb(void *fn, void *obj);

extern void lsc_open_entry(LscObj *lsc);

extern void lsc_poll_stream(LscObj *lsc);
extern void lsc_close_entry(LscObj *lsc);

extern unsigned long __moddi3(unsigned long a, unsigned long b);

extern unsigned long __divdi3(unsigned long a, unsigned long b);

extern int sceCdRead(int lsn, int nsct, void *buf, SrdMode *mode);

extern void *RFHN_CreateDirhn(void *area, int ndir);

/* -203 without a table. */
/* 0x001dc758 */
static int dcfSeek(void *fh, int ofst, int whence)
{
    if (rofs_pfs_tbl && rofs_pfs_tbl->seek)
        return rofs_pfs_tbl->seek(fh, ofst, whence);
    return rofs_error("rofs_pfs.c", 142, "dcfSeek", -203);
}

/* -203 without a table. */
/* 0x001dc7b0 */
static int dcfReqRd(void *fh, int nsct, void *buf)
{
    if (rofs_pfs_tbl && rofs_pfs_tbl->req_rd)
        return rofs_pfs_tbl->req_rd(fh, nsct, buf);
    return rofs_error("rofs_pfs.c", 156, "dcfReqRd", -203);
}

/* 0x001dc808 */
static void rofs_dcf_stop_tr(void *fh)
{
    if (rofs_pfs_tbl && rofs_pfs_tbl->stop_tr)
        rofs_pfs_tbl->stop_tr(fh);
}

/* 0x001dc840 */
static void rofs_dcf_exec_server(void)
{
    if (rofs_pfs_tbl && rofs_pfs_tbl->exec_server)
        rofs_pfs_tbl->exec_server();
}

/* 3 (error) without a table. */
/* 0x001dc878 */
static int dcfGetStat(void *fh)
{
    if (rofs_pfs_tbl && rofs_pfs_tbl->get_stat)
        return rofs_pfs_tbl->get_stat(fh);
    rofs_error("rofs_pfs.c", 196, "dcfGetStat", -203);
    return 3;
}

/* -203 without a table. */
/* 0x001dc8d8 */
static int dcfGetSctLen(void *fh)
{
    if (rofs_pfs_tbl && rofs_pfs_tbl->get_sct_len)
        return rofs_pfs_tbl->get_sct_len(fh);
    return rofs_error("rofs_pfs.c", 211, "dcfGetSctLen", -203);
}

/* Bytes; -203 without a table. */
/* 0x001dc930 */
static int dcfGetNumTr(void *fh)
{
    if (rofs_pfs_tbl && rofs_pfs_tbl->get_num_tr)
        return rofs_pfs_tbl->get_num_tr(fh);
    return rofs_error("rofs_pfs.c", 225, "dcfGetNumTr", -203);
}

/* 2048-byte sectors, converted to the file's sector size; returns 2048-byte sectors or an error. */
/* 0x001dc988 */
static int dcfReqRdBlk(void *fh, int lsn, int nsct, void *buf)
{
    int sl;
    int ret;

    if (fh == 0 || buf == 0)
        return rofs_error("rofs_pfs.c", 239, "dcfReqRdBlk", -100);
    sl = dcfGetSctLen(fh);
    if (sl <= 0)
        return rofs_error("rofs_pfs.c", 244, "dcfGetSctLen", -202);
    ret = parentSeek(fh, lsn, sl);
    if (ret != 0)
        return rofs_error("rofs_pfs.c", 250, "parentSeek", ret);
    ret = dcfReqRd(fh, (nsct << 11) / sl, buf);
    if (ret > 0)
        ret = ret * sl / 2048;
    return ret;
}

/* Returns bytes read or an error. */
/* 0x001dcab0 */
static int rofs_dcf_read_sync(void *fh, int lsn, int nsct, void *buf)
{
    int n;

    if (nsct <= 0)
        return 0;
    n = dcfReqRdBlk(fh, lsn, nsct, buf);
    if (n < 0)
        return rofs_error("rofs_pfs.c", 278, "dcfReqRdBlk", n);
    if (waitRdComplete(fh) != 1)
        return rofs_error("rofs_pfs.c", 282, "waitRdComplete", -207);
    return n << 11;
}

/* From sector base + lsn. */
/* 0x001dcb50 */
static int dcfReqRdCda(int base, void *fh, int lsn, int nsct, void *buf)
{
    return dcfReqRdBlk(fh, lsn + base, nsct, buf);
}

/* From sector base + lsn; bytes read or an error. */
/* 0x001dcb80 */
static int rofs_dcf_read_cda_sync(int base, void *fh, int lsn, int nsct, void *buf)
{
    int n;

    if (nsct <= 0)
        return 0;
    n = dcfReqRdCda(base, fh, lsn, nsct, buf);
    if (n < 0)
        return rofs_error("rofs_pfs.c", 322, "dcfReqRdCda", n);
    if (waitRdComplete(fh) != 1)
        return rofs_error("rofs_pfs.c", 327, "waitRdComplete", -207);
    return n << 11;
}

/* Between transfer polls. */
/* 0x001dcc20 */
void rofs_delay_short(void)
{
    int i;

    for (i = 0x3fff; i >= 0; i--)
        ;
}

/* 0x001dcc50 */
static int rofs_count_cb(void)
{
    rofs_vsync_count++;
    return 0;
}

/* Waits for the sector read server; saves the game's callback. */
/* 0x001dcc68 */
void rofs_install_count_cb(void)
{
    while (srd_is_busy() == 1)
        ;
    rofs_saved_vsync_cb = sceGsSyncVCallback(rofs_count_cb);
}

/* Restores the game's vsync callback. */
/* 0x001dccb8 */
void rofs_restore_cb(void)
{
    sceGsSyncVCallback((void *)rofs_saved_vsync_cb);
}

/* Runs the file server; 1 done; 0 failed, idle or watchdog stall. */
/* 0x001dccc8 */
static int waitRdComplete(void *hn)
{
    int three = 3;
    int ret;

    rofs_reset_watchdog();
    while ((ret = dcfGetStat(hn)) != 1) {
        if (ret == three)
            return 0;
        if (ret == 0)
            return 0;
        rofs_dcf_exec_server();
        rofs_delay_short();
        if (rofs_check_watchdog(hn) != 1)
            return 0;
    }
    return 1;
}

/* To 2048-byte sector nsct, in file sectors of size bytes; 0, -100 negative, -205 short. */
/* 0x001dcd58 */
static int parentSeek(void *hn, int nsct, int size)
{
    int n;

    if (nsct < 0)
        return -100;
    n = (nsct << 11) / size;
    return dcfSeek(hn, n, 0) == n ? 0 : -205;
}

/* Sets the table image files are read through. */
/* 0x001dcdb0 */
void rofs_set_pfs_flag(int x)
{
    rofs_pfs_tbl = x;
}

/* Vsync counter cleared. */
/* 0x001dcdc0 */
RofsDev *rofs_get_cd_device(void)
{
    rofs_saved_vsync_cb = 0;
    rofs_vsync_count = 0;
    return &rofs_dcf_dev;
}

/* 0x001dcde0 */
static int rofs_get_cb_count(void)
{
    return rofs_vsync_count;
}
