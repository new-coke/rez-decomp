/* Formation counters, pass motion helpers. */

#ifndef REZ_ENEMY_FORMATION_H
#define REZ_ENEMY_FORMATION_H

struct EnemyPassWork;

struct FormSlot;

void *enemy_formation_create(void *parent);
void enemy_formation_assign(int idx, int cnt);
void enemy_formation_appear(int idx, int cnt);
void enemy_formation_die(int idx, int cnt, float *pos);
void enemy_formation_away(int idx, int cnt);
void enemy_pass_get_matrix_angles(int *ang);
void enemy_pass_draw_tree(struct EnemyPassWork *node, int *child);

#endif /* REZ_ENEMY_FORMATION_H */
