/* Uranus boss wall form. */

#ifndef REZ_B_URANUS_URANUS_WALL_H
#define REZ_B_URANUS_URANUS_WALL_H

#include "b_uranus.h"
#include "types_common.h"

struct UranusWallWork;

void uranus_wall_vars_add(void);
void uranus_wall_vars_remove(void);
void uranus_wall_ring_place(UranusNode **link, UranusNode **node, Vec3 *ctr, int start_ang, int cnt, float rad);
void uranus_wall_set_laser(struct UranusWallWork *w, int on);
void uranus_wall_exec(struct UranusWallWork *w);
void uranus_wall_nodes_build(struct UranusWallWork *w, int data, char *link_pts, char *pt_pts);
void uranus_wall_release(struct UranusWallWork *w);
void uranus_wall_switch_cb(struct UranusWallWork *w);
struct UranusWallWork *uranus_wall_create(void *parent, UranusPartWork *pm, Vec3 **pts, UranusNode **out, int slot, ModelPart **parts);

#endif /* REZ_B_URANUS_URANUS_WALL_H */
