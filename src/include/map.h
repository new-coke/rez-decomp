/* Map tasks, part hooks, settings. */

#ifndef REZ_MAP_H
#define REZ_MAP_H

#include "types_game.h"

struct DcNodeChildView;
struct MapObj;
struct MapHookCtx;
struct MapSortKey;
struct MapVec4;
struct DcNode;
struct MapPart;

void map_hook_show(int *w);
void map_hook_set_display_list3(int *w);
void map_hook_set_beat_phase(float *w, int div);
void map_hook_beat_phase16(int w);
void map_hook_beat_phase8(int w);
void map_hook_beat_phase4(int w);
void map_part_eval_color(float scale, unsigned int *w, int x, int y, int z);
void map_draw_part(struct MapHookCtx *w, float time, float r, float g, float b, float a);
void map_draw_part_add(struct MapHookCtx *w, float time, float r, float g, float b, float a);
void map_draw_part_rgba(struct MapObj *w, float time, float col_r, float col_g, float col_b, float col_a);
void map_hook_draw(struct MapObj *w);
void map_hook_draw_ent_time(struct MapObj *w);
void map_hook_draw_ent_fade(struct MapObj *w);
int map_part_ctx_fill(struct MapObj *w, struct MapHookCtx *out, int idx);
void map_run_part_hooks(struct MapObj *w, int mode);
void map_key_insertion_sort(struct MapSortKey *keys, int n, int k);
void map_key_sort_runs(struct MapSortKey *keys, int n);
void map_draw_sorted(struct MapObj *w);
void map_task_init(struct MapObj *w, int *prm);
void map_task_release(struct MapObj *w);
void map_task_exec(struct MapObj *w);
void map_task_disp(struct MapObj *w);
void map_task_create(void *parent, int model);
void map_task_create_mode1(void *parent, int model);
void map_task_create_mode1_wires(void *parent, int model);
int map_task_create_mode2(void *parent, int model);
void map_task_create_mode2_wires(void *parent, int model);
float map_set_frame(struct MapObj *w, float v);
void map_part_get_vector(struct MapObj *w, int i, struct MapVec4 *out);
void map_part_set_vector(struct MapObj *w, int i, struct MapVec4 *in);
int map_part_set_follow_frame(struct MapObj *w, int i, int follow);
int map_slot_set_clip(struct MapObj *w, int i, int clip);
int map_part_set_cull_flag(struct MapObj *w, int i, int cull);
void *map_get_model(struct MapObj *w);
int map_part_set_wire(struct MapObj *w, int i, int wire);
int map_part_set_color(struct MapObj *w, int i, int col);
int map_part_set_display_list(struct MapObj *w, int i, int dlist);
int map_set_enable(struct MapObj *w, int enable);
int map_set_far_clip(struct MapObj *w, int far_clip);
int map_set_far_fade(struct MapObj *w, int far_fade);
int map_set_sort_type(struct MapObj *w, int sort_type);
void map_rebind_anims(struct MapObj *w);
void map_rebind_anims_mode2(struct MapObj *w);
void map_parts_reset_time(struct MapObj *w);
void map_part_set_radius(struct MapObj *w, int i, float v);
void map_set_part_radii(struct MapObj *w, float *radii);
void map_node_anim_walk(struct DcNode *node);
void dc_object_update(struct MapPart *part, float time);
void map_rotate_zyx(int *ang);

#endif /* REZ_MAP_H */
