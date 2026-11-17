/* Tiamat's laser. */

#ifndef REZ_COMMON_TIAMAT_LASER_H
#define REZ_COMMON_TIAMAT_LASER_H

#include "types_overlays.h"
#include "types_common.h"

void tiamat_laser_vars_register(void);
void tiamat_laser_vars_unregister(void);
int tiamat_laser_vars_init(int *fx_id);
int tiamat_laser_vars_release(void);
int tiamat_laser_get_init_arg(void);
void tiamat_laser_init(TiamatLaserWork *w, int *prm);
void tiamat_laser_release(TiamatLaserWork *w);
void tiamat_laser_set_state(TiamatLaserWork *w, int state);
void tiamat_laser_state_move(TiamatLaserWork *w);
void tiamat_laser_state_away(TiamatLaserWork *w);
void tiamat_laser_state_hit(TiamatLaserWork *w);
void tiamat_laser_state_turn(TiamatLaserWork *w);
void tiamat_laser_shot_down(TiamatLaserWork *w);
void tiamat_laser_exec(TiamatLaserWork *w);
void tiamat_laser_draw_beam(TiamatLaserWork *w, Vec3 *pts, int n);
void tiamat_laser_disp_straight(TiamatLaserWork *w);
void tiamat_laser_disp_bent(TiamatLaserWork *w);
void tiamat_laser_disp(TiamatLaserWork *w);
void *tiamat_laser_create(void *parent, int pos);
void tiamat_laser_attack_ray_release(TiamatLaserAttackRayWork *w);
void tiamat_laser_attack_ray_exec(TiamatLaserAttackRayWork *w);
void tiamat_laser_attack_ray_disp(TiamatLaserAttackRayWork *w);
void *tiamat_laser_attack_ray_create(void *parent, void *prm);
void tiamat_laser_turn_eff_init(TiamatLaserTurnEffWork *w, Vec3 *pos);
void tiamat_laser_turn_eff_release(TiamatLaserTurnEffWork *w);
void tiamat_laser_turn_eff_exec(TiamatLaserTurnEffWork *w);
void tiamat_laser_turn_eff_disp(TiamatLaserTurnEffWork *w);
void *tiamat_laser_turn_eff_create(void *parent, void *prm);
void tiamat_laser_rotate_yxz(int *ang);
void tiamat_laser_draw_polyline(void *ln, Vec3 *pts, int n, int capped);

#endif /* REZ_COMMON_TIAMAT_LASER_H */
