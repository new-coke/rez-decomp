/* Lightning between two points (Perfect Wall). */

#ifndef REZ_COMMON_EN_LIGHTNING_H
#define REZ_COMMON_EN_LIGHTNING_H

#include "types_overlays.h"

void en_lightning_vars_register(void);
void en_lightning_vars_unregister(void);
void en_lightning_init_segments(EnLightningWork *w);
void en_lightning_lay_points(EnLightningWork *w);
void en_lightning_build_polyline(EnLightningWork *w);
void en_lightning_exec(EnLightningWork *w);
void en_lightning_disp(EnLightningWork *w);
void en_lightning_init(EnLightningWork *w, EnLightningArg *prm);
void en_lightning_release(int *w);
void *en_lightning_create(void *parent, void *prm);
void en_lightning_rotate_yxz(int *ang);

#endif /* REZ_COMMON_EN_LIGHTNING_H */
