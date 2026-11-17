/* cvFs glue: each ROFS volume as a cvFs device of its name; ROFS reads image files through cvFs. */

#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "pfs_cvfs.h"
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
#include "dtr.h"
#include "dtx.h"
#include "dvci.h"
#include "htci.h"
#include "htci_filelist.h"
#include "kernel_syscalls.h"
#include "libcdvd.h"
#include "libgraph.h"
#include "libkernl_cache.h"
#include "libkernl_fileio.h"
#include "libkernl_iopheap.h"
#include "lsc.h"
#include "ps2rna.h"
#include "rofs_conv.h"
#include "rofs_dir.h"
#include "rofs_hn.h"
#include "rofs_mai.h"
#include "rofs_pfs.h"
#include "sjmem.h"
#include "sjrbf.h"
#include "sjrmt.h"
#include "sjuni.h"
#include "sjx.h"
#include "srd.h"
#include "svm.h"

#define CRI_MIN(a, b) ((a) < (b) ? (a) : (b))

#define CRI_SW16(x) (((x) >> 8 & 0xff) | ((x) << 8 & 0xff00))

#define CRI_SW32(x) (((x) >> 24 & 0xff) | ((x) >> 8 & 0xff00) | ((x) << 8 & 0xff0000) | (x) << 24)

#define ROFS_U32(p) (((RofsU32 *)(p))->v)

/* ROFS volume entry, 32 bytes */
typedef struct {
    char name[8];       /* 00 */
    char pad_08[4];
    void *arg;          /* 0c */
    char pad_10[12];
    void *dir;          /* 1c */
} PfsCvfsRofsVol;

#define ROFS_SW32(x) (((x) & 0xff) << 24 | ((x) >> 8 & 0xff) << 16 | ((x) >> 16 & 0xff) << 8 | ((x) >> 24 & 0xff))

/* Same swap on a value in memory (low byte read as a byte). */
#define ROFS_SW32M(x) ((unsigned char)(x) << 24 | ((x) >> 8 & 0xff) << 16 | ((x) >> 16 & 0xff) << 8 | ((x) >> 24 & 0xff))

/* SJ object; first word is the interface table */
typedef struct PfsCvfsSjObj PfsCvfsSjObj;

struct PfsCvfsSjObj {
    SjIf *vtbl;
};

extern LscObj lsc_obj_tbl[];

extern void lsc_set_error_cb(void *fn, void *obj);

extern void lsc_open_entry(LscObj *lsc);

extern void lsc_poll_stream(LscObj *lsc);
extern void lsc_close_entry(LscObj *lsc);

extern unsigned long __moddi3(unsigned long a, unsigned long b);
extern unsigned long __divdi3(unsigned long a, unsigned long b);

extern int sceCdRead(int lsn, int nsct, void *buf, SrdMode *mode);

extern void *RFHN_CreateDirhn(void *area, int ndir);
extern int rofs_if_set(void *arg);

extern void ROFS_EntryErrFunc(void *h);
extern int rofs_get_file_size(void *h);
extern int ROFS_Open(void *h);
extern int rofs_get_sct_len(void *h);
extern int ROFS_IsExistFile(void *h);
extern int ROFS_GetDirInf(void *h);
extern int rofs_get_file_size_dir(void *h);
extern void cvFsAddDev(const char *name, void *fn, int a);
extern void cvFsDelDev(const char *name);
extern int ROFS_AddVolume(const char *name, void *arg);

extern int ROFS_DelVolume(const char *name);
extern int ROFS_GetNumFiles(void *a);
extern int ROFS_GetVolumeInf(void *a, void *b);

extern char *volatile cvfs_ver_str;
extern void (*cvfs_error_fn)(void *obj, const char *msg, int arg);
extern void *cvfs_error_obj;
extern int cvfs_init_count;
extern CvFsHn cvfs_hn_tbl[40];

extern CvFsDev cvfs_dev_tbl[32];
extern char cvfs_def_dev_name[];
extern void cvfs_error(const char *msg);

extern int cvFsSeek(CvFsHn *h, int ofst, int whence);
extern int cvFsReqRd(void);
extern void cvFsStopTr(void);
extern unsigned int cvFsGetStat(void);
extern int cvFsGetSctLen(void);
extern int cvFsGetNumTr(void);
extern void cvfs_exec_server(void);
extern char pfs_cvfs_tbl[];

/* "ROFS" device. */
/* 0x001de1b8 */
int *rofs_get_cvfs_rec(void)
{
    return rofs_cvfs_rec_tbl;
}

/* Per ROFS volume. */
/* 0x001de1c8 */
static int *rofs_cvfs_get_if(void)
{
    return rofs_cvfs_if_tbl;
}

/* 0x001de1d8 */
static void rofs_cvfs_exec(void)
{
    rofs_if_exec();
}

/* 0x001de1e0 */
static void rofs_cvfs_entry_err(void *fn)
{
    ROFS_EntryErrFunc(fn);
}

/* 0x001de1e8 */
static int rofs_cvfs_get_file_size(void *fname)
{
    return rofs_get_file_size(fname);
}

/* 0x001de208 */
static int rofs_cvfs_open(void *fname)
{
    return ROFS_Open(fname);
}

/* 0x001de228 */
static void rofs_cvfs_close(char *hn)
{
    rofs_close(hn);
}

/* whence 0 start, 1 current, 2 end; -205 otherwise. */
/* 0x001de230 */
static int rofs_cvfs_seek(void *hn, int ofs, int whence)
{
    int origin;

    switch ((unsigned int)whence) {
    case 0:
        origin = 0;
        break;
    case 1:
        origin = 1;
        break;
    case 2:
        origin = 2;
        break;
    default:
        return -205;
    }
    return ROFS_Seek(hn, ofs, origin);
}

/* 0x001de280 */
static int rofs_cvfs_tell(void *hn)
{
    return ROFS_Tell(hn);
}

/* 0x001de2a0 */
static int rofs_cvfs_req_rd(void *hn, int nsct, void *buf)
{
    return ROFS_ReqRd(hn, nsct, buf);
}

/* 0x001de2c0 */
static void rofs_cvfs_stop_tr(void *hn)
{
    ROFS_StopTr(hn);
}

/* 0, 1, 2 as is; anything else 3 (error). */
/* 0x001de2c8 */
static int rofs_cvfs_get_stat(void *hn)
{
    switch ((unsigned int)ROFS_GetStat(hn)) {
    case 0:
        return 0;
    case 1:
        return 1;
    case 2:
        return 2;
    }
    return 3;
}

/* 0x001de318 */
static int rofs_cvfs_get_sct_len(void *hn)
{
    return rofs_get_sct_len(hn);
}

/* 0x001de338 */
static int rofs_cvfs_get_num_tr(void *hn)
{
    return ROFS_GetNumTr(hn);
}

/* Loads the directory; 0, or -1 on failure. */
/* 0x001de358 */
static int rofs_cvfs_change_dir(void *dname)
{
    return rofs_load_dir_simple(dname) == 0 ? 0 : -1;
}

/* 0x001de380 */
static int rofs_cvfs_is_exist_file(void *fname)
{
    return ROFS_IsExistFile(fname);
}

/* 0x001de3a0 */
static int rofs_cvfs_zero(void)
{
    return 0;
}

/* 0x001de3a8 */
static int rofs_cvfs_get_dir_inf(void *dir)
{
    return ROFS_GetDirInf(dir);
}

/* 0x001de3c8 */
static int rofs_cvfs_get_file_size_ex(void *fname)
{
    return rofs_get_file_size_dir(fname);
}

/* Plus a same-name cvFs device (up to 8 chars, not "ROFS"); undone on failure unless error -104. */
/* 0x001de3e8 */
static int rofs_cvfs_add_volume(const char *name, void *img)
{
    int ret;

    if (strcmp(name, "ROFS") == 0)
        return -1;
    if (strlen(name) >= 9)
        return -1;
    cvFsAddDev(name, rofs_cvfs_get_if, 0);
    ret = ROFS_AddVolume(name, img);
    if (ret != 0 && ret != -104)
        cvFsDelDev(name);
    return ret;
}

/* And its cvFs device; not "ROFS" itself. */
/* 0x001de480 */
static int rofs_cvfs_del_volume(const char *name)
{
    int ret;

    if (strcmp(name, "ROFS") == 0)
        return -1;
    ret = ROFS_DelVolume(name);
    if (ret != 0)
        return ret;
    cvFsDelDev(name);
    return 0;
}

/* 2 add volume, 3 remove, 4 file count, 5 info, 100 "handles volumes"; others (incl. 1) -1. */
/* 0x001de4d0 */
static int rofs_cvfs_option(void **prm, int cmd)
{
    switch (cmd) {
    case 2:
        if (prm == 0 || prm[0] == 0 || prm[1] == 0)
            return -1;
        return rofs_cvfs_add_volume(prm[1], prm[0]);
    case 3:
        if (prm == 0 || prm[1] == 0)
            return -1;
        return rofs_cvfs_del_volume(prm[1]);
    case 4:
        if (prm == 0 || prm[1] == 0)
            return -1;
        return ROFS_GetNumFiles(prm[1]);
    case 5:
        if (prm == 0 || prm[1] == 0)
            return -1;
        return ROFS_GetVolumeInf(prm[1], prm[2]);
    case 100:
        return 1;
    case 1:
        return -1;
    }
    return -1;
}

/* whence 0, 1 or 2; others reported as -100. */
/* 0x001de5e0 */
static int pfsCvfsSeek(CvFsHn *hn, int ofst, unsigned int whence)
{
    int org;

    switch (whence) {
    case 0:
        org = 0;
        break;
    case 1:
        org = 1;
        break;
    case 2:
        org = 2;
        break;
    default:
        return rofs_error("pfs_cvfs.c", 119, "pfsCvfsSeek", -100);
    }
    return cvFsSeek(hn, ofst, org);
}

/* 0x001de648 */
static int pfs_cvfs_req_rd(void)
{
    return cvFsReqRd();
}

/* 0x001de668 */
static void pfs_cvfs_stop_tr(void)
{
    cvFsStopTr();
}

/* 0, 1, 2 as is; anything else 3 (error). */
/* 0x001de670 */
static int pfs_cvfs_get_stat(void)
{
    switch (cvFsGetStat()) {
    case 0:
        return 0;
    case 1:
        return 1;
    case 2:
        return 2;
    }
    return 3;
}

/* 0x001de6c0 */
static int pfs_cvfs_get_sct_len(void)
{
    return cvFsGetSctLen();
}

/* 0x001de6e0 */
static int pfs_cvfs_get_num_tr(void)
{
    return cvFsGetNumTr();
}

/* cvFs server, then the sector read server. */
/* 0x001de700 */
static void pfs_cvfs_exec_server(void)
{
    cvfs_exec_server();
    srd_exec_server();
}

/* Table ROFS reads its image files through. */
/* 0x001de720 */
char *pfs_cvfs_get_table(void)
{
    return pfs_cvfs_tbl;
}
