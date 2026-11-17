/* Break effect for shot-down enemies. */

#ifndef REZ_COMMON_ENEMY_SHATTER_H
#define REZ_COMMON_ENEMY_SHATTER_H

#include "types_overlays.h"
#include "types_common.h"

void enemy_shatter_vars_register(void);
void enemy_shatter_vars_unregister(void);
void *enemy_shatter_nth_node(unsigned int *node, int *nth);
void *enemy_shatter_nth_node_at(unsigned int *node, int nth);
int enemy_shatter_count_drawn(unsigned int *node);
void enemy_shatter_node_matrix_walk(unsigned int *node, int *nth, float *mtx);
void enemy_shatter_mul_node_matrix(unsigned int *node, int nth);
void enemy_shatter_emit_piece(Shatter *w, unsigned int *src, Vec3 *pos, Vec3 *dir, Vec3 *vel, int idx);
void enemy_shatter_emit_tree_from(Shatter *w, unsigned int *node, Vec3 *center, Vec3 *vel, int *idx);
void enemy_shatter_emit_tree(Shatter *w, unsigned int *node, Vec3 *center, Vec3 *vel);
void enemy_shatter_move(Shatter *w);
void enemy_shatter_exec(Shatter *w);
void enemy_shatter_disp(Shatter *w);
unsigned int *enemy_shatter_nth_any_node(unsigned int *node, int *nth);
unsigned int *enemy_shatter_nth_any_node_at(unsigned int *node, int nth);
void enemy_shatter_init(Shatter *w, SpawnDesc *desc);
void enemy_shatter_release(Shatter *w);
Shatter *enemy_shatter_create(void *parent, SpawnDesc *desc, float *params, int *fog);
void enemy_shatter_rotate_yxz(int *ang);

#endif /* REZ_COMMON_ENEMY_SHATTER_H */
