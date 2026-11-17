/* Small Shiva, Area 2 enemy. */

#ifndef REZ_STG02_EN_SHIVA_MINI_H
#define REZ_STG02_EN_SHIVA_MINI_H

struct EnShivaMiniWork;

void en_shiva_mini_vars_register(void);
void en_shiva_mini_vars_unregister(void);
void en_shiva_mini_slide(void *obj, struct EnShivaMiniWork *w);
void en_shiva_mini_calc_core(void *obj, int *work);
void en_shiva_mini_calc_points(void *obj, int *w);
void en_shiva_mini_hit_check(void *obj, void *work);
void en_shiva_mini_state_move_in(void *obj, struct EnShivaMiniWork *w);
void en_shiva_mini_state_escape(void *obj, struct EnShivaMiniWork *w);
void en_shiva_mini_state_wait(void *obj, struct EnShivaMiniWork *w);
void en_shiva_mini_exec_normal(void *obj, struct EnShivaMiniWork *w);
void en_shiva_mini_exec_destroyed(void *obj, struct EnShivaMiniWork *w);
void en_shiva_mini_exec(void *obj, void *w);
void en_shiva_mini_pre_exec(void *obj, void *w);
void en_shiva_mini_post_exec(void *obj, int *w);
void en_shiva_mini_disp_glow(void *obj, struct EnShivaMiniWork *w);
void en_shiva_mini_disp_body(void *obj, struct EnShivaMiniWork *w);
void en_shiva_mini_disp(void *obj, void *w);
void en_shiva_mini_init(void *obj, struct EnShivaMiniWork *w);
void en_shiva_mini_release(void *obj, char *w);
int en_shiva_mini_create(void *parent, void *place);
void en_shiva_mini_nodes_init(void);
void en_shiva_mini_rotate_yxz(int *ang);

#endif /* REZ_STG02_EN_SHIVA_MINI_H */
