#ifndef REZ_BSPLINE_H
#define REZ_BSPLINE_H

struct Vec3;

void bspline_make_knots(int n, int k, float **out);
void bspline_eval(int k, int n, struct Vec3 *pts, float *kv, struct Vec3 *out, float t);

#endif /* REZ_BSPLINE_H */
