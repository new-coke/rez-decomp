/* Garuda, Area 2 enemy. */

#ifndef REZ_STG02_EN_GARUDA_H
#define REZ_STG02_EN_GARUDA_H

#include "types_common.h"
#include "stg02.h"

struct EnGarudaWork;

void en_garuda_vars_register(void);
void en_garuda_vars_unregister(void);
int en_garuda_marker_draw(int obj, void *place);
int en_garuda_vars_init(int *id);
int en_garuda_vars_release(void);
void en_garuda_fire_missile(void *obj, int w, Vec3 *dir);
void en_garuda_sample_path(float t, void *obj, struct EnGarudaWork *w, Vec3 *out, int *ang);
void en_garuda_state_attack(void *obj, struct EnGarudaWork *w);
void en_garuda_state_escape(void *obj, struct EnGarudaWork *w);
void en_garuda_exec_normal(void *obj, struct EnGarudaWork *w);
void en_garuda_exec_destroyed(void *obj, struct EnGarudaWork *w);
void en_garuda_exec(void *obj, void *w);
void en_garuda_pre_exec(void);
void en_garuda_post_exec(void *obj, float *w);
void en_garuda_toggle_missile(void *obj, int *w);
void en_garuda_disp_glow(void *obj, struct EnGarudaWork *w);
void en_garuda_disp_body(void *obj, struct EnGarudaWork *w);
void en_garuda_disp(void *obj, struct EnGarudaWork *w);
void en_garuda_init(void *obj, struct EnGarudaWork *w, void *prm);
void en_garuda_release(void *obj, char *w);
int en_garuda_create(void *parent, EnGarudaArg *place);
void en_garuda_nodes_init(void);
void en_garuda_se_play(void);
void en_garuda_rotate_yxz(int *ang);

#endif /* REZ_STG02_EN_GARUDA_H */
