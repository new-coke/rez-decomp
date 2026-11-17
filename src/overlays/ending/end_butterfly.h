/* Ending: butterfly scene. */

#ifndef REZ_ENDING_END_BUTTERFLY_H
#define REZ_ENDING_END_BUTTERFLY_H

struct EndButterflyChild;
struct EndButterfly;

void end_butterfly_vars_register(void);
void end_butterfly_vars_unregister(void);
void end_butterfly_vars_init(void);
void end_butterfly_vars_release(void);
void end_butterfly_child_position(struct EndButterflyChild *child);
void end_butterfly_children_init(struct EndButterfly *w);
void end_butterfly_children_update(struct EndButterfly *w);
void end_butterfly_path_eval(struct EndButterfly *w);
void end_butterfly_init(struct EndButterfly *w, float *start_t);
void end_butterfly_release(struct EndButterfly *w);
void end_butterfly_exec_end(struct EndButterfly *w);
void end_butterfly_exec_wait(struct EndButterfly *w);
void end_butterfly_exec_path(struct EndButterfly *w);
void end_butterfly_exec(struct EndButterfly *w);
void end_butterfly_disp(struct EndButterfly *w);
void *end_butterfly_create(void *parent, float t);
void end_butterfly_rotate_yxz(int *ang);

#endif /* REZ_ENDING_END_BUTTERFLY_H */
