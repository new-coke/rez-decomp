/* Vishnu, Area 2 enemy. */

#ifndef REZ_STG02_EN_VISHNU_H
#define REZ_STG02_EN_VISHNU_H

#include "types_common.h"

struct EnVishnuWork;

void en_vishnu_vars_register(void);
void en_vishnu_vars_unregister(void);
int en_vishnu_marker_draw(int obj, void *w);
int en_vishnu_vars_init(int *prm);
int en_vishnu_vars_release(void);
void en_vishnu_place_vec(int *ang, Vec3 *out);
void en_vishnu_shuffle_hands(void *obj, struct EnVishnuWork *w);
float en_vishnu_hand_open_frm(void *obj, struct EnVishnuWork *w, int i);
float en_vishnu_hand_flash_frm(void *obj, struct EnVishnuWork *w, int i);
void en_vishnu_calc_hand_points(void *obj, int *work);
void en_vishnu_calc_lock_point(void *obj, int *work);
void en_vishnu_calc_points(void *obj, void *work);
void en_vishnu_swing(void *obj, struct EnVishnuWork *w);
void en_vishnu_hit_check(void *obj, int *work);
void en_vishnu_deploy_hand(void *obj, struct EnVishnuWork *w, int i, Vec3 *ofs);
void en_vishnu_state_deploy(void *obj, struct EnVishnuWork *w);
void en_vishnu_state_drop_hands(void *obj, struct EnVishnuWork *w);
void en_vishnu_state_crab_walk(void *obj, struct EnVishnuWork *w);
void en_vishnu_state_escape(void *obj, struct EnVishnuWork *w);
void en_vishnu_exec_normal(void *obj, struct EnVishnuWork *w);
void en_vishnu_exec_destroyed(void *obj, void *w);
void en_vishnu_exec(void *obj, void *w);
void en_vishnu_pre_exec(void *obj, int *w);
void en_vishnu_post_exec(void *obj, void *w);
void en_vishnu_disp_glow(void *obj, struct EnVishnuWork *w);
void en_vishnu_disp_body(void *obj, struct EnVishnuWork *w);
void en_vishnu_disp(void *obj, struct EnVishnuWork *w);
void en_vishnu_init(void *obj, struct EnVishnuWork *w, char *place);
void en_vishnu_release(void *obj, char *w);
int en_vishnu_create(void *parent, void *place);
void en_vishnu_nodes_init(void);
void en_vishnu_get_hand_pos(void *obj, int i, Vec3 *out);
void en_vishnu_hand_gone(void *obj, int idx);
int en_vishnu_count_hands(void *obj);
void en_vishnu_rotate_yxz(int *ang);

#endif /* REZ_STG02_EN_VISHNU_H */
