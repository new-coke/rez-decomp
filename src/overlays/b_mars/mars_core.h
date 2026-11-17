/* Mars boss core and flower. */

#ifndef REZ_B_MARS_MARS_CORE_H
#define REZ_B_MARS_MARS_CORE_H

#include "types_overlays.h"
#include "types_common.h"
#include "b_mars.h"

struct MarsCNode;
struct MarsCoreWork;
struct MarsFlowerWork;
struct MarsFlSlot;
struct Mars1stDemoWork;

struct MarsCNode *mars_c_nodes_init(struct MarsCNode *p, int n);
void mars_c_init(struct MarsCoreWork *w, MarsCoreArg *arg);
void mars_c_to_world(struct MarsCoreWork *w, float *in, float *out);
void mars_c_motion_step(struct MarsCoreWork *w);
void mars_c_move(struct MarsCoreWork *w);
int mars_c_random_kind(void);
void mars_c_exec(struct MarsCoreWork *w);
void mars_c_disp(struct MarsCoreWork *w);
void mars_c_release(struct MarsCoreWork *w);
struct MarsCoreWork *mars_c_create(void *parent, MarsCoreArg *arg);
int mars_c_get_state(struct MarsCoreWork *w);
void mars_c_set_state(struct MarsCoreWork *w, int state);
void mars_c_trail_start(struct MarsCoreWork *w, int mode);
int mars_c_life_level(struct MarsCoreWork *w);
void *mars_c_get_pos(struct MarsCoreWork *w);
void mars_c_get_pose(struct MarsCoreWork *w, void **pos_out, void **ang_out);
void mars_c_pulse(struct MarsCoreWork *w);
void mars_c_rotate_yxz(int *ang);
void mars_demo_vars_add(void);
void mars_demo_vars_remove(void);
void mars_1st_demo_exec(struct Mars1stDemoWork *w);
void mars_1st_demo_release(struct Mars1stDemoWork *w);
int *mars_1st_demo_create(void *parent, int cb, int arg);
void tree_hide_depth_r(WireObj *obj, int *depth, int lim);
void tree_hide_depth(WireObj *obj, int lim);
int mars_fl_slots_busy(struct MarsFlowerWork *w);
void mars_fl_locks_set(struct MarsFlowerWork *w, int on);
void mars_fl_slots_copy_part(struct MarsFlowerWork *w);
void mars_fl_slot_motion(void *wp, int i, int mode, int arg);
void mars_fl_slots_fade(struct MarsFlowerWork *w);
void mars_fl_slots_sway(struct MarsFlowerWork *w);
void mars_fl_slot_aim(struct MarsFlSlot *slot);
void mars_fl_fire(struct MarsFlowerWork *w);
void mars_fl_pose(struct MarsFlowerWork *w);
int mars_fl_slots_state(struct MarsFlowerWork *w);
int mars_fl_hit_check(struct MarsFlowerWork *w);
void mars_fl_slot_start(struct MarsFlowerWork *w, struct MarsFlSlot *slot);
int mars_fl_slot_try(struct MarsFlowerWork *w, int start);
void mars_fl_attack_step(struct MarsFlowerWork *w);
void mars_fl_break_step(struct MarsFlowerWork *w);
void mars_fl_exec(struct MarsFlowerWork *w);
void mars_fl_release(struct MarsFlowerWork *w);
void mars_fl_disp(char *w);
struct MarsFlowerWork *mars_fl_create(void *parent, int *src, int start_mot, int *mask, int mode);
int mars_fl_any_busy(struct MarsFlowerWork *w);
void mars_fl_command(struct MarsFlowerWork *w, int cmd);
void mars_fl_rotate_yxz(int *ang);
void mars_score_vars_add(void);
void mars_score_vars_remove(void);
void mars_map_exec(int *w);
void mars_map_release(void);
int mars_map_create(char *w, int arg);
void mars_map_set_visible(char *w, int on);
void boss_se_play(int id, int lock, int pos);
void boss_vol_play(int id, int arg);
void boss_vol_stop(int id);
void boss_bgm_exit_cb(BgmMan *bgm);
BgmMan *boss_bgm_start(void *parent);
void boss_adx_request(void);
void boss_bgm_switch(int cb, int cb_arg, int idx);
void boss_bgm_seq_get(int i, int *seq_a, int *seq_b);
void boss_pcm_load(int i);
void boss_pcm_start(void);

#endif /* REZ_B_MARS_MARS_CORE_H */
