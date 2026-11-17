#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

extern void main_exit(int status);

/*
 * Libc_abort (no name string; behaviour only). libc_abort: loops raise(SIGABRT=6) (raise) then
 * main_exit(1) (_exit). Proposed name: libc_abort
 */
/* 0x001f1d58 */
void libc_abort(void)
{
    while (1) {
        raise(6);
        main_exit(1);
    }
}
