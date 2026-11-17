#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"

extern struct _reent_a *_impure_ptr;

/* Strtok: strtok_r with the save pointer at offset 92 of the reentrancy block _impure_ptr. */
/* 0x001ef4c0 */
char *strtok(char *s, const char *delim)
{
    return strtok_r(s, delim, (char **)((char *)_impure_ptr + 92));
}
