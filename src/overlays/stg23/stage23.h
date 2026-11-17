/* Trance Mission stage module. */

#ifndef REZ_STG23_STAGE23_H
#define REZ_STG23_STAGE23_H

#include "types_overlays.h"

void phase_init(PhaseWork *w);
void phase_release(PhaseWork *w);
void phase_exec(PhaseWork *w);
void phase_disp(void);
void *phase_create(void *parent);
void start_phase(PhaseWork *w, int phase);
void end_phase(PhaseWork *w, int phase);
void phase_request(int phase);
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

#endif /* REZ_STG23_STAGE23_H */
