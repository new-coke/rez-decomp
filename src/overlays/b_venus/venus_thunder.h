/* Venus boss thunder. */

#ifndef REZ_B_VENUS_VENUS_THUNDER_H
#define REZ_B_VENUS_VENUS_THUNDER_H

#include "b_venus.h"
#include "types_common.h"

struct ThBallWork;

void venus_thunder_vars_add(void);
void venus_thunder_vars_remove(void);
void cm_thl_exec(VenusPartObj *w);
void cm_thl_disp(VenusPartObj *w);
void cm_thl_release(VenusPartObj *w);
void *cm_thl_create(VenusPartSet *mng, ThlRec *rec);
void cm_thl_man_exec(VenusPartSet *w);
void cm_thl_man_release(VenusPartSet *w);
VenusPartSet *cm_thl_man_create(void *parent, ThlRec *rec, int n);
void cm_thl_man_set_fade(VenusPartSet *w, int fade);
void cm_thl_man_jolt(VenusPartSet *s);
void cm_thl_rec_init(ThlRec *w, Vec3 *from, Vec3 *to);
void cm_thl_disp_th(VenusPartObj *w);
void th_ball_arms_step(struct ThBallWork *w);
void th_ball_move(struct ThBallWork *w);
void th_ball_exec(struct ThBallWork *w);
void th_ball_disp(struct ThBallWork *w);
void th_ball_release(struct ThBallWork *w);
int *th_ball_create(void *parent, void *models, Vec3 *pos, Vec3 *center);
void th_ball_ring_create(void *parent, void *models, Vec3 *center);
void th_ball2_exec(int *w);
int *th_ball2_create(void *parent);
void venus_thunder_rotate_yxz(int *ang);

#endif /* REZ_B_VENUS_VENUS_THUNDER_H */
