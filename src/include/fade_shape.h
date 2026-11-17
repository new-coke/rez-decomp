/* Shape transition renderer. */
#ifndef REZ_FADE_SHAPE_H
#define REZ_FADE_SHAPE_H

struct FadeShapeWork;
struct ShapeNode;

extern float cam_dist;

void fade_shape_triangle(short *vert, unsigned int col, int flag);
void fade_shape_fan_list(short *p, int col);
void fade_shape_draw(int obj, int aux, int motion, int slots, int alpha, float frame);

#endif /* REZ_FADE_SHAPE_H */
