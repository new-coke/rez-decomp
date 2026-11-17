/* Ending module. */

#ifndef REZ_ENDING_ENDING_H
#define REZ_ENDING_ENDING_H

#include "ending_shared.h"

void *module_entry(void *parent);
void ending_start(Ending *w);
void ending_release(Ending *w);
void ending_exec_wait(Ending *w);
void ending_disp(void);
void ending_exec(Ending *w);
void *ending_create(void *parent);
int ending_type_get(void);
void ending_stop_scene(Ending *w);
void ending_set_end(void);

#endif /* REZ_ENDING_ENDING_H */
