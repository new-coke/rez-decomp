/* Dominions head, Area 5. */

#ifndef REZ_STG05_DOMINIONS_HEAD_CARRIER_H
#define REZ_STG05_DOMINIONS_HEAD_CARRIER_H

#include "types_common.h"
#include "stg05.h"
#include "types_overlays.h"

struct DominionsHeadCarrierWork;
struct DominionsHeadCarrierKind;

void dominions_head_carrier_vars_register(void);
void dominions_head_carrier_vars_unregister(void);
int dominions_head_carrier_vars_init(int *prm);
int dominions_head_carrier_vars_release(void);
int dominions_head_carrier_get_init_arg(void);
int dominions_head_carrier_tail_raised(void *obj, struct DominionsHeadCarrierWork *w);
void dominions_head_carrier_path_eval(void *out, int idx, float rate);
void dominions_head_carrier_forward(int *ang, Vec3 *out, float len);
void dominions_head_carrier_calc_tip(void *obj, struct DominionsHeadCarrierWork *w);
void dominions_head_carrier_face(Vec3 *to, Vec3 *from, int *ang);
void dominions_head_carrier_face_move(void *obj, int work, void *p);
void dominions_head_carrier_init(void *obj, struct DominionsHeadCarrierWork *w, void *place, struct DominionsHeadCarrierKind *k);
void dominions_head_carrier_release(void *obj, struct DominionsHeadCarrierWork *w);
int dominions_head_carrier_state_enter(void *obj, struct DominionsHeadCarrierWork *w);
int dominions_head_carrier_state_course(void *obj, struct DominionsHeadCarrierWork *w);
int dominions_head_carrier_state_course_turn(void *obj, struct DominionsHeadCarrierWork *w);
int dominions_head_carrier_state_coast(void *obj, struct DominionsHeadCarrierWork *w);
void dominions_head_carrier_update(void *obj, TypeEnemyWork *w);
void dominions_head_carrier_exec_appear(void *obj, struct DominionsHeadCarrierWork *w);
void dominions_head_carrier_exec_normal(void *obj, struct DominionsHeadCarrierWork *w);
void dominions_head_carrier_exec_leave(void *obj, TypeEnemyWork *w);
void dominions_head_carrier_exec_destroyed(void *obj, struct DominionsHeadCarrierWork *w);
void dominions_head_carrier_pre_exec(void *obj);
void dominions_head_carrier_exec(void *obj, TypeEnemyWork *w);
void dominions_head_carrier_post_exec(void *obj, struct DominionsHeadCarrierWork *w);
void dominions_head_carrier_disp(void *obj, TypeEnemyWork *w);
void *dominions_head_carrier_create(void *obj, ShotArg *sarg, int n, int seq, int mode, int k2c, int launch_b, int wait, int *timer, float spd);
int dominions_head_carrier_is_done(void *obj);
void dominions_head_carrier_rotate_yxz(int *ang);

#endif /* REZ_STG05_DOMINIONS_HEAD_CARRIER_H */
