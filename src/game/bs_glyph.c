/* Polygon font glyphs for ASCII 33..126, from the system PNB archive, scaled to unit size. */

#include "bs_glyph.h"
#include "pnb.h"

typedef struct GlyphInfo {
    void *verts;
    int count;
} GlyphInfo;

typedef struct GlyphVtx {
    float x, y, z;
    float u, v, w;
} GlyphVtx;


typedef struct FontData { char *base; } FontData;
typedef struct FontObj { char pad_00[4]; FontData *data; } FontObj;
typedef struct FontRef { FontObj *font; } FontRef;

extern GlyphInfo glyph_table[94];
extern int system_pnb;

static void glyph_table_init(void);

/* 0x001038f0 */
void glyph_init(void)
{
    glyph_table_init();
}

/* Zeros outside 33..126. */
/* 0x001038f8 */
void glyph_lookup(int ch, void **verts, int *count)
{
    unsigned int i = ch - 33;

    if (i >= 94) {
        if (verts) *verts = 0;
        if (count) *count = 0;
    } else {
        GlyphInfo *glyph = &glyph_table[i];
        if (verts) *verts = glyph->verts;
        if (count) *count = glyph->count;
    }
}

/* Scales positions down by 16 in place. */
/* steer(cast) */
/* 0x00103950 */
static void glyph_load(int ch, GlyphInfo *out)
{
    char *data;
    short *words;
    int n;
    GlyphVtx *p;

    data = (char *)((FontRef *(*)(int, int))pnb_get_entry)(system_pnb, ch)->font->data;
    data = ((FontData *)data)->base;
    data += 6;
    words = (short *)data;
    n = *words++;
    p = (GlyphVtx *)words;
    out->verts = p;
    out->count = n;
    if (n > 0) {
        do {
            p->x *= 0.0625f;
            p->y *= 0.0625f;
            p->z *= 0.0625f;
            p++;
        } while (--n);
    }
}

/* PNB entries 10..103 -> characters 33..126. */
/* 0x001039e0 */
static void glyph_table_init(void)
{
    int i;
    int ch = 10;
    GlyphInfo *p = glyph_table;

    for (i = 94; i != 0; i--) { glyph_load(ch, p); ch++; p++; }
}
