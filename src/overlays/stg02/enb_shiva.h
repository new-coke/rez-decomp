/* Shiva's missiles, Area 2. */

#ifndef REZ_STG02_ENB_SHIVA_H
#define REZ_STG02_ENB_SHIVA_H

#include "types_common.h"

struct EnbShivaWork;

void enb_shiva_vars_register(void);
void enb_shiva_vars_unregister(void);
int enb_shiva_vars_init(int *prm);
int enb_shiva_vars_release(void);
void enb_shiva_calc_lock_point(struct EnbShivaWork *w);
void enb_shiva_shatter(struct EnbShivaWork *w);
void enb_shiva_slot_cb(struct EnbShivaWork *w, int *ev);
void enb_shiva_hit_check(struct EnbShivaWork *w);
void enb_shiva_hit_player(struct EnbShivaWork *w);
void enb_shiva_clip_check(struct EnbShivaWork *w);
void enb_shiva_fall_init(struct EnbShivaWork *w);
void enb_shiva_fall(struct EnbShivaWork *w);
float enb_shiva_fall_spd(struct EnbShivaWork *w, float dt);
void enb_shiva_state_rise(struct EnbShivaWork *w);
void enb_shiva_state_fall(struct EnbShivaWork *w);
void enb_shiva_state_turn(struct EnbShivaWork *w);
void enb_shiva_state_attack(struct EnbShivaWork *w);
void enb_shiva_exec(struct EnbShivaWork *w);
void enb_shiva_disp(struct EnbShivaWork *w);
void enb_shiva_init(struct EnbShivaWork *w, Vec3 *pos, int *ang);
void enb_shiva_release(struct EnbShivaWork *w);
void *enb_shiva_create(void *parent, int pos, int ang);
void enb_shiva_nodes_init(void);
void enb_shiva_rotate_yxz(int *ang);

#endif /* REZ_STG02_ENB_SHIVA_H */
