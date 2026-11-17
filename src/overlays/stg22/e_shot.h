#ifndef REZ_STG22_E_SHOT_H
#define REZ_STG22_E_SHOT_H

#include "types_overlays.h"
#include "types_common.h"

void e_shot_vars_register(void);
void e_shot_vars_unregister(void);
int e_shot_vars_init(int *fx_id);
int e_shot_vars_release(void);
void e_shot_desc_setup(ShotDesc *shot, EShotParamSet *set);
void e_shot_type_event(ShotHit *hit);
void e_shot_event(ShotHit *hit);
int e_shot_fire(void *obj, Vec3 *pos, Vec3 *dir, int n);
void e_shot_initial_angles(int *ang, Vec3 *pos, Vec3 *dir, int level);
void cr_needle_vars_register(void);
void cr_needle_vars_unregister(void);
int cr_needle_vars_init(int *fx_id);
int cr_needle_vars_release(void);
void cr_needle_desc_setup(ShotDesc *shot, CrNeedleSet *typ);
void cr_needle_event(ShotHit *hit);
void *cr_needle_fire(void *obj, Vec3 *pos, Vec3 *dir, int n);
void cr_needle_initial_angles(int *ang, Vec3 *pos, Vec3 *dir, int level);
void models_load(void);
void models_free(void);
int use_circle_path(void);

#endif /* REZ_STG22_E_SHOT_H */
