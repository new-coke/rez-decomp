/* Venus boss core. */

#ifndef REZ_B_VENUS_VENUS_CORE_H
#define REZ_B_VENUS_VENUS_CORE_H

#include "types_common.h"

struct VenusCoreWork;
struct VenusEndCoreWork;
struct Venus1stDemoWork;
struct VenusFogWork;

void venus_core_vars_add(void);
void venus_core_vars_remove(void);
void venus_core_spin(struct VenusCoreWork *w);
void venus_core_set_lock_points(struct VenusCoreWork *w);
void venus_core_exec(struct VenusCoreWork *w);
void venus_core_disp(void *work);
void venus_core_disp_broken(void *work);
void venus_core_release(struct VenusCoreWork *w);
int *venus_core_create(void *parent, int parts, Vec3 *pos);
void venus_core_start(struct VenusCoreWork *w, int on);
void venus_core_break(struct VenusCoreWork *w);
void venus_end_core_exec(struct VenusEndCoreWork *w);
void venus_end_core_disp(struct VenusEndCoreWork *w);
int *venus_end_core_create(void *parent, int parts, Vec3 *pos, int col, int move_frm, int fade_frm);
void venus_core_rotate_yxz(int *ang);
void venus_1st_demo_calc_bars(int *frm, int *bars);
void venus_1st_demo_exec(struct Venus1stDemoWork *w);
void venus_1st_demo_release(struct Venus1stDemoWork *w);
int *venus_1st_demo_create(void *parent, int cb, int cb_arg);
void star_color_apply(void);
void venus_fog_vars_add(void);
void venus_fog_vars_remove(void);
void venus_fog_exec(struct VenusFogWork *w);

#endif /* REZ_B_VENUS_VENUS_CORE_H */
