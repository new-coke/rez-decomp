/* Debug GUI font. */

#ifndef REZ_GW_FONT_H
#define REZ_GW_FONT_H

struct GwGlyphVtx;

extern int gw_font_cell_h;

void gw_font_init(int font);
void gw_font_get_cell_size(unsigned font, int *cw, int *ch);
int gw_font_set(unsigned font);
float gw_font_set_scale(float scale);
float gw_font_set_depth(float z);
unsigned gw_font_set_color(unsigned col);
void gw_font_putc(float x, float y, unsigned short code);
void gw_font_text_clip(char *s, float x, float y, float clip_x, float clip_y);
void gw_font_icon(int idx, float x, float y);
void gw_font_icon_scaled(float x, float y, float scale, int idx);

#endif /* REZ_GW_FONT_H */
