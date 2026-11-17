/* Area 2 stage support. */

#ifndef REZ_STG02_STAGE_BASE_H
#define REZ_STG02_STAGE_BASE_H

#include "types_overlays.h"
#include "types_common.h"

struct CircleWork;
struct CircleRing;

int stage_rating(void);
void stage_data_free(void);
void boss_boot_exec(int *w);
int *boss_boot_create(void *parent);
void listeners_add(void);
void listeners_remove(void);
void listener_stars_on(void);
void listener_next_map(void);
void listener_mute(int *mute);
void listener_lock_hit(HitEv *ev);
void listener_area_change(int *ev);
void filter_vars_add(void);
void filter_vars_remove(void);
void filter_reset(void);
void filter_set_area(unsigned int area);
void filter_fade_last(void);
int stars_on(void);
void stars_off(void);
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
void *map_get_work(void);
void circle_init(struct CircleWork *w, struct CircleRing *ring);
void circle_release(void);
void circle_exec(struct CircleWork *w);
void circle_disp(struct CircleWork *w);
void *circle_create(void *parent, void *ring);
void circle_mng_spawn_nodes(unsigned int *node, int n, Vec3 *step, int scene, float scale);
void circle_mng_set_scene(int ev);
void circle_mng_init(int *w);
void circle_mng_release(int *w);
void circle_mng_exec(void);
void circle_mng_disp(void);
void *circle_mng_create(void *parent);

#endif /* REZ_STG02_STAGE_BASE_H */
