/*
 * ADX file access: sixteen handles reading sectors into a buffer or stream joint via ADXSTM; AFS
 * partitions (open by partition and file id); ROFS device and volumes for cvFs; a call log.
 */

#include "cri.h"
#include "libc.h"
#include "adxf.h"
#include "adx_error.h"
#include "adx_lock.h"
#include "adxstm.h"
#include "sjrbf.h"

static void adxf_close_all(void);
static int ADXF_Stop(AdxfObj *adxf);
static int ADXF_GetNumReadSct(AdxfObj *adxf);

extern int cvFsAddDev(const char *dev, void *getif, int arg);
extern int cvFsAddVolume(const char *dev, const char *vol, void *img);
extern void cvFsClose(void *fd);
extern int cvFsDelDev(const char *dev);
extern int cvFsDelVolume(const char *dev, const char *vol);
extern int cvFsGetVolumeInfo(const char *dev, const char *vol, void *info);
extern void *cvFsOpen(const char *fname, void *dir, int mode);
extern int cvFsSetDefDev(const char *dev);

typedef struct {
    unsigned char ptid;
    unsigned char kind;
    unsigned short count;
    int arg1;
    int arg2;
    int arg3;
} AdxfLoadRec;

extern void *rofs_get_cvfs_rec(void);
extern void cvfs_exec_server(void);

/* First call clears handles, partitions and the call log; counts calls. */
/* 0x001c3c68 */
void adxf_lib_init(void)
{
    adxf_ver_str;
    if (adxf_init_count == 0) {
        memset(adxf_obj_tbl, 0, 4992);
        memset(adxf_pt_info_tbl, 0, 1024);
        memset(adxf_history_ring, 255, 4096);
        memset(adxf_pt_load_count, 0, 32);
        adxf_history_idx = 0;
        adxf_read_dcache_flag = 0;
        adxf_ptload_stat = 0;
        adxf_ptload_handle = 0;
        adxf_ptload_ptid = -1;
    }
    adxf_init_count++;
}

/* Last user: closes every handle, clears the tables. */
/* 0x001c3d28 */
void adxf_lib_finish(void)
{
    if (--adxf_init_count == 0) {
        adxf_close_all();
        adxf_ptload_ptid = -1;
        adxf_ptload_handle = 0;
        adxf_ptload_stat = 0;
        adxf_read_dcache_flag = 0;
        adxf_history_idx = 0;
        memset(adxf_pt_load_count, 0, 32);
        memset(adxf_history_ring, 255, 4096);
        memset(adxf_pt_info_tbl, 0, 1024);
        memset(adxf_obj_tbl, 0, 4992);
    }
}

/* 0x001c3de0 */
void adxf_rofs_add_device(void)
{
    cvFsAddDev("ROFS", rofs_get_cvfs_rec, 0);
}

/* 0x001c3df8 */
void adxf_rofs_del_device(void)
{
    cvFsDelDev("ROFS");
}

/* Image file fname as ROFS volume vol; 0, -3 bad arguments, -1 open/add failed. */
/* 0x001c3e08 */
int adxf_rofs_add_volume(const char *vol, const char *fname)
{
    void *fd;

    if (vol == 0)
        return -3;
    if (fname == 0)
        return -3;
    if (strcmp(vol, "ROFS") == 0)
        return -3;
    fd = cvFsOpen(fname, 0, 0);
    if (fd == 0)
        return -1;
    if (cvFsAddVolume("ROFS", vol, fd) < 0) {
        cvFsClose(fd);
        return -1;
    }
    return 0;
}

/* Also closes its image file. */
/* 0x001c3eb0 */
static void adxf_rofs_del_volume(const char *vol)
{
    CvFsVolInfo info;

    if (vol == 0)
        return;
    if (cvFsGetVolumeInfo("ROFS", vol, &info) < 0)
        return;
    cvFsDelVolume("ROFS", vol);
    cvFsClose(info.img);
}

/* 0x001c3f10 */
static int adxf_rofs_is_volume(const char *vol)
{
    CvFsVolInfo info;

    if (cvFsGetVolumeInfo("ROFS", vol, &info) < 0)
        return 0;
    return strcmp(info.name, vol) == 0;
}

/* Default device, unless vol is "ROFS" itself. */
/* 0x001c3f60 */
void adxf_rofs_set_def_volume(const char *vol)
{
    if (vol == 0)
        return;
    if (strcmp(vol, "ROFS") == 0)
        return;
    cvFsSetDefDev(vol);
}

/*
 * 256-entry call log: call (1 open, 2 open in archive, 3 close, 4 read, 5 stop, 6 seek), start (0)
 * or done (1), that call's start count, up to three arguments.
 */
/* 0x001c3fa8 */
static void adxf_history_add(int ptid, int kind, int target, int param, int extra)
{
    AdxfLoadRec *rec;

    adxf_history_idx %= 256;
    rec = &((AdxfLoadRec *)adxf_history_ring)[adxf_history_idx];
    if (kind == 0)
        adxf_pt_load_count[ptid]++;
    adxf_history_idx++;
    rec->ptid = ptid;
    rec->kind = kind;
    rec->count = adxf_pt_load_count[ptid];
    rec->arg1 = target;
    rec->arg2 = param;
    rec->arg3 = extra;
}

/* Busy-wait between server polls. */
/* 0x001c4048 */
static void adxf_delay(void)
{
    int i;

    for (i = 50000; i != 0; i--)
        ;
}

/* ptid < 256 and info pointer; reports and returns -3 on error. */
/* 0x001c4078 */
static int adxf_ChkPrmPt(unsigned int ptid, void *ptinfo)
{
    if (ptid >= 256) {
        adx_error("E9040801:'ptid' is range outside.(adxf_ChkPrmPt)");
        return -3;
    }
    if (ptinfo == 0) {
        adx_error("E9040802:'ptinfo' is NULL.(adxf_ChkPrmPt)");
        return -3;
    }
    return 0;
}

/* Retries the request, then runs the file servers (up to 10000 polls each); 0 if never started. */
/* 0x001c40c0 */
static int adxf_read_sync(void *adxf, int nsct, void *buf)
{
    int i;

    for (i = 0; i < 10000 && ADXF_ReadNw32(adxf, nsct, buf) <= 0; i++) {
        cvfs_exec_server();
        adxstm_exec_server();
        adxf_exec_server();
        adxf_delay();
    }
    if (i == 10000)
        return 0;
    for (i = 0; i < 10000; i++) {
        cvfs_exec_server();
        adxstm_exec_server();
        adxf_exec_server();
        if (ADXF_GetStat(adxf) == 3)
            break;
        adxf_delay();
    }
    return ADXF_GetNumReadSct(adxf);
}

/* Reads the AFS header (at most nmax files) into first-sector and size tables; -4 broken header. */
/* 0x001c41b0 */
static int ADXF_LoadPartitionEx(int ptid, const char *fname, AdxfPtInfo *pt, int nmax)
{
    int ret;
    int err;
    int i;
    int k;
    int n;
    int nf;
    void *adxf;
    unsigned char *buf;
    short *sct_tbl;
    short *tbl;

    ret = adxf_ChkPrmPt(ptid, pt);
    if (ret < 0)
        return ret;
    adxf = ADXF_Open(fname, 0);
    if (adxf == 0) {
        adx_error("E9040804:can't open file.(ADXF_LoadPartition)");
        return -1;
    }
    memset(pt, 0, 280);
    tbl = pt->fsct;
    pt->next = 0;
    ((AdxfPtInfo **)adxf_pt_info_tbl)[ptid] = pt;
    strncpy(pt->fname, fname, 256);
    ret = -1;
    k = 0;
    pt->added = 0;
    adxf_work_ptr2 = adxf_work_ptr = (unsigned char *)((unsigned int)adxf_work_buf & 0xffffffc0);
    pt->nfile = 0;
    err = 0;
    while (err < 3) {
        if (adxf_read_sync(adxf, 1, adxf_work_ptr2) > 0) {
            if (pt->nfile == 0) {
                buf = adxf_work_ptr2;
                n = LE32(buf + 4);
                if (n > 0x10000) {
                    *(int *)(buf + 4) = 0x10000;
                    ret = -4;
                    goto end;
                }
                nf = (nmax < n) ? nmax : n;
                i = 3;
                pt->nfile16 = nf;
                pt->nfile = (unsigned short)nf;
                pt->size = ((unsigned short)nf + 140U) / 2 * 4;
                tbl[0] = LE32(buf + 8) / 2048;
            } else {
                i = 1;
            }
            for (; i < 512; i += 2) {
                k++;
                tbl[k] = (LE32(adxf_work_ptr2 + i * 4) + 2047) / 2048;
                if (k >= pt->nfile) {
                    ret = 0;
                    goto end;
                }
            }
            err = 0;
        } else {
            err++;
        }
    }
    if (err == 3)
        adx_error("E9040805:read error.(ADXF_LoadPartition)");
end:
    if (ret == -4)
        adx_error("E9040806:file is broken.(ADXF_LoadPartition)");
    adxf_close_handle(adxf);
    return ret;
}

/* As ADXF_LoadPartitionEx without a file limit; keeps dir for later opens. */
/* 0x001c4488 */
static int ADXF_LoadPartition(int ptid, const char *fname, void *dir, AdxfPtInfo *pt)
{
    int ret;
    int err;
    int i;
    int k;
    int n;
    void *adxf;
    unsigned char *buf;
    short *sct_tbl;
    short *tbl;

    ret = adxf_ChkPrmPt(ptid, pt);
    if (ret < 0)
        return ret;
    adxf = ADXF_Open(fname, dir);
    if (adxf == 0) {
        adx_error("E9040804:can't open file.(ADXF_LoadPartition)");
        return -1;
    }
    memset(pt, 0, 280);
    tbl = pt->fsct;
    pt->next = 0;
    ((AdxfPtInfo **)adxf_pt_info_tbl)[ptid] = pt;
    strncpy(pt->fname, fname, 256);
    ret = -1;
    k = 0;
    pt->added = 0;
    pt->dir = dir;
    adxf_work_ptr2 = adxf_work_ptr = (unsigned char *)((unsigned int)adxf_work_buf & 0xffffffc0);
    pt->nfile = 0;
    err = 0;
    while (err < 3) {
        if (adxf_read_sync(adxf, 1, adxf_work_ptr2) > 0) {
            if (pt->nfile == 0) {
                buf = adxf_work_ptr2;
                n = LE32(buf + 4);
                if (n > 0x10000) {
                    *(int *)(buf + 4) = 0x10000;
                    adx_error("E9040806:file is broken.(ADXF_LoadPartition)");
                    ret = -4;
                    goto end;
                }
                i = 3;
                pt->nfile = pt->nfile16 = buf[4] | (buf[5] << 8);
                pt->size = (pt->nfile + 140U) / 2 * 4;
                tbl[0] = LE32(buf + 8) / 2048;
            } else {
                i = 1;
            }
            for (; i < 512; i += 2) {
                k++;
                tbl[k] = (LE32(adxf_work_ptr2 + i * 4) + 2047) / 2048;
                if (k >= pt->nfile) {
                    ret = 0;
                    goto end;
                }
            }
            err = 0;
        } else {
            err++;
        }
    }
    if (err == 3)
        adx_error("E9040805:read error.(ADXF_LoadPartition)");
end:
    adxf_close_handle(adxf);
    return ret;
}

/* Non-blocking: requests the first header sector; ADXF_GetPtStat completes. One at a time. */
/* 0x001c4750 */
static int ADXF_LoadPartitionNw(int ptid, const char *fname, void *dir, AdxfPtInfo *pt)
{
    int ret;
    int st;

    st = adxf_ChkPrmPt(ptid, pt);
    if (st < 0)
        return st;
    if (adxf_ptload_ptid >= 0) {
        adx_error("E0042401:multi-load partition.(ADXF_LoadPartitionNw)");
        return -1;
    }
    adxf_ptload_handle = ADXF_Open(fname, dir);
    if (adxf_ptload_handle == 0) {
        adx_error("E9040804:can't open file.(ADXF_LoaddPartitionNw)");
        return -1;
    }
    adxf_ptload_ptid = ptid;
    adxf_ptload_stat = 0;
    memset(pt, 0, 280);
    pt->next = 0;
    ((AdxfPtInfo **)adxf_pt_info_tbl)[ptid] = pt;
    strncpy(pt->fname, fname, 256);
    pt->added = 0;
    pt->dir = dir;
    adxf_work_ptr = adxf_work_ptr2 = (unsigned char *)((unsigned int)adxf_work_buf & 0xffffffc0);
    ret = ADXF_ReadNw32(adxf_ptload_handle, 1, adxf_work_ptr2);
    if (ret < 0) {
        adxf_close_handle(adxf_ptload_handle);
        return ret;
    }
    pt->nfile = 0;
    return 0;
}

/* Closes the background load handle, resets the load state. */
/* 0x001c4898 */
static void adxf_ptload_end(void)
{
    adxf_close_handle(adxf_ptload_handle);
    adxf_ptload_handle = 0;
    adxf_ptload_ptid = -1;
    adxf_ptload_stat = 0;
}

/* 0x001c48d8 */
static void adxf_ptload_abort(void)
{
    if (adxf_ptload_handle == 0 || adxf_ptload_ptid < 0)
        return;
    if (ADXF_GetStat(adxf_ptload_handle) != 1)
        ADXF_Stop(adxf_ptload_handle);
    adxf_ptload_end();
}

/* Polls a background load: checks each header sector, adds sizes; 3 done, 4 error. */
/* 0x001c4940 */
static int ADXF_GetPtStat(int ptid)
{
    AdxfPtInfo *pt;
    int stat;
    int i;
    unsigned char *buf;
    short *sct_tbl;
    unsigned short nf;

    if (ptid != adxf_ptload_ptid) {
        adx_error("E0041303:illigal parameter 'ptid'.(ADXF_GetPtStat)");
        return -3;
    }
    stat = ADXF_GetStat(adxf_ptload_handle);
    if (stat != 3)
        return stat;
    pt = ((AdxfPtInfo **)adxf_pt_info_tbl)[ptid];
    sct_tbl = &pt->fsct[1];
    if (pt->nfile == 0) {
        if (memcmp(adxf_work_ptr2, "AFS", 3) != 0) {
            adx_error("E0040701:Illigal format(not AFS).(ADXF_GetPtStat)");
            adxf_ptload_end();
            return 4;
        }
        buf = adxf_work_ptr2;
        if (LE32(buf + 4) > 0x10000) {
            adx_error("E0040702:Illigal number of file.(ADXF_GetPtStat)");
            adxf_ptload_end();
            return 4;
        }
        i = 3;
        pt->nfile = pt->nfile16 = buf[4] | (buf[5] << 8);
        pt->size = (pt->nfile + 140U) / 2 * 4;
        pt->fsct[0] = LE32(buf + 8) / 2048;
    } else {
        i = 1;
    }
    for (; i < 512; i += 2) {
        sct_tbl[adxf_ptload_stat++] = (LE32(adxf_work_ptr2 + i * 4) + 2047) / 2048;
        if (adxf_ptload_stat >= pt->nfile) {
            adxf_ptload_end();
            stat = 3;
            break;
        }
    }
    if (i >= 512) {
        if (ADXF_ReadNw32(adxf_ptload_handle, 1, adxf_work_ptr2) < 0) {
            adxf_ptload_end();
            stat = 4;
        } else {
            stat = ((AdxfObj *)adxf_ptload_handle)->stat;
        }
    }
    return stat;
}

/* AFS archive linked over partition ptid; its non-empty files replace same-id files. */
/* 0x001c4bc8 */
static int ADXF_AddPartition(int ptid, const char *fname, AdxfPtInfo *pt, int nmax)
{
    int ret;
    int err;
    int i;
    int k;
    int cnt;
    int n;
    void *adxf;
    unsigned char *buf;
    AdxfPtInfo *prev;
    unsigned char *scan;
    AdxfPtEnt *new_ent;
    AdxfPtEnt *ent;

    ret = adxf_ChkPrmPt(ptid, pt);
    if (ret < 0)
        return ret;
    adxf = ADXF_Open(fname, 0);
    if (adxf == 0) {
        adx_error("E9040808:can't open.(ADXF_AddPartition)");
        return -1;
    }
    pt->next = (int)((AdxfPtInfo **)adxf_pt_info_tbl)[ptid];
    ((AdxfPtInfo **)adxf_pt_info_tbl)[ptid] = pt;
    strncpy(pt->fname, fname, 256);
    ret = -1;
    ent = (AdxfPtEnt *)pt->fsct;
    pt->added = 1;
    adxf_work_ptr2 = adxf_work_ptr = (unsigned char *)((unsigned int)adxf_work_buf & 0xffffffc0);
    k = 0;
    cnt = 0;
    pt->nfile = 0;
    err = 0;
    while (err < 3) {
        if (adxf_read_sync(adxf, 1, adxf_work_ptr2) > 0) {
            i = 0;
            if (pt->nfile == 0) {
                buf = adxf_work_ptr2;
                n = LE32(buf + 4);
                if (n > 0x10000) {
                    *(int *)(buf + 4) = 0x10000;
                    adx_error("E9040810:file is broken.(ADXF_LoadPartition)");
                    ret = -4;
                    goto end;
                }
                pt->nfile = (nmax < n) ? nmax : n;
                i = 2;
            }
            for (; i < 512; ) {
                if (LE32(adxf_work_ptr2 + i * 4) == 0) {
                    i += 2;
                } else {
                    new_ent = &ent[cnt++];
                    new_ent->flid = k;
                    new_ent->ofst = (LE32(adxf_work_ptr2 + i * 4) + 2047) / 2048;
                    i += 4;
                    new_ent->nsct = (LE32(adxf_work_ptr2 + i * 4) + 2047) / 2048;
                    i += 4;
                }
                k++;
                if (k >= pt->nfile) {
                    ret = 0;
                    goto end;
                }
            }
            err = 0;
        } else {
            err++;
        }
    }
    if (err == 3)
        adx_error("E9040809:read error.(ADXF_LoadPartition)");
end:
    prev = (AdxfPtInfo *)pt->next;
    pt->nfile16 = cnt;
    pt->size = prev->size + (276 + pt->nfile16 * 8);
    adxf_close_handle(adxf);
    return ret;
}

/* 0x001c4ee0 */
static int adxf_get_pt_rec_size(int ptid)
{
    return ((AdxfPtInfo **)adxf_pt_info_tbl)[ptid]->size;
}

/* 0 if none. */
/* 0x001c4f00 */
static AdxfObj *adxf_find_free_handle(void)
{
    AdxfObj *adxf;
    int i;

    adxf = 0;
    for (i = 0; i < 16; i++) {
        if (adxf_obj_tbl[i].used == 0) {
            adxf = &adxf_obj_tbl[i];
            break;
        }
    }
    return adxf;
}

/* Defaults: idle, 512-sector request limit. */
/* 0x001c4f58 */
static AdxfObj *adxf_alloc_handle(void)
{
    AdxfObj *adxf;

    adxf = adxf_find_free_handle();
    if (adxf == 0)
        return 0;
    adxf->maxsct = 512;
    adxf->stat = 1;
    adxf->used = 1;
    adxf->stm = 0;
    adxf->rqsct = 0;
    adxf->rqnsct = 0;
    adxf->nreadsct = 0;
    adxf->sjflag = 0;
    adxf->sj = 0;
    return adxf;
}

/* Also records the file size. */
/* 0x001c4fb8 */
static int adxf_open_stream(AdxfObj *adxf, const char *fname, void *dir)
{
    void *stm;
    int size;

    if (fname == 0) {
        adx_error("E9081901:illigal parameter fname=null.(ADXF_Open)");
        return -1;
    }
    stm = ADXSTM_OpenFnameEx(fname, dir, 0);
    if (stm == 0) {
        adx_error("E0110901:can't open file.(ADXF_Open)");
        return -1;
    }
    adxf->stm = stm;
    size = adxstm_get_fsize_byte(stm);
    adxf->fsize_byte = size;
    adxf->fsize_sct = (size + 2047) / 2048;
    return 0;
}

/* 0 on failure. */
/* 0x001c5048 */
AdxfObj *ADXF_Open(const char *fname, void *dir)
{
    AdxfObj *adxf;

    adxf_history_add(1, 0, (int)fname, (int)dir, -1);
    adx_lock();
    adxf = adxf_alloc_handle();
    if (adxf != 0) {
        if (adxf_open_stream(adxf, fname, dir) < 0) {
            adxf_close_handle(adxf);
            adxf = 0;
        }
    }
    adx_unlock();
    adxf_history_add(1, 1, (int)fname, (int)dir, -1);
    return adxf;
}

/* 0x001c50f8 */
static int adxf_open_afs_stream(AdxfObj *adxf, int ptid, int flid)
{
    char fname[256];
    void *dir;
    int ofst;
    int nsct;
    void *stm;

    if (adxf_get_pt_file_info_dir(ptid, flid, fname, &dir, &ofst, &nsct) < 0)
        return -3;
    adxf->ofst = ofst;
    stm = ADXSTM_OpenFnameRange(fname, dir, ofst, nsct, 0);
    adxf->stm = stm;
    if (stm == 0) {
        adx_error("E0110902:can't open file.(ADXF_OpenAfs)");
        return -1;
    }
    adxf->fsize_sct = nsct;
    adxf->fsize_byte = nsct << 11;
    return 0;
}

/* 0 on failure. */
/* 0x001c5190 */
static AdxfObj *ADXF_OpenAfs(int ptid, int flid)
{
    AdxfObj *adxf;

    adxf_history_add(2, 0, ptid, flid, -1);
    adx_lock();
    adxf = adxf_alloc_handle();
    if (adxf != 0) {
        if (adxf_open_afs_stream(adxf, ptid, flid) < 0) {
            adxf_close_handle(adxf);
            adxf = 0;
        }
    }
    adx_unlock();
    adxf_history_add(2, 1, ptid, flid, -1);
    return adxf;
}

/* Only the handle's own joint; invalidates buffer cache first if adxf_read_dcache_flag. */
/* 0x001c5240 */
static void adxf_release_sj(AdxfObj *adxf)
{
    if (adxf->sj == 0)
        return;
    if (adxf->sjflag != 0)
        return;
    if (adxf_read_dcache_flag == 1)
        adx_invalidate_dcache(adxf->buf, adxf->bufsize);
    adxf->sj->vtbl->destroy(adxf->sj);
    adxf->sj = 0;
}

/* Stops a running read, closes the stream, frees the handle. */
/* 0x001c52b0 */
void adxf_close_handle(void *obj)
{
    AdxfObj *adxf;

    adxf = obj;
    adxf_history_add(3, 0, (int)adxf, -1, -1);
    if (adxf == 0)
        return;
    adx_lock();
    if (adxf->stat == 2)
        ADXF_Stop(adxf);
    if (adxf->stm != 0)
        adxstm_close(adxf->stm);
    memset(adxf, 0, 312);
    adx_unlock();
    adxf_history_add(3, 1, (int)adxf, -1, -1);
}

/* 0x001c5360 */
static void adxf_close_all(void)
{
    int i;

    for (i = 0; i < 16; i++) {
        if (adxf_obj_tbl[i].used == 1)
            adxf_close_handle(&adxf_obj_tbl[i]);
    }
}

/* Returns sectors requested; 0 at end of file, which finishes the handle at once. */
/* 0x001c53c0 */
static int adxf_start_read_sj(AdxfObj *adxf, int nsct, SjObj *sj)
{
    int n;

    if (adxstm_get_stat(adxf->stm) != 1)
        adxstm_stop(adxf->stm);
    adx_lock();
    n = adxf->fsize_sct - adxf->cursct;
    adxf->nreadsct = 0;
    if (nsct < n)
        n = nsct;
    adxf->rqsct = adxf->ofst + adxf->cursct;
    adxf->rqnsct = n;
    if (n == 0) {
        adxf->stat = 3;
        nsct = 0;
    } else {
        adxstm_set_size_from_sct(adxf->stm, n);
        adxstm_set_size_sct(adxf->stm, -1);
        adxstm_set_sj(adxf->stm, sj);
        adxstm_set_ofst(adxf->stm, adxf->ofst + adxf->cursct);
        adxstm_set_max_sct(adxf->stm, adxf->maxsct);
        adxf->stat = 2;
        adxstm_start(adxf->stm);
        nsct = adxf->rqnsct;
    }
    adx_unlock();
    return nsct;
}

/* Returns sectors requested, 0 while a read runs, or -3 on bad arguments. */
/* 0x001c54b8 */
int ADXF_ReadSj32(AdxfObj *adxf, int nsct, SjObj *sj)
{
    int n;

    if (adxf == 0) {
        adx_error("E9040811:'adxf' is NULL.(ADXF_ReadSj32)");
        return -3;
    }
    if (nsct < 0) {
        adx_error("E9040812:'nsct'is negative.(ADXF_ReadSj32)");
        return -3;
    }
    if (sj == 0) {
        adx_error("E9040813:'sj'is NULL.(ADXF_ReadSj32)");
        return -3;
    }
    if (adxf->stat == 2)
        return 0;
    adx_lock();
    n = adxf_start_read_sj(adxf, nsct, sj);
    adxf->sjflag = 1;
    adx_unlock();
    return n;
}

/* Non-blocking: wraps buf in a new ring buffer joint. Sectors requested, 0 while busy, or < 0. */
/* 0x001c5568 */
int ADXF_ReadNw32(void *obj, int nsct, void *buf)
{
    AdxfObj *adxf = obj;
    int size;
    int n;
    SjObj *sj;

    adxf_history_add(4, 0, (int)adxf, nsct, (int)buf);
    if (adxf == 0) {
        adx_error("E9040816:'adxf' is NULL.(ADXF_ReadNw32)");
        return -3;
    }
    if (nsct < 0) {
        adx_error("E9040817:'nsct' is negative.(ADXF_ReadNw32)");
        return -3;
    }
    if (buf == 0) {
        adx_error("E9040818:'buf' is NULL.(ADXF_ReadNw32)");
        return -3;
    }
    if (adxf->stat == 2)
        return 0;
    if (adxf->sj != 0) {
        adx_error("E9040821:'sj' is NULL.(ADXF_ReadNw32)");
        return -1;
    }
    size = nsct * 2048;
    sj = sjrbf_create(buf, size, 0);
    if (sj == 0)
        return -2;
    adxf->bufsize = size;
    adxf->buf = buf;
    adx_lock();
    if (adxf_read_dcache_flag == 1)
        adx_invalidate_dcache(adxf->buf, adxf->bufsize);
    n = adxf_start_read_sj(adxf, nsct, sj);
    if (n <= 0)
        sj->vtbl->destroy(sj);
    else
        adxf->sj = sj;
    adxf->sjflag = 0;
    adx_unlock();
    adxf_history_add(4, 1, (int)adxf, nsct, (int)buf);
    return n;
}

/* As ADXF_ReadNw32, but -3 for a buffer not 64-byte aligned. */
/* 0x001c56d8 */
int ADXF_ReadNw(AdxfObj *adxf, int nsct, void *buf)
{
    if ((unsigned int)buf & 0x3f) {
        adx_error("E0120401:'buf' isn't 64byte alignment.(ADXF_ReadNw)");
        return -3;
    }
    return ADXF_ReadNw32(adxf, nsct, buf);
}

/* Records sectors read, releases the handle's own joint; returns the current sector. */
/* 0x001c5710 */
static int ADXF_Stop(AdxfObj *adxf)
{
    int n;

    adxf_history_add(5, 0, (int)adxf, -1, -1);
    if (adxf == 0) {
        adx_error("E9040822:'adxf' is NULL.(ADXF_Stop)");
        return -3;
    }
    if (adxf->stat == 1)
        return adxf->cursct;
    if (adxf->stat == 3) {
        adxf->stat = 1;
        return adxf->cursct;
    }
    if (adxf->stm == 0) {
        adx_error("E9040823:'adxf->stm' is NULL.(ADXF_Stop)");
        return -1;
    }
    adx_lock();
    adxstm_stop(adxf->stm);
    adxstm_update_nread(adxf->stm, &n);
    adxf->nreadsct = n;
    adxf_release_sj(adxf);
    adxf->stat = 1;
    adx_unlock();
    adxf_history_add(5, 1, (int)adxf, -1, -1);
    return adxf->cursct;
}

/* On read end (done or error) advances the position and releases the handle's own joint. */
/* 0x001c57f0 */
static void adxf_exec_handle(AdxfObj *adxf)
{
    int n;

    if (adxf->stat == 2) {
        adxf->stat = adxstm_get_stat(adxf->stm);
        adxstm_update_nread(adxf->stm, &n);
        adxf->nreadsct = n;
        if (adxf->stat == 3 || adxf->stat == 4) {
            adxf->cursct += n;
            adxf_release_sj(adxf);
        }
    }
}

/* 0x001c5868 */
void adxf_exec_server(void)
{
    int i;

    adx_lock();
    for (i = 0; i < 16; i++) {
        if (adxf_obj_tbl[i].used == 1)
            adxf_exec_handle(&adxf_obj_tbl[i]);
    }
    adx_unlock();
}

/* type 0 start, 1 current, 2 end; stops a running read, clamps; returns the new sector. */
/* 0x001c58d0 */
int ADXF_Seek(AdxfObj *adxf, int pos, int type)
{
    adxf_history_add(6, 0, (int)adxf, pos, type);
    if (adxf == 0) {
        adx_error("E9040825:'adxf' is NULL.(ADXF_Seek)");
        return -3;
    }
    if (adxf->stat == 2)
        ADXF_Stop(adxf);
    if (type == 0) {
        adxf->cursct = pos;
    } else if (type == 1) {
        adxf->cursct = adxf->cursct + pos;
    } else if (type == 2) {
        adxf->cursct = adxf->fsize_sct + pos;
    } else {
        adx_error("E9040826:'type' is illigal.(ADXF_Seek)");
        return -3;
    }
    if (adxf->cursct < 0)
        adxf->cursct = 0;
    else if (adxf->cursct > adxf->fsize_sct)
        adxf->cursct = adxf->fsize_sct;
    adxf_history_add(6, 1, (int)adxf, pos, type);
    return adxf->cursct;
}

/* Sectors; -3 without a handle. */
/* 0x001c59d8 */
static int ADXF_Tell(AdxfObj *adxf)
{
    if (adxf == 0) {
        adx_error("E9040827:'adxf' is NULL.(ADXF_Tell)");
        return -3;
    }
    return adxf->cursct;
}

/* -3 without a handle. */
/* 0x001c5a08 */
int ADXF_GetFsizeSct(AdxfObj *adxf)
{
    if (adxf == 0) {
        adx_error("E9040828:'adxf' is NULL.(ADXF_GetFsizeSct)");
        return -3;
    }
    return adxf->fsize_sct;
}

/* -3 without a handle. */
/* 0x001c5a38 */
static int ADXF_GetFsizeByte(AdxfObj *adxf)
{
    if (adxf == 0) {
        adx_error("E9040829:'adxf' is NULL.(ADXF_GetFsizeByte)");
        return -3;
    }
    return adxf->fsize_byte;
}

/* Sectors of the last request, start sector in *sct; -3 and 0 without a handle. */
/* 0x001c5a68 */
static int ADXF_GetNumReqSct(AdxfObj *adxf, int *sct)
{
    if (adxf == 0) {
        adx_error("E9040830:'adxf' is NULL.(ADXF_GetNumReqSct)");
        *sct = 0;
        return -3;
    }
    *sct = adxf->rqsct;
    return adxf->rqnsct;
}

/* Sectors read by the last request; -3 without a handle. */
/* 0x001c5ab0 */
static int ADXF_GetNumReadSct(AdxfObj *adxf)
{
    if (adxf == 0) {
        adx_error("E9040831:'adxf' is NULL.(ADXF_GetNumReadSct)");
        return -3;
    }
    return adxf->nreadsct;
}

/* 1 idle, 2 reading, 3 done, 4 error; -3 without a handle. */
/* 0x001c5ae0 */
int ADXF_GetStat(void *obj)
{
    AdxfObj *adxf = obj;

    if (adxf == 0) {
        adx_error("E9040832:'adxf' is NULL.(ADXF_GetStat)");
        return -3;
    }
    return adxf->stat;
}

/* Loaded partition and flid in range; reports and returns -3 on error. */
/* 0x001c5b10 */
static int adxf_check_pt_file_id(unsigned int ptid, int flid)
{
    AdxfPtInfo *pt;

    if (ptid >= 256) {
        adx_error("E9040828:'ptid' is range outside.");
        return -3;
    }
    pt = ((AdxfPtInfo **)adxf_pt_info_tbl)[ptid];
    if (pt == 0) {
        adx_error("E9040828:'ptid' is range outside.");
        return -3;
    }
    if (flid < 0 || flid >= pt->nfile) {
        adx_error("E9040828:'flid' is range outside.");
        return -3;
    }
    return 0;
}

/* Searches added archives for flid, else sums base partition sizes; -1, -1 for a bad id. */
/* 0x001c5b88 */
static int adxf_get_pt_file_info(int ptid, int flid, char *fname, int *ofst, int *nsct)
{
    int ret;
    int i;
    int sct;
    AdxfPtInfo *pt;
    AdxfPtEnt *ents;
    AdxfPtEnt *ent;
    unsigned short *len;

    ret = adxf_check_pt_file_id(ptid, flid);
    if (ret < 0) {
        *ofst = -1;
        *nsct = -1;
        return ret;
    }
    pt = ((AdxfPtInfo **)adxf_pt_info_tbl)[ptid];
    while (pt->next != 0) {
        ents = (AdxfPtEnt *)pt->fsct;
        for (i = 0; i < pt->nfile16; i++) {
            ent = &ents[i];
            if (ents[i].flid == flid) {
                strncpy(fname, pt->fname, 256);
                *ofst = ent->ofst;
                *nsct = ent->nsct;
                return ret;
            }
        }
        pt = (AdxfPtInfo *)pt->next;
    }
    sct = (unsigned short)pt->fsct[0];
    len = (unsigned short *)&pt->fsct[1];
    for (i = 0; i < flid; i++)
        sct += len[i];
    strncpy(fname, pt->fname, 256);
    *ofst = sct;
    *nsct = len[flid];
    return ret;
}

/* As adxf_get_pt_file_info, plus the directory. */
/* 0x001c5d08 */
int adxf_get_pt_file_info_dir(int ptid, int flid, char *fname, void **dir, int *ofst, int *nsct)
{
    int ret;
    int i;
    int sct;
    AdxfPtInfo *pt;
    AdxfPtEnt *ents;
    AdxfPtEnt *ent;
    unsigned short *len;

    ret = adxf_check_pt_file_id(ptid, flid);
    if (ret < 0) {
        *dir = 0;
        *ofst = -1;
        *nsct = -1;
        return ret;
    }
    pt = ((AdxfPtInfo **)adxf_pt_info_tbl)[ptid];
    while (pt->next != 0) {
        ents = (AdxfPtEnt *)pt->fsct;
        for (i = 0; i < pt->nfile16; i++) {
            ent = &ents[i];
            if (ents[i].flid == flid) {
                strncpy(fname, pt->fname, 256);
                *ofst = ent->ofst;
                *dir = pt->dir;
                *nsct = ent->nsct;
                return ret;
            }
        }
        pt = (AdxfPtInfo *)pt->next;
    }
    sct = (unsigned short)pt->fsct[0];
    len = (unsigned short *)&pt->fsct[1];
    for (i = 0; i < flid; i++)
        sct += len[i];
    strncpy(fname, pt->fname, 256);
    *ofst = sct;
    *dir = pt->dir;
    *nsct = len[flid];
    return ret;
}

/* 1: buffer reads invalidate its data cache lines first. */
/* 0x001c5ea0 */
static void adxf_set_read_dcache_flag(int flag)
{
    adxf_read_dcache_flag = flag;
}

/* Sectors; refused while reading. */
/* 0x001c5eb0 */
static void ADXF_SetReqRdSct(AdxfObj *adxf, int nsct)
{
    if (adxf->stat == 2) {
        adx_error("E0041201:state is reading(ADXF_SetReqRdSct)");
        return;
    }
    adxf->maxsct = nsct;
}
