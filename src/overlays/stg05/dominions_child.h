/* Dominions parts, Area 5. */

#ifndef REZ_STG05_DOMINIONS_CHILD_H
#define REZ_STG05_DOMINIONS_CHILD_H

#include "stg05.h"
#include "types_overlays.h"

struct DominionsChildWork;
struct DominionsChildKind;
struct PosAngArg;
struct EnDominionsHoleWork;

void dominions_child_vars_register(void);
void dominions_child_vars_unregister(void);
int dominions_child_get_init_arg(void);
int dominions_child_vars_init(int *prm);
int dominions_child_vars_release(void);
void dominions_child_eval_pass(void *obj, struct DominionsChildWork *w, void *out);
void dominions_child_init(void *obj, struct DominionsChildWork *w, void *prm, struct DominionsChildKind *k);
void dominions_child_release(void);
int dominions_child_next_state(void *obj, struct DominionsChildWork *w, int n, int state);
int dominions_child_state_start(void *obj, struct PosAngArg *prm);
int dominions_child_state_launch(void *obj, struct DominionsChildWork *w);
int dominions_child_state_join(void *obj, struct DominionsChildWork *w);
int dominions_child_state_follow(void *obj, struct DominionsChildWork *w);
int dominions_child_state_stop(void *obj, struct DominionsChildWork *w);
int dominions_child_state_leave(void *obj, struct DominionsChildWork *w);
int dominions_child_state_wait(void *obj, struct DominionsChildWork *w);
void dominions_child_update(void *obj, TypeEnemyWork *work);
void dominions_child_exec_appear(void *obj, TypeEnemyWork *w);
void dominions_child_exec_normal(void *obj, TypeEnemyWork *w);
void dominions_child_exec_leave(void *obj, struct DominionsChildWork *w);
void dominions_child_exec_destroyed(void *obj, struct DominionsChildWork *w);
void dominions_child_pre_exec(void *obj);
void dominions_child_exec(void *obj, TypeEnemyWork *w);
void dominions_child_post_exec(void *obj, struct DominionsChildWork *w);
void dominions_child_disp(void *obj, TypeEnemyWork *w);
void *dominions_child_create(void *obj, ShotArg *sarg, DominionsChildLaunch *launch);
void dominions_child_launch(void *obj);
void dominions_child_destroy(void *obj);
void en_dominions_hole_vars_register(void);
void en_dominions_hole_vars_unregister(void);
int en_dominions_hole_vars_init(int *prm);
int en_dominions_hole_vars_release(void);
int en_dominions_hole_marker_draw(void *w, EnDominionsHoleArg *prm);
void cmd_enemy_kill_all(void);
int en_dominions_hole_get_init_arg(void);
void en_dominions_hole_init(struct EnDominionsHoleWork *w, EnDominionsHoleArg *prm);
void en_dominions_hole_release(struct EnDominionsHoleWork *w);
void en_dominions_hole_exec_close(struct EnDominionsHoleWork *w);
void en_dominions_hole_rewind(struct EnDominionsHoleWork *w);
void en_dominions_hole_exec_open(struct EnDominionsHoleWork *w);
void en_dominions_hole_exec(struct EnDominionsHoleWork *w);
void en_dominions_hole_disp(struct EnDominionsHoleWork *w);
void *en_dominions_hole_create(void *parent, void *prm);
void en_dominions_hole_rotate_yxz(int *ang);

#endif /* REZ_STG05_DOMINIONS_CHILD_H */
