/* Wire renderer and wire packet compiler. */

#ifndef REZ_REZ_WIRE_COMPILE_H
#define REZ_REZ_WIRE_COMPILE_H

struct WireLineVtx;
struct VifBuf;
struct MeshObj;
struct ScanRec;
struct CmdCtx;
struct CmdRec;
struct DNode;
struct DCtx;
struct PktObj;
struct RezWireCompileDNode;
struct RezWireCompileDCtx;
struct DumpNode;
struct TNode;
struct TCtx;
struct SNode;
struct WireMesh;
struct WHdr;
struct WNode;
struct WEnt;
struct WireObj;

extern char nj_clip_matrix[];

void line_begin_vu0(int on);
void line_draw_vu0(void *src, int n);
void line_end_vu0(void);
void line_begin_vu1(int on);
void line_draw_vu1(struct WireLineVtx *src, int n);
void line_end_vu1(void);
unsigned int draw_set_color(unsigned int color);
float line_width_set(float v);
void draw_clear_poly_count(void);
unsigned int draw_cull_box(float half, float *center);
int model_obj_draw_direct(int *obj, int rec);
int model_cmd_transform(struct CmdRec *rec);
int model_obj_draw_normal(int *obj, int rec);
void model_obj_draw(void *obj, void *rec);
void model_packet_draw_normal(struct PktObj *obj);
void model_packet_draw_simple(struct PktObj *obj);
void wire_write(int wire, int root, int list);
int wire_tree_packet_size(struct SNode *ent);
void wire_free(int block);
int wire_packet_write(int *obj, struct WireMesh *mesh, unsigned int *out);
void *wire_compile(struct WireObj *obj, void *list);
void wire_alloc_set(int alloc_fn, int free_fn);

#endif /* REZ_REZ_WIRE_COMPILE_H */
