/* Eden boss final stage. */

#ifndef REZ_B_EDEN_EDEN_MODEL_H
#define REZ_B_EDEN_EDEN_MODEL_H

#include "b_eden.h"
#include "types_common.h"

struct EdenModelWork;
struct EdenModelManWork;
struct EdenMapWork;

void eden_model_vars_add(void);
void eden_model_vars_remove(void);
int eden_model_node_count(WireObj *node);
void eden_model_orb_init(struct EdenModelWork *w, EdenOrb *item, WireObj *obj, WireObj *sub_obj, int src, int *ang, float t);
int eden_model_orb_add(struct EdenModelWork *w);
void eden_model_orbs_free(struct EdenModelWork *w);
int eden_model_orb_update(void *w, EdenOrb *orb, Vec3 *base, int *ang);
void eden_model_orbs_update(struct EdenModelWork *w);
void eden_model_orbs_draw(struct EdenModelWork *w);
void eden_model_orbs_draw_fade(struct EdenModelWork *w, float alpha);
void eden_model_se_play(struct EdenModelWork *w);
void eden_model_exec(struct EdenModelWork *w);
void eden_model_screen_offset(Vec3 *in, Vec3 *out, float dx, float dy);
void eden_model_draw_mirror(ModelPart *wire, Vec3 *pos, Vec3 *ctr, float alpha, float dist, float s);
void eden_model_disp(struct EdenModelWork *w);
void eden_model_release(struct EdenModelWork *w);
struct EdenModelWork *eden_model_create(void *parent, int arg, int mdl, int mdl_b, ModelPart *wire, Vec3 *pos, int idx);
int eden_model_get_state(struct EdenModelWork *w);
void eden_model_man_seq_step(struct EdenModelManWork *w);
void eden_model_man_exec(struct EdenModelManWork *man);
void eden_model_man_part_draw(struct EdenModelManWork *w, int i, float t, int mode);
void eden_model_man_disp(struct EdenModelManWork *w);
void eden_model_man_parts_init(struct EdenModelManWork *w);
void eden_model_man_parts_free(struct EdenModelManWork *w);
void eden_model_man_release(struct EdenModelManWork *w);
struct EdenModelManWork *eden_model_man_create(void *parent, struct EdenMapWork *map, int arg);
int eden_model_man_is_end(struct EdenModelManWork *w);
int eden_model_man_get_part(struct EdenModelManWork *w);
void eden_model_man_set_visible(struct EdenModelManWork *w, int on);
void eden_model_rotate_yxz(int *ang);

#endif /* REZ_B_EDEN_EDEN_MODEL_H */
