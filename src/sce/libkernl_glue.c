/* C runtime glue: newlib system calls (TTY console, file/process stubs) and boot vblank waits. */

#include "types_libc.h"
#include "types_sce.h"
#include "libc_internal.h"
#include "libkernl_glue.h"
#include "kernel_syscalls.h"
#include "libkernl_tty.h"

typedef struct StatBuf {
    char pad_00[4];
    int mode;               /* 0x04 */
    char pad_08[64];
    long unk_48; /* 0x48: cleared by _fstat */
} StatBuf;

extern int crt_tty_ready;

/* The next console read or write reopens it. */
/* 0x001f6250 */
void crt_tty_reset(void)
{
    crt_tty_ready = 0;
}

/* Clears the INTC vblank-start status and polls until raised. */
/* 0x001f6260 */
void crt_wait_vblank(void)
{
    volatile int *intc_stat = (volatile int *)0x1000f000;

    *intc_stat = 4;
    while (!(*intc_stat & 4))
        ;
    *intc_stat = 4;
}

/* Also stops on the SetVSyncFlag flag; returns the value stored with it. */
/* 0x001f62a0 */
long crt_wait_vblank_flag(void)
{
    volatile int *intc_stat;
    volatile int flag;
    volatile long data;

    flag = 0;
    SetVSyncFlag(&flag, &data);
    intc_stat = (volatile int *)0x1000f000;
    *intc_stat = 4;
    while (!(*intc_stat & 4) && !flag)
        ;
    *intc_stat = 4;
    return data;
}

/* stdout/stderr via TTY, opened on first use; others -1. */
/* 0x001f6300 */
int _write(int fd, const void *buf, int n)
{
    if (fd == 1 || fd == 2) {
        if (!crt_tty_ready) {
            if (!libkernl_tty_open())
                return -1;
            crt_tty_ready = 1;
        }
        return libkernl_tty_write(buf, n);
    }
    return -1;
}

/* stdin via TTY, opened on first use; others -1. */
/* 0x001f6380 */
int _read(int fd, void *buf, int n)
{
    if (fd == 0) {
        if (!crt_tty_ready) {
            if (!libkernl_tty_open())
                return -1;
            crt_tty_ready = 1;
        }
        return libkernl_tty_read(buf, n);
    }
    return -1;
}

/* errno = EIO (5), returns -1. */
/* 0x001f63f8 */
static int crt_unsupported(void)
{
    *__errno() = 5;
    return -1;
}

/* 0x001f6420 */
int _close(void)
{
    return -1;
}

/* 0x001f6428 */
static int crt_return_minus_one(void)
{
    return -1;
}

/* 0x001f6430 */
int _lseek(void)
{
    return -1;
}

/* Every descriptor is the console. */
/* 0x001f64e8 */
int isatty(void)
{
    return 1;
}

/* Every descriptor is a character device. */
/* 0x001f64f0 */
int _fstat(int fd, StatBuf *st)
{
    st->mode = 0x2000;
    st->unk_48 = 0;
    return 0;
}

/* Always process 1. */
/* 0x001f6508 */
int _getpid(void)
{
    return 1;
}

/* Signalling process 1 exits with the signal number. */
/* 0x001f6510 */
int _kill(int pid, int sig)
{
    if (pid == 1)
        _Exit(sig);
    return 0;
}

/* errno = EIO (5), returns -1. */
/* 0x001f6538 */
static int crt_unsupported_2(void)
{
    *__errno() = 5;
    return -1;
}

/* errno = EIO (5), returns -1. */
/* 0x001f6560 */
static int crt_unsupported_3(void)
{
    *__errno() = 5;
    return -1;
}
