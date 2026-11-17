/* Venus boss player view. */

#ifndef REZ_B_VENUS_VENUS_PLAYER_H
#define REZ_B_VENUS_VENUS_PLAYER_H

#include "types_common.h"

struct VenusPlayerWork;
struct VenusPlayerCamRamp;
struct VenusPlayerCamWork;

void venus_player_view_reset(void);
void venus_pl_vars_add(void);
void venus_pl_vars_remove(void);
void venus_player_motion_set(void *mot, Vec3 *pos, int *ang, Vec3 *dir, float t);
void venus_player_motion_blend(char *view, Vec3 *mot_pos, Vec3 *mot_dir, int *mot_ang, float t);
void venus_player_orbit(char *view, int apply);
void venus_player_view_set(struct VenusPlayerWork *w, int frm, Vec3 *pos, float tilt_x, float tilt_z, float ang_spd);
void venus_player_view_set_spd(struct VenusPlayerWork *w, int frm, float ang_spd);
void venus_player_seq(char *w);
void venus_player_exec(char *w);
void venus_player_ent_add(void *work, void *rec_in, int i);
void venus_player_disp(void);
void venus_player_release(void *w);
int venus_player_create(char *parent, int x);
int venus_player_ent_next(int *w);
int venus_player_orbit_calc(void *cam_mot, void *mot, float *ofs, float *out_y, float *out_ang, float *out_spd, float *out_len);
void venus_player_view_rand(void *work, int n);
void venus_player_cam_exec(struct VenusPlayerCamRamp *w);
void venus_player_cam_disp(float *w);
struct VenusPlayerCamWork *venus_player_cam_create(void *parent, void *cam_mot, void *mot, float *ofs);
void venus_player_pass_exec(int *w);
void venus_player_pass_disp(int *w);
int *venus_player_pass_create(void *parent, int mot, int frm_p);
void venus_player_rotate_zyx(int *ang);
void venus_player_rotate_yxz(int *ang);

#endif /* REZ_B_VENUS_VENUS_PLAYER_H */
