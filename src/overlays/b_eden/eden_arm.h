/* Eden boss arm form. */

#ifndef REZ_B_EDEN_EDEN_ARM_H
#define REZ_B_EDEN_EDEN_ARM_H

#include "types_common.h"

struct EdenArmJointWork;
struct EdenArmManWork;
struct EdenArmAttack;

void eden_arm_vars_add(void);
void eden_arm_vars_remove(void);
void eden_arm_joint_exec(struct EdenArmJointWork *w);
void eden_arm_joint_release(struct EdenArmJointWork *w);
int *eden_arm_joint_create(void *parent, int node, int node2);
void eden_arm_joint_set_target(struct EdenArmJointWork *w, Vec3 *target, Vec3 *pos, Vec3 *dir);
void eden_arm_joint_set_spring(struct EdenArmJointWork *w, float stiff, float damp);
void eden_arm_joint_set_anchor(struct EdenArmJointWork *w, int anchor, float t);
void eden_arm_attack_start(struct EdenArmAttack *w, Vec3 *base, float x, float y);
void eden_arm_attack_step(struct EdenArmAttack *atk);
void eden_arm_orbit_offset(int ang, Vec3 *out);
void eden_arm_man_follow(struct EdenArmManWork *w);
void eden_arm_man_face_player(struct EdenArmManWork *w);
void eden_arm_man_bank(struct EdenArmManWork *w);
void eden_arm_man_base_update(struct EdenArmManWork *w);
void eden_arm_man_exec(struct EdenArmManWork *arm);
void eden_arm_man_release(struct EdenArmManWork *w);
int *eden_arm_man_create(void *parent, int *arg);
void eden_arm_man_get_counts(struct EdenArmManWork *w, int *nchain, int *nlink);
void eden_arm_man_get_link(struct EdenArmManWork *w, int i, int j, Vec3 **pos, int **ang);
int eden_arm_man_get_chain(struct EdenArmManWork *w, int i);
void eden_arm_man_attack(struct EdenArmManWork *w, int i, int flags);
void eden_arm_man_rotate_start(struct EdenArmManWork *w);
void eden_arm_man_next_mode(struct EdenArmManWork *w);

#endif /* REZ_B_EDEN_EDEN_ARM_H */
