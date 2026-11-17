#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

/*
 * Default math-exception handler: compares arg1 with itself (NaN test) and returns 0 in every
 * case.
 */
/* 0x001e9ab8 */
int matherr(struct exception *x)
{
    int n = 0;

    if (x->arg1 != x->arg1)
        return 0;
    return n;
}
