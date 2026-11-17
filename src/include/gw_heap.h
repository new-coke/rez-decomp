/* Debug GUI tagged heap, heap window. */

#ifndef REZ_GW_HEAP_H
#define REZ_GW_HEAP_H

struct GwWin;
struct GwMem;

struct GwWin *gw_heap_window_create(int x, int y);
void *gw_dc_malloc(unsigned size, char *file, int line);
void *gw_dc_calloc(int n, int size, char *file, int line);
void gw_dc_free(void *ptr, char *file, int line);
void gw_heap_init(struct GwMem *base, int size);

#endif /* REZ_GW_HEAP_H */
