/* Eden boss MB enemies. */

#ifndef REZ_B_EDEN_EDEN_ENEMY_H
#define REZ_B_EDEN_EDEN_ENEMY_H

#include "types_common.h"

struct EdenEnemyMbWork;
struct EdenEnemyMbManWork;

void eden_enemy_vars_add(void);
void eden_enemy_vars_remove(void);
int eden_enemy_cyl_outside(Vec3 *pos, Vec3 *vel, float r_in, float r_out, float k);
int eden_enemy_cyl_inside(Vec3 *pos, Vec3 *vel, float r_in, float r_out, float k);
int eden_enemy_ceiling_check(Vec3 *pos, Vec3 *vel, float x, float y, float z);
int eden_enemy_floor_check(Vec3 *pos, Vec3 *vel, float x, float y, float z);
void eden_enemy_mb_draw_body(struct EdenEnemyMbWork *w, int flag, float s);
void eden_enemy_mb_disp(struct EdenEnemyMbWork *w);
int eden_enemy_mb_pulse_step(struct EdenEnemyMbWork *w);
void eden_enemy_mb_rotate(Vec3 *axis, float *ang, int *rot);
void eden_enemy_mb_set_dir(struct EdenEnemyMbWork *w, Vec3 *dir, float s);
void eden_enemy_mb_exec(char *w);
void eden_enemy_mb_release(struct EdenEnemyMbWork *w);
char *eden_enemy_mb_create(void *owner, void *parent, Vec3 *pos, int arg, int side);
void eden_enemy_mb_push_apart(struct EdenEnemyMbWork *mb_a, struct EdenEnemyMbWork *mb_b, Vec3 *pa, Vec3 *pb, Vec3 *va, Vec3 *vb, float k);
void eden_enemy_mb_man_collide(struct EdenEnemyMbManWork *w);
void eden_enemy_mb_man_exec(struct EdenEnemyMbManWork *w);
void eden_enemy_mb_man_disp(void);
void eden_enemy_mb_man_release(struct EdenEnemyMbManWork *w);
int eden_enemy_mb_man_create(void *parent, int arg);
void eden_enemy_mb_man_next_group(struct EdenEnemyMbManWork *w, int grp);
void eden_enemy_rotate_yxz(int *ang);

#endif /* REZ_B_EDEN_EDEN_ENEMY_H */
