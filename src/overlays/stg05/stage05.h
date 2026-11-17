/* Area 5 stage module. */

#ifndef REZ_STG05_STAGE05_H
#define REZ_STG05_STAGE05_H

#include "types_overlays.h"
#include "types_common.h"

struct MapWork;
struct StageMessageWork;

void cmd_change_map(void);
void cmd_clear(void);
void map_blur_release(void);
void map_blur_exec(void);
void *map_blur_create(void *parent);
void start_map(void *menu);
void start_map_shape(void *menu);
void map_init(struct MapWork *w);
void map_release(struct MapWork *w);
void map_exec(void);
void *map_create(void *w);
void map_menu_create(void *parent);
void map_menu_next(void);
void map_menu_set_page(int page);
void map_menu_set_value(int scene);
int map_get_current(void);
void *map_menu_get(void);
int *map_get_work(void);
void draw_cb_no_fog(DrawObj *w);
void stage_message_init(struct StageMessageWork *w);
void stage_message_release(void **con);
void stage_message_center(void **con);
void stage_message_exec_close(struct StageMessageWork *w);
void stage_message_exec_type(struct StageMessageWork *w);
void stage_message_exec(struct StageMessageWork *w);
void stage_message_disp(void);
void *stage_message_create(void *parent);
void phase_init(PhaseWork *w);
void phase_release(PhaseWork *w);
void phase_exec(PhaseWork *w);
void phase_disp(void);
void *phase_create(void *parent);
void start_phase(PhaseWork *w, int phase);
void end_phase(PhaseWork *w, int phase);
void phase_request(int phase);
void module_entry(void);
float module_exit(void);
void stage_init(void);
void stage_release(void);
void stage_mng_exec(void);
void *stage_mng_create(void *parent);
void stage_mng_debug_keys(void);
int stage_vars_group_get(void);
void stage_vars_add(void);
void stage_vars_remove(void);
void models_load(void);
void models_free(void);
float stage_ground_height(Vec3 *p);

#endif /* REZ_STG05_STAGE05_H */
