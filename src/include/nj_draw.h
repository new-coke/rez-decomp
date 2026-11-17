/* Object trees, shapes, screen strips, cube clip test. */

#ifndef REZ_NJ_DRAW_H
#define REZ_NJ_DRAW_H

struct NjVec3;
struct NjObject;
struct NjPointList;
struct NjColVtx;
struct NjTexVtx;

int nj_cube_clipped(struct NjVec3 *center, float radius);
void nj_set_screen_offset(float x, float y);
int nj_shape_draw(int shape);
void nj_object_draw_shapes(int obj);
void nj_tex_strip_draw(void *verts, int n, void *trans);
void nj_screen_strip_draw(struct NjColVtx *vtx, int n, int trans);

#endif /* REZ_NJ_DRAW_H */
