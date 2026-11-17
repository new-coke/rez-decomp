/* Trance Mission enemy "uni". */

#ifndef REZ_STG23_EN_UNI_H
#define REZ_STG23_EN_UNI_H

#include "stg23.h"

struct EnUniWork;

void en_uni_vars_register(void);
void en_uni_vars_unregister(void);
int en_uni_marker_draw(int obj, void *place);
int en_uni_vars_init(int *id);
int en_uni_vars_release(void);
void en_uni_swing(void *obj, struct EnUniWork *w);
void en_uni_life_scale(void *obj, struct EnUniWork *w);
float en_uni_curve_at(float t);
void en_uni_poly_init(void *obj, struct EnUniWork *w);
void en_uni_buffer_free(void *obj, int *w);
void en_uni_state_move(void *obj, struct EnUniWork *w);
void en_uni_exec_normal(void *obj, struct EnUniWork *w);
void en_uni_exec_destroyed(void *obj, struct EnUniWork *w);
void en_uni_exec(void *obj, struct EnUniWork *w);
void en_uni_pre_exec(void);
void en_uni_post_exec(void *obj, int *w);
void en_uni_disp_body(void *obj, struct EnUniWork *w);
void en_uni_disp(void *obj, struct EnUniWork *w);
void en_uni_init(void *obj, struct EnUniWork *w);
void en_uni_release(void *obj, int *w);
int en_uni_create(void *parent, TypeRec *place);
void en_uni_rotate_yxz(int *ang);

#endif /* REZ_STG23_EN_UNI_H */
