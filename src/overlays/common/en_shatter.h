/* stg05 break effects and draw sorting. */

#ifndef REZ_COMMON_EN_SHATTER_H
#define REZ_COMMON_EN_SHATTER_H

#include "types_overlays.h"

struct EnSortEntry;

void en_shatter_vars_register(void);
void en_shatter_vars_unregister(void);
Shatter *en_shatter_spawn(void *obj, SpawnDesc *desc, int kind);
void en_sort_vars_register(void);
void en_sort_vars_unregister(void);
void en_sort_insertion_sort(struct EnSortEntry *list, int n);
void en_sort_init(EnSortWork *w);
void en_sort_release(EnSortWork *w);
void en_sort_exec(EnSortWork *w);
void en_sort_disp(EnSortWork *w);
void *en_sort_create(void *parent);
int en_sort_add(EnArmorDispArg *ent);
void en_sort_remove(int idx);

#endif /* REZ_COMMON_EN_SHATTER_H */
