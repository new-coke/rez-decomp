/* Ramses' ground rings. */

#ifndef REZ_COMMON_RAMSES_CIRCLE_H
#define REZ_COMMON_RAMSES_CIRCLE_H

#include "types_overlays.h"

void ramses_circle_init(RingWork *w, RingArg *prm);
void ramses_circle_release(void);
void ramses_circle_exec(RingWork *w);
void ramses_circle_disp(RingWork *w);
void *ramses_circle_create(void *obj, RingArg *prm);
void ramses_circle_rotate_yxz(int *ang);

#endif /* REZ_COMMON_RAMSES_CIRCLE_H */
