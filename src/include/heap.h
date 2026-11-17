/* Game heaps: allocation, delayed free, statistics. */

#ifndef REZ_HEAP_H
#define REZ_HEAP_H

struct HeapBase;
struct HeapView;

extern char heap_base_end[];

void heap_system_init(void);
void heap_pool_tick(void);
int heap_pool_add(void *obj, int count);
struct HeapBase *heap_create(struct HeapBase *heap, int size, const char *name);
void heap_destroy(struct HeapBase *heap);
int heap_set_current(int heap);
int heap_get_default(void);
int heap_next(int *p);
int heap_count(void);
void *heap_block_tag(void *mem, const char *src, int line);
void heap_bzero(void *buf, unsigned int n);
void *game_malloc(int size);
void *game_malloc_back(int size);
void *game_calloc(int size);
void *game_calloc_back(int size);
void heap_free(void *mem, int ret);
unsigned int heap_free_size(void);
unsigned int heap_max_free(void);
int heap_free_blocks(void);
int heap_alloc_count(void);
void *heap_alloc_front(struct HeapBase *heap, int size, int ret);
void *heap_alloc_back(struct HeapBase *heap, int size, int ret);
void *heap_calloc_front(void *heap, int size);
void *heap_calloc_back(void *heap, int size);
int heap_alloc_count_of(int *p);
void heap_free_error(int ret, int ptr);
int heap_find_in_range(unsigned int lo, unsigned int hi);
int heap_list_names(void);
int heap_map_dump(int kind, char **argv);
void heap_map_dump_all(void);
int heap_check(struct HeapView *heap);
int heap_check_all(void);
int heap_check_default(void);
int heap_is_block(unsigned int addr);
void heap_draw_bars(float x, float y, float z);
void heap_debug_draw(void *ctx);
void heap_menu_toggle(void);
void heap_menu_register(void);

#endif /* REZ_HEAP_H */
