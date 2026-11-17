/* Shiva, Area 2 enemy. */

#ifndef REZ_STG02_EN_SHIVA_H
#define REZ_STG02_EN_SHIVA_H

struct EnShivaWork;
struct EnShivaPlace;

void en_shiva_vars_register(void);
void en_shiva_vars_unregister(void);
int en_shiva_marker_draw(int obj, void *w);
int en_shiva_vars_init(int *prm);
int en_shiva_vars_release(void);
void en_shiva_slide(void *obj, struct EnShivaWork *w);
void en_shiva_calc_points(void *obj, char *work);
void en_shiva_part_shatter(void *obj, struct EnShivaWork *w, int i);
void en_shiva_part_debris(void *obj, struct EnShivaWork *w, int i);
void en_shiva_fire_missile(void *obj, struct EnShivaWork *w, int i);
void en_shiva_hit_check(void *obj, void *work);
void en_shiva_state_move_in(void *obj, struct EnShivaWork *w);
void en_shiva_state_escape(void *obj, struct EnShivaWork *w);
void en_shiva_state_wait_open(void *obj, struct EnShivaWork *w);
void en_shiva_state_wait_escape(void *obj, struct EnShivaWork *w);
void en_shiva_state_open(void *obj, struct EnShivaWork *w);
void en_shiva_state_close(void *obj, struct EnShivaWork *w);
void en_shiva_state_fire(void *obj, struct EnShivaWork *w);
void en_shiva_state_wait_fire(void *obj, struct EnShivaWork *w);
void en_shiva_exec_normal(void *obj, struct EnShivaWork *w);
void en_shiva_exec_destroyed(void *obj, struct EnShivaWork *w);
void en_shiva_exec(void *obj, void *w);
void en_shiva_pre_exec(void *obj, void *w);
void en_shiva_post_exec(void *obj, char *w);
void en_shiva_toggle_parts(void *obj, struct EnShivaWork *w);
void en_shiva_toggle_glow(void *obj, struct EnShivaWork *w);
void en_shiva_disp_glow(void *obj, struct EnShivaWork *w);
void en_shiva_disp_body(void *obj, struct EnShivaWork *w);
void en_shiva_disp(void *obj, void *w);
void en_shiva_init(void *obj, struct EnShivaWork *w);
void en_shiva_release(void *obj, struct EnShivaWork *w);
int en_shiva_create(void *parent, struct EnShivaPlace *place);
void en_shiva_nodes_init(void);
void en_shiva_rotate_yxz(int *ang);

#endif /* REZ_STG02_EN_SHIVA_H */
