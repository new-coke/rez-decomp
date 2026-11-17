/* Mars boss bullets. */

#ifndef REZ_B_MARS_MARS_BULLET_H
#define REZ_B_MARS_MARS_BULLET_H

#include "types_common.h"
#include "types_overlays.h"

struct MarsDebrisWork;
struct MarsSbWork;
struct MarsSbmWork;
struct MarsMbWork;
struct MarsMbmWork;
struct MarsNbWork;
struct MarsBurstWork;
struct MarsSparkWork;
struct MarsBrLArg;

void mars_bullet_vars_add(void);
void mars_bullet_vars_remove(void);
void mars_mb_move(struct MarsMbWork *w);
void mars_mb_disp_fade(struct MarsMbWork *w);
void mars_mb_exec(struct MarsMbWork *w);
void mars_mb_disp(struct MarsMbWork *w);
void mars_mb_release(struct MarsMbWork *w);
int *mars_mb_create(void *parent, Vec3 *src_pos, Vec3 *v, int mdl, int break_mdl, int end_flag, float t);
void mars_mbm_exec(struct MarsMbmWork *w);
int *mars_mbm_create(void *parent, int src_pos, int mdl, int break_mdl, int end_flag);
void mars_nb_move(struct MarsNbWork *w);
void mars_nb_disp_fade(struct MarsNbWork *w);
void mars_nb_exec(struct MarsNbWork *w);
void mars_nb_disp(struct MarsNbWork *w);
void mars_nb_release(struct MarsNbWork *w);
int *mars_nb_create(void *parent, Vec3 *pos, Vec3 *dir, int mdl, int break_mdl, int kind);
void mars_nb_fire_ring(void *parent, Vec3 *pos, int mdl, int break_mdl, int kind);
void mars_sb_home(struct MarsSbWork *w);
void mars_sb_disp_fade(struct MarsSbWork *w);
void mars_sb_exec(struct MarsSbWork *w);
void mars_sb_disp(struct MarsSbWork *w);
void mars_sb_release(struct MarsSbWork *w);
void *mars_sb_create(void *parent, void *owner, Vec3 *pos, Vec3 *v, ModelPart *mdl, ModelPart *mdl_b, int break_mdl, int *end_flag, float spd);
void mars_sbm_exec(struct MarsSbmWork *w);
int *mars_sbm_create(void *parent, Vec3 *pos, int mdl, int mdl_b, int break_mdl, int end_flag);
void mars_b_br_verts_build(BossV4 *dst);
void mars_b_br_verts_init(void);
void mars_b_br_init(struct MarsBurstWork *w, int *src);
void mars_b_br_exec(struct MarsBurstWork *w);
void mars_b_br_disp(struct MarsBurstWork *w);
void *mars_b_br_create(void *parent, void *arg);
void mars_br_l_init(struct MarsSparkWork *w, struct MarsBrLArg *src);
void mars_br_l_exec(struct MarsSparkWork *w);
void mars_br_l_disp(struct MarsSparkWork *w);
void *mars_br_l_create(void *parent, void *arg);
void mars_br_l_spawn(void *parent, Vec3 *start, Vec3 *span, int col);
void mars_b_br_d_init(struct MarsDebrisWork *w, void **arg);
void mars_b_br_d_exec(struct MarsDebrisWork *w);
void mars_b_br_d_disp(struct MarsDebrisWork *w);
void *mars_b_br_d_create(void *parent, void *arg);
void mars_bullet_rotate_yxz(int *ang);

#endif /* REZ_B_MARS_MARS_BULLET_H */
