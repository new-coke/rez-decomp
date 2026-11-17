/* Eden boss pieces. */

#ifndef REZ_B_EDEN_EDEN_PIECE_H
#define REZ_B_EDEN_EDEN_PIECE_H

#include "types_common.h"

struct EdenPieceWork;
#include "b_eden.h"

void eden_piece_vars_add(void);
void eden_piece_vars_remove(void);
void eden_piece_cell_break(struct EdenPieceWork *w, EdenPieceCell *mate);
void eden_piece_cells_shatter(struct EdenPieceWork *w);
void eden_piece_cells_debris(struct EdenPieceWork *w);
EdenPieceCell *eden_piece_row_get(struct EdenPieceWork *w, EdenPieceCell *cell);
void eden_piece_charge_init(struct EdenPieceWork *w, EdenPieceCell *cell);
void eden_piece_charge_step(struct EdenPieceWork *w, EdenPieceCell *row);
void eden_piece_cell_fire(struct EdenPieceWork *w, EdenPieceCell *cell);
void eden_piece_charge_start(struct EdenPieceWork *w);
void eden_piece_charge_reset(struct EdenPieceWork *w);
void eden_piece_arm_step(struct EdenPieceWork *w);
void eden_piece_obj_change(struct EdenPieceWork *w, int kind);
void eden_piece_snake_step(struct EdenPieceWork *w);
void eden_piece_exec(struct EdenPieceWork *w);
void eden_piece_cells_draw(struct EdenPieceWork *w);
void eden_piece_disp(struct EdenPieceWork *w);
void eden_piece_release(struct EdenPieceWork *w);
int eden_piece_create(void *w, int kind, int *tbl, int *arg, int k);
void eden_piece_shatter(struct EdenPieceWork *w);
int eden_piece_get_remain(struct EdenPieceWork *w);
void eden_piece_cells_hide(struct EdenPieceWork *w, int on);
void eden_piece_cells_shake(struct EdenPieceWork *w, int flag);
int eden_piece_cell_get_pose(struct EdenPieceWork *w, int row, int col, Vec3 *pos, Vec3 *dir, int *ang);
int eden_piece_cell_get_flags(EdenPieceCell *cell);
EdenPieceCell *eden_piece_cell_find(struct EdenPieceWork *w, int row, int col);
void eden_piece_cell_command(struct EdenPieceWork *w, int row, int col, int cmd);
void eden_piece_cells_command(struct EdenPieceWork *w, int mode);
void eden_piece_wall_cell_break(struct EdenPieceWork *w, EdenPieceCell *cell);
void eden_piece_set_hold(EdenPieceCell *cell, int on);
int eden_piece_get_damage(struct EdenPieceWork *w);
void eden_piece_clear_damage(struct EdenPieceWork *w);
void eden_piece_rotate_yxz(int *ang);

#endif /* REZ_B_EDEN_EDEN_PIECE_H */
