/* Lesser Thrones. */

#ifndef REZ_COMMON_THRONES_CHILD_H
#define REZ_COMMON_THRONES_CHILD_H

#include "types_overlays.h"

struct ThronesChildCursor;

void thrones_child_vars_register(void);
void thrones_child_vars_unregister(void);
int thrones_child_get_init_arg(void);
int thrones_child_vars_init(int *prm);
int thrones_child_vars_release(void);
int thrones_child_marker_draw(int obj, void *place);
int thrones_child_node_cb(unsigned int *n, int idx, struct ThronesChildCursor *cur);
void thrones_child_pose_model(void *obj, ThronesChildWork *w);
void thrones_child_follow_path_circle(void *obj, ThronesChildWork *w);
int thrones_child_root_node_cb(unsigned int *n, int idx, ThronesChildWork *w);
void thrones_child_follow_path(void *obj, ThronesChildWork *w);
void thrones_child_init(void *obj, ThronesChildWork *w, ThronesChildArg *prm, void *kind);
void thrones_child_release(void *obj, ThronesChildWork *w);
int thrones_child_next_state(void *obj, ThronesChildWork *w, int n, int state);
void thrones_child_state_move(void *obj, ThronesChildWork *w);
void thrones_child_state_animate(void *obj, ThronesChildWork *w);
void thrones_child_update(void *obj, ThronesChildWork *w);
void thrones_child_state_die(void *obj, ThronesChildWork *w);
void thrones_child_exec_appear(void *obj, ThronesChildWork *w);
void thrones_child_exec_normal(void *obj, ThronesChildWork *w);
void thrones_child_exec_leave(void *obj, ThronesChildWork *w);
void thrones_child_exec_destroyed(void *obj, ThronesChildWork *w);
void thrones_child_pre_exec(void);
void thrones_child_exec(void *obj, ThronesChildWork *w);
void thrones_child_post_exec(void *obj, ThronesChildWork *w);
void thrones_child_disp_body(void *obj, ThronesChildWork *w);
void thrones_child_disp_die(void *obj, ThronesChildWork *w);
void thrones_child_disp(void *obj, ThronesChildWork *w);
int thrones_child_create(void *parent, ThronesChildArg *prm);
void thrones_child_rotate_yxz(int *ang);

#endif /* REZ_COMMON_THRONES_CHILD_H */
