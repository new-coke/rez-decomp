/* Hunter (stg04 enemy). */

#ifndef REZ_COMMON_EN_HUNTER_H
#define REZ_COMMON_EN_HUNTER_H

#include "types_overlays.h"

struct EnHunterCbArg;

void en_hunter_vars_register(void);
void en_hunter_vars_unregister(void);
int en_hunter_marker_draw(int obj, void *place);
int en_hunter_vars_init(void);
int en_hunter_vars_release(void);
void en_hunter_calc_tip(void *obj, EnHunterWork *w);
void en_hunter_calc_spawn_points(void *obj, EnHunterWork *w);
void en_hunter_calc_points(void *obj, EnHunterWork *w);
void en_hunter_break_part(void *obj, EnHunterWork *w, int part);
void en_hunter_hit_check(void *obj, EnHunterWork *w);
void en_hunter_swing(void *obj, EnHunterWork *w);
void en_hunter_fire_flashes(void *obj, EnHunterWork *w);
void en_hunter_fire_shots(void *obj, EnHunterWork *w);
void en_hunter_toggle_parts(void *obj, EnHunterWork *w);
void en_hunter_state_rise(void *obj, EnHunterWork *w);
void en_hunter_state_fade_in(void *obj, EnHunterWork *w);
void en_hunter_state_float(void *obj, EnHunterWork *w);
void en_hunter_state_attack(void *obj, EnHunterWork *w);
float en_hunter_ease_curve(float t);
void en_hunter_state_beam(void *obj, EnHunterWork *w);
void en_hunter_state_charge(void *obj, EnHunterWork *w);
void en_hunter_state_retreat(void *obj, EnHunterWork *w);
void en_hunter_exec_normal(void *obj, EnHunterWork *w);
void en_hunter_exec_destroyed(void *obj, EnHunterWork *w);
void en_hunter_exec(void *obj, void *w);
void en_hunter_pre_exec(void *obj, EnHunterWork *w);
void en_hunter_post_exec(void *obj, EnHunterWork *w);
int en_hunter_node_cb(unsigned int *node, int idx, struct EnHunterCbArg *cb);
void en_hunter_disp_fade(void *obj, EnHunterWork *w);
void en_hunter_disp_body(void *obj, EnHunterWork *w);
void en_hunter_disp(void *obj, EnHunterWork *w);
void en_hunter_init(void *obj, EnHunterWork *w);
void en_hunter_release(void *obj, char *w);
int en_hunter_create(void *parent, void *place);
void en_hunter_nodes_init(void);
void en_hunter_rotate_yxz(int *ang);

#endif /* REZ_COMMON_EN_HUNTER_H */
