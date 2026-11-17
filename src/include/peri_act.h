/* Vibration player: records, action packs, queued tables. */

#ifndef REZ_PERI_ACT_H
#define REZ_PERI_ACT_H

struct PeriNode;
struct PeriAcc;
struct PeriRec;
struct PeriRec;
struct PeriRec;
struct PeriPack;
struct PeriPack;
struct PeriList;
struct EvRef;

void peri_act_close_all(void);
void peri_act_reset_slot(int port, int slot);
void peri_act_update_all(void);
struct PeriPack *peri_act_pack_load(void *name);
void peri_act_pack_free(int *pk);
void peri_act_play_pack_entry(struct PeriPack *pack, int idx, int port, int slot);
void peri_act_queue_table(int port, int slot, int *data);
void peri_act_destroy_all(void);

#endif /* REZ_PERI_ACT_H */
