/* Model break-up into loose triangles. */

#ifndef REZ_MODBREAK_H
#define REZ_MODBREAK_H

struct ModbreakOut;
struct ModbreakVtx;
struct ModbreakTri;
struct ModbreakOutTex;
struct DcNodeData;
struct DcNode;
struct MapPart;
struct ModbreakTriRec;
struct ModbreakWork;

int modbreak_project_vertex(struct ModbreakOut *out, struct ModbreakVtx *vtx, float mul_a, float mul_r, float mul_g, float mul_b, float add_r, float add_g, float add_b);
void modbreak_draw_tris(struct ModbreakVtx *vtx, struct ModbreakTri *tri, int n, float mul_a, float mul_r, float mul_g, float mul_b, float add_r, float add_g, float add_b);
int modbreak_project_vertex_tex(struct ModbreakOutTex *out, struct ModbreakVtx *vtx, float mul_a, float mul_r, float mul_g, float mul_b, float add_r, float add_g, float add_b);
void modbreak_draw_tris_tex(struct ModbreakVtx *vtx, struct ModbreakTri *tri, int n, float mul_a, float mul_r, float mul_g, float mul_b, float add_r, float add_g, float add_b);
unsigned short *modbreak_decode_attrs(unsigned flags, unsigned short *p, float *mul, float *add, float sx, float sy, float sz);
void modbreak_draw_compiled(struct ModbreakTri *tri, short *cmd, int vtx_buf);
int modbreak_strip_size(short *p);
int modbreak_strip3_size(short *p);
void modbreak_count_commands(struct DcNodeData *pay, int *ntri, int *nshort);
void modbreak_count_tree(struct DcNode *node, int *ntri, int *nshort);
void modbreak_count(struct MapPart *w, int *ntri, int *nshort);
void modbreak_transform_points(int *p);
int modbreak_compile_strips(short *p, int flag);
int modbreak_compile_strips3(short *p);
int modbreak_compile_commands(unsigned short *p);
int modbreak_compile_tree(struct DcNode *node);
void modbreak_build_buffers(struct MapPart *w, int *tbl, int *buf, short *sbuf);
void modbreak_scatter(struct ModbreakTriRec *tri, int n, int amp, float *center, float scale, float range);
void modbreak_compile(struct ModbreakWork *obj, struct MapPart *w);
void modbreak_work_scatter(struct ModbreakWork *w, int amp, float *center, float scale, float range);
void modbreak_work_free(struct ModbreakWork *w);
void modbreak_work_draw(struct ModbreakWork *w);

#endif /* REZ_MODBREAK_H */
