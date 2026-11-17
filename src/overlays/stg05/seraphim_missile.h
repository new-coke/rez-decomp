/* Seraphim missiles, Area 5. */

#ifndef REZ_STG05_SERAPHIM_MISSILE_H
#define REZ_STG05_SERAPHIM_MISSILE_H

#include "stg05.h"

struct SeraphimMissileKind;

void seraphim_missile_vars_register(void);
void seraphim_missile_vars_unregister(void);
int seraphim_missile_get_init_arg(void);
int seraphim_missile_vars_init(int *prm);
int seraphim_missile_vars_release(void);
int seraphim_missile_node_cb(unsigned int *n, int idx, SeraphimMissileWork *w);
void seraphim_missile_pose_model(void *obj, SeraphimMissileWork *w);
void seraphim_missile_init(void *obj, SeraphimMissileWork *w, int prm, struct SeraphimMissileKind *k);
void seraphim_missile_release(void *obj, SeraphimMissileWork *w);
int seraphim_missile_next_state(void *obj, SeraphimMissileWork *w, int n, int state);
void seraphim_missile_state_launch(void *obj, SeraphimMissileWork *w);
void seraphim_missile_state_cruise(void *obj, SeraphimMissileWork *w);
void seraphim_missile_update(void *obj, SeraphimMissileWork *w);
void seraphim_missile_exec_appear(void *obj, SeraphimMissileWork *w);
void seraphim_missile_exec_normal(void *obj, SeraphimMissileWork *w);
void seraphim_missile_exec_leave(void *obj, SeraphimMissileWork *w);
void seraphim_missile_exec_destroyed(void *obj, SeraphimMissileWork *w);
void seraphim_missile_pre_exec(void *obj);
void seraphim_missile_exec(void *obj, SeraphimMissileWork *w);
void seraphim_missile_post_exec(void *obj, SeraphimMissileWork *w);
void seraphim_missile_disp_body(void *obj, SeraphimMissileWork *w);
void seraphim_missile_disp(void *obj, SeraphimMissileWork *w);
int seraphim_missile_create(void *obj, SeraphimMissileArg *prm);
void *seraphim_missile_get_link(void *obj);
void seraphim_missile_rotate_yxz(int *ang);

#endif /* REZ_STG05_SERAPHIM_MISSILE_H */
