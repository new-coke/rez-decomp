/* Area 1 stage module. */

#ifndef REZ_STG01_STAGE01_H
#define REZ_STG01_STAGE01_H

#include "types_overlays.h"

void draw_cb_list3(DrawObj *act);
void draw_cb_red(DrawObj *obj);
void draw_cb_by_map(DrawObj *act);
void draw_cb_plain(DrawObj *act);
void phase_init(PhaseWork *w);
void phase_release(PhaseWork *w);
void phase_exec(PhaseWork *w);
void phase_disp(void);
void *phase_create(void *parent);
void start_phase(PhaseWork *w, int phase);
void end_phase(PhaseWork *w, int phase);
void phase_request(int phase);
void phase01_request(void);
void phase02_request(void);
void phase03_request(void);
void phase04_request(void);
void module_entry(void);
void module_exit(void);
void stage_init(void);
void stage_release(void);
void stage_mng_exec(void);
void *stage_mng_create(void *parent);
void debug_item_create(int kind);
void stage_mng_debug_keys(void);
int stage_vars_group_get(void);
void stage_vars_add(void);
void stage_vars_remove(void);

#endif /* REZ_STG01_STAGE01_H */
