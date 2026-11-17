/*
 * Sound banks: downloads banks and the multi-unit project to the sound driver at once or via
 * queued "sdDMA" tasks (SE muted meanwhile); "MpbLoad" reads a bank file in the background first.
 * Also the "Sound HostCommand" and "Sound Slot" debug windows.
 */

#include "types_game.h"
#include "color.h"
#include "debug.h"
#include "gw_draw.h"
#include "gw_system.h"
#include "gw_window.h"
#include "ulsnd.h"
#include "sound_bank.h"
#include "debugmenu.h"
#include "file.h"
#include "float_bits.h"
#include "line_batch.h"
#include "sound_main.h"
#include "sound_midise.h"
#include "system.h"
#include "task.h"

#define SD_CHECK(name, err) \
    do { \
        sound_sd_err = (err); \
        if (err) \
            ERROR_PRINTF("%s (E) : %08x\n", name, err); \
    } while (0)

extern int sound_bank_state;
extern unsigned system_flags;
extern struct TaskDesc sound_dma_desc;
extern struct TaskDesc sound_mpb_load_desc;
extern void *system1_work;
extern int sound_host_command_window;
extern char str_sound_host_command[];
extern int sound_host_command_idx;
extern int sound_host_command_samples[256];

extern struct {
    int pad[6];
    float x;
    int pad1c;
    float y;
} screen_layout;

extern int sound_slot_window;
extern char str_sound_slot[];
extern int *ulsnd_spu_status;
extern char str_sound_slot_off[];  /* "------------------------" */
extern char str_sound_slot_on[];   /* "000000000000000000000000" */
extern char *bits_to_string(int v, int n, char *on, char *off);

static void sound_debug_menu_init(void);

/* 0x00163df8 */
static int sound_bank_get_state(void)
{
    return sound_bank_state;
}

/* Returns 1. */
/* 0x00163e08 */
int sound_bank_debug_init(void)
{
    sound_debug_menu_init();
    return 1;
}

/* Clears the bank state; returns whether the last driver call succeeded. */
/* 0x00163e28 */
static int sound_bank_check_error(void)
{
    int ok = sound_sd_err == 0;

    sound_bank_state = 0;
    return ok;
}

/* 0x00163e40 */
static int sound_bank_return_one(void)
{
    return 1;
}

/* 0x00163e48 */
static int sound_bank_return_one_2(void)
{
    return 1;
}

/* 0x36330 bytes, at once; driver errors printed and kept in sound_sd_err. */
/* 0x00163e50 */
static void sound_mlt_download(void *buf)
{
    int blk[4];
    int err;

    err = sdMemBlkCreate(blk);
    sound_sd_err = err;
    if (err)
        ERROR_PRINTF("%s (E) : %08x\n", "sdMemBlkCreate", err);
    err = sdMemBlkSetPrm(blk[0], (int)buf, 0x36330, 0xffffffff, 0);
    sound_sd_err = err;
    if (err)
        ERROR_PRINTF("%s (E) : %08x\n", "sdMemBlkSetPrm", err);
    err = sdMltDownload(blk[0]);
    sound_sd_err = err;
    if (err)
        ERROR_PRINTF("%s (E) : %08x\n", "sdMltDownload", err);
    err = sdMemBlkDestroy(blk[0]);
    sound_sd_err = err;
    if (err)
        ERROR_PRINTF("%s (E) : %08x\n", "sdMemBlkDestroy", err);
}

/* 0x00163f38 */
void sound_mlt_load_file(void *path)
{
    void *buf = file_load_sync(path, 0, 0);

    sound_mlt_download(buf);
    file_heap_free(buf);
}

/* At once; errors as in sound_mlt_download. */
/* 0x00163f70 */
static void sound_bank_download(void *buf, int size, int bank, int mode)
{
    int blk[4];
    int err;

    sound_sd_err = err = sdMemBlkCreate(blk);
    if (err)
        ERROR_PRINTF("%s (E) : %08x\n", "sdMemBlkCreate", err);
    err = sdMemBlkSetPrm(blk[0], (int)buf, size, 0xffffffff, 0);
    SD_CHECK("sdMemBlkSetPrm", err);
    err = sdBankDownload(blk[0], bank, mode);
    SD_CHECK("sdBankDownload", err);
    err = sdMemBlkDestroy(blk[0]);
    SD_CHECK("sdMemBlkDestroy", err);
}

/* 0x00164088 */
static void sound_bank_load_file(void *path, int bank, int mode)
{
    void *buf = file_load_sync(path, 0, 0);

    sound_bank_download(buf, file_get_size(path), bank, mode);
    file_heap_free(buf);
}

/* 0x001640f8 */
static int sound_bank_return_zero(void)
{
    return 0;
}

/* 0x00164100 */
static void sound_dma_init(SoundDmaWork *w)
{
    w->phase = 0;
    w->polls = 0;
    w->se_muted = 0;
    w->created = 0;
}

/*
 * Unmutes the MIDI SE (laser SE for mode 4), frees the block, sets up the port's SE channels
 * (modes 1, 2, 4), wakes the next transfer.
 */
/* 0x00164118 */
static void sound_dma_release(SoundDmaWork *w)
{
    int err;
    Task *hdr;
    SoundDmaWork *nxt;

    if (w->se_muted) {
        if (w->mode == 4)
            SoundMidiSeEnableLaser(1);
        else
            SoundMidiSeEnable(1);
    }
    if (w->created) {
        err = sdMemBlkDestroy(w->blk);
        sound_sd_err = err;
        if (err)
            ERROR_PRINTF("%s (E) : %08x\n", "sdMemBlkDestroy", err);
        switch (w->mode) {
        case 1:
            se_channel_setup_port1();
            break;
        case 2:
            se_channel_setup_port2();
            break;
        case 4:
            se_channel_setup_port3();
            break;
        }
    }
    hdr = TASK_OF(w)->next;
    nxt = hdr ? (SoundDmaWork *)(hdr + 1) : 0;
    if (nxt)
        TASK_OF(nxt)->flags &= ~TASK_F_SUSPENDED;
}

/* Polls until done (error after 60 idle polls); a soft reset ends it at once. */
/* 0x00164230 */
static void sound_dma_wait(SoundDmaWork *w)
{
    int st[4];
    int err;
    int old;

    if (system_flags & SYS_SOFT_RESET) {
        do {
            old = debug_color_set(RGBA(0x80, 0, 0, 0x80));
            DEBUG_PRINTF("Sound DMA Transfer Wait Cancel by SOFT_RESET : %s\n", w->name);
            debug_color_set(old);
        } while (0);
    } else {
        switch (w->phase) {
        case 0:
            err = sdMemBlkGetStat(w->blk, st);
            SD_CHECK("sdMemBlkGetStat", err);
            if (st[0] != 1) {
                if (st[0] != 0)
                    return;
                if (w->polls++ < 60)
                    return;
                ERROR_PRINTF("sdDMA Sound Block Trans Error !?\n");
            } else {
                if (w->polls > 0)
                    DEBUG_PRINTF("sdDMA TRANSFER_RMEAIN count : %d\n", w->polls);
                DEBUG_PRINTF("Sound DMA Transfer Finished : %s\n", w->name);
            }
            w->phase = w->phase + 1;
            return;
        case 1:
            break;
        default:
            return;
        }
    }
    if (w)
        TASK_OF(w)->flags |= TASK_F_END;
}

/*
 * Mutes the MIDI SE (laser SE for mode 4), waits two frames and for no file read, starts the DMA,
 * then sound_dma_wait. A soft reset ends it.
 */
/* steer(cast) */
/* 0x00164378 */
void sound_dma_exec(SoundDmaWork *w)
{
    int err;
    int old;

    if (system_flags & SYS_SOFT_RESET) {
        do {
            old = debug_color_set(RGBA(0x80, 0, 0, 0x80));
            DEBUG_PRINTF("Sound DMA Transfer Cancel by SOFT_RESET : %s\n", w->name);
            debug_color_set(old);
        } while (0);
        if (w)
            TASK_OF(w)->flags |= TASK_F_END;
        return;
    }
    switch (w->phase) {
    case 0:
        if (w->mode == 4)
            SoundMidiSeEnableLaser(0);
        else
            SoundMidiSeEnable(0);
        w->se_muted = 1;
        w->phase = w->phase + 1;
        break;
    case 1:
    case 2:
        w->phase = w->phase + 1;
        break;
    case 3:
        if (filer_is_reading())
            return;
        do {
            sound_sd_err = err = sdMemBlkCreate(w);
            if (err)
                ERROR_PRINTF("%s (E) : %08x\n", "sdMemBlkCreate", err);
        } while (0);
        err = ((int (*)(int))sdMemBlkSetTransferMode)(1);
        SD_CHECK("sdMemBlkSetTransferMode", err);
        err = sdMemBlkSetPrm(w->blk, w->buf, w->size, 0, 0);
        SD_CHECK("sdMemBlkSetPrm", err);
        DEBUG_PRINTF("Sound DMA Transfer Start : %s\n", w->name);
        switch (w->kind) {
        case 0:
            err = sdMltDownload(w->blk);
            SD_CHECK("sdMultiUnitDownload", err);
            break;
        case 1:
            err = sdBankDownload(w->blk, w->bank, w->mode);
            sound_sd_err = err;
            if (err) {
                ERROR_PRINTF("%s (E) : %08x\n", "sdBankDownload", err);
                if (sound_sd_err)
                    TASK_OF(w)->flags |= TASK_F_END;
            }
            break;
        }
        w->created = 1;
        TASK_OF(w)->exec = sound_dma_wait;
        w->phase = 0;
        break;
    }
}

/* Under "DMALoad"; sleeps while an older transfer runs, until woken. */
/* 0x001645d8 */
static void *sound_dma_create(void)
{
    void *parent = sound_get_dma_load();
    Task *hdr = TASK_OF(parent)->child;
    SoundDmaWork *cur = hdr ? (SoundDmaWork *)(hdr + 1) : 0;
    SoundDmaWork *w = task_create(parent, &sound_dma_desc, sizeof(SoundDmaWork));

    while (cur) {
        if (!(TASK_OF(cur)->flags & TASK_F_END)) {
            do {
                if (w)
                    TASK_OF(w)->flags |= TASK_F_SUSPENDED;
            } while (0);
            break;
        }
        hdr = TASK_OF(cur)->next;
        cur = hdr ? (SoundDmaWork *)(hdr + 1) : 0;
    }
    return w;
}

/* 0x00164668 */
static SoundDmaWork *sound_dma_create_multi_unit(int buf, int size)
{
    SoundDmaWork *w = sound_dma_create();

    w->buf = buf;
    w->size = size;
    w->name = "Multi Unit";
    w->kind = 0;
    return w;
}

/* name: for the log. */
/* 0x001646b8 */
SoundDmaWork *sound_dma_create_bank(char *name, int buf, int size, int bank, int mode)
{
    SoundDmaWork *w = sound_dma_create();

    w->kind = 1;
    w->buf = buf;
    w->size = size;
    w->bank = bank;
    w->mode = mode;
    w->name = name;
    return w;
}

/* Switches the effect program off. */
/* 0x00164730 */
static void sound_bank_clear(int bank)
{
    ulsnd_set_fx_prg(0, 0);
}

/* 0x00164740 */
static void sound_clear_fx_program(void)
{
    int err;

    do {
        err = sdSndClearFxPrg();
        sound_sd_err = err;
        if (err)
            ERROR_PRINTF("%s (E) : %08x\n", "sdSndClearFxPrg", err);
    } while (0);
}

/* Log name = file name; starts idle. */
/* 0x00164788 */
static void sound_mpb_load_init(SoundMpbLoadWork *w)
{
    w->self = w->name;
    w->busy = 0;
}

/* Switches the effect program off. */
/* 0x00164798 */
static void sound_mpb_load_release(void)
{
    sound_bank_clear(0);
}

/*
 * Requests the file; once read queues the DMA, waits, frees the file. A soft reset before the
 * download cancels the read.
 */
/* 0x001647a0 */
static void sound_mpb_load_exec(SoundMpbLoadWork *w)
{
    int old;
    int err;
    void *req;

    if (w->state < 2) {
        if (system_flags & SYS_SOFT_RESET) {
            do {
                old = debug_color_set(RGBA(0x80, 0, 0, 0x80));
                DEBUG_PRINTF("Sound Read Cancel by SOFT_RESET : %s\n", w->self);
                debug_color_set(old);
            } while (0);
            if (w->busy)
                file_request_cancel(&w->req);
            goto fin;
        }
    }
    switch (w->state) {
    case 0:
        file_request_simple(&w->req, 0);
        w->busy = 1;
        w->state = w->state + 1;
        return;
    case 1:
        req = &w->req;
        err = file_request_status(req);
        if (err < 0) {
            do {
                if (w)
                    TASK_OF(w)->flags |= TASK_F_END;
            } while (0);
            w->busy = 0;
            return;
        }
        if (err > 0) {
            w->buf = file_load_sync(req, 0, 0);
            w->child = sound_dma_create_bank(w->self, w->buf, file_get_size(req), w->arg2, w->arg1);
            w->state = w->state + 1;
            w->busy = 0;
        }
        return;
    case 2:
        if (w->child == 0 || (TASK_OF(w->child)->flags & TASK_F_END))
            goto fin;
        return;
    default:
        return;
    }
fin:
    file_heap_free(w->buf);
    w->buf = 0;
    if (w)
        TASK_OF(w)->flags |= TASK_F_END;
}

/* Under the system task. */
/* 0x00164948 */
static SoundMpbLoadWork *sound_mpb_load_create(void *path, int mode, int bank)
{
    SoundMpbLoadWork *w = task_create(system1_work, &sound_mpb_load_desc, sizeof(SoundMpbLoadWork));

    file_desc_copy(&w->req, w->name, path);
    w->arg1 = mode;
    w->arg2 = bank;
    return w;
}

/* Background load into the 'BPMS' bank. */
/* 0x001649c0 */
int sound_mpb_load_create_bpms(char *path, int mode)
{
    return sound_mpb_load_create(path, mode, 0x42504d53);
}

/* Background load into the 'BPFS' bank. */
/* 0x001649e0 */
int sound_mpb_load_create_bpfs(int path)
{
    return sound_mpb_load_create(path, 0, 0x42504653);
}

/* 0x00164a08 */
static void sound_host_command_window_opened(int win)
{
    sound_host_command_window = win;
    menu_item_set_value(info_menu_name, str_sound_host_command, 1);
}

/* 0x00164a28 */
static void sound_host_command_window_closed(void)
{
    sound_host_command_window = 0;
    menu_item_set_value(info_menu_name, str_sound_host_command, 0);
}

/* Last 256 samples of the driver's buffered command count, oldest first. */
/* 0x00164a48 */
static void sound_host_command_draw(void *win)
{
    int cw;
    int ch;
    int ox;
    int oy;
    float z;
    float x;
    float y;
    float next_x;
    float next_y;
    int *sample;
    int left;
    int n;
    int line;
    int idx;

    gw_get_cell_size(&cw, &ch);
    gw_window_get_abs_pos(win, &ox, &oy);
    z = float_add_bits16(1, gw_window_get_scale(win));
    idx = sound_host_command_idx;
    sample = &sound_host_command_samples[idx];
    left = 256 - idx;
    line = line_set_color(COL_BLACK);
    oy = (int)((float)oy + 2.0f);
    ox = (int)((float)ox + 2.0f);
    x = (float)ox;
    y = (float)oy + 100.0f - (float)*sample;
    n = 255;
    do {
        left--;
        sample++;
        if (left <= 0) {
            left = 256;
            sample = sound_host_command_samples;
        }
        next_y = (float)oy + 100.0f - (float)*sample;
        next_x = x + 1.0f;
        line = line_add_seg(line, x, y, z, next_x, next_y, z);
        x = next_x;
        y = next_y;
        n--;
    } while (n != 0);
    line_flush(line);
}

/* Opens at 260x104. */
/* 0x00164be8 */
static void sound_host_command_toggle(void)
{
    int cw;
    int ch;
    void *w;

    if (sound_host_command_window != 0) {
        gw_window_close(sound_host_command_window);
        return;
    }
    gw_get_cell_size(&cw, &ch);
    w = gw_window_open("Sound HostCommand", 95, screen_layout.x + 4.0f, screen_layout.y + 4.0f, 260, 104, 0);
    if (w != 0) {
        sound_host_command_window_opened(w);
        gw_window_set_handlers(w, 0, sound_host_command_draw, sound_host_command_window_closed);
    }
}

/* 0x00164ca8 */
static void sound_slot_window_opened(int win)
{
    sound_slot_window = win;
    menu_item_set_value(info_menu_name, str_sound_slot, 1);
}

/* 0x00164cc8 */
static void sound_slot_window_closed(void)
{
    sound_slot_window = 0;
    menu_item_set_value(info_menu_name, str_sound_slot, 0);
}

/* SPU voice end (endx) and key-on (kon) masks of both cores, 24 voices each, as '-' and '0'. */
/* 0x00164ce8 */
static void sound_slot_draw(void *w)
{
    int cw;
    int ch;
    int k = 30;
    int y;

    gw_get_cell_size(&cw, &ch);
    gw_win_printf(w, 4, 0, "endx %s", bits_to_string(ulsnd_spu_status[171], 24, str_sound_slot_off, str_sound_slot_on));
    gw_win_printf(w, cw * k + 4, 0, "%s", bits_to_string(ulsnd_spu_status[172], 24, str_sound_slot_off, str_sound_slot_on));
    y = ch;
    gw_win_printf(w, 4, y, "kon  %s", bits_to_string(ulsnd_spu_status[173], 24, str_sound_slot_on, str_sound_slot_off));
    gw_win_printf(w, cw * k + 4, y, "%s", bits_to_string(ulsnd_spu_status[174], 24, str_sound_slot_on, str_sound_slot_off));
}

/* Opens sized to two rows of 56 cells. */
/* 0x00164e58 */
static void sound_slot_toggle(void)
{
    int cw;
    int ch;
    void *w;

    if (sound_slot_window != 0) {
        gw_window_close(sound_slot_window);
        return;
    }
    gw_get_cell_size(&cw, &ch);
    w = gw_window_open("Sound Slot", 95, screen_layout.x + 4.0f, screen_layout.y + 4.0f, cw * 56, ch * 2 + 4, 0);
    if (w != 0) {
        sound_slot_window_opened(w);
        gw_window_set_handlers(w, 0, sound_slot_draw, sound_slot_window_closed);
    }
}

/* 0x00164f30 */
static void sound_debug_menu_init(void)
{
    menu_item_add(info_menu_name, "Sound HostCommand", sound_host_command_toggle, 0, 0);
    menu_item_add(info_menu_name, "Sound Slot", sound_slot_toggle, 0, 0);
}

/* 256-sample ring for "Sound HostCommand"; called from the main loop and sound_end_exec. */
/* 0x00164f90 */
void sound_host_command_sample(void)
{
    int val = ulsnd_buffer_commands();

    sound_host_command_samples[sound_host_command_idx] = val;
    sound_host_command_idx++;
    if (sound_host_command_idx > 255)
        sound_host_command_idx = 0;
}
