/* Air trail. */

#ifndef REZ_COMMON_AIR_MNG_H
#define REZ_COMMON_AIR_MNG_H

#include "types_overlays.h"

void air_mng_init(AirMngWork *w, AirArg *air);
void air_mng_release(AirMngWork *w);
void air_mng_exec(AirMngWork *w);
void air_mng_disp(AirMngWork *w);
void *air_mng_create(EnbNormalWork *owner, TrailArg *tail);
void air_mng_stop(AirMngWork *w);

#endif /* REZ_COMMON_AIR_MNG_H */
