/* Camera passes: load, evaluate, debug draw. */

#ifndef REZ_PASS_H
#define REZ_PASS_H

struct PassVec3;
struct PathTblRef;

int sequence_eval(int idx, float time, int *key_out, void *curve0_out, void *curve1_out);
void sequence_lookup(float time, int idx, int *key_out, int *curve0_out, int *curve1_out);
int sequence_load_stage(void);
void sequence_free_stage(void);
int pass_find_by_symbol(int sym);
int pass_eval_relative(float time, int pass, int mode, float *vec, int mask_cam, int mask_look, int mask_pitch, int mask_yaw, int mask_roll);
void pass_eval_channel(int pass, float time, int mode, float *src, float *out, unsigned *flags,
                       unsigned bit, int mask_none, int mask_abs, int mask_cam, int mask_look, int mask_pitch, int mask_yaw,
                       int mask_roll);
unsigned pass_eval_part(int ctx, float time, int *count, float *cam_pos, float *look_at);
void pass_draw_camera_axes(struct PassVec3 *pos, struct PassVec3 *look_at, int roll);
void pass_draw_path(int pass, float time, float step, float last);
void pass_draw_part_path(int pass, float time, float step, float last);
void *pass_table_get(void);
void pass_table_set(void *tbl, int count);
int pass_table_count(void);
float pass_table_start(int i);
float pass_table_end(int i);
void pass_table_push(struct PathTblRef *src);
void pass_table_pop(void);
void pass_rotate_zyx(int *ang);

#endif /* REZ_PASS_H */
