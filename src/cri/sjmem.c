/* Memory stream joint: up to 32 fixed blocks, all data at first (side 1); side 0 empty. */

#include "types_cri.h"
#include "sjmem.h"
#include "libc.h"
#include "cri_lock.h"

static void sjmem_reset(SjMem *sj);

/* Hangs. */
/* 0x001d1db8 */
static void sjmem_default_error_cb(void *obj, int err)
{
    for (;;)
        ;
}

/* First call clears the 32 joints; counts calls. */
/* 0x001d1dd8 */
void sjmem_lib_init(void)
{
    if (sjmem_init_count == 0)
        memset(sjmem_obj_tbl, 0, 1152);
    sjmem_init_count++;
}

/* Last user clears the joints. */
/* 0x001d1e18 */
void sjmem_lib_finish(void)
{
    if (--sjmem_init_count == 0)
        memset(sjmem_obj_tbl, 0, 1152);
}

/* All data; 0 when all 32 are taken. */
/* 0x001d1e48 */
static SjMem *sjmem_create(char *buf, int size)
{
    SjMem *sj;
    int i;

    for (i = 0; i < 32; i++) {
        if (sjmem_obj_tbl[i].used == 0)
            break;
    }
    if (i == 32)
        return 0;
    sj = &sjmem_obj_tbl[i];
    sj->used = 1;
    sj->vtbl = &sjmem_vtbl;
    sj->buf = buf;
    sj->size = size;
    sj->uuid = sjmem_uuid;
    sj->errfn = sjmem_default_error_cb;
    sj->errobj = sj;
    sjmem_reset(sj);
    return sj;
}

/* 0x001d1f08 */
static void sjmem_destroy(SjMem *sj)
{
    if (sj != 0) {
        memset(sj, 0, 36);
        sj->used = 0;
    }
}

/* 0x001d1f40 */
static void *sjmem_get_uuid(SjMem *sj)
{
    return sj->uuid;
}

/* 0x001d1f48 */
static void sjmem_set_error_cb(SjMem *sj, SjErrFn fn, void *obj)
{
    sj->errfn = fn;
    sj->errobj = obj;
}

/* All data again, read from the start. */
/* 0x001d1f58 */
static void sjmem_reset(SjMem *sj)
{
    sj->pos = 0;
    sj->len = sj->size;
}

/* Side 1 data, side 0 none; other sides -3. */
/* 0x001d1f68 */
static int sjmem_get_num_data(SjMem *sj, int id)
{
    if (id == 1)
        return sj->len;
    if (id != 0 && sj->errfn != 0)
        sj->errfn(sj->errobj, -3);
    return 0;
}

/* Up to nbyte from side 1 at the read position; side 0 gives an empty chunk. */
/* 0x001d1fb0 */
static void sjmem_get_chunk(SjMem *sj, int id, int nbyte, SjChunk *ck)
{
    sj_lock();
    if (id == 0) {
        ck->len = 0;
        ck->data = 0;
    } else if (id == 1) {
        ck->len = (sj->len < nbyte) ? sj->len : nbyte;
        ck->data = sj->buf + sj->pos;
        sj->pos += ck->len;
        sj->len -= ck->len;
    } else {
        ck->len = 0;
        ck->data = 0;
        if (sj->errfn != 0)
            sj->errfn(sj->errobj, -3);
    }
    sj_unlock();
}

/* Ignored for either side; other sides -3. */
/* 0x001d2068 */
static void sjmem_put_chunk(SjMem *sj, int id, SjChunk *ck)
{
    if (ck->len > 0 && ck->data != 0) {
        sj_lock();
        if (id == 0) {
        } else if (id == 1) {
        } else {
            ck->len = 0;
            ck->data = 0;
            if (sj->errfn != 0)
                sj->errfn(sj->errobj, -3);
        }
        sj_unlock();
    }
}

/* Back to side 1, rewinding the read position; -3 if not the last chunk taken or another side. */
/* 0x001d2100 */
static void sjmem_unget_chunk(SjMem *sj, int id, SjChunk *ck)
{
    int pos;
    int len;

    if (ck->len > 0 && ck->data != 0) {
        sj_lock();
        if (id == 0) {
            if (sj->errfn != 0)
                sj->errfn(sj->errobj, -3);
        } else if (id == 1) {
            pos = sj->pos - ck->len;
            if (pos < 0)
                pos = 0;
            sj->pos = pos;
            len = sj->len + ck->len;
            if (len > sj->size)
                len = sj->size;
            sj->len = len;
            if (pos != ck->data - sj->buf) {
                if (sj->errfn != 0)
                    sj->errfn(sj->errobj, -3);
            }
        } else {
            ck->len = 0;
            ck->data = 0;
            if (sj->errfn != 0)
                sj->errfn(sj->errobj, -3);
        }
        sj_unlock();
    }
}

/* *rbyte = how much of nbyte a get would give; 1 if all of it. */
/* 0x001d2200 */
static int sjmem_is_get_chunk(SjMem *sj, int id, int nbyte, int *rbyte)
{
    int n;

    sj_lock();
    if (id == 0) {
        n = 0;
    } else if (id == 1) {
        n = (sj->len < nbyte) ? sj->len : nbyte;
    } else {
        n = 0;
        if (sj->errfn != 0)
            sj->errfn(sj->errobj, -3);
    }
    *rbyte = n;
    sj_unlock();
    return n == nbyte;
}

/* 0x001d2298 */
static char *sjmem_get_buf(SjMem *sj)
{
    return sj->buf;
}

/* 0x001d22a0 */
static int sjmem_get_size(SjMem *sj)
{
    return sj->size;
}
