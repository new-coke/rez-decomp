#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"

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
extern unsigned long malloc_trim_threshold;

/* trim_threshold */
extern unsigned long malloc_top_pad;

/* top_pad */
extern void __malloc_lock(struct _reent_a *ptr);

extern void __malloc_unlock(struct _reent_a *ptr);
extern int _malloc_trim_r(struct _reent_a *ptr, unsigned int pad);

/*
 * _free_r: under the malloc lock (__malloc_lock/__malloc_unlock) returns a chunk to the dlmalloc
 * bins (av_ malloc_av): merges with top and calls malloc_trim (_malloc_trim_r) with top_pad
 * (malloc_top_pad) past trim_threshold (malloc_trim_threshold), otherwise coalesces with free
 * neighbours (keeping last_remainder) and front-links into its small/large bin, marking binblocks.
 */
/* 0x001f34c0 */
void _free_r(struct _reent_a *reent_ptr, void *mem)
{
    mchunkptr_a p;
    unsigned int hd;
    unsigned int sz;
    int idx;
    mchunkptr_a next;
    unsigned int nextsz;
    unsigned int prevsz;
    mchunkptr_a bck;
    mchunkptr_a fwd;
    int islr;

    if (mem == 0)
        return;

    __malloc_lock(reent_ptr);

    p = (mchunkptr_a)((char *)mem - 8);
    hd = p->size;
    sz = hd & ~1;
    next = chunk_at_offset_a(p, sz);
    nextsz = chunksize_a(next);

    if (next == top_a) {
        sz += nextsz;
        if (!(hd & 1)) {
            prevsz = p->prev_size;
            p = chunk_at_offset_a(p, -prevsz);
            sz += prevsz;
            unlink_a(p, bck, fwd);
        }
        p->size = sz | 1;
        top_a = p;
        if ((unsigned long)(sz) >= (unsigned long)malloc_trim_threshold)
            _malloc_trim_r(reent_ptr, malloc_top_pad);
        __malloc_unlock(reent_ptr);
        return;
    }

    next->size = nextsz;
    islr = 0;

    if (!(hd & 1)) {
        prevsz = p->prev_size;
        p = chunk_at_offset_a(p, -prevsz);
        sz += prevsz;
        if (p->fd == last_remainder_a)
            islr = 1;
        else
            unlink_a(p, bck, fwd);
    }

    if (!(chunk_at_offset_a(next, nextsz)->size & 1)) {
        sz += nextsz;
        if (!islr && next->fd == last_remainder_a) {
            islr = 1;
            last_remainder_a->fd = last_remainder_a->bk = p;
            p->fd = p->bk = last_remainder_a;
        } else
            unlink_a(next, bck, fwd);
    }

    p->size = sz | 1;
    chunk_at_offset_a(p, sz)->prev_size = sz;
    if (!islr) {
        if (sz < 512) {
            idx = (((unsigned long)(sz)) >> 3);
            mark_binblock_a(idx);
            bck = bin_at_a(idx);
            fwd = bck->fd;
            p->bk = bck;
            p->fd = fwd;
            fwd->bk = bck->fd = p;
        } else {
            idx = (((((unsigned long)(sz)) >> 9) == 0) ? (((unsigned long)(sz)) >> 3) :
                   ((((unsigned long)(sz)) >> 9) <= 4) ? 56 + (((unsigned long)(sz)) >> 6) :
                   ((((unsigned long)(sz)) >> 9) <= 20) ? 91 + (((unsigned long)(sz)) >> 9) :
                   ((((unsigned long)(sz)) >> 9) <= 84) ? 110 + (((unsigned long)(sz)) >> 12) :
                   ((((unsigned long)(sz)) >> 9) <= 340) ? 119 + (((unsigned long)(sz)) >> 15) :
                   ((((unsigned long)(sz)) >> 9) <= 1364) ? 124 + (((unsigned long)(sz)) >> 18) :
                   126);
            bck = bin_at_a(idx);
            fwd = bck->fd;
            if (fwd == bck)
                mark_binblock_a(idx);
            else {
                while (fwd != bck && sz < chunksize_a(fwd))
                    fwd = fwd->fd;
                bck = fwd->bk;
            }
            p->bk = bck;
            p->fd = fwd;
            fwd->bk = bck->fd = p;
        }
    }

    __malloc_unlock(reent_ptr);
}
