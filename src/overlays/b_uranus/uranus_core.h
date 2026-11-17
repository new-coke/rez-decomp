/* Uranus boss core. */

#ifndef REZ_B_URANUS_URANUS_CORE_H
#define REZ_B_URANUS_URANUS_CORE_H

#include "b_uranus.h"
#include "types_common.h"
#include "types_overlays.h"

struct UranusPitWork;
struct UranusDcubeWork;
struct UranusCmWork;

void uranus_core_vars_add(void);
void uranus_core_vars_remove(void);
void uranus_core_beat_tick(void);
void uranus_pit_exec(struct UranusPitWork *w);
void uranus_pit_disp(struct UranusPitWork *w);
void uranus_pit_release(struct UranusPitWork *w);
struct UranusPitWork *uranus_pit_create(void *parent, UranusPartWork *pm, Vec3 **pts, UranusNode **out, Vec3 *pos, UranusNode *shape, ModelPart **parts);
void uranus_pit_get_pos(struct UranusPitWork *w, Vec3 *pos, Vec3 *vel, int *ang);
void uranus_dcube_exec(struct UranusDcubeWork *w);
void uranus_dcube_disp(struct UranusDcubeWork *w);
void uranus_dcube_release(struct UranusDcubeWork *w);
struct UranusDcubeWork *uranus_dcube_create(void *parent, UranusPartWork *pm, Vec3 *pos, ModelPart **parts, int form, int wait);
void uranus_cm_exec(struct UranusCmWork *w);
void uranus_cm_disp(struct UranusCmWork *w);
struct UranusCmWork *uranus_cm_create(void *parent, UranusPartWork *pm, Vec3 *pos, int *ang, ModelPart **parts);
CmObjectBlurWork *uranus_cm_blur_create(void *parent, Vec3 *pos, int *ang, ModelPart **parts);
void uranus_core_rotate_yxz(int *ang);

#endif /* REZ_B_URANUS_URANUS_CORE_H */
