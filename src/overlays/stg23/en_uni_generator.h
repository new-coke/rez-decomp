/* Trance Mission uni generator. */

#ifndef REZ_STG23_EN_UNI_GENERATOR_H
#define REZ_STG23_EN_UNI_GENERATOR_H

#include "stg23.h"

void en_uni_generator_vars_register(void);
void en_uni_generator_vars_unregister(void);
void en_uni_generator_rnd_pos(EnPlacement *base, EnPlacement *out);
void en_uni_generator_rnd_vec(EnGeneratorWork *w, EnPlacement *place);
void en_uni_generator_rnd_rot(void *gen, EnPlacement *place);
void en_uni_generator_fill_arg(EnGeneratorWork *w);
void en_uni_generator_exec(EnGeneratorWork *w);
void en_uni_generator_init(EnGeneratorWork *w, EnPlacement *place);
void en_uni_generator_release(void);
void *en_uni_generator_create(void *parent, void *place);

#endif /* REZ_STG23_EN_UNI_GENERATOR_H */
