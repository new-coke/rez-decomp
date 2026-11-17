/* Marimo, Area 4 enemy. */

#ifndef REZ_STG04_EN_MARIMO_H
#define REZ_STG04_EN_MARIMO_H

#include "types_common.h"

struct EnMarimoWork;
struct EnMarimoCbArg;

void en_marimo_vars_register(void);
void en_marimo_vars_unregister(void);
int en_marimo_marker_draw(int obj, void *mark);
int en_marimo_vars_init(int *id);
int en_marimo_vars_release(void);
void en_marimo_calc_lock_points(void *obj, struct EnMarimoWork *w);
void en_marimo_calc_points(void *obj, struct EnMarimoWork *w);
void en_marimo_get_shot_points(void *obj, struct EnMarimoWork *w, Vec3 *out);
void en_marimo_part_shatter(void *obj, struct EnMarimoWork *w, int grp, int piece);
void en_marimo_shell_shatter(void *obj, struct EnMarimoWork *w, int shell);
int en_marimo_spawn_break_cb(void *part, int node, struct EnMarimoCbArg *cb);
void en_marimo_spawn_break(void *obj, struct EnMarimoWork *w, int kind);
void en_marimo_hit_check_base(void *obj, struct EnMarimoWork *w);
void en_marimo_hit_check_container(void *obj, struct EnMarimoWork *w, int cont);
void en_marimo_hit_check(void *obj, struct EnMarimoWork *w);
void en_marimo_swing(void *obj, struct EnMarimoWork *w);
void en_marimo_palette_start(void *obj, struct EnMarimoWork *w);
void en_marimo_palette_update(void *obj, struct EnMarimoWork *w);
void en_marimo_spawn_hoppers(void *obj, struct EnMarimoWork *w, int fire);
void en_marimo_state_move_in(void *obj, struct EnMarimoWork *w);
void en_marimo_state_ready(void *obj, struct EnMarimoWork *w);
void en_marimo_state_open(void *obj, struct EnMarimoWork *w);
void en_marimo_state_shoot(void *obj, struct EnMarimoWork *w);
void en_marimo_state_close(void *obj, struct EnMarimoWork *w);
void en_marimo_state_escape(void *obj, struct EnMarimoWork *w);
void en_marimo_exec_normal(void *obj, struct EnMarimoWork *w);
void en_marimo_exec_destroyed(void *obj, struct EnMarimoWork *w);
void en_marimo_exec(void *obj, void *w);
void en_marimo_pre_exec(void *obj, struct EnMarimoWork *w);
void en_marimo_post_exec(void *obj, struct EnMarimoWork *w);
void en_marimo_toggle_model_nodes(void *obj, struct EnMarimoWork *w);
void en_marimo_toggle_body_parts(void *obj, struct EnMarimoWork *w);
void en_marimo_toggle_part_a(void *obj, struct EnMarimoWork *w);
void en_marimo_disp_body(void *obj, struct EnMarimoWork *w);
void en_marimo_disp(void *obj, struct EnMarimoWork *w);
void en_marimo_init(void *obj, struct EnMarimoWork *w);
void en_marimo_release(void *obj, struct EnMarimoWork *w);
int en_marimo_create(void *parent, void *place);
void en_marimo_nodes_init(void);
void en_marimo_rotate_yxz(int *ang);

#endif /* REZ_STG04_EN_MARIMO_H */
