/* Hopper shot, Area 4. */

#ifndef REZ_STG04_ENB_HOPPER_H
#define REZ_STG04_ENB_HOPPER_H

#include "stg04.h"
#include "types_common.h"

void enb_hopper_vars_register(void);
void enb_hopper_vars_unregister(void);
int enb_hopper_vars_init(int *fx_id);
int enb_hopper_vars_release(void);
void enb_hopper_calc_points(EnbHopperWork *w);
void enb_hopper_hit_check(EnbHopperWork *w);
void enb_hopper_crash_check(EnbHopperWork *w);
void enb_hopper_wire_color(EnbHopperWork *w);
void enb_hopper_state_approach(EnbHopperWork *w);
void enb_hopper_state_break(EnbHopperWork *w);
void enb_hopper_exec(EnbHopperWork *w);
void enb_hopper_disp(EnbHopperWork *w);
void enb_hopper_init(EnbHopperWork *w, Vec3 *pos, void *place);
void enb_hopper_release(EnbHopperWork *w);
EnbHopperWork *enb_hopper_create(void *parent, Vec3 *pos, void *place);
void enb_hopper_models_init(void);
void enb_hopper_rotate_yxz(int *ang);

#endif /* REZ_STG04_ENB_HOPPER_H */
