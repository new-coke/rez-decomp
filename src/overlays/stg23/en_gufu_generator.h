/* Trance Mission gufu generator. */

#ifndef REZ_STG23_EN_GUFU_GENERATOR_H
#define REZ_STG23_EN_GUFU_GENERATOR_H

#include "stg23.h"

void en_gufu_generator_vars_register(void);
void en_gufu_generator_vars_unregister(void);
void en_gufu_generator_rnd_pos(EnPlacement *base, EnPlacement *out);
void en_gufu_generator_rnd_vec(EnGeneratorWork *w, EnPlacement *place);
void en_gufu_generator_rnd_rot(void *gen, EnPlacement *place);
void en_gufu_generator_rnd_escape(EnGeneratorWork *w, EnPlacement *place);
void en_gufu_generator_fill_arg(EnGeneratorWork *w);
void en_gufu_generator_exec(EnGeneratorWork *w);
void en_gufu_generator_init(EnGeneratorWork *w, EnPlacement *place);
void en_gufu_generator_release(void);
void *en_gufu_generator_create(void *parent, void *place);

#endif /* REZ_STG23_EN_GUFU_GENERATOR_H */
