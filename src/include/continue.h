#ifndef REZ_CONTINUE_H
#define REZ_CONTINUE_H

struct CharWork;
struct CharParam;
struct NumWork;
struct NumParam;
struct ContWork;

void char_task_init(struct CharWork *w, struct CharParam *prm);
void char_task_release(struct CharWork *w);
void char_task_exec(struct CharWork *w);
void char_task_disp(struct CharWork *w);
void *char_task_create(void *parent, int prm);
void char_set_pos(struct CharWork *w, float x, float y);
void char_set_rot(struct CharWork *w, int x, int y, int z);
void char_set_col(struct CharWork *w, int col);
void char_set_scl(struct CharWork *w, float v);
void num_task_init(struct NumWork *w, struct NumParam *prm);
void num_task_release(void);
void num_task_exec(struct NumWork *w);
void num_task_disp(void);
void *num_task_create(void *parent, int prm);
void num_layout(struct NumWork *w);
void num_set_state_min(struct NumWork *w, unsigned state);
void continue_count_exec(struct ContWork *w);
void continue_task_init(int *w);
void continue_task_release(void **p);
void continue_task_exec_end(int *w);
void continue_task_exec(int *w);
void continue_task_disp(int *w);
void *continue_task_create(void);
void continue_rotate_yxz(int *p);

#endif /* REZ_CONTINUE_H */
