/* Axis markers, crosses, bar gauges. */

#ifndef REZ_DEBUGDRAW_H
#define REZ_DEBUGDRAW_H

void debug_draw_box(int n, float half);
void debug_draw_cross(int dev, float x, float y, float z, float half);
void debug_draw_gauge(float z, float x, float y, float width, float height, float rate, int bar_col, int bg_col);
void debug_draw_gauge_dual(float z, float x, float y, float width, float height, float rate, float add_rate, int bar_col, int add_col,
                 int bg_col);

#endif /* REZ_DEBUGDRAW_H */
