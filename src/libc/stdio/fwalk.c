#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

/*
 * _fwalk: calls fn on every open stream (flags != 0) of every glue block starting at reent+472,
 * ORing the results.
 */
/* 0x001f3d28 */
int _fwalk(struct _reent_a *ptr, int (*function)(FILE_a *))
{
    FILE_a *fp;
    int n, ret = 0;
    struct _glue_a *g;

    for (g = (struct _glue_a *)((char *)ptr + 472); g != NULL; g = g->_next)
        for (fp = g->_iobs, n = g->_niobs; --n >= 0; fp++)
            if (fp->_flags != 0)
                ret |= (*function)(fp);
    return ret;
}
