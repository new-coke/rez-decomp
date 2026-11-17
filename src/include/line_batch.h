/* Batched lines through the VU0 and VU1 line renderers. */

#ifndef REZ_LINE_BATCH_H
#define REZ_LINE_BATCH_H

void line_flush_vu1(void);
void line_flush_vu0(void);
int line_set_color(int color);
void line_flush(void *ret);
void *line_add_v(void *ret, float *start, float *end);
void *line_add_seg(void *ret, float x0, float y0, float z0, float x1, float y1, float z1);
void *line_add_v_w(void *ret, float *start, float *end);
void *line_add_f_w(void *ret, float x0, float y0, float z0, float x1, float y1, float z1);
void *line_add_seg_w(void *ret, float x0, float y0, float z0, float x1, float y1, float z1);
int line_set_translucent(int on);
void line_end(void *ret);
void *line_add_seg_rgb(void *ret, int start_col, int end_col, float x0, float y0, float z0, float x1, float y1, float z1);
void *line_add_v_rgb(void *ret, int start_col, int end_col, float *start, float *end);

#endif /* REZ_LINE_BATCH_H */
