/* Balloon, Area 4 enemy. */

#ifndef REZ_STG04_EN_BALLOON_H
#define REZ_STG04_EN_BALLOON_H

struct EnBalloonWork;
struct EnBalloonReleaseWork;

typedef struct EnBalloonReleaseWork EnBalloonReleaseWork;
typedef struct EnBalloonWork EnBalloonWork;

void en_balloon_vars_register(void);
void en_balloon_vars_unregister(void);
int en_balloon_marker_draw(int obj, void *w);
int en_balloon_vars_init(int *prm);
int en_balloon_vars_release(void);
void en_balloon_aim_core(void *obj, EnBalloonWork *w);
void en_balloon_calc_lock_point(void *obj, EnBalloonWork *w);
void en_balloon_calc_points(void *obj, EnBalloonWork *w);
void en_balloon_swing(void *obj, EnBalloonWork *w);
void en_balloon_hit_check(void *obj, EnBalloonWork *w);
void en_balloon_state_move_in(void *obj, EnBalloonWork *w);
void en_balloon_state_wait(void *obj, EnBalloonWork *w);
void en_balloon_spawn_debris_all(void *obj, EnBalloonWork *w, int piece);
void en_balloon_state_attack(void *obj, EnBalloonWork *w);
void en_balloon_state_fire(void *obj, EnBalloonWork *w);
void en_balloon_state_escape(void *obj, EnBalloonWork *w);
void en_balloon_place_ctrlbars(void *obj, EnBalloonWork *w);
void en_balloon_exec_normal(void *obj, EnBalloonWork *w);
void en_balloon_exec_destroyed(void *obj, EnBalloonWork *w);
void en_balloon_exec(void *obj, void *w);
void en_balloon_pre_exec(void *obj, EnBalloonWork *w);
void en_balloon_post_exec(void *obj, EnBalloonWork *w);
void en_balloon_wire_update(void *obj, EnBalloonWork *w, int first, int n);
void en_balloon_wire_draw_tree(void *obj, EnBalloonWork *w, unsigned int *node, int *chain);
void en_balloon_wire_draw(void *obj, EnBalloonWork *w);
void en_balloon_draw_rect(void *obj, EnBalloonWork *w, float z);
void en_balloon_draw_release(void *obj, EnBalloonWork *w);
void en_balloon_toggle_parts(void *obj, EnBalloonWork *w);
void en_balloon_disp_fade_in(void *obj, EnBalloonWork *w);
void en_balloon_disp_body(void *obj, EnBalloonWork *w);
void en_balloon_disp(void *obj, EnBalloonWork *w);
void en_balloon_init(void *obj, EnBalloonWork *w);
void en_balloon_release(void *obj, EnBalloonReleaseWork *w);
int en_balloon_create(void *parent, void *rec);
void en_balloon_models_init(void);
void en_balloon_rotate_yxz(int *ang);

#endif /* REZ_STG04_EN_BALLOON_H */
