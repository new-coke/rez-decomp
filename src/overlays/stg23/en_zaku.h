/* Trance Mission enemy "zaku". */

#ifndef REZ_STG23_EN_ZAKU_H
#define REZ_STG23_EN_ZAKU_H

#include "stg23.h"

struct EnZakuWork;

void en_zaku_vars_register(void);
void en_zaku_vars_unregister(void);
int en_zaku_marker_draw(int obj, void *place);
int en_zaku_vars_init(int *id);
int en_zaku_vars_release(void);
void en_zaku_swing(void *obj, struct EnZakuWork *w);
void en_zaku_life_scale(void *obj, struct EnZakuWork *w);
void en_zaku_state_drift(void *obj, struct EnZakuWork *w);
void en_zaku_exec_normal(void *obj, struct EnZakuWork *w);
void en_zaku_exec_destroyed(void *obj, struct EnZakuWork *w);
void en_zaku_exec(void *obj, struct EnZakuWork *w);
void en_zaku_pre_exec(void);
void en_zaku_post_exec(void *obj, int *w);
void en_zaku_disp_body(void *obj, struct EnZakuWork *w);
void en_zaku_disp(void *obj, struct EnZakuWork *w);
void en_zaku_init(void *obj, struct EnZakuWork *w);
void en_zaku_release(void);
int en_zaku_create(void *parent, TypeRec *place);
void en_zaku_rotate_yxz(int *ang);

#endif /* REZ_STG23_EN_ZAKU_H */
