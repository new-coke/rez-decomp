/*
 * File loading: a 16-slot background read queue served one transfer at a time, blocking loads,
 * the file heap, PACK archive lookups, the "NowLoading" line and the FileReq debug window.
 */

#include "types_game.h"
#include "color.h"
#include "debug.h"
#include "libc.h"
#include "adxf.h"
#include "gw_draw.h"
#include "gw_menu.h"
#include "gw_system.h"
#include "gw_window.h"
#include "kernel_syscalls.h"
#include "file.h"
#include "debugmenu.h"
#include "dir_read.h"
#include "heap.h"
#include "perform.h"

extern int filer_heap_mem;
extern int filer_heap;
extern char str_filer_heap_file[];     /* "HeapFile" */
extern char str_filer_file[];          /* "File" */

extern FileSys filer_queue;
extern int filer_reading;
extern int filer_pause;

extern void soft_reset(char *name);

extern int filer_flags;
extern char str_filer_pause_enable[];  /* "FilerServer Pause Enable\n" */
extern char str_filer_pause_disable[]; /* "FilerServer Pause Disable\n" */

extern char str_file_volume[];         /* "VOL:%s" */
extern int filer_not_found_cnt;
extern char str_filer_req_pause[];     /* "File Req  Pause : %s\n" */
extern char str_file_req_not_found[];  /* "[WORNING]File Req File Not Found : %s\n" */
extern char str_filer_read_cancel[];   /* "File Req  Read Cancel? : %s\n" */
extern char str_filer_read_already_get[]; /* "File Req  Read Already Get : %s\n" */
extern char str_filer_read_complete[]; /* "File Req  Read Complete : %s\n" */
extern char str_filer_read_retry[];    /* "File Req  Read Retry : %s (%2d/%2d)\n" */
extern char str_filer_retry_over[];

extern int fptoui(float f);
extern char str_file_req_over[];
extern char str_file_req_name_too_long[]; /* "[ERROR]File Req FileName is too long %-15s\n" */
extern char str_file_req_log[];        /* "File Req  %08x %-15s %8d\n" */
extern char str_file_req_malloc_miss[];

extern char str_file_not_found_error[]; /* "[ERROR] File Not Found : %s\n" */
extern char str_file_not_found[];      /* "File Not Found!!! : %s\n" */
extern char str_file_malloc_error[];   /* "FileMalloc Error!!!\nSize %d\n" */
extern char str_file_read_log[];       /* "File Read %08x %-15s %8d %5.3fs\n" */

extern void nj_copy256(int dst, int src, int n);
extern char str_file_req_cancel[];     /* "File Req Cancel! %s\n" */
extern int pack_file_magic;            /* "PACK" */
extern char str_pack_file_get_num_error[]; /* "PackFileGetNum() : Not PackFile!\n" */

extern char str_pack_file_get_error[]; /* "PackFileGet() : Not PackFile!\n" */

extern char str_pack_file_get_id_not_found[]; /* "PackFileGetId() : NotFound!\n" */

extern char str_pack_file_get_not_found[]; /* "PackFileGet() : NotFound!\n" */

extern int heap_free_size_of(int h);

extern int heap_free_blocks_of(int h);

extern int debug_disp_level;
extern char str_file_now_loading[];    /* "NowLoading" */

extern char *file_req_state_names[];
extern char str_file_req_no_request[]; /* "NOREQUEST" */
extern char str_file_req_row[];        /* "%s %-12s %08x %3d %2d" */

extern int file_req_window;
extern char str_file_req[];            /* "FileReq" */

extern struct {
    int pad[7];
    float x;                    /* 0x1c */
    float y;                    /* 0x20 */
} screen_layout;

static int file_heap_alloc_back(int size);
static void file_now_loading_print(char *req);
static void file_req_menu_register(void);

/* filer_heap: 9.5 MB from the back of the game heap. */
/* steer(cast) */
/* 0x00117070 */
void filer_boot(void)
{
    int mem;

    filer_reset_flags();
    mem = heap_block_tag(game_malloc_back(0x980000), str_filer_heap_file, -1);
    filer_heap_mem = mem;
    filer_heap = ((int (*)(int, int, char *))heap_create)(mem, 0x980000, str_filer_file);
}

/* Each slot's name points at its own buffer. Adds FileReq to the debug menu. */
/* 0x001170d0 */
void filer_init(void)
{
    FileEnt *ent;
    int i;

    soft_reset(str_filer_file);
    ent = filer_queue.ent;
    filer_queue.cur = 0;
    filer_queue.prio_next = 0;
    i = 16;
    do {
        ent->buf[0] = 0;
        ent->name = ent->buf;
        i--;
        ent->unk_408 = 0;
        ent->dest = 0;
        ent->size = 0;
        ent->elapsed = 0;
        ent->state = 0;
        ent->retry = 0;
        ent->async = 0;
        ent->own_buf = 0;
        ent++;
    } while (i != 0);
    filer_reading = 0;
    filer_pause = 0;
    file_req_menu_register();
}

/* 0x00117150 */
int filer_is_ready(void)
{
    return 1;
}

/* Bit 0: file_load_sync logs a missing file, no debug_error; bit 2: same for a failed alloc. */
/* 0x00117158 */
void filer_set_flags(int bits)
{
    filer_flags |= bits;
}

/* 0x00117170 */
static void filer_clear_flags(int bits)
{
    filer_flags &= ~bits;
}

/* 0x00117188 */
void filer_reset_flags(void)
{
    filer_flags = 0;
}

/* 0x00117198 */
int filer_is_reading(void)
{
    return filer_reading;
}

/* Paused: no new reads start. */
/* 0x001171a8 */
static void filer_set_pause(int pause)
{
    filer_pause = pause;
    if (pause)
        DEBUG_PRINTF(str_filer_pause_enable);
    else
        DEBUG_PRINTF(str_filer_pause_disable);
}

/* req[0]: directory, req[1]: file name. NULL if missing. */
/* 0x001171d0 */
static void *file_open(int *req)
{
    char buf[1024];
    char *name = rofs_dir_entry(req[0]);

    sprintf(buf, str_file_volume, req[1]);
    return ADXF_Open(buf, name);
}

/* 0x00117220 */
static void file_close(void *fh)
{
    adxf_close_handle(fh);
}

/* 0x00117228 */
static int filer_req_group(void)
{
    return 0;
}

/* 0x00117230 */
static int filer_handle_group(void)
{
    return 0;
}

/* Lowest priority number waiting, else next slot. Empty queue: slot 0, priority counter reset. */
/* 0x00117238 */
static void filer_pick_next(void)
{
    FileSys *fs = &filer_queue;
    FileEnt *ent = fs->ent;
    int prio = 0x10000;
    int n = 0;
    int best = -1;
    int i;

    for (i = 0; i < 16; i++, ent++) {
        if (ent->state != 0)
            n++;
        if (ent->state == 1) {
            if (ent->prio < prio) {
                prio = ent->prio;
                best = i;
            }
        }
    }
    if (n == 0) {
        fs->prio_next = 0;
        fs->cur = 0;
        return;
    }
    if (best == -1)
        fs->cur = (fs->cur + 1) & 15;
    else
        fs->cur = best & 15;
}

/*
 * Slot states: 0 free, 1 waiting, 2 reading, 3 done, 4 collected, 5 failed. Starts waiting slots
 * when not paused; reissues on drive request; up to 10 retries; frees collected slots each step.
 */
/* 0x001172c8 */
void filer_server_step(void)
{
    FileSys *fs = &filer_queue;
    FileEnt *ent = &fs->ent[fs->cur];
    int st = fs->busy;
    int stat;
    int old_col;
    int i;
    unsigned int start;

    switch (st) {
    default:
        break;
    case 0:
        if (ent->state == 1) {
            if (filer_pause != 0) {
                old_col = debug_color_set(RGBA(0x80, 0, 0, 0x80));
                DEBUG_PRINTF(str_filer_req_pause, ent->name);
                debug_color_set(old_col);
            } else {
                fs->handle = file_open(ent);
                if (fs->handle != 0) {
                    ent->state = 2;
                    start = perform_timer_read();
                    ent->elapsed = (float)start;
                    if (ent->async != 0)
                        file_handle_seek(fs->handle, ent->async, 0);
                    ADXF_ReadNw(fs->handle, ent->dest, ent->size);
                    filer_reading = 1;
                    fs->busy = 1;
                } else {
                    ERROR_PRINTF(str_file_req_not_found, ent->name);
                    ent->state = 0;
                    filer_not_found_cnt++;
                    file_close(fs->handle);
                }
            }
        } else {
            filer_pick_next();
        }
        break;
    case 1:
        stat = ADXF_GetStat(fs->handle);
        switch (stat) {
        case 2:
            break;
        case 1:
            ADXF_ReadNw(fs->handle, ent->dest, ent->size);
            break;
        case 3:
            ent->elapsed = perform_elapsed(fptoui(ent->elapsed)) * 1e-6f;
            ent->state = stat;
            break;
        case 4:
            ent->state = 5;
            break;
        }
        switch ((unsigned int)ent->state) {
        case 0:
            file_close(fs->handle);
            filer_reading = 0;
            DEBUG_PRINTF(str_filer_read_cancel, ent->name);
            filer_pick_next();
            fs->busy = 0;
            break;
        case 4:
            DEBUG_PRINTF(str_filer_read_already_get, ent->name);
        case 3:
            file_close(fs->handle);
            filer_reading = 0;
            DEBUG_PRINTF(str_filer_read_complete, ent->name);
            filer_pick_next();
            fs->busy = 0;
            break;
        case 5:
            file_close(fs->handle);
            filer_reading = 0;
            ent->retry++;
            if (ent->retry < 11) {
                ent->state = 1;
                DEBUG_PRINTF(str_filer_read_retry, ent->name, ent->retry, 10);
            } else {
                ent->state = 0;
                ERROR_PRINTF(str_filer_retry_over, ent->name);
            }
            fs->busy = 0;
            break;
        case 1:
            fs->busy = 0;
            break;
        case 2:
            break;
        }
        file_now_loading_print(ent);
        break;
    }
    ent = fs->ent;
    for (i = 16; i != 0; i--, ent++) {
        if (ent->state == 4)
            ent->state = 0;
    }
}

/*
 * prio 0: next in queue order. dest: sectors to read (default whole file); *pout: file sectors.
 * size: buffer; none -> file heap (back when sync). async: start position. 0 if full or no memory.
 */
/* 0x001175e8 */
static int file_request(int *req, int sync, int async, int dest, int size, int *pout, int prio)
{
    FileSys *fs = &filer_queue;
    char *name = (char *)req[1];
    FileEnt *ent = fs->ent;
    int i = 16;
    int fh;
    int irq;

    do {
        if (ent->state == 0)
            break;
        i--;
        ent++;
    } while (i != 0);
    if (i == 0) {
        ERROR_PRINTF(str_file_req_over, name);
        return 0;
    }
    if (prio == 0) {
        ent->prio = fs->prio_next;
        fs->prio_next++;
    } else {
        ent->prio = prio;
    }
    fh = file_open(req);
    if (fh == 0) {
        ERROR_PRINTF(str_file_req_not_found, name);
    } else {
        ent->unk_408 = 0;
        if (dest != 0) {
            ent->dest = dest;
            if (pout != 0)
                *pout = ADXF_GetFsizeSct(fh);
        } else {
            ent->dest = ADXF_GetFsizeSct(fh);
            if (pout != 0)
                *pout = ent->dest;
        }
        if (async != 0)
            ent->async = async;
        else
            ent->async = 0;
        if (size == 0) {
            ent->own_buf = 1;
            if (sync)
                ent->size = heap_block_tag(file_heap_alloc_back(ent->dest << 11), name, -1);
            else
                ent->size = heap_block_tag(file_heap_alloc(ent->dest << 11), name, -1);
        } else {
            ent->size = size;
            ent->own_buf = 0;
        }
        if (ent->size != 0) {
            irq = ((int (*)(unsigned int))debug_color_set)(0x80800000);
            if (strlen(name) >= 1024)
                ERROR_PRINTF(str_file_req_name_too_long, name);
            ent->dir = req[0];
            copy_line(ent->buf, name, 1024);
            ent->state = 1;
            ent->retry = 0;
            DEBUG_PRINTF(str_file_req_log, ent->size, name, ent->dest << 11);
            debug_color_set(irq);
        } else {
            ERROR_PRINTF(str_file_req_malloc_miss, name, ent->dest << 11);
        }
    }
    file_close(fh);
    if (fs->busy == 0)
        fs->cur = 0;
    return ent->size;
}

/* New buffer, queue order. */
/* 0x00117838 */
int file_request_simple(int req, int sync)
{
    return file_request(req, sync, 0, 0, 0, 0, 0);
}

/* Priority -1: ahead of all. Returns dest. */
/* 0x00117868 */
static int file_request_sync(int req, int async, int dest, int size, int pout)
{
    file_request(req, 0, async, dest, size, pout, -1);
    return dest;
}

/* Same directory and name. Always NULL when flag < 0. */
/* 0x001178a8 */
static FileEnt *file_request_find(int *req, int flag)
{
    FileSys *fs = &filer_queue;
    FileEnt *ent = fs->ent;
    int i = 16;

    if (flag >= 0) {
        do {
            if (ent->state != 0 && ent->dir == req[0] && strcmp(ent->name, (char *)req[1]) == 0)
                return ent;
            i--;
            ent++;
        } while (i != 0);
    }
    return 0;
}

/*
 * Waiting/failed: cancelled, buffer freed. Done: bytes, buffer, time out, slot collected, 1.
 * Reading: spins on the state without running the file server; ends only if something else does.
 */
/* 0x00117938 */
static int file_request_poll(int *req, int flag, int *out_pos, int *out_size, float *out_time, FileEnt **pe)
{
    FileEnt *ent = file_request_find(req, flag);
    int old_col;

    *pe = ent;
    if (ent == 0)
        return 0;
    switch ((unsigned int)ent->state) {
    case 0:
        break;
    case 1:
    case 5:
        old_col = debug_color_set(RGBA(0, 0, 0x80, 0x80));
        DEBUG_PRINTF("File Req  Canceled %-15s\n", ent->name);
        debug_color_set(old_col);
        if (ent->own_buf != 0) {
            file_heap_free(ent->size);
            ent->own_buf = 0;
            ent->size = 0;
        }
        ent->state = 0;
        break;
    case 2:
        while (ent->state != 3)
            ;
    case 3:
        debug_color_set(RGBA(0, 0x28, 0, 0x80));
        *(void **)out_pos = (void *)(ent->dest << 11);
        *(char **)out_size = *(char **)&ent->size;
        *out_time = ent->elapsed;
        ent->state = 4;
        return 1;
    default:
        ERROR_PRINTF("File Req Check %-15s %d\n", ent->name, ent->state);
        break;
    }
    return 0;
}

/*
 * Takes a finished queued read if any (copied into dest if given). Else reads directly into dest or
 * a file heap buffer (back when sync), retrying. 0 if missing or no memory.
 */
/* 0x00117a80 */
int file_load_sync(int *req, int dest, int sync)
{
    int len;
    int buf;
    float time;
    FileEnt *ent;
    int start;
    int fh;
    char *name;
    int sectors;

    ent = 0;
    start = perform_timer_read();
    fh = file_open(req);
    name = (char *)req[1];
    if (fh == 0) {
        if (filer_flags & 1)
            ERROR_PRINTF(str_file_not_found_error, name);
        else
            debug_error(str_file_not_found, name);
        goto fail;
    }
    if (file_request_poll(req, ((int (*)(int))filer_handle_group)(fh), &len, &buf, &time, &ent)) { /* steer(cast) */
        if (dest != 0) {
            nj_copy256(dest, buf, (len + 31) >> 5);
            FlushCache(0);
            FlushCache(2);
            if (ent->own_buf != 0)
                file_heap_free(buf);
            buf = dest;
        }
    } else {
        if (ent != 0) {
            dest = ent->size;
            if (ent->async != 0)
                file_handle_seek(fh, ent->async, 0);
            sectors = ent->dest;
            len = sectors << 11;
        } else {
            sectors = ADXF_GetFsizeSct(fh);
            len = sectors << 11;
        }
        if (dest != 0) {
            buf = dest;
        } else {
            if (sync != 0)
                buf = heap_block_tag(file_heap_alloc_back(len), name, -1);
            else
                buf = heap_block_tag(file_heap_alloc(len), name, -1);
            if (buf == 0) {
                if (filer_flags & 4)
                    ERROR_PRINTF(str_file_malloc_error, len);
                else
                    debug_error(str_file_malloc_error, len);
                goto fail;
            }
        }
        ADXF_ReadNw(fh, sectors, buf);
        do {
            do {
                sync = 0;
                switch (ADXF_GetStat(fh)) {
                case 1:
                    ADXF_ReadNw(fh, sectors, buf);
                    break;
                case 2:
                    break;
                case 3:
                    sync = 1;
                    break;
                case 4:
                    ADXF_ReadNw(fh, sectors, buf);
                    break;
                }
            } while (0);
        } while (sync == 0);
        if (dest != 0) {
            FlushCache(0);
            FlushCache(2);
        }
        time = perform_elapsed(start) * 1.0e-6f;
    }
    goto ok;
fail:
    file_close(fh);
    return 0;
ok:
    file_close(fh);
    DEBUG_PRINTF(str_file_read_log, buf, name, len, (double)time);
    debug_color_set(gw_get_default_color());
    return buf;
}

/* -1 not queued, 0 waiting/reading/failed, 1 done. */
/* 0x00117db0 */
int file_request_status(void *req)
{
    FileEnt *ent = file_request_find(req, filer_req_group());
    int st;

    if (ent != 0) {
        st = ent->state;
        if (st == 1 || st == 2 || st == 5)
            return 0;
        return 1;
    }
    return -1;
}

/* 1 when nothing is waiting, reading or failed. */
/* 0x00117e10 */
int file_request_all_done(void)
{
    FileSys *fs = &filer_queue;
    FileEnt *ent = fs->ent;
    int i;

    for (i = 0; i < 16; i++, ent++) {
        if ((unsigned int)(ent->state - 1) < 2 || ent->state == 5)
            return 0;
    }
    return 1;
}

/* A running transfer is stepped to its end first. Frees a queue-allocated buffer, then the slot. */
/* 0x00117e60 */
void file_request_cancel(int *req)
{
    char *name = (char *)req[1];
    FileEnt *ent = file_request_find(req, filer_req_group());
    int old_col;

    if (ent != 0) {
        if (ent->state == 2) {
            do {
                if (ent->state == 3 || ent->state == 5)
                    break;
                filer_server_step();
            } while (1);
        }
        old_col = debug_color_set(RGBA(0x80, 0, 0, 0x80));
        DEBUG_PRINTF(str_file_req_cancel, name);
        debug_color_set(old_col);
        if (ent->own_buf != 0)
            file_heap_free(ent->size);
        ent->state = 0;
    }
}

/* Waiting, running, uncollected or failed reads, as file_request_cancel. */
/* 0x00117f28 */
void filer_cancel_all(void)
{
    FileSys *fs = &filer_queue;
    FileEnt *ent = fs->ent;
    FileEnt *slot;
    FileEnt *next;
    int i;
    int cancel;
    int old_col;

    i = 0;
    do {
        slot = ent;
        cancel = 0;
        do {
            switch ((unsigned int)slot->state) {
            case 2:
                do {
                    if (slot->state == 3 || slot->state == 5)
                        break;
                    filer_server_step();
                } while (1);
                cancel = 1;
                break;
            case 3:
                next = slot + 1;
                cancel = 1;
                break;
            case 1:
            case 5:
                cancel = 1;
                break;
            }
        } while (0);
        if (cancel) {
            old_col = debug_color_set(RGBA(0x80, 0, 0, 0x80));
            DEBUG_PRINTF(str_file_req_cancel, slot->name);
            debug_color_set(old_col);
            if (slot->own_buf != 0)
                file_heap_free(slot->size);
            slot->state = 0;
        }
        i++;
        next = slot + 1;
        ent = next;
    } while (i < 16);
}

/* 2048-byte sectors; -1 if missing. */
/* 0x00118088 */
static int file_get_sectors(void *req)
{
    void *fh = file_open(req);
    int sectors;

    if (fh == 0)
        return -1;
    sectors = ADXF_GetFsizeSct(fh);
    file_close(fh);
    return sectors;
}

/* Rounded up to whole sectors. */
/* 0x001180d8 */
int file_get_size(void *req)
{
    return file_get_sectors(req) << 11;
}

/* Direct reading; see file_open. */
/* 0x001180f8 */
int file_handle_open(void *req)
{
    return file_open(req);
}

/* 0x00118118 */
void file_handle_close(void *fh)
{
    file_close(fh);
}

/* 0x00118120 */
static int file_handle_sectors(void *fh)
{
    return ADXF_GetFsizeSct(fh);
}

/* Rounded up to whole sectors. */
/* 0x00118140 */
int file_handle_size(void *fh)
{
    return file_handle_sectors(fh) << 11;
}

/* 0x00118160 */
int file_handle_read(void *fh, int nsct, void *buf)
{
    return ADXF_ReadNw(fh, nsct, buf);
}

/* ADXF status 1 or 3. Status is queried without the handle. */
/* 0x00118180 */
int file_handle_is_idle(void *fh)
{
    switch (ADXF_GetStat(fh)) {
    case 1:
        return 1;
    case 2:
        return 0;
    case 3:
        return 1;
    }
    return 0;
}

/* Always 1. */
/* 0x001181e0 */
int file_handle_seek(int fh, int pos, int type)
{
    ADXF_Seek(fh, pos, type);
    return 1;
}

/*
 * 0 without the "PACK" magic. Layout: magic, format byte, entry count. Format 0: 8-byte header,
 * {id, size, data} entries padded to 4. Newer: 32-byte header, 32-byte entry headers, padded to 32.
 */
/* 0x00118200 */
int PackFileGetNum(char *pack)
{
    if (*(int *)pack != pack_file_magic) {
        ERROR_PRINTF(str_pack_file_get_num_error);
        return 0;
    }
    return pack[5];
}

/* Format 0. */
/* 0x00118240 */
static int PackFileCountIdOld(char *pack, int id)
{
    int n = pack[5];
    int count = 0;
    int *ent = (int *)(pack + 8);

    while (n-- > 0) {
        if (ent[0] == id)
            count++;
        ent = (int *)(((int)ent + ent[1] + 11) & ~3u);
    }
    return count;
}

/* 0 if not a pack. */
/* 0x001182a0 */
int PackFileCountId(char *pack, int id)
{
    int *ent = (int *)(pack + 32);
    int count;
    int n;

    if (*(int *)pack != pack_file_magic) {
        ERROR_PRINTF(str_pack_file_get_error);
        return 0;
    }
    if (pack[4] == 0) {
        count = PackFileCountIdOld(pack, id);
    } else {
        count = 0;
        n = pack[5];
        while (n-- > 0) {
            if (ent[0] == id)
                count++;
            ent = (int *)(((int)ent + ent[1] + 63) & ~31u);
        }
    }
    return count;
}

/* Format 0. 0 if no such entry. */
/* 0x00118348 */
static int PackFileGetIdOld(char *pack, int idx)
{
    int n = pack[5];
    int *ent = (int *)(pack + 8);

    while (n-- > 0) {
        if (idx-- <= 0)
            return ent[0];
        ent = (int *)(((int)ent + ent[1] + 11) & ~3u);
    }
    ERROR_PRINTF(str_pack_file_get_id_not_found);
    return 0;
}

/* 0 if not a pack or no such entry. */
/* 0x001183c0 */
static int PackFileGetId(char *pack, int idx)
{
    int *ent = (int *)(pack + 32);
    int n;

    if (*(int *)pack != pack_file_magic) {
        ERROR_PRINTF(str_pack_file_get_error);
        return 0;
    }
    if (pack[4] == 0)
        return PackFileGetIdOld(pack, idx);
    n = pack[5];
    while (n-- > 0) {
        if (idx-- <= 0)
            return ent[0];
        ent = (int *)(((int)ent + ent[1] + 63) & ~31u);
    }
    ERROR_PRINTF(str_pack_file_get_id_not_found);
    return 0;
}

/* Format 0. size may be NULL. 0 if no such entry. */
/* 0x00118468 */
static char *PackFileGetOld(char *pack, int idx, int *size)
{
    int n = pack[5];
    int *ent = (int *)(pack + 8);

    while (n-- > 0) {
        if (idx-- <= 0) {
            if (size)
                *size = ent[1];
            return (char *)ent + 8;
        }
        ent = (int *)(((int)ent + ent[1] + 11) & ~3u);
    }
    ERROR_PRINTF(str_pack_file_get_not_found);
    return 0;
}

/* size may be NULL. 0 if not a pack or no such entry. */
/* 0x001184f0 */
char *PackFileGet(char *pack, int idx, int *size)
{
    int *ent = (int *)(pack + 32);
    int n;

    if (*(int *)pack != pack_file_magic) {
        ERROR_PRINTF(str_pack_file_get_error);
        return 0;
    }
    if (pack[4] == 0)
        return PackFileGetOld(pack, idx, size);
    n = pack[5];
    while (n-- > 0) {
        if (idx-- <= 0) {
            if (size)
                *size = ent[1];
            return (char *)ent + 32;
        }
        ent = (int *)(((int)ent + ent[1] + 63) & ~31u);
    }
    ERROR_PRINTF(str_pack_file_get_not_found);
    return 0;
}

/* Format 0. idx-th entry with id; size may be NULL. 0 if none. */
/* 0x001185a8 */
static char *PackFileGetByIdOld(char *pack, int id, int idx, int *size)
{
    int n = pack[5];
    int *ent = (int *)(pack + 8);

    while (n-- > 0) {
        if (ent[0] == id) {
            if (idx-- <= 0) {
                if (size)
                    *size = ent[1];
                return (char *)ent + 8;
            }
        }
        ent = (int *)(((int)ent + ent[1] + 11) & ~3u);
    }
    ERROR_PRINTF(str_pack_file_get_not_found);
    return 0;
}

/* idx-th entry with id; size may be NULL. 0 if not a pack or none. */
/* 0x00118638 */
char *PackFileGetById(char *pack, int id, int idx, int *size)
{
    int *ent = (int *)(pack + 32);
    int n;

    if (*(int *)pack != pack_file_magic) {
        ERROR_PRINTF(str_pack_file_get_error);
        return 0;
    }
    if (pack[4] == 0)
        return PackFileGetByIdOld(pack, id, idx, size);
    n = pack[5];
    while (n-- > 0) {
        if (ent[0] == id) {
            if (idx-- <= 0) {
                if (size)
                    *size = ent[1];
                return (char *)ent + 32;
            }
        }
        ent = (int *)(((int)ent + ent[1] + 63) & ~31u);
    }
    ERROR_PRINTF(str_pack_file_get_not_found);
    return 0;
}

/* Directory to *dst, file name into the name buffer. */
/* 0x00118700 */
void file_desc_copy(int *dst, int name, int *src)
{
    *dst = src[0];
    strcpy(name, src[1]);
}

/* List: {directory, name} pairs ended by a NULL name; NULL list has none. */
/* 0x00118718 */
int file_list_count(int *p)
{
    int n;

    if (p == 0)
        return 0;
    n = 0;
    while (p[1] != 0) {
        p += 2;
        n++;
    }
    return n;
}

/* 0 past the end. */
/* 0x00118758 */
static int file_list_get_name(int *list, int idx)
{
    if (idx >= file_list_count(list))
        return 0;
    return list[idx * 2 + 1];
}

/* 0x001187a8 */
void file_list_request(int *list)
{
    if (list != 0) {
        if (list[1] != 0) {
            do {
                file_request_simple(list, 0);
                list += 2;
            } while (list[1] != 0);
        }
    }
}

/* 1 for a NULL list. */
/* 0x001187f8 */
int file_list_is_loaded(int *list)
{
    if (list == 0)
        return 1;
    while (list[1] != 0) {
        if (file_request_status(list) != 1)
            return 0;
        list += 2;
    }
    return 1;
}

/* Front of the file heap. */
/* 0x00118868 */
int file_heap_alloc(int size)
{
    return heap_alloc_front(filer_heap, size, 0);
}

/* Back of the file heap. */
/* 0x00118890 */
static int file_heap_alloc_back(int size)
{
    return heap_alloc_back(filer_heap, size, 0);
}

/* 0x001188b8 */
void file_heap_free(void *mem)
{
    heap_free(mem, 0);
}

/* 0x001188c0 */
static int file_heap_free_size(void)
{
    return heap_free_size_of(filer_heap);
}

/* 0x001188e8 */
static int file_heap_free_blocks(void)
{
    return heap_free_blocks_of(filer_heap);
}

/* 0x00118910 */
static int file_heap_alloc_count(void)
{
    return heap_alloc_count_of(filer_heap);
}

/* Bottom right of the debug text screen, debug display level 0 only. */
/* 0x00118938 */
static void file_now_loading_print(char *req)
{
    char *name;

    if (debug_disp_level == 0) {
        name = req + 8;
        debug_set_origin(-10, -2);
        debug_puts(str_file_now_loading);
        debug_set_origin(-strlen(name), -1);
        debug_puts(name);
    }
}

/* Per slot: state, name, buffer, sectors, priority; "NOREQUEST" if free. */
/* 0x001189a8 */
static void file_req_window_draw(void *w)
{
    int cw;
    int ch;
    int y;
    int i;
    int j;
    FileEnt *ent;
    FileSys *fs;

    gw_get_cell_size(&cw, &ch);
    fs = &filer_queue;
    y = 0;
    j = 0;
    for (i = 16; i != 0; i--) {
        ent = &fs->ent[j];
        if (ent->state == 0) {
            gw_win_printf(w, 4, y, str_file_req_no_request);
        } else {

            char *name = ent->name;
            int size = ent->size;
            int dest = ent->dest;

            gw_win_printf(w, 4, y, str_file_req_row, file_req_state_names[ent->state], name, size, dest, ent->prio);
        }
        j++;
        y += ch;
    }
}

/* 0x00118a98 */
static void file_req_window_close(void)
{
    file_req_window = 0;
    menu_item_set_value(info_menu_name, str_file_req, 0);
}

/* Opens top right, 35 x 17 cells; menu tick follows the window. */
/* 0x00118ab8 */
static void file_req_window_toggle(int item)
{
    int cw;
    int ch;
    int win;

    if (file_req_window != 0) {
        gw_window_close(file_req_window);
        gw_item_set_value(item, 0);
        return;
    }
    gw_get_cell_size(&cw, &ch);
    win = gw_window_open(str_file_req, 95, (int)(screen_layout.x - (float)(cw * 35) - 4.0f),
                     (int)(screen_layout.y + 34.0f), cw * 35, ch * 17, 0);
    file_req_window = win;
    if (win != 0) {
        gw_window_set_handlers(win, 0, file_req_window_draw, file_req_window_close);
        gw_item_set_value(item, 1);
    }
}

/* 0x00118bb8 */
static void file_req_menu_register(void)
{
    menu_item_add(info_menu_name, str_file_req, file_req_window_toggle, 0, 0);
}
