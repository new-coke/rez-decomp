/* Debug GUI shapes in window coordinates. */

#ifndef REZ_GW_RECT_H
#define REZ_GW_RECT_H

struct GwWin;

void gw_draw_bevel_vline(struct GwWin *win, int mode, int x, int y0, int y1, unsigned col, int layer);
void gw_draw_rect_fill(struct GwWin *win, int x0, int y0, int x1, int y1, unsigned col, float layer);
void gw_draw_rect_fill_gradient(struct GwWin *win, int x0, int y0, int x1, int y1, unsigned *col, float layer);

#endif /* REZ_GW_RECT_H */
