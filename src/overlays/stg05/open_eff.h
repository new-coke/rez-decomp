/* Seraphim opening effect, Area 5. */

#ifndef REZ_STG05_OPEN_EFF_H
#define REZ_STG05_OPEN_EFF_H

#include "stg05.h"

struct OpenEffWork;

void open_eff_init(struct OpenEffWork *w, OpenEffArg *prm);
void open_eff_release(struct OpenEffWork *w);
void open_eff_exec(struct OpenEffWork *w);
void open_eff_disp(struct OpenEffWork *w);
void *open_eff_create(void *parent, void *prm);
void open_eff_rotate_yxz(int *ang);

#endif /* REZ_STG05_OPEN_EFF_H */
