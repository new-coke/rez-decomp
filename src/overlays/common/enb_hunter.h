/* The Hunter's shot. */

#ifndef REZ_COMMON_ENB_HUNTER_H
#define REZ_COMMON_ENB_HUNTER_H

#include "types_overlays.h"

void enb_hunter_vars_register(void);
void enb_hunter_vars_unregister(void);
int enb_hunter_vars_init(int *fx_id);
int enb_chaser_vars_init(int *fx_id);
int enb_hunter_vars_release(void);
void enb_hunter_hit_check(EnbHunterWork *w);
void enb_hunter_crash_check(EnbHunterWork *w);
void enb_hunter_area_check(EnbHunterWork *w);
void enb_hunter_state_fly(EnbHunterWork *w);
void enb_hunter_state_break(EnbHunterWork *w);
void enb_hunter_exec(EnbHunterWork *w);
void enb_hunter_disp(EnbHunterWork *w);
void enb_hunter_init(void *task, int pos, int ang, int kind);
void enb_hunter_release(EnbHunterWork *w);
void *enb_hunter_create(void *parent, int pos, int ang, int kind);
void enb_hunter_models_init(void);
void enb_hunter_rotate_yxz(int *ang);

#endif /* REZ_COMMON_ENB_HUNTER_H */
