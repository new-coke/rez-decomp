/* Ninja light slots and upload. */

#ifndef REZ_NJ_LIGHT_H
#define REZ_NJ_LIGHT_H

void nj_lights_clear(int n);
void nj_light_enable(int i, int on);
void nj_light_set_ambient(float x, float y, float z);
void nj_light_set_color(int i, float x, float y, float z);
void nj_light1_set_vector(float x, float y, float z);
void nj_light_set_point(int i, float x, float y, float z);
void nj_light_set_range(int i, float near_range, float far_range);
void nj_lights_upload(void);
void nj_easy_lights_clear(int n);
void nj_easy_light_enable(int i, int on);
void nj_easy_light_set_color(int i, float x, float y, float z);
void nj_easy_light_set_point(int i, float x, float y, float z);
void nj_easy_light_set_range(int i, float near_range, float far_range);
void nj_light_exit(void);

#endif /* REZ_NJ_LIGHT_H */
