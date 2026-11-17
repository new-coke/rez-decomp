/*
 * Peripheral slots: 24 (six per logical port) for controllers, memory cards and others. Registers
 * save data with the memory-card library, tracks connections (memory card in/out -> config code),
 * "PadSlot" debug window.
 */

#include "types_common.h"
#include "debug.h"
#include "gw_draw.h"
#include "gw_menu.h"
#include "gw_system.h"
#include "gw_window.h"
#include "libulmc.h"
#include "nj_system.h"
#include "padslot.h"
#include "config.h"
#include "debugmenu.h"
#include "peri_act.h"
#include "system.h"
#include "backup.h"

#ifdef VERSION_US

/* Product code + "REZ". */
#define BACKUP_GAME_ID "BASLUS-20344REZ"

#else

#define BACKUP_GAME_ID "BASLPM-62101REZ"

#endif /* !VERSION_US */

/* Registered with ulMcInit. */
typedef struct BackupSaveDesc {
    const char *id;
    char *icon;
    char *icon_data;
    int model;
    int copy_icon, copy_icon_size, del_icon, del_icon_size;
} BackupSaveDesc;

extern unsigned int system_flags;
extern int padslot_window;
extern float screen_layout[];
extern BackupSaveDesc backup_save_info;
extern char backup_icon_sys[];
extern char backup_icon_data[];
extern int backup_icon_model;
extern int mcslot_init_ports[];
extern int mcslot_reset_counts[];
extern char *const mcslot_state_names[];
extern char *bits_to_string(unsigned int bits, int n, const char *on, const char *off);

static void padslot_menu_add(void);

/* Directory name, icon.sys and icon model; slots reset to unassigned. */
/* 0x00144658 */
void backup_setup(void)
{
    int i;

    backup_save_info.id = BACKUP_GAME_ID;
    backup_save_info.icon = backup_icon_sys;
    backup_save_info.icon_data = backup_icon_data;
    backup_save_info.model = backup_icon_model;
    backup_save_info.copy_icon = 0;
    backup_save_info.copy_icon_size = 0;
    backup_save_info.del_icon = 0;
    backup_save_info.del_icon_size = 0;
    ulMcInit(&backup_save_info);
    mc_slot_mark_used(0, 0);
#ifndef VERSION_US
    mc_slot_mark_used(1, 0);
#endif
    for (i = 0; i < 24; i++) {
        mc_slots[i].port = mcslot_init_ports[i];
        mc_slots[i].index = i;
        mc_slots[i].id = -1;
        mc_slots[i].flags = 0;
        mc_slots[i].state = 0;
        mc_slots[i].job = 0;
        mc_slots[i].card_size = 0;
        mc_slots[i].buf = 0;
        mc_slots[i].mode = 0;
        mc_slots[i].access = 0;
    }
}

/* 0x00144728 */
void pad_driver_init(void)
{
    peri_act_close_all();
    padslot_menu_add();
}

/* Six slots from first on. */
/* 0x00144748 */
void mcslot_set_port(int port, int first)
{
    unsigned int i;
    McSlot *p = &mc_slots[first];

    for (i = 0; i < 6; i++, p++)
        p->id = port;
}

/* Six slots from idx on: stops vibration, clears flags, counts the reset. */
/* 0x00144788 */
void mcslot_reset_port(int port, int idx)
{
    McSlot *p = &mc_slots[idx];
    unsigned int i;

    peri_act_reset_slot(port, 0);
    for (i = 0; i < 6; i++, p++) {
        p->flags = 0;
        p->id = -1;
    }
    mcslot_reset_counts[idx]++;
}

/* Something connected in memory-card drive i. */
/* 0x00144810 */
int mcslot_is_busy(int i)
{
    McSlot *slot = &mc_slots[mc_drive_slots[i]];

    return slot->flags & 1;
}

/* One of the first six slots on port id with flag 8. */
/* 0x00144848 */
static int mcslot_find_ready(int id)
{
    unsigned int i;
    McSlot *p = mc_slots;
    int found = 0;

    for (i = 0; i < 6; i++, p++) {
        if (p->id == id) {
            if (p->flags & 8)
                found = 1;
        }
    }
    return found;
}

/*
 * Connect: ConfigVmsConnect, mode 0 -> 2. Mode 1 waits for the backup request; mode 2 for
 * SYS_DEBUG clear, slot flag 0x10, then debug_on, mode 3. Removed: ConfigVmsDisconnect.
 */
/* 0x00144890 */
void mcslot_update(McSlot *slot)
{
    int port;
    int port_slot;

    port = ulMcCardToPort(slot->port);
    port_slot = ulMcCardToSlot(slot->port);
    if (mc_is_connected(port, port_slot) != 0) {
        if (!(slot->flags & 1)) {
            slot->flags |= 1;
            ConfigVmsConnect(slot->port);
            slot->check = 0;
            slot->mode = 0;
        }
        switch (slot->mode) {
        case 0:
            slot->mode = 2;
            break;
        case 1:
            if (filereq_work.state != 1) {
                DEBUG_PRINTF("BackupDataBuf.stat = %d\n", filereq_work.state);
                filereq_work.state = 0;
                slot->check = 0;
                slot->mode++;
            }
            break;
        case 2:
            if (!(system_flags & SYS_DEBUG)) {
                if (slot->flags & 0x10) {
                    debug_on(slot->port);
                    slot->mode++;
                }
            }
            break;
        }
    } else if (slot->flags & 1) {
        slot->flags &= ~0x11U;
        if (slot->mode < 2) {
            slot->check = 0;
            DEBUG_PRINT("Still Check...\n");
            DEBUG_PRINTF("slot->mode_memory = %d\n", slot->mode);
            DEBUG_PRINTF("BackupDataBuf.stat = %d\n", filereq_work.state);
            filereq_work.state = 0;
        }
        ConfigVmsDisconnect(slot->port);
        debug_off(slot->port);
        slot->mode = 0;
    }
}

/* 0x00144a30 */
void mcslot_update_all(void)
{
    int i;
    McSlot *p;

    mc_poll();
    ulMcPoll();
    p = mc_slots;
    for (i = 0; i < 24; i++, p++)
        mcslot_update(p);
}

/* Per slot: number, port, flags as letters, state, device name. */
/* steer(cast) */
/* 0x00144a80 */
static void padslot_window_draw(int win)
{
    int cw;
    int ch;
    int y = 0;
    int i;
    char *obj;
    char *str;
    McSlot *slot;

    gw_get_cell_size(&cw, &ch);
    slot = mc_slots;
    for (i = 0; i < 24; i++, slot++) {
        obj = ((char *(*)(int))nj_pad_slot_info)(i);
        if (slot->id == -1) {
            str = bits_to_string(slot->flags, 5, "dVTLM", " ____");
            gw_win_printf(win, 4, y, "%2d   %s %s %32s", i, str, mcslot_state_names[slot->state], obj + 20);
        } else {
            str = bits_to_string(slot->flags, 5, "dVTLM", " ____");
            gw_win_printf(win, 4, y, "%2d %1d %s %s %32s", i, slot->id, str, mcslot_state_names[slot->state],
                         obj + 20);
        }
        y += ch;
    }
}

/* 0x00144bd0 */
static void padslot_menu_off(void)
{
    padslot_window = 0;
    menu_item_set_value(info_menu_name, "PadSlot", 0);
}

/* 53 x 24 characters, bottom right. */
/* 0x00144bf0 */
static void padslot_window_toggle(int item)
{
    int cw;
    int ch;
    int win;

    if (padslot_window) {
        gw_window_close(padslot_window);
        gw_item_set_value(item, 0);
        return;
    }
    gw_get_cell_size(&cw, &ch);
    win = gw_window_open("PadSlot", 95, (int)(screen_layout[7] - 2.0f - (float)(cw * 53)),
                     (int)(screen_layout[9] - 2.0f - (float)(ch * 24)), cw * 53, ch * 24, 0);
    padslot_window = win;
    if (win != 0) {
        gw_window_set_handlers(win, 0, padslot_window_draw, padslot_menu_off);
        gw_item_set_value(item, 1);
    }
}

/* 0x00144cf0 */
static void padslot_menu_add(void)
{
    menu_item_add(info_menu_name, "PadSlot", padslot_window_toggle, 0, 0);
}
