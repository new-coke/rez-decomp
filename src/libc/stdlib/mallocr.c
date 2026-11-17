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
#define last_a(b) ((b)->bk)
#define first_a(b) ((b)->fd)
#define next_bin_a(b) ((mbinptr_a)((char *)(b) + 2 * sizeof(mbinptr_a)))
#define prev_bin_a(b) ((mbinptr_a)((char *)(b) - 2 * sizeof(mbinptr_a)))
#define binblocks_a (bin_at_a(0)->size)
#define set_inuse_bit_at_offset_a(p, s) (((mchunkptr_a)(((char *)(p)) + (s)))->size |= 1)

#define link_last_remainder_a(P) \
    { last_remainder_a->fd = last_remainder_a->bk = P; P->fd = P->bk = last_remainder_a; }

#define clear_last_remainder_a \
    (last_remainder_a->fd = last_remainder_a->bk = last_remainder_a)

#define long_sub_size_t_a(x, y) \
    (sizeof(long) > sizeof(unsigned int) && x < y ? -(long)(y - x) : (long)(x - y))

#define bin_index_a(sz) \
    (((((unsigned long)(sz)) >> 9) == 0) ? (((unsigned long)(sz)) >> 3) : \
     ((((unsigned long)(sz)) >> 9) <= 4) ? 56 + (((unsigned long)(sz)) >> 6) : \
     ((((unsigned long)(sz)) >> 9) <= 20) ? 91 + (((unsigned long)(sz)) >> 9) : \
     ((((unsigned long)(sz)) >> 9) <= 84) ? 110 + (((unsigned long)(sz)) >> 12) : \
     ((((unsigned long)(sz)) >> 9) <= 340) ? 119 + (((unsigned long)(sz)) >> 15) : \
     ((((unsigned long)(sz)) >> 9) <= 1364) ? 124 + (((unsigned long)(sz)) >> 18) : \
     126)

#define frontlink_a(P, S, IDX, BK, FD) \
{ \
    if (S < 512) { \
        IDX = (((unsigned long)(S)) >> 3); \
        mark_binblock_a(IDX); \
        BK = bin_at_a(IDX); \
        FD = BK->fd; \
        P->bk = BK; \
        P->fd = FD; \
        FD->bk = BK->fd = P; \
    } else { \
        IDX = bin_index_a(S); \
        BK = bin_at_a(IDX); \
        FD = BK->fd; \
        if (FD == BK) \
            mark_binblock_a(IDX); \
        else { \
            while (FD != BK && S < chunksize_a(FD)) \
                FD = FD->fd; \
            BK = FD->bk; \
        } \
        P->bk = BK; \
        P->fd = FD; \
        FD->bk = BK->fd = P; \
    } \
}

extern mbinptr_a malloc_av[];
extern struct mallinfo_a malloc_current_mallinfo;

/* current_mallinfo; .arena = sbrked_mem */
extern unsigned long malloc_top_pad;

/* top_pad */
extern char *malloc_sbrk_base;

/* sbrk_base */
extern unsigned long malloc_max_sbrked_mem;

/* max_sbrked_mem */
extern unsigned long malloc_max_total_mem;

/* max_total_mem */
extern void *_sbrk_r(struct _reent_a *ptr, int incr);

extern void _free_r(struct _reent_a *ptr, void *mem);
extern void __malloc_lock(struct _reent_a *ptr);
extern void __malloc_unlock(struct _reent_a *ptr);
extern void malloc_extend_top(struct _reent_a *ptr, unsigned int nb);

/*
 * Malloc_extend_top: grows the top chunk (av_ malloc_av) by nb + top_pad (malloc_top_pad) + 16
 * through _sbrk_r, page-rounding and aligning the new space, recording sbrk_base
 * (malloc_sbrk_base) and sbrked_mem (malloc_current_mallinfo), fenceposting and freeing (_free_r
 * _free_r) an old top left behind by a foreign sbrk; updates max_sbrked_mem/max_total_mem
 * (malloc_max_sbrked_mem/malloc_max_total_mem).
 */
/* 0x001ecad0 */
void malloc_extend_top(struct _reent_a *reent_ptr, unsigned int nb)
{
    char *brk;
    unsigned int front_misalign;
    unsigned int correction;
    char *new_brk;
    unsigned int top_size;

    mchunkptr_a old_top = top_a;
    unsigned int old_top_size = chunksize_a(old_top);
    char *old_end = (char *)(chunk_at_offset_a(old_top, old_top_size));

    unsigned int sbrk_size = nb + malloc_top_pad + 16;
    unsigned long pagesz = 4096;

    if (malloc_sbrk_base != (char *)(-1))
        sbrk_size = (sbrk_size + (pagesz - 1)) & ~(pagesz - 1);

    brk = (char *)(_sbrk_r(reent_ptr, sbrk_size));

    if (brk == (char *)(-1) ||
        (brk < old_end && old_top != (mchunkptr_a)(bin_at_a(0))))
        return;

    malloc_current_mallinfo.arena += sbrk_size;

    if (brk == old_end) {
        top_size = sbrk_size + old_top_size;
        top_a->size = top_size | 1;
    } else {
        if (malloc_sbrk_base == (char *)(-1))
            malloc_sbrk_base = brk;
        else
            malloc_current_mallinfo.arena += brk - (char *)old_end;

        front_misalign = (unsigned int)((char *)brk + 8) & 15;
        if (front_misalign > 0) {
            correction = 16 - front_misalign;
            brk += correction;
        } else
            correction = 0;

        correction += pagesz - ((unsigned int)(brk + sbrk_size) & (pagesz - 1));

        new_brk = (char *)(_sbrk_r(reent_ptr, correction));
        if (new_brk == (char *)(-1))
            return;

        malloc_current_mallinfo.arena += correction;

        top_a = (mchunkptr_a)brk;
        top_size = new_brk - brk + correction;
        top_a->size = top_size | 1;

        if (old_top != (mchunkptr_a)(bin_at_a(0))) {
            if (old_top_size < 16) {
                top_a->size = 1;
                return;
            }
            old_top_size = (old_top_size - 3 * 4) & ~15;
            old_top->size = (old_top->size & 1) | old_top_size;
            chunk_at_offset_a(old_top, old_top_size)->size = 4 | 1;
            chunk_at_offset_a(old_top, old_top_size + 4)->size = 4 | 1;
            if (old_top_size >= 16)
                _free_r(reent_ptr, (char *)old_top + 8);
        }
    }

    if ((unsigned long)malloc_current_mallinfo.arena > (unsigned long)malloc_max_sbrked_mem)
        malloc_max_sbrked_mem = malloc_current_mallinfo.arena;
    if ((unsigned long)(0 + malloc_current_mallinfo.arena) > (unsigned long)malloc_max_total_mem)
        malloc_max_total_mem = 0 + malloc_current_mallinfo.arena;
}

/* 0x001ecd28 */
void *_malloc_r(struct _reent_a *reent_ptr, unsigned int bytes)
{
    mchunkptr_a victim;
    unsigned int victim_size;
    int idx;
    mbinptr_a bin;
    mchunkptr_a remainder;
    long remainder_size;
    int remainder_index;
    unsigned long block;
    int startidx;
    mchunkptr_a fwd;
    mchunkptr_a bck;
    mbinptr_a q;

    unsigned int nb = (((long)((bytes) + (4 + 15)) < (long)(16 + 15)) ? 16 : (((bytes) + (4 + 15)) & ~(15)));

    __malloc_lock(reent_ptr);

    if (nb < 512 - 8) {
        idx = (((unsigned long)(nb)) >> 3);
        q = bin_at_a(idx);
        victim = last_a(q);
        if (victim != q) {
            victim_size = chunksize_a(victim);
            unlink_a(victim, bck, fwd);
            set_inuse_bit_at_offset_a(victim, victim_size);
            __malloc_unlock(reent_ptr);
            return (char *)victim + 8;
        }
        idx += 2;
    } else {
        idx = bin_index_a(nb);
        bin = bin_at_a(idx);
        for (victim = last_a(bin); victim != bin; victim = victim->bk) {
            victim_size = chunksize_a(victim);
            remainder_size = long_sub_size_t_a(victim_size, nb);
            if (remainder_size >= (long)16) {
                --idx;
                break;
            } else if (remainder_size >= 0) {
                unlink_a(victim, bck, fwd);
                set_inuse_bit_at_offset_a(victim, victim_size);
                __malloc_unlock(reent_ptr);
                return (char *)victim + 8;
            }
        }
        ++idx;
    }

    if ((victim = last_remainder_a->fd) != last_remainder_a) {
        victim_size = chunksize_a(victim);
        remainder_size = long_sub_size_t_a(victim_size, nb);
        if (remainder_size >= (long)16) {
            remainder = chunk_at_offset_a(victim, nb);
            victim->size = nb | 1;
            link_last_remainder_a(remainder);
            remainder->size = remainder_size | 1;
            chunk_at_offset_a(remainder, remainder_size)->prev_size = remainder_size;
            __malloc_unlock(reent_ptr);
            return (char *)victim + 8;
        }
        clear_last_remainder_a;
        if (remainder_size >= 0) {
            set_inuse_bit_at_offset_a(victim, victim_size);
            __malloc_unlock(reent_ptr);
            return (char *)victim + 8;
        }
        frontlink_a(victim, victim_size, remainder_index, bck, fwd);
    }

    if ((block = idx2binblock_a(idx)) <= binblocks_a) {
        if ((block & binblocks_a) == 0) {
            idx = (idx & ~(4 - 1)) + 4;
            block <<= 1;
            while ((block & binblocks_a) == 0) {
                idx += 4;
                block <<= 1;
            }
        }
        for (;;) {
            startidx = idx;
            q = bin = bin_at_a(idx);
            do {
                for (victim = last_a(bin); victim != bin; victim = victim->bk) {
                    victim_size = chunksize_a(victim);
                    remainder_size = long_sub_size_t_a(victim_size, nb);
                    if (remainder_size >= (long)16) {
                        remainder = chunk_at_offset_a(victim, nb);
                        victim->size = nb | 1;
                        unlink_a(victim, bck, fwd);
                        link_last_remainder_a(remainder);
                        remainder->size = remainder_size | 1;
                        chunk_at_offset_a(remainder, remainder_size)->prev_size = remainder_size;
                        __malloc_unlock(reent_ptr);
                        return (char *)victim + 8;
                    } else if (remainder_size >= 0) {
                        set_inuse_bit_at_offset_a(victim, victim_size);
                        unlink_a(victim, bck, fwd);
                        __malloc_unlock(reent_ptr);
                        return (char *)victim + 8;
                    }
                }
                bin = next_bin_a(bin);
                if (idx < 63) {
                    bin = next_bin_a(bin);
                    ++idx;
                }
            } while ((++idx & (4 - 1)) != 0);

            do {
                if ((startidx & (4 - 1)) == 0) {
                    binblocks_a &= ~block;
                    break;
                }
                --startidx;
                q = prev_bin_a(q);
            } while (first_a(q) == q);

            if ((block <<= 1) <= binblocks_a && (block != 0)) {
                while ((block & binblocks_a) == 0) {
                    idx += 4;
                    block <<= 1;
                }
            } else
                break;
        }
    }

    remainder_size = long_sub_size_t_a(chunksize_a(top_a), nb);
    if (chunksize_a(top_a) < nb || remainder_size < (long)16) {
        malloc_extend_top(reent_ptr, nb);
        remainder_size = long_sub_size_t_a(chunksize_a(top_a), nb);
        if (chunksize_a(top_a) < nb || remainder_size < (long)16) {
            __malloc_unlock(reent_ptr);
            return 0;
        }
    }

    victim = top_a;
    victim->size = nb | 1;
    top_a = chunk_at_offset_a(victim, nb);
    top_a->size = remainder_size | 1;
    __malloc_unlock(reent_ptr);
    return (char *)victim + 8;
}
