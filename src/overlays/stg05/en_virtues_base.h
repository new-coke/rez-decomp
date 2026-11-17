/* Virtues base, Area 5. */

#ifndef REZ_STG05_EN_VIRTUES_BASE_H
#define REZ_STG05_EN_VIRTUES_BASE_H

#include "stg05.h"
#include "types_common.h"

struct EnVirtuesBaseCreateArg;

void en_virtues_base_vars_register(void);
void en_virtues_base_vars_unregister(void);
void en_virtues_base_init(EnVirtuesBaseWork *w, struct EnVirtuesBaseCreateArg *prm);
void en_virtues_base_release(EnVirtuesBaseWork *w);
void en_virtues_base_exec_end(EnVirtuesBaseWork *w);
void en_virtues_base_exec_rise(EnVirtuesBaseWork *w);
void en_virtues_base_exec_hold(EnVirtuesBaseWork *w);
void en_virtues_base_exec(EnVirtuesBaseWork *w);
void en_virtues_base_disp(EnVirtuesBaseWork *w);
void *en_virtues_base_create(void *w, EnVirtuesBaseArg *prm);
void en_virtues_base_stop_walls(EnVirtuesBaseWork *w);
void en_virtues_base_get_pose(PosAngRec *w, Vec3 *pos, int *ang);
void en_virtues_base_rotate_yxz(int *ang);

#endif /* REZ_STG05_EN_VIRTUES_BASE_H */
