/* Uranus boss snake form. */

#ifndef REZ_B_URANUS_URANUS_SNAKE_H
#define REZ_B_URANUS_URANUS_SNAKE_H

#include "types_common.h"
#include "b_uranus.h"
#include "types_overlays.h"

struct UranusSnakeWork;
struct UranusSnakeMove;

void uranus_snake_vars_add(void);
void uranus_snake_vars_remove(void);
int uranus_snake_move_t4(struct UranusSnakeWork *w, struct UranusSnakeMove *mv, Vec3 *out);
int uranus_snake_move_t3(struct UranusSnakeWork *w, struct UranusSnakeMove *mv, Vec3 *out);
int uranus_snake_move_t2(struct UranusSnakeWork *w, struct UranusSnakeMove *mv, Vec3 *out);
int uranus_snake_move_sn(struct UranusSnakeWork *w, struct UranusSnakeMove *mv, Vec3 *out);
int uranus_snake_move(struct UranusSnakeWork *w, int move, Vec3 *out);
void uranus_snake_drift(struct UranusSnakeWork *w);
void uranus_snake_chain_step(struct UranusSnakeWork *w);
void uranus_snake_pick_move(struct UranusSnakeWork *w, int attack);
void uranus_snake_exec(struct UranusSnakeWork *w);
void uranus_snake_reset_pos(struct UranusSnakeWork *w, Vec3 *pos);
void uranus_snake_nodes_build(struct UranusSnakeWork *w, int data, char *link_pts, char *pt_pts);
void uranus_snake_release(struct UranusSnakeWork *w);
void uranus_snake_disp(struct UranusSnakeWork *w);
void uranus_snake_switch_cb(struct UranusSnakeWork *w);
struct UranusSnakeWork *uranus_snake_create(void *parent, UranusPartWork *pm, Vec3 **pts, UranusNode **out, int slot, ModelPart **parts);
void uranus_snake_nodes_set(UranusNode *root, int data, Vec3 *link_pts, Vec3 *pt_pts);
struct UranusSnakeWork *uranus_snake_create_from(void *parent, UranusPartWork *pm, Vec3 **pts, UranusNode **out, int slot, UranusNode *shape, ModelPart **parts);
void boss_se_play(int id, int lock, int pos);
void boss_vol_play(int id, int arg);
void boss_vol_stop(int id);
void boss_bgm_exit_cb(BgmMan *bgm);
void boss_se_stop_all(void);
BgmMan *boss_bgm_start(void *parent);
void boss_adx_request(void);
void boss_bgm_switch(int cb, int cb_arg, int idx);
void boss_pcm_load(int i);
void boss_pcm_start(void);

#endif /* REZ_B_URANUS_URANUS_SNAKE_H */
