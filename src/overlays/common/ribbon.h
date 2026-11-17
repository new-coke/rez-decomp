/* Ribbon trails. */

#ifndef REZ_COMMON_RIBBON_H
#define REZ_COMMON_RIBBON_H

#include "types_overlays.h"
#include "types_common.h"

void ribbon_init(Ribbon *rib, void *buf, int frames, int seg, int step);
RibPt *ribbon_collect_nodes(RibPt *p, unsigned int *node, unsigned int mask, int count);
void ribbon_age(Ribbon *rib);
void ribbon_add_model(Ribbon *rib, Vec3 *pos, int *ang, int mask, int root, int clip, float time);
void ribbon_add_points(Ribbon *rib, Vec3 *pts, int *ang, float wid);
int ribbon_edge_vertices(int i, int last, float *p, float dx, float dy, float alpha, float ts, float wid);
void ribbon_draw_segment(RibPt *row, int n, int head, int count, int step, int *cur, int *nxt, Vec3 *prev, Vec3 *v, float *dis_a, float *dis_b, float alpha, float wid, float inv);
void ribbon_draw_row(RibPt *row, int n, int head, int count, int step, float alpha, float wid, float inv);
void ribbon_draw(Ribbon *rib, int tex, float fade, float width);
void ribbon_rotate_yxz(int *ang);

#endif /* REZ_COMMON_RIBBON_H */
