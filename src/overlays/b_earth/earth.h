/* Earth boss main task. */

#ifndef REZ_B_EARTH_EARTH_H
#define REZ_B_EARTH_EARTH_H

#include "types_overlays.h"
#include "types_common.h"
#include "b_earth.h"

struct BossCircleLight;
struct BossMoveLight;
struct BossMoveLightSet;

void star_color_apply(void);
int boss_main_demo_cb(BossMainWork *w);
void earth_init(BossMainWork *w, int *arg);
void earth_exec_start(BossMainWork *w);
void earth_materials_save(BossMainWork *w);
void earth_materials_restore(BossMainWork *w);
void earth_exec(BossMainWork *w);
void earth_start(BossMainWork *w, int restart);
void earth_restart(BossMainWork *w);
void earth_release(BossMainWork *w);
void earth_turn_to_player(BossMainWork *w);
void earth_lock_points(BossMainWork *w);
void earth_move(BossMainWork *w);
void earth_display(BossMainWork *w);
BossMainWork *earth_create(void *parent, void *arg);
void task_finish_if(void *w);
int shell_hit_check(BossMainWork *w);
void cubes_shatter_all(BossMainWork *w);
void shell_locks_enable(BossMainWork *w);
void earth_arm_disp_cb(void *w);
void earth_arm_exec_cb(void *w);
int earth_orbit_step(BossMainWork *w);
void earth_phase_step(BossMainWork *w);
void earth_exec_defeat(BossMainWork *w);
void earth_disp_final(BossMainWork *w);
void core_hp_reset(BossMainWork *w);
int core_hit_check(BossMainWork *w);
void core_stage1_clear(BossMainWork *w);
void core_stage2_clear(BossMainWork *w);
void core_stage3_clear(BossMainWork *w);
void earth_exec_battle(BossMainWork *w);
int earth_get_attack_end(BossMainWork *w);
int earth_damage(BossMainWork *w, int damage);
int earth_arm_hit(BossMainWork *w, int deal);
int earth_arm_add(BossMainWork *w);
int earth_get_level(BossMainWork *w);
int earth_is_defeated(BossMainWork *w);
void earth_debug_keys(BossMainWork *w);
void cube_light_draw_frames(int *ang, unsigned int color, int n, float z, float dz, float half);
void circle_light_disp(struct BossCircleLight *w);
void circle_light_exec(struct BossCircleLight *w);
struct BossCircleLight *circle_light_create(void *parent, Vec3 *pos, int tex, float secs, float delay, float size);
void cube_light_init(BossCubeLight *w, BossCubeLight *arg);
void cube_light_exec(BossCubeLight *w);
void cube_light_disp(BossCubeLight *w);
BossCubeLight *cube_light_create_arg(void *parent, BossCubeLight *arg);
BossCubeLight *cube_light_create(void *parent, Vec3 *pos, int *ang, int *frame_ang, int tex);
void earth_side_break(BossMainWork *w, int side);
void move_light_exec(struct BossMoveLight *w);
void move_light_disp(struct BossMoveLight *w);
struct BossMoveLight *move_light_create(void *parent, Vec3 *from, Vec3 *target, int tex, float secs, float secs2, float speed);
void move_light_set_exec(struct BossMoveLightSet *w);
struct BossMoveLightSet *move_light_set_create(void *parent, Vec3 *pos, int tex, int count, float secs);
void earth_rotate_yxz(int *ang);

#endif /* REZ_B_EARTH_EARTH_H */
