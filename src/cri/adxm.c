/*
 * ADX thread manager: SVM servers on their own EE threads. Vsync handler wakes the vsync server;
 * middleware idle thread runs when the game yields; the SVM lock raises the caller to top
 * priority so no server runs inside it. Extended setup adds user vsync and user idle threads.
 */

#include "cri.h"
#include "adxm.h"
#include "adx_error.h"
#include "adxt.h"
#include "kernel_syscalls.h"
#include "libkernl_patch.h"
#include "svm.h"

static void adxm_wakeup_thread(int tid);
static void adxm_sleep_thread(void);
static int adxm_resume_thread(int tid);
static int adxm_suspend_thread(int tid);
static void adxm_end_mw_idle_thread(void);
static void adxm_end_vsync_thread(void);
static void adxm_end_idle_thread(void);

extern int _gp;
extern char adxm_idle_stack[];
extern char adxm_svt1_stack[];
extern char adxm_svt2_stack[];
extern char adxm_svt5_stack[];
extern char adxm_svt6_stack[];
extern int adxm_ext_mode;

/* SVM lock: outermost raises the caller to adxm_thread_prio[0], resumes the idle thread; nests. */
/* 0x001c62d0 */
static void adxm_thread_setup_hook(void)
{
    int st[12];
    int tid;

    if (adxm_setup_count == 0) {
        tid = GetThreadId();
        ReferThreadStatus(tid, st);
        ChangeThreadPriority(tid, adxm_thread_prio[0]);
        adxm_setup_prio = st[6];
        adxm_setup_thread = tid;
        if (adxm_setup_tid != 0) {
            adxm_resume_thread(adxm_setup_tid);
            adxm_setup_tid;
        }
    }
    adxm_setup_count++;
}

/* SVM unlock: outermost suspends the idle thread, restores priority; count floors at 0. */
/* 0x001c6350 */
static void adxm_thread_shutdown_hook(void)
{
    if (--adxm_setup_count == 0) {
        if (adxm_setup_tid != 0) {
            adxm_suspend_thread(adxm_setup_tid);
            adxm_setup_tid;
        }
        ChangeThreadPriority(GetThreadId(), adxm_setup_prio);
    }
    if (adxm_setup_count < 0)
        adxm_setup_count = 0;
}

/*
 * Raises tid to top priority and wakes it until it clears adxm_border_wait (internal error after
 * 200000000 tries), then sets priority prio and suspends it.
 */
/* 0x001c63b0 */
static void adxm_goto_svr_border(int tid, int prio)
{
    int i;

    if (tid != 0) {
        adxm_border_wait = 1;
        ChangeThreadPriority(tid, adxm_thread_prio[0]);
        for (i = 0; i < 200000000; i++) {
            adxm_resume_thread(tid);
            adxm_wakeup_thread(tid);
            if (adxm_border_wait == 0)
                break;
        }
        if (i == 200000000)
            svm_error("1060102: Internal Error: adxm_goto_svr_border");
        ChangeThreadPriority(tid, prio);
        if (tid != 0)
            adxm_suspend_thread(tid);
    }
}

/* 0x001c6488 */
static void adxm_goto_svr_border_mw_idle(void)
{
    adxm_goto_svr_border(adxm_svt5_tid, adxm_thread_prio[5]);
}

/* 0x001c6498 */
static void adxm_goto_svr_border_usr_idle(void)
{
    adxm_goto_svr_border(adxm_svt6_tid, adxm_thread_prio[6]);
}

/* 0x001c64a8 */
static void adxm_idle_step(void)
{
}

/* Spins at low priority so the kernel always has a thread while the SVM lock holds servers off. */
/* 0x001c64b0 */
static void adxm_idle_thread(void)
{
    while (adxm_idle_exit_req == 0) {
        adxm_idle_loop_cnt++;
        adxm_idle_step();
    }
    adxm_idle_exited = 1;
    ExitDeleteThread();
}

/* Per wake: holds off the user idle thread, runs the user vsync servers. */
/* 0x001c64f8 */
static void adxm_usr_vsync_thread(void)
{
    while (adxm_svt1_exit_req == 0) {
        adxm_svt1_loop_cnt++;
        if (adxm_svt6_tid != 0)
            adxm_suspend_thread(adxm_svt6_tid);
        svm_exec_usr_vsync();
        adxm_sleep_thread();
    }
    adxm_svt1_exited = 1;
    ExitDeleteThread();
}

/* Per wake: holds off the middleware idle thread, runs vsync servers, wakes the game thread. */
/* 0x001c6560 */
static void adxm_vsync_thread(void)
{
    while (adxm_svt2_exit_req == 0) {
        adxm_svt2_loop_cnt++;
        if (adxm_svt5_tid != 0)
            adxm_suspend_thread(adxm_svt5_tid);
        adxm_svt2_running = 1;
        svm_exec_vsync();
        adxm_svt2_running = 0;
        adxm_wakeup_thread(adxm_idle_tid);
        adxm_sleep_thread();
    }
    adxm_svt2_exited = 1;
    ExitDeleteThread();
}

/* Runs idle servers; stops at a requested border; when idle, hands over to the user idle thread. */
/* 0x001c65e8 */
static void adxm_mw_idle_thread(void)
{
    int ret;

    while (adxm_svt5_exit_req == 0) {
        adxm_svt5_loop_cnt++;
        ret = svm_exec_mw_idle();
        if (adxm_border_wait == 1) {
            adxm_border_wait = 0;
            adxm_sleep_thread();
        }
        if (ret == 0) {
            if (adxm_resume_thread(adxm_svt6_tid) == adxm_svt6_tid)
                adxm_wakeup_thread(adxm_svt6_tid);
            adxm_sleep_thread();
        }
    }
    adxm_svt5_exited = 1;
    ExitDeleteThread();
}

/* Runs user idle servers; stops at a requested border; sleeps when idle. */
/* 0x001c6698 */
static void adxm_usr_idle_thread(void)
{
    int ret;

    while (adxm_svt6_exit_req == 0) {
        adxm_svt6_loop_cnt++;
        ret = svm_exec_usr_idle();
        if (adxm_border_wait == 1) {
            adxm_border_wait = 0;
            adxm_sleep_thread();
        }
        if (ret == 0)
            adxm_sleep_thread();
    }
    adxm_svt6_exited = 1;
    ExitDeleteThread();
}

/* Interrupt context; only if sleeping (status 4 or 12). */
/* 0x001c6720 */
static void adxm_iwakeup_thread(int tid)
{
    int st[12];

    if (tid != 0) {
        iReferThreadStatus(tid, st);
        if (st[0] == 4 || st[0] == 12)
            iWakeupThread(tid);
    }
}

/* Only if sleeping (status 4 or 12). */
/* 0x001c6770 */
static void adxm_wakeup_thread(int tid)
{
    int st[12];

    if (tid != 0) {
        ReferThreadStatus(tid, st);
        if (st[0] == 4 || st[0] == 12)
            WakeupThread(tid);
    }
}

/* 0x001c67c0 */
static void adxm_sleep_thread(void)
{
    SleepThread();
}

/* Only if suspended (status 8 or 12); else 0. */
/* 0x001c67c8 */
static int adxm_resume_thread(int tid)
{
    int st[12];
    int ret;

    ret = 0;
    if (tid != 0) {
        ReferThreadStatus(tid, st);
        if (st[0] == 8 || st[0] == 12)
            ret = ResumeThread(tid);
    }
    return ret;
}

/* Unless already suspended (status 8 or 12); else 0. */
/* 0x001c6828 */
static int adxm_suspend_thread(int tid)
{
    int st[12];
    int ret;

    ret = 0;
    if (tid != 0) {
        ReferThreadStatus(tid, st);
        if (st[0] != 8 && st[0] != 12)
            ret = SuspendThread(tid);
    }
    return ret;
}

/* 4096-byte stack adxm_idle_stack, id adxm_setup_tid; suspended, priority adxm_thread_prio[1]. */
/* 0x001c6888 */
static void adxm_create_idle_thread(void)
{
    AdxThrdParam tp;

    tp.entry = adxm_idle_thread;
    tp.stack = adxm_idle_stack;
    tp.stack_size = 4096;
    tp.prio = adxm_init_prio;
    tp.gp = &_gp;
    adxm_setup_tid = CreateThread(&tp);
    adxm_setup_tid;
    _StartThread(adxm_setup_tid, 0);
    if (adxm_setup_tid != 0) {
        adxm_suspend_thread(adxm_setup_tid);
        adxm_setup_tid;
    }
    ChangeThreadPriority(adxm_setup_tid, adxm_thread_prio[1]);
}

/* 4096-byte stack adxm_svt1_stack, id adxm_svt1_tid, priority adxm_thread_prio[2]. */
/* 0x001c6920 */
static void adxm_create_usr_vsync_thread(void)
{
    AdxThrdParam tp;

    tp.entry = adxm_usr_vsync_thread;
    tp.stack = adxm_svt1_stack;
    tp.stack_size = 4096;
    tp.prio = adxm_init_prio;
    tp.gp = &_gp;
    adxm_svt1_tid = CreateThread(&tp);
    adxm_svt1_tid;
    _StartThread(adxm_svt1_tid, 0);
    ChangeThreadPriority(adxm_svt1_tid, adxm_thread_prio[2]);
}

/* 4096-byte stack adxm_svt2_stack, id adxm_svt2_tid, priority adxm_thread_prio[3]. */
/* 0x001c6998 */
static void adxm_create_vsync_thread(void)
{
    AdxThrdParam tp;

    tp.entry = adxm_vsync_thread;
    tp.stack = adxm_svt2_stack;
    tp.stack_size = 4096;
    tp.prio = adxm_init_prio;
    tp.gp = &_gp;
    adxm_svt2_tid = CreateThread(&tp);
    adxm_svt2_tid;
    _StartThread(adxm_svt2_tid, 0);
    ChangeThreadPriority(adxm_svt2_tid, adxm_thread_prio[3]);
}

/* 8192-byte stack adxm_svt5_stack, id adxm_svt5_tid, priority adxm_thread_prio[5]; suspended. */
/* 0x001c6a10 */
static void adxm_create_mw_idle_thread(void)
{
    AdxThrdParam tp;

    tp.entry = adxm_mw_idle_thread;
    tp.stack = adxm_svt5_stack;
    tp.stack_size = 8192;
    tp.prio = adxm_init_prio;
    tp.gp = &_gp;
    adxm_svt5_tid = CreateThread(&tp);
    adxm_svt5_tid;
    _StartThread(adxm_svt5_tid, 0);
    ChangeThreadPriority(adxm_svt5_tid, adxm_thread_prio[5]);
    adxm_suspend_thread(adxm_svt5_tid);
    adxm_svt5_tid;
}

/* 8192-byte stack adxm_svt6_stack, id adxm_svt6_tid, priority adxm_thread_prio[6]; suspended. */
/* 0x001c6a98 */
static void adxm_create_usr_idle_thread(void)
{
    AdxThrdParam tp;

    tp.entry = adxm_usr_idle_thread;
    tp.stack = adxm_svt6_stack;
    tp.stack_size = 8192;
    tp.prio = adxm_init_prio;
    tp.gp = &_gp;
    adxm_svt6_tid = CreateThread(&tp);
    adxm_svt6_tid;
    _StartThread(adxm_svt6_tid, 0);
    ChangeThreadPriority(adxm_svt6_tid, adxm_thread_prio[6]);
    adxm_suspend_thread(adxm_svt6_tid);
    adxm_svt5_tid;
}

/* 1 under adxm_thread_init_ex (all five server threads). */
/* 0x001c6b20 */
static int adxm_get_ext_mode(void)
{
    return adxm_ext_mode;
}

/* 0x001c6b28 */
static int adxm_exec_usr_vsync(void)
{
    return svm_exec_usr_vsync();
}

/* 0x001c6b48 */
static int adxm_exec_vsync(void)
{
    return svm_exec_vsync();
}

/* 0x001c6b68 */
static int adxm_exec_main(void)
{
    return svm_exec_main();
}

/* 0x001c6b88 */
static int adxm_exec_mw_idle(void)
{
    return svm_exec_mw_idle();
}

/* 0x001c6ba8 */
static int adxm_exec_usr_idle(void)
{
    return svm_exec_usr_idle();
}

/* Every server kind once, for use without server threads. */
/* 0x001c6bc8 */
static int adxm_exec_all_svt(void)
{
    svm_exec_usr_vsync();
    svm_exec_vsync();
    svm_exec_main();
    svm_exec_mw_idle();
    svm_exec_usr_idle();
    return 0;
}

/* 0x001c6c08 */
static int adxm_is_initialized(void)
{
    return adxm_init_count != 0;
}

/* 0x001c6c18 */
static void adxm_set_error_cb(void (*fn)(void *obj, char *msg), void *obj)
{
    svm_set_error_cb(fn, obj);
}

/*
 * First call: lock callbacks, priorities from prm (or defaults), idle, vsync and middleware idle
 * threads; the game thread drops to adxm_thread_prio[4]. Counts calls.
 */
/* 0x001c6c20 */
void adxm_thread_init(int *prm)
{
    int st[12];

    adxps2_ver_str;
    if (adxm_init_count == 0) {
        svm_lib_init();
        svm_set_lock_cb(adxm_thread_setup_hook, 0);
        svm_set_unlock_cb(adxm_thread_shutdown_hook, 0);
        if (prm == 0) {
            adxm_thread_prio[4] = 24;
            adxm_thread_prio[0] = 1;
            adxm_thread_prio[1] = 8;
            adxm_thread_prio[3] = 16;
            adxm_thread_prio[5] = 25;
        } else {
            adxm_thread_prio[4] = prm[3];
            adxm_thread_prio[0] = prm[0];
            adxm_thread_prio[1] = prm[1];
            adxm_thread_prio[3] = prm[2];
            adxm_thread_prio[5] = prm[4];
        }
        adxm_idle_tid = GetThreadId();
        ReferThreadStatus(adxm_idle_tid, st);
        adxm_init_prio = st[6];
        adxm_create_idle_thread();
        adxm_create_vsync_thread();
        adxm_create_mw_idle_thread();
        ChangeThreadPriority(adxm_idle_tid, adxm_thread_prio[4]);
        svm_set_svr_border_cb(5, adxm_goto_svr_border_mw_idle, 0);
    }
    adxm_init_count++;
}

/* Last user: ends the three server threads, restores priority, removes lock callbacks, ends SVM. */
/* 0x001c6d50 */
static void adxm_thread_finish(void)
{
    if (--adxm_init_count == 0) {
        adxm_end_mw_idle_thread();
        adxm_end_vsync_thread();
        adxm_end_idle_thread();
        adxm_idle_tid = GetThreadId();
        ChangeThreadPriority(adxm_idle_tid, adxm_init_prio);
        svm_set_lock_cb(0, 0);
        svm_set_unlock_cb(0, 0);
        svm_lib_finish();
    }
}

/* Requests exit, wakes it at top priority until it exits, clears its flags and id. */
/* 0x001c6dd0 */
static void adxm_end_usr_idle_thread(void)
{
    while (adxm_svt6_exited == 0) {
        adxm_svt6_exit_req = 1;
        ChangeThreadPriority(adxm_svt6_tid, 1);
        WakeupThread(adxm_svt6_tid);
        ResumeThread(adxm_svt6_tid);
    }
    adxm_svt6_exited = 0;
    adxm_svt6_exit_req = 0;
    adxm_svt6_tid = 0;
}

/* As adxm_end_usr_idle_thread. */
/* 0x001c6e40 */
static void adxm_end_mw_idle_thread(void)
{
    while (adxm_svt5_exited == 0) {
        adxm_svt5_exit_req = 1;
        ChangeThreadPriority(adxm_svt5_tid, 1);
        WakeupThread(adxm_svt5_tid);
        ResumeThread(adxm_svt5_tid);
    }
    adxm_svt5_exited = 0;
    adxm_svt5_exit_req = 0;
    adxm_svt5_tid = 0;
}

/* As adxm_end_usr_idle_thread. */
/* 0x001c6eb0 */
static void adxm_end_vsync_thread(void)
{
    while (adxm_svt2_exited == 0) {
        adxm_svt2_exit_req = 1;
        ChangeThreadPriority(adxm_svt2_tid, 1);
        WakeupThread(adxm_svt2_tid);
        ResumeThread(adxm_svt2_tid);
    }
    adxm_svt2_exited = 0;
    adxm_svt2_exit_req = 0;
    adxm_svt2_tid = 0;
}

/* As adxm_end_usr_idle_thread. */
/* 0x001c6f20 */
static void adxm_end_usr_vsync_thread(void)
{
    while (adxm_svt1_exited == 0) {
        adxm_svt1_exit_req = 1;
        ChangeThreadPriority(adxm_svt1_tid, 1);
        WakeupThread(adxm_svt1_tid);
        ResumeThread(adxm_svt1_tid);
    }
    adxm_svt1_exited = 0;
    adxm_svt1_exit_req = 0;
    adxm_svt1_tid = 0;
}

/* As adxm_end_usr_idle_thread. */
/* 0x001c6f90 */
static void adxm_end_idle_thread(void)
{
    while (adxm_idle_exited == 0) {
        adxm_idle_exit_req = 1;
        ChangeThreadPriority(adxm_setup_tid, 1);
        WakeupThread(adxm_setup_tid);
        ResumeThread(adxm_setup_tid);
    }
    adxm_idle_exited = 0;
    adxm_idle_exit_req = 0;
    adxm_setup_tid = 0;
}

/* 0x001c7000 */
static void adxm_lock(void)
{
    svm_lock();
}

/* 0x001c7008 */
static void adxm_unlock(void)
{
    svm_unlock();
}

/*
 * Vsync interrupt: unless re-entered, wakes the vsync server (and user vsync, extended), counts
 * the frame, runs vsync interrupt servers; then chains to adxm_vsync_cb.
 */
/* 0x001c7010 */
static int adxm_int_handler(int cause)
{
    if (adxm_vsync_busy == 0) {
        adxm_vsync_busy = 1;
        if (adxm_init_count > 0) {
            if (adxm_svt2_running != 1)
                adxm_iwakeup_thread(adxm_svt2_tid);
            if (adxm_get_ext_mode() == 1)
                adxm_iwakeup_thread(adxm_svt1_tid);
            adxm_vsync_cnt++;
        }
        svm_exec_vint();
        adxm_vsync_busy = 0;
    }
    if (adxm_vsync_cb != 0)
        return adxm_vsync_cb(cause);
    return 0;
}

/* 0x001c70c0 */
int adxm_call_int_handler(int cause)
{
    return adxm_int_handler(cause);
}

/* Middleware idle thread, plus user idle in the extended setup. */
/* 0x001c70e0 */
static void adxm_resume_svr_threads(void)
{
    if (adxm_resume_thread(adxm_svt5_tid) == adxm_svt5_tid)
        adxm_wakeup_thread(adxm_svt5_tid);
    if (adxm_get_ext_mode() == 1) {
        if (adxm_resume_thread(adxm_svt6_tid) == adxm_svt6_tid)
            adxm_wakeup_thread(adxm_svt6_tid);
    }
}

/* 0x001c7150 */
static void adxm_yield_to_svr(void)
{
    adxm_sleep_thread();
}

/* Resumes idle servers; the game thread sleeps until the next vsync. */
/* 0x001c7158 */
void adxm_run_svr_threads(void)
{
    adxm_resume_svr_threads();
    adxm_yield_to_svr();
}

/* 0x001c7178 */
static void adxm_run_svr_threads_2(void)
{
    adxm_run_svr_threads();
}

/* Registered by ADXT: unless re-entered, runs the ADXT and sector read servers. */
/* 0x001c7180 */
int adxt_svm_server(void)
{
    if (adxm_adxt_svr_busy == 0) {
        adxm_adxt_svr_busy = 1;
        adxt_exec_server();
        adx_exec_srd();
        adxm_adxt_svr_busy = 0;
    }
    return 0;
}

/* Four priorities (middleware idle fixed at 25); prm null = defaults. */
/* 0x001c71c0 */
static void adxm_thread_init_4prio(int *prm)
{
    int prio[5];
    int *pp;

    if (prm != 0) {
        prio[3] = prm[3];
        prio[0] = prm[0];
        prio[1] = prm[1];
        prio[2] = prm[2];
        prio[4] = 25;
        pp = prio;
    } else {
        pp = 0;
    }
    adxm_thread_init(pp);
}

/* 0x001c7218 */
static void adxm_thread_finish_4prio(void)
{
    adxm_thread_finish();
}

/* No re-entry guard. */
/* 0x001c7220 */
static void adxt_exec_server_all(void)
{
    adxt_exec_server();
    adx_exec_srd();
}

/* 0x001c7240 */
static void adxm_lock_2(void)
{
    svm_lock();
}

/* 0x001c7248 */
static void adxm_unlock_2(void)
{
    svm_unlock();
}

/* Seven priorities (defaults 1, 8, 15, 16, 24, 25, 26); adds user vsync and user idle threads. */
/* 0x001c7250 */
static void adxm_thread_init_ex(int *prm)
{
    int st[12];

    adxps2_ver_str;
    if (adxm_init_count == 0) {
        svm_lib_init();
        svm_set_lock_cb(adxm_thread_setup_hook, 0);
        svm_set_unlock_cb(adxm_thread_shutdown_hook, 0);
        if (prm == 0) {
            adxm_thread_prio[4] = 24;
            adxm_thread_prio[0] = 1;
            adxm_thread_prio[1] = 8;
            adxm_thread_prio[2] = 15;
            adxm_thread_prio[3] = 16;
            adxm_thread_prio[5] = 25;
            adxm_thread_prio[6] = 26;
        } else {
            adxm_thread_prio[4] = prm[4];
            adxm_thread_prio[0] = prm[0];
            adxm_thread_prio[1] = prm[1];
            adxm_thread_prio[2] = prm[2];
            adxm_thread_prio[3] = prm[3];
            adxm_thread_prio[5] = prm[5];
            adxm_thread_prio[6] = prm[6];
        }
        adxm_idle_tid = GetThreadId();
        ReferThreadStatus(adxm_idle_tid, st);
        adxm_init_prio = st[6];
        adxm_create_idle_thread();
        adxm_create_usr_vsync_thread();
        adxm_create_vsync_thread();
        adxm_create_mw_idle_thread();
        adxm_create_usr_idle_thread();
        ChangeThreadPriority(adxm_idle_tid, adxm_thread_prio[4]);
        svm_set_svr_border_cb(5, adxm_goto_svr_border_mw_idle, 0);
        svm_set_svr_border_cb(6, adxm_goto_svr_border_usr_idle, 0);
        adxm_ext_mode = 1;
    }
    adxm_init_count++;
}

/* Last user: ends all five server threads, restores priority, removes lock callbacks, ends SVM. */
/* 0x001c73d0 */
static void adxm_thread_finish_ex(void)
{
    if (--adxm_init_count == 0) {
        adxm_end_usr_idle_thread();
        adxm_end_mw_idle_thread();
        adxm_end_vsync_thread();
        adxm_end_usr_vsync_thread();
        adxm_end_idle_thread();
        adxm_idle_tid = GetThreadId();
        ChangeThreadPriority(adxm_idle_tid, adxm_init_prio);
        svm_set_lock_cb(0, 0);
        svm_set_unlock_cb(0, 0);
        adxm_ext_mode = 0;
        svm_lib_finish();
    }
}

/* 0x001c7460 */
static void adxm_set_usr_vsync_cb(void *fn, void *obj)
{
    SVM_SetCbSvrId(1, 0, fn, obj);
}

/* 0x001c7478 */
static void adxm_set_usr_idle_cb(void *fn, void *obj)
{
    SVM_SetCbSvrId(6, 0, fn, obj);
}
