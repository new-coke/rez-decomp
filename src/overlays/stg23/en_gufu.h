/* Trance Mission enemy "gufu". */

#ifndef REZ_STG23_EN_GUFU_H
#define REZ_STG23_EN_GUFU_H

#include "stg23.h"

void en_gufu_vars_register(void);
void en_gufu_vars_unregister(void);
int en_gufu_marker_draw(int obj, void *place);
int en_gufu_vars_init(int *id);
int en_gufu_vars_release(void);
void en_gufu_swing(void *obj, EnDomWork *w);
void en_gufu_state_drift(void *obj, EnDomWork *w);
void en_gufu_state_escape_anticipate(void *obj, EnDomWork *w);
void en_gufu_state_escape(void *obj, EnDomWork *w);
void en_gufu_exec_normal(void *obj, EnDomWork *w);
void en_gufu_exec_destroyed(void *obj, EnDomWork *w);
void en_gufu_exec(void *obj, EnDomWork *w);
void en_gufu_pre_exec(void);
void en_gufu_post_exec(void *obj, int *w);
void en_gufu_disp_body(void *obj, EnDomWork *w);
void en_gufu_disp(void *obj, EnDomWork *w);
void en_gufu_init(void *obj, EnDomWork *w);
void en_gufu_release(void);
int en_gufu_create(void *parent, void *place);
void en_gufu_rotate_yxz(int *ang);

#endif /* REZ_STG23_EN_GUFU_H */
