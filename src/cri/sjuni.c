/* Chunk list stream joint: 64, four lists of 16-byte cells in a caller work area; merge mode. */

#include "types_cri.h"
#include "sjuni.h"
#include "libc.h"
#include "cri_lock.h"

static void sjuni_reset(SjUni *sj);

/* Hangs. */
/* 0x001d29a0 */
static void sjuni_default_error_cb(void *obj, int err)
{
    for (;;)
        ;
}

/* First call clears the 64 joints; counts calls. */
/* 0x001d29c0 */
void sjuni_lib_init(void)
{
    if (sjuni_init_count == 0)
        memset(sjuni_obj_tbl, 0, 3072);
    sjuni_init_count++;
}

/* Last user clears the joints. */
/* 0x001d2a00 */
void sjuni_lib_finish(void)
{
    if (--sjuni_init_count == 0)
        memset(sjuni_obj_tbl, 0, 3072);
}

/* mode 1 merges chunks; 0 when all 64 are taken. */
/* 0x001d2a30 */
void *sjuni_create(int mode, void *work, int size)
{
    SjUni *sj;
    int i;

    for (i = 0; i < 64; i++) {
        if (sjuni_obj_tbl[i].used == 0)
            break;
    }
    if (i == 64)
        return 0;
    sj = &sjuni_obj_tbl[i];
    sj->used = 1;
    sj->vtbl = &sjuni_vtbl;
    sj->mode = mode;
    sj->uuid = sjuni_uuid;
    sj->cells = (SjCell *)work;
    sj->ncell = size / 16;
    sj->errfn = sjuni_default_error_cb;
    sj->errobj = sj;
    sjuni_reset(sj);
    return sj;
}

/* 0x001d2b08 */
static void sjuni_destroy(SjUni *sj)
{
    if (sj != 0) {
        memset(sj, 0, 48);
        sj->used = 0;
    }
}

/* 0x001d2b40 */
static void *sjuni_get_uuid(SjUni *sj)
{
    return sj->uuid;
}

/* 0x001d2b48 */
static void sjuni_set_error_cb(SjUni *sj, SjErrFn fn, void *obj)
{
    sj->errfn = fn;
    sj->errobj = obj;
}

/* Every cell back to the free list. */
/* 0x001d2b58 */
static void sjuni_reset(SjUni *sj)
{
    SjCell *cell;
    int i;

    cell = sj->cells;
    sj->free = cell;
    for (i = 0; i < sj->ncell - 1; i++) {
        cell[i].next = &cell[i + 1];
        cell[i].ck.len = 0;
        cell[i].ck.data = 0;
    }
    cell[i].next = 0;
    cell[i].ck.data = 0;
    cell[i].ck.len = 0;
    for (i = 3; i >= 0; i--)
        sj->list[i] = 0;
}

/* Lists 0..3; others -3. */
/* 0x001d2bf8 */
static int sjuni_get_num_data(SjUni *sj, unsigned int id)
{
    SjCell *cell;
    int n;

    if (id >= 4) {
        if (sj->errfn != 0)
            sj->errfn(sj->errobj, -3);
        return 0;
    }
    n = 0;
    for (cell = sj->list[id]; cell != 0; cell = cell->next)
        n += cell->ck.len;
    return n;
}

/* First chunk; one larger than nbyte is split in merge mode, else not given. */
/* 0x001d2c70 */
static void sjuni_get_chunk(SjUni *sj, unsigned int id, int nbyte, SjChunk *ck)
{
    SjCell *cell;
    SjChunk tmpck;
    SjChunk rest;

    if (id >= 4) {
        if (sj->errfn != 0)
            sj->errfn(sj->errobj, -3);
        ck->data = 0;
        ck->len = 0;
        return;
    }
    sj_lock();
    cell = sj->list[id];
    if (cell == 0) {
        ck->data = 0;
        ck->len = 0;
    } else {
        tmpck = cell->ck;
        if (nbyte >= tmpck.len) {
            *ck = tmpck;
            sj->list[id] = cell->next;
            cell->next = sj->free;
            sj->free = cell;
        } else if (sj->mode == 1) {
            sj_split_chunk(&tmpck, nbyte, &tmpck, &rest);
            *ck = tmpck;
            cell->ck = rest;
        } else {
            ck->data = 0;
            ck->len = 0;
        }
    }
    sj_unlock();
}

/* At the end (merge mode joins touching chunks); -3 when no cell is free. */
/* 0x001d2da0 */
static void sjuni_put_chunk(SjUni *sj, unsigned int id, SjChunk *ck)
{
    SjCell *cell;
    SjCell *last;
    SjCell **pp;

    if (id >= 4) {
        if (sj->errfn != 0)
            sj->errfn(sj->errobj, -3);
    } else if (ck->len > 0 && ck->data != 0) {
        sj_lock();
        last = 0;
        pp = &sj->list[id];
        for (cell = *pp; cell != 0; cell = last->next) {
            last = cell;
            pp = &last->next;
        }
        if (sj->mode == 1 && last != 0 && last->ck.data + last->ck.len == ck->data) {
            last->ck.len += ck->len;
        } else {
            last = sj->free;
            if (last == 0) {
                if (sj->errfn != 0)
                    sj->errfn(sj->errobj, -3);
            } else {
                sj->free = last->next;
                last->next = 0;
                last->ck = *ck;
                *pp = last;
            }
        }
        sj_unlock();
    }
}

/* At the front (merge mode joins touching chunks); -3 when no cell is free. */
/* 0x001d2f08 */
static void sjuni_unget_chunk(SjUni *sj, unsigned int id, SjChunk *ck)
{
    SjCell *cell;

    if (id >= 4) {
        if (sj->errfn != 0)
            sj->errfn(sj->errobj, -3);
    } else if (ck->len > 0 && ck->data != 0) {
        sj_lock();
        cell = sj->list[id];
        if (sj->mode == 1 && cell != 0 && ck->data + ck->len == cell->ck.data) {
            cell->ck.data = ck->data;
            cell->ck.len += ck->len;
        } else {
            cell = sj->free;
            if (cell == 0) {
                if (sj->errfn != 0)
                    sj->errfn(sj->errobj, -3);
            } else {
                sj->free = cell->next;
                cell->next = 0;
                cell->ck = *ck;
                cell->next = sj->list[id];
                sj->list[id] = cell;
            }
        }
        sj_unlock();
    }
}

/* *rbyte = first chunk size; 1 if it covers nbyte (merge) or equals it. */
/* 0x001d3040 */
static int sjuni_is_get_chunk(SjUni *sj, unsigned int id, int nbyte, int *rbyte)
{
    SjCell *cell;
    SjChunk tmpck;

    *rbyte = 0;
    if (id >= 4) {
        if (sj->errfn != 0)
            sj->errfn(sj->errobj, -3);
        return 0;
    }
    cell = sj->list[id];
    if (cell == 0)
        return 0;
    tmpck = cell->ck;
    *rbyte = tmpck.len;
    if (sj->mode == 1) {
        if (tmpck.len >= nbyte)
            return 1;
        return 0;
    }
    if (tmpck.len == nbyte)
        return 1;
    return 0;
}

/* 0x001d30e0 */
static int sjuni_count_chunks(SjUni *sj, int id)
{
    SjCell *cell;
    int n;

    n = 0;
    for (cell = sj->list[id]; cell != 0; cell = cell->next)
        n++;
    return n;
}

/* 0x001d3120 */
static int sjuni_count_free_cells(SjUni *sj)
{
    SjCell *cell;
    int n;

    n = 0;
    for (cell = sj->free; cell != 0; cell = cell->next)
        n++;
    return n;
}

/* head: first n bytes at most; tail: the rest (null data if none). */
/* 0x001d3158 */
void sj_split_chunk(SjChunk *ck, int n, SjChunk *head, SjChunk *tail)
{
    *head = *ck;
    tail->len = head->len;
    if (n < head->len)
        head->len = n;
    tail->len -= head->len;
    if (tail->len == 0)
        tail->data = 0;
    else
        tail->data = head->data + head->len;
}
