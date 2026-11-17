/* ul vector and matrix routines, hand-written assembly (mostly VU0). */

#ifndef REZ_UL_VU0_H
#define REZ_UL_VU0_H

float rsqrtf(float);
struct Vec3;

float vec3_length(struct Vec3 *v);
float vec3_normalize(struct Vec3 *d, struct Vec3 *s);
void vu0_matrix_copy(float *, int);
void vu0_matrix_mul(float *d, float *a, float *b);
void vu0_matrix_normalize3(float *, float *);
void vu0_matrix_rotate_x(float *m, int a);
void vu0_matrix_rotate_x_sc(float *m, float a, float b);
void vu0_matrix_rotate_y(float *m, int a);
void vu0_matrix_rotate_y_sc(float *m, float a, float b);
void vu0_matrix_rotate_z(float *m, int a);
void vu0_matrix_rotate_z_sc(float *m, float a, float b);
void vu0_matrix_scale(float *m, float x, float y, float z);
void vu0_matrix_translate(float *m, float x, float y, float z);
void vu0_qword_copy(void *, void *);
void vu0_vec3_normalize(float *dst, float *src);
void vu0_vec_add(void *, void *, void *);
void vu0_vec_clamp(void *, void *, float, float);
void vu0_vec_mul(void *, void *, void *);
void vu0_vec_scale(void *, void *, float);

#endif /* REZ_UL_VU0_H */
