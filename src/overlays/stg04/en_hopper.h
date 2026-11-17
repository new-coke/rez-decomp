/* Hopper, Area 4 enemy. */

#ifndef REZ_STG04_EN_HOPPER_H
#define REZ_STG04_EN_HOPPER_H

#include "types_common.h"
#include "stg04.h"

struct EnHopperWork;
struct EnHopperReleaseWork;

void en_hopper_vars_register(void);
void en_hopper_vars_unregister(void);
int en_hopper_marker_draw(int obj, void *place);
int en_hopper_vars_init(int *se);
int en_hopper_vars_release(void);
void en_hopper_calc_base(void *obj, struct EnHopperWork *w);
void en_hopper_calc_muzzle(void *obj, struct EnHopperWork *w);
void en_hopper_calc_points(void *obj, struct EnHopperWork *w);
void en_hopper_hit_check(void *obj, struct EnHopperWork *w);
void en_hopper_swing(void *obj, struct EnHopperWork *w);
void en_hopper_fire_flash(void *obj, struct EnHopperWork *w);
void en_hopper_fire_shot(void *obj, struct EnHopperWork *w);
void en_hopper_pick_move(void *obj, struct EnHopperWork *w, Vec3 *mov);
void en_hopper_jump_curve(struct EnHopperWork *w, Vec3 *out, float t);
void en_hopper_jump_start(void *obj, struct EnHopperWork *w, float height);
void en_hopper_place_first(void *obj, struct EnHopperWork *w);
void en_hopper_land(void *obj, struct EnHopperWork *w);
void en_hopper_jump_update(void *obj, struct EnHopperWork *w);
void en_hopper_state_first_jump(void *obj, struct EnHopperWork *w);
void en_hopper_state_jump(void *obj, struct EnHopperWork *w);
void en_hopper_state_attack(void *obj, struct EnHopperWork *w);
void en_hopper_exec_normal(void *obj, struct EnHopperWork *w);
void en_hopper_exec_destroyed(void *obj, struct EnHopperWork *w);
void en_hopper_exec(void *obj, void *w);
void en_hopper_pre_exec(void *obj, struct EnHopperWork *w);
void en_hopper_post_exec(void *obj, struct EnHopperWork *w);
void en_hopper_disp_body(void *obj, struct EnHopperWork *w);
void en_hopper_disp(void *obj, struct EnHopperWork *w);
void en_hopper_init(void *obj, struct EnHopperWork *w);
void en_hopper_release(void *obj, struct EnHopperReleaseWork *w);
int en_hopper_create(void *parent, EnHopperPlace *place);
void en_hopper_models_init(void);
void en_hopper_rotate_yxz(int *ang);

#endif /* REZ_STG04_EN_HOPPER_H */
