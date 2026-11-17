/* HUD console: stage colours, level, boss gauge. */

#ifndef REZ_CONSOLE_H
#define REZ_CONSOLE_H

#include "types_game.h"

int console_task_create(void *parent);
void console_set_stage_colors(int col_a, int col_b);
int console_get_pcolp(void);
int console_get_pcol(void);
int console_set_disp_level(int level);
int console_get_disp_level(void);
void console_cell_pos_right(int n, float *px, float *py);
void console_cell_pos_left(int n, float *px, float *py);
int console2_swap_boss_on(int on);
void console2_set_boss_width(float v);
void console2_set_boss_rate(float v);

#endif /* REZ_CONSOLE_H */
