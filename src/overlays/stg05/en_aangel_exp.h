/* Archangel explosions, Area 5. */

#ifndef REZ_STG05_EN_AANGEL_EXP_H
#define REZ_STG05_EN_AANGEL_EXP_H

#include "types_common.h"

struct EnAAngelExpWork;
struct EnAAngelExpArg;

void en_aangel_exp_init(struct EnAAngelExpWork *w, struct EnAAngelExpArg *prm);
void en_aangel_exp_release(struct EnAAngelExpWork *w);
void en_aangel_exp_exec(struct EnAAngelExpWork *w);
void en_aangel_exp_disp(struct EnAAngelExpWork *w);
void *en_aangel_exp_create(void *w, Vec3 *pos, int counter, int timer, int big);
void *en_aangel_exp_create_scaled(void *w, Vec3 *pos, int counter, int timer, int big, float scale);

#endif /* REZ_STG05_EN_AANGEL_EXP_H */
