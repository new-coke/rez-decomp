/* Uranus boss main task. */

#ifndef REZ_B_URANUS_URANUS_H
#define REZ_B_URANUS_URANUS_H

#include "types_overlays.h"
#include "b_uranus.h"
#include "types_common.h"

struct UranusVtx;
struct UranusSeqWork;

void *module_entry(void *parent, void *arg);
void star_color_apply(void);
void *uranus_tc_create(void *parent);
struct UranusSeqWork *uranus_seq_create(UranusWork *parent);
int boss_main_demo_cb(BossMainWork *w);
void uranus_init(UranusWork *w, int *arg);
void uranus_exec_start(UranusWork *w);
void uranus_exec_load(UranusWork *w);
void uranus_release(UranusWork *w);
void uranus_end_tex_anime_start(void *w);
void uranus_start_finish(UranusWork *w);
void uranus_seq_exec(struct UranusSeqWork *seq);
void uranus_get_fog(int *w);
void uranus_disp(UranusWork *w);
UranusWork *uranus_create(void *parent, void *arg);
void task_finish_if(void *w);
int uranus_level_add(int i);
void uranus_player_damage(void);
void uranus_nodes_init(void);
void uranus_nodes_free(void);
UranusNode *uranus_node_alloc(void);
UranusNode *uranus_node_new(void);
UranusNode *uranus_node_add_child(UranusNode *n);
UranusNode *uranus_node_add_next(UranusNode *n);
void uranus_node_copy_tree(UranusNode *dst, UranusNode *src);
void uranus_node_free(UranusNode *n);
void uranus_node_free_tree(UranusNode *n);
void uranus_node_clear_flags(UranusNode *n, int mask);
int uranus_node_count_children(UranusNode *n);
int uranus_node_count_tree(UranusNode *n, int *cnt);
void uranus_node_draw_debug(UranusNode *n);
void venus_fog_exec(VenusFogWork *w);
void venus_fog_release(void);
VenusFogWork *venus_fog_create(void *parent, BossFog *to, float secs);
void uranus_col_tri_bound(Vec3 *p0, Vec3 *p1, Vec3 *p2, Vec3 *ctr, float *rad, float *rad_sq);
int uranus_col_strip_build(short *p, struct UranusVtx *v, int stride, char *rec);
int uranus_col_poly_build_cyl(short *p, struct UranusVtx *v, int mode, char *rec);
int uranus_col_poly_build_wall(short *p, struct UranusVtx *v, int mode, char *rec);
int uranus_col_mesh_build(void *mesh, char *rec, int *idx, int mode);
void uranus_col_tree_build_r(YNode *n, char *recs, int *idx, int *depth, int arg);
int uranus_col_tree_count(YNode *n);
void uranus_col_tree_build(YNode *n, char *recs);
int uranus_col_find_near(char **out, char *recs, Vec3 *pos, int n, float rad);
void uranus_col_debug_draw(int (*recs)[16], int n);
void uranus_col_tri_init(int *rec, Vec3 *p0, Vec3 *p1, Vec3 *p2, Vec3 *ref);
int uranus_col_tri_hit(Vec3 *ray, int va, int vb, int vc, int *nrm, float dist);
int uranus_col_tri_hit_uv(float *ray, Vec3 *nrm, float plane_d, float ofs_x, float ofs_y, float curve_k, float u_div, float v_k);
int uranus_col_check(int *rec, int pos, int near, float rad);
void uranus_col_merge_cyls(int (*recs)[16], int n);
float uranus_col_cyl_y_ends(float *rec, float *top, float *bottom);
float uranus_col_cyl_x_ends(float *rec, float *end0, float *end1);
void uranus_col_cyl_y_init(char *rec, Vec3 *va, Vec3 *vb, Vec3 *vc, Vec3 *n);
void uranus_col_cyl_x_init(char *rec, Vec3 *va, Vec3 *vb, Vec3 *vc, Vec3 *n);
void uranus_col_cyl_init(char *rec, Vec3 *va, Vec3 *vb, Vec3 *vc, Vec3 *n);
int uranus_col_cyl_y_hit(int *rec, float *p, float rad);
int uranus_col_cyl_x_hit(int *rec, float *p, float rad);
int uranus_col_cyl_merge(int *src, int *dst);
void uranus_col_plane_init(int *rec, Vec3 *va, Vec3 *vb, Vec3 *vc, Vec3 *ref);
void uranus_col_wall_init(char *rec, Vec3 *va, Vec3 *vb, Vec3 *vc, Vec3 *nrm);
int uranus_col_plane_hit(float *nrm, float *p, float rad);
int uranus_col_wall_hit(float *rec, float *pos, float rad);

#endif /* REZ_B_URANUS_URANUS_H */
