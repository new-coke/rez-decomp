/* Uranus boss shots and laser. */

#ifndef REZ_B_URANUS_URANUS_BULLET_H
#define REZ_B_URANUS_URANUS_BULLET_H

#include "types_common.h"
#include "types_overlays.h"
#include "b_uranus.h"

struct UranusMbWork;
struct UranusMbManWork;
struct UranusDemoWork;
struct UranusLWork;
struct VenusClWork;

void uranus_bullet_vars_add(void);
void uranus_bullet_vars_remove(void);
void uranus_mb_disp(struct UranusMbWork *w);
void uranus_mb_disp_fade(struct UranusMbWork *w);
void uranus_mb_curve(Vec3 *end, Vec3 *start, Vec3 *pos, Vec3 *bulge, float t);
void uranus_mb_exec_fade(struct UranusMbWork *w);
void uranus_mb_exec(struct UranusMbWork *w);
void uranus_mb_release(struct UranusMbWork *w);
struct UranusMbWork *uranus_mb_create(UranusPartWork *parent, struct UranusMapWork *map, Vec3 *pos, Vec3 *vel, ModelPart **parts, int type);
void uranus_mb_man_exec(struct UranusMbManWork *w);
struct UranusMbManWork *uranus_mb_man_create(UranusPartWork *parent, struct UranusMapWork *map, Vec3 *pos, ModelPart **parts, int delay, int type);
void uranus_bullet_rotate_yxz(int *ang);
void uranus_1st_demo_exec(struct UranusDemoWork *w);
struct UranusDemoWork *uranus_1st_demo_create(void *parent, int (*cb)(int arg), int cb_arg);
void uranus_laser_vars_add(void);
void uranus_laser_vars_remove(void);
void uranus_l_exec(struct UranusLWork *w);
void uranus_l_disp(struct UranusLWork *w);
void uranus_l_release(struct UranusLWork *w);
void uranus_l_stop(struct UranusLWork *w);
struct UranusLWork *uranus_l_create(void *parent, struct UranusMapWork *map, Vec3 *src, Vec3 *dir, ModelPart **parts);
void venus_cl_spark_def(BossSparkDef *def);
void venus_cl_exec(struct VenusClWork *w);
struct VenusClWork *venus_cl_create(UranusPartWork *parent, ModelPart **parts);
void venus_cl_stop(struct VenusClWork *w);
void score_vars_add(void);
void score_vars_remove(void);

#endif /* REZ_B_URANUS_URANUS_BULLET_H */
