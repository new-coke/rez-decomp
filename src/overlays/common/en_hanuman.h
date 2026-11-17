/* Hanuman (stg02 enemy). */

#ifndef REZ_COMMON_EN_HANUMAN_H
#define REZ_COMMON_EN_HANUMAN_H

#include "types_overlays.h"
#include "types_common.h"

struct EnHanumanArg;

void en_hanuman_vars_register(void);
void en_hanuman_vars_unregister(void);
int en_hanuman_vars_init(void);
int en_hanuman_vars_release(void);
void en_hanuman_update_target(void *obj, EnHanumanWork *w);
void en_hanuman_spawn_shield(void *obj, EnHanumanWork *w);
void en_hanuman_get_spawn_points(void *obj, void *work);
void en_hanuman_state_move(void *obj, EnHanumanWork *w);
float en_hanuman_ease_envelope(float t);
void en_hanuman_state_stop(void *obj, EnHanumanWork *w);
void en_hanuman_state_motion(void *obj, EnHanumanWork *w);
void en_hanuman_state_drift(void *obj, EnHanumanWork *w);
void en_hanuman_state_anticipate(void *obj, EnHanumanWork *w);
void en_hanuman_state_escape(void *obj, EnHanumanWork *w);
void en_hanuman_exec_normal(void *obj, EnHanumanWork *w);
void en_hanuman_exec_appear(void *obj, void *w);
void en_hanuman_exec_leave(void *obj, void *w);
void en_hanuman_exec_destroyed(void *obj, EnHanumanWork *w);
void en_hanuman_exec(void *obj, void *w);
void en_hanuman_hit_check(void *obj, void *work);
void en_hanuman_pre_exec(void *obj, void *w);
void en_hanuman_post_exec(void *obj, void *w);
void en_hanuman_disp_body(void *obj, EnHanumanWork *w);
void en_hanuman_disp(void *obj, void *w);
void en_hanuman_rotate_by_player(Vec3 *v);
void en_hanuman_init(void *obj, EnHanumanWork *w, struct EnHanumanArg *prm);
void en_hanuman_release(void *obj, char *w);
void en_hanuman_create(void *parent, EnHanumanArg *prm);
void en_hanuman_models_init(void);
void en_hanuman_rotate_yxz(int *ang);

#endif /* REZ_COMMON_EN_HANUMAN_H */
