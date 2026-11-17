#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

/*
 * _mbtowc_r (C locale): stores the byte at s into *pwc (or a dummy), returns 0 for NULL s, -1 for
 * n == 0, else whether the byte is non-zero.
 */
/* 0x001f4000 */
int _mbtowc_r(struct _reent_a *r, int *pwc, const char *s, unsigned int n)
{
    int dummy;
    unsigned char *t = (unsigned char *)s;

    if (pwc == NULL)
        pwc = &dummy;
    if (s != NULL && n == 0)
        return -1;
    if (s == NULL)
        return 0;
    *pwc = (int)*t;
    return (*t != '\0');
}
