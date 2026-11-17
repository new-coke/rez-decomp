/* Uranus boss model form. */

#ifndef REZ_B_URANUS_URANUS_MODEL_H
#define REZ_B_URANUS_URANUS_MODEL_H

#include "b_uranus.h"
#include "types_common.h"

struct UranusModelWork;
struct UranusModelMot;
struct UranusMapWork;

void uranus_model_vars_add(void);
void uranus_model_vars_remove(void);
void uranus_model_nodes_build_r(void *node, UranusNode *dst, int *ptr_a, int *ptr_b);
UranusNode *uranus_model_nodes_build(void *model, int data, int ptr_a, int ptr_b);
int uranus_model_find_motion(struct UranusModelWork *w, int id);
void uranus_model_get_motions(struct UranusModelWork *w, struct UranusModelMot **cur, struct UranusModelMot **next);
int uranus_model_count_live(struct UranusModelWork *w, int first, int cnt);
int uranus_model_pick_attack(struct UranusModelWork *w);
void uranus_model_pick_next(struct UranusModelWork *w);
void uranus_model_attack_cue(struct UranusModelWork *w);
void uranus_model_motion_end(struct UranusModelWork *w);
void uranus_model_pose_r(YNode *n, float **root, float **node_pos, float **vtx_pos, int *cnt);
void uranus_model_root_pos_r(YNode *n, float *root);
void uranus_model_ground_y(struct UranusMapWork *map, Vec3 *pos, Vec3 *ofs, YNode *n, void *mot, float *out, float t);
void uranus_model_blend_pose(struct UranusModelWork *w, struct UranusModelMot *mot_a, struct UranusModelMot *mot_b, float t);
void uranus_model_pose(struct UranusModelWork *w, struct UranusModelMot *ent);
void uranus_model_set_parts_on(struct UranusModelWork *w, int on, int first, int cnt);
void uranus_model_attack_step(struct UranusModelWork *w);
void uranus_model_exec(struct UranusModelWork *w);
void uranus_model_release(struct UranusModelWork *w);
void uranus_model_disp(struct UranusModelWork *w);
void uranus_model_restart_cb(struct UranusModelWork *w);
struct UranusModelWork *uranus_model_create(void *parent, UranusPartWork *pm, Vec3 **pts, UranusNode **out, YNode *model, int mode, int slot);

#endif /* REZ_B_URANUS_URANUS_MODEL_H */
