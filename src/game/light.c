/*
 * Scene lighting: one directional light and the ambient colour, as floats and packed PS2 colours
 * (0x80 = full), uploaded to Ninja each frame; Light variable editor group; point light table.
 */

#include "types_gw.h"
#include "gw_varedit.h"
#include "nj_light.h"
#include "ul_matrix.h"
#include "light.h"
#include "color.h"
#include "system.h"

extern float light_dir[3];
extern float light_col_r;
extern float light_col_g;
extern float light_col_b;
extern float light_amb_r;
extern float light_amb_g;
extern float light_amb_b;
extern GwVarDef light_vardef[];
extern GwVarDef light_vardefs[];
extern char gw_varedit_root[];
extern int light_vars_group;
extern void soft_reset(const char *s);
extern int light_amb;
extern int light_amb_cache;
extern int light_col;
extern int light_col_cache;
extern float light_view_dir[3];
extern float intensity_diff;
extern float intensity_ambi;
extern void nj_set_light_intensity(void);
extern float light_points[];

static void light_update_begin(void);
static void light_update_extra(void);
static void light_upload(void);
static void light_update_end(void);
static void light_set_ambient(float r, float g, float b);

/* 0x00127798 */
void light_init(void)
{
    int group;

    soft_reset("Light");
    light_dir[0] = 0.718f;
    light_dir[1] = -0.259f;
    light_dir[2] = -0.646f;
    nj_easy_lights_clear(1);
    nj_lights_clear(1);
    light_set_dir(light_dir);
    light_set_ambient(light_amb_r, light_amb_g, light_amb_b);
    light_set_color(light_col_r, light_col_g, light_col_b);
    group = gw_varedit_add(gw_varedit_root, 0, light_vardef);
    light_vars_group = group;
    gw_varedit_add_table(gw_varedit_root, group, light_vardefs);
}

/* Packed -> float when changed (e.g. in the variable editor). */
/* steer(cast) */
/* 0x00127880 */
static void light_sync_colors(void)
{
    if (light_amb_cache != light_amb) {
        light_amb_cache = light_amb;
        light_amb_r = (float)((int (*)(int))color_get_r)(light_amb_cache) * (1.0f / 255.0f);
        light_amb_g = (float)color_get_g(light_amb_cache) * (1.0f / 255.0f);
        light_amb_b = (float)color_get_b(light_amb_cache) * (1.0f / 255.0f);
    }
    if (light_col_cache != light_col) {
        light_col_cache = light_col;
        light_col_r = (float)((int (*)(int))color_get_r)(light_col_cache) * (1.0f / 255.0f);
        light_col_g = (float)color_get_g(light_col_cache) * (1.0f / 255.0f);
        light_col_b = (float)color_get_b(light_col_cache) * (1.0f / 255.0f);
    }
}

/* Direction to view space, then upload. */
/* 0x001279a8 */
void light_update(void)
{
    ulMatrixApplyVector(ulMatrixCur, light_dir, light_view_dir);
    light_update_begin();
    light_update_extra();
    light_upload();
    light_update_end();
}

/* 0x001279f0 */
static void light_update_begin(void)
{
}

/* 0x001279f8 */
static void light_update_extra(void)
{
}

/* 0x00127a00 */
static void light_upload(void)
{
    ((void (*)(float, float))nj_set_light_intensity)(intensity_diff, intensity_ambi); /* steer(cast) */
    nj_light1_set_vector(light_view_dir[0], light_view_dir[1], light_view_dir[2]);
    nj_light_set_color(1, light_col_r, light_col_g, light_col_b);
    nj_light_set_ambient(light_amb_r, light_amb_g, light_amb_b);
    nj_lights_upload();
}

/* 0x00127a80 */
static void light_update_end(void)
{
}

/* World space. */
/* 0x00127a88 */
void light_set_dir(float *dir)
{
    light_dir[0] = dir[0];
    light_dir[1] = dir[1];
    light_dir[2] = dir[2];
}

/* World space. */
/* 0x00127ab0 */
static void light_get_dir(float *out)
{
    out[0] = light_dir[0];
    out[1] = light_dir[1];
    out[2] = light_dir[2];
}

/* As of the last light_update. */
/* 0x00127ad8 */
static void light_get_view_dir(float *out)
{
    out[0] = light_view_dir[0];
    out[1] = light_view_dir[1];
    out[2] = light_view_dir[2];
}

/* steer(cast) */
/* 0x00127b00 */
void light_set_color_packed(int rgb)
{
    light_col = rgb;
    light_col_r = (float)((int (*)(int))color_get_r)(rgb) * (1.0f / 255.0f);
    light_col_g = (float)color_get_g(rgb) * (1.0f / 255.0f);
    light_col_b = (float)color_get_b(rgb) * (1.0f / 255.0f);
    nj_light_set_color(1, light_col_r, light_col_g, light_col_b);
}

/* 0x00127bb0 */
void light_get_color_packed(int *out)
{
    int ib = (int)(light_col_b * 255.0f);
    int ir = (int)(light_col_r * 255.0f);
    int ig = (int)(light_col_g * 255.0f);

    *out = RGBA(((ir + 1) >> 1) & 0xff, ((ig + 1) >> 1) & 0xff, ((ib + 1) >> 1) & 0xff, 0x80);
}

/* steer(cast) */
/* 0x00127c38 */
void light_set_ambient_packed(int rgb)
{
    light_amb = rgb;
    light_amb_r = (float)((int (*)(int))color_get_r)(rgb) * (1.0f / 255.0f);
    light_amb_g = (float)color_get_g(rgb) * (1.0f / 255.0f);
    light_amb_b = (float)color_get_b(rgb) * (1.0f / 255.0f);
    nj_light_set_ambient(light_amb_r, light_amb_g, light_amb_b);
}

/* Green scaled by 256, the others by 255. */
/* 0x00127ce8 */
void light_get_ambient_packed(int *out)
{
    int ib = (int)(light_amb_b * 255.0f);
    int ir = (int)(light_amb_r * 255.0f);
    int ig = (int)(light_amb_g * 256.0f);

    *out = RGBA(((ir + 1) >> 1) & 0xff, ((ig + 1) >> 1) & 0xff, ((ib + 1) >> 1) & 0xff, 0x80);
}

/* 0..1 per channel; packed form too. */
/* 0x00127d78 */
void light_set_color(float r, float g, float b)
{
    int ir, ig, ib;

    ir = (int)(r * 255.0f);
    ig = (int)(g * 255.0f);
    ib = (int)(b * 255.0f);
    light_col = RGBA(((ir + 1) >> 1) & 0xff, ((ig + 1) >> 1) & 0xff, ((ib + 1) >> 1) & 0xff, 0);
    light_col_r = r;
    light_col_g = g;
    light_col_b = b;
    nj_light_set_color(1, r, g, b);
}

/* 0x00127e00 */
static void light_get_color(float *r, float *g, float *b)
{
    *r = light_col_r;
    *g = light_col_g;
    *b = light_col_b;
}

/* 0..1 per channel; packed form too. */
/* 0x00127e28 */
static void light_set_ambient(float r, float g, float b)
{
    int ir, ig, ib;

    ir = (int)(r * 255.0f);
    ig = (int)(g * 255.0f);
    ib = (int)(b * 255.0f);
    light_amb = (((ib + 1) >> 1) & 0xff) << 16 | ((ig + 1) << 7 & 0xff00) | (((ir + 1) >> 1) & 0xff);
    light_amb_r = r;
    light_amb_g = g;
    light_amb_b = b;
    nj_light_set_ambient(r, g, b);
}

/* 0x00127ea8 */
void light_get_ambient(float *r, float *g, float *b)
{
    *r = light_amb_r;
    *g = light_amb_g;
    *b = light_amb_b;
}

/* 0x00127ed0 */
void light_set_intensity(float diffuse, float ambient)
{
    intensity_diff = diffuse;
    intensity_ambi = ambient;
    nj_set_light_intensity();
}

/* 0x00127ee8 */
void light_get_intensity(float *diffuse, float *ambient)
{
    *diffuse = intensity_diff;
    *ambient = intensity_ambi;
}

/* 0x00127f08 */
static void light_point_set(int i, float *pos, float range_start, float range_end)
{
    float *ent = light_points + i * 8;

    ent[0] = pos[0];
    ent[1] = pos[1];
    ent[2] = pos[2];
    ent[3] = range_start;
    ent[4] = range_end;
    nj_easy_light_set_point(i, pos[0], pos[1], pos[2]);
    nj_light_set_point(i, pos[0], pos[1], pos[2]);
    nj_easy_light_set_range(i, range_start, range_end);
    nj_light_set_range(i, range_start, range_end);
}

/* 0x00127fc0 */
static void light_point_get(int i, float *pos, float *range_start, float *range_end)
{
    float *ent = light_points + i * 8;

    pos[0] = ent[0];
    pos[1] = ent[1];
    pos[2] = ent[2];
    *range_start = ent[3];
    *range_end = ent[4];
}

/* 0x00128000 */
static void light_point_set_color(int i, float r, float g, float b)
{
    float *ent = light_points + i * 8;

    ent[5] = r;
    ent[6] = g;
    ent[7] = b;
    nj_easy_light_set_color(i, r, g, b);
    nj_light_set_color(i, r, g, b);
}

/* 0x00128078 */
static void light_point_get_color(int i, float *r, float *g, float *b)
{
    float *ent = light_points + i * 8;

    *r = ent[5];
    *g = ent[6];
    *b = ent[7];
}

/* 0x001280a8 */
static void light_point_enable(int i, int on)
{
    int j = i;

    do { } while (0);
    nj_easy_light_enable(i, on);
    nj_light_enable(j, on);
}
