/* Garuda's nest, Area 2 enemy. */

#ifndef REZ_STG02_EN_GARUDA_NEST_H
#define REZ_STG02_EN_GARUDA_NEST_H

#include "types_common.h"

struct EnGarudaNestPlace;
struct EnGarudaNestWork;

void en_garuda_nest_vars_register(void);
void en_garuda_nest_vars_unregister(void);
int en_garuda_nest_marker_draw(int obj, struct EnGarudaNestPlace *place);
int en_garuda_nest_vars_init(int *id);
int en_garuda_nest_vars_release(void);
void en_garuda_nest_swing(void *obj, struct EnGarudaNestWork *w);
void en_garuda_nest_pose_model(void *obj, void *wp);
float en_garuda_nest_calc_glow(void *obj, struct EnGarudaNestWork *w);
void en_garuda_nest_hit_check(void *obj, void *wp);
void en_garuda_nest_calc_spawn_point(void *obj, struct EnGarudaNestWork *w, Vec3 *pos, int *ang);
void en_garuda_nest_spawn_garuda(void *obj, struct EnGarudaNestWork *w);
void en_garuda_nest_state_wait(void *obj, struct EnGarudaNestWork *w);
void en_garuda_nest_state_launch(void *obj, struct EnGarudaNestWork *w);
void en_garuda_nest_exec_normal(void *obj, struct EnGarudaNestWork *w);
void en_garuda_nest_exec_destroyed(void *obj, struct EnGarudaNestWork *w);
void en_garuda_nest_exec(void *obj, struct EnGarudaNestWork *w);
void en_garuda_nest_pre_exec(void *obj, void *w);
void en_garuda_nest_post_exec(void *obj, float *w);
void en_garuda_nest_disp_glow(void *obj, struct EnGarudaNestWork *w);
void en_garuda_nest_disp_body(void *obj, struct EnGarudaNestWork *w);
void en_garuda_nest_disp(void *obj, struct EnGarudaNestWork *w);
void en_garuda_nest_init(void *obj, struct EnGarudaNestWork *w);
void en_garuda_nest_release(void *obj, char *w);
int en_garuda_nest_create(void *parent, void *place);
void en_garuda_nest_nodes_init(void);
void en_garuda_nest_rotate_yxz(int *ang);

#endif /* REZ_STG02_EN_GARUDA_NEST_H */
