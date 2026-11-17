/* Debug GUI window tree. */

#ifndef REZ_GW_WINDOW_H
#define REZ_GW_WINDOW_H

#include "types_gw.h"

struct GwWin;

int gw_get_default_color(void);
void gw_set_default_colors(int *src, int draw_color);
void gw_window_size_to_window(struct GwWin *win, struct GwWin *ref, int dx, int dy);
void gw_window_size_to_parent(struct GwWin *win, int dx, int dy);
void gw_window_get_cell_size(struct GwWin *win, int *cw, int *ch);
int *gw_get_default_colors(void);
int gw_window_is_visible(struct GwWin *win);
void gw_window_show(struct GwWin *win);
void gw_window_hide(struct GwWin *win);
int gw_window_is_slider(struct GwWin *win);
int gw_window_is_scrollbar(struct GwWin *win);
int gw_window_is_frame(struct GwWin *win);
int gw_window_hit_local(struct GwWin *win, int x, int y, int *rx, int *ry);
int gw_window_hit_mouse(struct GwWin *win, int *lx, int *ly);
float gw_window_get_scale(struct GwWin *win);
int gw_window_replace_flags(struct GwWin *win, int flags);
int gw_window_get_flags(struct GwWin *win);
void gw_window_set_user(struct GwWin *win, int user);
int gw_window_get_user(struct GwWin *win);
void gw_window_set_slot(struct GwWin *win, int i, int val);
int gw_window_get_slot(struct GwWin *win, int i);
int gw_get_border_width(void);
int gw_get_title_bar_height(void);
struct GwWin *gw_window_get_parent(struct GwWin *win);
int gw_window_hit_area(struct GwWin *win, int px, int py);
char *gw_window_get_title(struct GwWin *win);
void gw_window_set_title(struct GwWin *win, char *title);
void gw_window_get_pos(struct GwWin *win, int *x, int *y);
void gw_window_set_pos(struct GwWin *win, int x, int y);
void gw_window_get_size(struct GwWin *win, int *width, int *height);
void gw_window_set_size(struct GwWin *win, int width, int height);
void gw_window_find_at(struct GwWin *win, int x, int y, struct GwWin **out);
void gw_window_set_handlers(struct GwWin *win, GwFn update, GwFn draw, GwFn close);
void gw_window_set_close_handler(struct GwWin *win, GwFn fn);
void gw_window_set_draw_handler(struct GwWin *win, GwFn fn);
int gw_window_is_grabbed(int win);
int gw_window_has_active(struct GwWin *win);
int gw_window_is_active(int win);
int gw_window_activate(struct GwWin *win);
void gw_window_set_owner(struct GwWin *win, struct GwWin *owner);
int gw_window_inside_owner(struct GwWin *win);
int gw_rect_contains_rect(int *inner, int *outer);
int gw_rect_contains_point(int *rect, int x, int y);
void gw_window_get_abs_rect(struct GwWin *win, int *rect);
void gw_window_get_abs_pos(struct GwWin *win, int *x, int *y);
void gw_window_set_color(struct GwWin *win, unsigned col);
void gw_window_set_colors(struct GwWin *win, unsigned *cols);
void gw_window_count(struct GwWin *win, int *n);
struct GwWin *gw_window_close(struct GwWin *win);
struct GwWin *gw_window_new(int kind, char *name, int flags, int x, int y, int width, int height, struct GwWin *parent);
float gw_float_bits_add(float x, int n);
void gw_window_update_all(struct GwWin *win);
void gw_window_scale_pass(struct GwWin *win, float *scale);
struct GwWin *gw_window_get_maximize_box(struct GwWin *win);
struct GwWin *gw_window_get_minimize_box(struct GwWin *win);
struct GwWin *gw_window_get_vscrollbar(struct GwWin *win);
void gw_window_set_vscrollbar(struct GwWin *win, struct GwWin *sb);
void gw_window_add_scrollbar(struct GwWin *win, struct GwWin *sb);
struct GwWin *gw_window_open(char *title, int flags, int x, int y, int width, int height, int parent);

#endif /* REZ_GW_WINDOW_H */
