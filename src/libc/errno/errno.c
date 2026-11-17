#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

extern struct _reent_a *_impure_ptr;

/*
 * Returns the address of errno: the current reentrancy block _impure_ptr (errno is its first
 * word).
 */
/* 0x001eca88 */
int *__errno(void)
{
    return &_impure_ptr->_errno;
}
