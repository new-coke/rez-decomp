/* Venus boss missile pods and floor attacks. */

#ifndef REZ_B_VENUS_VENUS_PLATE_H
#define REZ_B_VENUS_VENUS_PLATE_H

#include "types_common.h"
#include "types_overlays.h"
#include "b_venus.h"

struct VenusMpodPod;
struct VenusMpodWork;
struct VenusPlateWork;
struct MotionTWork;
struct PlateCircleWork;

void venus_line_set_color(int *vtx, int col);
void venus_line_draw_capsules(int pts, int num, int tex, unsigned int color, float scale, float r_min, float r_max);
int venus_line_seg(Vec3 *p0, Vec3 *p1, int *buf, float scale, float r_min, float r_max);
void venus_line_draw(int pts, int num, int tex, unsigned int color, float scale, float r_min, float r_max);
void score_vars_add(void);
void score_vars_remove(void);
void venus_mpod_vars_add(void);
void venus_mpod_vars_remove(void);
void venus_mpod_place(struct VenusMpodWork *w);
void venus_mpod_row_push(struct VenusMpodWork *w, int row, float dvy);
void venus_mpod_fire(struct VenusMpodWork *w, struct VenusMpodPod *pod);
void venus_mpod_set_fire(struct VenusMpodWork *w, int side);
void venus_mpod_pass_check(struct VenusMpodWork *w);
void venus_mpod_move(struct VenusMpodWork *w);
void venus_mpod_exec(struct VenusMpodWork *w);
void venus_mpod_disp(struct VenusMpodWork *w);
void venus_mpod_release(struct VenusMpodWork *w);
void *venus_mpod_create(void *parent, int gb_man, Vec3 *pos, int *parts);
void venus_mpod_break_all(struct VenusMpodWork *w);
void venus_mpod_rotate_yxz(int *ang);
void venus_plate_set_save(void);
void venus_plate_set_load(void);
void venus_plate_vars_add(void);
void venus_plate_vars_remove(void);
void plate_exec(struct VenusPlateWork *w);
void plate_release(struct VenusPlateWork *w);
void plate_disp(struct VenusPlateWork *w);
int *plate_create(int parent, Vec3 *pos, int *ang, int model);
void motion_t_exec(struct MotionTWork *w);
void motion_t_disp(struct MotionTWork *w);
int *motion_t_create(void *parent, ModelPart *src, Vec3 *pos, int *ang);
void plate_man_spawn(PlateManWork *w, VenusParam *set);
void plate_man_exec(PlateManWork *w);
PlateManWork *plate_man_create(void *parent, int *parts);
void plate_circle_exec(struct PlateCircleWork *w);
void plate_circle_disp(struct PlateCircleWork *w);
int *plate_circle_create(int parent, int model, int pts, int fill, float r_max, float y, float sec);
short cm_vcc_man_pts_get(int *w, char **pts, float *lens);
short cm_vcc_man_pt_num(void ***w);
void cm_vcc_man_exec(CmVccManWork *w);
void cm_vcc_man_release(void **man);
CmVccManWork *cm_vcc_man_create(void *parent, int *model);
void cm_vcc_man_set_col(CmVccManWork *w, int col);
void cm_vcc_man_wave(CmVccManWork *w, float rad, float width);
void cm_vcc_man_wave_fill(CmVccManWork *w, float rad, float width);
void cm_vcc_man2_exec(CmVccManWork *w);
void cm_vcc_man2_restore(CmVccManWork *w);
void cm_vcc_man2_release(CmVccManWork *w);
short cm_vcc_man2_pts_get(int *w, char **pts, int *cols);
CmVccManWork *cm_vcc_man2_create(void *parent, int *model, int frm);
void venus_plate_rotate_yxz(int *ang);

#endif /* REZ_B_VENUS_VENUS_PLATE_H */
