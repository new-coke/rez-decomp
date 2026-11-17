/* Controller and keyboard input: ports, button records, owners. */

#ifndef REZ_PAD_H
#define REZ_PAD_H

/*
 * Game button bits (pad_decode_packet): the Dreamcast controller's layout, with the PS2 pad mapped
 * onto it. The pad debug window labels them "lrrlduDXYZRLDUSABC", bit 17 down to bit 0.
 */
#define PAD_B           0x2         /* Circle */
#define PAD_A           0x4         /* Cross */
#define PAD_START       0x8
#define PAD_UP          0x10
#define PAD_DOWN        0x20
#define PAD_LEFT        0x40
#define PAD_RIGHT       0x80
#define PAD_Z           0x100       /* Select */
#define PAD_Y           0x200       /* Triangle */
#define PAD_X           0x400       /* Square */
#define PAD_L2          0x1000
#define PAD_L3          0x2000
#define PAD_R2          0x4000
#define PAD_R3          0x8000
#define PAD_R1          0x10000     /* the Dreamcast's right trigger */
#define PAD_L1          0x20000     /* the Dreamcast's left trigger */

#include "types_game.h"

struct PadKeyRecView;
struct PadOwnerView;

extern unsigned int pad_cancel_mask;

void pad_info_callback(int id, int status);
void pad_status_callback(int id);
void pad_init(void);
void pad_module_init(void);
void pad_listener_remove(void);
void pad_history_push(void);
void pad_state_record_clear(struct PadKeyRecView *rec);
int pad_get_type_class(int port);
int pad_is_connected(int port);
int pad_id_is_ready(int i);
void pad_ports_update(void);
void pad_ports_assign(void);
int pad_keyboard_enabled(void);
void pad_update(void);
void peripherals_update(void);
void pad_button_repeat_update(char *cnt, PadButtonsView *rec);
void pad_button_repeat_update_alt(char *cnt, PadButtonsView *rec);
void pad_input_enable(int port);
void pad_input_disable(int port);
void pad_trigger_enable(int port);
void pad_trigger_disable(int port);
int pad_button_combine(PadButtonsView *rec, int val, unsigned int mask);
void pad_port_update(int sys, int *data);
struct PadOwnerView *pad_port_owner_find(struct PadOwnerView *owner, int key);
void pad_owner_add(int port, int key);
void pad_port_owner_remove(int port, int key);
void pad_port_merge_owned(int port, int key);
void pad_port_clear_owned(int port, int key);
void pad_state_restore(void);
struct PadOwnerView *pad_state_owner_find(struct PadOwnerView *owner, int key);
void pad_state_owner_add(int key);
void pad_owner_remove(int key);
void pad_state_restore_owned(int key);
void pad_state_clear_owned(int key);
void pad_keyboard_update(void);
void pad_keyboard_translate(void);
int pad_keyboard_key_down(int key);
int pad_keyboard_modifiers_match(int mask);
int pad_button_assign_get(int port, int slot);
void pad_button_assign_set(int port, int slot, int mask);
void pad_debug_window_draw(int win);
void pad_debug_enable(int win);
void pad_debug_disable(void);
void pad_debug_window_toggle(void);
void pad_debug_menu_init(void);

#endif /* REZ_PAD_H */
