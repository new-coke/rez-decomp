/* Earth boss cubes and Kuri. */

#ifndef REZ_B_EARTH_EARTH_CUBE_H
#define REZ_B_EARTH_EARTH_CUBE_H

#include "types_overlays.h"
#include "types_common.h"

void cube_face_edit_store(void);
void cube_vars_add(void);
void cube_vars_remove(void);
int kuri_hit_check(BossMainWork *w);
int cubes_any_in_state(BossMainWork *w, int state);
void cubes_build_tree(void *w, BossCubeSet *set, WireObj *obj, int *depth);
void cubes_build_tree4(void *w, BossCubeSet *set, WireObj *obj, WireObj *alt_a, WireObj *alt_b, WireObj *alt_c, int *depth);
void tree_show_models(WireObj *obj);
void grid_find_cell(signed char *grid, int n, float *x, float *y);
void cube_place_on_grid(BossMainWork *w, BossCube *cube, int n, int idx);
void cubes_place_on_grid(BossMainWork *w);
int cube_face_check(int v);
void cubes_rebuild(BossMainWork *w);
void cube_world_pose(BossMainWork *w, BossCube *cube, float *pos, int *ang);
void cube_restore(BossMainWork *w, BossCube *cube);
int cube_regrow_time(BossCube *cube);
void cube_set_mode(BossCube *cube, int mode);
void cube_break_off(BossMainWork *w, BossCube *cube, Vec3 *vel, int flags);
void cubes_break_all(BossMainWork *w, int mode);
void cubes_swing(BossMainWork *w);
int cube_damp(BossCube *cube, int *ang, float k);
int kuri_attack_step(BossMainWork *w);
void kuri_hand_arm(BossMainWork *w, int hand, int on);
void kuri_hands_start(BossMainWork *w, int on);
BossCube *kuri_nearest_by_height(BossMainWork *w, BossCube **list, int n);
int kuri_release_one(BossMainWork *w);
void kuri_hand_step(BossMainWork *w, BossKuriHand *hand, int on);
void kuri_hands_step(BossMainWork *w, int on);
void cube_update(BossMainWork *w, BossCube *cube, float *mtx, Vec3 *vel, int on);
void cubes_update(void *work);
void cubes_lock_remove(BossMainWork *w);
void cubes_scatter(BossMainWork *w);
void cube_models_free(BossMainWork *w);
void tree_set_poly_flags(WireObj *obj, int flags);
void cube_models_build(BossMainWork *w);
void tree_draw_flat(WireObj *obj);
void cube_part_draw(ModelPart *part);
void color_unpack(unsigned int col, float *out);
void cubes_disp(void *work);
int homing_step(Vec3 *lead, Vec3 *tgt, Vec3 *pos, Vec3 *vel, float *k, int *n);
void kuri_shatter(BossMainWork *w, Vec3 *pos, int *rot);
void kuri_shatter_both(BossMainWork *w, Vec3 *pos, int *rot);
void cube_model_reset_poly(BossMainWork *w);
void cube_shatter_small(BossMainWork *w, Vec3 *pos, int *rot);
void cube_rotate_zyx(int *ang);
void cube_rotate_yxz(int *ang);
int player_range_check(Vec3 *pos);
int player_touch(Vec3 *pos, float rad, float damage);
int player_touch_curve(Vec3 *pos, int amount, float key, float height);

#endif /* REZ_B_EARTH_EARTH_CUBE_H */
