/* Perfect Wall manager. */

#ifndef REZ_COMMON_EN_PERFECTWALL_MANAGER_H
#define REZ_COMMON_EN_PERFECTWALL_MANAGER_H

#include "types_overlays.h"
#include "types_common.h"

struct EnPerfectWallPlace;

void en_perfectwall_manager_vars_register(void);
void en_perfectwall_manager_vars_unregister(void);
int en_perfectwall_manager_vars_init(int *id);
int en_perfectwall_manager_vars_release(void);
void en_perfectwall_manager_calc_points(EnPerfectWallManagerWork *w);
void en_perfectwall_manager_swing(EnPerfectWallManagerWork *w);
void en_perfectwall_manager_lightning_create(EnPerfectWallManagerWork *w);
void en_perfectwall_manager_lightning_finish(EnPerfectWallManagerWork *w);
void en_perfectwall_manager_lightning_check(EnPerfectWallManagerWork *w);
void en_perfectwall_manager_collision(EnPerfectWallManagerWork *w);
void en_perfectwall_manager_joint_timer(EnPerfectWallManagerWork *w);
float en_perfectwall_manager_close_gap_curve(float t);
float en_perfectwall_manager_close_rot_curve(float t);
float en_perfectwall_manager_unite_gap_curve(float t);
float en_perfectwall_manager_unite_rot_curve(float t);
float en_perfectwall_manager_rotate_curve(float t);
void en_perfectwall_manager_state_close(EnPerfectWallManagerWork *w);
void en_perfectwall_manager_state_unite(EnPerfectWallManagerWork *w);
void en_perfectwall_manager_state_rotate(EnPerfectWallManagerWork *w);
void en_perfectwall_manager_charge(EnPerfectWallManagerWork *w);
void en_perfectwall_manager_exec(EnPerfectWallManagerWork *w);
void en_perfectwall_manager_init(EnPerfectWallManagerWork *w, struct EnPerfectWallPlace *place);
void en_perfectwall_manager_release(void);
void *en_perfectwall_manager_create(void *parent, void *prm);
void en_perfectwall_manager_set_ends(EnPerfectWallManagerWork *w, int kind, Vec3 *src);
void en_perfectwall_manager_wall_gone(EnPerfectWallManagerWork *w, int bit);
void en_perfectwall_manager_get_point(EnPerfectWallManagerWork *w, int kind, Vec3 *pos, int *ang);
void en_perfectwall_manager_get_angles(EnPerfectWallManagerWork *w, int kind, int *ang);
int en_perfectwall_manager_get_state(EnPerfectWallManagerWork *w);
int en_perfectwall_manager_is_united(EnPerfectWallManagerWork *w);
int en_perfectwall_manager_has_lightning(EnPerfectWallManagerWork *w);
int en_perfectwall_manager_side_score(EnPerfectWallManagerWork *w, int kind);
void en_perfectwall_manager_se_hit(EnPerfectWallManagerWork *w, Vec3 *pos);
void en_perfectwall_manager_se_break(EnPerfectWallManagerWork *w, Vec3 *pos);
void en_perfectwall_manager_se_reset(void);

#endif /* REZ_COMMON_EN_PERFECTWALL_MANAGER_H */
