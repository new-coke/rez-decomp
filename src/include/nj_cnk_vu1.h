/* VU1 chunk-model renderer, materials, light intensity. */

#ifndef REZ_NJ_CNK_VU1_H
#define REZ_NJ_CNK_VU1_H
#include "types_nj.h"

struct NjSysView;
struct NjCnkModel;
struct NjObject;
struct PsVifPkt;
struct NjVtxSrc;
struct NjCnkIdx;
struct NjCnkCtx;

extern unsigned long ulGxAlphaTable[];

void nj_set_light_intensity(float light, float ambient);
void nj_cnk_vu1_set_material(struct NjSysView *sys, int flags, int type, unsigned char *src);
void nj_cnk_strip_state(struct NjSysView *sys, int type, int flags, int vtype);
int nj_cnk_vu1pkt_draw_clip(struct NjCnkModel *model);
int nj_cnk_vu1pkt_draw_noclip(struct NjCnkModel *model);
int nj_cnk_vu1_draw_raw(struct NjCnkModel *model);
int nj_cnk_vu1_vlist_clip(int *p);
int nj_cnk_vu1_vlist_noclip(int *p);
int nj_cnk_vu1_draw_clip(struct NjCnkModel *model);
int nj_cnk_vu1_draw_noclip(struct NjCnkModel *model);
int nj_cnk_vu1_draw(struct NjCnkModel *model);
void nj_object_draw_vu1(struct NjObject *obj);

extern void psVifPktAddUnpack(void *dl, unsigned int code, void *data, int n);
/* Set by nj_cnk_set_override_flags. */
extern int nj_cnk_override_flags;

#endif /* REZ_NJ_CNK_VU1_H */
