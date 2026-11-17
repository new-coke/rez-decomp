/* Perfect Wall half. */

#ifndef REZ_COMMON_EN_PERFECTWALL_H
#define REZ_COMMON_EN_PERFECTWALL_H

#include "types_overlays.h"

struct EnPerfectWallPlace;

float depth_fade_rate(float z_near, float z_far, float a_near, float a_far);
void motion_sample_color(void *mot, float t, float z_near, float z_far, float a_near, float a_far, float *out);
unsigned int motion_sample_rgba(void *mot, float t, float z_near, float z_far, float a_near, float a_far);
float motion_sample_alpha(void *mot, float t, float z_near, float z_far, float a_near, float a_far);
void en_perfectwall_vars_register(void);
void en_perfectwall_vars_unregister(void);
void en_perfectwall_calc_points(void *obj, EnPerfectWallWork *w);
void en_perfectwall_calc_ends(void *obj, EnPerfectWallWork *w);
void en_perfectwall_calc_tip(void *obj, EnPerfectWallWork *w);
void en_perfectwall_calc_shape(void *obj, int *work);
void en_perfectwall_shatter(void *obj, void *work);
void en_perfectwall_hit_check(void *obj, EnPerfectWallWork *w);
float en_perfectwall_flash_curve(float t);
void en_perfectwall_reset_locks(void *obj, EnPerfectWallWork *w);
void en_perfectwall_follow_manager(void *obj, EnPerfectWallWork *w);
void en_perfectwall_state_break(void *obj, void *w);
void en_perfectwall_state_float_away(void *obj, EnPerfectWallWork *w);
void en_perfectwall_exec_normal(void *obj, EnPerfectWallWork *w);
void en_perfectwall_exec_destroyed(void *obj);
void en_perfectwall_exec(void *obj, void *w);
void en_perfectwall_pre_exec(void *obj, EnPerfectWallWork *w);
void en_perfectwall_post_exec(void *obj, int *w);
unsigned int color_add_saturate(unsigned int col_a, unsigned int col_b);
void en_perfectwall_draw_parts(void *obj, EnPerfectWallWork *w);
void en_perfectwall_draw_glow(void *obj, int *w);
void en_perfectwall_draw(void *obj, EnPerfectWallWork *w);
void en_perfectwall_disp_body(void *obj, EnPerfectWallWork *w);
void en_perfectwall_disp(void *obj, void *w);
void en_perfectwall_init(void *obj, EnPerfectWallWork *w);
void en_perfectwall_release(void *obj, EnPerfectWallWork *w);
int en_perfectwall_create(void *parent, struct EnPerfectWallPlace *place);
void en_perfectwall_models_init(void);
void en_perfectwall_rotate_yxz(int *ang);

#endif /* REZ_COMMON_EN_PERFECTWALL_H */
