/* Spring joint chains for boss limbs. */

#ifndef REZ_COMMON_BOSS_JOINT_H
#define REZ_COMMON_BOSS_JOINT_H

#include "types_overlays.h"
#include "types_common.h"

void boss_joint_chain_init(BossJointLink *node, Vec3 *pos, Vec3 *dir, Vec3 *up, int cb, int post_cb, int cb_arg, int n, float stiff, float damp, float up_rate);
int boss_joint_up_from_dir(Vec3 *dir, Vec3 *up, int ang, int no_flip);
void boss_joint_chain_twist(BossJointLink *node);
void boss_joint_chain_step(BossJointLink *node);
BossJointLink *boss_joint_step_fwd(BossJointLink *node, BossJointLink *src, BossJointLink *dst, int *count);
void boss_joint_step_back(BossJointLink *node, BossJointLink *src, BossJointLink *dst);
void boss_joint_step_both(BossJointLink *node, BossJointLink *pool, float t);
void boss_joint_root_set_up(BossJointLink *chain, Vec3 *pos, Vec3 *dir, Vec3 *up, float roll_deg);
void boss_joint_root_set(BossJointLink *chain, Vec3 *pos, Vec3 *dir, float roll_deg);
void boss_joint_set_pos(BossJointLink *node, Vec3 *pos);
void boss_joint_get_pose(BossJointLink *node, Vec3 *pos, int *ang);
void boss_joint_chain_set_spring(BossJointLink *node, float stiff, float damp, float up_rate);
void boss_joint_chain_set_all(BossJointLink *node, void *cb, int post_cb, float len, float stiff, float damp, float up_rate);
void boss_joint_chain_reset(BossJointLink *node);
void boss_joint_chain_set_len(BossJointLink *node, float len);
void boss_joint_chain_set_cb(BossJointLink *node, void *cb, int post_cb, int cb_arg);
void boss_joint_chain_pull(BossJointLink *node, float rate);
void boss_joint_count_r(WireObj *obj, int *n);
int boss_joint_count(WireObj *obj);
void boss_joint_tree_collect(WireObj *obj, int *cur);
void boss_joint_chain_build(WireObj *obj, BossJointLink *node, int *base_ang);
void boss_joint_axes_draw(float len);
void boss_joint_tree_draw(int tree, int draw_cb, int *cur, int *scl);
void boss_joint_tree_draw_axes(int tree, int draw_cb, int *cur);
void boss_joint_tree_draw_rest(WireObj *obj, int order);
void boss_joint_draw(int *w);
void boss_joint_create_tree(WireObj *obj, int draw_cb, BossJointLink **w);
void boss_joint_create(BossJointLink **w, int n);
void boss_joint_set_pose(char *w, int *ang, Vec3 *pos);
void boss_joint_update(BossJointLink **w, Vec3 *root, Vec3 *tip, float t);
void boss_joint_release(void **w);
void boss_joint_rotate_zyx(int *ang);
void boss_joint_rotate_yxz(int *ang);

#endif /* REZ_COMMON_BOSS_JOINT_H */
