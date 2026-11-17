/* Area 2 path recorder. */

#ifndef REZ_STG02_EN_BRAHMA_RAIL_MANAGER_H
#define REZ_STG02_EN_BRAHMA_RAIL_MANAGER_H

#include "types_common.h"

struct EnBrahmaRailManagerWork;

void en_brahma_rail_manager_kill_all(int msg);
void en_brahma_rail_manager_init(struct EnBrahmaRailManagerWork *w);
void en_brahma_rail_manager_release(struct EnBrahmaRailManagerWork *w);
void en_brahma_rail_manager_exec(struct EnBrahmaRailManagerWork *w);
void *en_brahma_rail_manager_create(void *parent);
void en_brahma_rail_manager_get_past(Vec3 *out, int *ang, int n);

#endif /* REZ_STG02_EN_BRAHMA_RAIL_MANAGER_H */
