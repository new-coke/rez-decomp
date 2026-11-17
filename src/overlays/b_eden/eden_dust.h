/* Eden boss piece motion. */

#ifndef REZ_B_EDEN_EDEN_DUST_H
#define REZ_B_EDEN_EDEN_DUST_H

#include "types_common.h"
#include "b_eden.h"

void eden_dust_vars_add(void);
void eden_dust_vars_remove(void);
void eden_dust_item_seed(EdenGrpItem *w, Vec3 *base, char *src, Vec3 *range, int k);
void eden_dust_piece_seed(void *parent, EdenGrp *w, Vec3 *base, Vec3 *range);
void eden_dust_lock_remove(EdenGrp *w);
void eden_dust_item_retarget(EdenGrpItem *w, Vec3 *base, Vec3 *range);
void eden_dust_piece_retarget(EdenGrp *w, Vec3 *base, Vec3 *range);
int eden_dust_item_step(void *w, EdenGrp *it, Vec3 *pos, int *ang, Vec3 *move, int on);
int eden_dust_shard_step(char *shard, Vec3 *pos, int *ang, Vec3 *move, int on);
void eden_dust_grp_seed(void *w, EdenOrb *item, Vec3 *base, Vec3 *rr, int arg);
void eden_dust_grp_retarget(EdenOrb *w, Vec3 *base, Vec3 *range);
void eden_dust_orbit_point(Vec3 *out, int *ang, float dist, float rad);
void eden_dust_orbit_step(EdenGrp *w, Vec3 *base, Vec3 *out);
void eden_dust_grp_step(EdenGrp *grp, Vec3 *pos, int on);
void eden_dust_grp_piece_step(EdenGrp *grp, EdenGrpItem *it, Vec3 *move, int on);
void eden_dust_grp_init(void *owner, EdenGrp *grp, Vec3 *pos, void **model, int tgt, Vec3 *org, int count, int *ang, int kind, float scl);
void eden_dust_grp_check(void *w, void *orbs, void *norb, char *grp);
int eden_dust_grps_update(void *w, void *orbs, void *norb, EdenGrp *grp, Vec3 *pos, int n, int lim);
void eden_dust_grps_draw(void *owner, char *grp, int n);
void eden_dust_shock_wave(void *w, void *orbs, void *norb, void *pos);
void eden_dust_rotate_yxz(int *ang);

#endif /* REZ_B_EDEN_EDEN_DUST_H */
