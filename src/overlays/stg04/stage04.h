/* Area 4 stage module. */

#ifndef REZ_STG04_STAGE04_H
#define REZ_STG04_STAGE04_H

#include "types_overlays.h"

void draw_cb_pulse_attach(MapDrawObj *obj, int idx);
void draw_cb_pulse_release(MapDrawObj *obj);
void draw_cb_pulse_exec(MapDrawObj *obj);
void draw_cb_pulse_disp(MapDrawObj *obj);
void draw_cb_pulse_init0(MapDrawObj *obj);
void draw_cb_pulse_init1(MapDrawObj *obj);
void draw_cb_pulse_init2(MapDrawObj *obj);
void draw_cb_pulse_init3(MapDrawObj *obj);
void draw_cb_pulse_init4(MapDrawObj *obj);
void draw_cb_pulse_init5(MapDrawObj *obj);
void draw_cb_pulse_init6(MapDrawObj *obj);
void draw_cb_pulse_init7(MapDrawObj *obj);
void draw_cb_pulse_init8(MapDrawObj *obj);
void draw_cb_pulse_init9(MapDrawObj *obj);
void draw_cb_pulse_init10(MapDrawObj *obj);
void draw_cb_pulse_init11(MapDrawObj *obj);
void draw_cb_pulse_init12(MapDrawObj *obj);
void draw_cb_pulse_init13(MapDrawObj *obj);
void draw_cb_pulse_init14(MapDrawObj *obj);
void draw_cb_red(DrawObj *obj);
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

#endif /* REZ_STG04_STAGE04_H */
