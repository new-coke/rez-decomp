/* Mars boss walls. */

#ifndef REZ_B_MARS_MARS_WALL_H
#define REZ_B_MARS_MARS_WALL_H

#include "types_overlays.h"
#include "types_common.h"
#include "b_mars.h"

struct MarsWallWork;
struct MarsWallManWork;

void mars_w2_break_group(struct MarsWallWork *wall, struct MarsWallWork *ref);
void mars_w2_locks_clear(struct MarsWallWork *wall, int mode);
void mars_w2_films_init(struct MarsWallWork *w);
void mars_w2_films_init_b(struct MarsWallWork *w);
void mars_w2_points_update(struct MarsWallWork *w);
void mars_w2_init(struct MarsWallWork *w, int *arg);
void mars_w2_release(struct MarsWallWork *w);
void mars_w2_move(struct MarsWallWork *w);
void mars_w2_films_step(struct MarsWallWork *w);
void mars_w2_exec(struct MarsWallWork *w);
void mars_w2_disp(struct MarsWallWork *w);
void *mars_w2_create(void *parent, void *arg);
void mars_wall_man_decode(int code, int *quarter, int *tilt, int *count, float x);
void mars_wall_man_exec(struct MarsWallManWork *w);
void mars_wall_man_release(Earth1stDemoWork *w);
void *mars_wall_man_create(void *parent, Vec3 *pos, int *ang, MarsWallManArg *wall_arg, int k);
int mars_wall_man_is_active(struct MarsWallManWork *w);
void mars_wall_man_stop(struct MarsWallManWork *w);
void mars_wall_vars_add(void);
void mars_wall_vars_remove(void);
void mars_wall_rotate_yxz(int *ang);

#endif /* REZ_B_MARS_MARS_WALL_H */
