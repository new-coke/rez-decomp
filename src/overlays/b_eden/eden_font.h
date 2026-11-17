/* Eden boss piece text. */

#ifndef REZ_B_EDEN_EDEN_FONT_H
#define REZ_B_EDEN_EDEN_FONT_H

#include "types_common.h"
#include "b_eden.h"

void eden_font_vars_add(void);
void eden_font_vars_remove(void);
int eden_font_glyph_vertex(void *owner, Vec3 *out, int i, char ch);
int eden_font_count_chars(char *str);
float eden_font_line_offset(char *str);
void eden_font_text_build(void *work, char *str);
EdenOrb *eden_font_next_rec(int *w);
int eden_font_glyph_step(void *w, EdenOrb *item, Vec3 *pos, Vec3 *move);
void eden_font_exec(void *work);
void eden_font_debug_box_draw(void);
void eden_font_disp(void *work);
void eden_font_grps_free(EdenText *work);
void eden_font_release(EdenText *w);
EdenText *eden_font_create(void *parent, int arg, int k);
int eden_font_is_done(EdenText *w);
int eden_font_count_live(EdenText *w);
void eden_font_piece_point_random(void *w, void *slot, Vec3 *pos, int *ang);
void eden_font_get_item_pose(EdenText *w, int idx, Vec3 *pos, int *ang);
void eden_font_end(EdenText *w);
void eden_font_shine_create(void *w, int arg);
void eden_font_shock_wave(EdenText *w, Vec3 *pos);
void eden_font_rotate_yxz(int *ang);

#endif /* REZ_B_EDEN_EDEN_FONT_H */
