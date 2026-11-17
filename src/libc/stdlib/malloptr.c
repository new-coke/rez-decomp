#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

/* av_: bins; bin_at(0)->size = binblocks, [2] = top */
#define av_a malloc_av

#define bin_at_a(i) ((mbinptr_a)((char *)&(av_a[2 * (i) + 2]) - 2 * 4))
#define top_a (bin_at_a(0)->fd)
#define last_remainder_a (bin_at_a(1))
#define idx2binblock_a(ix) ((unsigned long)1 << (ix / 4))
#define mark_binblock_a(ii) (bin_at_a(0)->size |= idx2binblock_a(ii))
#define chunk_at_offset_a(p, s) ((mchunkptr_a)(((char *)(p)) + (s)))
#define chunksize_a(p) ((p)->size & ~3)
#define unlink_a(P, BK, FD) { BK = P->bk; FD = P->fd; FD->bk = BK; BK->fd = FD; }

extern mbinptr_a malloc_av[];
extern char *malloc_sbrk_base;
extern struct mallinfo_a malloc_current_mallinfo;

/*
 * Malloc_trim: under the malloc lock releases whole 4096-byte pages from the top chunk (av_
 * malloc_av) back through _sbrk_r when no one else moved the break; updates sbrked_mem
 * (malloc_current_mallinfo, from sbrk_base malloc_sbrk_base); returns 1 if memory was released.
 */
/* steer(cast) */
/* 0x001f37e0 */
int _malloc_trim_r(struct _reent_a *reent_ptr, unsigned int pad)
{
    long top_size;
    long extra;
    char *current_brk;
    char *new_brk;
    unsigned long pagesz = 4096;

    ((void (*)(struct _reent_a *))__malloc_lock)(reent_ptr);

    top_size = chunksize_a(top_a);
    extra = ((top_size - pad - 16 + (pagesz - 1)) / pagesz - 1) * pagesz;

    if (extra < (long)pagesz) {
        ((void (*)(struct _reent_a *))__malloc_unlock)(reent_ptr);
        return 0;
    } else {
        current_brk = (char *)(_sbrk_r(reent_ptr, 0));
        if (current_brk != (char *)(top_a) + top_size) {
            ((void (*)(struct _reent_a *))__malloc_unlock)(reent_ptr);
            return 0;
        } else {
            new_brk = (char *)(_sbrk_r(reent_ptr, -extra));
            if (new_brk == (char *)(-1)) {
                current_brk = (char *)(_sbrk_r(reent_ptr, 0));
                top_size = current_brk - (char *)top_a;
                if (top_size >= (long)16) {
                    malloc_current_mallinfo.arena = current_brk - malloc_sbrk_base;
                    top_a->size = top_size | 1;
                }
                ((void (*)(struct _reent_a *))__malloc_unlock)(reent_ptr);
                return 0;
            } else {
                top_a->size = (top_size - extra) | 1;
                malloc_current_mallinfo.arena -= extra;
                ((void (*)(struct _reent_a *))__malloc_unlock)(reent_ptr);
                return 1;
            }
        }
    }
}
