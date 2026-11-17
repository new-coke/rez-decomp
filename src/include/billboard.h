/* Screen-space sprites and rectangles. */

#ifndef REZ_BILLBOARD_H
#define REZ_BILLBOARD_H

void billboard_draw(int tex_idx, int tex, int pt, float width, float height);
void billboard_draw_uv(int tex_idx, int tex, int pt, float *uv, float width, float height);
void tex_rect_draw(int tex_idx, int tex, float x, float y, float z, float width, float height);
void tex_rect_draw_uv(int tex_idx, int tex, float *uv, float x, float y, float z, float width, float height);
void billboard_draw_rot(int tex_idx, int tex, int ang, int pt, float width, float height);
void tex_rect_draw_rot(int tex_idx, int tex, int ang, float cx, float cy, float z, float width, float height);
void rect_draw_outline(float x, float y, float z, float width, float height, int line_col, int col);

#endif /* REZ_BILLBOARD_H */
