/* Dominions body and tail, Area 5. */

#ifndef REZ_STG05_DOMINIONS_BODY_AND_TAIL_H
#define REZ_STG05_DOMINIONS_BODY_AND_TAIL_H

#include "types_common.h"
#include "types_overlays.h"
#include "stg05.h"

struct DominionsBodyAndTailWork;
struct DominionsBodyAndTailKind;

void dominions_body_and_tail_vars_register(void);
void dominions_body_and_tail_vars_unregister(void);
int dominions_body_and_tail_vars_init(int *prm);
int dominions_body_and_tail_vars_release(void);
void dominions_body_and_tail_forward(int *ang, Vec3 *out, float len);
void dominions_body_and_tail_calc_tips(void *obj, void *work);
void dominions_body_and_tail_init(void *obj, struct DominionsBodyAndTailWork *w, ShotArg *place, struct DominionsBodyAndTailKind *k);
void dominions_body_and_tail_release(void *obj, struct DominionsBodyAndTailWork *w);
void dominions_body_and_tail_aim(void *obj, int *w);
void dominions_body_and_tail_lock_side_test(void *obj, void *work);
void dominions_body_and_tail_launcher_update(void *obj, struct DominionsBodyAndTailWork *w);
void dominions_body_and_tail_follow_dir(void *obj, void *work);
void dominions_body_and_tail_exec_appear(void *obj, struct DominionsBodyAndTailWork *w);
void dominions_body_and_tail_exec_normal(void *obj, struct DominionsBodyAndTailWork *w);
int dominions_body_and_tail_exec_leave(void *obj);
void dominions_body_and_tail_exec_destroyed(void *obj, struct DominionsBodyAndTailWork *w);
void dominions_body_and_tail_exec(void *obj, TypeEnemyWork *w);
void dominions_body_and_tail_pre_exec(void *obj);
void dominions_body_and_tail_post_exec(void *obj, struct DominionsBodyAndTailWork *w);
void dominions_body_and_tail_disp_body(void *obj, struct DominionsBodyAndTailWork *w);
void dominions_body_and_tail_disp(void *obj, TypeEnemyWork *w);
int dominions_body_and_tail_create(void *obj, DominionsLinkArg *prm, int n, int life, int i, int mode, int k1c, int launch_b, int wait, int tail, int sel);
void dominions_body_and_tail_chain_destroy(void *obj, int v, int step);
void dominions_body_and_tail_chain_end(void *obj);
void dominions_body_and_tail_chain_set_pair(void *obj, float *blend, float *ang_rate);
void dominions_body_and_tail_chain_update(void *obj);
void dominions_body_and_tail_chain_last_pose(void *obj, Vec3 *pos, int *ang);
void dominions_body_and_tail_chain_place(void *obj, Vec3 *pos, int *ang);
void dominions_body_and_tail_chain_set_rate(void *obj, float *v);
void dominions_body_and_tail_rotate_yxz(int *ang);

#endif /* REZ_STG05_DOMINIONS_BODY_AND_TAIL_H */
