/* Debug GUI drawing. */

#ifndef REZ_GW_DRAW_H
#define REZ_GW_DRAW_H

struct GwWin;

void gw_draw_rect_gradient(float x0, float y0, float x1, float y1, float z, unsigned col, int mode);
void gw_draw_rect(float x0, float y0, float x1, float y1, float z, unsigned col);
unsigned gw_color_scale(int col, float scale);
void gw_draw_title_bar(struct GwWin *win, float z);
void gw_draw_window_body(struct GwWin *win, float z);
unsigned gw_window_get_draw_color(struct GwWin *win);
unsigned gw_window_set_draw_color(struct GwWin *win, unsigned col);
void gw_draw_text_at(struct GwWin *win, int x, int y, char *text, float z);
void gw_win_text_clip(struct GwWin *win, int x, int y, int cw, int ch, char *s);
void gw_win_printf_clip(struct GwWin *win, int x, int y, int cw, int ch, char *fmt, ...);
void gw_win_text(struct GwWin *win, int x, int y, char *s);
void gw_win_printf(struct GwWin *win, int x, int y, char *fmt, ...);

#endif /* REZ_GW_DRAW_H */
