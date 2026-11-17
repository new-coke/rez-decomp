/* Brahma, Area 2 enemy. */

#ifndef REZ_STG02_EN_BRAHMA_H
#define REZ_STG02_EN_BRAHMA_H

struct EnBrahmaWork;
struct EnBrahmaPlace;

void en_brahma_vars_register(void);
void en_brahma_vars_unregister(void);
int en_brahma_marker_draw(int obj, void *w);
int en_brahma_vars_init(void);
int en_brahma_vars_release(void);
void en_brahma_aim_head(void *obj, struct EnBrahmaWork *w);
void en_brahma_hit_check(void *obj, int *wp);
void en_brahma_follow_rail(void *obj, struct EnBrahmaWork *w);
void en_brahma_start_move(void *obj, struct EnBrahmaWork *w);
void en_brahma_state_move_in(void *obj, struct EnBrahmaWork *w);
void en_brahma_state_wait(void *obj, struct EnBrahmaWork *w);
void en_brahma_state_attack_begin(void *obj, struct EnBrahmaWork *w);
void en_brahma_state_fire(void *obj, struct EnBrahmaWork *w);
void en_brahma_state_attack_end(void *obj, struct EnBrahmaWork *w);
void en_brahma_state_escape(void *obj, struct EnBrahmaWork *w);
void en_brahma_exec_normal(void *obj, struct EnBrahmaWork *w);
void en_brahma_exec_appear(void *obj, struct EnBrahmaWork *w);
void en_brahma_exec_destroyed(void *obj, void *w);
void en_brahma_pre_exec(void *obj, int *w);
void en_brahma_exec(void *obj, void *w);
void en_brahma_post_exec(void *obj, struct EnBrahmaWork *w);
void en_brahma_toggle_guns(void *obj, int *w);
void en_brahma_disp_body(void *obj, struct EnBrahmaWork *w);
void en_brahma_disp(void *obj, void *w);
void en_brahma_init(void *obj, struct EnBrahmaWork *w, struct EnBrahmaPlace *prm);
void en_brahma_release(void *obj, struct EnBrahmaWork *w);
int en_brahma_create(void *parent, struct EnBrahmaPlace *place);
void en_brahma_models_init(void);
void en_brahma_rotate_yxz(int *ang);

#endif /* REZ_STG02_EN_BRAHMA_H */
