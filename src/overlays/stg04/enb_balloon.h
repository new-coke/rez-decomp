/* Balloon payload, Area 4. */

#ifndef REZ_STG04_ENB_BALLOON_H
#define REZ_STG04_ENB_BALLOON_H

#include "stg04.h"
#include "types_common.h"

void enb_balloon_vars_register(void);
void enb_balloon_vars_unregister(void);
int enb_balloon_vars_init(int *fx_id);
int enb_balloon_vars_init_core(int *fx_id);
int enb_balloon_vars_release(void);
void enb_balloon_calc_lock_point(EnbBalloonWork *w);
void enb_balloon_release_core(EnbBalloonWork *w);
void enb_balloon_release_fragment(EnbBalloonWork *w, int i);
void enb_balloon_shatter_core(EnbBalloonWork *w);
void enb_balloon_break_fragment(EnbBalloonWork *w, int i);
void enb_balloon_hit_check(EnbBalloonWork *w);
void enb_balloon_crash_check(EnbBalloonWork *w);
void enb_balloon_clip_fragments(EnbBalloonWork *w);
void enb_balloon_setup_a(EnbBalloonWork *w, Vec3 *pos, int *ang);
void enb_balloon_setup_b(EnbBalloonWork *w, Vec3 *pos, int *ang);
void enb_balloon_state_a(EnbBalloonWork *w);
void enb_balloon_state_b(EnbBalloonWork *w);
void enb_balloon_exec(EnbBalloonWork *w);
void enb_balloon_disp(EnbBalloonWork *w);
void enb_balloon_init(EnbBalloonWork *w, Vec3 *pos, void *place);
void enb_balloon_release(EnbBalloonWork *w);
EnbBalloonWork *enb_balloon_create(void *parent, Vec3 *pos, void *place);
void enb_balloon_models_init(void);
void enb_balloon_rotate_yxz(int *ang);

#endif /* REZ_STG04_ENB_BALLOON_H */
