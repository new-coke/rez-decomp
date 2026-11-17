#ifndef REZ_STG03_E_SHOT_H
#define REZ_STG03_E_SHOT_H

#include "types_common.h"
#include "types_overlays.h"

void tiamat_laser_draw_polyline(void *ln, Vec3 *pts, int n, int capped);
void e_shot_vars_register(void);
void e_shot_vars_unregister(void);
int e_shot_vars_init(int *fx_id);
int e_shot_vars_release(void);
int enq_missile_vars_init(int *fx_id);
int dle_shot_vars_init(int *fx_id);
int dle_shot2_vars_init(int *fx_id);
int dle_shot3_vars_init(int *fx_id);
int dle_shot4_vars_init(int *fx_id);
int dle_shot5_vars_init(int *fx_id);
int dle_shot6_vars_init(int *fx_id);
int etb_shot_vars_init(int *fx_id);
void e_shot_setup_model5(ShotDesc *w, EShotParamSet *set);
void e_shot_common_event(ShotHit *hit);
void e_shot_setup_model15(ShotDesc *w, EShotParamSet *set);
void e_shot_enkidu_event(ShotHit *hit);
void e_shot_sprite_draw3d(float off, float wid, float hgt, Vec3 *pos, int angle, unsigned int col, int tex);
void e_shot_setup_model60(ShotDesc *w, EShotParamSet *set);
void e_shot_ishtar_event(ShotHit *hit);
void e_shot_event(ShotHit *hit);
int e_shot_fire(void *parent, Vec3 *pos, Vec3 *dir, int level);
void e_shot_initial_angles(int *ang, Vec3 *pos, Vec3 *dir, int level);

#endif /* REZ_STG03_E_SHOT_H */
