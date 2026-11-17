/* Archangel children, Area 5. */

#ifndef REZ_STG05_ARCH_ANGEL_CHILD_H
#define REZ_STG05_ARCH_ANGEL_CHILD_H

#include "stg05.h"

struct ArchAngelChildWork;
struct ArchAngelChildKind;
struct ShotRec;

void arch_angel_child_vars_register(void);
void arch_angel_child_vars_unregister(void);
int arch_angel_child_get_init_arg(void);
int arch_angel_child_leader_get_init_arg(void);
int arch_angel_child_vars_init(int *prm);
int arch_angel_child_vars_release(void);
int arch_angel_child_leader_vars_init(int *prm);
int arch_angel_child_leader_vars_release(void);
int arch_angel_child_marker_draw(int obj, void *place);
void arch_angel_child_steer(void *obj, struct ArchAngelChildWork *w, int noise);
int arch_angel_child_node_cb(unsigned int *n, int idx, struct ArchAngelChildWork *w);
int arch_angel_child_billboard_cb(unsigned int *n, int idx, struct ArchAngelChildWork *w);
void arch_angel_child_pose_model(void *obj, TypeEnemyWork *w);
void arch_angel_child_init(void *obj, struct ArchAngelChildWork *w, int prm, struct ArchAngelChildKind *k);
void arch_angel_child_release(void *obj, struct ArchAngelChildWork *w);
void arch_angel_child_state_fly(void *obj, struct ArchAngelChildWork *w);
void arch_angel_child_update(void *obj, struct ArchAngelChildWork *w);
void arch_angel_child_die(void *obj, struct ArchAngelChildWork *w);
void arch_angel_child_exec_appear(void *obj, struct ArchAngelChildWork *w);
void arch_angel_child_exec_normal(void *obj, struct ArchAngelChildWork *w);
void arch_angel_child_exec_leave(void *obj, struct ArchAngelChildWork *w);
void arch_angel_child_explode(void *obj, struct ArchAngelChildWork *w);
void arch_angel_child_exec_destroyed(void *obj, struct ArchAngelChildWork *w);
void arch_angel_child_pre_exec(void);
void arch_angel_child_exec(void *obj, TypeEnemyWork *w);
void arch_angel_child_post_exec(void *obj, struct ArchAngelChildWork *w);
void arch_angel_child_disp(void *obj, TypeEnemyWork *w);
int arch_angel_child_create(void *obj, int *pos, int *vel, int n, int leader, int kills, int lands, float act, float ramp);
void arch_angel_child_destroy(void *obj);
int arch_angel_child_spawn(void *obj, struct ShotRec *rec);
void arch_angel_child_rotate_yxz(int *ang);

#endif /* REZ_STG05_ARCH_ANGEL_CHILD_H */
