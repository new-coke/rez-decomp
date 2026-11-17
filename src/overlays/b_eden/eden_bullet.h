/* Eden boss bullets. */

#ifndef REZ_B_EDEN_EDEN_BULLET_H
#define REZ_B_EDEN_EDEN_BULLET_H

#include "types_common.h"

struct EdenBulletEbWork;
struct EdenBulletUbWork;
struct EdenBulletKbWork;
struct EdenBulletNbWork;
struct EdenBulletFbWork;
struct EdenBulletBbWork;
struct EdenBulletXbWork;
struct EdenBulletRbWork;

void eden_bullet_vars_add(void);
void eden_bullet_vars_remove(void);
void eden_bullet_eb_exec(struct EdenBulletEbWork *w);
void eden_bullet_eb_disp(struct EdenBulletEbWork *w);
void eden_bullet_eb_release(struct EdenBulletEbWork *w);
void *eden_bullet_eb_create(void *parent, void *pos, int arg, void *cell_data);
void eden_bullet_ub_disp(struct EdenBulletUbWork *w);
void eden_bullet_curve_move(Vec3 *from, Vec3 *to, Vec3 *pos, Vec3 *axis, float t);
void eden_bullet_ub_exec(char *w);
void eden_bullet_ub_release(struct EdenBulletUbWork *w);
struct EdenBulletUbWork *eden_bullet_ub_create(void *parent, Vec3 *pos, Vec3 *dir, int kind);
void eden_bullet_ub_fire(void *parent, void *vp, int kind, int n);
void eden_bullet_kb_exec(struct EdenBulletKbWork *w);
void eden_bullet_kb_disp(struct EdenBulletKbWork *w);
void eden_bullet_kb_release(struct EdenBulletKbWork *w);
void *eden_bullet_kb_create(void *parent, void *pos, void *vel, void *ang, int src);
void eden_bullet_nb_exec(char *w);
void eden_bullet_nb_disp(struct EdenBulletNbWork *w);
void eden_bullet_nb_release(struct EdenBulletNbWork *w);
void *eden_bullet_nb_create(void *parent, Vec3 *pos, Vec3 *dir, int arg);
void eden_bullet_fb_disp(struct EdenBulletFbWork *w);
void eden_bullet_fb_exec(char *w);
void eden_bullet_fb_release(struct EdenBulletFbWork *w);
void *eden_bullet_fb_create(void *w, Vec3 *pos, Vec3 *dir, void *src);
void eden_bullet_fb_spawn(void *w, void *src, unsigned int mask);
void eden_bullet_bb_disp(struct EdenBulletBbWork *w);
void eden_bullet_bb_disp_fade(struct EdenBulletBbWork *w);
void eden_bullet_bb_step(struct EdenBulletBbWork *w);
void eden_bullet_bb_exec_fade(struct EdenBulletBbWork *w);
void eden_bullet_bb_exec(struct EdenBulletBbWork *w);
void eden_bullet_bb_release(struct EdenBulletBbWork *w);
struct EdenBulletBbWork *eden_bullet_bb_create(void *parent, Vec3 *pos, int *ang, int src);
void eden_bullet_xb_disp(struct EdenBulletXbWork *w);
void eden_bullet_xb_disp_fade(struct EdenBulletXbWork *w);
void eden_bullet_xb_step(struct EdenBulletXbWork *w);
void eden_bullet_xb_exec_fade(struct EdenBulletXbWork *w);
void eden_bullet_xb_exec(struct EdenBulletXbWork *w);
void eden_bullet_xb_release(struct EdenBulletXbWork *w);
struct EdenBulletXbWork *eden_bullet_xb_create(void *parent, Vec3 *pos, Vec3 *dir, char *src);
void eden_bullet_rb_step(struct EdenBulletRbWork *w);
void eden_bullet_rb_exec(struct EdenBulletRbWork *w);
void eden_bullet_rb_disp(struct EdenBulletRbWork *w);
void eden_bullet_rb_release(struct EdenBulletRbWork *w);
void *eden_bullet_rb_create(void *parent, int arg, Vec3 *pos, Vec3 *start, int ang);
int eden_bullet_area_check(Vec3 *pos, Vec3 *vel, Vec3 *out);
void eden_bullet_rotate_yxz(int *ang);

#endif /* REZ_B_EDEN_EDEN_BULLET_H */
