/* Area 5 homing projectiles. */

#ifndef REZ_STG05_MACROS_MISSILE_H
#define REZ_STG05_MACROS_MISSILE_H

#include "types_common.h"
#include "stg05.h"

struct MacrosMissileWork;
struct MacrosMissileKind;
struct SeraphimBulletWork;
struct SeraphimBulletKind;

void macros_missile_vars_register(void);
void macros_missile_vars_unregister(void);
int macros_missile_get_init_arg(void);
int macros_missile_vars_init(int *prm);
int macros_missile_vars_release(void);
void macros_missile_steer(void *obj, struct MacrosMissileWork *w);
void macros_missile_init(void *obj, struct MacrosMissileWork *w, int prm, struct MacrosMissileKind *k);
void macros_missile_release(void);
void macros_missile_state_fly(void *obj, struct MacrosMissileWork *w);
void macros_missile_update(void *obj, struct MacrosMissileWork *w);
void macros_missile_exec_appear(void *obj, struct MacrosMissileWork *w);
void macros_missile_exec_normal(void *obj, struct MacrosMissileWork *w);
void macros_missile_exec_leave(void *obj, struct MacrosMissileWork *w);
void macros_missile_exec_destroyed(void *obj, struct MacrosMissileWork *w);
void macros_missile_pre_exec(void);
void macros_missile_exec(void *obj, struct MacrosMissileWork *w);
void macros_missile_post_exec(void *obj, struct MacrosMissileWork *w);
void macros_missile_disp(void *obj, struct MacrosMissileWork *w);
int macros_missile_create(void *obj, Vec3 *pos, Vec3 *vel, float act, float ramp);
void seraphim_bullet_vars_register(void);
void seraphim_bullet_vars_unregister(void);
int seraphim_bullet_get_init_arg(void);
int seraphim_bullet_vars_init(int *prm);
int seraphim_bullet_vars_release(void);
void seraphim_bullet_init(void *obj, struct SeraphimBulletWork *w, int prm, struct SeraphimBulletKind *k);
void seraphim_bullet_release(void);
int seraphim_bullet_next_state(void *obj, struct SeraphimBulletWork *w, int n, int state);
void seraphim_bullet_state_accel(void *obj, struct SeraphimBulletWork *w);
void seraphim_bullet_state_cruise(void *obj, struct SeraphimBulletWork *w);
void seraphim_bullet_update(void *obj, struct SeraphimBulletWork *w);
void seraphim_bullet_exec_appear(void *obj, struct SeraphimBulletWork *w);
void seraphim_bullet_exec_normal(void *obj, void *w);
void seraphim_bullet_exec_leave(void *obj);
void seraphim_bullet_exec_destroyed(void *obj);
void seraphim_bullet_pre_exec(void *obj);
void seraphim_bullet_exec(void *obj, struct SeraphimBulletWork *w);
void seraphim_bullet_post_exec(void *obj);
void seraphim_bullet_disp(void *obj, struct SeraphimBulletWork *w);
int seraphim_bullet_create(void *obj, SeraphimBulletArg *prm);

#endif /* REZ_STG05_MACROS_MISSILE_H */
