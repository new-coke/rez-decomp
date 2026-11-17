/* Immediate sprite and strip batches. */

#ifndef REZ_UL_GX_PRIM_H
#define REZ_UL_GX_PRIM_H

struct VifBuf;

int ulGxBeginVertexPkt(struct VifBuf *buf, int n, unsigned int **tag, unsigned int **xyz, unsigned int **col,
                  unsigned int **st);
void ulGxBeginSprite(int flag);
void ulGxAddSprites(unsigned int *verts, int n);
void ulGxEndSprite(void);
void ulGxBeginSpriteGif(int flag);
void ulGxAddSpritesGif(void *list, int n);
void ulGxEndSpriteGif(void);
void ulGxBeginStrip(int flag);
void ulGxAddStrip(unsigned int *verts, int n);
void ulGxEndStrip(void);
void ulGxBeginTexStrip(int flag);
void ulGxAddTexStrip(unsigned int *verts, int n);
void ulGxEndTexStrip(void);
void ulGxBeginTexGouraudGif(int flag);
void ulGxAddTexGouraudGif(char *verts, int n);
void ulGxEndTexGouraudGif(void);

#endif /* REZ_UL_GX_PRIM_H */
