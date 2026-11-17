/* Polygon font glyph table. */

#ifndef REZ_BS_GLYPH_H
#define REZ_BS_GLYPH_H

struct GlyphInfo;

void glyph_init(void);
void glyph_lookup(int ch, void **verts, int *count);

#endif /* REZ_BS_GLYPH_H */
