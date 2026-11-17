#ifndef REZ_LIGHT_H
#define REZ_LIGHT_H

void light_init(void);
void light_update(void);
void light_set_dir(float *dir);
void light_set_color_packed(int rgb);
void light_get_color_packed(int *out);
void light_set_ambient_packed(int rgb);
void light_get_ambient_packed(int *out);
void light_set_color(float r, float g, float b);
void light_get_ambient(float *r, float *g, float *b);
void light_set_intensity(float diffuse, float ambient);
void light_get_intensity(float *diffuse, float *ambient);

#endif /* REZ_LIGHT_H */
