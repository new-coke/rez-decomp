/* Image and texture uploads to GS memory. */

#ifndef REZ_PS_IMAGE_H
#define REZ_PS_IMAGE_H

struct NjGxPkt;
struct NjGsTex;

void psImageDlAddTransferRegs(unsigned long bitbltbuf, unsigned long trxpos, unsigned long trxreg, unsigned long trxdir);
void psImagePktAddTexture(struct NjGsTex *tex, int src);
void psImageDlAddImage(struct NjGxPkt *gx_pkt, int width, int height, int psm, int dbp, int size, void *src);
void psImageDlAddTexture(struct NjGsTex *tex, int src);

#endif /* REZ_PS_IMAGE_H */
