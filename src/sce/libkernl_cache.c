/* EE kernel library: data cache write-back and invalidate over a range (64-byte lines). */

#include "types_libc.h"
#include "types_sce.h"
#include "libkernl_cache.h"

/* [start, end]; line walk in _SyncDCache. */
/* 0x001f66b0 */
static void libkernl_sync_dcache_nolock(unsigned int start, unsigned int end)
{
    _SyncDCache(start & ~63, end & ~63);
}

/* [start, end]; line walk in _InvalidDCache. */
/* 0x001f67f0 */
static void libkernl_invalid_dcache_nolock(unsigned int start, unsigned int end)
{
    _InvalidDCache(start & ~63, end & ~63);
}
