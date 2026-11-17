/* Tutorial 'Missile' shot. */

#ifndef REZ_STG20_MISSILE_H
#define REZ_STG20_MISSILE_H

#include "stg20.h"
#include "types_common.h"
#include "types_overlays.h"

struct MissileWork;

void missile_fire(NulWork *w, Vec3 *from);
void missile_shatter(struct MissileWork *w);
void missile_init(struct MissileWork *w, ShotArg *prm);
void missile_release(struct MissileWork *w);
void missile_exec(struct MissileWork *w);
void missile_disp(struct MissileWork *w);
void *missile_create(void *parent, void *prm);
int missile_marker_draw(void **w, ShotArg *prm);
int missile_vars_init(int *prm);
int missile_vars_release(void);
void missile_rotate_yxz(int *ang);

#endif /* REZ_STG20_MISSILE_H */
