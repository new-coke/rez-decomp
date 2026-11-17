/* Earth boss bubbles and mines. */

#ifndef REZ_B_EARTH_EARTH_BUBBLE_H
#define REZ_B_EARTH_EARTH_BUBBLE_H

#include "b_earth.h"
#include "types_common.h"
#include "types_overlays.h"

struct BossBubble;
struct BossMine;
struct BossMineArg;
struct BossDebrisWork;

void earth_bubble_init(struct BossBubble *w, struct BossBubble *arg);
void earth_bubble_release(struct BossBubble *w);
void earth_bubble_exec_carried(struct BossBubble *w);
void earth_bubble_disp_carried(struct BossBubble *w);
void earth_bubble_exec(struct BossBubble *w);
void earth_bubble_disp(struct BossBubble *w);
struct BossBubble *earth_bubble_create(void *parent, struct BossBubble *arg);
void earth_bubbles_setup(BossPartList *parts);
void earth_bubbles_cleanup(void);
int earth_bubble_spawn(void *parent, Vec3 *pos, Vec3 *vel, int flags, int end_flag, int level);
void earth_bubble_manage_exec(BossBubbleGen *w);
BossBubbleGen *earth_bubble_manage_create(BossMainWork *w, Vec3 *pos, int mode, int end_flag, int delay);
int earth_bubble_spawn_carried(void *parent, int carrier, int *cell, int scale_cell, float speed_scale);
void earth_mine_init(struct BossMine *w, struct BossMineArg *arg);
void earth_mine_exec(struct BossMine *w);
void earth_mine_release(struct BossMine *w);
void earth_mine_disp(struct BossMine *w);
struct BossMine *earth_mine_create(void *parent, void *arg);
void earth_break2_init(struct BossDebrisWork *w, struct BossDebrisWork *arg);
void earth_break2_release(int *w);
void earth_break2_exec(struct BossDebrisWork *w);
void earth_break2_disp(struct BossDebrisWork *w);
int earth_break2_create(void *parent, Vec3 *pos, Vec3 *vel, Vec3 *size, int count, float speed, unsigned int col);
void earth_bubble_rotate_yxz(int *ang);

#endif /* REZ_B_EARTH_EARTH_BUBBLE_H */
