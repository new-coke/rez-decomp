/* Dominions shooter head, Area 5. */

#ifndef REZ_STG05_DOMINIONS_HEAD_SHOOTER_H
#define REZ_STG05_DOMINIONS_HEAD_SHOOTER_H

#include "types_common.h"
#include "stg05.h"

struct DominionsHeadShooterKind;

void dominions_head_shooter_vars_register(void);
void dominions_head_shooter_vars_unregister(void);
int dominions_head_shooter_vars_init(int *prm);
int dominions_head_shooter_vars_release(void);
int dominions_head_shooter_walk_nodes(Vec3 **pt_out, int **ap, unsigned int *node, int n);
void dominions_head_shooter_init(void *obj, DominionsHeadShooterWork *w, void *place, struct DominionsHeadShooterKind *k);
void dominions_head_shooter_release(void *obj, DominionsHeadShooterWork *w);
void dominions_head_shooter_chain_wave(void *obj, DominionsHeadShooterWork *w, float rate);
int dominions_head_shooter_next_state(void *obj, DominionsHeadShooterWork *w, int n, int state);
int dominions_head_shooter_play_motion(void *obj, DominionsHeadShooterWork *w, int n, int state, float frm_start, float frm_end);
int dominions_head_shooter_state_rise(void *obj, DominionsHeadShooterWork *w);
int dominions_head_shooter_state_follow(void *obj, DominionsHeadShooterWork *w);
int dominions_head_shooter_state_wave(void *obj, DominionsHeadShooterWork *w);
int dominions_head_shooter_state_wave_end(void *obj, DominionsHeadShooterWork *w);
int dominions_head_shooter_state_aim(void *obj, DominionsHeadShooterWork *w);
int dominions_head_shooter_state_fire(void *obj, DominionsHeadShooterWork *w);
int dominions_head_shooter_state_retract(void *obj, DominionsHeadShooterWork *w);
void dominions_head_shooter_update(void *obj, TypeEnemyWork *work);
void dominions_head_shooter_exec_appear(void *obj, TypeEnemyWork *w);
void dominions_head_shooter_exec_normal(void *obj, TypeEnemyWork *w);
void dominions_head_shooter_exec_leave(void *w);
void dominions_head_shooter_exec_destroyed(void *obj, DominionsHeadShooterWork *w);
void dominions_head_shooter_pre_exec(void *obj);
void dominions_head_shooter_exec(void *obj, TypeEnemyWork *w);
void dominions_head_shooter_post_exec(void *obj, DominionsHeadShooterWork *w);
void dominions_head_shooter_disp(void *obj, TypeEnemyWork *w);
int dominions_head_shooter_create(void *parent, EnDominionsHoleArg *prm, int *timer);
void dominions_head_shooter_rotate_yxz(int *ang);

#endif /* REZ_STG05_DOMINIONS_HEAD_SHOOTER_H */
