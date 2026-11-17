/* Plane cut, Area 5. */

#ifndef REZ_STG05_PLANE_CUT_H
#define REZ_STG05_PLANE_CUT_H

#include "types_common.h"
#include "types_overlays.h"

void plane_cut_mesh(int *pkt, Vec3 *org, Vec3 *nrm);
void plane_cut_mesh_saved(int *pkt, int *save, Vec3 *org, Vec3 *nrm);
void plane_cut_tree(YNode *n, YSave *save, unsigned int mask, Vec3 *org, Vec3 *nrm, float nx, float ny, float nz, float pd);
void plane_cut_draw_part(ModelPart *part, YSave *save, unsigned int mask, Vec3 *pos, int *ang, float dy);
void plane_cut_vertices(int i, int n);
void plane_cut_draw_tree(YNode *n);
void plane_cut_draw_part_flat(ModelPart *part, Vec3 *pos, int *ang, float height);
void plane_cut_rotate_yxz(int *ang);

#endif /* REZ_STG05_PLANE_CUT_H */
