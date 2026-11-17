#ifndef REZ_CAMERA_H
#define REZ_CAMERA_H

struct CamHooks;
struct Camera;
struct CamNode;
struct CamScene;
struct Vec3;

extern float cam_near;

void camera_set_fov(int fov);
void camera_hooks_clear(struct CamHooks *hooks);
void camera_init_defaults(struct Camera *cam);
void camera_node_init(struct CamNode *node, int id, struct CamHooks *hooks);
void camera_system_init(void);
void camera_node_disp(void);
void camera_task_init(void);
void camera_system_exec(void);
void camera_task_disp(void);
void camera_task_create(void);
void camera_build_matrices(struct Camera *cam, float *view_mtx, float *inv_mtx);
void camera_load_view_matrix(int mode, void *mtx, void *fov);
void camera_update(void);
void camera_task_exec(void);
struct CamNode *camera_node_add(unsigned int id, struct CamHooks *hooks, int user);
void camera_node_remove(struct CamNode *node);
int camera_get_mode(void);
int camera_get_view_mtx(void);
int camera_get_inv_mtx(void);
int camera_get_main_view_mtx(void);
struct CamScene *camera_scene_if_active(void);
void camera_get_eye(struct Vec3 *out);
void camera_get_view_vec(struct Vec3 *vec);
void camera_get_angles(int *ang);
int camera_get_fov(void);
void camera_get_params(void *eye, void *vec, void *ang);
struct Camera *camera_get_current(void);
int camera_get_main_cam(void);
void camera_move_eye(struct Vec3 *eye);
void camera_look_offset(struct Vec3 *ofs);
void camera_set_fov_direct(int fov);
void camera_set_lookat(struct Vec3 *eye, struct Vec3 *center, struct Vec3 *up);
void camera_set_far(float x);
float camera_get_far(void);
void camera_lerp(float t, struct Camera *cam, struct Camera *target, int mask);
void camera_eval_motion(struct Camera *cam, float *src, void *hdr, float t);
void camera_debug_draw(int win);
void camera_window_toggle(void);
void camera_free_toggle(void);
void camera_matrix_window_closed(void);
void camera_inv_matrix_window_closed(void);
void camera_matrix_window_toggle(void *menu);
void camera_debug_menu_init(void);
void camera_update_angles(void);
void camera_free_fly(float *out);
void camera_free_end(int cam);

#endif /* REZ_CAMERA_H */
