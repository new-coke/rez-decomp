/* Eden boss mirror lasers. */

#ifndef REZ_B_EDEN_EDEN_LASER_H
#define REZ_B_EDEN_EDEN_LASER_H

#include "types_overlays.h"

struct EdenLaserWork;

void eden_laser_vars_add(void);
void eden_laser_vars_remove(void);
void eden_laser_exec(struct EdenLaserWork *w);
void eden_laser_disp(struct EdenLaserWork *w);
void eden_laser_release(Earth1stDemoWork *w);
struct EdenLaserWork *eden_laser_create(void *parent, int sub, int tgt, int owner, int arg, int kind);
void eden_laser_stop(struct EdenLaserWork *w);
void eden_laser_fire(struct EdenLaserWork *w);
void eden_score_vars_add(void);
void eden_score_vars_remove(void);
void eden_map_edit_fog_load(void);
void eden_map_edit_fog_store(void);
void eden_map_edit_fog_cb(void);
void eden_map_edit_pass_cb(void);

#endif /* REZ_B_EDEN_EDEN_LASER_H */
