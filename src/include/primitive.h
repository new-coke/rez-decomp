/* Grid primitive. */

#ifndef REZ_PRIMITIVE_H
#define REZ_PRIMITIVE_H

struct PrimitiveGrid;
struct PrimitiveGrid;

struct PrimitiveGrid *primitive_grid_create(int width, int height);
void primitive_grid_free(void **obj);
void primitive_grid_set_uv(float du, float dv, struct PrimitiveGrid *grid, int width, int height);

#endif /* REZ_PRIMITIVE_H */
