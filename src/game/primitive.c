/* Grid primitive: textured grid as a Ninja chunk model; vertices supplied separately. */

#include "primitive.h"
#include "heap.h"

typedef struct PrimitiveGrid {
    int vlist;
    short *plist;
    float x, y, z, radius;
} PrimitiveGrid;


/* One strip per row over a (width + 1) x (height + 1) lattice; UVs over 640 x 960; radius 1000. */
/* 0x00156db0 */
PrimitiveGrid *primitive_grid_create(int width, int height)
{
    PrimitiveGrid *grid;
    short *dst;
    int cols;
    int du;
    int n;
    int dv;
    int u;
    int nv;
    int i;
    int v;
    int j;
    int nx;

    grid = heap_block_tag(game_malloc(24), "primitive.c", 42);
    cols = width + 1;
    grid->vlist = 0;
    n = (cols * 6 + 1) * height;
    dst = heap_block_tag(game_malloc((8 + n) * 2), "primitive.c", 52);
    grid->plist = dst;
    *dst++ = 13320;
    v = 0;
    *dst++ = 16384;
    *dst++ = 66;
    *dst++ = n + 1;
    *dst++ = height;
    du = 640 / width;
    dv = 960 / height;
    for (j = 0; height > j; j++) {
        nx = cols;
        *dst++ = cols * 2;
        u = 0;
        for (i = 0; nx > i; i++) {
            dst[1] = u;
            dst[4] = u;
            dst[2] = v;
            dst[0] = (1 + height) * i + j;
            dst[5] = v + dv;
            dst[3] = dst[0] + 1;
            u += du;
            dst += 6;
        }
        v += dv;
    }
    *dst++ = 255;
    grid->x = grid->y = grid->z = 0.0f;
    grid->radius = 1000.0f;
    dst[0] = 255;
    dst[1] = 255;
    return grid;
}

/* 0x00156f78 */
void primitive_grid_free(void **obj)
{
    heap_free(obj[1], 0);
    heap_free(obj, 0);
}

/* UVs cover du x dv. */
/* 0x00156fb0 */
void primitive_grid_set_uv(float du, float dv, PrimitiveGrid *grid, int width, int height)
{
    short *dst = grid->plist;
    float fv = 0;
    float fu;
    int i;
    int j;

    du = du / (float)width;
    dv = dv / (float)height;
    for (j = 0, dst += 5; j < height; j++) {
        dst = (short *)((char *)dst + 2);
        fu = 0;
        for (i = 0; i < width + 1; i++) {
            dst[1] = (int)fu;
            dst[2] = (int)fv;
            dst[5] = (int)(fv + dv);
            dst[4] = (int)fu;
            fu += du;
            dst += 6;
        }
        fv += dv;
    }
}
