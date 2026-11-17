/* Chaser, Area 4 enemy. */

#ifndef REZ_STG04_EN_CHASER_H
#define REZ_STG04_EN_CHASER_H

#include "types_common.h"

struct EnChaserWork;
struct EnChaserCbArg;

void en_chaser_vars_register(void);
void en_chaser_vars_unregister(void);
int en_chaser_marker_draw(int obj, void *place);
int en_chaser_vars_init(void);
int en_chaser_vars_release(void);
void en_chaser_calc_shot_points(void *obj, struct EnChaserWork *w, Vec3 *pts);
void en_chaser_break_part(void *obj, struct EnChaserWork *w, int part);
void en_chaser_hit_check(void *obj, void *work);
void en_chaser_swing(void *obj, struct EnChaserWork *w);
void en_chaser_face_player(void *obj, struct EnChaserWork *w);
void en_chaser_fire_flashes(void *obj, struct EnChaserWork *w);
void en_chaser_fire_shots(void *obj, struct EnChaserWork *w);
void en_chaser_toggle_parts(void *obj, struct EnChaserWork *w);
float en_chaser_forward_dist(Vec3 *pos);
float en_chaser_set_move(void *obj, struct EnChaserWork *w, float dis);
void en_chaser_state_wait(void *obj, struct EnChaserWork *w);
void en_chaser_state_approach(void *obj, struct EnChaserWork *w);
void en_chaser_state_retreat(void *obj, struct EnChaserWork *w);
void en_chaser_state_attack(void *obj, struct EnChaserWork *w);
void en_chaser_state_rush(void *obj, struct EnChaserWork *w);
void en_chaser_exec_normal(void *obj, struct EnChaserWork *w);
void en_chaser_exec_destroyed(void *obj, struct EnChaserWork *w);
void en_chaser_exec(void *obj, void *w);
void en_chaser_pre_exec(void *obj, void *w);
void en_chaser_post_exec(void);
int en_chaser_node_cb(unsigned int *node, int idx, struct EnChaserCbArg *cb);
void en_chaser_disp_fade(void *obj, struct EnChaserWork *w);
void en_chaser_disp_body(void *obj, struct EnChaserWork *w);
void en_chaser_disp(void *obj, struct EnChaserWork *w);
void en_chaser_init(void *obj, struct EnChaserWork *w);
void en_chaser_release(void *obj, char *w);
int en_chaser_create(void *parent, void *place);
void en_chaser_nodes_init(void);
void en_chaser_rotate_yxz(int *ang);

#endif /* REZ_STG04_EN_CHASER_H */
