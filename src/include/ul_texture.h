/* Texture manager: records, VRAM surfaces, uploads, sections. */

#ifndef REZ_UL_TEXTURE_H
#define REZ_UL_TEXTURE_H

struct TexList;
struct UlTexture;
struct TexList;
struct UlTexInfo;
struct UlTex;
struct UlSlot;
struct UlSecHdr;
struct UlBlk;
struct Rel;
struct TexSurf;
struct PicHdr;
struct UlTexBlk;
struct UlTextureTexSurf;

int ulTexListGet(struct TexList *list, int idx);
struct UlTexture *ulTextureFindById(int id);
void ulTextureSetId(struct UlTexture *tex, int id);
void ulTexListInit(int *list, int ent, int count);
void ulTexListReload(struct TexList *list);
int ulTextureGetSize(struct UlTexture *tex);
int ulTextureGetAddr(struct UlTexture *tex);
int ulTextureGetWidth(struct UlTexture *tex);
int ulTextureGetHeight(struct UlTexture *tex);
void ulTextureStub(void);
void ulTextureSetName(struct UlTexture *tex, char *name);
void ulTextureSetImage(struct UlTexture *tex, int image);
void ulTextureClearVramHeap(void * heap);
int ulTextureInitVramHeap(int work, int size, int vram, int vsize, int n);
char *ulTextureDescImage(char *desc);
void ulTextureFreeSlot(struct UlTexture *tex);
struct UlTexture *ulTextureAllocSlot(void);
void ulTextureReload(struct UlTexture *tex);
void ulTextureLoadImageDirect(void *tex, void *image);
void ulTextureLoadImage(void *tex, void *image);
int ulTextureCreateFromHeader(char *hdr);
int ulTextureCreateFromDesc(char *desc);
struct UlTexture *ulTextureCreateLocked(char *desc);
struct UlTexture *ulTextureCreate(char *desc);
struct UlTexture *ulTextureCreateSurface(int *req);
int ulTexListFree(struct TexList *list);
void ulTextureFree(struct UlTexture *tex);
struct UlTexture *ulTextureCreateSurfaceLocked(int *req);
void ulTextureFreeLocked(struct UlTexture *tex);
struct UlTexture *ulTextureFindUsedById(int id);
struct UlTexture *ulTextureGetUsed(int i);
int ulTextureGetRecords(void);
int ulTextureGetMax(void);
struct UlTexture *ulTextureGet(int i);
int ulTextureGetCount(void);
void ulTextureGetInfo(struct UlTexInfo *info);
void ulTextureDump(void);
void ulTextureRestoreVramHeap(void);
int ulTextureGetVramHeap(int heap);
int ulTextureSetVramHeap(int heap);
void ulTextureClearTable(void);
void ulTextureInitTable(struct UlTex *buf, int n);
void ulTextureCreateSection(struct UlSlot **table, struct UlSecHdr *hdr);
struct UlBlk *ulTextureFindBlock(struct UlSecHdr *hdr, int key);
char *ulTextureBlockImageOrNull(struct Rel *blk);
void ulTextureReloadSectionAbove(struct UlSecHdr *hdr, struct UlSlot **table, int limit);
void ulTextureReloadSection(struct UlSecHdr *hdr, struct UlSlot **table);
void ulTextureReloadBlock(void *dst, void *src);
void ulTextureLoadPicture(struct TexSurf *surf, struct PicHdr *pic, int no);
void ulTextureLoadPictureDirect(struct TexSurf *surf, struct PicHdr *pic, int no);
char *ulTextureBlockImage(struct Rel *blk);
struct UlTex *ulTextureLoadBlock(struct UlTexBlk *blk);
void ulTextureLoadSection(struct UlSecHdr *hdr, struct UlSlot **table);
struct UlTex *ulTextureLoadBlockDirect(struct UlTexBlk *blk);
void ulTextureLoadSectionDirect(struct UlSecHdr *hdr, struct UlSlot **table);
int ulTextureUploadBand(void *dev, unsigned long packed, int y, int x, int width, int height, char *data);
void ulTextureUpload(struct UlTextureTexSurf *surf, int n, int height, char *data);
int ulTextureUploadBandDirect(void *dev, unsigned long packed, int y, int x, int width, int height, unsigned int data);
void ulTextureUploadDirect(struct UlTextureTexSurf *surf, int n, int height, char *data);

#endif /* REZ_UL_TEXTURE_H */
