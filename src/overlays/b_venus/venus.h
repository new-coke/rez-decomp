/* Venus boss main task. */

#ifndef REZ_B_VENUS_VENUS_H
#define REZ_B_VENUS_VENUS_H

#include "types_overlays.h"

struct VenusWork;

void *module_entry(void *parent, void *arg);
int boss_main_demo_cb(BossMainWork *w);
void venus_exec_start(struct VenusWork *w);
void venus_init(struct VenusWork *w, int *arg);
void venus_start(struct VenusWork *w, int mode);
void venus_exec_load(struct VenusWork *w);
void venus_release(struct VenusWork *w);
void venus_pre_exec(Earth1stDemoWork *w);
void venus_get_pos_ang(struct VenusWork *w, void **pos_out, void **ang_out);
void venus_seq_update(struct VenusWork *w);
void venus_exec(struct VenusWork *w);
void venus_disp(struct VenusWork *w);
void venus_disp_final(struct VenusWork *w);
void *venus_create(void *parent, void *arg);
void venus_task_finish_if(void *w);
int venus_level_add(int i);
void venus_player_damage(void);
void venus_battle_init(struct VenusWork *w);
void venus_seq_wait_beat(struct VenusWork *w);
void venus_seq_regist(void *w, int idx, int id);
void venus_seq_step(int work);
void venus_player_stop_cb(struct VenusWork *w, int k);
void venus_debug_keys(struct VenusWork *w);
void venus_debug_disp(void *w);
void venus_rotate_yxz(int *ang);

#endif /* REZ_B_VENUS_VENUS_H */
