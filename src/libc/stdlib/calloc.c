#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

extern struct _reent_a *_impure_ptr;

/* Calloc: forwards to the reentrant _calloc_r with the current reentrancy block _impure_ptr. */
/* 0x001ec998 */
void *calloc(unsigned int n, unsigned int size)
{
    return _calloc_r(_impure_ptr, n, size);
}
