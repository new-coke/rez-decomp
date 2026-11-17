/* Star dust. */

#ifndef REZ_COMMON_EFF_STAR_H
#define REZ_COMMON_EFF_STAR_H

#include "types_overlays.h"
#include "types_common.h"

void calc_center(EffStarWork *w);
void eff_star_place(float spread, EffStarWork *w, Star *star);
void eff_star_move(Star *star, Vec3 *delta);
void *eff_star_add_streak(void *pkt, Star *star);
void eff_star_wrap(float cx, float cy, float cz, float r, Star *star);
void eff_star_place_all(EffStarWork *w, FogArg *cfg);
void eff_star_move_all(EffStarWork *w);
void eff_star_draw_all(EffStarWork *w);
void eff_star_init(EffStarWork *w, FogArg *cfg);
void eff_star_release(EffStarWork *w);
void eff_star_exec(EffStarWork *w);
void eff_star_disp(EffStarWork *w);
void *eff_star_create(void *parent, FogArg *cfg);
void eff_star_set_color(EffStarWork *w, unsigned int color);
unsigned int eff_star_get_color(EffStarWork *w);
void eff_star_set_count(EffStarWork *w, int n);
int eff_star_get_count(EffStarWork *w);
int eff_star_get_max(EffStarWork *w);
void eff_star_set_limit(float dis, EffStarWork *w);
void eff_star_col_init(EffStarColWork *work, EffStarWork *star, unsigned int to, int frames);
void eff_star_col_release(EffStarColWork *w);
void eff_star_col_exec(EffStarColWork *w);
int eff_star_col_create(void *star, unsigned int color, int frames);

#endif /* REZ_COMMON_EFF_STAR_H */
