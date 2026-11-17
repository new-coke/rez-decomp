/* Thrones' cores. */

#ifndef REZ_COMMON_THRONES_CORE_H
#define REZ_COMMON_THRONES_CORE_H

#include "types_overlays.h"

void thrones_core_vars_register(void);
void thrones_core_vars_unregister(void);
int thrones_core_get_init_arg(void);
int thrones_core_vars_init(int *prm);
int thrones_core_vars_release(void);
void thrones_core_init(void *obj, ThronesCoreWork *w, int prm, ThronesCoreKind *k);
void thrones_core_release(void);
int thrones_core_next_state(void *obj, ThronesCoreWork *w, int n, int state);
void thrones_core_state_launch(void *obj, ThronesCoreWork *w);
void thrones_core_state_back(void *obj, ThronesCoreWork *w);
void thrones_core_state_accel(void *obj, ThronesCoreWork *w);
void thrones_core_state_finish(void *obj, ThronesCoreWork *w);
void thrones_core_state_away(void *obj, ThronesCoreWork *w);
void thrones_core_update(void *obj, ThronesCoreWork *w);
void thrones_core_exec_appear(void *obj, ThronesCoreWork *w);
void thrones_core_exec_normal(void *obj, ThronesCoreWork *w);
void thrones_core_exec_leave(void *obj, ThronesCoreWork *w);
void thrones_core_pre_exec(void *obj, ThronesCoreWork *w);
void thrones_core_exec(void *obj, ThronesCoreWork *w);
void thrones_core_post_exec(void *obj, ThronesCoreWork *w);
void thrones_core_disp_body(void *obj, ThronesCoreWork *w);
void thrones_core_disp(void *obj, ThronesCoreWork *w);
int thrones_core_create(void *owner, ThronesCoreArg *core_prm);
void *thrones_core_get_lock(void *obj);
void thrones_core_set_siblings(void *obj, int core0, int core1, int core2, int core3);
void thrones_core_set_counters(void *obj, int *killed, int *escaped);
void thrones_core_rotate_yxz(int *ang);
int thrones_core_manager_get_init_arg(void);
int thrones_core_manager_vars_init(int *prm);
int thrones_core_manager_vars_release(void);
void thrones_core_manager_init(ThronesCoreManagerWork *w, ThronesCoreManagerArg *prm);
void thrones_core_manager_release(ThronesCoreManagerWork *w);
void thrones_core_manager_fire(ThronesCoreManagerWork *w, int op);
void thrones_core_manager_exec(ThronesCoreManagerWork *w);
void *thrones_core_manager_create(void *parent, void *core0, void *core1, void *core2, void *core3, int fx);

#endif /* REZ_COMMON_THRONES_CORE_H */
