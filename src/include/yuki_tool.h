/* Effect toolkit: sprites, clipping, aiming, node positions, beat colours. */

#ifndef REZ_YUKI_TOOL_H
#define REZ_YUKI_TOOL_H

struct Vec3;
struct YNode;
struct YModel;
struct YSave;

void *yuki_alloc(unsigned int size);
void yuki_free(void *buf);
int yuki_is_offscreen(float *pos);
float yuki_clip_line(struct Vec3 *start, struct Vec3 *end);
void yuki_sprite_draw(int angle, unsigned int col, int tex, float x, float y, float z, float half_w,
                   float half_h);
void yuki_sprite_draw3d_min(int angle, unsigned int col, int tex, struct Vec3 *pos, float half_w, float half_h,
                   float wmin, float hmin);
void yuki_sprite2_draw(int angle, unsigned int col, int tex, float x, float y, float z, float half_w,
                   float half_h);
void yuki_sprite2_draw3d(int angle, unsigned int col, int tex, struct Vec3 *pos, float half_w, float half_h);
void yuki_sprite2_draw3d_min(int angle, unsigned int col, int tex, struct Vec3 *pos, float half_w, float half_h,
                   float wmin, float hmin);
void yuki_matrix_get_scale(float *scale);
void yuki_draw_aimed(struct Vec3 *to, struct Vec3 *from, int *rot);
void yuki_lerp3(int *out, int *from, int *to, float rate);
int yuki_node_count(struct YNode *node, unsigned int mask, int count);
void yuki_model_positions(struct Vec3 *out, struct YModel *model, unsigned int mask, int count);
void yuki_model_positions_at(struct Vec3 *out, struct Vec3 *pos, void **rot, struct YModel *model, unsigned int mask, int count);
int yuki_camera_relative(int pos, int clip_ang, float t);
float yuki_beat_phase(int div);
unsigned int yuki_beat_color(int col_from, int col_to, int div, int linear, float scale, float znear,
                           float zfar, float anear, float afar);
struct YSave *yuki_save_create(struct YNode *node);
void yuki_save_free(struct YSave *save);
void yuki_save_restore(struct YSave *save, struct YNode *node, unsigned int mask);

#endif /* REZ_YUKI_TOOL_H */
