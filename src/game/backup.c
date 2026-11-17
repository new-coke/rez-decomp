/*
 * Memory-card backup: the file request block the menus fill in, and the Save, Load, Format and
 * card-check tasks that run it.
 */

#include "types_game.h"
#include "task.h"
#include "debug.h"
#include "libc.h"
#include "gw_draw.h"
#include "gw_system.h"
#include "gw_window.h"
#include "libulmc.h"
#include "nj_system.h"
#include "backup.h"
#include "fade.h"
#include "heap.h"
#include "node_draw_state.h"
#include "padslot.h"
#include "system.h"
#include "warning.h"
#include "color.h"

typedef struct SaveHdr {
    char title[18];             /* 0x00 */
    char sub[34];               /* 0x12 */
    char name[16];              /* 0x34 */
    int icon_palette;                     /* 0x44 */
    int icon_image;                     /* 0x48 */
    unsigned short block_kind;          /* 0x4c */
    unsigned short icon_speed;          /* 0x4e */
    int z;                      /* 0x50 */
    short zs;                   /* 0x54 */
    void *data;                 /* 0x58 */
    void *arg;                  /* 0x5c */
} SaveHdr;

typedef struct SaveTbl {
    int icon_palette;
    int icon_image;
    int block_kind;
    int icon_speed;
} SaveTbl;

#ifdef VERSION_US

/* Source line recorded in an allocation tag. */
#define SRC_LINE(early, us) (us)

/* Card directory: product code + "REZ". */
#define BACKUP_GAME_ID "BASLUS-20344REZ"

#else

#define SRC_LINE(early, us) (early)

#define BACKUP_GAME_ID "BASLPM-62101REZ"

#endif /* !VERSION_US */

typedef struct McInfo {
    char pad_00[88];
    int addr;
    int size;
} McInfo;

typedef struct McTime {
    unsigned short year;
    unsigned char month, day, hour, min, sec, dow;
} McTime;

#ifndef VERSION_US

typedef struct DispWork {
    int mode;
    int frame;
} DispWork;

#ifndef VERSION_US
extern void *backup_disp_create(int mode);
#endif

#endif /* !VERSION_US */

extern char *backup_status_names[];
extern char *backup_cmd_names[];
extern int backup_window;
extern float screen_layout[];
extern char str_backup_init[];
extern void backup_disp_kill(void);
extern char *backup_save_titles[][5];
extern const SaveTbl backup_save_table[];
extern int config_language;
extern char str_backup_sub_title_jp[];
extern char str_backup_save_name_jp[];
extern char str_backup_sub_title_en[];
extern char str_backup_save_name_en[];
extern int mc_build_save_image(void *buf, SaveHdr *hdr);
extern int mc_unpack_save_image(McInfo *buf, char *path);

#ifndef VERSION_US

extern void *backup_disp_work;

#endif /* !VERSION_US */

extern void *scr_root;
extern TaskDesc backup_save_desc;
extern TaskDesc backup_load_desc;
extern TaskDesc backup_format_desc;
extern TaskDesc backup_checkvms_desc;
extern int backup_system_crc;
extern unsigned int system_flags;

static void backup_unmount_request(int drive);
static int backup_drive_wait(void);

/* 0x00101558 */
int filereq_is_free(void)
{
    return filereq_work.state == 0;
}

/* 0x00101568 */
void filereq_clear(void)
{
    filereq_work.state = 0;
    filereq_work.buf = 0;
}

/* 1 when free or holding a finished request's code. */
/* 0x00101580 */
int filereq_is_idle(void)
{
    unsigned int state = filereq_work.state;

    switch (state) {
    case 0:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        return 1;
    default:
        return 0;
    }
}

/* 0x001015b0 */
int filereq_get_state(void)
{
    return filereq_work.state;
}

/* 0x001015c0 */
void *filereq_get_buf(void)
{
    return filereq_work.buf;
}

/* CRC of the file saved or loaded. */
/* 0x001015d0 */
int filereq_get_result(void)
{
    return filereq_work.result;
}

/* 0x001015e0 */
void filereq_set(int cmd, int drive, const char *name, void *buf, int size, int flag)
{
    filereq_work.cmd = cmd;
    filereq_work.drive = drive;
    filereq_work.name = name;
    filereq_work.buf = buf;
    filereq_work.size = size;
    filereq_work.flag = flag;
}

/* 0x00101608 */
void filereq_set_path(int drive, const char *name, void *buf)
{
    filereq_work.drive = drive;
    filereq_work.name = name;
    filereq_work.buf = buf;
}

/* 0x00101620 */
void filereq_set_drive(int drive)
{
    filereq_work.drive = drive;
}

#ifndef VERSION_US

/* Card access indicator task. */
void backup_disp_task_init(int *w, int mode)
{
    w[0] = mode;
    backup_disp_work = w;
    w[1] = 0;
    task_set_dlist(w, 13);
}

void backup_disp_task_release(void)
{
    backup_disp_work = 0;
}

/* Sprite 6, screen corner, from the second frame on. */
void backup_disp_task_disp(DispWork *w)
{
    int old_test; int old_fc; int old_ztest;
    float x;
    float y;

    w->frame++;
    if (w->frame == 1)
        return;
    if (w->mode != 0)
        return;
    x = screen_layout[7];
    y = screen_layout[8];
    old_test = draw_set_test_mode(0);
    old_fc = draw_set_fc_mode(0);
    old_ztest = draw_set_ztest_mode(0);
    sprite_draw(x, y, 3.3333333f, COL_WHITE, 6, 2, 0);
    draw_set_ztest_mode(old_ztest);
    draw_set_fc_mode(old_fc);
    draw_set_test_mode(old_test);
}

/* Mode 0 shows the indicator. */
#ifndef VERSION_US

extern TaskDesc backup_disp_desc;

void *backup_disp_create(int mode)
{
    return task_create_arg(fade_get_work(), &backup_disp_desc, 8, mode);
}

#endif

void backup_disp_kill(void)
{
    void *w = backup_disp_work;

    if (w) {
        do {
            TASK_OF(w)->flags |= TASK_F_END;
        } while (0);
    }
    backup_disp_work = 0;
}

#endif /* !VERSION_US */

/* Debug window: fields of the current request. */
/* 0x00101630 */
static void backup_window_draw(int win)
{
    FileReq *req = &filereq_work;
    int cw;
    int ch;
    int y;

    gw_get_cell_size(&cw, &ch);
    gw_win_printf(win, 2, 2, "status %s", backup_status_names[filereq_work.state]);
    y = ch + 2;
    gw_win_printf(win, 2, y, "crc    %08x", req->result);
    y += ch;
    gw_win_printf(win, 2, y, "type   %s", backup_cmd_names[req->cmd]);
    y += ch;
    gw_win_printf(win, 2, y, "drive  %d", req->drive);
    y += ch;
    if (req->name)
        gw_win_printf(win, 2, y, "name   %s", req->name);
    y += ch;
    gw_win_printf(win, 2, y, "data   %08x", req->buf);
    y += ch;
    gw_win_printf(win, 2, y, "size   %d", req->size);
    y += ch;
    gw_win_printf(win, 2, y, "copy flag %s", req->flag ? "TRUE" : "FALSE");
}

/* 0x001017d0 */
static void backup_window_closed(void)
{
    backup_window = 0;
}

/* 0x001017e0 */
static void backup_window_toggle(void)
{
    int cw;
    int ch;
    int win;

    if (backup_window) {
        gw_window_close(backup_window);
        return;
    }
    gw_get_cell_size(&cw, &ch);
    win = gw_window_open("Backup", 95, (int)(screen_layout[7] - (float)(cw * 27) - 4.0f),
                     (int)(screen_layout[8] + 4.0f + (float)ch + 10.0f), cw * 27, ch * 14, 0);
    backup_window = win;
    if (win != 0)
        gw_window_set_handlers(win, 0, backup_window_draw, backup_window_closed);
}

/* Events: 100 connect (card size), 101 mount, 102 disconnect, 16 format, 110/111 load, 112 save. */
/* steer(cast) */
/* 0x001018d0 */
int backup_mc_callback(int port, int op, int status, int param)
{
    McSlot *slot = &mc_slots[mc_drive_slots[port]];
    int drive = filereq_work.drive;
    int ret;

    if (drive != port)
        return 0;
    DEBUG_PRINTF("Drive %d Callback op % 4d(0x%02x) status %4d param %4d\n", drive, op, op, status,
                 param);
    switch (op) {
    case 100:
        switch (status) {
        case 15:
            DEBUG_PRINTF("Drive %d Memory Connect 128KB\n", drive);
            break;
        case 33:
            DEBUG_PRINTF("Drive %d Memory Connect 256KB\n", drive);
            break;
        case 65:
            DEBUG_PRINTF("Drive %d Memory Connect 512KB\n", drive);
            break;
        case 122:
            DEBUG_PRINTF("Drive %d Memory Connect 1MB\n", drive);
            break;
        default:
            DEBUG_PRINTF("Drive %d Memory Connect Unknown status = %d\n", port, status);
            status = 15;
            break;
        }
        slot->card_size = status;
        slot->state = 0;
        slot->flags |= 0x20;
        break;
    case 101:
        while (!((int (*)(int))mc_drain_events)(port))
            ;
        switch (ulMcProbe(port)) {
        case 0:
            DEBUG_PRINTF("Drive %d Memory Mount\n", port);
            slot->state = 3;
            break;
        case -2:
            DEBUG_PRINTF("Drive %d Memory Mount but Unformat\n", port);
            slot->state = 2;
            break;
        case -100:
            DEBUG_PRINTF("Drive %d Memory Mount but Disconnect\n", port);
            backup_unmount_request(port);
            break;
        }
        return 0;
    case 102:
        DEBUG_PRINTF("Drive %d Memory Disconnect\n", drive);
        slot->flags = 0;
        heap_free(slot->buf, 0);
        slot->buf = 0;
        debug_off(drive);
        return 0;
    case 112:
        if (status < 0) {
            DEBUG_PRINTF("Drive %d Save Failed\n", drive);
            slot->state = 6;
        } else {
            DEBUG_PRINTF("Drive %d Save Complete\n", drive);
            slot->state = 5;
        }
        return 0;
    case 110:
    case 111:
        if (status < 0) {
            DEBUG_PRINTF("Drive %d Load Failed\n", drive);
            slot->state = 6;
        } else {
            DEBUG_PRINTF("Drive %d Load Complete\n", drive);
            slot->state = 5;
        }
        return 0;
    case 16:
        if (status < 0) {
            DEBUG_PRINTF("Drive %d Format Failed\n", drive);
            slot->state = 6;
        } else {
            DEBUG_PRINTF("Drive %d Format Complete\n", drive);
            slot->state = 5;
        }
        return 0;
    }
    return 0;
}

/* 0x00101bf0 */
static int backup_mc_null_callback(void)
{
    return 0;
}

/* 0x00101bf8 */
static void backup_set_callbacks(void)
{
    ulMcSetCallback(backup_mc_callback);
    ulMcSetSubCallback(backup_mc_null_callback);
}

/* 0x00101c20 */
static void backup_init(int flag)
{
    DEBUG_PRINT("Backup Init\n");
    ulMcSetup(15, 255, 0, backup_set_callbacks);
    backup_window_toggle();
#ifndef VERSION_US
    backup_disp_create(flag == 0);
#endif
}

/* Waits until the library has no messages left. */
/* 0x00101c58 */
static void backup_exit(void)
{
    DEBUG_PRINT("Backup Exit\n");
    while (ulMcDrainMessages())
        ;
    backup_window_toggle();
#ifndef VERSION_US
    backup_disp_kill();
#endif
}

/* 0x00101c88 */
static int backup_get_crc(void *hdr)
{
    return *(unsigned short *)((char *)hdr + 70);
}

/* kind 0..4; header from backup_save_table, text in the current language. 1 ok, 0 error. */
/* 0x00101c90 */
static int make_save_data(void *data, void *size, void **out_buf, int *out_rc, unsigned int kind)
{
    SaveHdr hdr;
    char *title;
    char *sub;
    char *name;
    int rc;
    int jp;

    if (kind >= 5) {
        DEBUG_PRINTF("make_save_data() : \221\312\226\332\202\310\203^\203C\203v : %d\n", kind);
        return 0;
    }
    jp = (config_language == 0);
    memset(&hdr, 0, 96);
    if (jp) {
        title = backup_save_titles[0][kind];
        sub = str_backup_sub_title_jp;
        name = str_backup_save_name_jp;
    } else {
        title = backup_save_titles[1][kind];
        sub = str_backup_sub_title_en;
        name = str_backup_save_name_en;
    }
    hdr.icon_palette = backup_save_table[kind].icon_palette;
    hdr.icon_image = backup_save_table[kind].icon_image;
    hdr.block_kind = backup_save_table[kind].block_kind;
    hdr.icon_speed = backup_save_table[kind].icon_speed;
    strcpy(hdr.title, title);
    strcpy(hdr.sub, sub);
    {
        int n = strlen(name);

        memcpy(hdr.name, name, n);
    }
    hdr.data = data;
    hdr.z = 0;
    hdr.zs = 0;
    hdr.arg = size;
    *out_rc = mc_calc_save_blocks(hdr.block_kind, 0, size);
    *out_buf = heap_block_tag(game_malloc(*out_rc << 10), "backup.c", SRC_LINE(688, 710));
    *out_rc = mc_build_save_image(*out_buf, &hdr);
    if (*out_rc < 0) {
        heap_free(*out_buf, 0);
        *out_buf = 0;
        return 0;
    }
    return 1;
}

/* Card blocks for kind 0..4. Unknown-kind message printed without its argument. */
/* 0x00101e58 */
static int calc_save_block(int size, unsigned int type)
{
    int kind = 0;

    switch (type) {
    case 0:
        kind = 3;
        break;
    case 1:
        kind = 3;
        break;
    case 2:
        kind = 3;
        break;
    case 3:
        kind = 1;
        break;
    case 4:
        kind = 1;
        break;
    default:
        DEBUG_PRINTF("calc_save_block() : \221\312\226\332\202\310\203^\203C\203v : %d\n");
        break;
    }
    return mc_calc_save_blocks(kind, 0, size);
}

/* 0 and all three outputs zeroed on failure. */
/* 0x00101ed8 */
static int backup_read_file_info(char *path, int *addr, int *size, int *crc)
{
    McInfo buf;
    int rc = mc_unpack_save_image(&buf, path);

    if (rc >= -1011) {
        if (rc >= -1009) {
            if (rc == 0) {
                *(void **)addr = (void *)buf.addr;
                *size = buf.size;
                *crc = *(unsigned short *)(path + 70);
                return 1;
            }
        }
    }
    *size = 0;
    *addr = 0;
    *crc = 0;
    return 0;
}

/* Stamped with the current time. 1 on success. */
/* steer(cast) */
/* 0x00101f78 */
static int backup_save_file(int drive, const char *name, void *buf, int size, int flag)
{
    McTime now;
    McTime stamp;
    unsigned int color = RGBA(0xff, 0, 0, 0x80);

    if (flag)
        color = COL_BLACK;
    ((void (*)(McTime *))nj_clock_read)(&now);
    stamp.year = now.year;
    stamp.month = now.month;
    stamp.day = now.day;
    stamp.hour = now.hour;
    stamp.min = now.min;
    stamp.sec = now.sec;
    stamp.dow = now.dow;
    if (ulMcSaveFile(drive, name, buf, size, &stamp, color) == 0)
        return 1;
    ERROR_PRINTF("[ERROR]Backup Save Error : %s\n", name);
    return 0;
}

/* Directory buffer sized from the card size. A drive with no card is reset instead. */
/* steer(cast) */
/* 0x00102050 */
static void backup_mount_request(int drive)
{
    McSlot *slot;
    int port;
    int port_slot;
    int size;

    slot = &mc_slots[mc_drive_slots[drive]];
    port = ulMcCardToPort(drive);
    port_slot = ulMcCardToSlot(drive);
    if (mc_is_connected(port, port_slot) != 0) {
        if (slot->state == 0) {
            slot->state = 1;
            if (slot->buf == 0) {
                size = (slot->card_size << 9) + 32;
                slot->buf = heap_block_tag(game_malloc(size), "backup.c", SRC_LINE(828, 850));
                ((void (*)(int, void *, int))mc_mount_request)(drive, slot->buf, size);
                DEBUG_PRINTF("Drive %d Mount Request\n", drive);
            } else {
                DEBUG_PRINTF("Drive %d Mount Request ERROR\n", drive);
            }
        }
    } else {
        DEBUG_PRINTF("Drive %d Mount Request but slot have no memory\n", drive);
        DEBUG_PRINTF("        Slot Stat = %d\n", slot->state);
        slot->state = 0;
        heap_free(slot->buf, 0);
        slot->buf = 0;
    }
}

/* Also frees the slot's buffer. */
/* steer(cast) */
/* 0x001021a0 */
static void backup_unmount_request(int drive)
{
    McSlot *slot = &mc_slots[mc_drive_slots[drive]];

    if (slot->flags & 1) {
        DEBUG_PRINTF("Drive %d Unmount Request Slot Stat %d\n", drive, slot->state);
        ((void (*)(int))mc_unmount)(drive);
        slot->flags &= ~0x20;
        slot->state = 0;
        heap_free(slot->buf, 0);
        slot->buf = 0;
    } else {
        DEBUG_PRINTF("Drive %d Unmount Request but slot have no memory\n", drive);
        DEBUG_PRINTF("        Slot Stat = %d\n", slot->state);
        slot->state = 0;
        heap_free(slot->buf, 0);
        slot->buf = 0;
    }
}

/* 1 when mounted and the file exists. */
/* 0x00102278 */
static int backup_slot_ready(int port, const char *name)
{
    int rc;
    McSlot *slot;

    slot = &mc_slots[mc_drive_slots[port]];
    if (slot->state == 3) {
        rc = ulMcFindFile(port, name);
        if (rc != -4) {
            if (rc == 0)
                return 1;
        }
    }
    return 0;
}

/* Retries while the library is busy (-200). */
/* 0x001022f0 */
static int BackupGetFileBlock_Ex(int drive, const char *name)
{
    int r;

    do {
        r = ulMcGetFileBlocks(drive, name);
    } while (r == -200);
    return r;
}

/* Sets system flags 0xa00 for the duration (old ones restored). Result starts as 3 (error). */
/* 0x00102340 */
static void backup_save_task_init(SaveWork *w)
{
    unsigned int flags;
    McSlot *slot;

    w->code = 3;
    TASK_OF(w)->flags |= TASK_F_RUN_PAUSED;
    flags = system_flags;
    slot = &mc_slots[mc_drive_slots[filereq_work.drive]];
    filereq_work.result = 0;
    w->saved_flags = flags & (SYS_NO_SOFT_RESET | SYS_NO_VIBRATION);
    *(int *)&w->phase = 0; /* steer(type): the phase is stored as an int here, as the original */
    system_flags = flags | (SYS_NO_SOFT_RESET | SYS_NO_VIBRATION);
    filereq_work.state = 1;
    w->buf = 0;
    slot->job = MC_JOB_SAVE;
    DEBUG_PRINTF("Backup Save File Start\n");
    backup_init(1);
}

/* 0x001023e0 */
static void backup_save_task_release(SaveWork *w)
{
    FileReq *req = &filereq_work;
    McSlot *slot = &mc_slots[mc_drive_slots[req->drive]];

    slot->job = 0;
    slot->access = 0;
    heap_free(w->buf, 0);
#ifdef VERSION_US
    warning_cancel();
#endif
    system_flags &= ~(SYS_NO_SOFT_RESET | SYS_NO_VIBRATION);
    system_flags |= w->saved_flags;
    backup_exit();
    filereq_work.state = w->code;
    DEBUG_PRINTF("Backup Save File EndCode = %d\n", w->code);
}

/*
 * Phases: 0 claim, 1 wait for drive, 2 mount/space check/write/wait, 10 unmount, 11 end when idle.
 * Results: 2 saved, 3 error, 4 unformatted, 7 not enough space (a replaced file's blocks count).
 */
/* steer(cast) */
/* 0x00102490 */
static void backup_save_task_exec(SaveWork *w)
{
    FileReq *req = &filereq_work;
    McSlot *slot = &mc_slots[mc_drive_slots[req->drive]];
    int need;
    int have;
    int rc;
    int code;

    switch (w->phase) {
    case 0:
        slot->access = 1;
        w->phase++;
        break;
    case 1:
        ((int (*)())backup_drive_wait)(req->drive, 0);
        do { w->phase++; } while (0);
        break;
    case 2:
        switch (slot->state) {
        case 0:
            if (slot->flags & 1) {
                backup_mount_request(req->drive);
                return;
            }
            code = 3;
            goto fin;
        case 3:
            if (!(slot->flags & 1)) {
                code = 3;
                goto fin;
            }
            need = calc_save_block(req->size, req->cmd);
            have = ((int (*)(int, int))ulMcGetFree)(req->drive, 0);
            if (have < 0) {
                code = 3;
                goto fin;
            }
            if (backup_slot_ready(req->drive, req->name)) {
                DEBUG_PRINTF("Backup Drive %d name %s Update\n", req->drive, req->name);
                rc = BackupGetFileBlock_Ex(req->drive, req->name);
                have += rc;
                if (rc < 0) {
                    DEBUG_PRINTF("BackupGetFileBlock_Ex: error %d\n", rc);
                    code = 3;
                    goto fin;
                }
            } else {
                DEBUG_PRINTF("Backup Drive %d name %s Save\n", req->drive, req->name);
            }
            if (have < need) {
                DEBUG_PRINTF("\227e\227\312\202\252\221\253\202\350\202\334\202\271\202\361\n");
                code = 7;
                goto fin;
            }
            if (make_save_data(req->buf, (void *)req->size, &w->buf, &need, req->cmd)) {
                *(McSlotState *)&slot->state = 4;
                if (backup_save_file(req->drive, req->name, w->buf, need, req->flag) == 0) {
                    code = 3;
                    goto fin;
                }
                req->result = backup_get_crc(w->buf);
#ifdef VERSION_US
                warning_request(32, 0);
#endif
                return;
            }
            code = 3;
            goto fin;
        case 2:
            DEBUG_PRINTF("Unformat!!!\n");
            code = 4;
            goto fin;
        case 5:
            code = 2;
            goto fin;
        case 6:
            code = 3;
        fin:
            w->phase = 10;
            do { w->code = code; } while (0);
            break;
        }
        break;
    case 10:
        backup_unmount_request(req->drive);
        w->phase++;
        break;
    case 11:
        if (slot->state == 0) {
            if (w != 0)
                TASK_OF(w)->flags |= TASK_F_END;
        }
        break;
    }
}

/* Only when no request runs, the drive has a card and the slot has no job; else 0. */
/* 0x00102728 */
void *BackupSaveTaskSet(void)
{
    McSlot *slot = &mc_slots[mc_drive_slots[filereq_work.drive]];

    if (filereq_work.state != 0) {
        DEBUG_PRINTF("BackupSaveTaskSet() : Memory Card is Busy Drive %d\n", filereq_work.drive);
        return 0;
    }
    if (mcslot_is_busy(filereq_work.drive) == 0) {
        DEBUG_PRINTF("BackupSaveTaskSet() : NO VMS Drive %d\n", filereq_work.drive);
        return 0;
    }
    if (slot->job != 0) {
        DEBUG_PRINTF("BackupSaveTaskSet() : Save Failed work%d Drive%d\n", slot->job, filereq_work.drive);
        return 0;
    }
    return task_create(scr_root, &backup_save_desc, 16);
}

/* Load dropped if owner ends first. System flags 0xa00 for the duration. */
/* 0x001027f8 */
static void backup_load_task_init(LoadWork *w, void *owner)
{
    unsigned int flags;
    McSlot *slot;

    w->code = 3;
    TASK_OF(w)->flags |= TASK_F_RUN_PAUSED;
    w->invalid = 0;
    w->owner = owner;
    flags = system_flags;
    filereq_work.result = 0;
    slot = &mc_slots[mc_drive_slots[filereq_work.drive]];
    w->saved_flags = flags & (SYS_NO_SOFT_RESET | SYS_NO_VIBRATION);
    w->size = 0;
    *(int *)&w->phase = 0; /* steer(type): the phase is stored as an int here, as the original */
    w->data = 0;
    filereq_work.state = 1;
    slot->job = MC_JOB_LOAD;
    system_flags = flags | (SYS_NO_SOFT_RESET | SYS_NO_VIBRATION);
    DEBUG_PRINTF("Backup Load File Start\n");
    backup_init(0);
}

/* If the owner went away, frees the request's data and clears the request instead of posting. */
/* 0x001028a8 */
static void backup_load_task_release(LoadWork *w)
{
    FileReq *req = &filereq_work;
    McSlot *slot = &mc_slots[mc_drive_slots[req->drive]];

    slot->job = 0;
    slot->access = 0;
    heap_free(w->data, 0);
    system_flags &= ~(SYS_NO_SOFT_RESET | SYS_NO_VIBRATION);
    system_flags |= w->saved_flags;
    backup_exit();
    if (w->invalid == 0) {
        filereq_work.state = w->code;
    } else {
        heap_free(req->buf, 0);
        req->buf = 0;
        filereq_clear();
        DEBUG_PRINTF("Backup Load is Invalid!\n");
    }
    DEBUG_PRINTF("Backup Load File EndCode = %d\n", req->state);
}

/*
 * Phases as backup_save_task_exec; request buffer allocated here when null.
 * Results: 2 loaded, 3 error, 4 unformatted, 5 not found, 6 bad or empty.
 */
/* steer(cast) */
/* 0x00102988 */
static void backup_load_task_exec(LoadWork *w)
{
    FileReq *req = &filereq_work;
    McSlot *slot = &mc_slots[mc_drive_slots[req->drive]];
    int addr, size, res;
    int n;
    int code;

    if (w->owner != 0 && (TASK_OF(w->owner)->flags & TASK_F_END)) {
        w->owner = 0;
        w->invalid = 1;
    }
    switch (w->phase) {
    case 0:
        slot->access = 2;
        w->phase++;
        break;
    case 1:
        ((int (*)())backup_drive_wait)(req->drive, 1);
        w->phase++;
        break;
    case 2:
        switch (slot->state) {
        case 0:
            if (slot->flags & 1) {
                backup_mount_request(req->drive);
                return;
            }
            code = 3;
            goto fin;
        case 3:
            if (!(slot->flags & 1)) {
                code = 3;
                goto fin;
            }
            if (!backup_slot_ready(req->drive, req->name)) {
                DEBUG_PRINTF("Backup Drive %d name %s not found!\n", req->drive, req->name);
                code = 5;
                goto fin;
            }
            DEBUG_PRINTF("Backup Drive %d name %s Load\n", req->drive, req->name);
            n = BackupGetFileBlock_Ex(req->drive, req->name);
            if (n == 0) {
                DEBUG_PRINTF("BackupGetFileBlock_Ex: error %d\n", 0);
                code = 6;
                goto fin;
            }
            if (n < 0) {
                DEBUG_PRINTF("BackupGetFileBlock_Ex: error %d\n", n);
                code = 3;
                goto fin;
            }
            w->size = n;
            w->data = heap_block_tag(game_malloc(n << 10), "backup.c", SRC_LINE(1403, 1440));
            *(McSlotState *)&slot->state = 4;
            if (ulMcLoadFile(req->drive, req->name, w->data, w->size)) {
                ERROR_PRINTF("buLoadFile: error\n");
                return;
            }
            break;
        case 2:
            DEBUG_PRINTF("Unformat!!!\n");
            code = 4;
            goto fin;
        case 5:
            *(McSlotState *)&slot->state = 3;
            if (backup_read_file_info(w->data, &addr, &size, &res)) {
                w->code = 2;
                req->result = res;
                req->size = size;
                if (size > 0) {
                    void *dst = req->buf;

                    if (dst == 0)
                        req->buf = dst = heap_block_tag(game_malloc(size), "backup.c", SRC_LINE(1433, 1470));
                    memcpy(dst, (void *)addr, size);
                }
            } else {
                w->code = 6;
            }
            w->phase = 10;
            break;
        case 6:
            code = 3;
        fin:
            w->phase = 10;
            do { w->code = code; } while (0);
            break;
        }
        break;
    case 10:
        backup_unmount_request(req->drive);
        w->phase++;
        break;
    case 11:
        if (slot->state == 0) {
            if (w != 0)
                TASK_OF(w)->flags |= TASK_F_END;
        }
        break;
    }
}

/* See BackupSaveTaskSet. */
/* 0x00102c88 */
void *BackupLoadTaskSet(void *owner)
{
    FileReq *req = &filereq_work;
    int drive = req->drive;
    McSlot *slot = &mc_slots[mc_drive_slots[drive]];

    if (filereq_work.state != 0) {
        DEBUG_PRINTF("BackupLoadTaskSet() : Memory Card is Busy Drive %d\n", drive);
        return 0;
    }
    if (mcslot_is_busy(drive) == 0) {
        DEBUG_PRINTF("BackupLoadTaskSet() : NO VMS Drive %d\n", req->drive);
        return 0;
    }
    if (slot->job != 0) {
        DEBUG_PRINTF("BackupLoadTaskSet() : Load Failed work%d Drive%d\n", slot->job, req->drive);
        return 0;
    }
    return task_create_arg(scr_root, &backup_load_desc, 28, (int)owner);
}

/* See backup_save_task_init. */
/* 0x00102d68 */
static void backup_format_task_init(FormatWork *w)
{
    unsigned int flags;
    McSlot *slot;

    w->code = 3;
    TASK_OF(w)->flags |= TASK_F_RUN_PAUSED;
    flags = system_flags;
    slot = &mc_slots[mc_drive_slots[filereq_work.drive]];
    filereq_work.result = 0;
    w->saved_flags = flags & (SYS_NO_SOFT_RESET | SYS_NO_VIBRATION);
    w->phase = 0;
    filereq_work.state = 1;
    system_flags = flags | (SYS_NO_SOFT_RESET | SYS_NO_VIBRATION);
    slot->job = MC_JOB_FORMAT;
    DEBUG_PRINTF("Backup Fomat Start\n");
#ifdef VERSION_US
    backup_init(1);
#else
    backup_init(0);
#endif
}

/* 0x00102e00 */
static void backup_format_task_release(FormatWork *w)
{
    FileReq *req = &filereq_work;
    McSlot *slot;

    slot = &mc_slots[mc_drive_slots[req->drive]];
#ifdef VERSION_US
    slot->job = MC_JOB_NONE;
    *(float *)&slot->access = 0.0f;
    warning_cancel();
    system_flags &= ~(SYS_NO_SOFT_RESET | SYS_NO_VIBRATION);
    system_flags |= w->saved_flags;
#else
    system_flags &= ~(SYS_NO_SOFT_RESET | SYS_NO_VIBRATION);
    system_flags |= w->saved_flags;
    slot->job = MC_JOB_NONE;
    *(float *)&slot->access = 0.0f;
#endif
    backup_exit();
    filereq_work.state = w->code;
    DEBUG_PRINTF("Backup Format EndCode = %d\n", w->code);
}

/* Formats only an unformatted card; already formatted counts as success. Results: 2 ok, 3 error. */
/* steer(cast) */
/* 0x00102ea0 */
static void backup_format_task_exec(FormatWork *w)
{
    FileReq *req = &filereq_work;
    McSlot *slot = &mc_slots[mc_drive_slots[req->drive]];

    int ph = w->phase;

    switch (ph) {
    case 0:
        *(void **)&slot->access = (void *)1;
        w->phase = ph + 1;
        break;
    case 1:
        w->phase = ph + 1;
        break;
    case 2:
        switch (slot->state) {
        case 0:
            if (slot->flags & 1) {
                backup_mount_request(req->drive);
            } else {
                w->code = 3;
                w->phase = 10;
            }
            break;
        case 3:
            if (slot->flags & 1) {
                w->code = 2;
                w->phase = 10;
            } else {
                w->code = 3;
                w->phase = 10;
            }
            break;
        case 2:
            while (!((int (*)(int))mc_drain_events)(req->drive))
                ;
            ((int (*)(int, int, int, int, int))mc_format)(req->drive, 0, 0, 0, 0);
            slot->state = 4;
#ifdef VERSION_US
            warning_request(33, 0); /* "formatting" box */
#endif
            break;
        case 5:
            *(void **)&slot->state = (void *)3;
            w->code = 2;
            w->phase = 10;
            break;
        case 6:
            w->code = 3;
            w->phase = 10;
            break;
        }
        break;
    case 10:
        backup_unmount_request(req->drive);
        w->phase++;
        break;
    case 11:
        if (slot->state == 0) {
            do { } while (0);
            TASK_OF(w)->flags |= TASK_F_END;
        }
        break;
    }
}

/* See BackupSaveTaskSet. */
/* 0x00103068 */
void *BackupFormatTaskSet(void *owner)
{
    FileReq *req = &filereq_work;
    int drive = req->drive;
    McSlot *slot = &mc_slots[mc_drive_slots[drive]];

    if (filereq_work.state != 0) {
        DEBUG_PRINTF("BackupFormatTaskSet() : Memory Card is Busy Drive %d\n", drive);
        return 0;
    }
    if (mcslot_is_busy(drive) == 0) {
        DEBUG_PRINTF("BackupFormatTaskSet() : NO VMS Drive %d\n", req->drive);
        return 0;
    }
    if (slot->job != 0) {
        DEBUG_PRINTF("BackupFormatTaskSet() : Format Failed work%d Drive%d\n", slot->job, req->drive);
        return 0;
    }
    return task_create_arg(scr_root, &backup_format_desc, 28, (int)owner);
}

/* Checks a card for the system file. System flags 0xa00 for the duration. */
/* 0x00103148 */
void backup_checkvms_task_init(CheckVmsWork *w)
{
    unsigned int flags;
    McSlot *slot;
    unsigned int sf;
    ReqState st;

    DEBUG_PRINTF("Drive %d CheckVms Start\n", filereq_work.drive);
    flags = system_flags;
    TASK_OF(w)->flags |= TASK_F_RUN_PAUSED;
    filereq_work.result = 0;
    slot = &mc_slots[mc_drive_slots[filereq_work.drive]];
    st = REQ_BUSY;
    sf = flags & (SYS_NO_SOFT_RESET | SYS_NO_VIBRATION);
    flags |= (SYS_NO_SOFT_RESET | SYS_NO_VIBRATION);
    w->phase = 0;
    w->saved_flags = sf;
    slot->job = MC_JOB_LOAD;
    system_flags = flags;
    filereq_work.state = st;
    backup_init(0);
}

/* 0x00103200 */
static void backup_checkvms_task_release(CheckVmsWork *w)
{
    FileReq *req = &filereq_work;
    McSlot *slot;

    slot = &mc_slots[mc_drive_slots[req->drive]];
    system_flags &= ~(SYS_NO_SOFT_RESET | SYS_NO_VIBRATION);
    system_flags |= w->saved_flags;
    slot->job = MC_JOB_NONE;
    *(float *)&slot->access = 0.0f;
    backup_exit();
    DEBUG_PRINTF("Drive %d CheckVms End\n", req->drive);
    filereq_work.state = 0;
}

/* Flags a card with "KPRJ____.DBG"; auto-load drive: system file CRC -> backup_system_crc. */
/* steer(cast) */
/* 0x00103298 */
void backup_checkvms_task_exec(CheckVmsWork *w)
{
    FileReq *req = &filereq_work;
    McSlot *slot = &mc_slots[mc_drive_slots[req->drive]];
    int crc;

    switch (w->phase) {
    case 0:
        w->phase++;
        break;
    case 1:
        ((int (*)())backup_drive_wait)(req->drive, 1);
        w->phase++;
        break;
    case 2:
        switch (slot->state) {
        case 0:
            if (slot->flags & 1)
                backup_mount_request(req->drive);
            else
                w->phase = 10;
            break;
        case 3:
            if (slot->flags & 1) {
                if (backup_slot_ready(req->drive, "KPRJ____.DBG"))
                    slot->flags |= 0x10;
                if (auto_load_drive == req->drive) {
                    if (backup_slot_ready(req->drive, BACKUP_GAME_ID)) {
                        *(void **)&slot->state = (void *)4;
                        ulMcLoadFileAt(req->drive, BACKUP_GAME_ID, w, 0, 1);
                    } else {
                        DEBUG_PRINTF("CRC Check Drive %d No SystemFile\n", req->drive);
                        w->phase = 10;
                    }
                } else {
                    w->phase = 10;
                }
            } else {
                w->phase = 10;
            }
            break;
        case 2:
            DEBUG_PRINTF("Unformat!!!\n");
            w->phase = 10;
            break;
        case 5:
            crc = backup_get_crc(w);
            req->result = crc;
            DEBUG_PRINTF("Drive %d CRC Check Complete crc %08x\n", req->drive, crc);
            backup_system_crc = req->result;
            w->phase = 10;
            break;
        case 6:
            w->phase = 10;
            break;
        }
        break;
    case 10:
        backup_unmount_request(req->drive);
        w->phase++;
        break;
    case 11:
        if (slot->state == 0) {
            if (w != 0)
                TASK_OF(w)->flags |= TASK_F_END;
        }
        break;
    }
}

/* See BackupSaveTaskSet. */
/* 0x001034e8 */
static void *BackupCheckVms(int drive)
{
    McSlot *slot = &mc_slots[mc_drive_slots[drive]];

    if (filereq_work.state != 0) {
        DEBUG_PRINTF("BackupCheckVms() : Memory Card is Busy Drive %d\n", filereq_work.drive);
        return 0;
    }
    if (mcslot_is_busy(drive) == 0) {
        DEBUG_PRINTF("BackupCheckVms() : NO VMS Drive %d in Check\n", drive);
        return 0;
    }
    if (slot->job != 0) {
        DEBUG_PRINTF("BackupCheckVms() : CRCCheck Faiild work%d Drive%d\n", slot->job, drive);
        return 0;
    }
    filereq_work.drive = drive;
    return task_create(scr_root, &backup_checkvms_desc, 1032);
}

/* Never waits. */
/* 0x001035c8 */
static int backup_drive_wait(void)
{
    return 0;
}
