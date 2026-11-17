/* Trance Mission enemy "wara". */

#ifndef REZ_STG23_EN_WARA_H
#define REZ_STG23_EN_WARA_H

#include "stg23.h"

struct EnWaraWork;

void en_wara_vars_register(void);
void en_wara_vars_unregister(void);
int en_wara_marker_draw(int obj, void *place);
int en_wara_vars_init(int *id);
int en_wara_vars_release(void);
void en_wara_swing(void *obj, struct EnWaraWork *w);
void en_wara_life_scale(void *obj, struct EnWaraWork *w);
void en_wara_state_drift(void *obj, struct EnWaraWork *w);
void en_wara_exec_normal(void *obj, struct EnWaraWork *w);
void en_wara_exec_destroyed(void *obj, struct EnWaraWork *w);
void en_wara_exec(void *obj, struct EnWaraWork *w);
void en_wara_pre_exec(void);
void en_wara_post_exec(void *obj, int *w);
void en_wara_disp_body(void *obj, struct EnWaraWork *w);
void en_wara_disp(void *obj, struct EnWaraWork *w);
void en_wara_init(void *obj, struct EnWaraWork *w);
void en_wara_release(void);
int en_wara_create(void *parent, TypeRec *place);
void en_wara_rotate_yxz(int *ang);

#endif /* REZ_STG23_EN_WARA_H */
