/* Venus boss bullets. */

#ifndef REZ_B_VENUS_VENUS_BULLET_H
#define REZ_B_VENUS_VENUS_BULLET_H

#include "types_common.h"
#include "b_venus.h"

struct BBulletTailWork;
struct VenusTrailArg;
struct VenusGbManWork;
struct VenusBake;
struct VenusTail2Work;
struct VenusGBreakWork;
struct VenusGbWork;
struct VenusFbWork;

void venus_bullet_vars_add(void);
void venus_bullet_vars_remove(void);
void bbullet_tail_exec(struct BBulletTailWork *w);
void bbullet_tail_disp(struct BBulletTailWork *w);
struct BBulletTailWork *bbullet_tail_create(void *parent, void *owner, Vec3 *src, int n, int k);
void bbullet_tail_stop(struct BBulletTailWork *w);
void bbullet_tail2_exec(struct VenusTail2Work *w);
void bbullet_tail2_disp(struct VenusTail2Work *w);
void bbullet_tail2_release(struct VenusTail2Work *w);
int *bbullet_tail2_create(struct VenusTrailArg *desc);
void g_break_exec(struct VenusGBreakWork *w);
void g_break_disp(struct VenusGBreakWork *w);
int *g_break_create(int parent, int src, Vec3 *pos, int vel, int *ang);
void venus_gb_steer(struct VenusGbWork *w);
void venus_gb_move(struct VenusGbWork *w);
void venus_gb_break(struct VenusGbWork *w);
void venus_gb_exec(struct VenusGbWork *w);
void venus_gb_disp(struct VenusGbWork *w);
void venus_gb_release(struct VenusGbWork *w);
int *venus_gb_create(int *mgr, Vec3 *pos, Vec3 *dir, char *def, float rate1, float rate2);
void venus_gb_man_separate(void *work);
void venus_gb_man_exec(void *w);
void venus_gb_man_release(struct VenusGbManWork *w);
int venus_gb_man_create(char *parent, void *parts, void *pos);
void venus_gb_man_set_attr(int *w, short attr);
void venus_gb_man_break_near(void *w, Vec3 *pos);
int venus_gb_man_get_count(struct VenusGbManWork *w, int kind);
void venus_gb_man_add_count(struct VenusGbManWork *w, int delta, int kind);
void venus_gb_man_break_all(void *w);
void venus_gb_man_draw_bake(struct VenusBake *w, float t);
void venus_gb_man_draw_model(struct VenusGbManWork *w, int i, int mode, float t);
void venus_fb_move(struct VenusFbWork *w);
void venus_fb_exec(struct VenusFbWork *w);
void venus_fb_disp(struct VenusFbWork *w);
void venus_fb_release(struct VenusFbWork *w);
int *venus_fb_create(VenusFbManWork *w, int models, VenusFbManPod *pod, int ang, int ang_spd);
void venus_fb_man_place_pods(VenusFbManWork *w);
void venus_fb_man_fire(VenusFbManWork *w);
void venus_fb_man_fire_burst(VenusFbManWork *w);
void venus_fb_man_exec(VenusFbManWork *w);
void venus_fb_man_disp(VenusFbManWork *w);
void venus_fb_man_release(VenusFbManWork *w);
VenusFbManWork *venus_fb_man_create(void *parent, int models, int pos, int *ang);
int venus_fb_man_get_count(VenusFbManWork *w);
int venus_fb_man_count_shots(void *w);
void venus_fb_man_set_on(VenusFbManWork *w, int on, int delay);
void venus_fb_man_set_on_now(VenusFbManWork *w, int on);
void venus_fb_man_set_on_delay(VenusFbManWork *w, int on);
void venus_fb_man_revive(VenusFbManWork *w);
void venus_fb_man_stop(VenusFbManWork *w);
void venus_fb_man_start_burst(VenusFbManWork *w);
void venus_bullet_rotate_yxz(int *ang);

#endif /* REZ_B_VENUS_VENUS_BULLET_H */
