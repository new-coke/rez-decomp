#ifndef REZ_STG04_E_SHOT_H
#define REZ_STG04_E_SHOT_H

#include "types_overlays.h"
#include "types_common.h"

void e_shot_vars_register(void);
void e_shot_vars_unregister(void);
int e_shot_vars_init(int *fx_id);
int e_shot_vars_release(void);
void e_shot_setup_model71(ShotDesc *blk, EShotParamSet *set);
void e_shot_type_event(ShotHit *hit);
void e_shot_event(ShotHit *hit);
void *e_shot_fire(void *parent, Vec3 *pos, Vec3 *dir, int level);
void e_shot_initial_angles(int *ang, Vec3 *pos, Vec3 *dir, int level);

#endif /* REZ_STG04_E_SHOT_H */
