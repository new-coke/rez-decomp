/* Circle effect. */

#ifndef REZ_COMMON_EFF_CIRCLE_H
#define REZ_COMMON_EFF_CIRCLE_H

#include "types_overlays.h"

void eff_circle_vars_add(void);
void eff_circle_vars_remove(void);
void eff_circle_exec(EffCircleWork *w);
void eff_circle_draw_ring(unsigned int col, int n, float x, float y, float z, float width, float r);
int eff_circle_on_screen(float r, float *p);
void eff_circle_disp(EffCircleWork *w);
EffCircleWork *eff_circle_create(void *parent, EffCircleArg *prm);

#endif /* REZ_COMMON_EFF_CIRCLE_H */
