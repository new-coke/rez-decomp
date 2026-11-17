/* Uranus boss orbit forms. */

#ifndef REZ_B_URANUS_URANUS_ORBIT_H
#define REZ_B_URANUS_URANUS_ORBIT_H

#include "types_common.h"
#include "b_uranus.h"

struct UOrWork;
struct UOr2Work;

void uranus_orbit_vars_add(void);
void uranus_orbit_vars_remove(void);
void uranus_orbit_path_lerp(Vec3 *pts, Vec3 *out, float t);
void u_or_exec(struct UOrWork *w);
void u_or_release(struct UOrWork *w);
void u_or_nodes_build(struct UOrWork *w, int data, char *ring_pts, char *pt_pts);
struct UOrWork *u_or_create(void *parent, UranusPartWork *pm, Vec3 **pts, UranusNode **out, int slot);
void u_or2_rings_init(struct UOr2Work *w, UranusNode *n);
void u_or2_orbit_step(struct UOr2Work *w, UranusNode *node, Vec3 *out);
void u_or2_pulse(struct UOr2Work *w);
void u_or2_exec(struct UOr2Work *w);
void u_or2_release(struct UOr2Work *w);
void u_or2_nodes_build(struct UOr2Work *w, int data, char *ring_pts, char *pt_pts);
struct UOr2Work *u_or2_create(void *parent, UranusPartWork *pm, Vec3 **pts, UranusNode **out, int slot);

#endif /* REZ_B_URANUS_URANUS_ORBIT_H */
