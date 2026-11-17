/* Stage 6 support. */

#ifndef REZ_STG06_STAGE_BASE_H
#define REZ_STG06_STAGE_BASE_H

int stage_rating(void);
void boss_boot_exec(int *w);
int *boss_boot_create(void *parent);
void start_map(void *menu);
void start_map_shape(void *menu);
void map_init(int *w);
void map_release(void);
void map_exec(void);
void *map_create(void *w);
void map_menu_create(void *parent);
void map_menu_next(void);
void map_menu_set_page(int page);
void map_menu_set_value(int scene);
int map_get_current(void);
void *map_menu_get(void);
int *map_get_work(void);

#endif /* REZ_STG06_STAGE_BASE_H */
