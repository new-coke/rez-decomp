/* Hopper landing ring, Area 4. */

#ifndef REZ_STG04_EN_HOPPER_CIRCLE_H
#define REZ_STG04_EN_HOPPER_CIRCLE_H

#include "stg04.h"
#include "types_common.h"

void en_hopper_circle_vars_register(void);
void en_hopper_circle_vars_unregister(void);
void en_hopper_circle_exec(EnHopperCircleWork *w);
void en_hopper_circle_disp(EnHopperCircleWork *w);
void en_hopper_circle_init(EnHopperCircleWork *w, Vec3 *pos);
void en_hopper_circle_release(void);
EnHopperCircleWork *en_hopper_circle_create(void *parent, Vec3 *pos);

#endif /* REZ_STG04_EN_HOPPER_CIRCLE_H */
