/* Attack ray: gun-tip flash. */

#ifndef REZ_COMMON_ATTACK_RAY_H
#define REZ_COMMON_ATTACK_RAY_H

#include "types_overlays.h"

void attack_ray_vars_register(void);
void attack_ray_vars_unregister(void);
int attack_ray_vars_init(void);
int attack_ray_vars_release(void);
void attack_ray_set_model(int model, int motion, void *tex);
void attack_ray_set_radius(float radius);
void attack_ray_exec(AttackRayWork *w);
void attack_ray_disp(AttackRayWork *w);
void attack_ray_init(AttackRayWork *w, AttackRayArg *prm);
void attack_ray_release(char *w);
void attack_ray_create(void *parent, AttackRayArg *prm);

#endif /* REZ_COMMON_ATTACK_RAY_H */
