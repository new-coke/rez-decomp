/* ROFS public calls, each through the installed implementation's table; -203 when not set up. */

#include "types_cri.h"
#include "rofs_if.h"
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

/* volume entry, 32 bytes */
typedef struct {
    char name[8];       /* 00 */
    char pad_08[4];
    void *arg;          /* 0c */
    char pad_10[12];
    void *dir;          /* 1c */
} RofsIfRofsVol;

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

/* After adding a volume or loading a directory. */
/* 0x001da160 */
static void rofs_delay(void)
{
    int i;

    for (i = 0x40000; i != 0; i--)
        ;
}

/* Installs rofs_mai_if_tbl, set up from cfg. */
/* 0x001da190 */
int rofs_if_set(void *cfg)
{
    rofs_if = &rofs_mai_if_tbl;
    return rofs_mai_if_tbl.init(cfg, &rofs_mai_if_tbl);
}

/* 0x001da1c0 */
void rofs_if_finish(void)
{
    if (rofs_if != 0) {
        rofs_if->finish();
        rofs_if = 0;
    }
}

/* Table ROFS reads its image files through. */
/* 0x001da200 */
static int ROFS_SetPfsFuncTbl(void *tbl)
{
    if (rofs_if)
        return rofs_if->set_pfs_func_tbl(tbl);
    return rofs_error("rofs_if.c", 111, "ROFS_SetPfsFuncTbl", -203);
}

/* On an open image file. */
/* 0x001da250 */
int ROFS_AddVolume(const char *vol, void *img, int extra)
{
    int ret;

    if (rofs_if) {
        ret = rofs_if->add_volume(vol, img, extra);
        if (ret)
            rofs_delay();
        return ret;
    }
    return rofs_error("rofs_if.c", 130, "ROFS_AddVolume", -203);
}

/* 0x001da2c0 */
int ROFS_DelVolume(const char *vol)
{
    if (rofs_if)
        return rofs_if->del_volume(vol);
    return rofs_error("rofs_if.c", 142, "ROFS_DelVolume", -203);
}

/* 0x001da310 */
static int ROFS_SetDefVolume(const char *vol)
{
    if (rofs_if)
        return rofs_if->set_def_volume(vol);
    return rofs_error("rofs_if.c", 154, "ROFS_SetDefVolume", -203);
}

/* 0x001da360 */
static void *ROFS_GetDefVolume(void)
{
    if (rofs_if)
        return (void *)rofs_if->get_def_volume();
    rofs_error("rofs_if.c", 166, "ROFS_GetDefVolume", -203);
    return 0;
}

/* "volume:name" in dir, or the volume's current one. */
/* 0x001da3b8 */
void *ROFS_Open(const char *fname, void *dir)
{
    if (rofs_if)
        return (void *)rofs_if->open(fname, dir);
    rofs_error("rofs_if.c", 179, "ROFS_Open", -203);
    return 0;
}

/* nsct sectors from sector ofst as a file. */
/* 0x001da410 */
static void *ROFS_OpenRange(const char *fname, int ofst, int nsct, void *dir)
{
    if (rofs_if)
        return (void *)rofs_if->open_range(fname, ofst, nsct, dir);
    rofs_error("rofs_if.c", 192, "ROFS_OpenRange", -203);
    return 0;
}

/* 0x001da468 */
void rofs_if_exec(void)
{
    if (rofs_if)
        rofs_if->exec_server();
}

/* In the volume's current directory. */
/* 0x001da498 */
int rofs_get_file_size(const char *fname)
{
    if (rofs_if)
        return rofs_if->get_file_siz(fname, 0);
    return rofs_error("rofs_if.c", 215, "ROFS_GetFileSiz", -203);
}

/* 0x001da4e8 */
int rofs_get_file_size_dir(const char *fname, void *dir)
{
    if (rofs_if)
        return rofs_if->get_file_siz(fname, dir);
    return rofs_error("rofs_if.c", 227, "ROFS_GetFileSiz", -203);
}

/* 0x001da538 */
int rofs_get_sct_len(void)
{
    return 2048;
}

/* 0x001da540 */
int ROFS_GetVolumeInf(const char *vol, void *info)
{
    if (rofs_if)
        return rofs_if->get_volume_inf(vol, info);
    return rofs_error("rofs_if.c", 247, "ROFS_GetVolumeInf", -203);
}

/* Into buf (size entries), or the volume's own record. */
/* 0x001da590 */
int ROFS_LoadDir(const char *dname, void *buf, int size)
{
    int ret;

    if (rofs_if) {
        ret = rofs_if->load_dir(dname, buf, size);
        if (ret)
            rofs_delay();
        return ret;
    }
    return rofs_error("rofs_if.c", 266, "ROFS_LoadDir", -203);
}

/* Loaded record becomes the volume's current directory. */
/* 0x001da600 */
static int ROFS_SetDir(const char *vol, void *dir)
{
    if (rofs_if)
        return rofs_if->set_dir(vol, dir);
    return rofs_error("rofs_if.c", 278, "ROFS_SetDir", -203);
}

/* Up to n (name, size, flags) entries of the volume's current directory. */
/* 0x001da650 */
int ROFS_GetDirInf(void *dir, void *inf, int n)
{
    if (rofs_if)
        return rofs_if->get_dir_inf(dir, inf, n);
    return rofs_error("rofs_if.c", 290, "ROFS_GetDirInf", -203);
}

/* Non-zero when it exists. */
/* 0x001da6a0 */
void *ROFS_IsExistFile(const char *fname, void *dir)
{
    if (rofs_if)
        return (void *)rofs_if->is_exist_file(fname, dir);
    rofs_error("rofs_if.c", 302, "ROFS_IsExistFile", -203);
    return 0;
}

/* In the volume's current directory. */
/* 0x001da6f8 */
int ROFS_GetNumFiles(const char *vol)
{
    if (rofs_if)
        return rofs_if->get_num_files(vol);
    return rofs_error("rofs_if.c", 315, "ROFS_GetNumFiles", -203);
}

/* 0x001da748 */
void ROFS_EntryErrFunc(void *fn, void *obj)
{
    if (rofs_if == 0) {
        rofs_error("rofs_if.c", 328, "ROFS_EntryErrFunc", -203);
        return;
    }
    rofs_if->entry_err_func(fn, obj);
}
