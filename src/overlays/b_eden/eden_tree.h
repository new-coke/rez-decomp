/* Eden boss tree attack. */

#ifndef REZ_B_EDEN_EDEN_TREE_H
#define REZ_B_EDEN_EDEN_TREE_H

#include "types_overlays.h"
#include "types_common.h"

struct EdenTreeWork;
struct EdenAim;

void eden_tree_vars_add(void);
void eden_tree_vars_remove(void);
struct EdenAim *eden_tree_aim_alloc(struct EdenTreeWork *w);
void eden_tree_aim_spread(struct EdenTreeWork *w, struct EdenAim *src);
void eden_tree_aim_player(struct EdenTreeWork *w, struct EdenAim *src);
int eden_tree_aims_step(struct EdenTreeWork *w);
void eden_tree_exec(struct EdenTreeWork *w);
void eden_tree_disp(struct EdenTreeWork *w);
void eden_tree_release(Earth1stDemoWork *w);
void eden_tree_aim_start(struct EdenTreeWork *w);
int *eden_tree_create(void *parent, Vec3 *pos, Vec3 *dir, int arg, int kind);
void eden_tree_rotate_yxz(int *ang);

#endif /* REZ_B_EDEN_EDEN_TREE_H */
