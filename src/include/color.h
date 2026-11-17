/* Packed colour helpers; ring line primitive. */

#ifndef REZ_COLOR_H
#define REZ_COLOR_H

#define RGBA(r, g, b, a) ((a) << 24 | (b) << 16 | (g) << 8 | (r))
#define ARGB(a, r, g, b) ((a) << 24 | (r) << 16 | (g) << 8 | (b))

#define COL_WHITE RGBA(0x80, 0x80, 0x80, 0x80)
#define COL_BLACK RGBA(0, 0, 0, 0x80)

int draw_ring_xz(float radius, int col, int n);
unsigned int color_blend(int ia, int ib, float t);
int color_get_a(unsigned int color);
int color_get_r(unsigned char color);
int color_get_g(unsigned int color);
int color_get_b(unsigned int color);

#endif /* REZ_COLOR_H */
