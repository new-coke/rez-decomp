/* Stage enemy helpers. */

#ifndef REZ_COMMON_UTIL_H
#define REZ_COMMON_UTIL_H

#include "types_common.h"

void util_node_list_toggle(void *p);
void util_node_list_toggle_unmasked(unsigned int *node, unsigned int mask);
int util_node_list_collect(void *root, void *bits, void *max, int *dst);
int util_node_list_collect_pos(void *top, int flags, int num, int *out);
void util_rand_vec3(Vec3 *v);
void util_rand_unit_vec3(Vec3 *v);
void util_rand_vec3_len(Vec3 *v, float len);
float util_curve_eval(float *p, float t);
void util_rotate_vec3(float *in, int *ang, float *out);
void util_rotate_xyz(int *ang, float x, float y, float z, Vec3 *out);
int util_node_collect(void *root, void *bits, void *max, int dst);
void util_node_collect_pos_at(void *frm, void *top, float mot, int flags, int num, int out);
void util_node_toggle(int *node);
void util_node_toggle_unmasked(int *node, unsigned int mask);
void util_to_player_frame(Vec3 *pos, Vec3 *dir, int *ang_out);
void util_pos_to_player_frame(Vec3 *in, Vec3 *out);
void util_dir_to_player_frame(float *in, float *out);
void util_ang_to_player_frame(int *in, int *out);
void util_from_player_frame(Vec3 *pos, Vec3 *dir, int *ang_out);
void util_pos_from_player_frame(Vec3 *in, Vec3 *out);
void util_dir_from_player_frame(Vec3 *in, Vec3 *out);
void util_ang_from_player_frame(int *in, int *out);
int util_player_near_axis(Vec3 *pos, int *ang, float r, float lim);
int util_player_outside(Vec3 *pos, float r);
int util_behind_player(Vec3 *pos, float r);
int util_ahead_of_player(Vec3 *pos, float r);
int util_eye_outside(Vec3 *pos);
void util_rotate_yxz(int *ang);

#endif /* REZ_COMMON_UTIL_H */
