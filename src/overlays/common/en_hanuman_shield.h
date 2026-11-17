/* Hanuman's shield. */

#ifndef REZ_COMMON_EN_HANUMAN_SHIELD_H
#define REZ_COMMON_EN_HANUMAN_SHIELD_H

#include "types_overlays.h"
#include "types_common.h"

void en_hanuman_shield_vars_register(void);
void en_hanuman_shield_vars_unregister(void);
int en_hanuman_shield_vars_init(int *fx_id);
int en_hanuman_shield_vars_release(void);
void en_hanuman_shield_calc_parts(void *obj, int *work);
void en_hanuman_shield_calc_parts_once(void *obj, int *w);
void en_hanuman_shield_uv_scroll(void *obj, int *work);
void en_hanuman_shield_part_shatter(void *obj, int *work, int i);
void en_hanuman_shield_shatter_all(void *obj, int *w);
void en_hanuman_shield_spawn_piece(void *obj, EnHanumanShieldWork *w, int k, Vec3 *v);
void en_hanuman_shield_hit_check(void *obj, EnHanumanShieldWork *w);
void en_hanuman_shield_crash_check(void *obj, EnHanumanShieldWork *w);
void en_hanuman_shield_state_appear(void *obj, EnHanumanShieldWork *w);
void en_hanuman_shield_state_attack(void *obj, EnHanumanShieldWork *w);
void en_hanuman_shield_exec_normal(void *obj, EnHanumanShieldWork *w);
void en_hanuman_shield_exec_destroyed(void *obj, void *w);
void en_hanuman_shield_exec(void *obj, void *w);
void en_hanuman_shield_pre_exec(void *obj, EnHanumanShieldWork *w);
void en_hanuman_shield_post_exec(void *obj, int *w);
void en_hanuman_shield_toggle_parts(void *obj, int *w);
void en_hanuman_shield_toggle_cores(void *obj, int *w);
void en_hanuman_shield_disp_body(void *obj, EnHanumanShieldWork *w);
void en_hanuman_shield_disp(void *obj, void *w);
void en_hanuman_shield_init(void *obj, EnHanumanShieldWork *w);
void en_hanuman_shield_release(void *obj, EnHanumanShieldWork *w);
int en_hanuman_shield_create(void *parent, EnHanumanShieldArg *prm, void *owner, int dir_set);
void en_hanuman_shield_nodes_init(void);
void en_hanuman_shield_rotate_yxz(int *ang);

#endif /* REZ_COMMON_EN_HANUMAN_SHIELD_H */
