/* Needle shots, Area 5. */

#ifndef REZ_STG05_CR_NEEDLE_H
#define REZ_STG05_CR_NEEDLE_H

#include "types_overlays.h"
#include "types_common.h"

void cr_needle_vars_register(void);
void cr_needle_vars_unregister(void);
int cr_needle_vars_init(int *fx_id);
int cr_needle_vars_release(void);
void cr_needle_desc_setup(ShotDesc *shot, CrNeedleSet *typ);
void cr_needle_event(ShotHit *hit);
void *cr_needle_fire(void *obj, Vec3 *pos, Vec3 *dir, int n);
void cr_needle_initial_angles(int *ang, Vec3 *pos, Vec3 *dir, int level);

#endif /* REZ_STG05_CR_NEEDLE_H */
