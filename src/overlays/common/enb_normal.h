/* Common enemy bullet (E_Shot and others). */

#ifndef REZ_COMMON_ENB_NORMAL_H
#define REZ_COMMON_ENB_NORMAL_H

#include "types_common.h"
#include "types_overlays.h"

void enb_normal_aim_velocity(Vec3 *pos, Vec3 *dir, ShotDesc *desc, Vec3 *vel, Vec3 *aim);
void enb_normal_side_velocity(float speed, Vec3 *pos, Vec3 *vel);
void enb_normal_send_event(EnbNormalWork *w, int op);
void enb_normal_close_fx(EnbNormalWork *w, int op, int ev);
int enb_normal_area_check(EnbNormalWork *w, int quiet);
void enb_normal_init(EnbNormalWork *w, ObjShotArg *shot);
void enb_normal_release(EnbNormalWork *w);
int enb_normal_move(EnbNormalWork *w);
void enb_normal_animate(EnbNormalWork *w);
int enb_normal_hit_check(EnbNormalWork *w);
int enb_normal_range_check(EnbNormalWork *w);
void enb_normal_exec(EnbNormalWork *w);
void enb_normal_draw_model(EnbNormalWork *w);
void enb_normal_draw_sprite(EnbNormalWork *w);
void enb_normal_disp(EnbNormalWork *w);
void *enb_normal_create(void *parent, Vec3 *pos, Vec3 *dir, int aimed, void *kind, int *blk);
void *enb_normal_fire(void *parent, Vec3 *pos, Vec3 *dir, int aimed, void *kind, int *blk);
void *enb_normal_fire_same_lock(void *parent, Vec3 *pos, Vec3 *dir, int aimed, void *kind, int *blk);
void *enb_normal_fire_basic(void *parent, Vec3 *pos, Vec3 *dir, int aimed, void *kind, int *blk);
void enb_normal_initial_angles(int *ang, Vec3 *pos, Vec3 *dir, int aimed, int *blk);
void enb_normal_kill(EnbNormalWork *w);
void enb_normal_rotate_yxz(int *ang);

#endif /* REZ_COMMON_ENB_NORMAL_H */
