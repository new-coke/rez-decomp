/* Marimo break pieces, Area 4. */

#ifndef REZ_STG04_EN_MARIMO_BREAK_H
#define REZ_STG04_EN_MARIMO_BREAK_H

#include "stg04.h"

void en_marimo_break_shatter(EnMarimoBreakWork *w);
void en_marimo_break_calc_target(EnMarimoBreakWork *w);
int en_marimo_break_hit_check(EnMarimoBreakWork *w);
void en_marimo_break_exec(EnMarimoBreakWork *w);
void en_marimo_break_toggle_nodes(int *w);
void en_marimo_break_disp(EnMarimoBreakWork *w);
void en_marimo_break_init(EnMarimoBreakWork *w);
void en_marimo_break_release(char *w);
EnMarimoBreakWork *en_marimo_break_create(void *parent, EnMarimoBreakArg *prm);
void en_marimo_break_rotate_yxz(int *ang);

#endif /* REZ_STG04_EN_MARIMO_BREAK_H */
