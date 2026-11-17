/* Mars boss trees. */

#ifndef REZ_B_MARS_MARS_TREE_H
#define REZ_B_MARS_MARS_TREE_H

#include "types_common.h"

struct MarsTree;
struct MarsNode;
struct MarsTrManWork;

typedef struct MarsNode MarsNode;
typedef struct MarsTree MarsTree;

void mars_tree_vars_add(void);
void mars_tree_vars_remove(void);
int mars_tr_rand_ang(float lo, float hi);
void mars_tr_pool_alloc(MarsTree *tree);
MarsNode *mars_tr_node_find_free(MarsTree *tree);
MarsNode *mars_tr_node_add(MarsTree *tree, MarsNode *parent, int kind);
MarsNode *mars_tr_find_tip(MarsTree *tree);
void mars_tr_leaves_activate(MarsTree *tree, MarsNode *n, int *cnt);
void mars_tr_fire_walk_b(MarsTree *tree, MarsNode *n);
void mars_tr_fire_walk(MarsTree *tree, MarsNode *n);
void mars_tr_locks_remove(MarsNode *n);
void mars_tr_node_shatter_all(MarsTree *tree, MarsNode *n);
MarsNode *mars_tr_node_break(MarsTree *tree, MarsNode *n, int flag);
void mars_tr_hit_walk(MarsTree *tree, MarsNode *n, int *out);
int mars_tr_grow_ring(MarsTree *tree, MarsNode *n, int mode);
int mars_tr_grow_inward(MarsTree *tree, MarsNode *n, int mode);
int mars_tr_grow_fork(MarsTree *tree, MarsNode *n, int mode);
void mars_tr_move_all(MarsTree *tree, MarsNode *n, Vec3 *delta);
void mars_tr_move_rotate(MarsTree *tree, MarsNode *n, Vec3 *delta, int ang);
int mars_tr_grow_walk(MarsTree *tree, MarsNode *n, int *st);
void mars_tr_draw_walk(MarsTree *tree, int node_addr);
void mars_tr_init(char *w, int *arg);
void mars_tr_exec(MarsTree *w);
void mars_tr_disp(MarsTree *w);
void mars_tr_release(MarsTree *tree);
int mars_tr_take_state13(MarsTree *w);
int mars_tr_leaf_count(MarsTree *w);
void *mars_tr_create(void *parent, void *arg);
void mars_tr_set_done(struct MarsTrManWork *w);
void mars_tr_man_spawn(struct MarsTrManWork *w);
void mars_tr_man_exec(struct MarsTrManWork *w);
void mars_tr_man_release(struct MarsTrManWork *w);
void *mars_tr_man_create(void *parent, int *arg);

#endif /* REZ_B_MARS_MARS_TREE_H */
