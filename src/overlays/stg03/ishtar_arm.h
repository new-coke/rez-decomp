/* Ishtar's arms, Area 3. */

#ifndef REZ_STG03_ISHTAR_ARM_H
#define REZ_STG03_ISHTAR_ARM_H

#include "types_overlays.h"

typedef struct IshtarArmWork IshtarArmWork;

void ishtar_arm_vars_register(void);
void ishtar_arm_vars_unregister(void);
int ishtar_arm_vars_init(int *fx_id);
int ishtar_arm_vars_release(void);
int ishtar_arm_get_init_arg(void);
int ishtar_arm_find_node_pos(float *out, unsigned int *node);
void ishtar_arm_tip_pos(void *obj, IshtarArmWork *w, float *out);
void ishtar_arm_init(void *obj, IshtarArmWork *w, int flag, int *kind);
void ishtar_arm_release(void *obj, IshtarArmWork *w);
void ishtar_arm_fire_shot(void *obj, IshtarArmWork *w);
int ishtar_arm_collect_mounts(int *pos_a, int *pos_b, unsigned int *node, int cnt);
void ishtar_arm_pose_model(void *obj, IshtarArmWork *w);
void ishtar_arm_shatter(void *obj, IshtarArmWork *w);
int ishtar_arm_next_state(void *obj, IshtarArmWork *w, int n, int state);
void ishtar_arm_follow(void *obj, IshtarArmWork *w);
int ishtar_arm_held_state_ray(void *obj, IshtarArmWork *w);
int ishtar_arm_held_state_return(void *obj, IshtarArmWork *w);
int ishtar_arm_state_release(void *obj, IshtarArmWork *w);
int ishtar_arm_state_fly(void *obj, IshtarArmWork *w);
int ishtar_arm_state_aim(void *obj, IshtarArmWork *w);
int ishtar_arm_state_dive(void *obj, IshtarArmWork *w);
void ishtar_arm_update(void *obj, IshtarArmWork *w);
void ishtar_arm_exec_appear(void *obj, IshtarArmWork *w);
void ishtar_arm_exec_normal(void *obj, IshtarArmWork *w);
void ishtar_arm_exec_leave(void *obj, IshtarArmWork *w);
void ishtar_arm_exec_destroyed(void *obj, IshtarArmWork *w);
void ishtar_arm_exec(void *obj, IshtarArmWork *w);
void ishtar_arm_pre_exec(void *obj, IshtarArmWork *w);
void ishtar_arm_post_exec(void *obj, IshtarArmWork *w);
void ishtar_arm_draw_nodes(unsigned int *node, int angle);
void ishtar_arm_disp_body(void *obj, IshtarArmWork *w);
void ishtar_arm_disp(void *obj, IshtarArmWork *w);
int ishtar_arm_create(void *parent, char *flag, int *prm);
void ishtar_arm_detach(void *obj);
void ishtar_arm_held_update(void *obj);
void ishtar_arm_rotate_yxz(int *ang);

#endif /* REZ_STG03_ISHTAR_ARM_H */
