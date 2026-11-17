/* Trance Mission wara generator. */

#ifndef REZ_STG23_EN_WARA_GENERATOR_H
#define REZ_STG23_EN_WARA_GENERATOR_H

#include "stg23.h"

struct EnWaraGeneratorWork;

void en_wara_generator_vars_register(void);
void en_wara_generator_vars_unregister(void);
void en_wara_generator_rnd_pos(EnPlacement *base, EnPlacement *out);
void en_wara_generator_rnd_vec(struct EnWaraGeneratorWork *w, EnPlacement *place);
void en_wara_generator_rnd_rot(void *gen, EnPlacement *place);
void en_wara_generator_rnd_swing(struct EnWaraGeneratorWork *w, EnPlacement *place);
void en_wara_generator_fill_arg(struct EnWaraGeneratorWork *w);
int en_wara_generator_count_children(void *w);
void en_wara_generator_exec(struct EnWaraGeneratorWork *w);
void en_wara_generator_init(struct EnWaraGeneratorWork *w, EnPlacement *place);
void en_wara_generator_release(void);
void *en_wara_generator_create(void *parent, void *place);

#endif /* REZ_STG23_EN_WARA_GENERATOR_H */
