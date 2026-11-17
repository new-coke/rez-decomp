/* Ending: staff roll. */

#ifndef REZ_ENDING_END_STAFF_ROLL_H
#define REZ_ENDING_END_STAFF_ROLL_H

#include "types_overlays.h"
#include "types_common.h"

struct RollStr;
struct RollCmd;
struct RollTex;
struct Roll;
struct RollButterfly;
struct Cut;

void end_staff_roll_vars_register(void);
void end_staff_roll_vars_unregister(void);
void end_staff_roll_load(void);
void end_staff_roll_unload(void);
void str_init(struct RollStr *w, struct RollCmd *rcmd);
void str_release(void);
void str_exec(struct RollStr *w);
void str_disp(struct RollStr *w);
void *str_create(void *parent, struct RollCmd *rcmd);
void tex_init(struct RollTex *w, struct RollCmd *rcmd);
void tex_release(void);
void tex_exec(struct RollTex *w);
void tex_disp(struct RollTex *w);
void *tex_create(void *parent, struct RollCmd *rcmd);
void end_staff_roll_init(struct Roll *w);
void end_staff_roll_release(void);
void end_staff_roll_exec_end(struct Roll *w);
void end_staff_roll_exec(struct Roll *w);
void end_staff_roll_disp(void);
void *end_staff_roll_create(void *parent);
void end_staff_roll_butterfly_vars_register(void);
void end_staff_roll_butterfly_vars_unregister(void);
void end_staff_roll_butterfly_vars_init(void);
void end_staff_roll_butterfly_vars_release(void);
void end_staff_roll_butterfly_path_eval(ModelPart *path, Vec3 *pos, int *ang, float t);
void end_staff_roll_butterfly_mirror(Vec3 *spos, int *sang, Vec3 *dpos, int *dang);
void end_staff_roll_butterfly_mirror_body(struct RollButterfly *w);
void end_staff_roll_butterfly_init(struct RollButterfly *w);
void end_staff_roll_butterfly_release(struct RollButterfly *w);
void end_staff_roll_butterfly_exec_children(struct RollButterfly *w);
void end_staff_roll_butterfly_exec_leave(struct RollButterfly *w);
void end_staff_roll_butterfly_exec_rings(struct RollButterfly *w);
void end_staff_roll_butterfly_exec_sink(struct RollButterfly *w);
void end_staff_roll_butterfly_exec(struct RollButterfly *w);
int end_staff_roll_butterfly_draw_mirrored(void *w, Vec3 *pos, int *ang, ModelPart *mdl, Vec3 *mpos, int *mang, ModelPart *mmdl, int state, float t);
void end_staff_roll_butterfly_disp(struct RollButterfly *w);
void *end_staff_roll_butterfly_create(void *parent);
void cut_init(struct Cut *w, int *prm);
void cut_release(struct Cut *w);
void cut_exec(struct Cut *w);
void cut_disp(struct Cut *w);
void *cut_create(struct Roll *rw, int id, int *done);
void cut_rotate_yxz(int *ang);

#endif /* REZ_ENDING_END_STAFF_ROLL_H */
