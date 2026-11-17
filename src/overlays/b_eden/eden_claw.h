/* Eden boss claw form. */

#ifndef REZ_B_EDEN_EDEN_CLAW_H
#define REZ_B_EDEN_EDEN_CLAW_H

#include "types_common.h"

struct EdenClawLink;
struct EdenClawRow;

typedef struct EdenClawLink EdenClawLink;
typedef struct EdenClawRow EdenClawRow;
struct EdenClawWork;
struct EdenClawWallWork;

void eden_claw_vars_add(void);
void eden_claw_vars_remove(void);
void eden_claw_count_leaves(WireObj *obj, int *n);
void eden_claw_tree_place_leaves(WireObj *obj, EdenClawLink **pp);
void eden_claw_tree_pose(void *w, void *src, int val, float t);
void eden_claw_row_reset(struct EdenClawWork *w, EdenClawRow *row);
void eden_claw_row_follow(struct EdenClawWork *w, EdenClawRow *row);
void eden_claw_ramps_step(struct EdenClawWork *w);
void eden_claw_rows_set_speed(struct EdenClawWork *w, int spd);
void eden_claw_swing(struct EdenClawWork *w, int flag);
int eden_claw_move_step(struct EdenClawWork *w);
void eden_claw_tree_shoot(struct EdenClawWork *w);
void eden_claw_attack_step(struct EdenClawWork *w);
void eden_claw_exec(struct EdenClawWork *claw);
void eden_claw_release(struct EdenClawWork *w);
void eden_claw_disp(struct EdenClawWork *w);
int *eden_claw_create(void *parent, int *arg);
void eden_claw_get_counts(struct EdenClawWork *w, int *nrow, int *nlink);
void eden_claw_get_link(struct EdenClawWork *w, int i, int j, Vec3 **pos, Vec3 **ang);
int eden_claw_is_active(struct EdenClawWork *w);
void eden_claw_piece_check(struct EdenClawWork *w);
void eden_claw_wall_grow(struct EdenClawWallWork *w);
void eden_claw_wall_exec(struct EdenClawWallWork *w);
void eden_claw_wall_disp(struct EdenClawWallWork *w);
void eden_claw_wall_release(struct EdenClawWallWork *w);
struct EdenClawWallWork *eden_claw_wall_create(void *parent, int arg, void *pos, void *ang, void *spin, int n);
void eden_claw_rotate_yxz(int *ang);

#endif /* REZ_B_EDEN_EDEN_CLAW_H */
