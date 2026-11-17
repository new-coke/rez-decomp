/* Peripheral slots, memory-card save registration. */

#ifndef REZ_PADSLOT_H
#define REZ_PADSLOT_H

struct McSlot;

extern const int mc_drive_slots[];

void backup_setup(void);
void pad_driver_init(void);
void mcslot_set_port(int port, int first);
void mcslot_reset_port(int port, int idx);
int mcslot_is_busy(int i);
void mcslot_update(struct McSlot *slot);
void mcslot_update_all(void);

#endif /* REZ_PADSLOT_H */
