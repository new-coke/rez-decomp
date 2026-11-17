/* Vishnu's hands, Area 2. */

#ifndef REZ_STG02_EN_VISHNU_HAND_H
#define REZ_STG02_EN_VISHNU_HAND_H

#include "stg02.h"
#include "types_common.h"

void en_vishnu_hand_vars_register(void);
void en_vishnu_hand_vars_unregister(void);
int en_vishnu_hand_vars_init(int *prm);
void en_vishnu_hand_calc_lock_pos(EnVishnuHandWork *w);
void en_vishnu_hand_calc_flash_pos(EnVishnuHandWork *w);
void en_vishnu_hand_calc_string_pos(EnVishnuHandWork *w);
void en_vishnu_hand_calc_all(EnVishnuHandWork *w);
void en_vishnu_hand_calc_points(EnVishnuHandWork *w);
void en_vishnu_hand_swing(EnVishnuHandWork *w);
void en_vishnu_hand_shatter(EnVishnuHandWork *w);
void en_vishnu_hand_hit_check(EnVishnuHandWork *w);
int en_vishnu_hand_is_far(EnVishnuHandWork *w);
void en_vishnu_hand_tether(EnVishnuHandWork *w);
void en_vishnu_hand_aim(EnVishnuHandWork *w);
void en_vishnu_hand_fire(EnVishnuHandWork *w);
void en_vishnu_hand_state_wait(EnVishnuHandWork *w);
void en_vishnu_hand_state_open(EnVishnuHandWork *w);
void en_vishnu_hand_state_charge(EnVishnuHandWork *w);
void en_vishnu_hand_state_fall(EnVishnuHandWork *w);
void en_vishnu_hand_exec(EnVishnuHandWork *w);
void en_vishnu_hand_disp(EnVishnuHandWork *w);
void en_vishnu_hand_init(EnVishnuHandWork *w, int idx, Vec3 *pos, int *ang, int vel, float open_frm, float flash_frm);
void en_vishnu_hand_release(EnVishnuHandWork *w);
EnVishnuHandWork *en_vishnu_hand_create(void *obj, int idx, Vec3 *pos, int *ang, int vel, float open_frm, float flash_frm);
void en_vishnu_hand_nodes_init(void);
void en_vishnu_hand_get_muzzle(EnVishnuHandWork *w, Vec3 *pos, int *ang);
void en_vishnu_hand_get_string_pos(char *w, Vec3 *out);
void en_vishnu_hand_destroy(EnVishnuHandWork *w);
void en_vishnu_hand_detach(EnVishnuHandWork *w);
void en_vishnu_hand_finish(void *w);
void en_vishnu_hand_move_dir(EnVishnuHandWork *w, Vec3 *dir);
int en_vishnu_hand_count_others(int *w);
void en_vishnu_hand_rotate_yxz(int *ang);

#endif /* REZ_STG02_EN_VISHNU_HAND_H */
