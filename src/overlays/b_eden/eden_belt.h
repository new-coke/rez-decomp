/* Eden boss belts. */

#ifndef REZ_B_EDEN_EDEN_BELT_H
#define REZ_B_EDEN_EDEN_BELT_H

struct EdenBeltWork;
struct EdenBeltManWork;

void eden_belt_vars_add(void);
void eden_belt_vars_remove(void);
void eden_belt_swing_update(struct EdenBeltWork *w, float dt);
void eden_belt_rod_ends(struct EdenBeltWork *w);
void eden_belt_shot_step(struct EdenBeltWork *w);
void eden_belt_set_level(struct EdenBeltWork *w, int i);
void eden_belt_exec(struct EdenBeltWork *w);
void eden_belt_debug_draw(struct EdenBeltWork *belt);
void eden_belt_disp(struct EdenBeltWork *w);
void eden_belt_release(struct EdenBeltWork *w);
struct EdenBeltWork *eden_belt_create(void *parent, int arg, int i);
void eden_belt_set_hits(struct EdenBeltWork *w, int i, int hits);
int eden_belt_get_hits(struct EdenBeltWork *w, int i);
void eden_belt_set_spd(struct EdenBeltWork *w, float spd);
void eden_belt_set_period(struct EdenBeltWork *w, float t);
void eden_belt_part_break(struct EdenBeltWork *w, int i);
void eden_belt_man_exec(struct EdenBeltManWork *w);
void eden_belt_man_release(struct EdenBeltManWork *w);
void eden_belt_man_disp(struct EdenBeltManWork *w);
int *eden_belt_man_create(void *parent, int arg);
void eden_belt_man_hits_apply(struct EdenBeltManWork *w, int i, int j);
int eden_belt_man_get_mode(struct EdenBeltManWork *w);
void eden_belt_man_set_mode(struct EdenBeltManWork *w, int mode);
void eden_belt_man_set_wait(struct EdenBeltManWork *w, int wait);
void eden_belt_rotate_yxz(int *ang);

#endif /* REZ_B_EDEN_EDEN_BELT_H */
