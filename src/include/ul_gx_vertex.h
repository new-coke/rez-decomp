/* Vertex stage: transforms, outcodes, vertex lists, strips. */

#ifndef REZ_UL_GX_VERTEX_H
#define REZ_UL_GX_VERTEX_H

struct UlGxV;
struct UlGxCtx;
struct UlGxClip;
struct UlGxVtx;
struct UlGxStrip;

void ulGxMakeBoxCorners(float *box, float *out);
int ulGxBoxOutcode(float *pt, float *lo, float *hi);
float ulGxTransformDivide(void *in, float *out);
int ulGxVlistPacketClip(struct UlGxCtx *gx, int *pkt);
void ulGxVlistPacketNoClip(struct UlGxCtx *gx, int *pkt);
void ulGxEmitStrip(struct UlGxStrip *gx, short *idx, int n, int vbuf);
void ulGxEmitTexStripUV(struct UlGxStrip *gx, short *idx, int n, int vbuf);
void ulGxEmitTexStrip(struct UlGxStrip *gx, short *idx, int n, int vbuf);

#endif /* REZ_UL_GX_VERTEX_H */
