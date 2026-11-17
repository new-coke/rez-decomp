/* Tiamat's body and tail. */

#ifndef REZ_COMMON_TIAMAT_BODY_AND_TAIL_H
#define REZ_COMMON_TIAMAT_BODY_AND_TAIL_H

#include "types_overlays.h"
#include "types_common.h"

void tiamat_body_and_tail_vars_register(void);
void tiamat_body_and_tail_vars_unregister(void);
int tiamat_body_and_tail_vars_init(int *fx_id);
int tiamat_body_and_tail_vars_release(void);
void tiamat_body_and_tail_tip_pos(void *obj, TiamatBodyAndTailWork *w);
void tiamat_body_and_tail_init(void *obj, TiamatBodyAndTailWork *w, char *flag, int *kind);
void tiamat_body_and_tail_release(void *obj, TiamatBodyAndTailWork *w);
void tiamat_body_and_tail_aim(void *obj, TiamatBodyAndTailWork *w);
void tiamat_body_and_tail_forward(void *obj, int w, Vec3 *out);
void tiamat_body_and_tail_steer(void *obj, TiamatBodyAndTailWork *w, Vec3 *v);
void tiamat_body_and_tail_swing_offset(void *obj, TiamatBodyAndTailWork *w, Vec3 *out);
void tiamat_body_and_tail_swing_dir(void *obj, TiamatBodyAndTailWork *w);
void tiamat_body_and_tail_swing_start(void *obj, TiamatBodyAndTailWork *w);
void tiamat_body_and_tail_damage_swing(void *obj, TiamatBodyAndTailWork *w);
void tiamat_body_and_tail_follow(void *obj, TiamatBodyAndTailWork *w);
void tiamat_body_and_tail_exec_appear(void *obj, TiamatBodyAndTailWork *w);
void tiamat_body_and_tail_exec_normal(void *obj, TiamatBodyAndTailWork *w);
void tiamat_body_and_tail_exec_leave(void *obj, TiamatBodyAndTailWork *w);
void tiamat_body_and_tail_exec_destroyed(void *obj, TiamatBodyAndTailWork *w);
void tiamat_body_and_tail_exec(void *obj, TiamatBodyAndTailWork *w);
void tiamat_body_and_tail_pre_exec(void *obj);
void tiamat_body_and_tail_post_exec(void *obj, TiamatBodyAndTailWork *w);
int tiamat_body_and_tail_collect_node_matrices(float **out, unsigned int *node, int idx);
void tiamat_body_and_tail_draw_at_nodes(ModelPart *mdl, ModelPart *src, float zscl);
void tiamat_body_and_tail_disp_body(void *obj, TiamatBodyAndTailWork *w);
void tiamat_body_and_tail_disp(void *obj, TiamatBodyAndTailWork *w);
int tiamat_body_and_tail_create(void *head, int lnk, int seg, int frames, char *flag, float phase, float phase_step);
void tiamat_body_and_tail_chain_destroy(void *obj, int delay, int step);
void tiamat_body_and_tail_chain_set_points(void *obj, Vec3 *points, int count);
int tiamat_body_and_tail_chain_count_from(void *obj, int n);
int tiamat_body_and_tail_chain_count(void *w);
void tiamat_body_and_tail_chain_set_rate(void *obj, float *rate);
void tiamat_body_and_tail_chain_swing_start(void *obj);
void tiamat_body_and_tail_chain_set_swing_rate(void *obj, float *rate);
void tiamat_body_and_tail_chain_update(void *obj);
void tiamat_body_and_tail_rotate_yxz(int *ang);

#endif /* REZ_COMMON_TIAMAT_BODY_AND_TAIL_H */
