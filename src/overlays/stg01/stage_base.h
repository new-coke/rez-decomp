/* Area 1 stage support. */

#ifndef REZ_STG01_STAGE_BASE_H
#define REZ_STG01_STAGE_BASE_H

#include "types_overlays.h"

int stage_rating(void);
void stage_data_free(void);
void boss_boot_exec(int *w);
int *boss_boot_create(void *parent);
void listeners_add(void);
void listeners_remove(void);
void listener_stars_on(void);
void listener_next_map(void);
void listener_mute(int *mute);
void listener_lock_hit(HitEv *p);
void listener_area_change(int *ev);
void filter_vars_add(void);
void filter_vars_remove(void);
void filter_reset(void);
void filter_set_area(unsigned int area);
void filter_fade_last(void);
int stars_on(void);
int stars_off(void);
void start_map(void *menu);
void start_map_shape(void *menu);
void map_init(int *w);
void map_release(void);
void map_exec(void);
void map_disp(void);
void *map_create(void *w);
void map_menu_create(void *parent);
void map_menu_next(void);
void map_menu_set_page(int page);
void map_menu_set_value(int scene);
int map_get_current(void);
void *map_menu_get(void);
int *map_get_work(void);

#endif /* REZ_STG01_STAGE_BASE_H */
