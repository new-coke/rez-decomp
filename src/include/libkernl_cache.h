/* EE data cache write-back and invalidate over a range. */

#ifndef REZ_LIBKERNL_CACHE_H
#define REZ_LIBKERNL_CACHE_H

void InvalidDCache(void *a, void *b);
void SyncDCache(void *top, void *end);
void _InvalidDCache(unsigned int start, unsigned int end);
void _SyncDCache(unsigned int start, unsigned int end);

#endif /* REZ_LIBKERNL_CACHE_H */
