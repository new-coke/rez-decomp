/* GS start-up, flip, current display list, render state. */

#ifndef REZ_PS_GS_H
#define REZ_PS_GS_H
#include "types_nj.h"

struct NjDmaTag;
struct NjGsVram;
struct NjGsDrawEnv;
struct NjGxPkt;
struct NjGsState;
struct NjGsInit;

struct NjDmaTag *psGsAddCallTag(unsigned int addr);
void *psGsGetMainDisplayList(void);
void psGsCloseRefPkt(void);
void *psGsGetRefPkt(void);
void *psGsOpenRefPkt(void);
void *psGsGetDisplayList(void);
void *psGsSetDisplayList(void *dl);
void psGsSetAlphaTest(int ate, int atst, int aref, int afail);
void psGsSetDepthTest(int zte, int ztst);
void psGsSetZMask(int zmsk);
void psGsSetTexLod(int lcm, int lod_l, int lod_k);
void psGsSetTexFunc(int tfx, int tcc);
void psGsSetTexFilter(int mmag, int mmin);
void psGsSetClamp(int wms, int wmt, int minu, int maxu, int minv, int maxv);
void psGsWriteReg(int reg, unsigned long val);
void psGsRestoreVramHeap(void);
void psGsSetClearColor(int r, int g, int b);
void psGsGetVramLayout(struct NjGsVram *out);
void psGsGetScreenOrigin(float *x, float *y);
unsigned long psGsGetFrameReg(int i);
void sceGsSwapDBuff(void);
void psGsSyncPath(void);
void psGsVSyncField(void);
void psGsStub(void);
void psGsInitMainDisplayList(void);
struct NjGsState *psGsInit(struct NjGsInit *cfg);

extern NjGsState psSurfTexShadow;

#endif /* REZ_PS_GS_H */
