#include "types_libc.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

extern RtCtx *_impure_ptr;

/*
 * Allocates the context's table of 32 signal handlers (_malloc_r allocator) and sets them all to
 * default; -1 when out of memory.
 */
/* 0x001f5288 */
int _init_signal_r(RtCtx *ptr)
{
    int i;

    if (ptr->sig_func == 0) {
        ptr->sig_func = (SigFunc *)_malloc_r(ptr, sizeof(SigFunc) * 32);
        if (ptr->sig_func == 0)
            return -1;
        for (i = 0; i < 32; i++)
            ptr->sig_func[i] = 0;
    }
    return 0;
}

/*
 * Installs handler func for signal sig (0..31) in the context, returning the previous one; sets
 * error 22 and returns -1 for a bad signal.
 */
/* 0x001f52f8 */
SigFunc _signal_r(RtCtx *ptr, int sig, SigFunc func)
{
    SigFunc old_func;

    if (sig < 0 || sig >= 32) {
        ptr->err = 22;
        return (SigFunc)-1;
    }
    if (ptr->sig_func == 0 && _init_signal_r(ptr) != 0)
        return (SigFunc)-1;
    old_func = ptr->sig_func[sig];
    ptr->sig_func[sig] = func;
    return old_func;
}

/*
 * Raises signal sig in the context: default handler sends it to the own process (_kill_r with
 * _getpid_r's id), ignore returns 0, error sets error 22 and returns 1, otherwise resets the
 * handler to default and calls it.
 */
/* 0x001f5388 */
int _raise_r(RtCtx *ptr, int sig)
{
    SigFunc func;
    int result = 0;

    if (sig < 0 || sig >= 32) {
        ptr->err = 22;
        return -1;
    }
    if (ptr->sig_func == 0 && _init_signal_r(ptr) != 0)
        return -1;
    switch ((int)ptr->sig_func[sig]) {
    case 0:
        return _kill_r(ptr, _getpid_r(ptr), sig);
    case 1:
        break;
    case -1:
        ptr->err = 22;
        result = 1;
        break;
    default:
        func = ptr->sig_func[sig];
        ptr->sig_func[sig] = 0;
        func(sig);
        break;
    }
    return result;
}

/*
 * Dispatches signal sig from a trap: returns -1 for a bad signal, 1 for default, 2 for error, 3
 * for ignore, else resets and calls the handler, 0.
 */
/* 0x001f5478 */
int __sigtramp_r(RtCtx *ptr, int sig)
{
    SigFunc func;

    if (sig < 0 || sig >= 32)
        return -1;
    if (ptr->sig_func == 0 && _init_signal_r(ptr) != 0)
        return -1;
    switch ((int)ptr->sig_func[sig]) {
    case 0:
        return 1;
    case -1:
        return 2;
    case 1:
        return 3;
    default:
        func = ptr->sig_func[sig];
        ptr->sig_func[sig] = 0;
        func(sig);
        return 0;
    }
}

/* Raises sig in the default context _impure_ptr (_raise_r). */
/* 0x001f5530 */
int raise(int sig)
{
    return _raise_r(_impure_ptr, sig);
}

/* Installs a signal handler in the default context _impure_ptr (_signal_r). */
/* 0x001f5558 */
SigFunc signal(int sig, SigFunc func)
{
    return _signal_r(_impure_ptr, sig, func);
}

/* Sets up the signal table of the default context _impure_ptr (_init_signal_r). */
/* 0x001f5588 */
int _init_signal(void)
{
    return _init_signal_r(_impure_ptr);
}

/* Trap-side signal dispatch in the default context _impure_ptr (__sigtramp_r). */
/* 0x001f55b0 */
int __sigtramp(int sig)
{
    return __sigtramp_r(_impure_ptr, sig);
}
