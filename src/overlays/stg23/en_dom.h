/* Trance Mission enemy "dom". */

#ifndef REZ_STG23_EN_DOM_H
#define REZ_STG23_EN_DOM_H

#include "stg23.h"

void en_dom_vars_register(void);
void en_dom_vars_unregister(void);
int en_dom_marker_draw(int obj, void *place);
int en_dom_vars_init(void);
int en_dom_vars_release(void);
void en_dom_swing(void *obj, EnDomWork *w);
void en_dom_state_drift(void *obj, EnDomWork *w);
void en_dom_state_escape_anticipate(void *obj, EnDomWork *w);
void en_dom_state_escape(void *obj, EnDomWork *w);
void en_dom_exec_normal(void *obj, EnDomWork *w);
void en_dom_exec_destroyed(void *obj, EnDomWork *w);
void en_dom_exec(void *obj, EnDomWork *w);
void en_dom_pre_exec(void);
void en_dom_post_exec(void *obj, int *w);
void en_dom_disp_body(void *obj, EnDomWork *w);
void en_dom_disp(void *obj, EnDomWork *w);
void en_dom_init(void *obj, EnDomWork *w);
void en_dom_release(void);
int en_dom_create(void *parent, void *place);
void en_dom_rotate_yxz(int *ang);

#endif /* REZ_STG23_EN_DOM_H */
