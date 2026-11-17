/* Earth boss arm: segmented tentacles. */

#ifndef REZ_B_EARTH_EARTH_ARM_H
#define REZ_B_EARTH_EARTH_ARM_H

#include "types_common.h"
#include "types_overlays.h"
#include "b_earth.h"

void arm_collect_models(WireObj *obj, NjCnkModel **out, int *n);
void arm_slots_clear(void);
int *arm_slot_alloc(int size);
void arm_slot_free(int *data);
BossArmNode *arm_seg_append(BossArmSeg *seg);
void arm_seg_remove(BossArmWork *arm, BossArmSeg *seg, int mode);
BossArmNode *arm_seg_node(BossArmSeg *seg, int i);
void arm_init(BossArmWork *arm, BossArmArg *arg);
void arm_release(BossArmWork *arm);
void arm_reinit(BossArmWork *arm, BossArmArg *arg);
void arm_seg_lock_set(BossArmWork *arm, BossArmSeg *seg, int on);
void arm_seg_node_tips(BossArmSeg *seg);
void arm_seg_place_head(Vec3 *pos, int *ang, BossArmSeg *seg);
void arm_seg_scroll(BossArmSeg *seg);
void arm_seg_swing(BossArmSeg *seg);
void arm_seg_thrust(BossArmSeg *seg);
void arm_node_hit(BossArmSeg *seg, BossArmNode *n, Vec3 *v);
void arm_seg_shake(BossArmSeg *seg);
void arm_seg_retire(BossArmSeg *seg);
void arm_seg_drop(BossArmWork *arm, BossArmSeg *seg);
int arm_hit_check(BossArmWork *arm);
void arm_seg_sweep_start(BossArmSeg *seg, Vec3 *center, int n, float radius, float move_secs, float wait_secs, float slot_ofs);
void arm_seg_sweep(BossArmSeg *seg, Vec3 *pos, float *mtx);
void arm_command(BossArmWork *arm);
int arm_seg_wave_start(BossArmSeg *seg, int period, int steps);
void arm_seg_wave(BossArmWork *arm, BossArmSeg *seg);
void arm_seg_turn_nodes(BossArmSeg *seg);
void arm_wobble_shift(BossArmWork *arm, signed char *buf);
void arm_wobble_update(BossArmWork *arm);
void arm_seg_apply_wobble(BossArmWork *arm, BossArmSeg *seg);
void arm_segs_update(BossArmWork *arm, int k);
void arm_seg_launch(BossArmWork *arm, BossArmSeg *seg, Vec3 *v, int n);
void arm_launch_step(BossArmWork *arm);
void arm_exec(BossArmWork *arm);
void arm_disp(BossArmWork *arm);
BossArmWork *arm_create(void *parent, BossArmArg *arg);
void arm_enable(BossArmWork *arm);
void arm_seg_set_carried(BossArmWork *arm, BossArmSeg *seg, int state, int relock);
int arm_attach_cube(BossArmWork *arm, int *pos_out, int *ang_out, int *scale_out, int *color, BossCube *cube, int *flags_out);
void arm_rotate_yxz(int *ang);

#endif /* REZ_B_EARTH_EARTH_ARM_H */
