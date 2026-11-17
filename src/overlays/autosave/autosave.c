/*
 * Configuration auto save between stages: saves over the card's config file, asking before
 * overwriting or formatting; a failed or declined save offers to turn auto save off. Pad locked.
 */

#include "types_common.h"
#include "color.h"
#include "heap.h"
#include "pad.h"
#include "backup.h"
#include "config.h"
#include "libulmc.h"
#include "warning.h"
#include "task.h"
#include "system.h"
#include "debug.h"

#define TASK_END(w) do { if (w) FINISH(w); } while (0)

/* 32 bytes */
typedef struct AsRootWork {
    int rewind;                 /* drive disconnected or load check failed */
    int keep;                   /* 0x04: 0 = turn auto save off when the box closes */
    int format;                 /* 0x08: the load check found no file: offer to format */
    int state;                  /* 0x0c */
    int found;                  /* 0x10: the load check found a file (asks before saving) */
    int msg;                    /* 0x14: warning message to show */
    int load_busy;              /* 0x18: a load request owns the request buffer */
    int save_busy;              /* 0x1c: a save request is running */
} AsRootWork;

extern GameState game_state;

extern int mc_drive_slots[];
extern char config_file_name[];

extern struct TaskDesc as_root_desc;

void as_root_exec_check(AsRootWork *w);

void as_root_exec(AsRootWork *w);

/* Ends unless auto save is on or a card changed; no saved card link: check the file first. */
/* 0x00ca0700 */
void as_root_init(AsRootWork *w)
{
    if (!game_state.autosave_on && game_state.card_changed <= 0) {
        DEBUG_PRINTF("AutoSave Cancel\n");
        TASK_END(w);
    } else {
        if (!game_state.vms_link)
            TASK_OF(w)->exec = (void (*)(void *))as_root_exec_check;
        w->load_busy = 0;
        w->rewind = 0;
        w->format = 0;
        w->found = 0;
        w->save_busy = 0;
        w->keep = 1;
        pad_input_disable(0);
    }
}

/* 0x00ca07a8 */
void as_root_release(AsRootWork *w)
{
    int mask;

    pad_input_enable(0);
    if (w->load_busy) {
        heap_free(filereq_get_buf(), 0);
        filereq_clear();
        mask = debug_color_set(RGBA(0x80, 0, 0, 0x80));
        DEBUG_PRINT("MemoryCard Occupation Load\n");
        debug_color_set(mask);
    }
    if (w->save_busy) {
        filereq_clear();
        mask = debug_color_set(RGBA(0x80, 0, 0, 0x80));
        DEBUG_PRINT("MemoryCard Occupation Save\n");
        debug_color_set(mask);
    }
    DEBUG_PRINT("---- Config Auto Save : Exit ----\n");
}

/* Loads first to learn whether the file exists or the card needs formatting. */
/* 0x00ca0868 */
void as_root_exec_check(AsRootWork *w)
{
    int done = 0;
    void *buf;

    switch (w->state) {
    case 0:
        DEBUG_PRINT("---- Config Auto Save : Load Check Start ----\n");
#ifdef VERSION_US
        w->found = 0;
        w->format = 0;
#endif
        w->state++;
    case 1:
        w->rewind = 0;
        if (!filereq_is_free())
            break;
        filereq_set_path(auto_load_drive, config_file_name, 0);
        if (!BackupLoadTaskSet(w)) {
            w->rewind = 1;
            done = 1;
        } else {
            w->load_busy = 1;
            w->format = 0;
            w->state++;
        }
        break;
    case 2:
        if (!filereq_is_idle())
            break;
        buf = filereq_get_buf();
        switch (filereq_get_state()) {
        case 2:
        case 6:
            w->found = 1;
            break;
        case 4:
            w->format = 1;
            break;
        case 5:
            w->found = 0;
            break;
        }
        w->load_busy = 0;
        done = 1;
        heap_free(buf, 0);
        filereq_clear();
        break;
    }
    if (done) {
        game_state.card_changed = 0;
        TASK_OF(w)->exec = (void (*)(void *))as_root_exec;
        w->state = 0;
    }
}

#ifdef VERSION_US
/* Warning 29: new card or "no" restarts the check; "yes" turns auto save off. */
/* 0x00ca0a00 */
void as_root_exec_save_off(AsRootWork *w)
{
    switch (w->state) {
    case 0:
        if (warning_request(29, 0))
            w->state++;
        break;
    case 1:
        if (game_state.card_changed > 0) {
            game_state.card_changed = 0;
            DEBUG_PRINTF("New MemoryCard Insert\n");
            warning_cancel();
            w->state = 0;
            TASK_OF(w)->exec = (void (*)(void *))as_root_exec_check;
            break;
        }
        if (!warning_is_done())
            break;
        if (!warning_get_result()) {
            w->state = 0;
            TASK_OF(w)->exec = (void (*)(void *))as_root_exec_check;
            break;
        }
        game_state.autosave_on = 0;
        game_state.card_changed = 0;
        TASK_END(w);
        break;
    }
}

/* Warning 27 before overwriting, 30 before a new save; full card: message 11; failure: 29. */
/* steer(cast) */
/* 0x00ca0ae8 */
void as_root_exec(AsRootWork *w)
{
    int size;
    int st;
    int res;
    int need;
    int rc;

    switch (w->state) {
    case 0:
        DEBUG_PRINT("---- Config Auto Save : Save Start ----\n");
        w->state++;
        break;
    case 1:
        if (w->rewind) {
            w->msg = 1;
            w->state = 100;
            w->keep = 0;
            break;
        }
        if (w->format) {
            DEBUG_PRINT("Format Check\n");
            w->format = 0;
            w->state = 300;
            break;
        }
        if (!w->found) {
            w->state = 50;
            break;
        }
        if (warning_request(27, 0))
            w->state++;
        break;
    case 2:
        if (!(mc_slots[mc_drive_slots[game_state.vms_slot]].flags & 1)) {
            warning_cancel();
            DEBUG_PRINTF("rewind for disconnect drive[%d]\n", game_state.vms_slot);
            w->rewind = 1;
            w->state = 1;
            break;
        }
        if (game_state.card_changed > 0) {
            game_state.card_changed = 0;
            DEBUG_PRINTF("New MemoryCard Insert\n");
            warning_cancel();
            w->state = 0;
            TASK_OF(w)->exec = (void (*)(void *))as_root_exec_check;
            break;
        }
        if (!warning_is_done())
            break;
        if (!warning_get_result()) {
            game_state.autosave_on = 0;
            game_state.card_changed = 0;
            TASK_END(w);
            break;
        }
        w->state = 50;
        break;
    case 50:
        if (w->found) {
            w->state = 52;
            break;
        }
        while (!((int (*)(int))mc_drain_events)(game_state.vms_slot))
            ;
        rc = ulMcFindFile(game_state.vms_slot, config_file_name);
        if (!rc) {
            w->state += 2;
            break;
        }
        switch (rc) {
        case -4:
            break;
        case -2:
            DEBUG_PRINT("Format Check\n");
            w->format = 0;
            w->state = 300;
            break;
        case -100:
            w->rewind = 1;
            w->state = 1;
            break;
        }
        while (!((int (*)(int))mc_drain_events)(game_state.vms_slot))
            ;
        need = mc_calc_save_blocks(0, 0, filereq_work.size);
        rc = ((int (*)(int, int))ulMcGetFree)(game_state.vms_slot, 0);
        if (rc < 0) {
            if (rc == -100) {
                w->rewind = 1;
                w->state = 1;
                break;
            }
            if (rc == -2) {
                DEBUG_PRINT("Format Check\n");
                w->format = 0;
                w->state = 300;
            }
            break;
        }
        if (rc < need) {
            w->msg = 11;
            w->state = 150;
            break;
        }
        warning_request(30, 0);
        w->state++;
        break;
    case 51:
        if (!(mc_slots[mc_drive_slots[game_state.vms_slot]].flags & 1)) {
            warning_cancel();
            DEBUG_PRINTF("rewind for disconnect drive[%d]\n", game_state.vms_slot);
            w->rewind = 1;
            w->state = 1;
            break;
        }
        if (!warning_is_done())
            break;
        if (!warning_get_result()) {
            w->state = 0;
            TASK_OF(w)->exec = (void (*)(void *))as_root_exec_save_off;
            break;
        }
        w->state++;
        break;
    case 52:
        if (!filereq_is_free())
            break;
        size = system_data_get_size();
        DEBUG_PRINTF("ConfigDataSize : %8d(%08x)\n", size, size);
        filereq_set(0, game_state.vms_slot, config_file_name, system_data_init(), size, 1);
        if (!BackupSaveTaskSet()) {
            game_state.card_changed = 0;
            w->msg = 1;
            w->state = 100;
            w->keep = 0;
        } else {
            w->save_busy = 1;
            w->state++;
        }
        break;
    case 53:
        if (!filereq_is_idle())
            break;
        st = filereq_get_state();
        res = filereq_get_result();
        switch (st) {
        case 2:
            ConfigSetVmsLink(res);
            w->msg = 13;
            w->state = 200;
            break;
        case 3:
            w->msg = 5;
            w->state = 251;
            break;
        case 4:
            w->msg = 5;
            w->state = 251;
            w->keep = 0;
            break;
        case 7:
            w->msg = 11;
            w->state = 150;
            w->keep = 0;
            break;
        }
        w->save_busy = 0;
        filereq_clear();
        break;
    case 100:
        if (warning_request(w->msg, 0))
            w->state++;
        break;
    case 101:
        if (game_state.card_changed > 0) {
            game_state.card_changed = 0;
            DEBUG_PRINTF("New MemoryCard Insert\n");
            warning_cancel();
            w->state = 0;
            TASK_OF(w)->exec = (void (*)(void *))as_root_exec_check;
            break;
        }
        if (!warning_is_done())
            break;
        if (!w->keep) {
            game_state.card_changed = 0;
            game_state.autosave_on = 0;
        }
        TASK_END(w);
        break;
    case 150:
        if (warning_request(w->msg, 0))
            w->state++;
        break;
    case 151:
        if (!(mc_slots[mc_drive_slots[game_state.vms_slot]].flags & 1)) {
            warning_cancel();
            w->state = 0;
            TASK_OF(w)->exec = (void (*)(void *))as_root_exec_check;
            break;
        }
        if (game_state.card_changed > 0) {
            game_state.card_changed = 0;
            DEBUG_PRINTF("New MemoryCard Insert\n");
            warning_cancel();
            w->state = 0;
            TASK_OF(w)->exec = (void (*)(void *))as_root_exec_check;
            break;
        }
        if (!warning_is_done())
            break;
        if (!w->keep) {
            game_state.card_changed = 0;
            game_state.autosave_on = 0;
        }
        TASK_END(w);
        break;
    case 200:
        if (game_state.autosave_on) {
            TASK_END(w);
            break;
        }
        game_state.autosave_on = 1;
        w->state++;
    case 201:
        if (warning_request(w->msg, 0))
            w->state++;
        break;
    case 202:
        if (warning_is_done())
            TASK_END(w);
        break;
    case 250:
        if (game_state.autosave_on) {
            TASK_END(w);
            break;
        }
        w->state = 251;
    case 251:
        if (warning_request(w->msg, 0))
            w->state++;
        break;
    case 252:
        if (warning_is_done())
            w->state++;
        break;
    case 253:
        if (warning_request(29, 0))
            w->state++;
        break;
    case 254:
        if (!warning_is_done())
            break;
        if (!warning_get_result()) {
            w->state = 0;
            TASK_OF(w)->exec = (void (*)(void *))as_root_exec_check;
            break;
        }
        game_state.autosave_on = 0;
        game_state.card_changed = 0;
        TASK_END(w);
        break;
    case 300:
        if (warning_request(26, 0))
            w->state++;
        break;
    case 301:
        if (!mc_is_connected(ulMcCardToPort(game_state.vms_slot), ulMcCardToSlot(game_state.vms_slot))) {
            warning_cancel();
            w->rewind = 1;
            w->state = 1;
            break;
        }
        if (game_state.card_changed > 0) {
            game_state.card_changed = 0;
            DEBUG_PRINTF("New MemoryCard Insert\n");
            warning_cancel();
            w->state = 0;
            TASK_OF(w)->exec = (void (*)(void *))as_root_exec_check;
            break;
        }
        if (!warning_is_done())
            break;
        if (warning_get_result())
            w->state++;
        else
            w->state = 400;
        break;
    case 302:
        if (!filereq_is_free())
            break;
        filereq_set_drive(game_state.vms_slot);
        if (!BackupFormatTaskSet(w)) {
            game_state.card_changed = 0;
            w->msg = 1;
            w->state = 100;
            w->keep = 0;
        } else {
            w->state++;
        }
        break;
    case 303:
        if (!filereq_is_idle())
            break;
        st = filereq_get_state();
        filereq_get_result();
        switch (st) {
        case 2:
            w->found = 0;
            w->state = 0;
            break;
        case 3:
            w->msg = 16;
            w->state = 251;
            break;
        }
        filereq_clear();
        break;
    case 400:
        if (warning_request(29, 0))
            w->state++;
        break;
    case 401:
        if (game_state.card_changed > 0) {
            game_state.card_changed = 0;
            DEBUG_PRINTF("New MemoryCard Insert\n");
            warning_cancel();
            w->state = 0;
            TASK_OF(w)->exec = (void (*)(void *))as_root_exec_check;
            break;
        }
        if (!warning_is_done())
            break;
        if (warning_get_result()) {
            game_state.autosave_on = 0;
            game_state.card_changed = 0;
            TASK_END(w);
        } else {
            w->state = 300;
        }
        break;
    }
}

#else
/* 0x00ca0ae8 */
void as_root_exec(AsRootWork *w)
{
    int size;
    int st;
    int res;

    switch (w->state) {
    case 0:
        DEBUG_PRINT("---- Config Auto Save : Save Start ----\n");
        w->state++;
        break;
    case 1:
        if (w->rewind) {
            w->msg = 1;
            w->state = 100;
            w->keep = 0;
            break;
        }
        if (w->format) {
            DEBUG_PRINT("Format Check\n");
            w->format = 0;
            w->state = 300;
            break;
        }
        if (!w->found) {
            w->state = 50;
            break;
        }
        if (warning_request(24, 0))
            w->state++;
        break;
    case 2:
        if (!(mc_slots[mc_drive_slots[game_state.vms_slot]].flags & 1)) {
            warning_cancel();
            DEBUG_PRINTF("rewind for disconnect drive[%d]\n", game_state.vms_slot);
            w->rewind = 1;
            w->state = 1;
            break;
        }
        if (game_state.card_changed > 0) {
            game_state.card_changed = 0;
            DEBUG_PRINTF("New MemoryCard Insert\n");
            warning_cancel();
            w->state = 0;
            TASK_OF(w)->exec = (void (*)(void *))as_root_exec_check;
            break;
        }
        if (!warning_is_done())
            break;
        if (!warning_get_result()) {
            game_state.autosave_on = 0;
            game_state.card_changed = 0;
            TASK_END(w);
            break;
        }
        w->state = 50;
        break;
    case 50:
        if (!filereq_is_free())
            break;
        size = system_data_get_size();
        DEBUG_PRINTF("ConfigDataSize : %8d(%08x)\n", size, size);
        filereq_set(0, game_state.vms_slot, config_file_name, system_data_init(), size, 1);
        if (!BackupSaveTaskSet()) {
            game_state.card_changed = 0;
            w->msg = 1;
            w->state = 100;
        } else {
            w->save_busy = 1;
            w->state++;
        }
        break;
    case 51:
        if (!filereq_is_idle())
            break;
        st = filereq_get_state();
        res = filereq_get_result();
        switch (st) {
        case 2:
            ConfigSetVmsLink(res);
            w->msg = 13;
            w->state = 200;
            break;
        case 3:
            w->state = 100;
            w->msg = 5;
            break;
        case 4:
            w->state = 100;
            w->msg = 5;
            w->keep = 0;
            break;
        case 7:
            w->state = 100;
            w->msg = 11;
            w->keep = 0;
            break;
        }
        w->save_busy = 0;
        filereq_clear();
        break;
    case 100:
        if (warning_request(w->msg, 0))
            w->state++;
        break;
    case 101:
        if (game_state.card_changed > 0) {
            game_state.card_changed = 0;
            DEBUG_PRINTF("New MemoryCard Insert\n");
            warning_cancel();
            w->state = 0;
            TASK_OF(w)->exec = (void (*)(void *))as_root_exec_check;
            break;
        }
        if (!warning_is_done())
            break;
        if (!w->keep) {
            game_state.card_changed = 0;
            game_state.autosave_on = 0;
        }
        TASK_END(w);
        break;
    case 200:
        if (game_state.autosave_on) {
            TASK_END(w);
            break;
        }
        game_state.autosave_on = 1;
        w->state++;
    case 201:
        if (warning_request(w->msg, 0))
            w->state++;
        break;
    case 202:
        if (warning_is_done())
            TASK_END(w);
        break;
    case 300:
        if (warning_request(23, 0))
            w->state++;
        break;
    case 301:
        if (!mc_is_connected(ulMcCardToPort(game_state.vms_slot), ulMcCardToSlot(game_state.vms_slot))) {
            warning_cancel();
            w->rewind = 1;
            w->state = 1;
            break;
        }
        if (game_state.card_changed > 0) {
            game_state.card_changed = 0;
            DEBUG_PRINTF("New MemoryCard Insert\n");
            warning_cancel();
            w->state = 0;
            TASK_OF(w)->exec = (void (*)(void *))as_root_exec_check;
            break;
        }
        if (!warning_is_done())
            break;
        if (warning_get_result())
            w->state++;
        else
            w->state = 400;
        break;
    case 302:
        if (!filereq_is_free())
            break;
        filereq_set_drive(game_state.vms_slot);
        if (!BackupFormatTaskSet(w)) {
            game_state.card_changed = 0;
            w->msg = 1;
            w->state = 100;
        } else {
            w->state++;
        }
        break;
    case 303:
        if (!filereq_is_idle())
            break;
        st = filereq_get_state();
        filereq_get_result();
        switch (st) {
        case 2:
            w->found = 0;
            w->state = 0;
            break;
        case 3:
            if (!(mc_slots[mc_drive_slots[auto_load_drive]].flags & 1)) {
                DEBUG_PRINTF("format drive[%d] cancel by disconnect\n", auto_load_drive);
                w->rewind = 1;
                w->state = 1;
            } else {
                w->msg = 14;
                w->state = 201;
            }
            break;
        }
        filereq_clear();
        break;
    case 400:
        if (warning_request(26, 0))
            w->state++;
        break;
    case 401:
        if (game_state.card_changed > 0) {
            game_state.card_changed = 0;
            DEBUG_PRINTF("New MemoryCard Insert\n");
            warning_cancel();
            w->state = 0;
            TASK_OF(w)->exec = (void (*)(void *))as_root_exec_check;
            break;
        }
        if (!warning_is_done())
            break;
        if (warning_get_result()) {
            game_state.autosave_on = 0;
            TASK_END(w);
        } else {
            w->state = 300;
        }
        break;
    }
}
#endif

/* 0x00ca1510 */
void as_root_disp(void)
{
}

/* 0x00ca1518 */
void *as_root_create(void *parent)
{
    return task_create(parent, &as_root_desc, 32);
}
