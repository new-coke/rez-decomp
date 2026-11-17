#ifndef REZ_ENEMY_TASK_H
#define REZ_ENEMY_TASK_H

struct ActObj;
struct ObjPlace;
struct EnemyDesc;
struct ObjDesc;
struct Vec3;
struct IVec3;
struct ActSub;

void enemy_task_set_appear(struct ActObj *w, int flags, int frames);
void enemy_task_set_vanish(struct ActObj *w, int flags, int frames);
void enemy_task_appear_scale(struct ActObj *w, void *work);
void enemy_task_vanish_scale(struct ActObj *w, void *work);
void enemy_task_draw_fade(struct ActObj *w);
int obj_draw_at(int unused, struct ObjPlace *obj, void *model);
int enemy_task_task_create(void *parent, struct ObjDesc *desc);
void enemy_task_spawn(struct ActObj *w, int mask, int max, int out_a, int out_b, int out_c);
void enemy_task_get_pos(struct ActObj *w, struct Vec3 *out);
void enemy_task_get_rot(struct ActObj *w, int *out);
void enemy_task_get_param_vec(struct ActObj *w, struct Vec3 *out);
void enemy_task_get_scl(struct ActObj *w, struct Vec3 *out);
int enemy_task_get_pstate(struct ActObj *w);
int enemy_task_get_phase(struct ActObj *w);
float enemy_task_get_time(struct ActObj *w);
float enemy_task_get_state_time(struct ActObj *w);
float enemy_task_get_param(struct ActObj *w, int i);
struct Vec3 * enemy_task_pos_ptr(struct ActObj *w);
struct IVec3 * enemy_task_rot_ptr(struct ActObj *w);
struct Vec3 * enemy_task_param_vec_ptr(struct ActObj *w);
struct Vec3 * enemy_task_scl_ptr(struct ActObj *w);
void * enemy_task_work_ptr(struct ActObj *w);
void *enemy_task_gfx_ptr(struct ActObj *w);
struct ActSub * enemy_task_lock_ptr(struct ActObj *w);
int enemy_task_get_status_slot(struct ActObj *w);
int enemy_task_get_link(struct ActObj *w);
void enemy_task_get_hit_imp(struct ActObj *w, struct Vec3 *out);
void enemy_task_set_pos(struct ActObj *w, struct Vec3 *pos);
void enemy_task_set_rot(struct ActObj *w, int *rot);
void enemy_task_set_param_vec(struct ActObj *w, struct Vec3 *vec);
void enemy_task_set_scl(struct ActObj *w, struct Vec3 *scale);
void enemy_task_set_state(struct ActObj *w, int state);
void enemy_task_next_phase(struct ActObj *w);
void enemy_task_clear_flags(struct ActObj *w, int bits);
void enemy_task_set_end_event(struct ActObj *w, int event);
void enemy_task_set_status_pos(struct ActObj *w, int pos);
void enemy_task_rotate_yxz(struct IVec3 *rot);

#endif /* REZ_ENEMY_TASK_H */
