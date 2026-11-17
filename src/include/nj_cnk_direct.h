/* Pre-built, direct and wireframe chunk-model renderers; screen scale; texture by number. */

#ifndef REZ_NJ_CNK_DIRECT_H
#define REZ_NJ_CNK_DIRECT_H
#include "types_nj.h"

struct NjCnkModel;
struct NjObject;
struct PsVifPkt;
struct NjSysView;
struct NjCnkIdx;

void nj_set_screen_scale(float x, float y);
void njSetTextureNum(int idx);
int nj_cnk_packet_draw_culled(struct NjCnkModel *model);
int nj_cnk_packet_draw(struct NjCnkModel *model);
int nj_cnk_direct_draw_noclip(struct NjCnkModel *model);
int nj_cnk_direct_draw_clip(struct NjCnkModel *model);
int nj_cnk_wire_vlist(int *p, char *vbuf);

/* Current direct chunk vertex list; stride in bytes. */
extern int *nj_cnk_direct_vtx;
extern int nj_cnk_direct_stride;

#endif /* REZ_NJ_CNK_DIRECT_H */
