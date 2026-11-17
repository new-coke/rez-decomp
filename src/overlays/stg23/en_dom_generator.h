/* Trance Mission dom generator. */

#ifndef REZ_STG23_EN_DOM_GENERATOR_H
#define REZ_STG23_EN_DOM_GENERATOR_H

#include "types_common.h"
#include "stg23.h"

void en_dom_generator_vars_register(void);
void en_dom_generator_vars_unregister(void);
void en_dom_generator_glyph_dir(float *vtx, Vec3 *base, Vec3 *org, Vec3 *out);
int en_dom_generator_create(void *parent, EnPlacement *place);

#endif /* REZ_STG23_EN_DOM_GENERATOR_H */
