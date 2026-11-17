#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

extern struct _reent_a *_impure_ptr;

/*
 * Assertion failure: prints "assertion \"%s\" failed: file \"%s\", line %d" to stderr of the
 * current reentrancy block _impure_ptr (fiprintf) and aborts (libc_abort).
 */
/* 0x001ec940 */
void __assert(const char *file, int line, const char *failedexpr)
{
    fiprintf(_impure_ptr->_stderr, "assertion \"%s\" failed: file \"%s\", line %d\n",
                 failedexpr, file, line);
    libc_abort();
}
