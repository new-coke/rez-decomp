/* Boss wireframes. */

#ifndef REZ_COMMON_BOSS_WIRE_H
#define REZ_COMMON_BOSS_WIRE_H

#include "types_overlays.h"
#include "types_common.h"

void wire_pvr_debug(void);
void wire_edit_vars_add(void);
void wire_edit_vars_remove(void);
int wire_edge_known(float ax, float ay, float az, float bx, float by, float bz);
void wire_nearest_reset(void);
void wire_nearest_check(int x, int y, int idx);
void wire_unproject(float *out, float x, float y, float z);
void *wire_draw_edges(void *lines, int *edges, BossV4 *verts, float width);
int draw_model(int acc, NjCnkModel *model);
int wire_tree_walk(int acc, WireObj *obj);
void cmDcWireDrawObject(void *model, int edges, char *flags, unsigned int color, float width);
int wire_tree_walk_motion(int acc, WireObj *obj);
void wire_draw_motion(WireObj *obj, void *src, int edge_flags, int edges, unsigned int color, float t, float motion_time);
void wire_count_edges(WireObj *obj);
void wire_count_enabled(WireObj *obj, int *count, int *total);
void wire_write_edges(WireObj *obj);
void *wire_build_njw(void *model, void *data, char *flags, int *size, int mode);
int *wire_offsets_to_ptrs(int *base);
void wire_njw_relocate(int *data);
void wire_edit_exec(WireEditWorkView *w);
void wire_edit_disp(void **w);
void wire_edit_release(WireEditWork *w);
char *name_find_dot(char *name);
WireEditWork *wire_edit_create(void *parent, void *model, int edges, unsigned int *motion, char *name);
void wire_edges_build(void *model, int edges);
void wire_edges_free(void);
void wire_edges_rebuild(WireEditWork *w, float rad);
int wire_create_none(void);
void wire_export_model(void *model, char *path);
void wire_export_list(void **bank, int *pairs, int n);
void wire_key_register(void);
void wire_key_unregister(void);
void wire_draw_screen_edges(void **edges, SVtx *verts);
void wire_draw_model(NjCnkModel *model);
void wire_tree_draw(WireObj *obj);
void wire_draw_color(int model, int motion);
void wire_draw_color_b(void *obj, void *arg);
void wire_tree_draw_motion(WireObj *obj);
void wire_draw_color_c(int tree, int motion, float t, int wire);
void wire_colors_set(unsigned int color, unsigned int color2, float width);
void wire_color_set(unsigned int color, float width);
void part_draw_beat(ModelPart *part, int col_a, int col_b, float width);
void part_draw_beat_frame(ModelPart *part, float t, int col_a, int col_b, float width);
void part_draw_beat_motion(ModelPart *part, int motion, int col_a, int col_b, float t, float width);

#endif /* REZ_COMMON_BOSS_WIRE_H */
