/* GS library: display mode, display and draw environments, double buffers, vsync. */

#ifndef REZ_LIBGRAPH_H
#define REZ_LIBGRAPH_H

struct GsGParam;
struct GsDBuffA;
struct GsDispEnv;
struct GsDrawEnv1A;
struct GsClearA;

void sceGsResetGraph(short mode, short inter, short omode, short ffmd);
struct GsGParam *sceGsGetGParam(void);
int sceGsPutDrawEnv(unsigned long *giftag);
void sceGsSetDefDBuff(struct GsDBuffA *db, short psm, short width, short height, short ztest, short zpsm, short clear);
int sceGsSyncV(void);
int sceGsSyncVCallback(int (*fn)(int));
int sceGsPutIMR(int imr);
int sceGsGetIMR(void);
int isceGsPutIMR(int imr);
void sceGsSetDefDispEnv(struct GsDispEnv *disp, short psm, short width, short height, short dx, short dy);
void sceGsPutDispEnv(struct GsDispEnv *disp);
short libgraph_fb_pages(short psm, short width, short height);
int sceGsSetDefDrawEnv(struct GsDrawEnv1A *draw, short psm, short width, short height, short ztest, short zpsm);
int sceGsSetDefClear(struct GsClearA *cp, short ztest, short x, short y, short width, short height,
                 unsigned char r, unsigned char g, unsigned char b, unsigned char a, unsigned int z);
void sceGsResetPath(void);
int sceGsSyncPath(int, int);

#endif /* REZ_LIBGRAPH_H */
