/* Area 2 stage module. */

#ifndef REZ_STG02_STAGE02_H
#define REZ_STG02_STAGE02_H

#include "types_overlays.h"

void draw_cb_pulse_init(MapDrawObj *obj);
void draw_cb_pulse_release(int *obj);
void draw_cb_pulse_exec(MapDrawObj *obj);
void draw_cb_pulse_disp(MapDrawObj *obj);
void draw_cb_disp_dl5(MapDrawObj *obj);
void draw_cb_cyan(DrawObj *obj);
void draw_cb_blue_dl5(DrawObj *obj);
void draw_cb_red(DrawObj *obj);
void draw_cb_red2(DrawObj *obj);
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
void stage_mng_debug_keys(void);
int stage_vars_group_get(void);
void stage_vars_add(void);
void stage_vars_remove(void);
void models_load(void);
void models_free(void);

#endif /* REZ_STG02_STAGE02_H */
