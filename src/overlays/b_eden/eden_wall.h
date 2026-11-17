/* Eden boss mirror wall form. */

#ifndef REZ_B_EDEN_EDEN_WALL_H
#define REZ_B_EDEN_EDEN_WALL_H

#include "types_overlays.h"
#include "types_common.h"

struct EdenMirror;
struct EdenWallWork;

void eden_wall_vars_add(void);
void eden_wall_vars_remove(void);
int eden_wall_grid_get(struct EdenWallWork *w, int i, int j);
void eden_wall_grid_set(struct EdenWallWork *w, int x, int y, int val);
void eden_wall_mirror_grid_step(struct EdenWallWork *w, struct EdenMirror *mirror);
void eden_wall_mirror_grid_pos(struct EdenWallWork *w, struct EdenMirror *mirror);
void eden_wall_mirrors_place(struct EdenWallWork *w);
void eden_wall_mirror_form_ring(struct EdenMirror *mirror);
void eden_wall_mirror_form_wall(struct EdenMirror *mirror);
void eden_wall_mirrors_face(struct EdenWallWork *w);
void eden_wall_mirrors_step(struct EdenWallWork *w);
void eden_wall_mirrors_link(void *piece, struct EdenWallWork *w);
void eden_wall_mirror_shoot(struct EdenWallWork *w);
void eden_wall_mirrors_check(struct EdenWallWork *w);
void eden_wall_laser_step(struct EdenWallWork *w);
void eden_wall_mirrors_stack(struct EdenWallWork *w);
void eden_wall_flash_step(struct EdenWallWork *w);
void eden_wall_mirrors_reset(struct EdenWallWork *w);
void eden_wall_mirrors_restore(struct EdenWallWork *w);
void eden_wall_mirrors_blend(struct EdenWallWork *w, float t);
void eden_wall_mirrors_blend_step(struct EdenWallWork *w);
void eden_wall_exec(struct EdenWallWork *wall);
void eden_wall_release(struct EdenWallWork *w);
void eden_wall_disp(Earth1stDemoWork *w);
int *eden_wall_create(void *parent, int arg);
void eden_wall_mirror_seg_get(struct EdenWallWork *w, int i, int j, Vec3 **pos, Vec3 **ang);
void eden_wall_get_dims(struct EdenWallWork *w, int *nmirror, int *nseg);

#endif /* REZ_B_EDEN_EDEN_WALL_H */
