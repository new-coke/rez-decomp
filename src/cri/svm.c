/*
 * Server manager: four server functions for each of seven kinds (vsync interrupt, user vsync,
 * vsync, main, mw idle, user idle, unnamed), run by the vsync handler and server threads; shared
 * lock and error callbacks.
 */

#include "types_cri.h"
#include "libkernl_tty.h"
#include "svm.h"
#include <stdarg.h>
#include "libc.h"

/* called by name */
extern unsigned long __moddi3(unsigned long a, unsigned long b);
extern unsigned long __divdi3(unsigned long a, unsigned long b);

/* Via the lock callback, if set. */
/* 0x001d3a70 */
void svm_lock(void)
{
    if (svm_lock_fn != 0)
        svm_lock_fn(svm_lock_obj);
}

/* Via the unlock callback, if set. */
/* 0x001d3a98 */
void svm_unlock(void)
{
    if (svm_unlock_fn != 0)
        svm_unlock_fn(svm_unlock_obj);
}

/* printf-style. */
/* 0x001d3ac0 */
void svm_error_printf(const char *fmt, ...)
{
    va_list ap;

    memset(svm_error_buf, 0, 256);
    va_start(ap, fmt);
    vsprintf(svm_error_buf, fmt, ap);
    if (svm_error_fn != 0)
        svm_error_fn(svm_error_obj, svm_error_buf);
}

/* 0x001d3b48 */
void svm_error(const char *msg)
{
    strncpy(svm_error_buf, msg, 255);
    if (svm_error_fn != 0)
        svm_error_fn(svm_error_obj, svm_error_buf);
}

/* 0x001d3b98 */
static void svm_error_cat(const char *prefix, const char *detail)
{
    strncpy(svm_error_buf, prefix, 255);
    strncat(svm_error_buf, detail, 255);
    if (svm_error_fn != 0)
        svm_error_fn(svm_error_obj, svm_error_buf);
}

/*
 * Meant to write num in decimal. Bug: raw digit values go into buf and svm_num_buf, which nothing
 * fills, is copied back reversed.
 */
/* 0x001d3c00 */
static void svm_ltoa(unsigned long num, char *buf, long size)
{
    long i;
    long n;
    char *digit;

    for (i = 0; i < 32; i++) {
        digit = &buf[i];
        *digit = __moddi3(num, 10);
        num = __divdi3(num, 10);
        if (num == 0) {
            *digit = 0;
            break;
        }
    }
    n = strlen(svm_num_buf);
    if (n >= size - 1)
        n = size - 1;
    for (i = 0; i < n; i++)
        buf[i] = svm_num_buf[n - 1 - i];
    buf[i] = 0;
}

/* "first second" */
/* 0x001d3d18 */
static void svm_format_2num(unsigned long first, unsigned long second, char *buf, long size)
{
    int n;

    svm_ltoa(first, buf, size);
    n = size;
    strncat(buf, " ", n - strlen(buf) - 1);
    svm_ltoa(second, buf + strlen(buf), 4 - strlen(buf));
}

/* First free of the kind's four slots; returns the slot id, or -1. */
/* 0x001d3dc0 */
int SVM_SetCbSvr(int svtype, SvmFn fn, void *obj)
{
    SvmCb *cb;
    int i;

    svm_lock();
    for (i = 0; i < 4; i++) {
        cb = &svm_cb_tbl[svtype][i];
        if (cb->fn == 0) {
            cb->fn = fn;
            cb->obj = obj;
            break;
        }
    }
    if (i == 4)
        svm_error("1051001:SVM_SetCbSvr:too many server function");
    svm_unlock();
    if (i == 4)
        return -1;
    return i;
}

/* 0x001d3e70 */
void SVM_DelCbSvr(int svtype, unsigned int id)
{
    if (id >= 4)
        svm_error("1051002:SVM_DelCbSvr:illegal id");
    svm_lock();
    svm_cb_tbl[svtype][id].fn = 0;
    svm_cb_tbl[svtype][id].obj = 0;
    svm_unlock();
}

/* Bug: the "illegal svtype" check tests id, not svtype, against 7. */
/* 0x001d3ee0 */
void SVM_SetCbSvrId(int svtype, unsigned int id, SvmFn fn, void *obj)
{
    SvmCb *cb;

    if (id >= 4)
        svm_error("1071201:SVM_SetCbSvrId:illegal id");
    if (svtype < 0 || (int)id >= 7)
        svm_error("1071202:SVM_SetCbSvrId:illegal svtype");
    svm_lock();
    cb = &svm_cb_tbl[svtype][id];
    cb->fn = fn;
    cb->obj = obj;
    svm_unlock();
}

/* Brings that kind's server thread to the border between two passes. */
/* 0x001d3f80 */
void svm_set_svr_border_cb(int n, SvmFn fn, void *obj)
{
    svm_lock();
    svm_cb2_tbl[n].fn = fn;
    svm_cb2_tbl[n].obj = obj;
    svm_unlock();
}

/* If set. */
/* 0x001d3fd8 */
static void svm_goto_svr_border(int n)
{
    if (svm_cb2_tbl[n].fn != 0)
        svm_cb2_tbl[n].fn(svm_cb2_tbl[n].obj);
}

/* 0x001d4010 */
void svm_set_error_cb(void (*fn)(void *obj, char *msg), void *obj)
{
    svm_lock();
    svm_error_fn = fn;
    svm_error_obj = obj;
    svm_unlock();
}

/* 0x001d4048 */
void svm_set_lock_cb(void (*fn)(void *obj), void *obj)
{
    svm_lock_fn = fn;
    svm_lock_obj = obj;
}

/* 0x001d4058 */
void svm_set_unlock_cb(void (*fn)(void *obj), void *obj)
{
    svm_unlock_fn = fn;
    svm_unlock_obj = obj;
}

/* Marks the kind executing per call, counts the pass; returns the OR of results (work left). */
/* 0x001d4068 */
static int svm_exec_svt(int svtype)
{
    SvmCb *cb;
    int ret;
    int i;

    ret = 0;
    cb = svm_cb_tbl[svtype];
    for (i = 0; i < 4; i++, cb++) {
        if (cb->fn != 0) {
            svm_executing[svtype] = 1;
            ret |= cb->fn(cb->obj);
            svm_executing[svtype] = 0;
        }
    }
    svm_exec_count[svtype]++;
    return ret;
}

/* 0 when unset. Bug: the "illegal svtype" check tests id, not svtype, against 7. */
/* 0x001d4128 */
static int SVM_ExecSvrFuncId(int svtype, unsigned int id)
{
    SvmCb *cb;
    int ret;

    ret = 0;
    if (id >= 4)
        svm_error("1071301:SVM_ExecSvrFuncId:illegal id");
    if (svtype < 0 || (int)id >= 7)
        svm_error("1071302:SVM_ExecSvrFuncId:illegal svtype");
    cb = &svm_cb_tbl[svtype][id];
    if (cb->fn != 0)
        ret = cb->fn(cb->obj);
    return ret;
}

/* 0x001d41c8 */
int svm_exec_vint(void)
{
    return svm_exec_svt(0);
}

/* 0x001d41e8 */
int svm_exec_usr_vsync(void)
{
    return svm_exec_svt(1);
}

/* 0x001d4208 */
int svm_exec_vsync(void)
{
    return svm_exec_svt(2);
}

/* 0x001d4228 */
static int svm_exec_vsync_id0(void)
{
    return SVM_ExecSvrFuncId(2, 0);
}

/* 0x001d4248 */
static int svm_exec_vsync_id1(void)
{
    return SVM_ExecSvrFuncId(2, 1);
}

/* 0x001d4268 */
static int svm_exec_svt3(void)
{
    return svm_exec_svt(3);
}

/* 0x001d4288 */
int svm_exec_main(void)
{
    return svm_exec_svt(4);
}

/* 0x001d42a8 */
int svm_exec_mw_idle(void)
{
    return svm_exec_svt(5);
}

/* 0x001d42c8 */
int svm_exec_usr_idle(void)
{
    return svm_exec_svt(6);
}

/* 0 for kinds above 4. */
/* 0x001d42e8 */
static int svm_get_exec_count(unsigned int svtype)
{
    if (svtype >= 5)
        return 0;
    return svm_exec_count[svtype];
}

/* First call clears executing flags, lock callbacks, first four pass counters; counts calls. */
/* 0x001d4318 */
void svm_lib_init(void)
{
    int i;

    if (svm_init_count == 0) {
        memset(svm_executing, 0, 28);
        memset(&svm_lock_fn, 0, 8);
        memset(&svm_unlock_fn, 0, 8);
        for (i = 0; i < 4; i++)
            svm_exec_count[i] = 0;
    }
    svm_init_count++;
}

/* 0x001d43a0 */
void svm_lib_finish(void)
{
    svm_init_count--;
}
