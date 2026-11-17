/* Venus boss fog, star colour and lasers. */

#ifndef REZ_B_VENUS_VENUS_MPOD_H
#define REZ_B_VENUS_VENUS_MPOD_H

#include "types_common.h"
#include "types_overlays.h"

struct VenusLExWork;
struct VenusLaserWork;

int *venus_fog_create(void *parent, int mode, float sec);
int venus_fog_set(void *w);
int venus_fog_set_far(void *w, float far_rate);
void venus_ch_exec(int *w);
void venus_ch_release(int *w);
int *venus_ch_create(void *parent, void *star);
void venus_laser_vars_add(void);
void venus_laser_vars_remove(void);
void venus_l_pole_disp(void);
int venus_l_pole_hit(Vec3 *start, Vec3 *end);
int venus_l_clip_y(Vec3 *start, Vec3 *end, float *rate);
void venus_l_release(Earth1stDemoWork *w);
void venus_l_exec(struct VenusLaserWork *w);
void venus_l_disp(struct VenusLaserWork *w);
int *venus_l_create(void *parent, Vec3 *base, int *ang, int life, int cols, float ofs, float width);
void venus_l_ex_exec(struct VenusLExWork *w);
int *venus_l_ex_create(void *parent, Vec3 *pos, int life, int num);
void venus_l_ex_finish(void *w, int k);
float venus_line_project(Vec3 *v, Vec3 *out);
int venus_line_capsule(Vec3 *p0, Vec3 *p1, int *buf, float scale, float r_min, float r_max);

#endif /* REZ_B_VENUS_VENUS_MPOD_H */
