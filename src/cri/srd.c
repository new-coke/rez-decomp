/* Sector read driver: one request at a time, DVD (sceCdRead) or host file; drive checks. */

#include "types_cri.h"
#include "dvci.h"
#include "srd.h"
#include "libc.h"
#include "libcdvd.h"
#include "libkernl_fileio.h"
#include "svm.h"

/* 0x001d7520 */
static void srd_clear_request(void)
{
    srd_ver_str;
    memset(&srd_req, 0, 40);
}

/* Only if none pending; 1 if queued. */
/* 0x001d7540 */
int srd_req_cd_read(int lsn, int nsct, void *buf, SrdMode *mode)
{
    int ret;

    if (srd_req.stat == 0 || srd_req.stat == 3 || srd_req.stat == 9) {
        srd_clear_request();
        srd_req.req = 1;
        srd_req.stat = 1;
        srd_req.lsn = lsn;
        srd_req.nsct = nsct;
        srd_req.buf = buf;
        srd_req.mode = *mode;
        ret = 1;
    } else {
        ret = 0;
    }
    return ret;
}

/* Only if none pending; 1 if queued. */
/* 0x001d75f0 */
int srd_req_host_read(int fd, void *buf, int size)
{
    int ret;

    if (srd_req.stat == 0 || srd_req.stat == 3 || srd_req.stat == 9) {
        srd_clear_request();
        srd_req.req = 2;
        srd_req.stat = 1;
        srd_req.fd = fd;
        srd_req.hbuf = buf;
        srd_req.hsize = size;
        ret = 1;
    } else {
        ret = 0;
    }
    return ret;
}

/* 0 idle, 1 queued, 2 reading, 3 done, 9 error. */
/* 0x001d7688 */
int srd_get_stat(void)
{
    return srd_req.stat;
}

/* Between host status polls. */
/* 0x001d7698 */
static void srd_delay(void)
{
    int i;

    for (i = 0; i < 0x8000; i++)
        ;
}

/* 0x001d76c8 */
static void srd_wait_cd(void)
{
    sceCdSync(0);
}

/* With a time limit; a failing status poll is an error. */
/* 0x001d76d0 */
static void srd_wait_host(void)
{
    int st;
    int ret;
    int i;

    st = 1;
    for (i = 0; st == 1;) {
        ret = sceIoctl(srd_req.fd, 1, &st);
        if (ret < 0) {
            srd_req.stat = 9;
            srd_req.err = ret;
            break;
        }
        srd_delay();
        i++;
        if (i > 0xffffff)
            break;
    }
}

/* 0x001d7780 */
void srd_wait(void)
{
    SrdObj *srd;

    svm_lock();
    srd = &srd_req;
    if (srd->req == 1)
        srd_wait_cd();
    if (srd->req == 2)
        srd_wait_host();
    srd->stat = 3;
    svm_unlock();
}

/* Disc read broken off, host read waited for; then idle. */
/* 0x001d77e8 */
void srd_cancel(void)
{
    SrdObj *srd;

    srd = &srd_req;
    if (srd->stat == 1)
        srd_clear_request();
    svm_lock();
    if (srd->stat == 2) {
        if (srd_req.req == 1 && sceCdBreak() == 0)
            sceCdSync(0);
        if (srd_req.req == 2)
            srd_wait();
    }
    svm_unlock();
    srd_req.stat = 0;
}

/* During a disc read; a drive error fails it (printed). */
/* 0x001d7898 */
static void srd_check_drive_error(void)
{
    int err;

    if (srd_req.stat == 2) {
        err = sceCdGetError();
        if (err == 48 || err == 49 || err == 50 || err == 20 || err == 16 || err == 17 || err == 18) {
            srd_req.stat = 9;
            srd_req.err = err;
            printf("SRD: Drive Error (sceCdGetError = 0x%x)\n", err);
        }
    }
}

/* Issues a queued read, then polls for its end. */
/* 0x001d7930 */
static void srd_exec_cd(void)
{
    if (srd_req.stat == 1) {
        if (sceCdRead(srd_req.lsn, srd_req.nsct, srd_req.buf, &srd_req.mode) == 1) {
            srd_req.stat = 2;
        } else {
            srd_req.stat = 9;
            srd_req.err = -1;
        }
    }
    if (srd_req.stat == 2) {
        if (sceCdSync(1) == 0)
            srd_req.stat = 3;
    }
}

/* Issues a queued read, then polls for its end. */
/* 0x001d79d0 */
static void srd_exec_host(void)
{
    int st;
    int ret;

    if (srd_req.stat == 1) {
        ret = sceRead(srd_req.fd, srd_req.hbuf, srd_req.hsize);
        if (ret < 0) {
            srd_req.err = ret;
            srd_req.stat = 9;
        } else {
            srd_req.stat = 2;
        }
    }
    if (srd_req.stat == 2) {
        if (sceIoctl(srd_req.fd, 1, &st) < 0) {
            srd_req.stat = 9;
            srd_req.err = -1;
        } else if (st == 0) {
            srd_req.stat = 3;
        }
    }
}

/* While the server step runs. */
/* 0x001d7a88 */
int srd_is_busy(void)
{
    return srd_exec_busy;
}

/* Drive check and read step, each under the lock if set; nothing when re-entered. */
/* 0x001d7a98 */
void srd_exec_server(void)
{
    if (srd_exec_busy == 1)
        return;
    srd_exec_busy = 1;
    if (srd_req.req == 1) {
        if (srd_check_locked == 1)
            svm_lock();
        if (srd_check_drive == 1)
            srd_check_drive_error();
        if (srd_check_locked == 1)
            svm_unlock();
        if (srd_req.stat == 9)
            goto end;
        if (srd_exec_locked == 1)
            svm_lock();
        srd_exec_cd();
        if (srd_exec_locked == 1)
            svm_unlock();
    }
    if (srd_req.req == 2) {
        svm_lock();
        srd_exec_host();
        svm_unlock();
    }
end:
    srd_exec_busy = 0;
}

/* 1 disc, 2 host. */
/* 0x001d7ba8 */
static int srd_get_kind(void)
{
    return srd_req.req;
}

/* 0x001d7bb8 */
static int srd_get_error(void)
{
    return srd_req.err;
}

/* Prints a timeout message after a long wait. */
/* 0x001d7bc8 */
static void SRD_WaitForExecServer(void)
{
    srd_wait_count = 0;
    while (srd_exec_busy == 1) {
        srd_wait_count++;
        if (srd_wait_count == 0x1f40000) {
            printf("SRD: SRD_WaitForExecServer is Timeout.\n");
            return;
        }
    }
}

/* Waits for the server step first. */
/* 0x001d7c40 */
void srd_lock(void)
{
    SRD_WaitForExecServer();
    svm_lock();
}

/* 0x001d7c60 */
void srd_unlock(void)
{
    svm_unlock();
}

/* 0x001d7c68 */
static void srd_set_exec_locked(int sw)
{
    SRD_WaitForExecServer();
    srd_exec_locked = sw;
}

/* 0x001d7c98 */
static void srd_set_check_locked(int sw)
{
    SRD_WaitForExecServer();
    srd_check_locked = sw;
}

/* 0x001d7cc8 */
static void srd_set_check_drive(int sw)
{
    SRD_WaitForExecServer();
    srd_check_drive = sw;
}

/* 0x001d7cf8 */
static void srd_set_error_stat(void)
{
    srd_req.stat = 9;
}
