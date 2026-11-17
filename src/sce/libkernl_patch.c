/*
 * EE kernel patches and the deferred-operation thread: fixed syscalls and code at start-up;
 * interrupt-queued thread ops (wake-up, ready-queue rotation, suspend) on a helper thread.
 */

#include "types_libc.h"
#include "types_sce.h"
#include "libkernl_patch.h"
#include "kernel_syscalls.h"
#include "libkernl_tty.h"
#include "cri.h"

typedef struct SyscallPatch {
    int num;
    void *func;
} SyscallPatch;

extern SyscallPatch libkernl_syscall_patches[8];
extern char libkernl_patch_code[];
extern char libkernl_patch_code2[];
extern int libkernl_defer_sema;
extern int libkernl_defer_tid;
extern DeferQueue libkernl_defer_queue;
extern char libkernl_defer_stack[];
extern char _gp[];
extern int CreateSema(SemaParam *p);

/* As 32-bit words; returns 0. */
/* 0x001f6a48 */
static int libkernl_copy_words(unsigned int *dst, unsigned int *src, unsigned int size)
{
    unsigned int i;

    size >>= 2;
    for (i = 0; i < size; i++)
        *dst++ = *src++;
    return 0;
}

/*
 * Skipped when timer 3 mode bit 8 says not needed. First two libkernl_syscall_patches entries get
 * replacements, both patch code blocks are copied, caches flushed, the other six reset.
 */
/* 0x001f6a90 */
void libkernl_install_patches(void)
{
    unsigned int i;
    SyscallPatch *patch;

    if (*(volatile unsigned int *)0x10001810 & 0x100)
        return;
    RFU116_SetSyscall(libkernl_syscall_patches[0].num, libkernl_syscall_patches[0].func);
    RFU090((void *)0x80076000, libkernl_patch_code, 0x740);
    RFU090((void *)0x00082000, libkernl_patch_code2, 40);
    FlushCache(0);
    FlushCache(2);
    RFU116_SetSyscall(libkernl_syscall_patches[1].num, libkernl_syscall_patches[1].func);
    for (i = 2, patch = &libkernl_syscall_patches[2]; i < 8; i++, patch++)
        RFU116_SetSyscall(patch->num, RFU091(patch->num));
}

/*
 * Per queued command: 0 wake-up, 1 rotate ready queue, 2 suspend. Never returns. Typo is original.
 */
/* 0x001f6b68 */
static void libkernl_defer_thread(DeferQueue *dq)
{
    int i;

    for (;;) {
        WaitSema(libkernl_defer_sema);
        i = dq->rd & 0x1ff;
        dq->rd = i + 1;
        switch (dq->cmd[i].type) {
        case 0:
            WakeupThread(dq->cmd[i].arg);
            break;
        case 1:
            RotateThreadReadyQueue(dq->cmd[i].arg);
            break;
        case 2:
            SuspendThread(dq->cmd[i].arg);
            break;
        default:
            libkernl_printf("## internel error in libkernl.a!\n");
            break;
        }
    }
}

/*
 * 1 KB stack; drops the caller to priority 1 so the helper (0) runs first. -1 if running or failed.
 */
/* 0x001f6c40 */
int libkernl_start_defer_thread(void)
{
    ThreadParam tp;
    SemaParam sema_param;

    if (libkernl_defer_tid > 0)
        return -1;
    sema_param.init_count = 0;
    sema_param.max_count = 255;
    if ((libkernl_defer_sema = CreateSema(&sema_param)) < 0)
        return -1;
    tp.entry = (void (*)(void *))libkernl_defer_thread;
    tp.stack = libkernl_defer_stack;
    tp.stack_size = 0x400;
    tp.gp = _gp;
    tp.init_priority = 0;
    if ((libkernl_defer_tid = CreateThread(&tp)) < 0) {
        DeleteSema(libkernl_defer_sema);
        return -1;
    }
    libkernl_defer_queue.rd = 0;
    libkernl_defer_queue.wr = 0;
    _StartThread(libkernl_defer_tid, &libkernl_defer_queue);
    ChangeThreadPriority(GetThreadId(), 1);
    return libkernl_defer_tid;
}

/* prio below 128. -1 if out of range or the helper is not running. */
/* 0x001f6db0 */
static int libkernl_defer_rotate_queue(int prio)
{
    int i;
    DeferQueue *dq;

    if ((unsigned int)prio >= 128 || !libkernl_defer_tid)
        return -1;
    dq = &libkernl_defer_queue;
    i = dq->wr & 0x1ff;
    dq->wr = i + 1;
    dq->cmd[i].type = 1;
    dq->cmd[i].arg = prio;
    iSignalSema(libkernl_defer_sema);
    return prio;
}
