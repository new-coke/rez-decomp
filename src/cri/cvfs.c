/*
 * CRI virtual file system: 32 named devices (DVD, host, ROFS volumes), 40 open files. "DEVICE:file"
 * routes to the named or default device's function table. Errors go as text to one callback.
 */

#include "types_cri.h"
#include "cvfs.h"
#include "libc.h"

static CvFsDevIf *cvfs_add_device(char *name, CvFsDevIf *(*getif)(void));
static CvFsDevIf *cvfs_find_device(char *name);
static void cvfs_to_upper(char *s);
static int cvfs_is_device_name(char *name, int len);
static CvFsHn *cvfs_alloc_handle(void);
static void cvfs_free_handle(CvFsHn *hn);
static void cvfs_split_path(char *dev, char *fname, const char *path);
static void cvfs_get_def_dev(char *dst);
static int cvfs_is_volume_device(char *name);
static void cvfs_make_volume_path(char *dev, char *fname);

extern char cvfs_path_buf[];

/* Handed to every device; forwards to the error callback. */
/* 0x001cb8f8 */
static void cvfs_call_error_cb(void *obj, const char *msg, int code)
{
    if (cvfs_error_fn)
        cvfs_error_fn(cvfs_error_obj, msg, code);
}

/* 0x001cb928 */
void cvfs_error(const char *msg)
{
    cvfs_call_error_cb(&cvfs_error_obj, msg, 0);
}

/* First call clears handles, devices, default name; counts calls. */
/* 0x001cb940 */
static void cvfs_lib_init(void)
{
    char *ver;
    int i;

    ver = cvfs_ver_str;
    if (cvfs_init_count == 0) {
        for (i = 0; i < 40; i++) {
            cvfs_hn_tbl[i].dev = 0;
            cvfs_hn_tbl[i].fd = 0;
        }
        for (i = 0; i < 32; i++)
            cvfs_dev_tbl[i].name[0] = 0;
        memset(cvfs_def_dev_name, 0, 9);
        cvfs_def_dev_name[0] = 0;
    }
    cvfs_init_count++;
}

/* Last user: closes every file, clears handles, devices, default name. */
/* 0x001cb9f8 */
void cvfs_lib_finish(void)
{
    int i;

    if (--cvfs_init_count == 0) {
        for (i = 0; i < 40; i++) {
            if (cvfs_hn_tbl[i].dev)
                cvfs_hn_tbl[i].dev->close(cvfs_hn_tbl[i].fd);
            cvfs_hn_tbl[i].dev = 0;
            cvfs_hn_tbl[i].fd = 0;
        }
        for (i = 0; i < 32; i++)
            cvfs_dev_tbl[i].name[0] = 0;
        memset(cvfs_def_dev_name, 0, 9);
        cvfs_def_dev_name[0] = 0;
    }
}

/* Also hands it the cvFs error function. */
/* 0x001cbaf8 */
void cvFsAddDev(char *name, CvFsDevIf *(*getif)(void))
{
    char *ver;
    CvFsDevIf *dev;

    ver = cvfs_ver_str;
    if (name == 0) {
        cvfs_error("cvFsAddDev #1:illegal device name");
        return;
    }
    if (getif == 0) {
        cvfs_error("cvFsAddDev #2:illegal I/F func name");
        return;
    }
    dev = cvfs_add_device(name, getif);
    if (dev == 0) {
        cvfs_error("cvFsAddDev #3:failed added a device");
        return;
    }
    if (dev->entry_err_func)
        dev->entry_err_func(cvfs_call_error_cb, 0);
}

/* Upper-cases name; first free entry unless present; 0 when full. */
/* 0x001cbb80 */
static CvFsDevIf *cvfs_add_device(char *name, CvFsDevIf *(*getif)(void))
{
    int i;
    CvFsDevIf *dev;

    i = 0;
    cvfs_to_upper(name);
    dev = getif();
    if (cvfs_find_device(name) != 0)
        return dev;
    for (i = 0; i < 32; i++) {
        if (cvfs_dev_tbl[i].name[0] == 0)
            break;
    }
    if (i == 32)
        return 0;
    cvfs_dev_tbl[i].dev = dev;
    memcpy(cvfs_dev_tbl[i].name, name, strlen(name) + 1);
    return dev;
}

/* Entry must start with name; 0 if none. */
/* 0x001cbc58 */
static CvFsDevIf *cvfs_find_device(char *name)
{
    int len;
    unsigned int i;

    len = strlen(name);
    for (i = 0; i < 32; i++) {
        if (strncmp(name, cvfs_dev_tbl[i].name, len) == 0)
            return cvfs_dev_tbl[i].dev;
    }
    return 0;
}

/* 0x001cbcf0 */
static void cvfs_to_upper(char *s)
{
    unsigned int i;
    unsigned int n;

    n = strlen(s);
    for (i = 0; i < n + 1; i++) {
        if (((unsigned char *)s)[i] >= 'a' && ((unsigned char *)s)[i] <= 'z')
            s[i] = ((unsigned char *)s)[i] - 32;
    }
}

/* 0x001cbd50 */
void cvFsDelDev(char *name)
{
    int len;
    unsigned int i;

    if (name == 0) {
        cvfs_error("cvFsDelDev #1:illegal device name");
        return;
    }
    len = strlen(name);
    for (i = 0; i < 32; i++) {
        if (strncmp(name, cvfs_dev_tbl[i].name, len) == 0) {
            cvfs_dev_tbl[i].name[0] = 0;
            return;
        }
    }
}

/* Must be in the table; empty name clears it. */
/* 0x001cbe20 */
void cvFsSetDefDev(char *name)
{
    int len;

    if (name == 0) {
        cvfs_error("cvFsSetDefDev #1:illegal device name");
    } else {
        len = strlen(name);
        if (len != 0) {
            cvfs_to_upper(name);
            if (cvfs_is_device_name(name, len) == 1)
                memcpy(cvfs_def_dev_name, name, len + 1);
            else
                cvfs_error("cvFsSetDefDev #2:unknown device name");
        } else {
            cvfs_def_dev_name[0] = 0;
        }
    }
}

/* A device entry starts with name. */
/* 0x001cbee0 */
static int cvfs_is_device_name(char *name, int len)
{
    int i;

    for (i = 0; i < 32; i++) {
        if (strncmp(name, cvfs_dev_tbl[i].name, len) == 0)
            return 1;
    }
    return 0;
}

/* 0x001cbf60 */
static char *cvfs_get_def_dev_buf(void)
{
    return cvfs_def_dev_name;
}

/*
 * No device: default. Volume device: prefix put back on the file name. Unknown device: default,
 * with the whole path as file name. 0 if none.
 */
/* 0x001cbf70 */
static CvFsDevIf *cvfs_resolve_device(char *dev, char *fname, char *path)
{
    CvFsDevIf *devif;

    if (dev[0] == 0) {
        cvfs_get_def_dev(dev);
        if (dev[0] == 0)
            return 0;
    }
    cvfs_make_volume_path(dev, fname);
    devif = cvfs_find_device(dev);
    if (devif == 0) {
        cvfs_get_def_dev(dev);
        devif = cvfs_find_device(dev);
        if (devif == 0)
            return 0;
        strcpy(fname, path);
    }
    return devif;
}

/* 0 on any failure. */
/* 0x001cc020 */
CvFsHn *cvFsOpen(const char *path, void *dir, int mode)
{
    CvFsHn *hn;
    char dev[297];
    char fname[297];

    if (path == 0) {
        cvfs_error("cvFsOpen #1:illegal file name");
        return 0;
    }
    cvfs_split_path(dev, fname, path);
    if (fname[0] == 0) {
        cvfs_error("cvFsOpen #1:illegal file name");
        return 0;
    }
    hn = cvfs_alloc_handle();
    if (hn == 0) {
        cvfs_error("cvFsOpen #3:failed handle alloced");
        return 0;
    }
    hn->dev = cvfs_resolve_device(dev, fname, path);
    if (hn->dev == 0) {
        cvfs_free_handle(hn);
        cvfs_error("cvFsOpen #4:device not found");
        return 0;
    }
    if (hn->dev->open == 0) {
        cvfs_free_handle(hn);
        cvfs_error("cvFsOpen #5:vtbl error");
        return 0;
    }
    hn->fd = (void *)hn->dev->open(fname, dir, mode);
    if (hn->fd == 0) {
        cvfs_free_handle(hn);
        cvfs_error("cvFsOpen #6:open failed");
        return 0;
    }
    return hn;
}

/* 0 if none. */
/* 0x001cc140 */
static CvFsHn *cvfs_alloc_handle(void)
{
    int i;

    for (i = 0; i < 40; i++) {
        if (cvfs_hn_tbl[i].fd == 0)
            break;
    }
    if (i == 40)
        return 0;
    return &cvfs_hn_tbl[i];
}

/* 0x001cc198 */
static void cvfs_free_handle(CvFsHn *hn)
{
    hn->fd = 0;
    hn->dev = 0;
}

/* Device upper-cased; no ':' -> empty device; a one-letter prefix ("C:") stays in the file name. */
/* 0x001cc1a8 */
static void cvfs_split_path(char *dev, char *fname, const char *path)
{
    int i;
    int j;

    if (path == 0)
        return;
    for (i = 0; i < 297 && path[i] != ':' && path[i] != 0; i++)
        dev[i] = path[i];
    if (path[i] == 0) {
        dev[i] = 0;
        memcpy(fname, dev, strlen(dev) + 1);
        dev[0] = 0;
        return;
    }
    dev[i] = 0;
    i++;
    if (i == 2) {
        dev[0] = 0;
        i = 0;
    }
    for (j = i; j < 297 && path[j] != 0; j++)
        fname[j - i] = path[j];
    fname[j - i] = 0;
    cvfs_to_upper(dev);
}

/* Empty when none. */
/* 0x001cc2f0 */
static void cvfs_get_def_dev(char *dst)
{
    int len;

    len = strlen(cvfs_def_dev_name);
    if (cvfs_def_dev_name[0] != 0)
        memcpy(dst, cvfs_def_dev_name, len + 1);
    else
        dst[0] = 0;
}

/* 0x001cc368 */
void cvFsClose(CvFsHn *hn)
{
    if (hn == 0) {
        cvfs_error("cvFsClose #1:handle error");
    } else if (hn->dev->close) {
        hn->dev->close(hn->fd);
        cvfs_free_handle(hn);
    } else {
        cvfs_error("cvFsClose #2:vtbl error");
    }
}

/* Sectors; 0 on error. */
/* 0x001cc3d8 */
int cvFsTell(CvFsHn *hn)
{
    int ret;

    if (hn == 0) {
        cvfs_error("cvFsTell #1:handle error");
        return 0;
    }
    if (hn->dev->tell) {
        ret = hn->dev->tell(hn->fd);
    } else {
        ret = 0;
        cvfs_error("cvFsTell #2:vtbl error");
    }
    return ret;
}

/* Sectors from whence; 0 on error. */
/* 0x001cc440 */
int cvFsSeek(CvFsHn *hn, int ofst, int whence)
{
    int ret;

    if (hn == 0) {
        cvfs_error("cvFsSeek #1:handle error");
        return 0;
    }
    if (hn->dev->seek) {
        ret = hn->dev->seek(hn->fd, ofst, whence);
    } else {
        ret = 0;
        cvfs_error("cvFsSeek #2:vtbl error");
    }
    return ret;
}

/* Returns sectors requested; 0 on error. */
/* 0x001cc4a8 */
int cvFsReqRd(CvFsHn *hn, int nsct, void *buf)
{
    int ret;

    if (hn == 0) {
        cvfs_error("cvFsReqRd #1:handle error");
        return 0;
    }
    if (hn->dev->req_rd) {
        ret = hn->dev->req_rd(hn->fd, nsct, buf);
    } else {
        ret = 0;
        cvfs_error("cvFsReqRd #2:vtbl error");
    }
    return ret;
}

/* 0 on error. */
/* 0x001cc510 */
static int cvFsReqWr(CvFsHn *hn, int nsct, void *buf)
{
    int ret = 0;

    if (hn == 0) {
        cvfs_error("cvFsReqWr #1:handle error");
        return 0;
    }
    if (hn->dev->req_wr)
        ret = hn->dev->req_wr(hn->fd, nsct, buf);
    else
        cvfs_error("cvFsReqWr #2:vtbl error");
    return ret;
}

/* 0x001cc578 */
void cvFsStopTr(CvFsHn *hn)
{
    if (hn == 0)
        cvfs_error("cvFsStopTr #1:handle error");
    else if (hn->dev->stop_tr == 0)
        cvfs_error("cvFsStopTr #2:vtbl error");
    else
        hn->dev->stop_tr(hn->fd);
}

/* Every device's server function. */
/* 0x001cc5d0 */
void cvfs_exec_server(void)
{
    int i;

    for (i = 0; i < 32; i++) {
        if (cvfs_dev_tbl[i].dev && cvfs_dev_tbl[i].dev->exec_server)
            cvfs_dev_tbl[i].dev->exec_server();
    }
}

/* 1 done, 3 error (also for a bad handle). */
/* 0x001cc630 */
int cvFsGetStat(CvFsHn *hn)
{
    int ret = 3;

    if (hn == 0) {
        cvfs_error("cvFsGetStat #1:handle error");
        return 3;
    }
    if (hn->dev->get_stat)
        ret = hn->dev->get_stat(hn->fd);
    else
        cvfs_error("cvFsGetStat #2:vtbl error");
    return ret;
}

/* Bytes; 0 on error. Bug: after reporting a missing device, calls through the null device. */
/* 0x001cc698 */
int cvFsGetFileSize(const char *path)
{
    CvFsDevIf *dev;
    char dname[297];
    char fname[297];

    if (path == 0) {
        cvfs_error("cvFsGetFileSize #1:illegal file name");
        return 0;
    }
    cvfs_split_path(dname, fname, path);
    if (fname[0] == 0) {
        cvfs_error("cvFsGetFileSize #1:illegal file name");
        return 0;
    }
    dev = cvfs_resolve_device(dname, fname, path);
    if (dev == 0)
        cvfs_error("cvFsGetFileSize #3:device not found");
    if (dev->get_file_size == 0) {
        cvfs_error("cvFsGetFileSize #4:vtbl error");
        return 0;
    }
    return dev->get_file_size(fname);
}

/* Bytes; 0 on error. Bug: after reporting a missing device, calls through the null device. */
/* 0x001cc748 */
int cvFsGetFileSizeEx(const char *path, int dir)
{
    CvFsDevIf *dev;
    char dname[297];
    char fname[297];

    if (path == 0) {
        cvfs_error("cvFsGetFileSizeEx #1:illegal file name");
        return 0;
    }
    cvfs_split_path(dname, fname, path);
    if (fname[0] == 0) {
        cvfs_error("cvFsGetFileSizeEx #1:illegal file name");
        return 0;
    }
    dev = cvfs_resolve_device(dname, fname, path);
    if (dev == 0)
        cvfs_error("cvFsGetFileSizeEx #3:device not found");
    if (dev->get_file_size_ex == 0) {
        cvfs_error("cvFsGetFileSizeEx #4:vtbl error");
        return 0;
    }
    return dev->get_file_size_ex(fname, dir);
}

/* Named or default device. */
/* 0x001cc808 */
static int cvFsGetFreeSize(char *name)
{
    int ret;
    int len;
    int i;
    char dname[297];

    ret = 0;
    if (name == 0 || name[0] == 0) {
        cvfs_get_def_dev(dname);
        if (dname[0] == 0) {
            cvfs_error("cvFsGetFreeSize #5:device not found");
            return 0;
        }
    } else {
        memcpy(dname, name, strlen(name) + 1);
    }
    len = strlen(dname);
    if (len <= 0) {
        cvfs_error("cvFsGetFreeSize #5:device not found");
        return 0;
    }
    for (i = 0; i < 32; i++) {
        if (strncmp(dname, cvfs_dev_tbl[i].name, len) == 0) {
            if (cvfs_dev_tbl[i].dev == 0) {
                cvfs_error("cvFsGetFreeSize #6:vtbl error");
                return 0;
            }
            if (cvfs_dev_tbl[i].dev->get_free_size)
                ret = cvfs_dev_tbl[i].dev->get_free_size();
        }
    }
    return ret;
}

/* Bytes; 0 on error. */
/* 0x001cc928 */
int cvFsGetSctLen(CvFsHn *hn)
{
    int ret = 0;

    if (hn == 0) {
        cvfs_error("cvFsGetSctLen #1:handle error");
        return 0;
    }
    if (hn->dev->get_sct_len)
        ret = hn->dev->get_sct_len(hn->fd);
    else
        cvfs_error("cvFsGetSctLen #2:vtbl error");
    return ret;
}

/* Bytes. */
/* 0x001cc990 */
static void cvFsSetSctLen(CvFsHn *hn, int len)
{
    if (hn == 0)
        cvfs_error("cvFsSetSctLen #3:handle error");
    else if (hn->dev->set_sct_len == 0)
        cvfs_error("cvFsSetSctLen #4:vtbl error");
    else
        hn->dev->set_sct_len(hn->fd, len);
}

/* 0 on error. */
/* 0x001cc9e8 */
int cvFsGetNumTr(CvFsHn *hn)
{
    int ret = 0;

    if (hn == 0) {
        cvfs_error("cvFsGetNumTr #1:handle error");
        return 0;
    }
    if (hn->dev->get_num_tr)
        ret = hn->dev->get_num_tr(hn->fd);
    else
        cvfs_error("cvFsGetNumTr #2:vtbl error");
    return ret;
}

/* -1 on error. Bug: after reporting a missing device, calls through the null device. */
/* 0x001cca50 */
static int cvFsChangeDir(const char *path)
{
    CvFsDevIf *dev;
    char dname[297];
    char fname[297];

    if (path == 0) {
        cvfs_error("cvFsChangeDir #1:illegal directory name");
        return -1;
    }
    cvfs_split_path(dname, fname, path);
    if (fname[0] == 0) {
        cvfs_error("cvFsChangeDir #1:illegal directory name");
        return -1;
    }
    dev = cvfs_resolve_device(dname, fname, path);
    if (dev == 0)
        cvfs_error("cvFsChangeDir #3:device not found");
    if (dev->change_dir == 0) {
        cvfs_error("cvFsChangeDir #4:vtbl error");
        return -1;
    }
    return dev->change_dir(fname);
}

/* 0 on error. Bug: after reporting a missing device, calls through the null device. */
/* 0x001ccb00 */
static int cvFsIsExistFile(const char *path)
{
    CvFsDevIf *dev;
    char dname[297];
    char fname[297];

    cvfs_split_path(dname, fname, path);
    if (fname[0] == 0) {
        cvfs_error("cvFsIsExistFile #1:illegal file name");
        return 0;
    }
    dev = cvfs_resolve_device(dname, fname, path);
    if (dev == 0)
        cvfs_error("cvFsIsExistFile #3:device not found");
    if (dev->is_exist_file == 0) {
        cvfs_error("cvFsIsExistFile #4:vtbl error");
        return 0;
    }
    return dev->is_exist_file(fname);
}

/* On the device, or its volume for volume devices; 0 when unknown. */
/* 0x001ccba0 */
static int cvFsGetNumFiles(char *name)
{
    int len;
    int i;
    int ret;
    CvFsDevIf *dev;
    CvFsOptArg arg;

    ret = 0;
    len = strlen(name);
    for (i = 0; i < 32; i++) {
        if (strncmp(name, cvfs_dev_tbl[i].name, len) == 0) {
            dev = cvfs_dev_tbl[i].dev;
            if (cvfs_is_volume_device(name)) {
                if (dev && dev->opt_fn1) {
                    arg.name = name;
                    ret = dev->opt_fn1(&arg, 4, 0, 0);
                    break;
                }
            } else {
                if (dev && dev->get_num_files) {
                    ret = dev->get_num_files();
                    break;
                }
            }
        }
    }
    return ret;
}

/* Default device when the path names none; 0 if unavailable. Error message says cvFsIsExistFile. */
/* 0x001ccca8 */
static int cvFsLoadDirInfo(const char *path, int inf, int n)
{
    int ret;
    CvFsDevIf *dev;
    char dname[297];
    char fname[297];

    cvfs_split_path(dname, fname, path);
    ret = 0;
    if (dname[0] == 0) {
        cvfs_get_def_dev(dname);
        if (dname[0] == 0) {
            cvfs_error("cvFsIsExistFile #2:illegal device name");
            return 0;
        }
    }
    cvfs_make_volume_path(dname, fname);
    dev = cvfs_find_device(dname);
    if (dev && dev->load_dir_info)
        ret = dev->load_dir_info(path, inf, n);
    return ret;
}

/* Bytes per second; 0 on error. */
/* 0x001ccd78 */
static int cvFsGetMaxByteRate(CvFsHn *hn)
{
    int ret;

    if (hn == 0) {
        cvfs_error("cvFsGetMaxByteRate #1:handle error");
        return 0;
    }
    if (hn->dev->get_max_byte_rate) {
        ret = hn->dev->get_max_byte_rate(hn->fd);
    } else {
        cvfs_error("cvFsGetMaxByteRate #2:vtbl error");
        ret = 0;
    }
    return ret;
}

/* -1 on error. Bug: after reporting a missing device, calls through the null device. */
/* 0x001ccdd0 */
static int cvFsMakeDir(const char *path)
{
    CvFsDevIf *dev;
    char dname[48];
    char fname[48];

    if (path == 0) {
        cvfs_error("cvFsMakeDir #1:illegal directory name");
        return -1;
    }
    cvfs_split_path(dname, fname, path);
    if (fname[0] == 0) {
        cvfs_error("cvFsMakeDir #1:illegal directory name");
        return -1;
    }
    dev = cvfs_resolve_device(dname, fname, path);
    if (dev == 0)
        cvfs_error("cvFsMakeDir #3:device not found");
    if (dev->make_dir == 0) {
        cvfs_error("cvFsMakeDir #4:vtbl error");
        return -1;
    }
    return dev->make_dir(fname);
}

/* -1 on error. Bug: after reporting a missing device, calls through the null device. */
/* 0x001cce80 */
static int cvFsRemoveDir(const char *path)
{
    CvFsDevIf *dev;
    char dname[297];
    char fname[297];

    if (path == 0) {
        cvfs_error("cvFsRemoveDir #1:illegal directory name");
        return -1;
    }
    cvfs_split_path(dname, fname, path);
    if (fname[0] == 0) {
        cvfs_error("cvFsRemoveDir #1:illegal directory name");
        return -1;
    }
    dev = cvfs_resolve_device(dname, fname, path);
    if (dev == 0)
        cvfs_error("cvFsRemoveDir #3:device not found");
    if (dev->remove_dir == 0) {
        cvfs_error("cvFsRemoveDir #4:vtbl error");
        return -1;
    }
    return dev->remove_dir(fname);
}

/* -1 on error. Bug: after reporting a missing device, calls through the null device. */
/* 0x001ccf30 */
static int cvFsDeleteFile(const char *path)
{
    CvFsDevIf *dev;
    char dname[297];
    char fname[297];

    if (path == 0) {
        cvfs_error("cvFsDeleteFile #1:illegal file name");
        return -1;
    }
    cvfs_split_path(dname, fname, path);
    if (fname[0] == 0) {
        cvfs_error("cvFsDeleteFile #1:illegal file name");
        return -1;
    }
    dev = cvfs_resolve_device(dname, fname, path);
    if (dev == 0)
        cvfs_error("cvFsDeleteFile #3:device not found");
    if (dev->delete_file == 0) {
        cvfs_error("cvFsDeleteFile #4:vtbl error");
        return -1;
    }
    return dev->delete_file(fname);
}

/* 0x001ccfe0 */
static char *cvFsGetDevName(CvFsHn *hn)
{
    int i;

    if (hn == 0) {
        cvfs_error("cvFsGetDevName #1:vtbl error");
        return 0;
    }
    for (i = 0; i < 32; i++) {
        if (cvfs_dev_tbl[i].dev == hn->dev)
            break;
    }
    return cvfs_dev_tbl[i].name;
}

/* Null fn clears both. */
/* 0x001cd070 */
void cvfs_set_error_cb(void (*fn)(void *obj, const char *msg, int arg), void *obj)
{
    if (fn == 0) {
        cvfs_error_fn = 0;
        cvfs_error_obj = 0;
    } else {
        cvfs_error_fn = fn;
        cvfs_error_obj = obj;
    }
}

/* Device option function 1; 0 on error. */
/* 0x001cd098 */
static int cvFsOptFn1(CvFsHn *hn, int cmd, int param, int extra)
{
    int ret = 0;

    if (hn == 0) {
        cvfs_error("cvFsOptFn1 #1:handle error");
        return 0;
    }
    if (hn->dev->opt_fn1)
        ret = hn->dev->opt_fn1(hn->fd, cmd, param, extra);
    else
        cvfs_error("cvFsOptFn1 #2:vtbl error");
    return ret;
}

/* Device option function 2; 0 on error. */
/* 0x001cd100 */
static int cvFsOptFn2(CvFsHn *hn, int cmd, int param, int extra)
{
    int ret = 0;

    if (hn == 0) {
        cvfs_error("cvFsOptFn2 #1:handle error");
        return 0;
    }
    if (hn->dev->opt_fn2)
        ret = hn->dev->opt_fn2(hn->fd, cmd, param, extra);
    else
        cvfs_error("cvFsOptFn2 #2:vtbl error");
    return ret;
}

/* Option command 1; -1 on error. */
/* 0x001cd168 */
static int cvFsSetCurVolume(char *dev, void *img)
{
    CvFsDevIf *devif;
    CvFsOptArg arg;

    if (dev == 0) {
        cvfs_error("cvFsSetCurVolume #1:illegal device name");
        return -1;
    }
    if (img == 0) {
        cvfs_error("cvFsSetCurVolume #2:illegal image handle");
        return -1;
    }
    devif = cvfs_find_device(dev);
    if (devif == 0) {
        cvfs_error("cvFsSetCurVolume #3:device not found");
        return -1;
    }
    memset(&arg, 0, sizeof(arg));
    arg.img = img;
    if (devif->opt_fn1)
        return devif->opt_fn1(&arg, 1, 0, 0);
    return -1;
}

/* Option command 2; img is an open image file. */
/* 0x001cd228 */
int cvFsAddVolume(char *dev, char *vol, void *img)
{
    CvFsDevIf *devif;
    CvFsOptArg arg;

    if (dev == 0) {
        cvfs_error("cvFsAddVolume #1:illegal device name");
        return -1;
    }
    if (vol == 0) {
        cvfs_error("cvFsAddVolume #2:illegal volume name");
        return -1;
    }
    if (img == 0) {
        cvfs_error("cvFsAddVolume #3:illegal image handle");
        return -1;
    }
    devif = cvfs_find_device(dev);
    if (devif == 0) {
        cvfs_error("cvFsAddVolume #3:device not found");
        return -1;
    }
    memset(&arg, 0, sizeof(arg));
    arg.img = img;
    arg.name = vol;
    if (devif->opt_fn1)
        return devif->opt_fn1(&arg, 2, 0, 0);
    return -1;
}

/* Option command 3; -1 on error. */
/* 0x001cd310 */
int cvFsDelVolume(char *dev, char *vol)
{
    CvFsDevIf *devif;
    CvFsOptArg arg;

    if (dev == 0) {
        cvfs_error("cvFsDelVolume #1:illegal device name");
        return -1;
    }
    if (vol == 0) {
        cvfs_error("cvFsDelVolume #2:illegal volume name");
        return -1;
    }
    devif = cvfs_find_device(dev);
    if (devif == 0) {
        cvfs_error("cvFsDelVolume #3:device not found");
        return -1;
    }
    memset(&arg, 0, sizeof(arg));
    arg.name = vol;
    if (devif->opt_fn1)
        return devif->opt_fn1(&arg, 3, 0, 0);
    return -1;
}

/* Option command 5; -1 on error. */
/* 0x001cd3d0 */
int cvFsGetVolumeInfo(char *dev, char *vol, void *info)
{
    CvFsDevIf *devif;
    CvFsOptArg arg;

    if (dev == 0) {
        cvfs_error("cvFsGetVolumeInfo #1:illegal device name");
        return -1;
    }
    if (vol == 0) {
        cvfs_error("cvFsGetVolumeInfo #2:illegal volume name");
        return -1;
    }
    devif = cvfs_find_device(dev);
    if (devif == 0) {
        cvfs_error("cvFsGetVolumeInfo #3:device not found");
        return -1;
    }
    memset(&arg, 0, sizeof(arg));
    arg.name = vol;
    arg.info = info;
    if (devif->opt_fn1)
        return devif->opt_fn1(&arg, 5, 0, 0);
    return -1;
}

/* Option command 100; 0 when it cannot say. */
/* 0x001cd4a0 */
static int cvfs_is_volume_device(char *name)
{
    CvFsDevIf *dev;

    dev = cvfs_find_device(name);
    if (dev == 0)
        return 0;
    if (dev->opt_fn1)
        return dev->opt_fn1(0, 100, 0, 0);
    return 0;
}

/* "<device>:<file>" for volume devices. */
/* 0x001cd4f0 */
static void cvfs_make_volume_path(char *dev, char *fname)
{
    if (dev == 0)
        dev = cvfs_get_def_dev_buf();
    if (cvfs_is_volume_device(dev) == 1) {
        strcpy(cvfs_path_buf, fname);
        sprintf(fname, "%s:%s", dev, cvfs_path_buf);
    }
}

/* 0x001cd588 */
static int cvfs_is_device(char *name)
{
    return cvfs_find_device(name) != 0;
}
