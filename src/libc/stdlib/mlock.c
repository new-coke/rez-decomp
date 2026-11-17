#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

/* Empty hook (no-op), e.g. a malloc lock stub. */
/* 0x001ed6a8 */
void __malloc_lock(void)
{
}

/* Empty hook (no-op), e.g. a malloc unlock stub. */
/* 0x001ed6b0 */
void __malloc_unlock(void)
{
}
