/* ADX file system setup for the DVD drive: the "CDV" cvFs device and its dvci settings. */

#include "cri.h"
#include "adx_dvci.h"
#include "adxstm.h"
#include "cvfs.h"
#include "dvci.h"

extern void cvfs_set_error_cb(void (*fn)(void *obj, const char *msg), void *obj);
extern char adx_cdv_dev_name[];

/* cvFs errors -> adxstm_error_callback; adds "CDV" as default; prm null = defaults. */
/* steer(cast) */
/* 0x001c8fc8 */
void adx_fs_init_cdv(int *prm)
{
    cvfs_set_error_cb(adxstm_error_callback, 0);
    ((void (*)(const char *, void *, int))cvFsAddDev)(adx_cdv_dev_name, dvci_get_interface, 0);
    cvFsSetDefDev(adx_cdv_dev_name);
    if (prm != 0) {
        dvci_set_read_mode(prm[2], prm[3], prm[4], prm[1]);
        dvci_set_cur_dir(prm[0]);
    } else {
        dvci_set_read_mode(0, 0, 0, 0);
        dvci_set_cur_dir(0);
    }
}

/* prm: file name, buffer, size, name length; 0 when prm is null. */
/* 0x001c9070 */
static int adx_dvci_load_filelist(int *prm)
{
    int ret;

    ret = 0;
    if (prm != 0)
        ret = dvci_load_filelist(prm[0], prm[2], prm[3], prm[1]);
    else
        dvci_load_filelist(0, 0, 0, 0);
    return ret;
}

/* Read mode and current directory from prm (null = defaults). */
/* 0x001c90d0 */
static void adx_dvci_config(int *prm)
{
    if (prm != 0) {
        dvci_set_read_mode(prm[2], prm[3], prm[4], prm[1]);
        dvci_set_cur_dir(prm[0]);
    } else {
        dvci_set_read_mode(0, 0, 0, 0);
        dvci_set_cur_dir(0);
    }
}

/* 0x001c9138 */
static int adx_dvci_is_cached(const char *fname)
{
    return dvci_is_cached(fname);
}
