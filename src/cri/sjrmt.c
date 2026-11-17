/* Remote stream joint: EE calls for an IOP-side joint, each a DTX call. */

#include "types_cri.h"
#include "sjrmt.h"
#include "dtx.h"

/* 0x001d31b8 */
static int sjrmt_iop_call32(int word0, int word1, int word2)
{
    sjrmt_rpc_sbuf[0] = word0;
    sjrmt_rpc_sbuf[1] = word1;
    sjrmt_rpc_sbuf[2] = word2;
    dtx_call_iop(32, sjrmt_rpc_sbuf, 3, sjrmt_rpc_rbuf, 1);
    return sjrmt_rpc_rbuf[0];
}

/* 0x001d3208 */
static int sjrmt_iop_call33(int word0, int word1)
{
    sjrmt_rpc_sbuf[0] = word0;
    sjrmt_rpc_sbuf[1] = word1;
    dtx_call_iop(33, sjrmt_rpc_sbuf, 2, sjrmt_rpc_rbuf, 1);
    return sjrmt_rpc_rbuf[0];
}

/* Over size bytes of IOP memory at buf; returns the IOP handle. */
/* 0x001d3258 */
void *sjrmt_create(int mode, void *buf, int size)
{
    sjrmt_rpc_sbuf[0] = mode;
    sjrmt_rpc_sbuf[1] = (int)buf;
    sjrmt_rpc_sbuf[2] = size;
    dtx_call_iop(34, sjrmt_rpc_sbuf, 3, sjrmt_rpc_rbuf, 1);
    return (void *)sjrmt_rpc_rbuf[0];
}

/* 0x001d32a8 */
void sjrmt_destroy(void *sj)
{
    sjrmt_rpc_sbuf[0] = (int)sj;
    dtx_call_iop(35, sjrmt_rpc_sbuf, 1, 0, 0);
}

/* Kept in sjrmt_uuid_buf until the next call. */
/* 0x001d32c8 */
static int *sjrmt_get_uuid(void *sj)
{
    sjrmt_rpc_sbuf[0] = (int)sj;
    dtx_call_iop(36, sjrmt_rpc_sbuf, 1, sjrmt_rpc_rbuf, 4);
    sjrmt_uuid_buf[0] = sjrmt_rpc_rbuf[0];
    sjrmt_uuid_buf[1] = sjrmt_rpc_rbuf[1];
    sjrmt_uuid_buf[2] = sjrmt_rpc_rbuf[2];
    sjrmt_uuid_buf[3] = sjrmt_rpc_rbuf[3];
    return sjrmt_uuid_buf;
}

/* 0x001d3340 */
void sjrmt_reset(void *sj)
{
    sjrmt_rpc_sbuf[0] = (int)sj;
    dtx_call_iop(37, sjrmt_rpc_sbuf, 1, 0, 0);
}

/* 0x001d3360 */
static void sjrmt_get_chunk(void *sj, int id, int nbyte, SjChunk *ck)
{
    sjrmt_rpc_sbuf[0] = (int)sj;
    sjrmt_rpc_sbuf[1] = id;
    sjrmt_rpc_sbuf[2] = nbyte;
    dtx_call_iop(38, sjrmt_rpc_sbuf, 3, sjrmt_rpc_rbuf, 2);
    ck->data = (char *)sjrmt_rpc_rbuf[0];
    ck->len = sjrmt_rpc_rbuf[1];
}

/* 0x001d33d8 */
static void sjrmt_put_chunk(void *sj, int id, SjChunk *ck)
{
    sjrmt_rpc_sbuf[0] = (int)sj;
    sjrmt_rpc_sbuf[1] = id;
    sjrmt_rpc_sbuf[2] = (int)ck->data;
    sjrmt_rpc_sbuf[3] = ck->len;
    dtx_call_iop(39, sjrmt_rpc_sbuf, 4, 0, 0);
}

/* 0x001d3418 */
static void sjrmt_unget_chunk(void *sj, int id, SjChunk *ck)
{
    sjrmt_rpc_sbuf[0] = (int)sj;
    sjrmt_rpc_sbuf[1] = id;
    sjrmt_rpc_sbuf[2] = (int)ck->data;
    sjrmt_rpc_sbuf[3] = ck->len;
    dtx_call_iop(40, sjrmt_rpc_sbuf, 4, 0, 0);
}

/* 0x001d3458 */
int sjrmt_get_num_data(void *sj, int id)
{
    sjrmt_rpc_sbuf[0] = (int)sj;
    sjrmt_rpc_sbuf[1] = id;
    dtx_call_iop(41, sjrmt_rpc_sbuf, 2, sjrmt_rpc_rbuf, 1);
    return sjrmt_rpc_rbuf[0];
}

/* Bug: one result word is received, but *rbyte is read from the second. */
/* 0x001d34a8 */
static int sjrmt_is_get_chunk(void *sj, int id, int nbyte, int *rbyte)
{
    sjrmt_rpc_sbuf[0] = (int)sj;
    sjrmt_rpc_sbuf[1] = id;
    sjrmt_rpc_sbuf[2] = nbyte;
    dtx_call_iop(42, sjrmt_rpc_sbuf, 3, sjrmt_rpc_rbuf, 1);
    *rbyte = sjrmt_rpc_rbuf[1];
    return sjrmt_rpc_rbuf[0];
}

/* First call sets up DTX; counts calls. */
/* 0x001d3518 */
void sjrmt_lib_init(void)
{
    sjrmt_ver_str[0];
    if (sjrmt_init_count[0] == 0)
        DTX_Init();
    sjrmt_init_count[0]++;
}

/* Last user shuts DTX down. */
/* 0x001d3560 */
void sjrmt_lib_finish(void)
{
    if (--sjrmt_init_count[0] == 0)
        DTX_Finish();
}
