/*
 * Configuration auto load at boot: loads the config file from a card, offers to create one (or
 * format), then the vibration question and health warning. The options menu reruns it.
 */

#include "types_common.h"
#include "billboard.h"
#include "fade.h"
#include "heap.h"
#include "libc.h"
#include "model.h"
#include "module.h"
#include "node_draw_state.h"
#include "system.h"
#include "ul_gx.h"
#include "ul_texture.h"
#include "backup.h"
#include "config.h"
#include "libulmc.h"
#include "warning.h"
#include "task.h"
#include "game.h"

typedef struct AutoloadWork {
    char pad_00[4];
    void *textures;             /* 0x04 */
    int state;                  /* 0x08 */
    int loaded;                 /* 0x0c: drives with a configuration file */
    int drive;                  /* 0x10: drive being checked */
    int load_drive;             /* 0x14: first drive with a file, -1 = none */
    int empty_drive;            /* 0x18: first drive with no file, -1 = none */
    float fade;                 /* 0x1c */
    int timer;                  /* 0x20 */
    int msg;                    /* 0x24: warning message to show */
    int ask_vibration;          /* 0x28 */
} AutoloadWork;

#include "debug.h"
#include "color.h"

#ifdef VERSION_US
#define TASK_END(w) do { if (w) TASK_OF(w)->flags |= 1; } while (0)
#else
#define TASK_END(w) if (w) TASK_OF(w)->flags |= 1
#endif

extern Pad *pad_main;
extern unsigned int pad_decide_mask;
extern unsigned int system_flags;
extern GameState game_state;
extern int mc_drive_slots[];
extern char config_file_name[];

/* health warning, one per language */
extern char first_warning_textures[];

extern struct TaskDesc autoload_desc;

extern ScreenCfg screen_cfg;
void autoload_exec_warning(AutoloadWork *w);
void autoload_exec_create(AutoloadWork *w);
void autoload_exec(AutoloadWork *w);

/* 0x00ca0700 */
void first_warning_load(AutoloadWork *w)
{
    w->textures = model_create_timed(first_warning_textures, 2048);
}

/* 0x00ca0738 */
void first_warning_unload(AutoloadWork *w)
{
    model_list_remove(w->textures);
}

#ifdef VERSION_US
/* 1 while SYS_WARNING_PENDING is clear: later runs skip the vibration question and warning. */
/* 0x00ca0740 */
int warning_is_skipped(void)
{
    return ((int)system_flags >> 13 ^ 1) & 1;
}
#endif

/* 0x00ca0758 */
void autoload_init(AutoloadWork *w)
{
#ifndef VERSION_US
    system_flags &= ~SYS_WARNING_PENDING;
#endif
    TASK_OF(w)->flags |= TASK_F_RUN_PAUSED;
    game_state.card_changed = 0;
    first_warning_load(w);
    w->fade = 0;
    w->ask_vibration = 0;
}

/* 0x00ca0798 */
void autoload_release(AutoloadWork *w)
{
#ifdef VERSION_US
    system_flags &= ~SYS_WARNING_PENDING;
#endif
    first_warning_unload(w);
    DEBUG_PRINTF("AutoLoadDrive = %d\n", auto_load_drive);
    DEBUG_PRINT("---- Config Auto Load End   ----\n");
}

/* Health warning stays w->timer frames or until a button press. */
/* 0x00ca07e8 */
void autoload_exec_warning(AutoloadWork *w)
{
#ifdef VERSION_US
    if (warning_is_skipped()) {
        TASK_END(w);
        return;
    }
#endif
    switch (w->state) {
    case 0:
        DEBUG_PRINT("---- Config Auto Load : Start Vibration Use ----");
        fade_start_out(0);
        if (w->ask_vibration)
            w->state++;
        else
            w->state = 100;
        break;
    case 1:
#ifdef VERSION_US
        if (warning_request(28, 1))
#else
        if (warning_request(25, 1))
#endif
            w->state++;
        break;
    case 2:
        if (warning_is_done()) {
            ConfigSetVibration(warning_get_result());
            w->state = 100;
        }
        break;
    case 100:
        DEBUG_PRINT("---- Config Auto Load : Start Epilepsy Use ----\n");
        fade_start_out(0);
        w->state++;
        break;
    case 101:
        w->fade += 0.05f;
        if (1.0f <= w->fade) {
            w->fade = 1.0f;
            w->state++;
        }
        break;
    case 102:
        if (pad_main->raw.trig & (pad_decide_mask | 8))
            w->timer = 0;
        if (--w->timer <= 0)
            w->state = 103;
        break;
    case 103:
        w->fade -= 0.05f;
        if (w->fade <= 0) {
            w->fade = 0;
            TASK_END(w);
        }
        break;
    }
}

#ifdef VERSION_US
/* Warning 29: new card or "no" restarts the load; "yes" turns auto save off. */
/* 0x00ca09d8 */
void autoload_exec_save_off(AutoloadWork *w)
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
            TASK_OF(w)->exec = autoload_exec;
            break;
        }
        if (!warning_is_done())
            break;
        if (!warning_get_result()) {
            w->state = 0;
            TASK_OF(w)->exec = autoload_exec;
            break;
        }
        game_state.autosave_on = 0;
        game_state.card_changed = 0;
        TASK_OF(w)->exec = autoload_exec_warning;
        w->ask_vibration = 1;
        w->timer = 900;
        w->state = 0;
        break;
    }
}

typedef enum AutoloadMsg { AL_MSG_SAVE_EMPTY = 11 } AutoloadMsg;

/*
 * A too-full card counts as an empty drive. Warning 30 confirms the save (declining: save off);
 * warning 26 offers to format.
 */
/* steer(cast) */
/* 0x00ca0ad0 */
void autoload_exec_create(AutoloadWork *w)
{
    int done = 0;
    unsigned int st;
    int res;
    int need;
    int avail;

    switch (w->state) {
    case 0:
        DEBUG_PRINT("---- Config Auto Load : Start Create ----\n");
        w->drive = 0;
        w->load_drive = -1;
        w->empty_drive = -1;
        w->loaded = 0;
        w->state++;
    case 1:
        if (w->drive >= 8) {
            if (w->load_drive >= 0)
                w->state = 300;
            else {
                w->msg = 10;
                w->state = 100;
            }
            break;
        }
        w->state++;
        break;
    case 2:
        while (!((int (*)(int))mc_drain_events)(w->drive))
            ;
        need = mc_calc_save_blocks(0, 0, filereq_work.size);
        avail = ((int (*)(int, int))ulMcGetFree)(w->drive, 0);
        printf("*************************** %d\n", avail);
        if (avail < 0) {
            switch (avail) {
            case -100:
                break;
            case -2:
                if (w->load_drive < 0)
                    w->load_drive = w->drive;
                w->loaded++;
                break;
            }
            w->drive++;
            w->state = 1;
            break;
        }
        if (avail < need) {
            if (w->empty_drive < 0)
                w->empty_drive = w->drive;
            w->loaded++;
            w->drive++;
            w->state = 1;
            break;
        }
        warning_request(30, 0);
        w->state++;
        break;
    case 3:
        if (!(mc_slots[mc_drive_slots[w->drive]].flags & 1)) {
            warning_cancel();
            w->state = 0;
            TASK_OF(w)->exec = autoload_exec;
            break;
        }
        if (!warning_is_done())
            break;
        if (warning_get_result()) {
            w->state++;
        } else {
            w->state = 0;
            TASK_OF(w)->exec = autoload_exec_save_off;
        }
        break;
    case 4:
        if (!filereq_is_free())
            break;
        filereq_set(0, w->drive, config_file_name, system_data_init(), system_data_get_size(), 1);
        if (!BackupSaveTaskSet()) {
            w->state = 1;
            w->drive++;
        } else {
            w->loaded++;
            w->state++;
        }
        break;
    case 5:
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
            auto_load_drive = 0;
            w->msg = 14;
            w->state = 251;
            break;
        case 4:
            if (w->load_drive < 0)
                w->load_drive = w->drive;
            w->drive++;
            w->state = 1;
            break;
        case 7:
            if (w->empty_drive < 0)
                w->empty_drive = w->drive;
            w->drive++;
            w->state = 1;
            break;
        }
        filereq_clear();
        break;
    case 100:
        if (w->loaded > 0 && w->empty_drive >= 0) {
            *(AutoloadMsg *)&w->msg = AL_MSG_SAVE_EMPTY;
            w->drive = w->empty_drive;
            auto_load_drive = w->empty_drive;
        }
        if (warning_request(w->msg, 0))
            w->state++;
        break;
    case 101:
        if (!(mc_slots[mc_drive_slots[w->drive]].flags & 1)) {
            warning_cancel();
            w->state = 0;
            TASK_OF(w)->exec = autoload_exec;
            break;
        }
        if (game_state.card_changed > 0) {
            game_state.card_changed = 0;
            DEBUG_PRINTF("New MemoryCard Insert\n");
            warning_cancel();
            w->state = 0;
            TASK_OF(w)->exec = autoload_exec;
            break;
        }
        if (warning_is_done()) {
            game_state.card_changed = 0;
            game_state.vms_slot = 0;
            done = 1;
        }
        break;
    case 200:
        game_state.autosave_on = 1;
        game_state.vms_slot = w->drive;
        w->state++;
    case 201:
        if (warning_request(w->msg, 0))
            w->state++;
        break;
    case 202:
        done = warning_is_done() != 0;
        break;
    case 250:
        game_state.autosave_on = 1;
        game_state.vms_slot = w->drive;
        w->state++;
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
        if (game_state.card_changed > 0) {
            game_state.card_changed = 0;
            DEBUG_PRINTF("New MemoryCard Insert\n");
            warning_cancel();
            w->state = 0;
            TASK_OF(w)->exec = autoload_exec;
            break;
        }
        if (!warning_is_done())
            break;
        if (!warning_get_result()) {
            w->state = 0;
            TASK_OF(w)->exec = autoload_exec;
            break;
        }
        game_state.autosave_on = 0;
        done = 1;
        game_state.card_changed = 0;
        break;
    case 300:
        auto_load_drive = w->load_drive;
        if (warning_request(26, 0))
            w->state++;
        break;
    case 301:
        if (!mc_is_connected(ulMcCardToPort(w->load_drive), ulMcCardToSlot(w->load_drive))) {
            warning_cancel();
            w->state = 0;
            TASK_OF(w)->exec = autoload_exec;
            break;
        }
        if (game_state.card_changed > 0) {
            game_state.card_changed = 0;
            DEBUG_PRINTF("New MemoryCard Insert\n");
            warning_cancel();
            w->state = 0;
            TASK_OF(w)->exec = autoload_exec;
            break;
        }
        if (!warning_is_done())
            break;
        if (!warning_get_result())
            w->state = 400;
        else
            w->state++;
        break;
    case 302:
        if (!filereq_is_free())
            break;
        filereq_set_drive(w->load_drive);
        if (!BackupFormatTaskSet(w)) {
            w->msg = 10;
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
            TASK_OF(w)->exec = autoload_exec;
            break;
        }
        if (!warning_is_done())
            break;
        game_state.vms_slot = 0;
        if (warning_get_result()) {
            game_state.autosave_on = 0;
            done = 1;
            game_state.card_changed = 0;
        } else {
            w->state = 300;
        }
        break;
    }
    if (done) {
        TASK_OF(w)->exec = autoload_exec_warning;
        w->ask_vibration = 1;
        w->timer = 900;
        w->state = 0;
    }
}

#else
/* 0x00ca0ad0 */
void autoload_exec_create(AutoloadWork *w)
{
    int done = 0;
    unsigned int st;
    int res;

    switch (w->state) {
    case 0:
        DEBUG_PRINT("---- Config Auto Load : Start Create ----\n");
        w->drive = 0;
        w->load_drive = -1;
        w->empty_drive = -1;
        w->loaded = 0;
        w->state++;
    case 1:
        if (w->drive >= 8) {
            if (w->load_drive >= 0)
                w->state = 300;
            else {
                w->msg = 10;
                w->state = 100;
            }
            break;
        }
        w->state++;
        break;
    case 2:
        if (!filereq_is_free())
            break;
        filereq_set(0, w->drive, config_file_name, system_data_init(), system_data_get_size(), 1);
        if (!BackupSaveTaskSet()) {
            w->state = 1;
            w->drive++;
        } else {
            w->loaded++;
            w->state++;
        }
        break;
    case 3:
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
            if (!(mc_slots[mc_drive_slots[w->drive]].flags & 1)) {
                w->state = 0;
                TASK_OF(w)->exec = autoload_exec;
            } else {
                auto_load_drive = 0;
                w->msg = 14;
                w->state = 1;
            }
            break;
        case 4:
            if (w->load_drive < 0)
                w->load_drive = w->drive;
            w->drive++;
            w->state = 1;
            break;
        case 7:
            if (w->empty_drive < 0)
                w->empty_drive = w->drive;
            w->drive++;
            w->state = 1;
            break;
        }
        filereq_clear();
        break;
    case 100:
        if (w->loaded > 0 && w->empty_drive >= 0) {
            w->drive = w->empty_drive;
            w->msg = 11;
            auto_load_drive = w->empty_drive;
        }
        if (warning_request(w->msg, 0))
            w->state++;
        break;
    case 101:
        if (!(mc_slots[mc_drive_slots[w->drive]].flags & 1))
            goto cancel;
        if (game_state.card_changed > 0)
            goto inserted;
        if (warning_is_done()) {
            game_state.vms_slot = 0;
            done = 1;
            game_state.card_changed = 0;
        }
        break;
    case 200:
        game_state.autosave_on = 1;
        game_state.vms_slot = w->drive;
        w->state++;
    case 201:
        if (warning_request(w->msg, 0))
            w->state++;
        break;
    case 202:
        done = warning_is_done() != 0;
        break;
    case 300:
        auto_load_drive = w->load_drive;
        if (warning_request(23, 0))
            w->state++;
        break;
    case 301:
        if (!mc_is_connected(ulMcCardToPort(w->load_drive), ulMcCardToSlot(w->load_drive)))
            goto cancel;
        if (game_state.card_changed > 0)
            goto inserted;
        if (!warning_is_done())
            break;
        if (!warning_get_result())
            w->state = 400;
        else
            w->state++;
        break;
    case 302:
        if (!filereq_is_free())
            break;
        filereq_set_drive(w->load_drive);
        if (!BackupFormatTaskSet(w)) {
            w->msg = 10;
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
        if (st == 2) {
            w->state = 0;
        } else if (st == 3) {
            if (mc_slots[mc_drive_slots[w->load_drive]].flags & 1) {
                w->state = 201;
                w->msg = 14;
            } else {
                DEBUG_PRINTF("format drive[%d] cancel by disconnect\n", w->load_drive);
                w->state = 0;
            }
        }
        filereq_clear();
        break;
    case 400:
        if (warning_request(26, 0))
            w->state++;
        break;
    case 401:
        if (game_state.card_changed > 0) {
        inserted:
            game_state.card_changed = 0;
            DEBUG_PRINTF("New MemoryCard Insert\n");
        cancel:
            warning_cancel();
            w->state = 0;
            TASK_OF(w)->exec = autoload_exec;
            break;
        }
        if (!warning_is_done())
            break;
        if (!warning_get_result())
            w->state = 300;
        else
            done = 1;
        break;
    }
    if (done) {
        TASK_OF(w)->exec = autoload_exec_warning;
        w->ask_vibration = 1;
        w->timer = 900;
        w->state = 0;
    }
}
#endif

#ifdef VERSION_US

/* No file anywhere: autoload_exec_create; a failed load asks with warning 29 (states 250..254). */
/* 0x00ca1250 */
void autoload_exec(AutoloadWork *w)
{
    int done = 0;
    void *buf;
    int st;
    int res;

    switch (w->state) {
    case 0:
        if (!mc_scan_step())
            break;
        game_state.card_changed = 0;
        DEBUG_PRINT("---- Config Auto Load : Start Load ----\n");
        w->loaded = 0;
        w->drive = 0;
        w->state++;
        break;
    case 1:
        if (w->drive >= 8) {
            w->drive = -1;
            w->msg = 10;
            w->state = 100;
        } else {
            w->state = 2;
        }
        break;
    case 2:
        if (!filereq_is_free())
            break;
        filereq_set_path(w->drive, config_file_name, 0);
        if (!BackupLoadTaskSet(w)) {
            w->state = 1;
            w->drive++;
        } else {
            w->loaded++;
            w->state++;
        }
        break;
    case 3:
        if (!filereq_is_idle())
            break;
        buf = filereq_get_buf();
        st = filereq_get_state();
        res = filereq_get_result();
        switch (st) {
        case 2:
            if (system_data_load(buf)) {
                ConfigSetVmsLink(res);
                w->msg = 7;
                w->state = 200;
            } else {
                w->msg = 12;
                w->state = 250;
            }
            break;
        case 3:
            if (!(mc_slots[mc_drive_slots[w->drive]].flags & 1)) {
                DEBUG_PRINTF("nnnnnnnn disk nnnnnnn\n");
                w->state = 1;
                w->loaded--;
                w->drive++;
            } else {
                auto_load_drive = 0;
                w->msg = 8;
                w->state = 251;
            }
            break;
        case 6:
            auto_load_drive = 0;
            w->msg = 12;
            w->state = 251;
            break;
        case 4:
        case 5:
            w->state = 1;
            w->drive++;
            break;
        }
        heap_free(buf, 0);
        filereq_clear();
        break;
    case 100:
        if (warning_is_skipped()) {
            module_set_clear_flag(7, -1);
            TASK_END(w);
            return;
        }
        if (w->loaded > 0) {
            w->state = 0;
            TASK_OF(w)->exec = autoload_exec_create;
        } else if (warning_request(w->msg, 0)) {
            w->state++;
        }
        break;
    case 101:
        if (game_state.card_changed > 0) {
            game_state.card_changed = 0;
            warning_cancel();
            w->state = 0;
        } else if (warning_is_done()) {
            game_state.autosave_on = 0;
            game_state.card_changed = 0;
            game_state.vms_slot = 0;
            w->ask_vibration = 1;
            done = 1;
        }
        break;
    case 200:
        game_state.autosave_on = 1;
        game_state.vms_slot = w->drive;
        w->ask_vibration = 0;
        w->state++;
    case 201:
        if (warning_request(w->msg, 0))
            w->state++;
        break;
    case 202:
        done = warning_is_done() != 0;
        break;
    case 250:
        game_state.autosave_on = 1;
        game_state.vms_slot = w->drive;
        w->ask_vibration = 0;
        w->state++;
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
        if (warning_get_result())
            done = 1;
        else
            w->state = 0;
        break;
    }
    if (done) {
        w->timer = 900;
        TASK_OF(w)->exec = autoload_exec_warning;
        w->state = 0;
    }
}

#else
/* 0x00ca1250 */
void autoload_exec(AutoloadWork *w)
{
    int done = 0;
    void *buf;
    int st;
    int res;

    switch (w->state) {
    case 0:
        if (!mc_scan_step())
            break;
        game_state.card_changed = 0;
        DEBUG_PRINT("---- Config Auto Load : Start Load ----\n");
        w->loaded = 0;
        w->drive = 0;
        w->state++;
        break;
    case 1:
        if (w->drive >= 8) {
            w->drive = -1;
            w->msg = 10;
            w->state = 100;
        } else {
            w->state = 2;
        }
        break;
    case 2:
        if (!filereq_is_free())
            break;
        filereq_set_path(w->drive, config_file_name, 0);
        if (!BackupLoadTaskSet(w)) {
            w->state = 1;
            w->drive++;
        } else {
            w->loaded++;
            w->state++;
        }
        break;
    case 3:
        if (!filereq_is_idle())
            break;
        buf = filereq_get_buf();
        st = filereq_get_state();
        res = filereq_get_result();
        switch (st) {
        case 2:
            w->msg = system_data_load(buf) ? (ConfigSetVmsLink(res), 7) : 12;
            w->state = 200;
            break;
        case 3:
            if (!(mc_slots[mc_drive_slots[w->drive]].flags & 1)) {
                DEBUG_PRINTF("nnnnnnnn disk nnnnnnn\n");
                w->state = 1;
                w->loaded--;
                w->drive++;
            } else {
                auto_load_drive = 0;
                w->msg = 8;
                w->state = 201;
            }
            break;
        case 6:
            auto_load_drive = 0;
            w->msg = 12;
            w->state = 201;
            break;
        case 4:
        case 5:
            w->state = 1;
            w->drive++;
            break;
        }
        heap_free(buf, 0);
        filereq_clear();
        break;
    case 100:
        if (w->loaded > 0) {
            w->state = 0;
            TASK_OF(w)->exec = autoload_exec_create;
        } else if (warning_request(w->msg, 0)) {
            w->state++;
        }
        break;
    case 101:
        if (game_state.card_changed > 0) {
            game_state.card_changed = 0;
            warning_cancel();
            w->state = 0;
        } else if (warning_is_done()) {
            game_state.vms_slot = 0;
            game_state.autosave_on = 0;
            done = 1;
            game_state.card_changed = 0;
            w->ask_vibration = 1;
        }
        break;
    case 200:
        game_state.autosave_on = 1;
        game_state.vms_slot = w->drive;
        w->ask_vibration = 0;
        w->state++;
    case 201:
        if (warning_request(w->msg, 0))
            w->state++;
        break;
    case 202:
        done = warning_is_done() != 0;
        break;
    }
    if (done) {
        w->timer = 900;
        TASK_OF(w)->exec = autoload_exec_warning;
        w->state = 0;
    }
}
#endif

/* 0x00ca16e0 */
void autoload_disp(AutoloadWork *w)
{
    int lang;
    int color;
    float half;
    float tw;
    float th;
    float x;

    if (0 < w->fade) {
        lang = game_lang;
        ulGxSetTextureList(w->textures);
        half = 0.5f;
        draw_set_blend(4, 5);
        color = RGBA(0x80, 0x80, 0x80, ((int)(w->fade * 255.0f) + 1) >> 1);
        tw = ulTextureGetWidth(ulTexListGet(w->textures, lang));
        th = ulTextureGetHeight(ulTexListGet(w->textures, lang));
        x = floorf(screen_cfg.cx - tw * half);
        tex_rect_draw(lang, color, x, floorf(screen_cfg.cy - th * half), 1.0f, tw, th);
    }
}

/* 0x00ca1848 */
void *autoload_create(void *parent)
{
    return task_create(parent, &autoload_desc, 44);
}
