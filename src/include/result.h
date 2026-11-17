/* Stage result screen; background clear. */

#ifndef REZ_RESULT_H
#define REZ_RESULT_H

struct ResultScoreWork;
struct ResultMotionWork;
struct BgDeleteWork;
struct ResultNumberWork;
struct ResultRate;
struct WorldDrawWork;
struct ResultShellWork;
struct Camera;
struct ResultWork;
struct ResultDef;

void result_analyze(struct ResultWork *w);
void result_init(struct ResultWork *w, struct ResultDef *res);
void result_lock_input(struct ResultWork *w);
void result_analyze_set_next(struct ResultWork *w);
void result_show_exec(struct ResultWork *w);
void result_exec(struct ResultWork *w);
void result_disp(void);
void *result_create(void);
void result_draw_number(int val, float x, float y, float z);
void result_score_release(void);
void result_score_exec(struct ResultScoreWork *w);
void result_score_disp(struct ResultScoreWork *w);
void *result_score_create(void *parent);
int result_hit_callback(int *node, int idx, struct ResultRate *rec);
void result_draw_glyph(float x, float y, float z, float width, float height, int col, float *uv);
void result_draw_float(struct ResultRate *rec, int res, float v);
void result_number_init(struct ResultNumberWork *w, struct ResultWork *res);
void result_number_disp(struct ResultNumberWork *w);
void *result_number_create(void *parent);
void result_motion_cam_init(void);
void result_motion_cam_update(struct Camera *dst, struct ResultMotionWork *src);
void result_motion_cam_release(void);
void result_motion_init(struct ResultMotionWork *w, struct ResultWork *res);
void result_motion_release(struct ResultMotionWork *w);
void result_motion_exec(struct ResultMotionWork *w);
void result_motion_disp(struct ResultMotionWork *w);
void *result_motion_create(void *w);
float result_motion_get_remaining(struct ResultMotionWork *w);
void world_draw_init(struct WorldDrawWork *w, struct ResultWork *res);
void world_draw_release(struct WorldDrawWork *w);
void world_draw_exec(struct WorldDrawWork *w);
void world_draw_disp(struct WorldDrawWork *w);
void *world_draw_create(void *parent);
void result_shell_init(struct ResultShellWork *w, struct ResultWork *res, float *vals, float thr);
void result_shell_release(struct ResultShellWork *w);
void result_shell_exec(struct ResultShellWork *w);
void result_shell_disp(struct ResultShellWork *w);
void *result_shell_create(float thr, struct ResultWork *parent, float *vals);
void background_delete_init(struct BgDeleteWork *w);
void background_delete_restore(struct BgDeleteWork *w);
void background_delete_start(struct BgDeleteWork *w, void *owner, int frames);
void background_delete_release(struct BgDeleteWork *w);
void background_delete_exec(struct BgDeleteWork *w);
void background_delete_disp(struct BgDeleteWork *w);
void *background_delete_create(void *owner, int frames);
void result_rotate_yxz(int *ang);

#endif /* REZ_RESULT_H */
