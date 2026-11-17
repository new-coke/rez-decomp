/* Ground cut for models rising out of the floor. */

#ifndef REZ_COMMON_PLANE_CUT_H
#define REZ_COMMON_PLANE_CUT_H

#include "types_common.h"
#include "types_overlays.h"

void plane_cut_mesh(int *pkt, Vec3 *org, Vec3 *nrm);
void plane_cut_mesh_saved(int *pkt, int *save, Vec3 *org, Vec3 *nrm);
void plane_cut_tree(YNode *n, YSave *save, unsigned int mask, Vec3 *org, Vec3 *nrm, float nx, float ny, float nz, float pd);
void plane_cut_draw_part(ModelPart *part, YSave *save, unsigned int mask, Vec3 *pos, int *ang, Vec3 *scale, int rz, Vec3 *obj_pos, int *obj_ang, int up, float ground);
void plane_cut_vertices(int i, int n);
void plane_cut_draw_tree(YNode *n);
void plane_cut_draw_part_flat(ModelPart *part, Vec3 *pos, int *ang, Vec3 *scale, int rz, int up, float ground);
void plane_cut_rotate_yxz(int *ang);

#endif /* REZ_COMMON_PLANE_CUT_H */
