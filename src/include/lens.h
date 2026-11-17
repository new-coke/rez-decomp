/* Screen-space ribbons, C-Lens strip, lens flares, light beam glows. */

#ifndef REZ_LENS_H
#define REZ_LENS_H

struct PolylineStyle;
struct LensPoint;

float polyline_set_width_min(float v);
float polyline_get_width_min(void);
void clip_to_znear(float *pt, float *other);
int clip_line_2d(float *va, float *vb);
int transform_and_clip_line(float *va, float *vb);
void draw_line_marker(int center_col, int inner_col, int outer_col, float x, float y, float z, float ofs_x, float ofs_y, float inner_scale);
void draw_star(int center_col, int edge_col, float x, float y, float z, float ofs_x, float ofs_y);
void draw_polyline_wide_capped(int st, struct Vec3 *pts, int n);
void draw_polyline_wide_nocap(int st, struct Vec3 *pts, int n);
void draw_polyline_capped(int st, int pts, int n);
void draw_polyline_nocap(int st, int pts, int n);
void c_lens_draw_cap(int col, float x, float y, float z, float dx, float dy);
void c_lens_draw_strip(int tex_id, struct PolylineStyle *tbl, struct LensPoint *pts, int n);
void c_lens_params_load(void);
void c_lens_params_store(void);
void c_lens_init(int *w);
void c_lens_exec(int *obj);
void c_lens_release(void);
void *c_lens_create(void *parent);
void lens_flare_draw(float *pos, float *cam, float radius, float alpha, unsigned int sel);
void lens_flare_draw_mtx(int dir_in, int sel, float size, float alpha);
void lens_flare_draw_point(float size, float alpha, int point, int dir_in, int sel);
void light_beam_draw(unsigned int col, float *pos, float *dir, float half_w);
void light_beam_draw_mtx(int col, int dir_in, float half_w);
void light_beam_draw_point(int col, int point, int dir_in, float half_w);

#endif /* REZ_LENS_H */
