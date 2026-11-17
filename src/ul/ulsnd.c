/*
 * Sound driver, EE side: ring of command buffers for the IOP driver, sequence handles, volumes and
 * effects, DMA download of sound data (multi-file or single banks) and the channel map.
 */

#include "types_gw.h"
#include "ulsnd.h"
#include <stdarg.h>
#include "types_ul.h"
#include "libc.h"
#include "kernel_syscalls.h"
#include "libkernl_cache.h"
#include "libkernl_sifrpc.h"
#include "ps_heap.h"

static void ulsnd_reset_ring(void);
static void ulsnd_error_callback(int err);
static void ulsnd_printf(char *fmt, ...);
static int ulsnd_push_command(unsigned char *cmd);
static void ulsnd_copy_bytes(unsigned char *dst, unsigned char *src, unsigned int n);
static int ulsnd_handle_alloc(UlHandle **out);
static int ulsnd_handle_free(UlHandle *hn);
static int ulsnd_set_error_callback(void (*cb)());
static void ulsnd_send_iop_waves(void);
static void ulsnd_send_spu_waves(void);
static int ulsnd_dma_add(int src, int dst, int size, int *flag);
static int ulsnd_dma_free_entries(void);
static int ulsnd_dma_kick(void);
static void *ulsnd_iop_alloc(int size);
static void ulsnd_rec_step(void);
static int ulsnd_rec_alloc(UlRec **out);
static int ulsnd_rec_free(UlRec *rec);
static int ulsnd_rec_setup(UlRec *rec, int data, int size, int sync);
static int ulsnd_rec_get_state(UlRec *rec, int *out);
static int ulsnd_rec_start_mlt(UlRec *rec);
static int ulsnd_rec_start_bank(UlRec *rec, int kind, int idx);
static void ulsnd_set_channel_on(int on, int kind, int ch);

typedef struct { int count; int count2; char pad_08[52]; int flag; } UlFlushRec;

/* Always zero (never-taken tests and dead stores below). */
#define AZ(p) ((((int)(p)) & 1) >> 1)

typedef struct {
    int w[4];
    int *chunks;
} UlBank;

/* Channel map column word: base bytes in, entries stride apart. */
#define TW(base, stride, idx) (col = (char *)ulsnd_work.shared + (base), *(int *)(col + (idx) * (stride)))

extern UlShared *ulsnd_shared;
extern UlFlushRec ulsnd_flush_rec;
extern UlHandle ulsnd_handles[16];
extern int *ulsnd_spu_status[];

extern int ulsnd_spu_wave_index;
extern int ulsnd_spu_wave_list[];
extern int ulsnd_spu_wave_dst[];
extern int ulsnd_spu_wave_size[];
extern IopBlk ulsnd_spu_blk;
extern IopBlk ulsnd_mlt_blk;
extern void ulsnd_bank_wave_sizes(UlBank *bank, int *iop_size, int *spu_size);

/* 0 or 1, see ulsnd_set_stereo. */
/* 0x00187360 */
static int ulsnd_set_output_mode(unsigned int mode)
{
    unsigned char cmd[144];
    int err;

    if (mode >= 2) {
        err = 0x80300000;
    } else {
        cmd[0] = 5;
        cmd[1] = 2;
        cmd[4] = mode;
        err = ulsnd_push_command(cmd);
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* Clamped to 0..1. */
/* 0x001873c8 */
static int ulsnd_set_master_volume(float vol)
{
    unsigned char cmd[144];
    int err;
    unsigned short scaled;

    if (1.0f < vol)
        vol = 1.0f;
    if (vol < 0.0f)
        vol = 0.0f;
    cmd[0] = 6;
    cmd[1] = 1;
    scaled = (int)(vol * 16383.0f);
    *(unsigned short *)(cmd + 4) = scaled;
    err = ulsnd_push_command(cmd);
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* Channel ch must have its effect bank loaded. */
/* 0x00187468 */
int ulsnd_set_fx_prg(unsigned int ch, int val)
{
    unsigned char cmd[144];
    int err;

    if (ch >= 16 || val < 0) {
        err = 0x80300000;
    } else if (ulsnd_shared->kind3[ch].on == 0) {
        err = 0x80b00000;
    } else {
        cmd[0] = 6;
        cmd[1] = 64;
        cmd[4] = ch;
        cmd[5] = val;
        err = ulsnd_push_command(cmd);
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* Each clamped to 0..1. */
/* 0x001874f8 */
static int ulsnd_set_fx_volume(int ch, float left, float right)
{
    unsigned char cmd[144];
    int err;

    if (1.0f < left)
        left = 1.0f;
    if (left < 0.0f)
        left = 0.0f;
    if (1.0f < right)
        right = 1.0f;
    if (right < 0.0f)
        right = 0.0f;
    cmd[0] = 10;
    cmd[1] = 65;
    cmd[4] = ch;
    *(unsigned short *)(cmd + 6) = (int)(left * 32767.0f);
    *(unsigned short *)(cmd + 8) = (int)(right * 32767.0f);
    err = ulsnd_push_command(cmd);
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* 0x001875c8 */
static int ulsnd_clear_fx_prg(int ch)
{
    unsigned char cmd[144];
    int err;

    cmd[0] = 5;
    cmd[1] = 66;
    cmd[4] = ch;
    err = ulsnd_push_command(cmd);
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* Empties the ring; count = filled buffers that may wait before sending. */
/* 0x00187620 */
static void ulsnd_set_buffer_count(int count)
{
    ulsnd_reset_ring();
    ulsnd_work.nbuf = count;
}

/* All six; buffer 0 current. */
/* 0x00187650 */
static void ulsnd_reset_ring(void)
{
    int i;

    ulsnd_work.cur = 0;
    ulsnd_work.off = 0;
    ulsnd_work.ncmd = 0;
    for (i = 0; i < 6; i++) {
        ulsnd_work.bufs[i]->size = 16;
        ulsnd_work.bufs[i]->count = 0;
    }
}

/* Records the write position. */
/* 0x00187698 */
static int ulsnd_ring_free_bytes(void)
{
    ulsnd_work.bufs[ulsnd_work.cur]->size = ulsnd_work.off;
    return ulsnd_work.bufsize - ulsnd_work.bufs[ulsnd_work.cur]->size;
}

/* Records it too. */
/* 0x001876e0 */
int ulsnd_buffer_commands(void)
{
    ulsnd_work.bufs[ulsnd_work.cur]->count = ulsnd_work.ncmd;
    return ulsnd_work.bufs[ulsnd_work.cur]->count;
}

/* All ring buffers. */
/* 0x00187720 */
static int ulsnd_pending_commands(void)
{
    int i;
    int sum;

    ulsnd_work.bufs[ulsnd_work.cur]->count = ulsnd_work.ncmd;
    sum = 0;
    for (i = 0; i < ulsnd_work.nbuf + 2; i++)
        sum += ulsnd_work.bufs[i]->count;
    return sum;
}

/* Still on the last buffer sent. */
/* 0x00187788 */
int ulsnd_is_busy(void)
{
    return *(int *)((char *)ulsnd_work.iop_blk + 0x20000000 + 60) != 0;
}

/* Negative codes print as errors, positive as warnings. */
/* 0x001877a8 */
static void ulsnd_report_error(int err)
{
    if (err < 0)
        ulsnd_printf("Error (%08x)\n", err);
    else if (err > 0)
        ulsnd_printf("Warning (%08x)\n", err);
}

/* 0x001877d8 */
static void ulsnd_error_callback(int err)
{
    if (ulsnd_work.err_cb)
        ulsnd_work.err_cb(err);
}

/* 0x00187800 */
static void ulsnd_nop(void)
{
}

/* 0x00187808 */
static void ulsnd_nop_2(void)
{
}

/* "ulsnd : " prefix; only while debug output is on. */
/* 0x00187810 */
static void ulsnd_dprintf(char *fmt, ...)
{
    char buf[256];
    va_list ap;

    if (ulsnd_work.dbg) {
        va_start(ap, fmt);
        vsprintf(buf, fmt, ap);
        printf("ulsnd : ");
        printf(buf);
    }
}

/* "ulsnd : " prefix. */
/* 0x00187870 */
static void ulsnd_printf(char *fmt, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    printf("ulsnd : ");
    printf(buf);
}

/* 0x001878c8 */
static void ulsnd_nop_3(void)
{
}

/* 0x001878d0 */
static void ulsnd_nop_4(void)
{
}

/* Output removed: formats and drops. */
/* 0x001878d8 */
static void ulsnd_dprintf_nop(char *fmt, ...)
{
    char buf[256];
    va_list ap;

    if (ulsnd_work.dbg) {
        va_start(ap, fmt);
        vsprintf(buf, fmt, ap);
    }
}

/* Output removed: formats and drops. */
/* 0x00187928 */
static void ulsnd_printf_nop(char *fmt, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
}

/* cmd[0] bytes, word-padded. A full buffer is an error (TRAP loop with debug on). */
/* 0x00187968 */
static int ulsnd_push_command(unsigned char *cmd)
{
    UlSnd *snd = &ulsnd_work;
    int err = 0;
    char *buf = (char *)snd->bufs[snd->cur];
    int len = cmd[0];
    char *dst = buf + snd->off;

    if ((unsigned int)snd->bufsize < snd->off + ((len + 3) & 0x1fc)) {
        ulsnd_printf("HCMD : buffer over\n");
        ulsnd_printf("TRAP\n");
        while (snd->dbg)
            ;
        err = 0x80400000;
    } else {
        ulsnd_copy_bytes(dst, cmd, len);
        snd->off = snd->off + ((cmd[0] + 3) & 0x1fc);
        snd->ncmd = snd->ncmd + 1;
        *(int *)(buf + snd->off) = 0x444e45;
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/*
 * Oldest buffer; once the driver has room, queues it with its command count for DMA, marks the
 * command block busy, advances the ring. Copies four driver status words to the SPU status block.
 */
/* 0x00187a70 */
static int ulsnd_flush_commands(void)
{
    int err;
    int idx;
    UlBuf *buf;
    int *src = ulsnd_work.iop_blk->w;
    int *dst = ulsnd_work.spu_status;
    void *rec;
    int addr;

    dst[171] = src[3];
    dst[172] = src[4];
    dst[173] = src[5];
    dst[174] = src[6];
    buf = ulsnd_work.bufs[ulsnd_work.cur];
    buf->size = ulsnd_work.off;
    buf->count = ulsnd_work.ncmd;
    err = 0;
    idx = ulsnd_work.cur - ulsnd_work.nbuf;
    if (idx < 0) {
        idx += 2;
        idx += ulsnd_work.nbuf;
    }
    buf = ulsnd_work.bufs[idx];
    if (buf->count != 0 && ulsnd_dma_free_entries() >= 2) {
        if (ulsnd_dma_add(buf, ulsnd_work.iop_info->w[2], (((unsigned int)buf->size + 19) >> 4) << 4, 0) < 0) {
            ulsnd_printf("TRAP\n");
            while (ulsnd_work.dbg)
                ;
        }
        rec = &ulsnd_flush_rec;
        ((UlFlushRec *)rec)->flag = 1;
        addr = ulsnd_work.iop_info->w[0] + 128;
        ulsnd_flush_rec.count = buf->count;
        ((UlFlushRec *)rec)->count2 = buf->count;
        err = ulsnd_dma_add(rec, addr, 64, 0);
        if (err < 0) {
            ulsnd_printf("TRAP\n");
            while (ulsnd_work.dbg)
                ;
        }
        ulsnd_work.iop_blk->w[15] = 2;
        ulsnd_work.off = 16;
        ulsnd_work.cur++;
        ulsnd_work.ncmd = 0;
        if (!(ulsnd_work.cur < ulsnd_work.nbuf + 2))
            ulsnd_work.cur = 0;
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* 0x00187c70 */
static void ulsnd_sync_dcache(char *addr, int n)
{
    SyncDCache(addr, addr + n);
}

/* 0x00187c78 */
static void ulsnd_invalid_dcache(char *addr, int n)
{
    InvalidDCache(addr, addr + n);
}

/* 0x00187c80 */
static void ulsnd_copy_bytes(unsigned char *dst, unsigned char *src, unsigned int n)
{
    unsigned int i;

    for (i = 0; i < n; i++)
        *dst++ = *src++;
}

/* Rounded up to whole words. */
/* 0x00187cb0 */
static void ulsnd_copy_words(int *dst, int *src, unsigned int n)
{
    unsigned int i;

    n = (n + 3) >> 2;
    for (i = 0; i < n; i++)
        *dst++ = *src++;
}

/* 0x00187ce8 */
static int ulsnd_handle_alloc_thunk(void *out)
{
    return ulsnd_handle_alloc(out);
}

/* 0x00187d08 */
static int ulsnd_handle_free_thunk(void *hn)
{
    return ulsnd_handle_free(hn);
}

/* Up to three bytes. */
/* 0x00187d28 */
int ulsnd_handle_send_midi(UlHandle *hn, int n, unsigned char *data)
{
    unsigned char cmd[144];
    unsigned char *dst;
    int err;
    int i;

    if (n >= 4) {
        err = 0x80300000;
    } else {
        cmd[0] = n + 5;
        cmd[1] = 130;
        cmd[4] = hn->id;
        for (i = 0; i < n;) {
            int j = i + 1;
            dst = cmd + 4;
            dst[j] = data[i];
            i = j;
        }
        err = ulsnd_push_command(cmd);
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* idx -1: the whole multi-file. */
/* 0x00187db8 */
static int ulsnd_bank_ready(int idx)
{
    unsigned char cmd[144];
    int err;

    cmd[0] = 5;
    cmd[1] = 132;
    cmd[4] = idx;
    err = ulsnd_push_command(cmd);
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* 16 handles; *out = 0 and fails when all are in use. */
/* 0x00187e10 */
static int ulsnd_handle_alloc(UlHandle **out)
{
    unsigned char cmd[144];
    int err;
    int i;
    UlHandle *hn;

    for (i = 0; i < 16; i++) {
        hn = &ulsnd_handles[i];
        if (hn->used == 0) {
            hn->used = 1;
            break;
        }
    }
    if (i == 16) {
        *out = 0;
        err = 0x80c00000;
    } else {
        *out = hn;
        hn->id = i;
        cmd[0] = 5;
        cmd[1] = 96;
        cmd[4] = i;
        err = ulsnd_push_command(cmd);
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* 0x00187ed0 */
static int ulsnd_handle_free(UlHandle *hn)
{
    unsigned char cmd[144];
    int err;

    if (hn == 0 || hn->used == 0) {
        err = 0x80200000;
    } else {
        cmd[0] = 5;
        cmd[1] = 97;
        cmd[4] = hn->id;
        err = ulsnd_push_command(cmd);
        if (err >= 0) {
            hn->used = 0;
            hn->id = 255;
        }
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* Fails unless sequence ch and at least one wave bank are loaded. */
/* 0x00187f58 */
static int ulsnd_handle_play(UlHandle *hn, unsigned int ch, int val)
{
    unsigned char cmd[144];
    int err;
    int i;

    if (hn == 0 || hn->used == 0) {
        err = 0x80200000;
    } else if (ch >= 16) {
        err = 0x80300000;
    } else if (val < 0) {
        err = 0x80300000;
    } else if (ulsnd_work.shared->kind0[ch].on == 0) {
        err = 0x80b00000;
    } else {
        for (i = 0; i < 16; i++) {
            if (ulsnd_work.shared->kind1[i].on != 0)
                break;
        }
        if (i == 16) {
            err = 0x80b00000;
        } else {
            cmd[0] = 7;
            cmd[1] = 98;
            cmd[4] = hn->id;
            cmd[5] = ch;
            cmd[6] = val;
            err = ulsnd_push_command(cmd);
        }
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* 0x00188048 */
static int ulsnd_handle_stop(UlHandle *hn)
{
    unsigned char cmd[144];
    int err;

    if (hn == 0 || hn->used == 0) {
        err = 0x80200000;
    } else {
        cmd[0] = 5;
        cmd[1] = 99;
        cmd[4] = hn->id;
        err = ulsnd_push_command(cmd);
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* 0x001880b0 */
static int ulsnd_handle_cmd100(UlHandle *hn)
{
    unsigned char cmd[144];
    int err;

    if (hn == 0 || hn->used == 0) {
        err = 0x80200000;
    } else {
        cmd[0] = 5;
        cmd[1] = 100;
        cmd[4] = hn->id;
        err = ulsnd_push_command(cmd);
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* 0x00188118 */
static int ulsnd_handle_cmd101(UlHandle *hn)
{
    unsigned char cmd[144];
    int err;

    if (hn == 0 || hn->used == 0) {
        err = 0x80200000;
    } else {
        cmd[0] = 5;
        cmd[1] = 101;
        cmd[4] = hn->id;
        err = ulsnd_push_command(cmd);
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* 0x00188180 */
static int ulsnd_handle_cmd102(UlHandle *hn)
{
    unsigned char cmd[144];
    int err;

    if (hn == 0 || hn->used == 0) {
        err = 0x80200000;
    } else {
        cmd[0] = 5;
        cmd[1] = 102;
        cmd[4] = hn->id;
        err = ulsnd_push_command(cmd);
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* Command 103. */
/* 0x001881e8 */
int ulsnd_handle_all_off(UlHandle *hn)
{
    unsigned char cmd[144];
    int err;

    if (hn == 0 || hn->used == 0) {
        err = 0x80200000;
    } else {
        cmd[0] = 5;
        cmd[1] = 103;
        cmd[4] = hn->id;
        err = ulsnd_push_command(cmd);
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* Clamped to 0..2, sent in 1/128 steps. */
/* 0x00188250 */
static int ulsnd_handle_set_volume(UlHandle *hn, float vol)
{
    unsigned char cmd[144];
    int err;
    unsigned char scaled;

    if (hn == 0 || hn->used == 0) {
        err = 0x80200000;
    } else {
        if (vol < 0.0f)
            vol = 0.0f;
        if (2.0f < vol)
            vol = 1.9999f;
        cmd[0] = 6;
        cmd[1] = 104;
        cmd[4] = hn->id;
        scaled = (int)(vol * 128.0f);
        cmd[5] = scaled;
        err = ulsnd_push_command(cmd);
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* val clamped to 0..1, sent in 1/255 steps. */
/* 0x00188320 */
static int ulsnd_handle_cmd107(UlHandle *hn, float val)
{
    unsigned char cmd[144];
    int err;
    unsigned char scaled;

    if (hn == 0 || hn->used == 0) {
        err = 0x80200000;
    } else {
        if (val < 0.0f)
            val = 0.0f;
        if (1.0f < val)
            val = 1.0f;
        cmd[0] = 6;
        cmd[1] = 107;
        cmd[4] = hn->id;
        scaled = (int)(val * 255.0f);
        cmd[5] = scaled;
        err = ulsnd_push_command(cmd);
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* val clamped to 0..1, sent in 1/127 steps. */
/* 0x001883e0 */
static int ulsnd_handle_cmd105(UlHandle *hn, float val)
{
    unsigned char cmd[144];
    int err;
    unsigned char scaled;

    if (hn == 0 || hn->used == 0) {
        err = 0x80200000;
    } else {
        if (val < 0.0f)
            val = 0.0f;
        if (1.0f < val)
            val = 1.0f;
        cmd[0] = 6;
        cmd[1] = 105;
        cmd[4] = hn->id;
        scaled = (int)(val * 127.0f);
        cmd[5] = scaled;
        err = ulsnd_push_command(cmd);
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* Ratio clamped to 0.01..100, sent in 1/256 steps. */
/* 0x001884a0 */
static int ulsnd_handle_set_pitch(UlHandle *hn, float pitch)
{
    unsigned char cmd[144];
    int err;
    unsigned short scaled;

    if (hn == 0 || hn->used == 0) {
        err = 0x80200000;
    } else {
        if (pitch < 0.01f)
            pitch = 0.01f;
        if (100.0f < pitch)
            pitch = 100.0f;
        cmd[0] = 8;
        cmd[1] = 106;
        cmd[4] = hn->id;
        scaled = (int)(pitch * 256.0f);
        *(unsigned short *)(cmd + 6) = scaled;
        err = ulsnd_push_command(cmd);
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/*
 * Defaults: buffer 1024 bytes, SPU and aux areas 256 KB each; all rounded to 64. Returns the heap
 * size ulsnd_alloc_buffers needs.
 */
/* 0x00188568 */
int ulsnd_configure(int bufsize, int spu_area_size, int aux_size)
{
    int total;

    memset(&ulsnd_work, 0, 2500);
    ulsnd_work.standby = 0;
    ulsnd_set_debug(0);
    ulsnd_set_error_callback(ulsnd_report_error);
    if (bufsize < 0)
        bufsize = 1024;
    if (spu_area_size < 0)
        spu_area_size = 0x40000;
    if (aux_size < 0)
        aux_size = 0x40000;
    bufsize = (unsigned int)(bufsize + 63) / 64 * 64;
    spu_area_size = (unsigned int)(spu_area_size + 63) / 64 * 64;
    aux_size = (unsigned int)(aux_size + 63) / 64 * 64;
    total = bufsize * 6 + 7168;
    ulsnd_work.unk_1c = aux_size;
    ulsnd_work.bufsize = bufsize;
    ulsnd_work.spu_size = spu_area_size;
    ulsnd_work.total = total;
    return total;
}

/*
 * Driver info, sync and command blocks, channel map, SPU status, aux buffer, six ring buffers
 * ("HCLS"); frees all transfer records. Fails before ulsnd_configure.
 */
/* 0x00188640 */
int ulsnd_alloc_buffers(int heap)
{
    UlSnd *snd = &ulsnd_work;
    int err;
    int i;

    err = 0;
    if (snd->total == 0) {
        err = 0x80100000;
    } else {
        snd->iop_total = psHeapInit(heap, snd->total);
        {
            IopBlk *blk = ulsnd_iop_alloc(64);
            snd->iop_info = blk;
            blk->w[15] = 0;
        }
        {
            IopBlk *blk = ulsnd_iop_alloc(192);
            IopBlk *blk1, *blk2;
            snd->sync_blk = blk;
            snd->mlt_sync = blk;
            blk->w[15] = 0;
            blk1 = blk + 1;
            blk2 = blk + 2;
            snd->wave_sync = blk1;
            snd->iop_blk = blk2;
            blk1->w[15] = 0;
            blk2->w[15] = 0;
        }
        {
            int *shared = ulsnd_iop_alloc(1152);
            snd->shared = (UlShared *)shared;
            shared[0] = 0;
            shared[1] = 0;
        }
        {
            int *status = ulsnd_iop_alloc(896);
            ulsnd_spu_status[0] = status;
            snd->spu_status = status;
        }
        {
            int *aux_buf = ulsnd_iop_alloc(1024);
            aux_buf[0] = 0;
            snd->off = 16;
            snd->bufs[6] = (UlBuf *)aux_buf;
            snd->nbuf = 0;
            snd->cur = 0;
            snd->ncmd = 0;
        }
        for (i = 0; i < 6; i++) {
            UlBuf *blk = ulsnd_iop_alloc(ulsnd_work.bufsize);

            ulsnd_work.bufs[i] = blk;
            blk->tag = 0x534c4348;
            blk->size = 16;
            blk->count = 0;
        }
        for (i = 0; i < 16; i++)
            ulsnd_work.rec[i].kind = -1;
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* Retries until the IOP service answers, then sends the shared block addresses and sizes. */
/* 0x001887b8 */
int ulsnd_connect(void)
{
    UlSnd *snd = &ulsnd_work;
    int pkt[16];
    int i;
    int j;

    {
        for (;;) {
            if (sceSifBindRpc(ulsnd_work.client, 0x44534c55, 1) < 0) {
                ulsnd_printf("bind errr\n");
                ulsnd_printf("TRAP\n");
                do {
                } while (0);
                {
                    UlSnd *dbg_snd = &ulsnd_work;

                    while (dbg_snd->dbg)
                        ;
                }
            } else {
                while (sceSifCheckStatRpc(ulsnd_work.client) != 0)
                    ;
                if (snd->client[9] != 0)
                    goto done;
            }
            i = 0x100000;
            while ((j = i--) != 0)
                ;
        }
done:
        ulsnd_dprintf("RPC OK !!\n");
        {
            int *client;
            UlSnd *base;

            pkt[15] = 1;
            pkt[0] = (int)ulsnd_work.sync_blk;
            pkt[1] = (int)ulsnd_work.shared;
            pkt[2] = (int)ulsnd_work.spu_status;
            pkt[3] = (int)ulsnd_work.bufs[6];
            pkt[4] = ulsnd_work.bufsize;
            pkt[5] = ulsnd_work.spu_size;
            pkt[6] = ulsnd_work.unk_1c;
            client = ulsnd_work.client;
            base = &ulsnd_work;
            base = (UlSnd *)((char *)base + (((int)client & 1) >> 1)); /* adds zero */
            while (sceSifCheckStatRpc(client) != 0)
                ;
            sceSifCallRpc(client, 0x41414d44, 1, pkt, 64, base->iop_info, 64, 0, 0);
            do {
                ulsnd_dprintf("DMA Table OK !!\n");
            } while (0);
        }
    }
    return 0;
}

/*
 * Once a frame: waits for standby, sends the next buffer unless busy, advances bank transfers,
 * queues wave data, starts DMA. Returns the last error.
 */
/* 0x00188968 */
static int ulsnd_tick(void)
{
    UlSnd *snd = &ulsnd_work;
    int err = 0;
    int kick_err;

    snd->calls = snd->calls + 1;
    if (sceSifCheckStatRpc(snd->client) != 0)
        ulsnd_dprintf("RPC Busy !!\n");
    if (snd->standby == 0) {
        if (*(int *)((char *)snd->iop_info + 0x20000000 + 60) == 0)
            goto done;
        ulsnd_invalid_dcache(snd->iop_info, 64);
        ulsnd_dprintf("Sound Driver Standby !!\n");
        snd->standby = 1;
    }
    if (*(int *)((char *)snd->iop_blk + 0x20000000 + 60) != 0)
        err = 0x10200000;
    else
        err = ulsnd_flush_commands();
    ulsnd_rec_step();
    ulsnd_send_iop_waves();
    ulsnd_send_spu_waves();
    kick_err = ulsnd_dma_kick();
    if (kick_err)
        err = kick_err;
done:
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* 0x00188a58 */
static int ulsnd_is_standby(void)
{
    return ulsnd_work.standby;
}

/* 0x00188a68 */
static int ulsnd_set_error_callback(void (*cb)())
{
    ulsnd_work.err_cb = cb;
    return 0;
}

/* With it on, fatal errors stop in a TRAP loop. */
/* 0x00188a78 */
int ulsnd_set_debug(int dbg)
{
    ulsnd_work.dbg = dbg;
    return 0;
}

/* Level 0..15; higher is full volume. */
/* 0x00188a88 */
static int ulsnd_set_master_volume_level(char level)
{
    return ulsnd_set_master_volume(level * 0.06666667f);
}

/* 1 stereo, 0 mono; others rejected. */
/* 0x00188ac8 */
int ulsnd_set_stereo(int stereo)
{
    int ch = 0;

    if (stereo == 0)
        ch = 1;
    else if (stereo != 1)
        return 0x3000002;
    return ulsnd_set_output_mode(ch);
}

/* Effect channel 0. */
/* 0x00188b08 */
static int ulsnd_set_fx_prg0(char prg)
{
    return ulsnd_set_fx_prg(0, prg);
}

/* Both effect channels. */
/* 0x00188b30 */
int sdSndClearFxPrg(void)
{
    ulsnd_clear_fx_prg(0);
    return ulsnd_clear_fx_prg(1);
}

/* Transfer record. */
/* 0x00188b58 */
int sdMemBlkCreate(void * blk)
{
    return ulsnd_rec_alloc(blk);
}

/* 0x00188b78 */
int sdMemBlkDestroy(void * blk)
{
    return ulsnd_rec_free(blk);
}

/* mode -1: later downloads wait until done; 0: return at once. reserved is not read. */
/* 0x00188b98 */
int sdMemBlkSetPrm(int rec, int data, int size, unsigned int mode, int reserved)
{
    int flag = 0;

    if (mode == 0xffffffff) {
        flag = 1;
    } else if (mode != 0) {
        ulsnd_printf("TRAP\n");
        while (ulsnd_work.dbg)
            ;
    }
    return ulsnd_rec_setup(rec, data, size, flag);
}

/* 0 idle or queued, 2 loading, 1 done. */
/* 0x00188c40 */
int sdMemBlkGetStat(int rec, int *out)
{
    int state;
    int err;

    err = ulsnd_rec_get_state(rec, &state);
    switch (state) {
    case 2:
        *out = state;
        break;
    case 3:
        *out = 1;
        break;
    case 0:
    case 1:
        *out = 0;
        break;
    }
    return err;
}

/* 0x00188cb8 */
static int ulsnd_mlt_download(void * rec)
{
    return ulsnd_rec_start_mlt(rec);
}

/* Sound data with all its banks. */
/* 0x00188cd8 */
int sdMltDownload(void * rec)
{
    return ulsnd_mlt_download(rec);
}

/*
 * id: 'SMSB' sequence, 'SMDB'/'SMPB' wave, 'SOSB' one-shot, 'SFPB' effect bank. Others, 'SFOB' and
 * 'SMLT' among them, are rejected.
 */
/* 0x00188cf8 */
int sdBankDownload(int rec, unsigned int id, char idx)
{
    int mode = 0;

    switch (id) {
    case 0x42534d53:
        mode = 0;
        break;
    case 0x42444d53:
    case 0x42504d53:
        mode = 1;
        break;
    case 0x42534f53:
        mode = 2;
        break;
    case 0x42504653:
        mode = 3;
        break;
    case 0x424f4653:
    case 0x544c4d53:
        return 0x3000002;
    case 0x60000000:            /* unknown ids: default */
    case 0x70000000:
    default:
        return 0x3000002;
    }
    return ulsnd_rec_start_bank(rec, mode, idx);
}

/* 0x00188dd0 */
int sdMemBlkSetTransferMode(void)
{
    return 0;
}

/* 0x00188dd8 */
int sdMidiOpen(void *out)
{
    return ulsnd_handle_alloc(out);
}

/* 0x00188df8 */
static int sdMidiOpen_thunk(void *out)
{
    return sdMidiOpen(out);
}

/* 0x00188e18 */
int sdMidiClose(void *hn)
{
    return ulsnd_handle_free(hn);
}

/* 0x00188e38 */
static int sdMidiClose_thunk(void *hn)
{
    return sdMidiClose(hn);
}

/* 0x00188e58 */
static int sdMidiPlay(void *hn, char ch, char val)
{
    return ulsnd_handle_play(hn, ch, val);
}

/* steer(cast) */
/* 0x00188e80 */
static int sdMidiPlay_thunk(void *hn, char ch, char val, char extra)
{
    return ((int (*)(void *, int, int, int))sdMidiPlay)(hn, ch, val, extra);
}

/* 0x00188eb0 */
static int sdMidiStop(void *hn)
{
    return ulsnd_handle_stop(hn);
}

/* 0x00188ed0 */
static int sdMidiStop_thunk(void *hn)
{
    return sdMidiStop(hn);
}

/* 0x00188ef0 */
static int sdMidiCmd101(void *hn)
{
    return ulsnd_handle_cmd101(hn);
}

/* 0x00188f10 */
static int sdMidiCmd101_thunk(void *hn)
{
    return sdMidiCmd101(hn);
}

/* 0x00188f30 */
static int sdMidiCmd100(void *hn)
{
    return ulsnd_handle_cmd100(hn);
}

/* 0x00188f50 */
static int sdMidiCmd100_thunk(void *hn)
{
    return sdMidiCmd100(hn);
}

/* level -127..127: 0 normal, -127 silent, 127 double. Prints level and volume. */
/* steer(cast) */
/* 0x00188f70 */
int sdMidiSetVol(void *hn, char level)
{
    float vol;

    ((void (*)(char *, ...))printf)("vol : %x\n", level);
    if (level < 0)
        vol = (level + 127) * 0.007874016f;
    else
        vol = level * 0.007874016f + 1.0f;
    ((void (*)(char *, ...))printf)("vol : %f\n", vol);
    return ulsnd_handle_set_volume(hn, vol);
}

/* Fixed point: 0 = ratio 1.0, 16384 = 2. */
/* 0x00189030 */
static int ulsnd_handle_set_pitch_fixed(void *hn, short pitch)
{
    return ulsnd_handle_set_pitch(hn, (pitch + 16384) * 6.1035156e-05f);
}

/* 0x00189070 */
static int ulsnd_handle_set_pitch_fixed_thunk(void *hn, short pitch)
{
    return ulsnd_handle_set_pitch_fixed(hn, pitch);
}

/* 0x00189090 */
static int ulsnd_return_zero(void)
{
    return 0;
}

/* See ulsnd_tick. */
/* 0x00189098 */
int ulsnd_update(void)
{
    return ulsnd_tick();
}

/* As far as the DMA list has room; empties the list once all are out. */
/* 0x001890b8 */
static void ulsnd_send_iop_waves(void)
{
    UlWave *wave;
    int i;

    if (ulsnd_work.n_wave_iop) {
        for (i = 0; i < ulsnd_work.n_wave_iop; i++) {
            wave = &ulsnd_work.wave_iop[i];
            if (wave->kind == 0)
                continue;
            if (ulsnd_dma_add(wave->src, wave->dst, wave->size, (int *)(wave->rec + 12)) == 0x80600000)
                break;
            wave->kind = 0;
        }
        if (i == ulsnd_work.n_wave_iop) {
            ulsnd_dprintf("IOP DMA List Send Regist (%d)\n", i);
            ulsnd_work.n_wave_iop = 0;
        }
    }
}

static inline void ulsnd_send_spu_list(UlSnd *snd)
{
    UlWave *cur;
    int *blk;
    int *sz;
    int *psz;
    int *pblk;
    int off;
    int j;
    int adr;
    int next;
    int chunk;
    int jj;
    int lim;
    int ofs;
    int z;
    IopBlk *table;
    int one;
    int dstb;
    int listb;

    if (ulsnd_dma_free_entries() < 4)
        return;
    one = 1;
    do { } while (0);
    ulsnd_dprintf("send spu\n");
    snd = &ulsnd_work;
    j = 0;
    table = &ulsnd_spu_blk;
    dstb = (int)ulsnd_spu_wave_dst;
    listb = (int)ulsnd_spu_wave_list;
    {
        int *sizes = ulsnd_spu_wave_size;
        pblk = sizes - 2;
        sz = sizes;
    }
    psz = ulsnd_spu_wave_dst;
    off = 0;
    adr = 0;
    /* never taken */
    if (AZ("send wave  data:%08x adr:%08x  size:%6x  list:%d cur_head:%d size:%d\n"))
        snd->spu_state = 0;
    top:
    ulsnd_spu_wave_index = 0;
    if (snd->n_wave_spu <= 0)
        goto fin;
        cur = snd->wave_spu;
        if (cur->kind == 0) {
            jj = j << 6;
            next = adr + 64;
        search:
            ulsnd_spu_wave_index = ulsnd_spu_wave_index + 1;
            if (!(ulsnd_spu_wave_index < ulsnd_work.n_wave_spu))
                goto fin;
            cur = &ulsnd_work.wave_spu[ulsnd_spu_wave_index];
            if (cur->kind == 0)
                goto search;
        } else {
            jj = j << 6;
            next = adr + 64;
        }
        if (!(ulsnd_spu_wave_index < ulsnd_work.n_wave_spu))
            goto fin;
        if (ulsnd_dma_free_entries() < 4)
            goto fin;
        blk = (int *)(off + listb);
        memset(blk, 0, 64);
        ofs = cur->ofs;
        lim = ulsnd_work.spu_size;
        *sz = ofs;
        {
            int wave_dst;

            wave_dst = cur->dst;
            z = cur->size;
            *(int *)((char *)ulsnd_spu_wave_size + 4 + off) = z;
            *(void **)blk = (void *)(wave_dst + ofs);
        }
        /* never taken */
        if (AZ(cur)) { cur[0].rec = jj; cur[0].dst = jj; cur[0].src = jj; cur[0].ofs = jj; cur[0].size = (int)&ulsnd_work; cur[1].rec = (int)&ulsnd_work; cur[1].dst = (int)&ulsnd_work; cur[1].src = (int)&ulsnd_work; cur[1].ofs = (int)&ulsnd_work; cur[1].size = next; cur[2].rec = next; cur[2].dst = j; cur[2].src = (int)sz; cur[2].ofs = (int)((char *)ulsnd_spu_wave_size + 4 + off); cur[2].size = z; z = jj + AZ(jj); cur[3].rec = z; cur[3].dst = z; }
        chunk = ((unsigned int)(cur->size - cur->ofs + 63) >> 6) << 6;
        if ((unsigned int)(adr + chunk + 128) < (unsigned int)lim) {
            *(int *)(jj + dstb) = chunk;
            cur->kind = 0;
            cur->ofs = cur->size;
        } else {
            int room = lim - adr - 128;
            *(int *)(jj + dstb) = room;
            cur->ofs += room;
        }
        ulsnd_dma_add(pblk, snd->iop_info->w[3] + adr, 64, 0);
        pblk += 16;
        off += 64;
        ulsnd_dprintf("send wave  data:%08x adr:%08x  size:%6x  list:%d cur_head:%d size:%d\n",
                     cur->src + *sz, snd->iop_info->w[3] + next, *psz, ulsnd_spu_wave_index, j++, next);
        ulsnd_dma_add((void *)(cur->src + *sz), snd->iop_info->w[3] + next, *psz, (int *)(cur->rec + 12));
        sz += 16;
        adr = next + (((unsigned int)*psz + 63) >> 6 << 6);
        psz += 16;
        if (!((unsigned int)adr < (unsigned int)(snd->spu_size - 64)))
            goto fin;
        if (j < 10)
            goto top;
fin:
    *pblk = 0;
    ulsnd_dma_add(pblk, snd->iop_info->w[3] + adr, 64, 0);
    {
    int table_dst = (int)*(void **)&snd->iop_info->w[0] + 64;
    table->w[15] = one;
    ulsnd_dma_add(table, table_dst, 64, 0);
    }
    snd->spu_state = one;
    return;
}

/*
 * Rounds through the aux area: state 0 queues up to ten waves (or parts) with descriptors and
 * tells the driver; state 1 waits for its reply. Empties the list when none are left.
 */
/* 0x00189180 */
static void ulsnd_send_spu_waves(void)
{
    UlSnd *snd = &ulsnd_work;

    if (snd->n_wave_spu == 0)
        return;
    switch (snd->spu_state) {
    case 0:
        ulsnd_send_spu_list(snd);
        return;
    case 1:
        if (*(int *)((char *)snd->wave_sync + 0x20000000 + 60) != 1)
            return;
        snd->wave_sync->w[15] = 0;
        snd->spu_state = 0;
        ulsnd_spu_wave_index = 0;
        if (snd->n_wave_spu > 0) {
            if (snd->wave_spu[0].kind == 0) {
                for (;;) {
                    ulsnd_spu_wave_index = ulsnd_spu_wave_index + 1;
                    if (!(ulsnd_spu_wave_index < ulsnd_work.n_wave_spu))
                        goto done;
                    if (ulsnd_work.wave_spu[ulsnd_spu_wave_index].kind != 0)
                        break;
                }
            }
            if (ulsnd_spu_wave_index < ulsnd_work.n_wave_spu)
                return;
        }
        break;
    default:
        return;
    }
done:
    ulsnd_dprintf("SPU DMA List Send Regist (%d)\n", ulsnd_work.n_wave_spu);
    ulsnd_work.n_wave_spu = 0;
}

/*
 * size rounded to 16. A given flag is cleared now and gets the DMA id once started.
 * Fails when the 22-entry list is full.
 */
/* 0x001895a8 */
static int ulsnd_dma_add(int src, int dst, int size, int *flag)
{
    UlSnd *snd = &ulsnd_work;
    int err = 0;
    UlDma *entry;
    UlDma *list;

    if (snd->n_dma >= 22) {
        err = 0x80600000;
    } else {
        if (flag)
            *flag = 0;
        snd->flags[snd->n_dma] = flag;
        list = snd->dma;
        entry = &list[snd->n_dma];
        entry->src = src;
        entry->dst = dst;
        entry->size = ((unsigned int)(size + 15) >> 4) << 4;
        entry->reserved = 0;
        snd->n_dma = snd->n_dma + 1;
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* 0x00189658 */
static int ulsnd_dma_free_entries(void)
{
    return 22 - ulsnd_work.n_dma;
}

/* Up to 16 entries; each flag gets its DMA id. A busy channel is reported and retried next time. */
/* 0x00189670 */
static int ulsnd_dma_kick(void)
{
    UlSnd *snd = &ulsnd_work;
    int err = 0;
    int n;
    int id;
    int i;

    if (snd->n_dma) {
        FlushCache(0);
        n = snd->n_dma - snd->n_dma_sent;
        if (!(n < 17))
            n = 16;
        id = sceSifSetDma(&ulsnd_work.dma[ulsnd_work.n_dma_sent], n);
        if (id == 0) {
            ulsnd_printf("DMA Busy...  Try Next Time.\n");
            err = 0x10300000;
        } else {
            for (i = ulsnd_work.n_dma_sent; i < ulsnd_work.n_dma_sent + n; i++) {
                if (ulsnd_work.flags[i])
                    *ulsnd_work.flags[i] = id;
            }
            ulsnd_work.n_dma_sent = ulsnd_work.n_dma_sent + n;
            if (ulsnd_work.n_dma == ulsnd_work.n_dma_sent) {
                ulsnd_work.n_dma = 0;
                ulsnd_work.n_dma_sent = 0;
            }
        }
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* On failure prints the heap state (TRAP loop with debug on). */
/* 0x00189790 */
static void *ulsnd_iop_alloc(int size)
{
    int info[8];
    void *blk;

    blk = psHeapAlloc(ulsnd_work.iop_total, size);
    if (blk == 0) {
        psHeapGetStat(ulsnd_work.iop_total, info);
        ulsnd_printf("Heap : Malloc Error !!\n");
        ulsnd_printf("       size:%x  rest:%x\n", size, info[2]);
        ulsnd_printf("TRAP\n");
        while (ulsnd_work.dbg)
            ;
    }
    return blk;
}

/* Zero-filled. */
/* 0x00189840 */
static void *ulsnd_iop_heap_alloc(int size)
{
    int info[8];
    void *blk;

    blk = psHeapCalloc(ulsnd_work.iop_total, size);
    if (blk == 0) {
        psHeapGetStat(ulsnd_work.iop_total, info);
        ulsnd_printf("Heap : Malloc Error !!\n");
        ulsnd_printf("       size:%d  rest:%d\n", size, info[2]);
        ulsnd_printf("TRAP\n");
        while (ulsnd_work.dbg)
            ;
    }
    return blk;
}

/*
 * 'IOPD' chunks to the IOP list, 'SPUD' to the SPU list, at the addresses the map reserves for
 * slot idx of kind. A full list (32) prints "buffer over". Returns the chunk after 'ENDB'.
 */
/* 0x001898f0 */
static int *ulsnd_parse_bank(int *p, int kind, int idx, UlRec *rec)
{
    int iop_dst = 0;
    int spu_dst = 0;
    int *wave;
    unsigned int tag;
    UlWave *entry;

    switch (kind) {
    case 0:
        iop_dst = ulsnd_shared->kind0[idx].addr;
        break;
    case 1:
        iop_dst = ulsnd_shared->kind1[idx].iop_addr;
        spu_dst = ulsnd_shared->kind1[idx].spu_addr;
        break;
    case 2:
        iop_dst = ulsnd_shared->kind2[idx].iop_addr;
        spu_dst = ulsnd_shared->kind2[idx].spu_addr;
        break;
    case 3:
        iop_dst = ulsnd_shared->kind3[idx].addr;
        break;
    case 4:
        spu_dst = ulsnd_shared->kind4[idx].addr;
        break;
    }
    p += 4;
    tag = *p;
    wave = p + 4;
    if (tag == 0x42444e45)
        goto early;
    {
    do {
loop:
        switch (tag) {
        case 0x44504f49: {
            UlSnd *snd = &ulsnd_work;
            UlWave *base;

            {
                volatile UlWave *vw;
                int len;
                int small = snd->n_wave_iop < 32;

            do {
                p = (int *)((char *)p + (((int)p & 1) >> 1));   /* adds zero */
                if (!small)
                    ulsnd_printf("IOP DMA \203o\203b\203t\203@\203I\201[\203o\201[\n");
                base = snd->wave_iop;
                entry = base + snd->n_wave_iop;
                vw = entry;
                vw->kind = 3;
                vw->src = (int)wave;
                len = p[1] - 16;
                vw->ofs = 0;
            } while (0);
                vw->rec = (int)rec;
                entry->size = len;
                vw->dst = iop_dst;
            }
            snd->n_wave_iop = snd->n_wave_iop + 1;
            tag = p[1];
            p = (int *)((char *)p + tag);
            wave = p + 4;
            tag = *p;
            break;
        }
        case 0x44555053: {
            UlSnd *snd = &ulsnd_work;
            UlWave *base;

            if (snd->n_wave_spu >= 32)
                ulsnd_printf("SPU DMA \203o\203b\203t\203@\203I\201[\203o\201[\n");
            base = snd->wave_spu;
            entry = base + snd->n_wave_spu;
            {
                volatile UlWave *vw = entry;

                vw->kind = 4;
                vw->src = (int)wave;
                entry->size = p[1] - 16;
                vw->ofs = 0;
                vw->rec = (int)rec;
                do { vw->dst = spu_dst; } while (0);
            }
            snd->n_wave_spu = snd->n_wave_spu + 1;
            tag = p[1];
            p = (int *)((char *)p + tag);
            wave = p + 4;
            tag = *p;
            break;
        }
        case 0x42444e45:
            break;
        }
    } while (0);
    if (tag != 0x42444e45)
        goto loop;
    }
    goto done;
early:
/* same value wave already holds */
    wave = p + 4 + AZ(p);
done:
    return wave;
}

/*
 * 'MSQB', 'MPGB', 'OSTB', 'FXPB' banks queued and marked loaded; 'FXWB' and banks too small for a
 * header skipped. Unknown chunk: "bad multi-file format", -1; else 0 at 'ENDM'.
 */
/* 0x00189b60 */
static int ulsnd_parse_mlt(int *mlt, void *rec)
{
    int *cur;
    unsigned int tag;
    int idx;

    cur = mlt;
    cur += 4;
    tag = *cur;
    if (tag == 0x4d444e45)
        return 0;
    do {
        switch (tag) {
        case 0x4251534d:
            if ((unsigned int)cur[1] >= 49) {
                idx = cur[3];
                cur = ulsnd_parse_bank(cur, 0, idx, rec);
                ulsnd_set_channel_on(1, 0, idx);
                tag = *cur;
            } else {
                cur = (int *)((char *)cur + cur[1]);
                tag = *cur;
            }
            break;
        case 0x4247504d:
            if ((unsigned int)cur[1] >= 65) {
                idx = cur[3];
                cur = ulsnd_parse_bank(cur, 1, idx, rec);
                ulsnd_set_channel_on(1, 1, idx);
                tag = *cur;
            } else {
                cur = (int *)((char *)cur + cur[1]);
                tag = *cur;
            }
            break;
        case 0x4254534f:
            if ((unsigned int)cur[1] >= 65) {
                idx = cur[3];
                cur = ulsnd_parse_bank(cur, 2, idx, rec);
                ulsnd_set_channel_on(1, 2, idx);
                tag = *cur;
            } else {
                cur = (int *)((char *)cur + cur[1]);
                tag = *cur;
            }
            break;
        case 0x42505846:
            if ((unsigned int)cur[1] >= 49) {
                idx = cur[3];
                cur = ulsnd_parse_bank(cur, 3, idx, rec);
                ulsnd_set_channel_on(1, 3, idx);
                tag = *cur;
            } else {
                cur = (int *)((char *)cur + cur[1]);
                tag = *cur;
            }
            break;
        case 0x42575846:
            cur = (int *)((char *)cur + cur[1]);
            tag = *cur;
            break;
        case 0x4d444e45:
            break;
        default:
            ulsnd_printf("DMA \203}\203\213\203`\203t\203@\203C\203\213\202\314\203t\203H\201[\203}\203b\203g\202\252\210\331\217\355\202\305\202\267\n");
            ulsnd_printf("mlt_data=%08x  mark=%08x  markadr=%08x\n", mlt, cur[0], cur);
            return -1;
        }
    } while ((tag ^ 0x4d444e45) != 0);
    return 0;
}

/*
 * Phases (ulsnd_work.phase): build and send the map, await the reply, queue all banks, drain the
 * wave lists and last DMA, tell the driver, mark the record done.
 */
/* 0x00189d98 */
static void ulsnd_load_mlt_step(UlRec *rec)
{
    int one;
    int dst;
    IopBlk *blk;

    switch (ulsnd_work.phase) {
    case 0:
        if (ulsnd_dma_free_entries() < 2)
            break;
        if (ulsnd_build_channel_map(rec->data)) {
            ulsnd_work.cur_rec = 0;
            break;
        }
        one = 1;
        ulsnd_dma_add(ulsnd_work.shared, ulsnd_work.iop_info->w[1], 1152, 0);
        dst = ulsnd_work.iop_info->w[0];
        blk = &ulsnd_mlt_blk;
        blk->w[15] = one;
        ulsnd_dma_add(blk, dst, 64, 0);
        ulsnd_work.phase = one;
        break;
    case 1:
        if (*(int *)((char *)ulsnd_work.mlt_sync + 0x20000000 + 60) != 1)
            break;
        ulsnd_work.mlt_sync->w[15] = 0;
        ulsnd_work.phase = 2;
    case 2:
        ulsnd_parse_mlt(rec->data, rec);
        ulsnd_work.phase = 3;
    case 3:
        if (ulsnd_work.n_wave_iop)
            break;
        ulsnd_work.phase = 4;
    case 4:
        if (ulsnd_work.n_wave_spu)
            break;
        ulsnd_work.phase = 5;
    case 5:
        if (rec->dma_id == 0)
            break;
        if (sceSifDmaStat(rec->dma_id) >= 0)
            break;
        ulsnd_bank_ready(-1);
        rec->kind = -5;
        rec->status = 3;
        ulsnd_work.cur_rec = 0;
        break;
    }
}

/*
 * Queue, drain, last DMA, mark the slot loaded (telling the driver for a wave bank), record done.
 */
/* 0x00189f18 */
static void ulsnd_load_bank_step(UlRec *rec)
{
    UlSnd *snd = &ulsnd_work;

    switch (snd->phase) {
    case 0:
        ulsnd_parse_bank(rec->data, rec->table, rec->index, rec);
        snd->phase++;
        return;
    case 1:
        if (snd->n_wave_iop)
            return;
        snd->phase = snd->phase + 1;
    case 2:
        if (ulsnd_work.n_wave_spu)
            return;
        ulsnd_work.phase = ulsnd_work.phase + 1;
    case 3:
        if (rec->dma_id == 0)
            return;
        if (sceSifDmaStat(rec->dma_id) >= 0)
            return;
        if (rec->table == 1)
            ulsnd_bank_ready(rec->index);
        ulsnd_set_channel_on(1, rec->table, rec->index);
        rec->kind = -5;
        rec->status = 3;
        ulsnd_work.cur_rec = 0;
    }
}

/*
 * Queued records count down; the one reaching 0 starts. No data: prints "memory block has no data
 * set" (TRAP loop with debug on), then starts anyway.
 */
/* 0x0018a050 */
static void ulsnd_load_select_next(void)
{
    UlRec *rec;
    int i;

    if (ulsnd_work.cur_rec != 0)
        return;
    if (ulsnd_work.queued == 0)
        return;
    for (i = 0; i < 16; i++) {
        if (ulsnd_work.rec[i].kind > 0)
            ulsnd_work.rec[i].kind--;
    }
    ulsnd_work.queued = ulsnd_work.queued - 1;
    for (i = 0; i < 16; i++) {
        if (ulsnd_work.rec[i].kind == 0)
            break;
    }
    if (i != 16) {
        rec = &ulsnd_work.rec[i];
        rec->status = 2;
        switch (rec->state) {
        case 0:
            ulsnd_printf("\203f\201[\203^\202\252\203Z\203b\203g\202\263\202\352\202\304\202\242\202\310\202\242\203\201\203\202\203\212\203u\203\215\203b\203N\n");
            ulsnd_printf("TRAP\n");
            while (ulsnd_work.dbg)
                ;
        case 1:
        case 2:
            ulsnd_work.n_wave_spu = 0;
            ulsnd_work.phase = 0;
            ulsnd_work.n_wave_iop = 0;
            ulsnd_work.cur_rec = rec;
        case 3:
        case 4:
            break;
        }
    }
}

/* Starts the next queued one when none is running. */
/* 0x0018a1a0 */
static void ulsnd_rec_step(void)
{
    UlSnd *snd = &ulsnd_work;
    UlRec *rec;

    if (snd->cur_rec == 0) {
        ulsnd_load_select_next();
    }
    rec = snd->cur_rec;
    if (rec != 0) {
        switch (rec->state) {
        case 1:
            ulsnd_load_mlt_step(rec);
            break;
        case 2:
            ulsnd_load_bank_step(rec);
            break;
        case 3:
            rec->kind = -3;
            snd->cur_rec->status = 3;
            snd->cur_rec = 0;
            break;
        case 4:
            rec->kind = -3;
            snd->cur_rec->status = 3;
            snd->cur_rec = 0;
            break;
        }
    }
}

/* 16 records; *out = 0 and fails when all are in use. */
/* 0x0018a258 */
static int ulsnd_rec_alloc(UlRec **out)
{
    int err = 0;
    int i;

    for (i = 0; i < 16; i++) {
        if (ulsnd_work.rec[i].kind == -1)
            break;
    }
    if (i == 16) {
        *out = 0;
        err = 0x80500000;
    } else {
        *out = &ulsnd_work.rec[i];
        ulsnd_work.rec[i].kind = -2;
        ulsnd_work.rec[i].status = 0;
        ulsnd_work.rec[i].unk_03 = 0;
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* Warns if still queued. */
/* 0x0018a310 */
static int ulsnd_rec_free(UlRec *rec)
{
    int err;

    if (rec == 0 || rec->kind == -1) {
        err = 0x80200000;
    } else {
        err = 0x10100000;
        rec->kind = -1;
        if (rec->status != 1)
            err = 0;
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* Ready to download; warns if still queued. */
/* 0x0018a370 */
static int ulsnd_rec_setup(UlRec *rec, int data, int size, int sync)
{
    int err;

    if (rec == 0 || rec->kind == -1) {
        err = 0x80200000;
    } else {
        err = 0x10100000;
        rec->data = data;
        rec->unk_08 = size;
        rec->sync = sync;
        rec->kind = -3;
        if (rec->status != 1)
            err = 0;
        rec->status = 0;
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* 0x0018a3e0 */
static int ulsnd_rec_get_state(UlRec *rec, int *out)
{
    int err = 0;

    if (rec == 0 || rec->kind == -1)
        err = 0x80200000;
    else
        *out = rec->status;
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* Data must start with "MLT". With rec->sync, runs driver steps until done. */
/* 0x0018a438 */
static int ulsnd_rec_start_mlt(UlRec *rec)
{
    int err = 0;
    int st;

    if (rec == 0 || rec->kind == -1) {
        err = 0x80200000;
    } else if (rec->kind == -2) {
        err = 0x80700000;
    } else if (*(int *)rec->data != 0x544c4d) {
        err = 0x80800000;
    } else {
        ulsnd_work.queued = ulsnd_work.queued + 1;
        rec->kind = ulsnd_work.queued;
        rec->status = 1;
        rec->table = -1;
        rec->state = 1;
        rec->index = -1;
        rec->unk_03 = 0;
        if (rec->sync == 1) {
            while (ulsnd_rec_get_state(rec, &st), st != 3) {
                ulsnd_rec_step();
                ulsnd_send_iop_waves();
                ulsnd_send_spu_waves();
                err = ulsnd_dma_kick();
            }
        }
    }
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* Data sizes of the 'IOPD' and 'SPUD' chunks. */
/* 0x0018a548 */
void ulsnd_bank_wave_sizes(UlBank *bank, int *iop_size, int *spu_size)
{
    unsigned int tag;
    int *chunk;

    chunk = bank->chunks;
    chunk += 4;
    tag = *chunk;

    while (tag != 0x42444e45) {
        switch (tag) {
        case 0x44504f49:
            *iop_size = chunk[1] - 16;
            goto next;
        case 0x44555053:
            *spu_size = chunk[1] - 16;
        next:
            tag = chunk[1];
            chunk = (int *)((char *)chunk + tag);
            tag = *chunk;
            break;
        case 0x42444e45:
            break;
        }
    }
}

/*
 * Tag must match kind, slot reserved in the map, IOP/SPU data within the reserved sizes. With
 * rec->sync, runs driver steps until done.
 */
/* 0x0018a5d0 */
static int ulsnd_rec_start_bank(UlRec *rec, int kind, int idx)
{
    int err = 0;
    int iop_size;
    int spu_size;
    int st;
    int lim;
    unsigned int tag;
    char *col;

    if (rec == 0 || rec->kind == -1) {
        err = 0x80200000;
        goto done;
    }
    if (rec->kind == -2) {
        err = 0x80700000;
        goto done;
    }
    tag = *(unsigned int *)rec->data;
    switch (tag) {
    case 0x4251534d:
        if (kind != 0) {
            err = 0x80800000;
            goto done;
        }
        break;
    case 0x4247504d:
        if (kind != 1) {
            err = 0x80800000;
            goto done;
        }
        break;
    case 0x42505846:
        if (kind != 3) {
            err = 0x80800000;
            goto done;
        }
        break;
    case 0x4254534f:
        if (kind != 2) {
            err = 0x80800000;
            goto done;
        }
        break;
    case 0x42575846:    /* 'FXWB' cannot be loaded alone */
    default:
        err = 0x80800000;
        goto done;
    }
    ulsnd_bank_wave_sizes(rec, &iop_size, &spu_size);
    switch (*(unsigned int *)rec->data) {
    case 0x4251534d:
        lim = TW(40, 12, idx);
        if (lim == 0) {
            err = 0x80900000;
            goto done;
        }
        if (lim < (unsigned int)iop_size) {
            err = 0x80a00000;
            goto done;
        }
        break;
    case 0x4247504d:
        lim = TW(240, 20, idx);
        if (lim == 0) {
            err = 0x80900000;
            goto done;
        }
        if (lim < (unsigned int)iop_size) {
            err = 0x80a00000;
            goto done;
        }
        if (TW(248, 20, idx) < (unsigned int)spu_size) {
            err = 0x80a00000;
            goto done;
        }
        break;
    case 0x4254534f:
        lim = TW(568, 20, idx);
        if (lim == 0) {
            err = 0x80900000;
            goto done;
        }
        if (lim < (unsigned int)iop_size) {
            err = 0x80a00000;
            goto done;
        }
        if (TW(576, 20, idx) < (unsigned int)spu_size) {
            err = 0x80a00000;
            goto done;
        }
        break;
    case 0x42505846:
        lim = TW(896, 12, idx);
        if (lim == 0) {
            err = 0x80900000;
            goto done;
        }
        if (lim < (unsigned int)iop_size) {
            err = 0x80a00000;
            goto done;
        }
        break;
    case 0x42575846:
        err = 0x80800000;
        goto done;
    default:
        err = 0x80800000;
        goto done;
    }
    ulsnd_work.queued = ulsnd_work.queued + 1;
    rec->kind = ulsnd_work.queued;
    rec->status = 1;
    rec->state = 2;
    rec->table = kind;
    rec->index = idx;
    if (rec->sync == 1) {
        while (ulsnd_rec_get_state(rec, &st), st != 3) {
            ulsnd_rec_step();
            ulsnd_send_iop_waves();
            ulsnd_send_spu_waves();
            err = ulsnd_dma_kick();
        }
    }
done:
    if (err)
        ulsnd_error_callback(err);
    return err;
}

/* 0x0018a8a0 */
static int ulsnd_return_zero_2(void)
{
    return 0;
}

/* kind 0..3; on 1 = loaded. */
/* 0x0018a8a8 */
static void ulsnd_set_channel_on(int on, int kind, int ch)
{
    switch (kind) {
    case 0:
        ulsnd_shared->kind0[ch].on = on;
        break;
    case 1:
        ulsnd_shared->kind1[ch].on = on;
        break;
    case 2:
        ulsnd_shared->kind2[ch].on = on;
        break;
    case 3:
        ulsnd_shared->kind3[ch].on = on;
        break;
    }
}

/* IOP size into out; returns the next chunk, or 0 if not 'IOPD'. */
/* 0x0018a950 */
static char *ulsnd_chunk_iopd(UlChunk *chunk, int *out)
{
    if (chunk->tag != 0x44504f49)
        return 0;
    out[1] = chunk->unk_0c;
    return (char *)chunk + chunk->size;
}

/* SPU address and size into out; returns the next chunk, or 0 if not 'SPUD'. */
/* 0x0018a980 */
static char *ulsnd_chunk_spud(UlChunk *chunk, int *out)
{
    int x;
    int y;

    if (chunk->tag != 0x44555053)
        return 0;
    y = chunk->unk_0c;
    x = chunk->unk_08;
    out[1] = y;
    out[0] = x;
    return (char *)chunk + chunk->size;
}

/*
 * "MAP" header, sub-tables 'MSMP', 'MPMP', 'OSMP', 'FPMP', 'FWMP', all slots empty, then each
 * bank's IOP/SPU areas. Unknown chunk: "bad map multi-file format", -1.
 */
/* 0x0018a9b8 */
int ulsnd_build_channel_map(int *p)
{
    int *map = (int *)ulsnd_shared;
    int *msmp;
    int *mpmp;
    int *osmp;
    int *fpmp;
    int *fwmp;
    int *base = p;
    int *mpmp_on;
    int *osmp_on;
    int *fpmp_on;
    int *mpmp_ent;
    int *osmp_ent;
    int i;

    msmp = map + 7;
    mpmp = map + 57;
    osmp = map + 139;
    fpmp = map + 221;
    fwmp = map + 271;
    mpmp_on = map + 63;
    osmp_on = map + 145;
    fpmp_on = map + 225;
    map[0] = 0x50414d;
    map[1] = 1116;
    map[2] = 28;
    map[3] = 228;
    map[4] = 556;
    map[5] = 884;
    map[6] = 1084;
    map[7] = 0x504d534d;
    msmp[1] = 200;
    map[57] = 0x504d504d;
    mpmp[1] = 328;
    map[139] = 0x504d534f;
    osmp[1] = 328;
    map[221] = 0x504d5046;
    fpmp[1] = 200;
    map[271] = 0x504d5746;
    fwmp[1] = 32;
    for (i = 15; i >= 0; i--)
        map[11 + i * 3] = 0;
    for (i = 15; i >= 0; i--)
        mpmp_on[i * 5] = 0;
    for (i = 15; i >= 0; i--)
        osmp_on[i * 5] = 0;
    for (i = 15; i >= 0; i--)
        fpmp_on[i * 3] = 0;
    p += 4;
    while (*p != 0x4d444e45) {
        switch ((unsigned int)*p) {
        case 0x4251534d:
            p = ulsnd_chunk_iopd(p + 4, msmp + (p[3] * 3 + 2));
            p += 4;
            break;
        case 0x4247504d:
            i = p[3];
            mpmp_ent = mpmp + (i * 5 + 2);
            p = ulsnd_chunk_spud(ulsnd_chunk_iopd(p + 4, mpmp_ent), mpmp_ent + 2);
            p += 4;
            break;
        case 0x4254534f:
            i = p[3];
            osmp_ent = osmp + (i * 5 + 2);
            p = ulsnd_chunk_spud(ulsnd_chunk_iopd(p + 4, osmp_ent), osmp_ent + 2);
            p += 4;
            break;
        case 0x42505846:
            p = ulsnd_chunk_iopd(p + 4, fpmp + (p[3] * 3 + 2));
            p += 4;
            break;
        case 0x42575846:
            p = ulsnd_chunk_spud(p + 4, fwmp + p[3] * 3 + 3);
            p += 4;
            break;
        case 0x4d444e45:
            break;
        default:
            ulsnd_printf("map \203}\203\213\203`\203t\203@\203C\203\213\202\314\203t\203H\201[\203}\203b\203g\202\252\210\331\217\355\202\305\202\267\n");
            ulsnd_printf("mlt_data=%08x  mark=%08x  markadr=%08x\n", base, p[0], p);
            return -1;
        }
    }
    return 0;
}
