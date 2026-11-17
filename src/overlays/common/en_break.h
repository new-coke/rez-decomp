/* Break effects for stg04 objects. */

#ifndef REZ_COMMON_EN_BREAK_H
#define REZ_COMMON_EN_BREAK_H

#include "types_overlays.h"
#include "types_common.h"

Shatter *en_break_spawn_plain(void *obj, Vec3 *pos, int *ang, int kind);
Shatter *en_break_spawn_tuned(void *obj, Vec3 *pos, int *ang, int kind);
Shatter *en_break_spawn(void *obj, Vec3 *pos, int *ang, int kind);

#endif /* REZ_COMMON_EN_BREAK_H */
