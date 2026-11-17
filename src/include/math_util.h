/* Random generators, vectors, curves, transforms, quaternions. */

#ifndef REZ_MATH_UTIL_H
#define REZ_MATH_UTIL_H

struct SplineCfg;

extern int rand_lcg_count_a;
extern int rand_lcg_count_b;

void rand_lcg_seed_a(int seed);
void rand_lcg_seed_b(int seed);
int rand_lcg_next_a(void);
int rand_lcg_next_b(void);
void float_frac(void);
float vec2_normalize_xz(float *vec, float *out);
float vec3_normalize_into(float *out, float x, float y, float z);
float vec3_length_squared(float *vec);
float vec3_distance_xz(float *va, float *vb);
float vec3_distance_squared(struct Vec3 *va, struct Vec3 *vb);
void bezier_cubic_eval(float *pts, float *out, float t);
void spline_cardinal_eval(float t, float *pts, float *out, struct SplineCfg *cfg);
void matrix_unit_set(int mtx);
void matrix_apply_vectors(float *src, float *dst, int n);
void matrix_apply_points(float *src, float *dst, int n);
void matrix_apply_pairs(float *src, float *dst, int n);
void quat_multiply(float *qa, float *qb, float *out);
void quat_nlerp(float *from, float *to, float t, float *out);
void quat_from_euler(int *ang, float *quat);
void quat_to_matrix(float *quat);
void quat_to_matrix_split(float qw, float *vec);

#endif /* REZ_MATH_UTIL_H */
