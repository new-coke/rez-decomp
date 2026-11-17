/* Seraphim, Area 5 enemy. */

#ifndef REZ_STG05_SERAPHIM_H
#define REZ_STG05_SERAPHIM_H

#include "types_overlays.h"
#include "types_common.h"
#include "stg05.h"

struct SeraphimPartRec;
struct SeraphimWork;
struct SeraphimNodeArg;
struct SeraphimMirrorCtx;
struct SeraphimWalk;
struct SeraphimPartDraw;
struct SeraphimNodeCamView;
struct SeraphimNode;

typedef struct SeraphimNodeCamView SeraphimNodeCamView;
typedef struct SeraphimPartRec SeraphimPartRec;
typedef struct SeraphimNode SeraphimNode;
typedef struct SeraphimWork SeraphimWork;
typedef struct SeraphimNodeArg SeraphimNodeArg;
typedef struct SeraphimMirrorCtx SeraphimMirrorCtx;
typedef struct SeraphimPartDraw SeraphimPartDraw;
typedef struct SeraphimWalk SeraphimWalk;

void seraphim_vars_register(void);
void seraphim_vars_unregister(void);
int seraphim_get_init_arg(void);
void seraphim_models_load(void);
void seraphim_models_free(void);
int seraphim_vars_init(int *prm);
int seraphim_vars_release(void);
int seraphim_marker_draw(int obj, void *place);
int seraphim_part_node_cb(unsigned int *n, int idx, SeraphimPartRec **cur);
void seraphim_pose_parts(void *obj, SeraphimWork *w);
int seraphim_node_cb(unsigned int *n, int idx, SeraphimNodeArg *prm);
void seraphim_pose_model(void *obj, SeraphimWork *w);
void seraphim_mirror_node_cb(unsigned int *n, SeraphimMirrorCtx *ctx);
void seraphim_eval_nodes_mirrored(ModelPart *part, Vec3 *src, Vec3 *dst);
void seraphim_wing_node_cb(unsigned int *n, SeraphimWalk *ctx);
void seraphim_update_wings(void *obj, SeraphimWork *w);
int seraphim_part_record(int i, int *k);
void seraphim_draw_part(SeraphimPartDraw *p, int i);
int seraphim_root_node_cb(unsigned int *n, int idx, SeraphimNodeCamView *w);
void seraphim_update_egg(void *obj, SeraphimWork *w);
int seraphim_next_state(void *obj, TypeEnemyWork *w, int n, int state);
void seraphim_record_wing_nodes(void);
void seraphim_wing_nodes_free(void);
void seraphim_update_missiles(void *obj, SeraphimWork *w);
void seraphim_read_wing_nodes(void *obj, SeraphimWork *w);
void seraphim_check_wing_nodes(void *obj, SeraphimWork *w);
void seraphim_init(void *obj, SeraphimWork *w, void *prm, int *k);
void seraphim_release(void *obj, SeraphimWork *w);
void seraphim_launch_missiles(void *obj, SeraphimWork *w);
void seraphim_state_approach(void *obj, SeraphimWork *w);
void seraphim_state_break(void *obj, SeraphimWork *w);
void seraphim_state_stop(void *obj, SeraphimWork *w);
void seraphim_state_egg_break(void *obj, SeraphimWork *w);
void seraphim_state_missile_appear(void *obj, SeraphimWork *w);
void seraphim_state_open_wait(void *obj, SeraphimWork *w);
void seraphim_state_open(void *obj, SeraphimWork *w);
void seraphim_state_open_stop(void *obj, SeraphimWork *w);
void seraphim_state_shot(void *obj, SeraphimWork *w);
void seraphim_state_close_wait(void *obj, SeraphimWork *w);
void seraphim_state_close(void *obj, SeraphimWork *w);
void seraphim_update(void *obj, SeraphimWork *w);
void seraphim_exec_appear(void *obj, void *w);
void seraphim_exec_normal(void *obj, SeraphimWork *w);
void seraphim_exec_leave(void *obj, void *w);
void seraphim_break_wing_part(void *obj, SeraphimWork *w, LockObj *lock, SeraphimNode *n, int half, ModelPart *mdl_a, ModelPart *mdl_b, ModelPart *wire, int type_a, int type_b, int pose);
void seraphim_exec_destroyed(void *obj, SeraphimWork *w);
void seraphim_pre_exec(void);
void seraphim_exec(void *obj, TypeEnemyWork *w);
void seraphim_spawn_debris(void *obj, Vec3 *pos, LockObj *lock);
void seraphim_drop_wing_parts(void *obj, SeraphimWork *w, int idx, SeraphimNode *n, ModelPart *mdl_a, ModelPart *mdl_b, int type_a, int type_b);
void seraphim_wing_hit_check(void *obj, SeraphimWork *w, SeraphimPartRec *rec, SeraphimNode *n, ModelPart *mdl_a, ModelPart *mdl_b, int type_a, int type_b);
int seraphim_core_hit_check(void *obj, SeraphimWork *w, Vec3 **hit_pos);
void seraphim_post_exec(void *obj, SeraphimWork *w);
void seraphim_disp_shell(void *obj, SeraphimWork *w);
void seraphim_disp_body(void *obj, SeraphimWork *w);
void seraphim_disp(void *obj, void *w);
int seraphim_create(void *obj, ShotArg *place);
void seraphim_rotate_zyx(int *ang);
void seraphim_rotate_yxz(int *ang);

#endif /* REZ_STG05_SERAPHIM_H */
