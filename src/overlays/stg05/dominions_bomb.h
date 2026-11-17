/* Dominions bomb, Area 5. */

#ifndef REZ_STG05_DOMINIONS_BOMB_H
#define REZ_STG05_DOMINIONS_BOMB_H

#include "stg05.h"
#include "types_common.h"

void dominions_bomb_vars_register(void);
void dominions_bomb_vars_unregister(void);
int dominions_bomb_get_init_arg(void);
int dominions_bomb_vars_init(int *prm);
int dominions_bomb_vars_release(void);
void dominions_bomb_models_load(void);
void dominions_bomb_models_free(void);
void dominions_bomb_place_bomblets(void *obj, DominionsBombWork *w);
void dominions_bomb_init(void *obj, DominionsBombWork *w, int place, int *rec);
void dominions_bomb_release(void *obj, DominionsBombWork *w);
void dominions_bomb_state_fall(void *obj, DominionsBombWork *w);
void dominions_bomb_state_fire(void *obj, DominionsBombWork *w);
void dominions_bomb_state_drop(void *obj, DominionsBombWork *w);
void dominions_bomb_update(void *obj, DominionsBombWork *w);
void dominions_bomb_exec_appear(void *obj, DominionsBombWork *w);
void dominions_bomb_exec_normal(void *obj, DominionsBombWork *w);
void dominions_bomb_exec_leave(void *obj, DominionsBombWork *w);
void dominions_bomb_exec_destroyed(void *obj, DominionsBombWork *w);
void dominions_bomb_pre_exec(void *obj);
void dominions_bomb_exec(void *obj, DominionsBombWork *w);
void dominions_bomb_post_exec(void *obj, DominionsBombWork *w);
void dominions_bomb_disp(void *obj, DominionsBombWork *w);
int dominions_bomb_create(void *obj, Vec3 *pos, Vec3 *dir);
void dominions_bomb_rotate_yxz(int *ang);

#endif /* REZ_STG05_DOMINIONS_BOMB_H */
