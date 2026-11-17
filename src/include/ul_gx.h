/* Graphics core: render state, matrices, frustum, fog, textures, clipping. */

#ifndef REZ_UL_GX_H
#define REZ_UL_GX_H
#include "types_nj.h"

struct UlGxBig;
struct UlClipVtx;
struct ClipCtx;
struct UlGxClipBuf;
struct UlGxDrawable;
struct UlGxFrustum;
struct UlGxTexList;
struct GxDesc;
struct GxInitDesc;

extern char ulGxLockBlock1[];

void ulGxClipDrawTexTriangle(struct UlGxBig *gx, struct UlClipVtx *vtx0, struct UlClipVtx *vtx1, struct UlClipVtx *vtx2, int cull);
void ulGxClipDrawTriangle(struct UlGxBig *gx, struct UlClipVtx *vtx0, struct UlClipVtx *vtx1, struct UlClipVtx *vtx2, int cull);
int ulGxClipTriangle(struct UlGxClipBuf *cb, struct UlClipVtx *vtx0, struct UlClipVtx *vtx1, struct UlClipVtx *vtx2);
void ulGxDlReadAttr(struct UlGxBig *gx, unsigned char fmt, unsigned char *p);
void ulGxDlSetAlphaMode(struct UlGxBig *gx, unsigned int mode);
void ulGxSetRenderFlags(struct UlGxBig *gx, unsigned int flags);
void ulGxDlSetTexture(struct UlGxBig *gx, short mode, short sel);
void ulGxDrawShape(struct UlGxDrawable *shape, int flags);
void ulGxSetFogRange(float z0, float z1, float fog0, float fog1);
void ulGxEnableFog(int on);
void ulGxSetFogColor(float r, float g, float b);
int ulGxGetDrawFlags(void);
void ulGxSetMatrix(int sel, void *mtx);
void ulGxSetDepthRange(float znear, float zfar);
void ulGxGetPixelScale(float *sx, float *sy);
void ulGxSetAspect(float x, float y);
void ulGxSetHFov(float fov);
void ulGxMakeProjMatrix(float *mtx, float dist, float xscale, float yscale, float cx, float cy);
void ulGxGetFrustum(struct UlGxFrustum *dst);
void ulGxSetFrustum(struct UlGxFrustum *fr);
void ulGxSetClearColor(unsigned int col);
void ulGxSetAlphaBlend(int src_mode, int dst_mode);
void ulGxSetTexFilter(int mode);
void ulGxSetTexFunc(int tfx);
void ulGxSetTexClamp(int wms, int wmt);
void ulGxSelectTexture(struct UlGxTexList *list, int idx);
int ulGxGetTextureList(void);
struct UlGxTexList *ulGxSetTextureList(struct UlGxTexList *list);
void ulGxSetTexture(void *tex);
void ulGxLock(int kind);
void ulGxUnlock(int kind);
void ulGxResetCounters(void);
void ulGxSyncPath(void);
void ulGxSetVertexBuffer_thunk(void * buf, int size);
unsigned int ulGxGetWorkSize(struct GxDesc *desc);
void ulGxInit(struct GxInitDesc *desc);

extern int nj_cnk_strip_and_mask;
extern int nj_cnk_strip_or_bits;

#endif /* REZ_UL_GX_H */
