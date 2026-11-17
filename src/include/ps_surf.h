/* VRAM texture surfaces and their heap. */

#ifndef REZ_PS_SURF_H
#define REZ_PS_SURF_H

struct PsTexRec;
struct PsSurf;
struct PsVramHeap;

void psSurfBind(struct PsTexRec *rec);
void psSurfFree(struct PsSurf *surf);
struct PsSurf *psSurfAlloc(int *req, struct PsSurf *surf);
struct PsVramHeap *psSurfGetVramHeap(void);
struct PsVramHeap *psSurfSetVramHeap(struct PsVramHeap *heap);
void psSurfClearVramHeap(void * heap);
int psSurfInitVramHeap(int buf, int bufsize, int base, int size);

#endif /* REZ_PS_SURF_H */
