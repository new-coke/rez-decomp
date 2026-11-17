/*
 * ROFS core: work area from the game (handles, volumes, dir records, sector buffer); mounting an
 * image as a named volume; files, ranges, dirs; read server with watchdog; errors.
 */

#define CRI_MIN(a, b) ((a) < (b) ? (a) : (b))

#include "types_cri.h"
#include "libc.h"
#include "rofs_mai.h"
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
#include "pfs_cvfs.h"
#include "ps2rna.h"
#include "rofs_conv.h"
#include "rofs_dir.h"
#include "rofs_hn.h"
#include "rofs_if.h"
#include "rofs_pfs.h"
#include "sjmem.h"
#include "sjrbf.h"
#include "sjrmt.h"
#include "sjuni.h"
#include "sjx.h"
#include "srd.h"
#include "svm.h"

static void rofs_clear_volume(RofsVol *vol);
static void rofs_shutdown(void);
static int RFMAI_SetPfsFuncTblG(void *tbl);
static int mountVolume(const char *vname, const char *dev);
static int rofs_unmount(const char *vname);
static RofsVol *rofs_get_def_volume(void);

#define CRI_SW16(x) (((x) >> 8 & 0xff) | ((x) << 8 & 0xff00))

#define CRI_SW32(x) (((x) >> 24 & 0xff) | ((x) >> 8 & 0xff00) | ((x) << 8 & 0xff0000) | (x) << 24)

#define ROFS_U32(p) (((RofsU32 *)(p))->v)

#define ROFS_SW32(x) (((x) & 0xff) << 24 | ((x) >> 8 & 0xff) << 16 | ((x) >> 16 & 0xff) << 8 | ((x) >> 24 & 0xff))

/* Same swap on a value in memory (low byte read as a byte). */
#define ROFS_SW32M(x) ((unsigned char)(x) << 24 | ((x) >> 8 & 0xff) << 16 | ((x) >> 16 & 0xff) << 8 | ((x) >> 24 & 0xff))

/* SJ object; first word is the interface table */
typedef struct RofsMaiSjObj RofsMaiSjObj;

struct RofsMaiSjObj {
    SjIf *vtbl;
};

/* primary volume descriptor, unaligned */
typedef struct {
    char unk_00[132];
    int ptsize;
    int unk_88;
    int ptloc;
} __attribute__((packed)) RofsPvd;

/* Always 0. */
#define AZ(p) ((((int)(p)) & 1) >> 1)

typedef struct {
    char *name;
    int size;
    unsigned char flags;
} RofsDirInf;

extern void lsc_set_error_cb(void *fn, void *obj);

extern void lsc_open_entry(LscObj *lsc);

extern void lsc_poll_stream(LscObj *lsc);
extern void lsc_close_entry(LscObj *lsc);

extern unsigned long __moddi3(unsigned long a, unsigned long b);

extern unsigned long __divdi3(unsigned long a, unsigned long b);

extern int sceCdRead(int lsn, int nsct, void *buf, SrdMode *mode);

extern void *RFHN_CreateDirhn(void *area, int ndir);

extern int rofs_watchdog[3];

/* ISO 9660 identifier "CD001". */
/* 0x001da798 */
static int checkMedia(char *vd)
{
    char *p;
    char *std_id;
    int i;

    p = vd + 1;
    std_id = "CD001";
    for (i = 0; i < 5; i++) {
        if (*std_id++ != *p++)
            return 0;
    }
    return 1;
}

/* "VOLUME:file"; volume upper-cased, default without ':'; both size-checked; 1 on success. */
/* 0x001da7e0 */
static int divideFname(char *dev, char *fname, int devsize, int fsize, char *path)
{
    char buf[512];
    char *sep;
    int i;
    int j;
    int len;
    char *def_vol;

    if (dev == 0)
        return 0;
    if (fname == 0)
        return 0;
    if (path == 0)
        return 0;
    if (devsize <= 0)
        return 0;
    if (fsize <= 0)
        return 0;
    if (strlen(path) > 512)
        return 0;
    memset(buf, 0, 512);
    for (i = 0; i < 512 && path[i] != ':' && path[i] != 0; i++)
        buf[i] = path[i];
    sep = path + i;
    len = strlen(buf) + 1;
    if (*sep == 0) {
        if (fsize < len)
            return 0;
        memcpy(fname, buf, len);
        *dev = 0;
        def_vol = rofs_get_def_volume();
        if (def_vol == 0)
            return 0;
        strcpy(dev, def_vol);
        return 1;
    }
    if (devsize < len)
        return 0;
    memcpy(dev, buf, len);
    i++;
    for (j = i; j < 512; j++) {
        if (path[j] == ':' || path[j] == 0)
            break;
        buf[j - i] = path[j];
    }
    buf[j - i] = 0;
    len = strlen(buf) + 1;
    if (fsize < len)
        return 0;
    memcpy(fname, buf, len);
    rofs_to_upper(dev);
    return 1;
}

/* 0 for a bad index. */
/* 0x001da9e8 */
static void *rofs_get_dir_cache(int i)
{
    RofsWork *work;
    int n;
    int nd;

    work = rofs_work;
    n = work->nvol;
    nd = work->ndir;
    if (i < 0 || n < i)
        return 0;
    return (char *)work->vol + n * 32 + i * (nd * 48 + 24);
}

/* "ROFS", or no name with dflt 1, means the default volume; 0 if none. */
/* 0x001daa38 */
static RofsVol *getVolume(const char *name, int dflt)
{
    RofsVol *tab;
    int n;
    int i;

    if (name == 0) {
        if (dflt != 1)
            return 0;
        name = rofs_get_def_volume();
        if (name == 0)
            return 0;
    } else if (strcmp(name, "ROFS") == 0) {
        name = rofs_get_def_volume();
        if (name == 0)
            return 0;
    }
    n = rofs_work->nvol;
    tab = rofs_work->vol;
    for (i = 0; i < n; i++) {
        if (strcmp(name, tab[i].name) == 0)
            return &tab[i];
    }
    return 0;
}

/* New name of 1-8 characters, upper-cased; empty directory record; 0 on failure. */
/* 0x001dab00 */
static RofsVol *addVolList(char *name, void *dev)
{
    RofsVol *tab;
    int n;
    int ndir;
    int i;
    RofsVol *vol;
    void *dirbuf;

    if ((strlen(name) - 1) >= 8)
        return 0;
    n = rofs_work->nvol;
    ndir = rofs_work->ndir;
    tab = rofs_work->vol;
    rofs_to_upper(name);
    if (getVolume(name, 0) != 0)
        return 0;
    for (i = 0; i < n; i++) {
        if (tab[i].name[0] == 0)
            break;
    }
    if (i >= n)
        return 0;
    vol = (RofsVol *)(i * 32 + (long)tab);
    memset(vol, 0, sizeof(RofsVol));
    strncpy(vol->name, name, 8);
    vol->fh = dev;
    dirbuf = rofs_get_dir_cache(i);
    if (dirbuf == 0 || (vol->dirbuf = RFHN_CreateDirhn(dirbuf, ndir)) == 0) {
        rofs_clear_volume(vol);
        return 0;
    }
    return vol;
}

/* Also forgets it as the current volume. */
/* 0x001dac30 */
static void rofs_clear_volume(RofsVol *vol)
{
    if (vol != 0)
        memset(vol, 0, sizeof(RofsVol));
    if (vol == rofs_work->cur)
        rofs_work->cur = 0;
}

/* Volume header (sector 0) to vd, zone header to out; device error, -105 unknown, -207 short. */
/* 0x001dac78 */
static int getCvmfInfo(void *dev, RofsVd *vd, char *out)
{
    char *buf;
    int ret;

    buf = rofs_work->sctbuf;
    ret = rofs_work->io.dev->read(dev, 0, 1, buf);
    if (ret != 2048)
        return ret != 0 ? ret : -207;
    if (rofs_conv_vol_sector(buf, (char *)vd) != 0)
        return -105;
    ret = rofs_work->io.dev->read(dev, vd->tbl[vd->idx], 1, buf);
    if (ret != 2048)
        return ret != 0 ? ret : -207;
    if (rofs_conv_zone_sector(buf, out) != 0)
        return -105;
    return 0;
}

/* Sector 16 of the area; 0 on failure. */
/* 0x001dad70 */
static char *readPvd(void *dev, char *vd)
{
    if (rofs_work->io.dev->read(dev, ROFS_U32(vd + 32) + 16, 1, rofs_work->sctbuf) != 2048)
        return 0;
    return rofs_work->sctbuf;
}

/* From the PVD root record; 0, the error, or -105 when empty. */
/* 0x001dadd0 */
static int readRootDirRec(void *vol, char *pvd, int *out)
{
    char *rec;
    unsigned int lsn;
    unsigned int len;
    int ret;

    rec = pvd + 156;
    lsn = ROFS_U32(rec + 6);
    len = ROFS_U32(rec + 14);
    out[2] = 0;
    ret = rofs_read_dir(vol, rofs_work->sctbuf, ROFS_SW32(lsn), out, ROFS_SW32(len));
    if (ret != 0)
        return ret;
    return out[0] != 0 ? 0 : -105;
}

/* 0x001dae80 */
int rofs_if_init(void *cfg)
{
    return rofs_if_set(cfg);
}

/* Lays out tables, handles and the 64-byte aligned sector buffer; -100 bad config, -203 device. */
/* 0x001daea0 */
static int RFMAI_InitG(RofsCfg *cfg, void *devif)
{
    RofsDev *dev;
    int nhn;
    int nvol;
    int ndir;
    RofsWork *work;
    char *vol;
    char *hn;
    int ret;
    int i;

    if (cfg == 0)
        return -100;
    if (devif == 0)
        return -100;
    if (cfg->work == 0)
        return -100;
    if (cfg->nhn <= 0)
        return -100;
    if (cfg->nvol <= 0)
        return -100;
    if (cfg->ndir < 3)
        return -100;
    rofs_shutdown();
    dev = rofs_get_cd_device();
    if (dev->init() != 0)
        return -203;
    ndir = cfg->ndir;
    nvol = cfg->nvol;
    nhn = cfg->nhn + 1;
    rofs_work = cfg->work;
    memset(rofs_work, 0, 4288 + 56 * nhn + 56 + (ndir * 48 + 56) * nvol);
    {
        RofsWork *base = rofs_work;
        volatile RofsWork *vw = base;

        vol = (char *)base + (4288 + 56 * nhn);
        vw->dirarea = vol + nvol * 32;
        vw->nhn = nhn;
        vw->ndir = ndir;
        vw->hn_area = (char *)base + 4288;
        vw->hn_tbl = (char *)base + 4344;
        vw->stat = 0;
        vw->nvol = nvol;
        vw->count = 0;
        vw->vol = (RofsVol *)vol;
        base->cur = 0;
    }
    ret = RFMAI_SetPfsFuncTblG(pfs_cvfs_get_table());
    if (ret != 0)
        return ret;
    work = rofs_work;
    work->io.dev = dev;
    work->devarg = devif;
    work->sctbuf = (char *)((unsigned int)((char *)work + 128) & ~63);
    work->errfn = 0;
    work->errobj = 0;
    hn = (char *)work + 4288;
    for (i = 0; i < work->nhn; i++, hn += 56) {
        ((RofsHn *)hn)->used = 0;
        ((RofsHn *)hn)->work = rofs_work;
    }
    RFHN_ReopenHn(rofs_work->hn_area, 0, 0, 0, 0);
    rofs_work->stat = 1;
    return 0;
}

/* Closes handles, forgets volumes, stops the device, clears the work. */
/* 0x001db088 */
static void rofs_shutdown(void)
{
    RofsWork *work;
    char *hn;
    RofsVol *vol;
    int i;

    work = rofs_work;
    if (work == 0 || work->stat != 1)
        return;
    hn = (char *)work + 4288;
    for (i = 0; i < rofs_work->nhn; i++, hn += 56)
        rofs_close(hn);
    vol = rofs_work->vol;
    for (i = 0; i < rofs_work->nvol; i++, vol++)
        rofs_clear_volume(vol);
    rofs_work->io.dev->finish();
    rofs_work->stat = 0;
    memset(rofs_work, 0,
                 4288 + 56 * rofs_work->nhn + 56 + (rofs_work->ndir * 48 + 56) * rofs_work->nvol);
    rofs_work = 0;
}

/* Image file table and the device that uses it. */
/* 0x001db190 */
static int RFMAI_SetPfsFuncTblG(void *tbl)
{
    if (tbl == 0)
        return rofs_error("rofs_mai.c", 671, "RFMAI_SetPfsFuncTblG", -100);
    rofs_work->pfs_tbl = tbl;
    rofs_set_pfs_flag(tbl);
    rofs_work->io.pfs = rofs_get_cd_device();
    return 0;
}

/* "ROFS" is reserved (-103). */
/* 0x001db200 */
static int RFMAI_MountG(const char *vname, const char *dev)
{
    if (vname == 0 || dev == 0)
        return rofs_error("rofs_mai.c", 690, "RFMAI_MountG", -100);
    if (strcmp(vname, "ROFS") == 0)
        return rofs_error("rofs_mai.c", 694, "RFMAI_MountG", -103);
    return mountVolume(vname, dev);
}

/* Headers, PVD (ISO 9660), root dir; -104 name in use, -200 busy, -207 read, -105 unusable. */
/* 0x001db2a0 */
static int mountVolume(const char *vname, const char *dev)
{
    RofsHn *hn;
    RofsVol *vol;
    char *pvd;
    char *pvd_ptr;
    int ret;
    int info[476];
    int sub[12];

    if (getVolume(vname, 0) != 0)
        return rofs_error("rofs_mai.c", 714, "mountVolume", -104);
    hn = RFHN_ReopenHn(rofs_work->hn_area, 0, 0, 0, 0);
    if (hn == 0)
        return rofs_error("rofs_mai.c", 722, "RFHN_ReopenHn", -206);
    if (rofs_is_volume_busy(hn, dev) == 1)
        return rofs_error("rofs_mai.c", 726, "mountVolume", -200);
    hn->stat = 1;
    rofs_install_count_cb();
    ret = getCvmfInfo(dev, info, sub);
    if (ret != 0) {
        hn->stat = 2;
        rofs_restore_cb();
        return rofs_error("rofs_mai.c", 738, "getCvmfInfo", ret);
    }
    pvd = readPvd(dev, sub);
    if (pvd == 0) {
        hn->stat = 2;
        rofs_restore_cb();
        return rofs_error("rofs_mai.c", 746, "readPvd", -207);
    }
    rofs_restore_cb();
    hn->stat = 0;
    if (checkMedia(pvd) != 1)
        return rofs_error("rofs_mai.c", 756, "checkMedia", -105);
    vol = addVolList(vname, dev);
    pvd_ptr = pvd;
    pvd_ptr += AZ(pvd_ptr);   /* steer(zero) */
    if (vol == 0) {
        if (pvd_ptr == 0)
            return rofs_error("rofs_mai.c", 761, "addVolList", -105);
        if (AZ(pvd_ptr))
            readPvd(dev, pvd_ptr);
        return rofs_error("rofs_mai.c", 761, "addVolList", -105);
    }
    vol->unk_10 = info[27];
    vol->ptloc = ((RofsPvd *)pvd)->ptloc;
    vol->ptsize = ((RofsPvd *)pvd)->ptsize;
    ret = readRootDirRec(vol, pvd, vol->dirbuf);
    if (ret != -109 && ret != 0) {
        rofs_clear_volume(vol);
        return rofs_error("rofs_mai.c", 780, "readRootDirRec", ret);
    }
    return ret;
}

/* "ROFS" is reserved (-103). */
/* 0x001db518 */
static int RFMAI_UnMountG(const char *vname)
{
    if (vname == 0)
        return rofs_error("rofs_mai.c", 792, "RFMAI_UnMountG", -100);
    if (strcmp(vname, "ROFS") == 0)
        return rofs_error("rofs_mai.c", 797, "RFMAI_UnMountG", -103);
    return rofs_unmount(vname);
}

/* Closes handles on its image; -105 unknown. The walk only passes open handles with a volume. */
/* 0x001db5a0 */
static int rofs_unmount(const char *vname)
{
    RofsVol *vol;
    RofsHn *hn;
    int i;

    vol = getVolume(vname, 0);
    if (vol == 0)
        return rofs_error("rofs_mai.c", 814, "getVolume", -105);
    if (vol->fh) {
        hn = rofs_work->hns;
        for (i = 0; i < rofs_work->nhn; i++) {
            if (hn->used == 1 && hn->vol) {
                if (hn->vol->fh == vol->fh)
                    rofs_close(hn);
                hn++;
            }
        }
    }
    rofs_clear_volume(vol);
    return 0;
}

/* -103 for "ROFS" or an unknown volume. */
/* 0x001db690 */
static int RFMAI_SetDefVolumeG(const char *vname)
{
    RofsVol *vol;

    if (vname == 0)
        return rofs_error("rofs_mai.c", 844, "RFMAI_SetDefVolumeG", -100);
    if (strcmp(vname, "ROFS") == 0)
        return rofs_error("rofs_mai.c", 849, "RFMAI_SetDefVolumeG", -103);
    vol = getVolume(vname, 0);
    if (vol == 0)
        return rofs_error("rofs_mai.c", 855, "getVolume", -103);
    rofs_work->cur = vol;
    return 0;
}

/* 0x001db750 */
static RofsVol *rofs_get_def_volume(void)
{
    return rofs_work->cur;
}

/* "volume:name" in dir (same volume) or the current one; reports and returns 0 on failure. */
/* 0x001db760 */
static RofsHn *RFMAI_OpenG(const char *path, char *dir)
{
    RofsVol *vol;
    char *dirbuf;
    int idx;
    int i;
    RofsHn *hn;
    RofsDirEnt *ent;
    char vname[16];
    char fname[32];

    if (divideFname(vname, fname, 9, 32, path) == 0) {
        rofs_error("rofs_mai.c", 888, "divideFname", -100);
        return 0;
    }
    vol = getVolume(vname, 0);
    if (vol == 0) {
        rofs_error("rofs_mai.c", 894, "getVolume", -105);
        return 0;
    }
    if (dir) {
        dirbuf = dir;
        if (strcmp(vol->name, dirbuf + 12) != 0) {
            rofs_error("rofs_mai.c", 906, "dir_buf", -108);
            return 0;
        }
    } else {
        dirbuf = vol->dirbuf;
    }
    idx = RFDIR_SearchName(fname, dirbuf, 0);
    if (idx == 0) {
        rofs_error("rofs_mai.c", 914, "RFDIR_SearchName", -107);
        return 0;
    }
    hn = rofs_work->hn_tbl;
    for (i = 0; i < rofs_work->nhn - 1; i++) {
        if (hn->used == 0) {
            ent = (RofsDirEnt *)(dirbuf + 24);
            ent += idx - 1;
            RFHN_CreateHnG(hn, idx, ent->lsn, ent->size, vol);
            if (hn == 0)
                rofs_error("rofs_mai.c", 928, "RFHN_CreateHnG", -102);
            return hn;
        }
        hn++;
    }
    rofs_error("rofs_mai.c", 935, "RFMAI_OpenG", -102);
    return 0;
}

/* Default volume when unnamed; reports and returns 0 on failure. */
/* 0x001db928 */
static RofsHn *RFMAI_OpenRangeG(const char *vname, int lsn, int nsct)
{
    RofsVol *vol;
    int i;
    RofsHn *hn;

    if (nsct < 0 || lsn < 0) {
        rofs_error("rofs_mai.c", 950, "RFMAI_OpenRangeG", -100);
        return 0;
    }
    vol = getVolume(vname, 1);
    if (vol == 0) {
        rofs_error("rofs_mai.c", 957, "getVolume", -105);
        return 0;
    }
    hn = rofs_work->hn_tbl;
    for (i = 0; i < rofs_work->nhn - 1; i++) {
        if (hn->used == 0) {
            RFHN_CreateHnG(hn, 0, lsn, nsct << 11, vol);
            if (hn == 0)
                rofs_error("rofs_mai.c", 968, "RFHN_CreateHnG", -102);
            return hn;
        }
        hn++;
    }
    rofs_error("rofs_mai.c", 975, "RFMAI_OpenRangeG", -102);
    return 0;
}

/* Part done: next or finish; busy: records bytes; device error/short: -202. Device state. */
/* 0x001dba68 */
static int updateHnStat(RofsHn *hn)
{
    void *fh;
    int st;
    int n;
    int pos;

    if (hn == 0 || hn->vol == 0)
        return 0;
    fh = hn->vol->fh;
    st = rofs_work->io.pfs->seek(fh);
    if (fh) {
        switch (st) {
        case 0:
        case 1:
            n = rofs_work->io.pfs->req_rd(fh);
            if (n != hn->rqsct << 11) {
                hn->trstat = 3;
                hn->err = -202;
                goto report;
            }
            pos = hn->pos + hn->rqsct;
            hn->ntr = 0;
            hn->pos = pos;
            if (pos < hn->nsct) {
                n = hn->lsn + hn->sct_ofs + pos;
                st = hn->work->io.pfs->get_stat(hn->vol->unk_10, fh, n, hn->rqsct,
                                        rofs_work->sctbuf);
            } else {
                hn->trstat = st;
                hn->stat = 0;
                hn->sct_ofs += hn->nsct;
            }
            break;
        case 2:
            n = rofs_work->io.pfs->req_rd(fh);
            if (n < 0) {
                hn->trstat = 3;
                hn->err = -202;
                goto report;
            }
            hn->ntr = n;
            break;
        case 3:
            hn->stat = 2;
            hn->err = -202;
            hn->trstat = st;
        report:
            if (rofs_work->errfn)
                rofs_error("rofs_mai.c", 1063, "updateHnStat", hn->err);
        default:
            hn->trstat = (unsigned char)st;
            hn->stat = 2;
            break;
        }
    } else {
        rofs_error("rofs_mai.c", 1074, "updateHnStat", -1000);
    }
    return st;
}

/* Polls reading handles; counts calls (the watchdog clock). */
/* 0x001dbc30 */
static void rofs_exec_server(void)
{
    int i;
    RofsHn *hn;

    hn = rofs_work->hns;
    for (i = 0; i < rofs_work->nhn; i++) {
        if (hn->stat == 1)
            updateHnStat(hn);
        hn++;
    }
    rofs_work->count++;
}

/* From the current server count. */
/* 0x001dbcc0 */
void rofs_reset_watchdog(void)
{
    memset(rofs_watchdog, 0, 12);
    rofs_watchdog[0] = rofs_work->count;
}

/* Every 2048 server calls: has fh transferred more? 1 progressing, 0 stalled. */
/* 0x001dbd00 */
int rofs_check_watchdog(void *fh)
{
    unsigned int cnt;
    int n;
    int *watch;

    if (fh == 0)
        return 0;
    cnt = rofs_work->count;
    watch = rofs_watchdog;
    if (cnt - watch[0] > 2048) {
        n = rofs_work->io.pfs->req_rd(fh);
        if (n - watch[2] <= 0)
            return 0;
        watch[2] = n;
        rofs_watchdog[0] = cnt;
    }
    if ((unsigned int)rofs_watchdog[1] > 2048) {
        if (cnt == 0)
            return 0;
        rofs_watchdog[1] = 0;
    }
    rofs_watchdog[1]++;
    return 1;
}

/* "volume:name" in dir (same volume, else -108) or the current one; -107 not found. */
/* 0x001dbdb8 */
static int RFMAI_GetFileSiz(const char *path, char *dir)
{
    RofsVol *vol;
    int idx;
    char *dbuf;
    char vname[16];
    char fname[32];

    if (divideFname(vname, fname, 9, 32, path) == 0)
        return rofs_error("rofs_mai.c", 1159, "divideFname", -100);
    vol = getVolume(vname, 0);
    if (vol == 0)
        return rofs_error("rofs_mai.c", 1164, "getVolume", -105);
    if (dir == 0) {
        dbuf = vol->dirbuf;
    } else {
        if (strcmp(vol->name, dir + 12) != 0)
            return rofs_error("rofs_mai.c", 1173, "RFMAI_GetFileSiz", -108);
        dbuf = dir;
    }
    idx = RFDIR_SearchName(fname, dbuf, 0);
    if (idx == 0)
        return rofs_error("rofs_mai.c", 1182, "RFDIR_SearchName", -107);
    return ((RofsDirEnt *)(dbuf + 24))[idx - 1].size;
}

/* Volume and image file; zeros and -105 when unknown. */
/* 0x001dbee0 */
static int RFMAI_GetVolumeInfG(const char *vname, int *info)
{
    RofsVol *vol;

    if (info == 0)
        return rofs_error("rofs_mai.c", 1199, "RFMAI_GetVolumeInfG", -100);
    vol = getVolume(vname, 1);
    if (vol == 0) {
        info[0] = 0;
        info[1] = 0;
        return -105;
    }
    info[0] = (int)vol;
    info[1] = (int)vol->fh;
    return 0;
}

/* Absolute: path table; relative: current dir (-106 if not a dir). Into dir or the own record. */
/* 0x001dbf58 */
static int RFMAI_LoadDirG(const char *path, char *dir, int size)
{
    RofsVol *vol;
    int lsn;
    int idx;
    int len;
    RofsDirEnt *ent;
    char vname[16];
    char fname[512];

    if (divideFname(vname, fname, 9, 512, path) == 0)
        return rofs_error("rofs_mai.c", 1228, "divideFname", -100);
    vol = getVolume(vname, 0);
    if (vol == 0)
        return rofs_error("rofs_mai.c", 1234, "getVolume", -105);
    if (dir) {
        dir = RFHN_CreateDirhn(dir, size);
        if (dir == 0)
            return rofs_error("rofs_mai.c", 1245, "RFHN_CreateDirhn", -100);
    } else {
        dir = vol->dirbuf;
    }
    if (fname[0] == '/' || fname[0] == '\\') {
        lsn = RFDIR_SearchPath(vol, rofs_work->sctbuf + 64, vol->ptloc, vol->ptsize, fname);
        if (lsn <= 0)
            return rofs_error("rofs_mai.c", 1254, "RFDIR_SearchPath", -107);
        len = 2048;
    } else {
        idx = RFDIR_SearchName(fname, vol->dirbuf, 0);
        if (idx == 0)
            return rofs_error("rofs_mai.c", 1266, "RFDIR_SearchName", -107);
        ent = (RofsDirEnt *)(vol->dirbuf + idx * 48);
        ent = (RofsDirEnt *)((char *)ent - 24);
        if ((ent->flags & 2) == 0)
            return rofs_error("rofs_mai.c", 1275, "ROFS_ATR_DIRECTORY", -106);
        lsn = ent->lsn;
        len = ent->size;
    }
    return rofs_read_dir(vol, rofs_work->sctbuf, lsn, dir, len);
}

/* Copies (as many as fit); -109 entries dropped, -108 another volume's record. */
/* 0x001dc160 */
static int rofs_set_dir_rec(const char *vname, RofsDir *dir)
{
    RofsVol *vol;
    RofsDir *drec;
    int n;
    int max;
    int i;
    int cnt;
    RofsDirEnt *src;
    RofsDirEnt *dst;

    if (dir == 0)
        return rofs_error("rofs_mai.c", 1299, "dir_buf", -100);
    vol = getVolume(vname, 1);
    if (vol == 0)
        return rofs_error("rofs_mai.c", 1305, "getVolume", -105);
    if (strcmp(vol->name, dir->name) != 0)
        return rofs_error("rofs_mai.c", 1311, "dir_buf", -108);
    drec = (RofsDir *)vol->dirbuf;
    n = dir->n;
    max = drec->max;
    drec->unk_08 = dir->unk_08;
    cnt = (n < max) ? n : max;
    src = dir->ent;
    dst = drec->ent;
    for (i = 0; i < cnt; i++, dst++, src++)
        *dst = *src;
    drec->n = i;
    return (max < n) ? -109 : 0;
}

/* "volume:name" in the volume's current directory. */
/* 0x001dc2e0 */
static int rofs_is_exist_file(const char *path)
{
    RofsVol *vol;
    char vname[16];
    char fname[32];

    if (divideFname(vname, fname, 9, 32, path) == 0) {
        rofs_error("rofs_mai.c", 1352, "divideFname", -100);
        return 0;
    }
    vol = getVolume(vname, 0);
    if (vol == 0) {
        rofs_error("rofs_mai.c", 1358, "getVolume", -105);
        return 0;
    }
    return RFDIR_SearchName(fname, vol->dirbuf, 0) != 0;
}

/* Without "." and ".."; -105 unknown or unmounted. */
/* 0x001dc388 */
static int RFMAI_GetNumFiles(const char *vname)
{
    RofsVol *vol;

    vol = getVolume(vname, 1);
    if (vol == 0)
        return rofs_error("rofs_mai.c", 1382, "getVolume", -105);
    if (vol->fh == 0)
        return rofs_error("rofs_mai.c", 1385, "RFMAI_GetNumFiles", -105);
    return ((RofsDir *)vol->dirbuf)->n - 2;
}

/* Up to n (name, size, flags); -100 bad arguments, -105 unknown volume. */
/* 0x001dc400 */
static int RFMAI_GetDirInfG(const char *vname, RofsDirInf *inf, int n)
{
    RofsVol *vol;
    RofsDir *dir;
    RofsDirEnt *ent;
    int cnt;
    int i;

    if (n < 0 || inf == 0)
        return rofs_error("rofs_mai.c", 1404, "RFMAI_GetDirInfG", -100);
    vol = getVolume(vname, 1);
    if (vol == 0)
        return rofs_error("rofs_mai.c", 1408, "getVolume", -105);
    dir = (RofsDir *)vol->dirbuf;
    cnt = dir->n;
    if (n < cnt)
        cnt = n;
    ent = dir->ent;
    for (i = 0; i < cnt; i++) {
        inf->name = ent->name;
        inf->size = ent->size;
        inf->flags = ent->flags;
        ent++;
        inf++;
    }
    return cnt;
}

/* 0x001dc4d8 */
static void rofs_set_error_cb(void (*fn)(void *obj, const char *msg, int code), void *obj)
{
    if (rofs_work) {
        rofs_work->errfn = fn;
        rofs_work->errobj = obj;
    }
}

/* On ROFS's own handle; sectors read or an error. */
/* 0x001dc4f8 */
int RFMAI_ReadWG(RofsVol *vol, int lsn, int nsct, void *buf)
{
    RofsHn *hn;
    int ret;
    int err;

    if (vol == 0 || buf == 0)
        return rofs_error("rofs_mai.c", 1457, "RFMAI_ReadWG", -100);
    if (vol->fh == 0)
        return rofs_error("rofs_mai.c", 1460, "RFMAI_ReadWG", -100);
    hn = RFHN_ReopenHn(rofs_work->hn_area, 0, lsn, nsct << 11, vol);
    if (hn == 0)
        return rofs_error("rofs_mai.c", 1466, "RFMAI_ReadWG", -200);
    ret = ROFS_ReqRd(hn, nsct, buf);
    if (ret < 0)
        return rofs_error("rofs_mai.c", 1473, "ROFS_ReqRd", ret);
    err = RFHN_WaitCompleteReq(hn);
    if (err != 0)
        return rofs_error("rofs_mai.c", 1479, "RFHN_WaitCompleteReq", err);
    return ret;
}

/* "file#msg:line" to the error callback; returns err (-203 when not set up). */
/* 0x001dc638 */
int rofs_error(const char *msg, int line, const char *file, int err)
{
    if (rofs_work == 0)
        return -203;
    if (rofs_work->errfn != 0) {
        sprintf(rofs_error_buf, "%s#%s:%d", file, msg, (short)line);
        rofs_work->errfn(rofs_work->errobj, rofs_error_buf, err);
    }
    return err;
}

/* 0x001dc6c8 */
void rofs_to_upper(char *s)
{
    int n;
    int i;

    if (s == 0)
        return;
    n = strlen(s) + 1;
    for (i = 0; i < n; i++) {
        if ((unsigned char)s[i] >= 'a' && (unsigned char)s[i] <= 'z')
            s[i] -= 32;
    }
}

/* Server call count. */
/* 0x001dc738 */
static int rofs_get_work10(void)
{
    return rofs_work->count;
}

/* 0x001dc748 */
static int rofs_dcf_init(void)
{
    return 0;
}

/* 0x001dc750 */
static void rofs_dcf_finish(void)
{
}
