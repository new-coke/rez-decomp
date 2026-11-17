/* Area 5 stage support. */

#ifndef REZ_STG05_STAGE_BASE_H
#define REZ_STG05_STAGE_BASE_H

struct GateWork;
struct GateArg;

void listeners_add(void);
void listeners_remove(void);
void listener_stars_on(void);
void listener_next_map(void);
void listener_mute(int *mute);
void listener_lock_hit(void);
void listener_area_change(int *ev);
void filter_vars_add(void);
void filter_vars_remove(void);
void filter_reset(void);
void filter_set_area(unsigned int area);
void filter_fade_last(void);
void gate_shatter(struct GateWork *w);
void gate_bar_shatter(struct GateWork *w, int i);
int gate_node_cb(unsigned int *n, int idx, struct GateWork *w);
void gate_bars_open(struct GateWork *w);
void gate_bars_move(struct GateWork *w);
void gate_locks_init(struct GateWork *w);
void gate_locks_remove(struct GateWork *w);
void gate_locks_enable(struct GateWork *w);
int gate_hit_check(struct GateWork *w);
void gate_move(struct GateWork *w);
void gate_models_init(struct GateWork *w);
void gate_models_open(struct GateWork *w);
void gate_init(struct GateWork *w, struct GateArg *prm);
void gate_release(struct GateWork *w);
void gate_pose(struct GateWork *w);
void gate_exec(struct GateWork *w);
void gate_disp(struct GateWork *w);
void *gate_create(void *parent, void *prm);
int gate_marker_draw(void *w, struct GateArg *prm);
int gate_vars_register(void);
int gate_vars_unregister(void);
void gate_rotate_yxz(int *ang);

#endif /* REZ_STG05_STAGE_BASE_H */
