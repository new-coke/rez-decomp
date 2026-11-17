/* Mars boss main task. */

#ifndef REZ_B_MARS_MARS_H
#define REZ_B_MARS_MARS_H

#include "types_common.h"
#include "types_overlays.h"

struct MarsIV;
struct MarsWork;

void star_color_apply(void);
int boss_main_demo_cb(struct MarsWork *w);
void mars_exec_start(struct MarsWork *w);
void mars_init(struct MarsWork *w, int *arg);
void mars_start(char *w, int restart);
void mars_exec(struct MarsWork *w);
void mars_restart(struct MarsWork *w);
void mars_release(struct MarsWork *w);
void mars_vec_ease(Vec3 *vec, float x, float y, float z);
void mars_bgm_next_cb(struct MarsWork *w);
void mars_bgm_next(struct MarsWork *w);
void mars_phase_step(struct MarsWork *w);
void mars_fog_change(struct MarsWork *w, int fog_from, int fog_to, float t);
void mars_move(struct MarsWork *w, int dir);
void mars_exec_battle(struct MarsWork *w);
void mars_motion_start(struct MarsWork *w, int kind, int mode);
void mars_motion_step(struct MarsWork *w);
void mars_draw_pair(struct MarsWork *w, ModelPart *part_a, ModelPart *part_b);
void mars_disp_body(struct MarsWork *w, int kind);
void mars_fog_blend(struct MarsWork *w, float t, int fog_from, int fog_to);
void mars_fog_update(struct MarsWork *w);
void mars_fog_params(int *out);
int mars_wait_done(int task);
void mars_disp(char *w);
void *mars_create(void *parent, void *arg);
void task_finish_if(void *w);
void mars_pose_cb(struct MarsWork *w, Vec3 *pos, int *ang);
void mars_pose_still_cb(struct MarsWork *w, Vec3 *pos, int *ang);
void mars_pose_back_cb(struct MarsWork *w, Vec3 *pos, int *ang);
void mars_collect_points(WireObj *obj, struct MarsIV *out, int n);
void key_table_lerp(void *keys, int n, Vec3 *out, float x);
float mars_curve_y(struct MarsWork *w, float x);
int mars_bounds_reflect(struct MarsWork *w, Vec3 *pos, Vec3 *v);
int mars_is_early_phase(struct MarsWork *w);
void mars_player_damage(void);
int mars_get_level(int boss);
float mars_fog_progress(struct MarsWork *w, int s);
void mars_set_ref_pose(struct MarsWork *w, Vec3 *pos, int *ang);
void mars_get_ref_pose(struct MarsWork *w, Vec3 *pos, int *ang);
void mars_request_next(struct MarsWork *w);
int mars_debris_bounded(void);
void mars_debug_keys(struct MarsWork *w);
void mars_ak_act_exec(int *work);
int *mars_ak_act_create(void *parent, int flip);
void *mars_get_ang(struct MarsWork *w);
void *mars_attack_start(struct MarsWork *w, unsigned int kind);
int mars_bounds_bounce(struct MarsWork *w, Vec3 *pos, Vec3 *v, float k, float rad);
void mars_rotate_yxz(int *ang);

#endif /* REZ_B_MARS_MARS_H */
