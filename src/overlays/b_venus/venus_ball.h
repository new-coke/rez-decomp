/* Venus boss rolling balls. */

#ifndef REZ_B_VENUS_VENUS_BALL_H
#define REZ_B_VENUS_VENUS_BALL_H

#include "b_venus.h"
#include "types_common.h"

struct VenusCyl;
struct VenusBallWork;

void venus_tire_vars_add(void);
void venus_tire_vars_remove(void);
int venus_ball_cyl_hit(void *pos_in, void *vel_in, VenusParam *cyl, float rad, float bounce, float friction);
int venus_ball_cyls_hit(void *pos, void *vel, VenusParam *cyl, int n, float rad, float bounce, float friction);
int venus_ball_floor_hit(Vec3 *pos, Vec3 *v, float rad, float floor_y, float bounce);
int venus_ball_ceil_hit(Vec3 *pos, Vec3 *v, float rad, float top, float bounce);
int venus_ball_wall_hit(Vec3 *pos, Vec3 *v, float rad, float wall_r, float bounce);
void venus_ball_rotate_axes(Vec3 *axis_a, Vec3 *axis_b, int ry, int rx);
void venus_ball_move(struct VenusBallWork *w);
void venus_ball_exec(struct VenusBallWork *w);
void venus_ball_disp(struct VenusBallWork *w);
void venus_ball_release(struct VenusBallWork *w);
int *venus_ball_create(int *mng, Vec3 *pos, Vec3 *dir, int models, int cyls, int cyl_num);
void venus_ball_m_cyls_set(struct VenusCyl *cyl);
void venus_ball_m_cyls_draw(float *cyl, int n);
void venus_ball_m_separate(void *w);
void venus_ball_m_exec(int *w);
void venus_ball_m_disp(int *w);
int *venus_ball_m_create(void *parent, void *pos, void *models);
void venus_ball_rotate_yxz(int *ang);

#endif /* REZ_B_VENUS_VENUS_BALL_H */
