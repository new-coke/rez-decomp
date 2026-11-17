/* Triangle strips in begin/vertices/end batches. */

#ifndef REZ_NJ_STRIP_H
#define REZ_NJ_STRIP_H

struct NjVtxUV;
struct NjColVtx;
struct NjVec2;
struct NjTexVtx;

void nj_tex_strip_begin(int trans);
void nj_tex_strip_vertices(struct NjVtxUV *verts, int n);
void nj_tex_strip_end(void);
void nj_color_strip_begin(int trans);
void nj_color_strip_vertices(struct NjColVtx *verts, int n);
void nj_color_strip_end(void);
void nj_flat_strip_begin(unsigned int col, int trans);
void nj_flat_strip_vertices(struct NjVec2 *verts, int n, float z);
void nj_flat_strip_end(void);
void nj_clip_tex_strip_begin(int trans);
void nj_clip_tex_strip_vertices(struct NjTexVtx *vtx, int n);
void nj_clip_tex_strip_end(void);
void nj_clip_tex_strip_draw(struct NjTexVtx *verts, int n, int trans);
void nj_clip_color_strip_begin(int trans);
void nj_clip_color_strip_vertices(struct NjColVtx *vtx, int n);
void nj_clip_color_strip_end(void);

#endif /* REZ_NJ_STRIP_H */
