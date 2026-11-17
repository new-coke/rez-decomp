/* Uranus boss part manager. */

#ifndef REZ_B_URANUS_URANUS_PART_H
#define REZ_B_URANUS_URANUS_PART_H

#include "b_uranus.h"
#include "types_common.h"

struct UranusPbWork;

void uranus_pm_init(UranusPartWork *w, UranusWork *boss);
void uranus_pm_blend_mtx(UranusPartWork *w);
void uranus_pm_col_step(UranusPartWork *w);
void uranus_pm_set_col(UranusPartWork *w, int kind, float width, float secs);
void uranus_pm_hit_reset(UranusPartWork *w);
int uranus_pm_is_end(UranusPartWork *w);
int uranus_pm_fire(UranusPartWork *w, int type);
int uranus_pm_get_sec(UranusPartWork *w);
void uranus_pm_clear_hits(UranusPartWork *w);
int uranus_pm_get_hits(UranusPartWork *w);
void uranus_pm_set_gauge(UranusPartWork *w, int on);
void uranus_pm_to_core(UranusPartWork *w);
int uranus_pm_is_core(UranusPartWork *w);
int uranus_pm_can_core(UranusPartWork *w);
void uranus_pm_pit_reached(UranusPartWork *w, int id);
int uranus_pm_get_models(UranusPartWork *w);
void uranus_pm_break_objs(UranusPartWork *w);
int uranus_pm_sw_request(UranusPartWork *w, int id, int delay);
int uranus_pm_sw_force(UranusPartWork *w, int id, int delay);
void uranus_pm_set_cb(UranusPartWork *w, int owner, void (*cb)(int), int slot);
int uranus_pm_sw_is_live(UranusPartWork *w, int id);
int uranus_pm_sw_is_ready(UranusPartWork *w, int id);
int uranus_pm_sw_get(UranusPartWork *w, int id, int *cnt, Vec3 *pos);
int uranus_pm_slot_create(UranusPartWork *w, int type, int slot, UranusNode **out);
void uranus_pm_set_level(UranusPartWork *w);
void uranus_pm_damage_cb(UranusPartWork *w);
void uranus_pm_damage_req(UranusPartWork *w, int kind, int score);
int uranus_pm_check_kill(UranusPartWork *w);
int uranus_pm_check_damage(UranusPartWork *w, int score);
void uranus_pm_reset(UranusPartWork *w);
void uranus_pm_update(UranusPartWork *w);
void uranus_pm_exec_core(UranusPartWork *w);
void uranus_pm_step(UranusPartWork *w);
void uranus_pm_exec(UranusPartWork *w);
void uranus_pm_release(UranusPartWork *w);
void uranus_pm_disp(UranusPartWork *w);
UranusPartWork *uranus_pm_create(UranusWork *boss);
void uranus_pm_get_centre(UranusPartWork *w, Vec3 *out);
void uranus_pm_obj_kick(UranusPartWork *w, UranusObj *obj);
void uranus_pm_obj_push_apart(UranusPartWork *w, UranusObj *self, Vec3 *pos);
void uranus_pm_beh_home(UranusPartWork *w, UranusObj *obj, UranusBeh *beh);
void uranus_pm_beh_wander(UranusPartWork *w, UranusObj *obj, UranusBeh *beh);
void uranus_pm_beh_drift(UranusPartWork *w, UranusObj *obj, UranusBeh *beh);
void uranus_pm_beh_centre(UranusPartWork *w, UranusObj *obj, UranusBeh *beh);
void uranus_pm_beh_follow(UranusPartWork *w, UranusObj *obj, UranusBeh *beh);
void uranus_pm_beh_avoid(UranusPartWork *w, UranusObj *obj, UranusBeh *beh);
void uranus_pm_beh_wall(UranusPartWork *w, UranusObj *obj, UranusBeh *beh);
void uranus_pm_beh_push(UranusPartWork *w, UranusObj *obj, UranusBeh *beh);
UranusObj *uranus_pm_obj_alloc(UranusPartWork *w);
void uranus_pm_obj_init(UranusObj *obj, UranusPartWork *w, int lvl, Vec3 *pos);
void uranus_pm_push_from_player(Vec3 *pos, Vec3 *vel, float rad, float push);
void uranus_pm_flip_ang(float *ang);
void uranus_pm_objs_attach(UranusPartWork *w, int idx, int lvl, UranusNode *node);
UranusNode *uranus_pm_node_match(UranusNode *tree, UranusNode *ref);
void uranus_pm_objs_bind(UranusPartWork *w, int k, int on, UranusNode *n);
void uranus_pm_objs_bind_tree(UranusObj *obj, int slot, UranusNode *n);
void uranus_pm_obj_split(UranusPartWork *w, UranusObj *obj);
void uranus_pm_obj_split_at(UranusPartWork *w, UranusObj *obj);
void uranus_pm_obj_move(UranusPartWork *w, UranusObj *obj);
void uranus_pm_obj_hit(UranusPartWork *w, UranusObj *obj);
void uranus_pm_obj_exec(UranusPartWork *w, UranusObj *obj);
void uranus_pm_obj_disp(UranusPartWork *w, UranusObj *obj);
void uranus_pm_obj_free(UranusPartWork *w, UranusObj *obj);
void uranus_pm_set_lockable(UranusPartWork *w, int on);
void uranus_pm_set_layout(UranusPartWork *w, UranusPmLayout *wgts, float sec);
int uranus_pm_get_core(UranusPartWork *w, Vec3 **pos, int **ang);
struct UranusMapWork *uranus_pm_get_map(UranusPartWork *w);
Vec3 *uranus_pm_get_obj0_pos(UranusPartWork *w);
void uranus_pm_set_mode(UranusPartWork *w, int mode);
void uranus_pb_exec(struct UranusPbWork *w);
void uranus_pb_disp(struct UranusPbWork *w);
struct UranusPbWork *uranus_pb_create(UranusPartWork *parent, struct UranusMapWork *map, ModelPart *model, Vec3 *pos, int *ang, Vec3 *vel, float rad);
void uranus_pm_objs_scatter(UranusPartWork *w);
void uranus_pm_obj_set_glow(UranusObj *obj, int on);
int uranus_pm_obj_get_hp(UranusObj *obj);
void uranus_pm_hit_circle(void *parent, Vec3 *pos, float rad);
void uranus_part_rotate_yxz(int *ang);

#endif /* REZ_B_URANUS_URANUS_PART_H */
