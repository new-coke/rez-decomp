/* Eden boss break-up effects. */

#ifndef REZ_B_EDEN_EDEN_BREAK_H
#define REZ_B_EDEN_EDEN_BREAK_H

#include "types_common.h"
#include "types_overlays.h"

struct EdenDrop;
struct EdenBreakWork;

void eden_break_vars_add(void);
void eden_break_vars_remove(void);
int eden_break_count_alive(WireObj *w);
struct EdenDrop *eden_break_fall_start(struct EdenDrop *drop, WireObj *src, int lvl);
void eden_break_fall_step(struct EdenBreakWork *w);
void eden_break_exec(struct EdenBreakWork *w);
void eden_break_pieces_draw(struct EdenBreakWork *w);
void eden_break_disp(struct EdenBreakWork *w);
void eden_break_release(struct EdenBreakWork *w);
void *eden_break_d_create(void *parent, int kind, int lvl);
void *eden_break_d_fall_start(struct EdenDrop *drop, void *node);
void eden_break_d_exec(struct EdenBreakWork *w);
void eden_break_d_disp(struct EdenBreakWork *w);
int *eden_break_create(void *parent, int *src);
void eden_break_rotate_yxz(int *ang);

#endif /* REZ_B_EDEN_EDEN_BREAK_H */
